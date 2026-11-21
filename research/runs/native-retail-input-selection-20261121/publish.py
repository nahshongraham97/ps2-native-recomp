"""Publish source physical candidate/scalar mode with exact current and chained execution identities."""
import importlib.util,json
from pathlib import Path
F=Path(__file__).resolve().parents[3];P=F.parents[2];H=Path(__file__).resolve().parent
C=P/'research/runs/native-retail-startup-20261120/connected-retail-entry-01';D=C/'native-retail-input-selection-22';R=C/'native-retail-scalar-mode-23'
spec=importlib.util.spec_from_file_location('previous',F/'research/runs/native-connected-main-dependencies-20261121/publish.py')
previous=importlib.util.module_from_spec(spec);spec.loader.exec_module(previous);pub=previous.pub
if __name__=='__main__':
    vpath=H/'validation-01/verification.json'
    pub.need(pub.sha(vpath)=='988a18ca7e291f8ef5c9bcd1b948f66ca5fb622fe8772c3b6230044835abeb69','exact measured producer')
    result=json.loads(vpath.read_text(encoding='utf-8'))
    pub.need(result['status']=='PASS_NATIVE_RETAIL_INPUT_SELECTION_CPU_FILE_CHECKS' and len(result['sources'])==120 and len(result['fixtures'])==38 and len(result['executed_clients'])==25 and len(result['reused_clients'])==13 and len(result['remaining_integration_symbols'])==39 and result['symlink_creation_exercised'] is False and not result['complete_game_entry_linkable'] and not result['game_executed'] and not result['physical_endpoints_opened'],'exact bounded candidate/reuse scope')
    for path,digest in result['sources'].items():pub.need(pub.sha(path)==digest,'measured source '+path)
    rpath=D/'review-saved-execution01/EXECUTION_REVIEW.json'
    pub.need(pub.sha(rpath)=='9434b6cd82478ed57d819ec3d3dc2f3c9f216707980390e8a209c4eed73737c4','independent saved review')
    review=json.loads(rpath.read_text(encoding='utf-8'))
    pub.need(review['schema']=='native-retail-input-selection-saved-execution-independent-review-v1' and review['status']=='VERIFIED_SAVED_CPU_FILE_CHECKS' and review['source_pins_checked']==120 and review['source_pin_mismatches']==0 and review['source_sha256']==result['sources'] and review['current_fixture_executables_checked']==38 and review['new_executions']==result['executed_clients'] and review['new_execution_count']==25 and review['reused_executions']==result['reused_clients'] and review['reused_execution_count']==13 and review['verified_reuse_chain_hops']==24 and review['fixture_exit_failures']==review['fixture_binary_hash_failures']==review['fresh_symbol_audit_failures']==review['archive_hash_failures']==review['reused_origin_hash_failures']==review['reused_normalized_argument_failures']==0 and review['remaining_integration_symbols']==result['remaining_integration_symbols'] and review['negative_entry_exact_symbols'] and not review['negative_entry_executable_exists'],'actual independent schema and reuse identities')
    spath=D/'review-source01/REVIEW.json'
    pub.need(pub.sha(spath)=='3d56088bba581d73ba6c5015baab8751b097fb8a48f3dc52878d041e02350655','independent source review')
    source=json.loads(spath.read_text(encoding='utf-8'))
    freeze=json.loads((D/'INTEGRATION.json').read_text(encoding='utf-8'))
    pub.need(source['schema']=='native-retail-physical-candidate-scalar-independent-source-review-v1' and source['status']=='SOURCE_REVIEW_PASS_READY_FOR_CPU_FILE_CHECKS' and source['source_pins_checked']==119 and source['source_pin_mismatches']==0 and source['source_record_pins_checked']==8 and source['source_record_pin_mismatches']==0 and source['source_sha256']==freeze['sources'] and source['source_records_sha256']==freeze['source_records'] and source['findings']==[] and source['original_raw_words_rejoined']==970 and source['original_raw_word_mismatches']==0 and source['integration_manifest_sha256']==pub.sha(D/'INTEGRATION.json')=='9a9425b45501560d155c96fcc1323b9118cccf1a9420ef61bb1967b9f6571f4d','actual source freeze and review')
    for path,digest in freeze['source_records'].items():pub.need(pub.sha(path)==digest,'frozen source record')
    for key,name in [('archive_sha256','libmka_native_retail_startup.a'),('frame_archive_sha256','libmka_native_retail_middleware_frame.a'),('alias_archive_sha256','libmka_native_retail_file_paths.a'),('profile_archive_sha256','libmka_native_retail_profile_owners.a'),('game_session_archive_sha256','libmka_native_retail_game_session.a'),('keyboard_archive_sha256','libmka_native_retail_keyboard.a'),('platform_archive_sha256','libmka_native_retail_platform.a')]:
        pub.need(pub.sha(F/'build_native_services_only_20261119-01'/name)==result[key],'actual measured archive '+name)
    chain_hops=0
    for row in result['fixtures']:
        pub.need(pub.sha(F/'build_native_services_only_20261119-01'/(row['target']+'.exe'))==row['sha256'],'current measured client')
        path=H/'validation-01'/(row['target']+'.json');record=json.loads(path.read_text(encoding='utf-8'))
        if row['target'] in result['reused_clients']:
            outer=dict(record);seen=set()
            while 'reused_from' in record:
                pub.need(str(path) not in seen,'acyclic exact receipt chain');seen.add(str(path))
                old=Path(record['reused_from']);pub.need(pub.sha(old)==record['reused_record_sha256'],'prior receipt bytes')
                earlier=json.loads(old.read_text(encoding='utf-8'))
                pub.need(earlier['exit']==0 and earlier['executable_sha256']==row['sha256']==earlier['executable_after_sha256'] and record['argv']==earlier['argv'],'actual prior execution/target retained')
                record,path=earlier,old;chain_hops+=1
            pub.need(str(path)==outer['original_execution_record'] and pub.sha(path)==outer['original_execution_record_sha256'],'authenticated ultimate original execution')
        else:pub.need('reused_from' not in record and record['exit']==0,'fresh successful execution')
    pub.need(chain_hops==24,'exact authenticated reuse hop count')
    protected={F/'runtime/src/iop/mcserv.cpp':'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',F/'runtime/src/iop/msl_native.cpp':'756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23',P/'research/doc-audit-20261118/inventory.json':'26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6'}
    for path,digest in protected.items():pub.need(pub.sha(path)==digest,'protected bytes')
    bases={F:'a4c3a6d9a782b95e785ada0d8fba250b652e4835',P:'07ed25ccbf36947150c50dda6f27c16d421eac9e'}
    for root,expected in bases.items():pub.need(pub.git(root,'rev-parse','HEAD')==expected and pub.git(root,'branch','--show-current')=='main','publication base')
    authored=['README.md','cmake/native_game.cmake','runtime/include/mka/retail_game_session.hpp','runtime/src/native/retail_game_session.cpp','tests/test_retail_game_session.cpp','runtime/include/mka/retail_input_selection.hpp','runtime/src/native/retail_input_selection.cpp','tests/test_retail_input_selection.cpp']
    forks={F/name for name in authored}|{F/'research/runs/native-retail-profile-generation-20261121/PUBLICATION.json'}
    forks.update(p for p in H.rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.name!='PUBLICATION.json' and not {'owned-content','disposable-package-errors','exclusive-startup-content','volume-owner-content'} & set(p.relative_to(H).parts))
    parents={P/'PORT_STATUS.md',P/'ROADMAP.md'}
    for scope in [D,R]:parents.update(p for p in scope.rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.suffix!='.exe')
    for path in forks|parents:
        if path.suffix in ['.bin','.raw','.tsv','.txt','.log','.csv']:continue
        if path.name.startswith(('RAW','ORIGINAL_WORDS')) or path.stem.endswith('_RAW'):continue
        raw=path.read_bytes();pub.need(b'\r' not in raw and raw.endswith(b'\n') and not raw.endswith(b'\n\n'),'LF/EOF '+str(path))
        pub.need(all(line==line.rstrip(b' \t') for line in raw.splitlines()),'whitespace '+str(path))
    fr,pr=pub.remote_probe(F,bases[F]),pub.remote_probe(P,bases[P])
    fh,fs=previous.commit_sync(F,sorted(forks),'Connect native retail input candidate selection and persistent scalar mode',fr)
    marker='Latest measured working-tree native-retail-input-selection-22-23 (base fork '+chr(96)+bases[F]+chr(96)+')'
    for name in ['PORT_STATUS.md','ROADMAP.md']:
        path=P/name;text=path.read_text(encoding='utf-8');pub.need(text.count(marker)==1,'one current marker')
        path.write_text(text.replace(marker,'Published native candidate/scalar checkpoint fork '+chr(96)+fh+chr(96)+' (work22/23)'),encoding='utf-8',newline='\n')
    ph,ps=previous.commit_sync(P,sorted(parents),'Record verified retail candidate selection and persistent scalar state',pr)
    for path,digest in result['sources'].items():pub.need(pub.sha(path)==digest,'source through publication')
    for path,digest in protected.items():pub.need(pub.sha(path)==digest,'protected through publication')
    record=dict(fork_commit=fh,fork=fs,parent_commit=ph,parent=ps,verification_sha256=pub.sha(vpath),scope='38 current native CPU/file clients:25 fresh/13 exact prior executions through24 authenticated hops;120 pins;39 unresolved main integrations, full game acceptance OPEN')
    with (H/'PUBLICATION.json').open('x',encoding='utf-8',newline='\n') as stream:json.dump(record,stream,indent=2);stream.write('\n')
    print(json.dumps(record))
