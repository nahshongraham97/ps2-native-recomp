"""Original-source packing-domain proof; static def/use equations, no execution.

No descriptor, pointer, allocator, callee ABI or loaded-resource invariant is
invented. Unknown fields retain all32 bits. A compatible bit abstraction is an
OPEN question, never a constructed executable instruction or runtime matcher.
"""
from __future__ import annotations
import argparse
from collections import Counter, deque
import hashlib
import json
from pathlib import Path
import audit_r192_vu0_routes as audit

PROOF_SHA = 'e3f46531c6026be4331a6b7f558a3cafad66ef3d35f0691d4a51c661f40fddf5'
AUDIT_SHA = 'fc654acb23866b558e459b816d5776ad0b5f5ee2a140c90c355f06e87ea1bc6e'
FULL = 0xffffffff
VCALL_MASK = 0xfe00003f
EFFECTS = {0x2ed300,0x2edc50,0x2edfa0,0x2ee160,0x2ee290}


def need(ok, why): audit.require(ok, why)


def destination(w):
    op,rs,rt,rd,fn = w>>26,w>>21&31,w>>16&31,w>>11&31,w&63
    if op in audit.STORE_OPS or op in (2,4,5,6,7,20,21,22,23,0x2f,0x33): return None
    if op==3: return 31
    if op==1: return 31 if rt in (16,17,18,19) else None
    if op==0: return None if fn in (8,0xc,0xd,0xf,0x11,0x13) else rd
    if op in (16,17,18): return rt if rs in (0,1,2) else None
    return rd if op==28 else rt


def merge(a,b): return tuple(x|y for x,y in zip(a,b))


def definitions(words, entry):
    """Finite reaching-definitions CFG, with complete branch/delay bundles.

    Sets contain definition PCs; negative PCs denote an unbound callee clobber.
    Likely branches annul their delay on the not-taken edge. No branch condition
    or callee semantics is guessed. Only this original function entry is rooted.
    """
    initial=tuple(frozenset({0}) if r==0 else frozenset({None}) for r in range(32))
    incoming={entry:initial}; before={}; work=deque([entry]); edges=set(); open_control=[]
    def write(state,pc):
        d=destination(words[pc]); state=list(state)
        if d: state[d]=frozenset({pc})
        return tuple(state)
    def offer(pc,state):
        if pc not in words: return
        old=incoming.get(pc); new=state if old is None else merge(old,state)
        if new!=old: incoming[pc]=new; work.append(pc)
    while work:
        pc=work.popleft(); state=incoming[pc]; before[pc]=merge(before[pc],state) if pc in before else state
        w=words[pc]; op,rs,rt,fn=w>>26,w>>21&31,w>>16&31,w&63
        if not audit.control(w):
            if op==0 and fn in (0xc,0xd): open_control.append((pc,'exception_boundary')); continue
            offer(pc+4,write(state,pc)); edges.add((pc,pc+4)); continue
        if pc+4 not in words or audit.control(words[pc+4]):
            open_control.append((pc,'missing_or_control_delay')); continue
        branch_state=write(state,pc)
        likely=op in (20,21,22,23) or op==1 and rt in (2,3,18,19) or op in (16,17,18) and bool(rt&2)
        identity = True if op in (4,20) and rs==rt or op in (6,22) and rs==0 else False if op in (5,21) and rs==rt or op in (7,23) and rs==0 else None
        if not (likely and identity is False):
            before[pc+4]=merge(before[pc+4],branch_state) if pc+4 in before else branch_state
        delayed=write(branch_state,pc+4)
        call=op==3 or op==0 and fn==9
        if call:
            clobbered=tuple(frozenset({0}) if r==0 else frozenset({-pc}) for r in range(32))
            offer(pc+8,clobbered); edges.add((pc,pc+8)); continue
        if op==0: continue  # JR exits; no invented target or return ABI.
        if op==2:
            target=((pc+4)&0xf0000000)|((w&0x3ffffff)<<2)
            offer(target,delayed); edges.add((pc,target)); continue
        immediate=w&65535; immediate-=65536 if immediate&32768 else 0
        target=pc+4+4*immediate
        # Source identities only; no runtime-valued branch evaluation. The
        # original BEQ zero,zero is unconditional and must not invent a bypass.
        if identity is not False: offer(target,delayed); edges.add((pc,target))
        if identity is not True: offer(pc+8,branch_state if likely else delayed); edges.add((pc,pc+8))
    return before,sorted(set(open_control)),edges


def node(op,pc=0,args=(),constant=None,detail=None):
    m=v=0
    if op=='constant': m,v=FULL,constant&FULL
    elif op=='join' and args:
        m,v=args[0]['mask'],args[0]['value']
        for a in args[1:]: m &= a['mask'] & ~(v^a['value']); v &= m
    elif op in ('or','and','xor'):
        a,b=args; ma,va,mb,vb=a['mask'],a['value'],b['mask'],b['value']
        if op=='or': ones=(ma&va)|(mb&vb); zeros=(ma&~va)&(mb&~vb)
        elif op=='and': ones=(ma&va)&(mb&vb); zeros=(ma&~va)|(mb&~vb)
        else: ones=(va^vb)&ma&mb; zeros=~(va^vb)&ma&mb
        m=(ones|zeros)&FULL; v=ones&FULL
    elif op in ('sll32','srl32'):
        a=args[0]; n=constant
        if op=='sll32': m=((a['mask']<<n)|((1<<n)-1))&FULL; v=(a['value']<<n)&FULL
        else: m=(a['mask']>>n)|((FULL<<(32-n))&FULL if n else 0); v=a['value']>>n
    elif op in ('add32','sub32') and all(a['mask']==FULL for a in args):
        m=FULL; v=(args[0]['value']+(args[1]['value'] if op=='add32' else -args[1]['value']))&FULL
    return {'op':op,'pc':pc,'args':args,'constant':constant,'detail':detail,'mask':m&FULL,'value':v&FULL}


class Equations:
    def __init__(self,words,entry):
        self.words=words; self.before,self.open,self.edges=definitions(words,entry)
    def value(self,reg,use,seen=frozenset()):
        if reg==0: return node('constant',constant=0)
        key=(reg,use)
        if key in seen or len(seen)>48: return node('unbound_cycle_or_depth',use,detail=f'r{reg}')
        defs=self.before.get(use,())[reg] if use in self.before else frozenset({None})
        values=[self.definition(reg,d,seen|{key}) for d in sorted(defs,key=lambda x:-1 if x is None else x)]
        return values[0] if len(values)==1 else node('join',use,values)
    def definition(self,reg,pc,seen):
        if pc is None: return node('unbound_entry',detail=f'r{reg}')
        if pc<0: return node('unbound_callee_clobber',-pc,detail=f'r{reg}')
        w=self.words[pc]; op,rs,rt,fn,s=w>>26,w>>21&31,w>>16&31,w&63,w>>6&31
        imm=w&65535; simm=imm-65536 if imm&32768 else imm
        def get(r): return self.value(r,pc,seen)
        def const(v): return node('constant',pc,constant=v)
        if op==15: return const(imm<<16)
        if op in (8,9,24,25): return node('add32',pc,[get(rs),const(simm)])
        if op in (12,13,14): return node({12:'and',13:'or',14:'xor'}[op],pc,[get(rs),const(imm)])
        if op==0 and fn in (0,2): return node('sll32' if fn==0 else 'srl32',pc,[get(rt)],constant=s)
        if op==0 and fn in (0x21,0x2d,0x23,0x2f,0x24,0x25,0x26):
            if fn in (0x21,0x2d) and (rs==0 or rt==0): return get(rt if rs==0 else rs)
            return node({0x21:'add32',0x2d:'add32',0x23:'sub32',0x2f:'sub32',0x24:'and',0x25:'or',0x26:'xor'}[fn],pc,[get(rs),get(rt)])
        if op in (0x20,0x21,0x23,0x24,0x25,0x27,0x37,0x1e):
            return node('loaded_field',pc,[get(rs)],constant=simm,detail={'base_register':rs,'opcode':op,'input_width':'unbounded low32; no allocator/writer invariant assumed'})
        if op==3: return const(pc+8)
        return node('unbound_definition',pc,detail=f'{w:08X}')


def flatten(expr):
    yield expr
    for a in expr['args']: yield from flatten(a)


def equation(expr,depth=0):
    if depth>8: return '...'
    op=expr['op']
    if op=='constant': return f'{expr["constant"]&FULL:08X}'
    if op=='loaded_field': return f'load@{expr["pc"]:08X}({equation(expr["args"][0],depth+1)}{expr["constant"]:+d})'
    if not expr['args']: return f'{op}@{expr["pc"]:08X}:{expr["detail"]}'
    return op+'('+','.join(equation(a,depth+1) for a in expr['args'])+(f';{expr["constant"]}' if expr['constant'] is not None else '')+')'


def compatibility(expr):
    out={}
    for name,pattern in [('VCALLMS',0x4a000038),('VCALLMSR',0x4a000039)]:
        conflict=(expr['value']^pattern)&expr['mask']&VCALL_MASK
        alternatives=[compatibility(a)[name] for a in expr['args']] if expr['op']=='join' else []
        disjoint=bool(alternatives) and all(x['status'].startswith('EXCLUDED') for x in alternatives)
        out[name]={'status':'EXCLUDED_BY_FORCED_BITS' if conflict else 'EXCLUDED_BY_EACH_SOURCE_ALTERNATIVE' if disjoint else 'OPEN_ABSTRACTION_COMPATIBLE',
                   'conflict_mask':f'{conflict:08X}'}
        if alternatives: out[name]['source_alternatives']=alternatives
    return out


def output_pointer(expr):
    for a in flatten(expr):
        if a['op']!='loaded_field' or a['constant']!=24: continue
        for b in flatten(a['args'][0]):
            if b['op']=='loaded_field' and b['constant']==3736:
                if any(c['op']=='loaded_field' and c['constant']==-21920 and c['detail']['base_register']==28 for c in flatten(b['args'][0])): return True
    return False


def describe(expr):
    nodes=list(flatten(expr)); shifts=[]
    for a in nodes:
        if a['op']!='sll32': continue
        n=a['constant']; fields=[f'{b["pc"]:08X}' for b in flatten(a['args'][0]) if b['op']=='loaded_field']
        if fields: shifts.append({'shift_pc':f'{a["pc"]:08X}','shift':n,'loaded_fields':sorted(set(fields)),
            'actual_semantics':'low32 shift/truncation; NO implicit5/16-bit field mask',
            'retained_input_bits':[0,31-n],'discarded_input_bits':[32-n,31] if n else [],
            'conditional_allocated_index_0_31_output_bits':[n,min(31,n+4)],
            'condition_status':'Not assumed for arbitrary descriptor fields; requires exact writer/allocator provenance.'})
    masks=[{'pc':f'{a["pc"]:08X}','mask':f'{a["args"][1]["value"]:08X}','source_equation':equation(a['args'][0])}
           for a in nodes if a['op']=='and' and a['args'][1]['mask']==FULL]
    return {'equation_low32':equation(expr),'known_mask':f'{expr["mask"]:08X}','known_value':f'{expr["value"]:08X}',
            'VCALL_compatibility':compatibility(expr),'original_definition_pcs':sorted({f'{a["pc"]:08X}' for a in nodes if a['pc']}),
            'loaded_fields':[{'pc':f'{a["pc"]:08X}','offset':a['constant'],'base_register':a['detail']['base_register']} for a in nodes if a['op']=='loaded_field'],
            'field_spill_and_truncation':shifts,'explicit_AND_masks':masks}


def self_test():
    checks=[]; u=node('unbound_entry')
    x=node('or',args=[node('constant',constant=0x46000000),node('sll32',args=[u],constant=16)])
    need(all(v['status']=='EXCLUDED_BY_FORCED_BITS' for v in compatibility(x).values()),'forced_opcode_bit_control_failed')
    checks.append('unbounded_field_does_not_clear_forced_OR_bit')
    need(node('sll32',args=[node('constant',constant=0x10000)],constant=16)['value']==0,'low32_truncation_control_failed')
    need(node('or',args=[node('constant',constant=0xac000000),node('constant',constant=0x40000000)])['value']==0xec000000,'offset_silent_mask_control_failed')
    checks+=['shift_truncates_at32','unmasked_offset_spills_no_invented_16bit_mask']
    words={0x1000:0x10000003,0x1004:0x3c024600,0x1008:0x3c024a00,0x100c:0x34420038,0x1010:0xac820000,0x1014:0x03e00008,0x1018:0}
    e=Equations(words,0x1000); need(e.value(2,0x1010)['value']==0x46000000,'unconditional_branch_bypass_control_failed'); checks.append('unconditional_branch_has_no_fabricated_fallthrough')
    words={0x1000:0x54000002,0x1004:0x3c024a00,0x1008:0x3c024600,0x100c:0xac820000,0x1010:0x03e00008,0x1014:0}
    e=Equations(words,0x1000); need(0x1004 not in e.before and e.value(2,0x100c)['value']==0x46000000,'likely_annulment_control_failed'); checks.append('known_not_taken_likely_annuls_delay')
    words={0x1000:0x3c024600,0x1004:0x0c000800,0x1008:0,0x100c:0xac820000,0x1010:0x03e00008,0x1014:0}
    e=Equations(words,0x1000); need(e.value(2,0x100c)['mask']==0,'stale_value_after_call_control_failed'); checks.append('callee_clobber_kills_stale_packing_value')
    words={0x1000:0x10800004,0x1004:0,0x1008:0x3c024600,0x100c:0x08000408,0x1010:0,0x1014:0x3c024a00,0x1018:0x34420038,0x101c:0,0x1020:0xac820000,0x1024:0x03e00008,0x1028:0}
    e=Equations(words,0x1000); need(compatibility(e.value(2,0x1020))['VCALLMS']['status']=='OPEN_ABSTRACTION_COMPATIBLE','branch_join_false_absence_control_failed'); checks.append('branch_join_retains_possible_VCALL')
    alternatives=node('join',args=[node('constant',constant=0x46000018),node('constant',constant=0x4be0003c)])
    need(all(v['status'].startswith('EXCLUDED') for v in compatibility(alternatives).values()),'disjoint_source_alternative_control_failed'); checks.append('each_source_alternative_excludes_without_lossy_merge_claim')
    words={0x1000:0x0c000800,0x1004:0x3c044a00,0x1008:0,0x100c:0x03e00008,0x1010:0}
    e=Equations(words,0x1000); delay=e.definition(4,0x1004,frozenset()); need(delay['value']==0x4a000000 and e.value(4,0x1008)['mask']==0,'call_delay_order_control_failed'); checks.append('delay_value_then_callee_clobber_distinct')
    return [{'name':x,'status':'PASS'} for x in checks]


def main():
    p=argparse.ArgumentParser(description=__doc__); p.add_argument('--producer-proof',type=Path,required=True); p.add_argument('--output',type=Path,required=True)
    a=p.parse_args(); a.producer_proof=a.producer_proof.resolve(); a.output=a.output.resolve(); script=Path(__file__).resolve()
    need(audit.sha(Path(audit.__file__))==AUDIT_SHA,'frozen_audit_dependency_changed')
    need(audit.sha(a.producer_proof)==PROOF_SHA,'producer_proof_pin_mismatch')
    need(a.output.is_relative_to(audit.W/'build/oracle-telemetry') and not a.output.exists(),'fresh_owned_output_required')
    proof=json.loads(a.producer_proof.read_text()); record=Path(proof['record']['path']); need(audit.sha(record)==proof['record']['sha256'],'producer_record_changed')
    pins=[{'path':str(x),'sha256':audit.sha(x)} for x in [script,a.producer_proof,record,Path(audit.__file__)]]+proof['source_pins']
    for pin in pins: need(audit.sha(Path(pin['path']))==pin['sha256'],'source_pin_changed:'+pin['path'])
    elf=next(Path(x['path']) for x in pins if Path(x['path']).name=='SLUS_214.10'); image=audit.Original(elf)
    records=json.loads(record.read_text()); rows=[]; manifests=[]; total_stores=Counter(); parameter_callers=[]
    raw_parameters={0x2ee8c0:4,0x2ee940:7,0x2eeab0:6,0x2ef330:5,0x2effc0:4}
    sources={Path(x['path']).name:Path(x['path']) for x in pins if Path(x['path']).suffix=='.cpp'}
    for r in records:
        start,end=[int(x,16) for x in r['span']]; raw=image.read(start,end-start)
        need(hashlib.sha256(raw).hexdigest()==r['span_sha256'],'original_span_mismatch:'+r['entry'])
        for s in r['source_excerpts']:
            encoded=s['generated_cpp'].encode(); need(hashlib.sha256(encoded).hexdigest()==s['excerpt_sha256'],'excerpt_hash_mismatch')
            actual=sources[s['unit']].read_bytes(); need(actual[s['start_byte']:s['start_byte']+len(encoded)]==encoded,'actual_generated_excerpt_mismatch')
        words={pc:image.word(pc) for pc in range(start,end,4)}; e=Equations(words,int(r['entry'],16))
        manifests.append({'entry':r['entry'],'span':r['span'],'span_sha256':r['span_sha256'],'source_excerpts':[{'unit':s['unit'],'sha256':s['excerpt_sha256']} for s in r['source_excerpts']],
                          'original_words':len(words),'entry_reachable_definition_sites':len(e.before),'open_control':e.open,'listing_unowned_words':r['listing_unowned_words']})
        requested=start in EFFECTS or 0x2ee3b0<=start<0x2f0600
        for pc,w in words.items():
            if pc not in e.before: continue
            op,rs,rt=w>>26,w>>21&31,w>>16&31
            if op==0x2b:
                total_stores['all_SW']+=1; value=e.value(rt,pc); addr=e.value(rs,pc); inline=output_pointer(addr)
                role='CURRENT_CONTEXT_OUTPUT_WORD_STORE' if inline else 'OTHER_OR_UNPROVED_STORE_DESTINATION'
                if inline: total_stores['identified_output_SW']+=1
                if requested: rows.append({'function':r['entry'],'pc':f'{pc:08X}','word':f'{w:08X}','file_offset':image.offset(pc),'role':role,
                    'destination_base_equation':equation(addr),'destination_offset_signed':(w&65535)- (65536 if w&32768 else 0),
                    'destination_owner_and_MMIO_exclusion':'OPEN: pointer values/ranges not admitted by this bit-domain proof.',**describe(value)})
            if op==3 and (((pc+4)&0xf0000000)|((w&0x3ffffff)<<2))==0x2effc0:
                delay=pc+4; d=destination(words[delay]); value=e.definition(4,delay,frozenset()) if d==4 else e.value(4,delay)
                rows.append({'function':r['entry'],'pc':f'{pc:08X}','word':f'{w:08X}','file_offset':image.offset(pc),'role':'RAW_APPEND_A0_AFTER_DELAY',
                             'delay_word':f'{words[delay]:08X}','destination_owner_and_MMIO_exclusion':'OPEN: append context/buffer ownership is not a raw-word value invariant.',**describe(value)})
            if op==3:
                target=((pc+4)&0xf0000000)|((w&0x3ffffff)<<2)
                if target in raw_parameters:
                    delay=pc+4; reg=raw_parameters[target]; d=destination(words[delay])
                    value=e.definition(reg,delay,frozenset()) if d==reg else e.value(reg,delay)
                    parameter_callers.append({'caller_function':r['entry'],'pc':f'{pc:08X}','word':f'{w:08X}','target':f'{target:08X}',
                        'raw_word_parameter_register':reg,'delay_word':f'{words[delay]:08X}',**describe(value),
                        'scope':'This represented direct source edge only; external/indirect callers are not excluded.'})
    selected=[x for x in rows if x['role'] in ('CURRENT_CONTEXT_OUTPUT_WORD_STORE','RAW_APPEND_A0_AFTER_DELAY')]
    counts=Counter((x['role'],'BOTH_EXCLUDED' if all(v['status'].startswith('EXCLUDED') for v in x['VCALL_compatibility'].values()) else 'OPEN') for x in selected)
    changed=[x['path'] for x in pins if audit.sha(Path(x['path']))!=x['sha256']]
    result={'status':'BOUNDED_ORIGINAL_PACKING_DOMAINS_COMPLETE' if not changed else 'INPUT_CHANGED','classification':'static original-source equations; no runtime matcher/interpreter, no execution or native credit',
        'native_credit':0,'source_pins':pins,'input_changes':changed,'function_manifest':manifests,'stores_and_raw_appends':rows,
        'opaque_word_parameter_direct_callers':parameter_callers,
        'conditional_memory_equation_counterexample':{'source_function':'002EF700','output_store_pc':'002EF864',
            'source_equation':'AC000000 OR (load@002EF814<<16) OR (load@002EF818<<21) OR load@002EF820, truncated low32',
            'conditional_fields':{'source_index':1,'base_index':2,'offset_low32':0},'result_word':'AC410000',
            'conditional_runtime_base_value':'GPR2.low32=11000000 gives a4-byte store to VU0 code bus.',
            'status':'CONDITIONAL_EQUATION_ONLY_NOT_ORIGINAL_EXECUTION',
            'missing_proofs':['Valid descriptor graph and branch/type conditions selecting this SW equation.','Actual runtime GPR2 pointer provenance/value and caller ABI.'],
            'point':'Even0..31 index constraints alone do not establish a no-MMIO domain.'},
        'summary':{'authenticated_original_records':len(records),'all_record_SW_count':dict(total_stores),'selected_word_sites':len(selected),'role_compatibility_counts':[{'role':k[0],'status':k[1],'count':v} for k,v in sorted(counts.items())],
                   'denominator':'Static source sites only. Raw-append call sites invoke the separately counted shared append-store body; these populations are not summed as dynamic emitted instructions.'},
        'controls':self_test(),'domain_rules':['Each loaded field remains unbounded low32 unless a source mask proves fewer bits. No global descriptor/allocated-register invariant is assumed.',
            'Low32 shifts truncate, OR-fields may spill into neighboring/opcode fields. Conditional0..31 index profiles are explicitly conditional, not an input admission.',
            'Forced-bit VCALL exclusion is universal over the overapproximated input values for that source equation; compatibility is not reachability or an executable-code claim.',
            'Entry-rooted source CFG reaching definitions includes branch/delay/likely annulment; calls invalidate all GPR values after the delay, with no invented ABI.'],
        'remaining_bounds':['Unknown descriptor fields, raw appends/constant blocks, interprocedural register values, source loops and computed callbacks remain OPEN where equations cannot close them.',
            'The allocator index0..31 does not bound runtime register VALUES or pointer effective addresses. Emitted memory op families cannot earn a no-MMIO certificate from register index bounds.',
            'Output buffer/context pointers and any loaded/fixup metadata need independently proved ownership, ranges, lifetime and executable-vs-data placement.',
            'Other SW destinations are recorded without claiming they are emitted instructions. VCALL-compatible data words do not establish executable VCALL.',
            'No whole-game/VU0 absence, microprogram identity, runtime execution equivalence, memory safety or native credit is claimed.']}
    audit.reserve(2<<20); a.output.mkdir(parents=True); (a.output/'source.py').write_bytes(script.read_bytes()); out=a.output/'RESULT.json'
    out.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
    print(json.dumps({'result':str(out),'sha256':audit.sha(out),'source_sha256':audit.sha(script),'status':result['status'],'summary':result['summary']})); return 0 if not changed else 1


if __name__=='__main__': raise SystemExit(main())
