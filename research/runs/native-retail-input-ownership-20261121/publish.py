"""Publish measured native input ownership and exact source corrections."""
import importlib.util,json
from pathlib import Path
F=Path(__file__).resolve().parents[3];P=F.parents[2];HERE=Path(__file__).resolve().parent
S=P/'research/runs/native-retail-startup-20261120/connected-retail-entry-01';D=S/'native-input-ownership-08'
spec=importlib.util.spec_from_file_location('pub',F/'research/runs/native-connected-retail-entry-20261120/publish.py');pub=importlib.util.module_from_spec(spec);spec.loader.exec_module(pub)
if __name__=='__main__':
    result=json.loads((HERE/'validation-01/verification.json').read_text(encoding='utf-8'))
    pub.need(result['status']=='PASS_NATIVE_INPUT_OWNERSHIP_CPU_FILE_CHECKS' and len(result['sources'])==56 and len(result['fixtures'])==15 and len(result['remaining_integration_symbols'])==46 and not result['complete_game_entry_linkable'],'actual measured input scope')
    for path,digest in result['sources'].items():pub.need(pub.sha(path)==digest,'current measured source '+path)
    review_path=D/'EXECUTION_REVIEW.json';pub.need(pub.sha(review_path)=='38ff19c5cbdabb63fb81f877467a3a5707f2bf75b159de20f072ec07f027436b','exact independent execution review')
    review=json.loads(review_path.read_text(encoding='utf-8'))
    pub.need(review['schema']=='native-retail-input-ownership-saved-execution-independent-review-v1' and review['status']=='PASS_SAVED_CPU_FILE_EVIDENCE_REVIEW' and review['source_pins_checked']==56 and review['client_receipts_checked']==15 and review['remaining_integration_symbols']==result['remaining_integration_symbols'] and review['findings']==[],'actual review schema')
    source_review=json.loads((D/'FINAL_SOURCE_REVIEW.json').read_text(encoding='utf-8'))
    pub.need(source_review['status']=='SCOPED_SOURCE_READY_FOR_CPU_FIXTURES' and len(source_review['reviewed_source_sha256'])==15 and source_review['findings']==[],'actual final source review')
    for path,digest in source_review['reviewed_source_sha256'].items():pub.need(pub.sha(path)==digest,'current reviewed source')
    for path,digest in source_review['evidence_sha256'].items():pub.need(pub.sha(path)==digest,'reviewed evidence remains exact')
    for key,filename in [('archive_sha256','libmka_native_retail_startup.a'),('keyboard_archive_sha256','libmka_native_retail_keyboard.a'),('platform_archive_sha256','libmka_native_retail_platform.a')]:pub.need(pub.sha(F/'build_native_services_only_20261119-01'/filename)==result[key],'actual archive '+key)
    for row in result['fixtures']:pub.need(pub.sha(F/'build_native_services_only_20261119-01'/(row['target']+'.exe'))==row['sha256'],'retained actual fixture')
    for path,digest in result['independent_manifest_sha256'].items():pub.need(pub.sha(path)==digest,'actual manifest')
    protected=review['protected_source_sha256']
    for path,digest in protected.items():pub.need(pub.sha(path)==digest,'protected bytes')
    bases={F:'fe7b73a4e59a629e5e5ef601ae0d198fff4c914e',P:'476806bf90f2b07326db71710e020f09871ffe25'}
    for root,expected in bases.items():pub.need(pub.git(root,'rev-parse','HEAD')==expected and pub.git(root,'branch','--show-current')=='main','expected publication bases')
    authored=['runtime/include/mka/retail_auxiliary_words.hpp','runtime/src/native/retail_auxiliary_words.cpp','runtime/src/native/retail_main_input_observation.cpp','runtime/include/mka/retail_participants.hpp','runtime/src/native/retail_participants.cpp','runtime/include/mka/retail_input_assignments.hpp','runtime/src/native/retail_input_assignments.cpp','runtime/include/mka/retail_input_roles.hpp','runtime/src/native/retail_input_roles.cpp','runtime/include/mka/retail_main.hpp','runtime/src/native/retail_main.cpp','tests/test_retail_participants.cpp','tests/test_retail_input_assignments.cpp','tests/test_retail_bound_owners.cpp','cmake/native_game.cmake','README.md']
    forks={F/n for n in authored}|{F/'research/runs/native-retail-keyboard-feed-20261121/PUBLICATION.json'}
    forks.update(p for p in HERE.rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.name!='PUBLICATION.json' and not {'owned-content','disposable-package-errors'} & set(p.relative_to(HERE).parts))
    parents={P/'PORT_STATUS.md',P/'ROADMAP.md'}
    for folder in ['native-input-ownership-08','native-controller-poll-08','native-main-transfer-state-09']:
        parents.update(p for p in (S/folder).rglob('*') if p.is_file() and '__pycache__' not in p.parts)
    path=D/'RESULTS.md';data=path.read_text(encoding='utf-8')
    if 'Independent saved-execution review is pending.' in data:
        path.write_text(data.replace('Independent saved-execution review is pending.','Independent saved-execution review verifies all56 source/tool pins, fifteen client execution/executable/symbol identities, three archives, original raw inputs/manifests/protected hashes and the exact46-name failed entry link. Complete-game acceptance remains OPEN.'),encoding='utf-8',newline='\n')
    pub.need('Independent saved-execution review verifies all56 source/tool pins' in path.read_text(encoding='utf-8'),'actual reviewed result marker')
    raw_exports={}
    for path in forks|parents:
        relative=path.relative_to(F) if path.is_relative_to(F) else path.relative_to(P)
        if 'before' in str(relative) or path.suffix in ['.bin','.csv','.log','.txt','.tsv']:continue
        raw=path.read_bytes()
        if path.name.startswith(('RAW','ORIGINAL_WORDS')) or path.stem.endswith('_RAW'):
            raw_exports[str(path)]=pub.sha(path);continue
        pub.need(b'\r' not in raw and raw.endswith(b'\n') and not raw.endswith(b'\n\n'),'authored LF/EOF '+str(path));pub.need(all(line==line.rstrip(b' \t') for line in raw.splitlines()),'authored whitespace '+str(path))
    for path,digest in raw_exports.items():pub.need(pub.sha(path)==digest,'raw export unchanged')
    fr,pr=pub.remote_probe(F,bases[F]),pub.remote_probe(P,bases[P])
    fh,fs=pub.commit_sync(F,sorted(forks),'Implement native participant-role protocol and real main input observations',fr)
    for name in ['PORT_STATUS.md','ROADMAP.md']:
        path=P/name;data=path.read_text(encoding='utf-8');marker='Latest measured working-tree native-input-ownership-08 (base fork `fe7b73a4e59a629e5e5ef601ae0d198fff4c914e`)';pub.need(data.count(marker)==1,'single live marker');path.write_text(data.replace(marker,'Published native input-ownership checkpoint fork `'+fh+'` (integration08)'),encoding='utf-8',newline='\n')
    ph,ps=pub.commit_sync(P,sorted(parents),'Record native input ownership and source-verified startup reset evidence',pr)
    for path,digest in result['sources'].items():pub.need(pub.sha(path)==digest,'measured source stable after publication')
    for path,digest in protected.items():pub.need(pub.sha(path)==digest,'protected bytes after publication')
    record=dict(fork_commit=fh,fork=fs,parent_commit=ph,parent=ps,verification_sha256=pub.sha(HERE/'validation-01/verification.json'),scope='Fifteen CPU/file clients; actual protocol/observers/reset. Complete startup, host poll/selection, fighter, scene/render/audio/game acceptance OPEN')
    with (HERE/'PUBLICATION.json').open('x',encoding='utf-8',newline='\n') as out:json.dump(record,out,indent=2);out.write('\n')
    print(json.dumps(record))
