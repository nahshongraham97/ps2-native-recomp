"""Original source block selection, literal placement and buffer-pop provenance.

This is a static source proof. It does not execute instructions, infer pointer
ownership, assume successful partial emission, or classify every data bitpattern
as an executable instruction.
"""
from __future__ import annotations
import argparse
from collections import deque
import hashlib
import json
from pathlib import Path
import prove_r192_builder_packing_domains as packing
import audit_r192_vu0_routes as audit

PACKING_SHA='877bd1ad511c4a6a6b5172aafb492840f93c43bd7ebd611095a9771e779d4822'
PACKING_SOURCE='b4ffd8d486b6cbc33aa6aa7e86ef7dd37c1d308985c940801f6983dba826a647'
CHAINS=[
    # function, append, select block40, restore saved prior context
    (0x2ed300,0x2ed364,0x2ed35c,0x2ed394),
    (0x2edc50,0x2edd24,0x2edd1c,0x2edd54),
    (0x2edc50,0x2edda8,0x2edda0,0x2eddd8),
    (0x2edc50,0x2eddf8,0x2eddf0,0x2ede28),
    (0x2edc50,0x2ede48,0x2ede40,0x2ede78),
    (0x2edfa0,0x2ee020,0x2ee010,0x2ee064),
    (0x2edfa0,0x2ee02c,0x2ee010,0x2ee064),
    (0x2edfa0,0x2ee034,0x2ee010,0x2ee064),
    (0x2edfa0,0x2ee0c0,0x2ee0b8,0x2ee0f0)]
POP_WORDS=[0x8f82aa60,0x8c440e98,0x8c820008,0x50400007,0x0000102d,
           0x2442ffff,0xac820008,0x00021880,0x8c820018,0x00431021,
           0x8c420000,0x03e00008,0]
SETTER_WORDS=[0x10800002,0x8f83aa60,0xac640e98,0x03e00008,0]
DECREMENT_PACKING=[0x8c840004,0x3c052000,0x8f83aa60,0x00043540,
                   0x00c52825,0x00042400,0x8c660e98,0x00a41825,0x3467ffff]


def need(ok,why): audit.require(ok,why)


def target(w,pc): return ((pc+4)&0xf0000000)|((w&0x3ffffff)<<2)


def call(words,pc,expected):
    need(words[pc]>>26==3 and target(words[pc],pc)==expected,'original_call_target_mismatch')


def words_at(image,start,end):
    return [{'pc':f'{p:08X}','file_offset':image.offset(p),'word':f'{image.word(p):08X}'} for p in range(start,end,4)]


def graph(words,entry):
    _,_,edges=packing.definitions(words,entry)
    succ={pc:set() for pc in words}
    pred={pc:set() for pc in words}
    reachable={entry}
    for x,y in edges:
        if x in words and y in words: succ[x].add(y); pred[y].add(x); reachable.update((x,y))
    dom={pc:({pc} if pc==entry else set(reachable)) for pc in reachable}
    changed=True
    while changed:
        changed=False
        for pc in sorted(reachable-{entry}):
            parents=pred[pc]&reachable
            value={pc}|(set.intersection(*(dom[x] for x in parents)) if parents else set())
            if value!=dom[pc]: dom[pc]=value; changed=True
    return succ,dom


def before_stop(succ,start,stop):
    todo=deque([start]); seen=set(); exits=[]
    while todo:
        pc=todo.popleft()
        if pc==stop or pc in seen: continue
        seen.add(pc); following=succ.get(pc,set())
        if not following: exits.append(pc)
        todo.extend(following)
    return seen,exits


def validate_data_chain(words,entry,append,select,restore):
    call(words,select-12,0x2ee3b0); call(words,select,0x2ee3c0)
    call(words,append,0x2effc0); call(words,restore,0x2ee3c0)
    load=words[select-4]
    need(load>>26==0x23 and load>>16&31==4 and load&65535==40,'selected_block40_original_load_mismatch')
    save=words[select+4]; saved=save>>11&31
    need(save>>26==0 and save&63==0x2d and save>>21&31==2 and save>>16&31==0 and saved!=0,'saved_context_copy_mismatch')
    restore_word=words[restore+4]
    need(restore_word==(saved<<21)|(4<<11)|0x2d,'original_restore_saved_context_mismatch')
    # Only own source writes are checked here; callable ABI/frame/memory-owner
    # validity remains an explicit boundary below, not a native admission.
    overwritten=[pc for pc in range(select+8,restore,4) if packing.destination(words[pc])==saved]
    need(not overwritten,'saved_context_register_overwritten_in_caller')
    succ,dom=graph(words,entry)
    need(select in dom.get(append,set()),'block40_selector_does_not_dominate_append')
    between,_=before_stop(succ,select+8,append)
    setters=[pc for pc in between if words[pc]>>26==3 and target(words[pc],pc)==0x2ee3c0]
    need(not setters,'intervening_original_context_selector')
    _,exits=before_stop(succ,append+8,restore)
    need(not exits,'finite_original_path_exits_before_restore')
    return {'source_builder_base_register':load>>21&31,'saved_context_register':saved,
            'selection_dominates_append':True,'intervening_context_setters':[],
            'all_represented_finite_return_paths_reach_restore':True}


def pop_equation(count):
    count&=0xffffffff
    if count==0: return {'result':'ZERO','new_count':0,'byte_offset':None}
    n=(count-1)&0xffffffff
    return {'result':'LOAD32_SIGN_EXTENDED_LOW64','new_count':n,'byte_offset':(n<<2)&0xffffffff}


def fresh_pop_admission(count,capacity,valid_type):
    need(valid_type and 0<=count<capacity<=0x7fffffff,'fresh_pop_requires_successful_guarded_append')
    return pop_equation(count+1)['byte_offset']==((count<<2)&0xffffffff)


def decrement_source_word(words,index):
    """Evaluate the joined source packing equation, never a guessed mnemonic."""
    need([words[p] for p in range(0x2ef418,0x2ef43c,4)]==DECREMENT_PACKING,
         'original_decrement_packing_words_changed')
    need(0<=index<32,'conditional_allocated_index_out_of_range')
    base=(words[0x2ef41c]&65535)<<16
    rs_shift=words[0x2ef424]>>6&31
    rt_shift=words[0x2ef42c]>>6&31
    immediate=words[0x2ef438]&65535
    return (base | (index<<rs_shift) | (index<<rt_shift) | immediate)&0xffffffff


def validate_decrement_claim(claim,words,index):
    need(claim==decrement_source_word(words,index),'ADDIU_or_other_word_substituted_for_original_ADDI')


def controls(image,wordmaps):
    result=[]; words=wordmaps[0x2ed300].copy(); words[0x2ed358]=(words[0x2ed358]&0xffff0000)|44
    try: validate_data_chain(words,0x2ed300,0x2ed364,0x2ed35c,0x2ed394)
    except ValueError as e: need(str(e)=='selected_block40_original_load_mismatch','wrong_block_control_refusal'); result.append({'name':'wrong_literal_block_field','status':'PASS','refusal':str(e)})
    else: raise ValueError('wrong_block_admitted')
    words=wordmaps[0x2ed300].copy(); words[0x2ed398]^=1<<21
    try: validate_data_chain(words,0x2ed300,0x2ed364,0x2ed35c,0x2ed394)
    except ValueError as e: need(str(e)=='original_restore_saved_context_mismatch','wrong_restore_control_refusal'); result.append({'name':'wrong_saved_context_register','status':'PASS','refusal':str(e)})
    else: raise ValueError('wrong_restore_admitted')
    for name,n,cap,valid in [('type_refusal',1,8,False),('capacity_refusal',8,8,True)]:
        try: fresh_pop_admission(n,cap,valid)
        except ValueError as e: result.append({'name':'no_fresh_word_on_'+name,'status':'PASS','refusal':str(e)})
        else: raise ValueError('failed_append_treated_as_fresh')
    need(pop_equation(0)['result']=='ZERO' and pop_equation(0xffffffff)['byte_offset']==0xfffffff8,'pop_empty_or_wrap_control_failed')
    result.append({'name':'empty_pop_and_unchecked_negative_count','status':'PASS'})
    need(fresh_pop_admission(0,8,True) and fresh_pop_admission(3,8,True) and
         fresh_pop_admission(0x40000000,0x40000001,True),'valid_fresh_append_pop_equation_failed')
    result.append({'name':'successful_append_pop_returns_same_byte_slot','status':'PASS'})
    # Original selector's zero branch skips SW entirely. Its truth table must
    # retain the old selection, not manufacture block40 after a null allocation.
    prior='CODE28'; selected=None; actual=selected if selected is not None else prior
    need(actual==prior and actual!='DATA40','null_selector_noop_control_failed')
    result.append({'name':'null_selector_retains_prior_block','status':'PASS'})
    decrement=wordmaps[0x2ef400]
    for index in range(32):
        w=decrement_source_word(decrement,index)
        need(w>>26==8 and w>>21&31==index and w>>16&31==index and w&65535==65535,
             'source_decrement_opcode_or_fields_mismatch')
    result.append({'name':'all32_source_decrements_decode_ADDI_not_ADDIU','status':'PASS'})
    try: validate_decrement_claim(0x254affff,decrement,10)
    except ValueError as e: result.append({'name':'substituted_ADDIU_claim','status':'PASS','refusal':str(e)})
    else: raise ValueError('substituted_ADDIU_claim_admitted')
    mutated=decrement.copy(); mutated[0x2ef41c]=0x3c052400
    try: decrement_source_word(mutated,10)
    except ValueError as e: result.append({'name':'substituted_ADDIU_source_LUI','status':'PASS','refusal':str(e)})
    else: raise ValueError('substituted_ADDIU_source_admitted')
    return result


def main():
    p=argparse.ArgumentParser(description=__doc__); p.add_argument('--packing-proof',type=Path,required=True); p.add_argument('--output',type=Path,required=True)
    a=p.parse_args(); a.packing_proof=a.packing_proof.resolve(); a.output=a.output.resolve(); source=Path(__file__).resolve()
    need(audit.sha(a.packing_proof)==PACKING_SHA and audit.sha(Path(packing.__file__))==PACKING_SOURCE,'frozen_packing_proof_or_source_changed')
    need(a.output.is_relative_to(audit.W/'build/oracle-telemetry') and not a.output.exists(),'fresh_owned_output_required')
    proof=json.loads(a.packing_proof.read_text()); pins=proof['source_pins']+[{'path':str(source),'sha256':audit.sha(source)},{'path':str(a.packing_proof),'sha256':audit.sha(a.packing_proof)}]
    for pin in pins: need(audit.sha(Path(pin['path']))==pin['sha256'],'original_source_pin_changed:'+pin['path'])
    record=next(Path(x['path']) for x in pins if Path(x['path']).name=='producer-functions.json'); records=json.loads(record.read_text())
    elf=next(Path(x['path']) for x in pins if Path(x['path']).name=='SLUS_214.10'); image=audit.Original(elf); wordmaps={}; manifests=[]
    for r in records:
        start,end=[int(x,16) for x in r['span']]; raw=image.read(start,end-start)
        need(hashlib.sha256(raw).hexdigest()==r['span_sha256'],'original_span_changed')
        wordmaps[int(r['entry'],16)]={pc:image.word(pc) for pc in range(start,end,4)}
        manifests.append({'entry':r['entry'],'span':r['span'],'sha256':r['span_sha256']})
    need(list(wordmaps[0x2eff80].values())==POP_WORDS,'original_pop_words_changed')
    need(list(wordmaps[0x2ee3c0].values())==SETTER_WORDS,'original_selector_words_changed')
    parent=wordmaps[0x2ed4b0]
    need([parent[x] for x in range(0x2ed60c,0x2ed638,4)]==[0x2404000a,0x0c0bc010,0x24050002,0x24040002,0x0040982d,0x0c0bc010,0x0080282d,0x0040b02d,0x27a400a0,0x0c0bb69c,0x0280282d], 'original_fixed_node_and_builder_frame_setup_changed')
    need([wordmaps[0x2f0040][x] for x in range(0x2f00b4,0x2f00c0,4)]==[0xad040010,0x2502000c,0xad050014],'original_node_index_class_stores_changed')
    data=[]
    for fn,append,select,restore in CHAINS:
        facts=validate_data_chain(wordmaps[fn],fn,append,select,restore)
        data.append({'function':f'{fn:08X}','append_call':f'{append:08X}','select_call':f'{select:08X}','restore_call':f'{restore:08X}',**facts,
            'classification':'BLOCK40_LITERAL_DATA_IF_SELECTION_NON_NULL_AND_SOURCE_LAYOUT_INTACT',
            'selection_and_append_words':words_at(image,select-12,append+8),'restore_words':words_at(image,restore,restore+8),
            'null_selection_case':'Original setter skips assignment; previous block remains selected. No unconditional DATA claim.',
            'restoration_scope':'Exact restore call and unchanged caller saved register proved; original returned-call/frame/object nonaliasing discipline remains separate from native ownership admission.'})
    raw_open={x['pc'] for x in proof['stores_and_raw_appends'] if x['role']=='RAW_APPEND_A0_AFTER_DELAY' and any(v['status'].startswith('OPEN') for v in x['VCALL_compatibility'].values())}
    need(raw_open=={f'{x[1]:08X}' for x in CHAINS},'nine_raw_append_domain_changed')
    pop_chains=[]
    for select,field,producer,pop,consumer,helper,outpc in [(0x2ed6e8,28,0x2ed704,0x2ed70c,0x2ed718,0x2ef1f0,0x2ef314),
                                                          (0x2ed9a0,52,0x2ed9c0,0x2ed9c8,0x2ed9e4,0x2ef400,0x2ef45c)]:
        words=wordmaps[0x2ed4b0]; call(words,select,0x2ee3c0); call(words,producer,helper); call(words,pop,0x2eff80)
        call(words,consumer,0x2ef330 if field==28 else 0x2eeab0)
        need(words[select+4]==(0x8fa40000|(160+field)),'selected_parent_stack_block_mismatch')
        packed=next(x for x in proof['stores_and_raw_appends'] if x['function']==f'{helper:08X}' and x['role']=='CURRENT_CONTEXT_OUTPUT_WORD_STORE')
        need(all(x['status'].startswith('EXCLUDED') for x in packed['VCALL_compatibility'].values()),'fresh_source_word_not_excluded')
        succ,dom=graph(words,0x2ed4b0); need(select in dom.get(producer,set()) and producer in dom.get(pop,set()),'pop_selection_chain_not_dominated')
        between,_=before_stop(succ,select+8,consumer)
        need(not [pc for pc in between if words[pc]>>26==3 and target(words[pc],pc)==0x2ee3c0],'pop_chain_context_switched')
        pop_chains.append({'selected_builder_field':field,'selector_call':f'{select:08X}','producer_call':f'{producer:08X}','producer':f'{helper:08X}',
            'pop_call':f'{pop:08X}','consumer_call':f'{consumer:08X}','original_chain_words':words_at(image,select,consumer+8),
            'fresh_word_VCALL_exclusion':packed['VCALL_compatibility'],'fresh_word_equation':packed['equation_low32'],
            'conditional_fixed_node_fresh_word':'0000102D' if field==28 else f'{decrement_source_word(wordmaps[0x2ef400],10):08X}',
            'fixed_node_scope':'Original constructor passes(index10,class2) intoS3 and(index2,class2) intoS6; zero-source lookup requests(index0,class2). Exact word additionally requires those non-null node identities retained, selected type path and successful append.',
            'selected_block_scope':'Parent passesSP+160 to002EDA70; selector loadsSP+160+field. Effective selection requires non-null block.',
            'pop_cases':['Successful guarded append: reads the fresh appended word, whose source equation excludes both VCALL forms.',
                         'Producer null/type/capacity refusal: reads earlier selected-block word if count!=0; returns0 if empty. Prior-word provenance remains required.',
                         'Malformed negative/out-of-range count: unchecked modulo32 address; not admitted by this proof.']})
    changed=[x['path'] for x in pins if audit.sha(Path(x['path']))!=x['sha256']]
    result={'status':'BOUNDED_ORIGINAL_BLOCK_AND_POP_PROVENANCE_COMPLETE' if not changed else 'INPUT_CHANGED','source_pins':pins,'input_changes':changed,'original_span_manifest':manifests,
        'classification':'source-only conditional provenance; no instruction execution/runtime matcher/native credit','native_credit':0,
        'selector_original_words':words_at(image,0x2ee3c0,0x2ee3d4),'pop_original_words':words_at(image,0x2eff80,0x2effb4),
        'fixed_node_and_builder_frame_setup':words_at(image,0x2ed60c,0x2ed638),
        'node_index_class_store_and_return_pointer':words_at(image,0x2f00b4,0x2f00c0),
        'original_decrement_packing_words':words_at(image,0x2ef418,0x2ef43c),
        'decrement_identity_correction':{'original_opcode':'ADDI','conditional_index10_word':'214AFFFF',
            'superseded03_field':'two_delay_word_pop_chains[1].conditional_fixed_node_fresh_word was incorrectly hardcoded254AFFFF (ADDIU).',
            'unchanged_correct03_fields':['fresh_word_equation used original20000000 base','fresh_word_VCALL_exclusion used20000007/20000006 conflict masks'],
            'correction_scope':'Manual conditional-word summary and prior prose only; underlying packing04/source equation, original buffers, observers and runtime unchanged.'},
        'pop_exact_source_equation':{'count_zero':'return0, count unchanged','count_nonzero':'nprime=(n-1)mod2^32; store nprime toblock+8; return sign_extend32(LW32(buffer+((nprime<<2)mod2^32)))','missing_guards':['n>=0','n<=capacity','buffer/pointer range']},
        'all_nine_raw_append_data_chains':data,'two_delay_word_pop_chains':pop_chains,'controls':controls(image,wordmaps),
        'source_guard_boundaries':['All9 block40 selections and finite-return restore call paths are source-proved, conditional on non-null selections and intact original object/callframe discipline.',
            'Original002EE3C0 skips null selections/restores. Original002EE460 can return null; no universal allocation-success invariant is invented.',
            'Pop does not prove the preceding append succeeded. Partial emission is retained; older-buffer-word and backpatch provenance are separate obligations.',
            'Literal data placement does not itself prove final executable entry/control-flow exclusion or physical object ownership/nonaliasing.',
            'Runtime GPR pointer values, MMIO destinations, source count bounds and whole-game/native acceptance remain OPEN.']}
    audit.reserve(2<<20); a.output.mkdir(parents=True); (a.output/'source.py').write_bytes(source.read_bytes()); out=a.output/'RESULT.json'
    out.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n'); print(json.dumps({'result':str(out),'sha256':audit.sha(out),'source_sha256':audit.sha(source),'status':result['status'],'data_chains':len(data),'pop_chains':len(pop_chains)}))
    return 0 if not changed else 1


if __name__=='__main__': raise SystemExit(main())
