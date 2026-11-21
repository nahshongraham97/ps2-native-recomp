"""Publish measured real native file startup; exact scope and explicit skipped coverage."""
import importlib.util,json
from pathlib import Path
F=Path(__file__).resolve().parents[3];P=F.parents[2];H=Path(__file__).resolve().parent
C=P/'research/runs/native-retail-startup-20261120/connected-retail-entry-01';D=C/'native-retail-file-startup-17'
spec=importlib.util.spec_from_file_location('previous',F/'research/runs/native-connected-main-dependencies-20261121/publish.py')
previous=importlib.util.module_from_spec(spec);spec.loader.exec_module(previous)
pub=previous.pub
if __name__=='__main__':
    vpath=H/'validation-02/verification.json'
    pub.need(pub.sha(vpath)=='553ccd7a86dd9d56f8a62910eacf3fd2b5ebafe103d6e5c663aaedad858ca6df','exact producer result')
    result=json.loads(vpath.read_text(encoding='utf-8'))
    pub.need(result['status']=='PASS_NATIVE_RETAIL_FILE_STARTUP_CPU_FILE_CHECKS' and len(result['sources'])==95 and len(result['fixtures'])==30 and len(result['remaining_integration_symbols'])==40 and result['symlink_creation_exercised'] is False and not result['complete_game_entry_linkable'] and not result['game_executed'] and not result['physical_endpoints_opened'],'actual scoped result including skipped branch')
    for path,digest in result['sources'].items():pub.need(pub.sha(path)==digest,'stable measured source '+path)
    rpath=D/'review-saved-execution01/EXECUTION_REVIEW.json'
    pub.need(pub.sha(rpath)=='361e0b5b433f3ae1fc65c2769a70f42976d6c8fd4e791fe6faa68261dee75c7b','exact independent saved review')
    review=json.loads(rpath.read_text(encoding='utf-8'))
    pub.need(review['schema']=='native-retail-file-startup-17-independent-saved-execution-review-v1' and review['status']=='PASS_SAVED_NATIVE_RETAIL_FILE_STARTUP_CPU_FILE_EVIDENCE' and review['source_pins_checked']==95 and review['fixtures_checked']==30 and review['reused_receipts_verified']==30 and review['sources']==result['sources'] and review['remaining_integration_symbols']==result['remaining_integration_symbols'] and review['symlink_creation_exercised'] is False and review['negative_entry_executable_absent'] is True,'actual independent review schema')
    spath=D/'review-final-source01/SOURCE_REVIEW.json'
    pub.need(pub.sha(spath)=='870acf46785b8c9517dfc70f08c6679aba4480d4ce8e7d1e763c64ab9cab7a26','exact final source review')
    source=json.loads(spath.read_text(encoding='utf-8'))
    pub.need(source['schema']=='native-retail-file-startup-final-source-review-v1' and source['status']=='SOURCE_REVIEW_PASSED_UNBUILT' and source['verified_pins']==94 and source['pin_mismatches']==0 and source['manifest_sha256']==pub.sha(D/'IMPLEMENTATION.json')=='c7f92c22833727cbac9a59c56c593b7a25f4a150f8b836613393aa5063b5877a','actual dated source review and unchanged manifest')
    for key,name in [('archive_sha256','libmka_native_retail_startup.a'),('alias_archive_sha256','libmka_native_retail_file_paths.a'),('profile_archive_sha256','libmka_native_retail_profile_owners.a'),('game_session_archive_sha256','libmka_native_retail_game_session.a'),('keyboard_archive_sha256','libmka_native_retail_keyboard.a'),('platform_archive_sha256','libmka_native_retail_platform.a')]:
        pub.need(pub.sha(F/'build_native_services_only_20261119-01'/name)==result[key],'actual archive '+name)
    for row in result['fixtures']:pub.need(pub.sha(F/'build_native_services_only_20261119-01'/(row['target']+'.exe'))==row['sha256'],'retained actual client')
    protected={F/'runtime/src/iop/mcserv.cpp':'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',F/'runtime/src/iop/msl_native.cpp':'756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23',P/'research/doc-audit-20261118/inventory.json':'26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6'}
    for path,digest in protected.items():pub.need(pub.sha(path)==digest,'protected bytes')
    bases={F:'703725f2218d79a59f2a916a218cf04459a4989e',P:'52bcb4b41eb0b4b8f559d54ced4f529550bc9a0a'}
    for root,expected in bases.items():pub.need(pub.git(root,'rev-parse','HEAD')==expected and pub.git(root,'branch','--show-current')=='main','known publication base')
    authored=['README.md','cmake/native_game.cmake','recomp/native_startup_codegen.cpp',
        'runtime/include/mka/content_volume.hpp','runtime/src/native/content_volume.cpp',
        'runtime/include/mka/retail_middleware_work.hpp','runtime/src/native/retail_middleware_work.cpp',
        'runtime/include/mka/retail_main.hpp','runtime/src/native/retail_main_state.cpp',
        'runtime/src/native/retail_file_startup.cpp','tests/test_retail_file_startup.cpp','tests/test_retail_package.cpp']
    forks={F/name for name in authored}|{F/'research/runs/native-connected-main-dependencies-20261121/PUBLICATION.json'}
    forks.update(p for p in H.rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.name!='PUBLICATION.json' and
        not {'owned-content','disposable-package-errors','exclusive-startup-content','volume-owner-content'} & set(p.relative_to(H).parts))
    parents={P/'PORT_STATUS.md',P/'ROADMAP.md'}
    for scope in [D,C/'native-retail-middleware-files-15/completion-closure02']:
        parents.update(p for p in scope.rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.suffix!='.exe')
    for path in forks|parents:
        if path.suffix in ['.bin','.raw','.tsv','.txt','.log','.csv']:continue
        if path.name.startswith(('RAW','ORIGINAL_WORDS')) or path.stem.endswith('_RAW'):continue
        raw=path.read_bytes()
        pub.need(b'\r' not in raw and raw.endswith(b'\n') and not raw.endswith(b'\n\n'),'LF/EOF '+str(path))
        pub.need(all(line==line.rstrip(b' \t') for line in raw.splitlines()),'whitespace '+str(path))
    fr,pr=pub.remote_probe(F,bases[F]),pub.remote_probe(P,bases[P])
    fh,fs=previous.commit_sync(F,sorted(forks),'Implement real native retail file startup and safe middleware ownership',fr)
    marker='Latest measured working-tree native-retail-file-startup-17 (base fork '+chr(96)+bases[F]+chr(96)+')'
    for name in ['PORT_STATUS.md','ROADMAP.md']:
        path=P/name;text=path.read_text(encoding='utf-8');pub.need(text.count(marker)==1,'one current marker')
        path.write_text(text.replace(marker,'Published native file-startup checkpoint fork '+chr(96)+fh+chr(96)+' (work17)'),encoding='utf-8',newline='\n')
    ph,ps=previous.commit_sync(P,sorted(parents),'Record verified native file startup and exact completion/time source boundary',pr)
    for path,digest in result['sources'].items():pub.need(pub.sha(path)==digest,'source through publication')
    for path,digest in protected.items():pub.need(pub.sha(path)==digest,'protected bytes through publication')
    record=dict(fork_commit=fh,fork=fs,parent_commit=ph,parent=ps,verification_sha256=pub.sha(vpath),scope='30 native CPU/file clients,95 pins,40 remaining integrations; symlink case skipped, game/endpoint acceptance OPEN')
    with (H/'PUBLICATION.json').open('x',encoding='utf-8',newline='\n') as stream:json.dump(record,stream,indent=2);stream.write('\n')
    print(json.dumps(record))
