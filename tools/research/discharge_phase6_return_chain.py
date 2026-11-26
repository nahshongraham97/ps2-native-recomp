"""Bounded machine normal-return composition, never a typed/native ABI admission.

The source-word abstract proof retains entry register identities and exact
stack spill widths. All callees are rederived from original bodies; unknown
calls, alias effects, tails and context restoration remain refusals. The
optional component harness compiles actual selected generated-AOT statements.
It checks machine postconditions, not independently recovered numeric results.
"""
from __future__ import annotations
import argparse, collections, copy, csv, hashlib, json, os, re, shutil, subprocess, sys
from pathlib import Path
import classify_stack_call_boundaries as source
import discharge_call_return_contracts as old
from verify_stack_exit_paths import decode as independent_control
import transform_generated_units as tr

ROOT=Path(__file__).resolve().parents[2]
PRIOR_SHA='9e9caf2d7e335b589e0f2ac88d9e5fc48c4a3851fb55c4c4a4d50cd1296b79e7'
CHAIN={0x122c30,0x125920,0x125430,0x125488,old.PARENT,*old.HELPERS}
COMPILER_SHA='ee85bef2c733e24a26c22be3df94bd631cf9528a048fce91241c182a936737fe'
RESERVE=12<<30
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def digest(value):return hashlib.sha256(json.dumps(value,sort_keys=True,separators=(',',':')).encode()).hexdigest()
def hx(n):return f'{n:08x}'
def need(ok,why):
    if not ok:raise ValueError(why)
def dump(path,obj):path.write_text(json.dumps(obj,indent=2)+'\n',encoding='utf-8',newline='\n')
def text(path,s):path.write_text(s,encoding='utf-8',newline='\n')
def imm(word):return (word&65535)-(65536 if word&32768 else 0)
def constant(n):return ('C',n&0xffffffffffffffff,0)
ZERO=constant(0)
def plus(value,n,word32=False):
    if value is None:return None
    kind,index,offset=value
    if kind=='C':
        val=index+n
        if word32:val=(val&0xffffffff)-(1<<32 if val&0x80000000 else 0)
        return constant(val)
    if kind=='S':return (kind,index,offset+n) # conditional on bounded canonical entry SP
    if kind in ('I','A'):return ('A' if word32 else kind,index,offset+n)
    return None

class Prover:
    def __init__(self,bodies):self.bodies=bodies;self.cache={};self.active=set()
    def proof(self,entry):
        if entry in self.cache:return self.cache[entry]
        need(entry in self.bodies,'unproved_callee_body:'+hx(entry))
        need(entry not in self.active,'recursive_return_contract_unproved:'+hx(entry))
        self.active.add(entry)
        try:result=self._proof(entry)
        finally:self.active.remove(entry)
        self.cache[entry]=result;return result
    def _proof(self,entry):
        words=self.bodies[entry];initial=tuple(ZERO if r==0 else ('S',0,0) if r==29 else ('I',r,0) for r in range(32))
        states={entry:(initial,{})};queue=collections.deque([entry]);returns={};footprints={};calls={};upper_clobbers=set();steps=0
        def footprint(base,offset,width,pc,owner):
            need(base is not None and base[0] in ('I','A','S'),'unproved_store_address_origin:'+hx(pc))
            kind,index,delta=base;key=('SP' if kind=='S' else 'input',index,delta+offset,width,pc,owner)
            footprints[key]=dict(base=key[0],register=index,offset=key[2],width=width,pc=hx(pc),owner=hx(owner))
        def put_memory(memory,base,offset,width,value,pc):
            if base is not None and base[0]=='S':
                address=base[2]+offset
                for k in list(memory):
                    if max(k[0],address)<min(k[0]+k[1],address+width):del memory[k]
                memory[(address,width)]=value if width==8 else ('low32',value) if width==4 else None
            else:
                need(not any(v is not None for v in memory.values()),'external_store_may_alias_retained_frame:'+hx(pc))
        def execute(pc,regs,memory):
            need(pc in words,'instruction_or_delay_gap:'+hx(pc));w=words[pc];op,rs,rt,rd,fn=w>>26,w>>21&31,w>>16&31,w>>11&31,w&63
            if not w:return
            def assign(r,value):
                if r:regs[r]=value
            if op in (9,25):assign(rt,plus(regs[rs],imm(w),op==9));return
            if op==15:assign(rt,constant(((w&65535)<<16)-(1<<32 if w&32768 else 0)));return
            if op==0 and fn in (33,45):
                a,b=regs[rs],regs[rt]
                if a==ZERO:assign(rd,plus(b,0,fn==33))
                elif b==ZERO:assign(rd,plus(a,0,fn==33))
                elif a is not None and b is not None and a[0]==b[0]=='C':assign(rd,plus(a,b[1],fn==33))
                else:assign(rd,None)
                return
            if op in (35,39,55):
                base=regs[rs];width=8 if op==55 else 4;value=None
                if base is not None and base[0]=='S':
                    saved=memory.get((base[2]+imm(w),width))
                    if width==8:value=saved
                    elif saved and saved[0]=='low32':
                        original=saved[1]
                        if original is not None and original[0]=='C':value=plus(original,0,op==35)
                        elif original is not None and original[0] in ('I','A'):value=('A',original[1],original[2])
                assign(rt,value);return
            if op in (43,63):
                width=8 if op==63 else 4;base=regs[rs]
                footprint(base,imm(w),width,pc,entry);put_memory(memory,base,imm(w),width,regs[rt],pc);return
            if op==28 and fn==48 and (w>>6&31)==0:
                # Original kMmi[48]/mode0 and generated mmi::pmfhl(const Cpu&,
                # Reg128&,0): writes only encoded destination, all128 bits.
                assign(rd,None);upper_clobbers.add(rd);return
            if op==0 and fn in (24,25):assign(rd,None);return # EE MULT/MULTU rd + HI/LO; no implicit GPR31/29
            if op==0 and fn in (10,11):assign(rd,regs[rd] if regs[rd]==regs[rs] else None);return
            # Existing bounded decoder supplies the remaining scalar write set.
            for r in old.writes(w):assign(r,None)
        def join(target,regs,memory):
            need(target in words,'CFG_gap_tail_or_unproved_external_exit:'+hx(target))
            candidate=(tuple(regs),dict(memory));previous=states.get(target)
            if previous is None:states[target]=candidate;queue.append(target);return
            merged=(tuple(a if a==b else None for a,b in zip(previous[0],candidate[0])),{k:v for k,v in previous[1].items() if k in memory and memory[k]==v})
            if merged!=previous:states[target]=merged;queue.append(target)
        while queue:
            pc=queue.popleft();steps+=1;need(steps<=50000,'finite_abstract_proof_work_limit')
            regs=list(states[pc][0]);memory=dict(states[pc][1]);w=words[pc];transfer=old.flow(pc,w);other=independent_control(pc,w)
            need(bool(transfer)==bool(other) and (not transfer or transfer[0]==other['kind']),'independent_control_decode_disagreement:'+hx(pc))
            if transfer:
                kind,targets,likely=transfer
                need(pc+4 in words and old.flow(pc+4,words[pc+4]) is None,'unsafe_or_missing_original_delay:'+hx(pc))
                if kind=='call':
                    target=targets[0];callee=self.proof(target);regs[31]=constant(pc+8);execute(pc+4,regs,memory);arguments=tuple(regs)
                    need(29 in callee['preserved_low64'] and 31 in callee['preserved_low64'] and 29 not in callee['upper_clobbers'] and 31 not in callee['upper_clobbers'],'callee_not_proved_SP_RA_preserving:'+hx(target))
                    for effect in callee['memory_writes']:
                        base=arguments[29] if effect['base']=='SP' else arguments[effect['register']]
                        footprint(base,effect['offset'],effect['width'],int(effect['pc'],16),int(effect['owner'],16))
                        put_memory(memory,base,effect['offset'],effect['width'],None,pc)
                    upper_clobbers.update(callee['upper_clobbers'])
                    for r in range(1,32):
                        value=callee['return_values'][r]
                        if value is None:regs[r]=None
                        elif value[0]=='I':regs[r]=plus(arguments[value[1]],value[2])
                        elif value[0]=='A':regs[r]=plus(arguments[value[1]],value[2],True)
                        elif value[0]=='S':regs[r]=plus(arguments[29],value[2])
                        else:regs[r]=value
                    calls[pc]=dict(pc=hx(pc),word=hx(w),delay_word=hx(words[pc+4]),target=hx(target),summary_sha256=digest(callee))
                    join(pc+8,regs,memory)
                elif kind=='return':
                    captured=regs[31];execute(pc+4,regs,memory)
                    need(captured==('I',31,0),'return_target_not_retained_entry_RA:'+hx(pc))
                    need(regs[31]==('I',31,0) and regs[29]==('S',0,0),'SP_RA_not_restored_after_return_delay:'+hx(pc))
                    need(not upper_clobbers&{29,31},'full128_SP_RA_not_preserved')
                    returns[pc]=tuple(regs)
                elif kind in ('jump','branch'):
                    for target in targets:
                        r=list(regs);m=dict(memory)
                        if not likely or target!=pc+8:execute(pc+4,r,m)
                        join(target,r,m)
                else:raise ValueError('unproved_indirect_call_exit_or_context:'+hx(pc))
            else:execute(pc,regs,memory);join(pc+4,regs,memory)
        need(returns,'no_proved_normal_return')
        return_values=[next(iter(returns.values()))[r] if all(v[r]==next(iter(returns.values()))[r] for v in returns.values()) else None for r in range(32)]
        preserved=[r for r in range(1,32) if return_values[r]==initial[r]]
        return dict(entry=hx(entry),status='PROVED_CONDITIONAL_MACHINE_NORMAL_COMPLETION',body_sha256=hashlib.sha256(b''.join(words[p].to_bytes(4,'little') for p in sorted(words))).hexdigest(),
                    preserved_low64=preserved,preserved_GPR128=sorted(set(preserved)-upper_clobbers),upper_clobbers=sorted(upper_clobbers),return_values=return_values,
                    memory_writes=sorted(footprints.values(),key=lambda x:(x['base'],x['register'],x['offset'],x['width'],x['owner'],x['pc'])),calls=list(calls.values()),normal_return_pcs=[hx(p) for p in sorted(returns)],abstract_steps=steps,
                    preconditions=['Canonical positive32-bit entry SP; every derived stack address nonwrapping/aligned/in ordinary RAM.','Every original load/store valid ordinary memory, no MMIO/device effect; normal completion only, termination and numeric results unproved.','No external write may alias a retained caller-frame spill; actual callee writes must have proved substituted origins.','Entry RA aligned; JR captures original entry link before the original delay; all128 SP/RA bits preserved.'],native_typed_ABI='UNPROVED',actual_game_state_E2='NOT_RUN',native_credit=0)

def load_bodies(export,read):
    ranges=collections.defaultdict(list);listing={}
    for row in csv.DictReader((export/'function-bodies.tsv').open(encoding='utf-8',newline=''),delimiter='\t'):ranges[int(row['entry'],16)].append((int(row['start'],16),int(row['end'],16)+1))
    for row in csv.DictReader((export/'instructions.tsv').open(encoding='utf-8',newline=''),delimiter='\t'):listing[int(row['address'],16)]=row
    def body(entry):
        need(bool(ranges[entry]),'original_function_extent_unavailable:'+hx(entry));words={}
        for lo,hi in ranges[entry]:
            for pc in range(lo,hi,4):
                raw=read(pc,4);need(pc in listing and bytes.fromhex(listing[pc]['bytes'])==raw,'original_listing_join_failed:'+hx(pc));words[pc]=int.from_bytes(raw,'little')
        return words
    return body

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('prior','original-elf','export-dir','generated-dir','output'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();a.output=a.output.resolve();need(a.output.is_relative_to(ROOT/'build/oracle-telemetry') and not a.output.exists(),'fresh_owned_output_required');need(shutil.disk_usage(ROOT).free>=RESERVE,'12GiB_reserve');a.output.mkdir(parents=True)
    pins={};report=dict(status='STARTED',native_credit=0,invocation=[sys.executable,str(Path(__file__).resolve()),*sys.argv[1:]])
    def pin(q,expected=None):
        q=q.resolve();s=sha(q);need(expected is None or s==expected,'source_pin_changed:'+str(q));pins[str(q)]=s
    try:
        for q in (Path(__file__),Path(old.__file__),Path(source.__file__),Path(source.__file__).with_name('verify_stack_exit_paths.py'),ROOT/'recomp/r5900.cpp',ROOT/'runtime/include/ps2/mmi.hpp'):pin(q)
        pin(a.prior,PRIOR_SHA);prior=json.loads(a.prior.read_text());pin(a.original_elf);read=source.reader(a.original_elf.read_bytes())
        for n,s in source.PINS.items():pin(a.export_dir/n,s)
        body=load_bodies(a.export_dir,read);bodies={e:body(e) for e in CHAIN};prover=Prover(bodies);summaries={}
        for entry in sorted(CHAIN):
            try:summaries[hx(entry)]=prover.proof(entry)
            except ValueError as error:summaries[hx(entry)]=dict(entry=hx(entry),status='REFUSED',reason=str(error))
        sites=[]
        for oldsite in prior['all101_sites']:
            if oldsite['class']!='DIRECT_CLOSED_NONLEAF':continue
            target=oldsite['target'];proof=summaries.get(target);status='NEW_MACHINE_NORMAL_RETURN_ONLY' if proof and proof['status'].startswith('PROVED') and oldsite['machine_summary']=='REFUSED' else 'EXISTING_MACHINE_NORMAL_RETURN_ONLY' if proof and proof['status'].startswith('PROVED') else 'REFUSED'
            sites.append(dict(oldsite,status=status,prior_machine_summary=oldsite['machine_summary'],new_machine_summary_sha256=digest(proof) if proof and proof['status'].startswith('PROVED') else None))
        need(len(sites)==45 and sum(s['prior_machine_summary']=='REFUSED' for s in sites)==44,'original45_44_denominator_changed')
        report.update(status='STATIC_MACHINE_COMPONENT_MEASURED',summaries=summaries,all45_ordinary_sites=sites,summary=dict(prior_outstanding=44,new_machine_sites=sum(s['status']=='NEW_MACHINE_NORMAL_RETURN_ONLY' for s in sites),remaining_machine_sites=sum(s['status']=='REFUSED' for s in sites),native_ABI_admissions=0,actual_saved_state_entries_exercised=0,native_credit=0))
    except Exception as error:report.update(status='FAILED_PRESERVED',reason=str(error))
    finally:
        changed=[q for q,s in pins.items() if sha(q)!=s];report.update(source_pins=pins,input_changes=changed)
        if changed:report['status']='INPUT_CHANGED'
        dump(a.output/'RESULT.json',report);print(json.dumps(dict(path=str(a.output/'RESULT.json'),sha256=sha(a.output/'RESULT.json'),status=report['status'],summary=report.get('summary'))))
    return 0 if report['status']=='STATIC_MACHINE_COMPONENT_MEASURED' else 1

if __name__=='__main__':sys.exit(main())
