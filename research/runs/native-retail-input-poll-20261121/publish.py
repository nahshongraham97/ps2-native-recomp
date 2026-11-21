"""Publish native assigned-input checkpoint with exact measured and preserved evidence."""
import importlib.util,json
from pathlib import Path
F=Path(__file__).resolve().parents[3];P=F.parents[2];H=Path(__file__).resolve().parent
C=P/'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
D=C/'native-retail-input-poll-19';R=C/'native-retail-input-control-21'
spec=importlib.util.spec_from_file_location('previous',F/'research/runs/native-connected-main-dependencies-20261121/publish.py')
previous=importlib.util.module_from_spec(spec);spec.loader.exec_module(previous);pub=previous.pub
if __name__=='__main__':
    vpath=H/'validation-01/verification.json'
    pub.need(pub.sha(vpath)=='0c1b7bfad32fd57d285a2e55b926e57b655e8ccf313b4ec55b3a7be564bb2a7e','exact measured producer')
    result=json.loads(vpath.read_text(encoding='utf-8'))
    pub.need(result['status']=='PASS_NATIVE_RETAIL_ASSIGNED_INPUT_CPU_FILE_CHECKS' and len(result['sources'])==114 and len(result['fixtures'])==36 and len(result['remaining_integration_symbols'])==39 and result['symlink_creation_exercised'] is False and not result['complete_game_entry_linkable'] and not result['game_executed'] and not result['physical_endpoints_opened'],'exact bounded input scope')
    for path,digest in result['sources'].items():pub.need(pub.sha(path)==digest,'measured source '+path)
    rpath=D/'review-saved-execution01/EXECUTION_REVIEW.json'
    pub.need(pub.sha(rpath)=='642733be4c90d75763a93b043f658b1330f82541c819a965c11af87d3b335ec3','independent saved review')
    review=json.loads(rpath.read_text(encoding='utf-8'))
    pub.need(review['schema']=='native-retail-assigned-input-saved-execution-independent-review-v1' and review['status']=='VERIFIED_SAVED_CPU_FILE_CHECKS' and review['source_pins_checked']==114 and review['source_pin_mismatches']==0 and review['source_sha256']==result['sources'] and review['fixture_executions_checked']==36 and review['fixture_exit_failures']==review['fixture_binary_identity_failures']==review['fixture_symbol_target_failures']==0 and review['archive_identity_failures']==0 and review['remaining_integration_symbols']==result['remaining_integration_symbols'] and review['negative_link_exact_symbols'] and not review['negative_entry_executable_exists'] and review['symlink_creation_exercised'] is False and 'prebuild_source_correction' in review and 'report_only_correction' not in review,'actual saved-review schema')
    spath=D/'review-source01/REVIEW.json'
    pub.need(pub.sha(spath)=='3a0e708fdb7269bfe5bcff4490c3fc195158a1c2e563ee43a911fd5b18492344','exact current independent source review')
    source=json.loads(spath.read_text(encoding='utf-8'))
    pub.need(source['schema']=='native-retail-assigned-input-independent-source-review-v1' and source['status']=='READY_FOR_CPU_FILE_FIXTURES' and source['source_pins_checked']==113 and source['source_pin_mismatches']==0 and source['source_record_pins_checked']==5 and source['source_record_pin_mismatches']==0 and source['findings']==[] and source['integration_manifest_sha256']==pub.sha(D/'INTEGRATION.json')=='a963ca9d3a5510f4f6c781acdcb8c9789450968b3d6f89b2c75826a0c5bd8532','source freeze R2')
    for path,digest in source['reviewed_file_sha256'].items():pub.need(pub.sha(F/path)==digest,'actual reviewed input source')
    rootpath=R/'review-root-source01/REVIEW.json'
    pub.need(pub.sha(rootpath)=='97dcf8d9cb3836fa455d1503ae1e3f3bca78ad2bc4e01f4a92222ab0df2fa2a2','exact root-control review')
    rootreview=json.loads(rootpath.read_text(encoding='utf-8'))
    pub.need(rootreview['status']=='SOURCE_REVIEW_NO_BLOCKING_DEFECT_IN_ROOT21_SCOPE' and rootreview['raw_words_checked']==45 and rootreview['raw_bytes_match_original_elf'] and rootreview['verifier_client_rows']==36 and rootreview['all_client_targets_declared'],'actual root source review')
    for path,digest in rootreview['reviewed_sha256'].items():pub.need(pub.sha(path)==digest,'actual root-reviewed file')
    freeze=json.loads((D/'INTEGRATION.json').read_text(encoding='utf-8'))
    for path,digest in freeze['source_records'].items():pub.need(pub.sha(path)==digest,'frozen source record')
    delta=json.loads((D/'review-saved-execution01/REVIEW_METADATA_CORRECTION.json').read_text(encoding='utf-8'))
    for row in delta['changes']:
        current=D/row['artifact']
        old=current.parent/('before-lf-normalization' if current.parent.name=='review-source01' else 'before-metadata-correction')/current.name
        pub.need(pub.sha(current)==row['after_sha256'] and pub.sha(old)==row['before_sha256'],'exact review metadata prior/current chain')
    for key,name in [('archive_sha256','libmka_native_retail_startup.a'),('frame_archive_sha256','libmka_native_retail_middleware_frame.a'),('alias_archive_sha256','libmka_native_retail_file_paths.a'),('profile_archive_sha256','libmka_native_retail_profile_owners.a'),('game_session_archive_sha256','libmka_native_retail_game_session.a'),('keyboard_archive_sha256','libmka_native_retail_keyboard.a'),('platform_archive_sha256','libmka_native_retail_platform.a')]:
        pub.need(pub.sha(F/'build_native_services_only_20261119-01'/name)==result[key],'actual measured archive '+name)
    for row in result['fixtures']:pub.need(pub.sha(F/'build_native_services_only_20261119-01'/(row['target']+'.exe'))==row['sha256'],'retained measured client')
    protected={F/'runtime/src/iop/mcserv.cpp':'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',F/'runtime/src/iop/msl_native.cpp':'756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23',P/'research/doc-audit-20261118/inventory.json':'26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6'}
    for path,digest in protected.items():pub.need(pub.sha(path)==digest,'protected bytes')
    bases={F:'f847f7fbc73f89b433d01e264fd5a5673edd68a5',P:'9688ef356400907daeaff1e2f047211db027e59f'}
    for root,expected in bases.items():pub.need(pub.git(root,'rev-parse','HEAD')==expected and pub.git(root,'branch','--show-current')=='main','known publication base')
    authored=['README.md','cmake/native_game.cmake','runtime/include/mka/controller_input.hpp','runtime/src/native/controller_input.cpp','runtime/src/native/retail_platform.cpp','runtime/include/mka/retail_input_roles.hpp','runtime/src/native/retail_input_roles.cpp','runtime/include/mka/retail_input_poll.hpp','runtime/src/native/retail_input_poll.cpp','runtime/include/mka/retail_game_controls.hpp','runtime/src/native/retail_game_controls.cpp','tests/test_retail_input_poll.cpp','tests/test_retail_game_session.cpp']
    forks={F/name for name in authored}|{F/'research/runs/native-retail-middleware-frame-20261121/PUBLICATION.json'}
    forks.update(p for p in H.rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.name!='PUBLICATION.json' and not {'owned-content','disposable-package-errors','exclusive-startup-content','volume-owner-content'} & set(p.relative_to(H).parts))
    parents={P/'PORT_STATUS.md',P/'ROADMAP.md'}
    for scope in [D,R]:parents.update(p for p in scope.rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.suffix!='.exe')
    raw_records=json.loads((D/'FROZEN_RECORD_BYTES.json').read_text(encoding='utf-8'))['frozen_crlf_sha256']
    for path,digest in raw_records.items():pub.need(pub.sha(path)==digest,'preserved raw CRLF record')
    for path in forks|parents:
        if path.suffix in ['.bin','.raw','.tsv','.txt','.log','.csv']:continue
        if path.name.startswith(('RAW','ORIGINAL_WORDS')) or path.stem.endswith('_RAW'):continue
        raw=path.read_bytes()
        if str(path) in raw_records:
            pub.need(path.suffix in ['.json','.md'] and b'\r\r\n' not in raw and b'\r' not in raw.replace(b'\r\n',b'\n'),'exact raw record line endings')
            raw=raw.replace(b'\r\n',b'\n')
        pub.need(b'\r' not in raw and raw.endswith(b'\n') and not raw.endswith(b'\n\n'),'LF/EOF or exact frozen record '+str(path))
        pub.need(all(line==line.rstrip(b' \t') for line in raw.splitlines()),'whitespace '+str(path))
    fr,pr=pub.remote_probe(F,bases[F]),pub.remote_probe(P,bases[P])
    fh,fs=previous.commit_sync(F,sorted(forks),'Implement native retail assigned input with full precision host samples',fr)
    marker='Latest measured working-tree native-retail-assigned-input-19-21 (base fork '+chr(96)+bases[F]+chr(96)+')'
    for name in ['PORT_STATUS.md','ROADMAP.md']:
        path=P/name;text=path.read_text(encoding='utf-8');pub.need(text.count(marker)==1,'one current marker')
        path.write_text(text.replace(marker,'Published native assigned-input checkpoint fork '+chr(96)+fh+chr(96)+' (work19/21)'),encoding='utf-8',newline='\n')
    ph,ps=previous.commit_sync(P,sorted(parents),'Record verified native assigned input and live analog control checkpoint',pr)
    for path,digest in result['sources'].items():pub.need(pub.sha(path)==digest,'source through publication')
    for path,digest in protected.items():pub.need(pub.sha(path)==digest,'protected through publication')
    record=dict(fork_commit=fh,fork=fs,parent_commit=ph,parent=ps,verification_sha256=pub.sha(vpath),scope='36 native CPU/file clients,114 pins,39 unresolved integrations; whole main poll/physical input/game acceptance OPEN')
    with (H/'PUBLICATION.json').open('x',encoding='utf-8',newline='\n') as stream:json.dump(record,stream,indent=2);stream.write('\n')
    print(json.dumps(record))
