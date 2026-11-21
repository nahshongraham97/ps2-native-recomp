"""Publish independently verified native middleware main/time checkpoint."""
import importlib.util,json
from pathlib import Path
F=Path(__file__).resolve().parents[3];P=F.parents[2];H=Path(__file__).resolve().parent
C=P/'research/runs/native-retail-startup-20261120/connected-retail-entry-01';D=C/'native-retail-middleware-time-18'
spec=importlib.util.spec_from_file_location('previous',F/'research/runs/native-connected-main-dependencies-20261121/publish.py')
previous=importlib.util.module_from_spec(spec);spec.loader.exec_module(previous);pub=previous.pub
if __name__=='__main__':
    vpath=H/'validation-01/verification.json'
    pub.need(pub.sha(vpath)=='e7b98f1371769cceb8677a484bdd16a83389eed4eda59ce791b482b870bb7c78','measured producer result')
    result=json.loads(vpath.read_text(encoding='utf-8'))
    pub.need(result['status']=='PASS_NATIVE_RETAIL_MIDDLEWARE_FRAME_CPU_FILE_CHECKS' and len(result['sources'])==102 and len(result['fixtures'])==32 and len(result['remaining_integration_symbols'])==39 and result['symlink_creation_exercised'] is False and not result['complete_game_entry_linkable'] and not result['game_executed'] and not result['physical_endpoints_opened'],'exact scoped result')
    for path,digest in result['sources'].items():pub.need(pub.sha(path)==digest,'stable measured source '+path)
    rpath=D/'review-saved-execution01/EXECUTION_REVIEW.json'
    pub.need(pub.sha(rpath)=='fd1abb80b7ff7a7bd671395dbe9a1af31634c961c53638d181083418784ee44c','exact independent saved review')
    review=json.loads(rpath.read_text(encoding='utf-8'))
    pub.need(review['schema']=='native-retail-middleware-frame-independent-saved-execution-review-v1' and review['status']=='PASS_SAVED_NATIVE_RETAIL_MIDDLEWARE_FRAME_CPU_FILE_EVIDENCE' and review['source_pins_checked']==102 and review['fixtures_checked']==32 and review['sources']==result['sources'] and review['remaining_integration_symbols']==result['remaining_integration_symbols'] and review['negative_entry_executable_absent'] is True and review['all_client_exits_zero'] and review['all_executable_and_symbol_target_hashes_current'] and review['symlink_creation_exercised'] is False,'actual saved-review schema')
    for name,digest,status,pinkey in [
        ('review-frame-source01/REVIEW.json','e769e2be7488327be3afbb679db45f7211eaf7911cb5a3c19db687c9db609cf3','SOURCE_REVIEW_PASSED_UNBUILT_WITH_LIFETIME_PRECONDITION','integration_source_pins_checked'),
        ('review-root-integration01/REVIEW.json','f2cbf17238172a569776e9256a063231b33a23c9b011855420f5b999c086ed23','SOURCE_REVIEWED_UNBUILT','integration_pins_checked')]:
        p=D/name;pub.need(pub.sha(p)==digest,'exact source review');j=json.loads(p.read_text(encoding='utf-8'))
        pub.need(j['status']==status and j[pinkey]==101 and j['integration_pin_mismatches']==0 and j['integration_manifest_sha256']==pub.sha(D/'INTEGRATION.json')=='e3f020fe72a47a5f3537105c3355cfb80a76d5716317d7b6a11bfd879ff63ead','actual source review and source freeze')
        for path,digest in j['reviewed_files_sha256'].items():pub.need(pub.sha(F/path)==digest,'current reviewed source '+path)
    for key,name in [('archive_sha256','libmka_native_retail_startup.a'),('frame_archive_sha256','libmka_native_retail_middleware_frame.a'),('alias_archive_sha256','libmka_native_retail_file_paths.a'),('profile_archive_sha256','libmka_native_retail_profile_owners.a'),('game_session_archive_sha256','libmka_native_retail_game_session.a'),('keyboard_archive_sha256','libmka_native_retail_keyboard.a'),('platform_archive_sha256','libmka_native_retail_platform.a')]:
        pub.need(pub.sha(F/'build_native_services_only_20261119-01'/name)==result[key],'measured archive '+name)
    for row in result['fixtures']:pub.need(pub.sha(F/'build_native_services_only_20261119-01'/(row['target']+'.exe'))==row['sha256'],'retained measured client')
    protected={F/'runtime/src/iop/mcserv.cpp':'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',F/'runtime/src/iop/msl_native.cpp':'756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23',P/'research/doc-audit-20261118/inventory.json':'26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6'}
    for path,digest in protected.items():pub.need(pub.sha(path)==digest,'protected bytes')
    bases={F:'6f971ece7a306e64ff8e3da7d111f14161951578',P:'a9cb4980b3bebb94b81fe09f3f94e44899bf3a0f'}
    for root,expected in bases.items():pub.need(pub.git(root,'rev-parse','HEAD')==expected and pub.git(root,'branch','--show-current')=='main','known publication base')
    authored=['README.md','cmake/native_game.cmake','recomp/native_startup_codegen.cpp','runtime/src/native/retail_file_startup.cpp','runtime/src/native/retail_main.cpp','runtime/include/mka/retail_middleware_frame.hpp','runtime/src/native/retail_middleware_frame.cpp','runtime/src/native/retail_main_control.hpp','runtime/src/native/retail_main_reentry.cpp','runtime/src/native/retail_middleware_main.cpp','tests/test_retail_middleware_frame.cpp','tests/test_retail_middleware_main.cpp','tests/test_retail_file_startup.cpp']
    forks={F/name for name in authored}|{F/'research/runs/native-retail-file-startup-20261121/PUBLICATION.json'}
    forks.update(p for p in H.rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.name!='PUBLICATION.json' and not {'owned-content','disposable-package-errors','exclusive-startup-content','volume-owner-content'} & set(p.relative_to(H).parts))
    parents={P/'PORT_STATUS.md',P/'ROADMAP.md'}
    parents.update(p for p in D.rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.suffix!='.exe')
    for path in forks|parents:
        if path.suffix in ['.bin','.raw','.tsv','.txt','.log','.csv']:continue
        if path.name.startswith(('RAW','ORIGINAL_WORDS')) or path.stem.endswith('_RAW'):continue
        raw=path.read_bytes()
        pub.need(b'\r' not in raw and raw.endswith(b'\n') and not raw.endswith(b'\n\n'),'LF/EOF '+str(path))
        pub.need(all(line==line.rstrip(b' \t') for line in raw.splitlines()),'whitespace '+str(path))
    fr,pr=pub.remote_probe(F,bases[F]),pub.remote_probe(P,bases[P])
    fh,fs=previous.commit_sync(F,sorted(forks),'Connect native retail middleware main pump and persistent native time',fr)
    marker='Latest measured working-tree native-retail-middleware-time-18 (base fork '+chr(96)+bases[F]+chr(96)+')'
    for name in ['PORT_STATUS.md','ROADMAP.md']:
        path=P/name;text=path.read_text(encoding='utf-8');pub.need(text.count(marker)==1,'one current marker')
        path.write_text(text.replace(marker,'Published native middleware main/time checkpoint fork '+chr(96)+fh+chr(96)+' (work18)'),encoding='utf-8',newline='\n')
    ph,ps=previous.commit_sync(P,sorted(parents),'Record independently verified native middleware main and time checkpoint',pr)
    for path,digest in result['sources'].items():pub.need(pub.sha(path)==digest,'source through publication')
    for path,digest in protected.items():pub.need(pub.sha(path)==digest,'protected through publication')
    record=dict(fork_commit=fh,fork=fs,parent_commit=ph,parent=ps,verification_sha256=pub.sha(vpath),scope='32 native CPU/file clients,102 pins,39 unresolved integrations; original callback producers, CRT lifetime precondition, symlink and game/endpoint acceptance OPEN')
    with (H/'PUBLICATION.json').open('x',encoding='utf-8',newline='\n') as stream:json.dump(record,stream,indent=2);stream.write('\n')
    print(json.dumps(record))
