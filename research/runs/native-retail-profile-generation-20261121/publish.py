"""Publish actual13-Wave profile generations and exact reused/new execution evidence."""
import importlib.util,json
from pathlib import Path
F=Path(__file__).resolve().parents[3];P=F.parents[2];H=Path(__file__).resolve().parent
C=P/'research/runs/native-retail-startup-20261120/connected-retail-entry-01';D=C/'native-retail-profile-main-20'
spec=importlib.util.spec_from_file_location('previous',F/'research/runs/native-connected-main-dependencies-20261121/publish.py')
previous=importlib.util.module_from_spec(spec);spec.loader.exec_module(previous);pub=previous.pub
if __name__=='__main__':
    vpath=H/'validation-01/verification.json'
    pub.need(pub.sha(vpath)=='b559abfe0e1f3135a03a374cf8345bff15ee4bcbdaea27fdd67b22b0c33ee25e','exact measured producer')
    result=json.loads(vpath.read_text(encoding='utf-8'))
    pub.need(result['status']=='PASS_NATIVE_RETAIL_WAVE_PROFILE_GENERATION_CPU_FILE_CHECKS' and len(result['sources'])==117 and len(result['fixtures'])==37 and len(result['executed_clients'])==2 and len(result['reused_clients'])==35 and len(result['remaining_integration_symbols'])==39 and result['symlink_creation_exercised'] is False and not result['complete_game_entry_linkable'] and not result['game_executed'] and not result['physical_endpoints_opened'],'exact bounded generation/reuse scope')
    for path,digest in result['sources'].items():pub.need(pub.sha(path)==digest,'measured source '+path)
    rpath=D/'review-saved-execution01/EXECUTION_REVIEW.json'
    pub.need(pub.sha(rpath)=='e282aafc1e29aaeec78c391edcdb59b52c59996d4f3b248549ad5d7242567d96','independent saved review')
    review=json.loads(rpath.read_text(encoding='utf-8'))
    pub.need(review['schema']=='native-retail-wave-profile-generation-saved-execution-independent-review-v1' and review['status']=='VERIFIED_SAVED_CPU_FILE_CHECKS' and review['source_pins_checked']==117 and review['source_pin_mismatches']==0 and review['source_sha256']==result['sources'] and review['fixture_count']==37 and review['new_executions']==result['executed_clients'] and len(review['new_executions'])==2 and review['reused_execution_count']==35 and review['reused_execution_names']==result['reused_clients'] and review['fixture_exit_failures']==review['fixture_binary_identity_failures']==review['fixture_symbol_target_failures']==0 and review['archive_identity_failures']==0 and review['remaining_integration_symbols']==result['remaining_integration_symbols'] and review['negative_entry_symbols_exact'] and not review['negative_entry_executable_exists'] and review['partial_active_wave_count']=='unknown/nullopt','actual independent schema and reuse identities')
    spath=D/'review-root-source01/REVIEW.json'
    pub.need(pub.sha(spath)=='703de3ea85707e821169d5104cbf47b2c161c987ab03c510311924f6497321ab','exact independent root source review')
    source=json.loads(spath.read_text(encoding='utf-8'))
    pub.need(source['schema']=='native-retail-wave-profile-generation-root-source-review-v1' and source['status']=='SOURCE_REVIEW_PASS_BOUNDED_MAIN_BINDING_OPEN' and source['source_pins_checked']==116 and source['source_pin_mismatches']==0 and source['source_record_pins_checked']==5 and source['source_record_pin_mismatches']==0 and source['findings']==[] and source['original_pinned_csv_files_rejoined']==5 and source['original_words_rejoined']==864 and source['integration_manifest_sha256']==pub.sha(D/'INTEGRATION.json')=='b5df9ded0bf8fdcb1dd71e99f71e647b5dbc96908b6b037c87bdfe53e5c47093','actual bounded source scope')
    freeze=json.loads((D/'INTEGRATION.json').read_text(encoding='utf-8'))
    for path,digest in freeze['source_records'].items():pub.need(pub.sha(path)==digest,'frozen source record')
    for key,name in [('archive_sha256','libmka_native_retail_startup.a'),('frame_archive_sha256','libmka_native_retail_middleware_frame.a'),('alias_archive_sha256','libmka_native_retail_file_paths.a'),('profile_archive_sha256','libmka_native_retail_profile_owners.a'),('game_session_archive_sha256','libmka_native_retail_game_session.a'),('keyboard_archive_sha256','libmka_native_retail_keyboard.a'),('platform_archive_sha256','libmka_native_retail_platform.a')]:
        pub.need(pub.sha(F/'build_native_services_only_20261119-01'/name)==result[key],'actual measured archive '+name)
    for row in result['fixtures']:
        pub.need(pub.sha(F/'build_native_services_only_20261119-01'/(row['target']+'.exe'))==row['sha256'],'current measured client')
        record=json.loads((H/'validation-01'/(row['target']+'.json')).read_text(encoding='utf-8'))
        if row['target'] in result['reused_clients']:
            old=Path(record['reused_from']);pub.need(pub.sha(old)==record['reused_record_sha256'],'exact prior receipt bytes')
            old_record=json.loads(old.read_text(encoding='utf-8'))
            pub.need(old_record['executable_sha256']==row['sha256']==record['executable_sha256'] and record['argv']==old_record['argv'],'actual old execution retained for current identical binary')
        else:pub.need('reused_from' not in record and record['exit']==0,'actual fresh execution')
    protected={F/'runtime/src/iop/mcserv.cpp':'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',F/'runtime/src/iop/msl_native.cpp':'756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23',P/'research/doc-audit-20261118/inventory.json':'26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6'}
    for path,digest in protected.items():pub.need(pub.sha(path)==digest,'protected bytes')
    bases={F:'f6e137f21641c4e07f621e508e8ed9c9ab9c8245',P:'c1e1ee46ad1edb83ff8b3f4f8fccc73f46b24507'}
    for root,expected in bases.items():pub.need(pub.git(root,'rev-parse','HEAD')==expected and pub.git(root,'branch','--show-current')=='main','known publication base')
    authored=['README.md','cmake/native_game.cmake','runtime/include/mka/retail_profile_owners.hpp','runtime/src/native/retail_profile_owners.cpp','runtime/include/mka/retail_profile_runtime.hpp','runtime/src/native/retail_profile_runtime.cpp','tests/test_retail_profile_runtime.cpp']
    forks={F/name for name in authored}|{F/'research/runs/native-retail-input-poll-20261121/PUBLICATION.json'}
    forks.update(p for p in H.rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.name!='PUBLICATION.json' and not {'owned-content','disposable-package-errors','exclusive-startup-content','volume-owner-content'} & set(p.relative_to(H).parts))
    parents={P/'PORT_STATUS.md',P/'ROADMAP.md'}
    parents.update(p for p in D.rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.suffix!='.exe')
    raw_records=json.loads((D/'FROZEN_RECORD_BYTES.json').read_text(encoding='utf-8'))['historical_no_eof_sha256']
    for path,digest in raw_records.items():pub.need(pub.sha(path)==digest,'exact historical report bytes')
    for path in forks|parents:
        if path.suffix in ['.bin','.raw','.tsv','.txt','.log','.csv']:continue
        if path.name.startswith(('RAW','ORIGINAL_WORDS')) or path.stem.endswith('_RAW'):continue
        raw=path.read_bytes()
        if str(path) in raw_records:
            pub.need(path.suffix=='.md' and any(part.startswith('before-') for part in path.parts) and not raw.endswith(b'\n') and b'\r' not in raw,'only exact pinned before-report EOF exception')
            raw+=b'\n'
        pub.need(b'\r' not in raw and raw.endswith(b'\n') and not raw.endswith(b'\n\n'),'LF/EOF or exact frozen before record '+str(path))
        pub.need(all(line==line.rstrip(b' \t') for line in raw.splitlines()),'whitespace '+str(path))
    fr,pr=pub.remote_probe(F,bases[F]),pub.remote_probe(P,bases[P])
    fh,fs=previous.commit_sync(F,sorted(forks),'Implement native repeated Wave profile generations and real retirement',fr)
    marker='Latest measured working-tree native-retail-wave-profile-generation-20 (base fork '+chr(96)+bases[F]+chr(96)+')'
    for name in ['PORT_STATUS.md','ROADMAP.md']:
        path=P/name;text=path.read_text(encoding='utf-8');pub.need(text.count(marker)==1,'one current marker')
        path.write_text(text.replace(marker,'Published native Wave-profile generation checkpoint fork '+chr(96)+fh+chr(96)+' (work20)'),encoding='utf-8',newline='\n')
    ph,ps=previous.commit_sync(P,sorted(parents),'Record verified native Wave profile generations with exact execution reuse',pr)
    for path,digest in result['sources'].items():pub.need(pub.sha(path)==digest,'source through publication')
    for path,digest in protected.items():pub.need(pub.sha(path)==digest,'protected through publication')
    record=dict(fork_commit=fh,fork=fs,parent_commit=ph,parent=ps,verification_sha256=pub.sha(vpath),scope='37 current CPU/file clients:2 fresh and35 byte-identical prior executions;117 pins,39 open integrations/full Main reset/game acceptance OPEN')
    with (H/'PUBLICATION.json').open('x',encoding='utf-8',newline='\n') as stream:json.dump(record,stream,indent=2);stream.write('\n')
    print(json.dumps(record))
