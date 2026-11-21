"""Publish measured persistent native session; exact scope, no game acceptance."""
import importlib.util,json
from pathlib import Path
F=Path(__file__).resolve().parents[3];P=F.parents[2];HERE=Path(__file__).resolve().parent
S=P/'research/runs/native-retail-startup-20261120/connected-retail-entry-01';D=S/'native-main-session-13'
spec=importlib.util.spec_from_file_location('pub',F/'research/runs/native-connected-retail-entry-20261120/publish.py')
pub=importlib.util.module_from_spec(spec);spec.loader.exec_module(pub)
if __name__=='__main__':
    verification=HERE/'validation-03/verification.json'
    pub.need(pub.sha(verification)=='b1f2064fab20de6059861c079b328660be09e2879832201feec93a4ed5e77db7','exact measured result')
    result=json.loads(verification.read_text(encoding='utf-8'))
    pub.need(result['status']=='PASS_NATIVE_MAIN_SESSION_CPU_FILE_CHECKS' and len(result['sources'])==74 and len(result['fixtures'])==22 and len(result['remaining_integration_symbols'])==41 and not result['complete_game_entry_linkable'] and not result['game_executed'] and not result['physical_endpoints_opened'],'actual measured scope')
    for path,digest in result['sources'].items():pub.need(pub.sha(path)==digest,'stable measured source '+path)
    execution=D/'independent-review01/EXECUTION_REVIEW.json'
    pub.need(pub.sha(execution)=='d347e5345f81a77ffff2face2f5d39c5ad26f8e7972a5743e0d1f5fc7aa3efea','exact saved-execution review')
    review=json.loads(execution.read_text(encoding='utf-8'))
    pub.need(review['schema']=='native-main-session-saved-execution-independent-review-v1' and review['status']=='PASS_SAVED_NATIVE_MAIN_SESSION_CPU_FILE_EVIDENCE' and review['source_pins_checked']==74 and review['fixture_count']==22 and review['sources']==result['sources'] and review['remaining_integration_symbols']==result['remaining_integration_symbols'] and review['findings']==[],'actual execution review schema and pins')
    source_review=D/'independent-review01/INDEPENDENT_SOURCE_REVIEW.json'
    pub.need(pub.sha(source_review)=='ffc09c216f3cb92ad3aa7665f63e3b36e86764e00f68f5241f64573b79011ae4','exact independent source review')
    source=json.loads(source_review.read_text(encoding='utf-8'))
    pub.need(source['schema']=='native-main-session-independent-source-review-v1' and source['status']=='SCOPED_SOURCE_READY_FOR_CPU_FIXTURES' and source['source_pins_checked']==73 and source['findings']==[],'source review schema')
    for path,digest in source['sources'].items():pub.need(pub.sha(path)==digest,'current reviewed source')
    for key,name in [('archive_sha256','libmka_native_retail_startup.a'),('game_session_archive_sha256','libmka_native_retail_game_session.a'),('keyboard_archive_sha256','libmka_native_retail_keyboard.a'),('platform_archive_sha256','libmka_native_retail_platform.a')]:
        pub.need(pub.sha(F/'build_native_services_only_20261119-01'/name)==result[key],'measured archive '+name)
    for row in result['fixtures']:pub.need(pub.sha(F/'build_native_services_only_20261119-01'/(row['target']+'.exe'))==row['sha256'],'retained actual client')
    protected=review['protected_sha256']
    for path,digest in protected.items():pub.need(pub.sha(path)==digest,'protected bytes')
    bases={F:'4cd209a5098506d0b5b78ed4f7d3757427b488a6',P:'8ec545657b6eb777b1b8ecb71e4d850c3bf5d22f'}
    for root,expected in bases.items():pub.need(pub.git(root,'rev-parse','HEAD')==expected and pub.git(root,'branch','--show-current')=='main','publication base')
    authored=['recomp/native_startup_codegen.cpp','cmake/native_game.cmake','runtime/include/mka/retail_main.hpp','runtime/src/native/retail_main.cpp','runtime/src/native/retail_auxiliary_words.cpp',
        'runtime/include/mka/retail_game_controls.hpp','runtime/src/native/retail_game_controls.cpp',
        'runtime/include/mka/retail_main_settings.hpp','runtime/src/native/retail_main_settings.cpp',
        'runtime/include/mka/retail_random.hpp','runtime/src/native/retail_random.cpp',
        'runtime/include/mka/retail_game_session.hpp','runtime/src/native/retail_game_session.cpp',
        'runtime/src/native/retail_game_session_startup.cpp','runtime/src/native/retail_main_settings_binding.cpp',
        'tests/test_retail_main_settings.cpp','tests/test_retail_random.cpp','tests/test_retail_game_session.cpp',
        'tests/test_retail_bound_owners.cpp','tests/test_native_startup_codegen.cpp','README.md']
    forks={F/name for name in authored}|{F/'research/runs/native-retail-input-ownership-20261121/PUBLICATION.json'}
    forks.update(p for p in HERE.rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.name!='PUBLICATION.json'
        and not {'owned-content','disposable-package-errors','exclusive-startup-content'} & set(p.relative_to(HERE).parts))
    parents={P/'PORT_STATUS.md',P/'ROADMAP.md'}
    for folder in ['native-main-settings-10','native-resource-values-11','native-retail-random-12','native-main-session-13']:
        parents.update(p for p in (S/folder).rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.suffix!='.exe'
            and 'next-main-gates-source01' not in p.parts)
    # Raw original exports and preserved before snapshots retain exact bytes.
    for path in forks|parents:
        rel=path.relative_to(F) if path.is_relative_to(F) else path.relative_to(P)
        if 'before' in str(rel) or path.suffix in ['.bin','.raw','.tsv','.txt','.log']:continue
        if path.name.startswith(('RAW','ORIGINAL_WORDS')) or path.stem.endswith('_RAW'):continue
        raw=path.read_bytes()
        pub.need(b'\r' not in raw and raw.endswith(b'\n') and not raw.endswith(b'\n\n'),'authored LF/EOF '+str(path))
        pub.need(all(line==line.rstrip(b' \t') for line in raw.splitlines()),'authored whitespace '+str(path))
    fr,pr=pub.remote_probe(F,bases[F]),pub.remote_probe(P,bases[P])
    fh,fs=pub.commit_sync(F,sorted(forks),'Connect persistent native retail settings RNG and auxiliary state to main',fr)
    marker='Latest measured working-tree native-main-session-13 (base fork '+chr(96)+bases[F]+chr(96)+')'
    for name in ['PORT_STATUS.md','ROADMAP.md']:
        path=P/name;text=path.read_text(encoding='utf-8');pub.need(text.count(marker)==1,'one current marker')
        path.write_text(text.replace(marker,'Published native persistent-session checkpoint fork '+chr(96)+fh+chr(96)+' (integration13)'),encoding='utf-8',newline='\n')
    ph,ps=pub.commit_sync(P,sorted(parents),'Record verified native session and original recomp random oracle checkpoint',pr)
    for path,digest in result['sources'].items():pub.need(pub.sha(path)==digest,'source stable through publication')
    for path,digest in protected.items():pub.need(pub.sha(path)==digest,'protected bytes through publication')
    record=dict(fork_commit=fh,fork=fs,parent_commit=ph,parent=ps,verification_sha256=pub.sha(verification),
        scope='22 native CPU/file clients,74 pins,41 remaining integration symbols; no complete game/endpoint run')
    with (HERE/'PUBLICATION.json').open('x',encoding='utf-8',newline='\n') as out:json.dump(record,out,indent=2);out.write('\n')
    print(json.dumps(record))
