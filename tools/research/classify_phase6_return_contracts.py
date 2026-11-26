"""Reclassify the exact101 boundaries and attempt existing return-proof composition.

Machine normal-completion proofs, typed native signatures, ownership and E2 are
separate. The available bounded125370/three-helper proof is rederived; no
arbitrary callee return, pointer ABI, tail convention or context restoration is
invented to turn an unresolved dependency into an ordinary-call contract.
"""
from __future__ import annotations
import sys
sys.dont_write_bytecode=True
import argparse
from collections import Counter,defaultdict
import csv
import hashlib
import json
from pathlib import Path
import shutil
import classify_stack_call_boundaries as classify
import discharge_call_return_contracts as discharge
from verify_stack_exit_paths import audit_words

X=Path(__file__).resolve().parents[2]
INVENTORY='93760a5dcaf4e955e20814e6c173cc7ac7cd4e4312142ef5d9f192f88baa5b1a'
PRIOR='139855d2d901764d3710d1478b421e1ab19bde9dd37d46ffbb5e4ec1f3975dce'
DISCHARGE_SOURCE='a9c2dbe1b343ea7134603db786e23e7d5f3014979ca48118043a0922274c25ef'
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def need(value,reason):
    if not value:raise ValueError(reason)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for k in ('inventory','prior-discharge','original-elf','export-dir','generated-dir','output'):p.add_argument('--'+k,type=Path,required=True)
    a=p.parse_args()
    for k,v in vars(a).items():setattr(a,k,v.resolve())
    need(a.output.is_relative_to(X/'build/oracle-telemetry') and not a.output.exists(),'fresh_X_owned_output_required')
    need(sha(a.inventory)==INVENTORY and sha(a.prior_discharge)==PRIOR,'historical_proof_identity_changed')
    need(sha(Path(discharge.__file__))==DISCHARGE_SOURCE,'bounded_composition_producer_changed')
    inventory=json.loads(a.inventory.read_text());prior=json.loads(a.prior_discharge.read_text());read=classify.reader(a.original_elf.read_bytes())
    exit_source=Path(classify.__file__).with_name('verify_stack_exit_paths.py')
    need(sha(Path(classify.__file__))==inventory['input_pins']['tool_sha256'] and sha(exit_source)==inventory['input_pins']['exit_verifier_sha256'],'source_CFG_or_classifier_changed')
    pins={str(q):sha(q) for q in (Path(__file__),Path(classify.__file__),exit_source,Path(discharge.__file__),a.inventory,a.prior_discharge,a.original_elf)}
    for name,digest in classify.PINS.items():need(sha(a.export_dir/name)==digest,'original_export_changed');pins[str(a.export_dir/name)]=digest
    ranges=defaultdict(list);listing={}
    with (a.export_dir/'function-bodies.tsv').open(encoding='utf-8',newline='') as f:
        for row in csv.DictReader(f,delimiter='\t'):ranges[int(row['entry'],16)].append((int(row['start'],16),int(row['end'],16)+1))
    with (a.export_dir/'instructions.tsv').open(encoding='utf-8',newline='') as f:
        for row in csv.DictReader(f,delimiter='\t'):listing[int(row['address'],16)]=row
    def body(entry):
        words={}
        for lo,hi in ranges[entry]:
            for pc in range(lo,hi,4):
                raw=read(pc,4);need(pc in listing and bytes.fromhex(listing[pc]['bytes'])==raw,'original_listing_body_gap_or_mismatch');words[pc]=int.from_bytes(raw,'little')
        return words
    for name,digest in inventory['input_pins']['used_generated_units_sha256'].items():need(sha(a.generated_dir/name)==digest,'measured_AOT_source_changed');pins[str(a.generated_dir/name)]=digest
    bodies={int(c['entry'],16):body(int(c['entry'],16)) for c in inventory['direct_callees']}
    for entry in discharge.HELPERS|{discharge.PARENT,discharge.CONTEXT}:bodies[entry]=body(entry)
    helper={entry:discharge.prove_helper(entry,bodies[entry]) for entry in discharge.HELPERS}
    composed=discharge.prove_parent(bodies[discharge.PARENT],bodies,helper)
    need(composed==prior['machine_proof'],'existing_machine_composition_facts_changed')
    controls=discharge.mutation_tests(bodies,helper)
    leaf_summaries={};leaf_refusals=[]
    for c in inventory['direct_callees']:
        if c['exclusive_group']!='DIRECT_CLOSED_LEAF':continue
        entry=int(c['entry'],16)
        try:leaf_summaries[entry]=discharge.prove_helper(entry,bodies[entry])
        except ValueError as e:leaf_refusals.append({'entry':c['entry'],'reason':str(e),'scope':'bounded preserveSP/RA/S0 helper prover; not a general typed ABI'})
    known={**helper,**leaf_summaries};targets={};classifications=[]
    for c in inventory['direct_callees']:
        entry=int(c['entry'],16);words=bodies[entry];cfg=audit_words(words,entry);group=classify.classify(cfg,bool(words))
        context_loads=[w for w in words.values() if w>>26 in (0x23,0x37,0x1e) and w>>16&31 in (29,31) and w>>21&31!=29]
        if context_loads and entry==discharge.CONTEXT:group='DIRECT_CONTEXT_RESTORE_CONTRACT_UNPROVED'
        need(group==c['exclusive_group'],'rederived101_target_partition_changed:'+c['entry'])
        attempts=[]
        if group=='DIRECT_CLOSED_NONLEAF':
            for call in cfg['call_boundaries']:
                target=int(call['target'],16) if call['target'] else None
                attempts.append({'pc':call['pc'],'target':call['target'],'kind':call['kind'],
                    'dependency_status':'SOURCE_REDERIVED_BOUNDED_MACHINE_HELPER' if target in known else 'REFUSED_UNPROVED_RETURN_OR_ALIAS_SUMMARY',
                    'native_signature':'UNPROVED'})
            if entry==discharge.PARENT:status='REPROVED_EXISTING_MACHINE_NORMAL_RETURN_ONLY';reason='Exact source-frame/three-helper composition rederived; typed argument/result semantics remain unproved.'
            elif any(x['dependency_status'].startswith('REFUSED') for x in attempts):status='REFUSED';reason='unproved_nested_callee_return_or_memory_alias_effects'
            else:status='REFUSED';reason='caller_frame_RA_alias_and_return_composition_not_proved_by_available_summary'
        elif entry in leaf_summaries:status='SOURCE_MACHINE_HELPER_RETURN_ONLY';reason='NoSP/RA/S0 writes over complete normal CFG; argument/result/native memory semantics remain unproved.'
        else:status='REFUSED';reason=('ordinary_contract_inapplicable_context_restore' if group=='DIRECT_CONTEXT_RESTORE_CONTRACT_UNPROVED' else 'potential_tail_or_local_return_contract_unproved')
        targets[c['entry']]={'entry':c['entry'],'class':group,'status':status,'reason':reason,'nested_dependencies':attempts,
            'original_body_sha256':hashlib.sha256(b''.join(words[pc].to_bytes(4,'little') for pc in sorted(words))).hexdigest(),
            'body_spans':c['body_spans'],'native_typed_ABI':'UNPROVED','native_credit':0}
    for s in inventory['sites']:
        pc=int(s['pc'],16);need(int.from_bytes(read(pc,4),'little')==int(s['word'],16),'101_call_word_changed')
        delay=int(s['original_delay_pc'],16);need(int.from_bytes(read(delay,4),'little')==int(s['original_delay_word'],16),'101_delay_word_changed')
        target=targets.get(s['target']);group=target['class'] if target else 'INDIRECT_TARGET_UNRESOLVED';need(group==s['exclusive_group'],'101_site_class_changed')
        classifications.append({'pc':s['pc'],'caller':s['caller_entry'],'target':s['target'],'class':group,
            'source_call_word':s['word'],'source_delay_word':s['original_delay_word'],
            'machine_summary':target['status'] if target else 'REFUSED_UNPROVED_INDIRECT_TARGET_SET',
            'machine_reason':target['reason'] if target else 'Source register/pointer target domain is not authenticated.',
            'native_typed_ABI':'UNPROVED','native_admission':'REFUSED','native_credit':0})
    counts=dict(Counter(s['class'] for s in classifications));need(len(classifications)==101 and counts==inventory['summary']['exclusive_site_groups'],'101_denominator_or_partition_changed')
    largest=[s for s in classifications if s['class']=='DIRECT_CLOSED_NONLEAF'];need(len(largest)==45,'largest_class_denominator_changed')
    changed=[p for p,d in pins.items() if sha(p)!=d];need(shutil.disk_usage(X).free>(12<<30)+(8<<20),'12GiB_reserve');a.output.mkdir(parents=True)
    report={'status':'EXACT101_RECLASSIFIED_BOUNDED_COMPOSITION_ATTEMPT_COMPLETE' if not changed else 'INPUT_CHANGED','source_pins':pins,'input_changes':changed,
        'invocation':[sys.executable,str(Path(__file__).resolve()),*sys.argv[1:]],
        'summary':{'sites':101,'classes':counts,'largest_ordinary_class_sites_attempted':45,'largest_class_unique_targets':32,
            'existing_machine_subobligation_reproved_sites':sum(s['machine_summary']=='REPROVED_EXISTING_MACHINE_NORMAL_RETURN_ONLY' for s in largest),
            'largest_class_sites_still_refused':sum(s['machine_summary']=='REFUSED' for s in largest),'native_typed_ABIs_proved_from101':0,'native_credit':0},
        'all101_sites':classifications,'all52_direct_targets':list(targets.values()),'supporting_machine_leaf_summaries':list(leaf_summaries.values()),'supporting_leaf_refusals':leaf_refusals,
        'existing_composed_machine_proof':composed,'existing_three_helper_contracts':list(helper.values()),'original_word_mutation_controls':controls,'selector_controls':classify.self_test(),
        'separate_ABI_shim':'001A3C40 and its two callers are a separate admitted-owned slice, not substituted into or credited against this fixed101 cohort.',
        'limits':['Potential tails are not admitted tail ABIs; context restore001DDF98 never receives ordinary-preserving summary.','One existing machine proof rederived does not prove native signatures or a new full-function equivalence result.','The helper proof is deliberately bounded; unsupported instruction/alias effects remain named refusals.','No original code execution, savedstates, globalcontext restorer, R192, game/card, ownership or credit changes.']}
    out=a.output/'RESULT.json';out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n');print(json.dumps({'result':str(out),'sha256':sha(out),'status':report['status'],'summary':report['summary']}));return 0 if not changed else 1


if __name__=='__main__':raise SystemExit(main())
