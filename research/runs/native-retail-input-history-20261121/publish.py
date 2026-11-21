"""Publish the measured native action-history binding only, no keyboard drafts."""
import importlib.util,json
from pathlib import Path
F=Path(__file__).resolve().parents[3];P=F.parents[2]
HERE=Path(__file__).resolve().parent
S=P/'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
spec=importlib.util.spec_from_file_location('pub',F/'research/runs/native-connected-retail-entry-20261120/publish.py')
pub=importlib.util.module_from_spec(spec);spec.loader.exec_module(pub)
if __name__=='__main__':
    result=json.loads((HERE/'validation-01/verification.json').read_text(encoding='utf-8'))
    pub.need(result['status']=='PASS_NATIVE_ACTION_HISTORY_BINDING_CPU_FILE_CHECKS' and not result['complete_game_entry_linkable'] and len(result['remaining_integration_symbols'])==51 and len(result['sources'])==34 and len(result['fixtures'])==12,'actual measured action-history scope')
    for path,digest in result['sources'].items():pub.need(pub.sha(path)==digest,'current measured source '+path)
    review_path=S/'native-input-state-06/SAVED_EXECUTION_REVIEW_20261121.json'
    pub.need(pub.sha(review_path)=='71bd672ab9396628134756328e96bab9b6145ee505a53214015058d1118d8ccb','independent review exact bytes')
    review=json.loads(review_path.read_text(encoding='utf-8'))
    pub.need(review['schema']=='native-retail-action-history-saved-execution-independent-review-v1' and review['status']=='SCOPED_SAVED_EXECUTION_VERIFIED' and review['source_pins_checked']==34 and review['fixture_receipts_checked']==12 and review['actual_failed_link_matches_exact_set'] is True and review['current_missing_integration_symbols']==result['remaining_integration_symbols'] and review['findings']==[],'actual independent review schema')
    for path,digest in result['independent_manifest_sha256'].items():pub.need(pub.sha(path)==digest,'current manifest '+path)
    for row in result['fixtures']:pub.need(pub.sha(F/'build_native_services_only_20261119-01'/(row['target']+'.exe'))==row['sha256'],'actual retained fixture binary')
    pub.need(pub.sha(F/'build_native_services_only_20261119-01/libmka_native_retail_startup.a')==result['archive_sha256'],'actual retained startup archive')
    bases={F:'19e9e2e948de989a74ff2bd5a4e8c70176ea51c7',P:'71ae9dcb40a13052cf01bf22b34cb9bc2fda601d'}
    for root,expected in bases.items():pub.need(pub.git(root,'rev-parse','HEAD')==expected and pub.git(root,'branch','--show-current')=='main','expected checkpoint bases')
    protected={F/'runtime/src/iop/mcserv.cpp':'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',F/'runtime/src/iop/msl_native.cpp':'756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23',P/'research/doc-audit-20261118/inventory.json':'26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6'}
    for path,digest in protected.items():pub.need(pub.sha(path)==digest,'protected bytes')
    forks={Path(q) for q in result['sources']}|{F/'README.md',F/'research/runs/native-retail-package-constructors-20261121/PUBLICATION.json'}
    forks.update(q for q in HERE.rglob('*') if q.is_file() and '__pycache__' not in q.parts and q.name!='PUBLICATION.json' and not {'owned-content','disposable-package-errors'} & set(q.relative_to(HERE).parts))
    parents={P/'PORT_STATUS.md',P/'ROADMAP.md'}
    for folder in ['native-input-state-06','action-journal-20261121/implementation01']:
        parents.update(q for q in (S/folder).rglob('*') if q.is_file() and '__pycache__' not in q.parts)
    path=S/'native-input-state-06/RESULTS.md'
    data=path.read_text(encoding='utf-8')
    pub.need('Independent saved execution review is pending.' in data,'dated result marker')
    path.write_text(data.replace('Independent saved execution review is pending.','Independent saved execution review verifies all34 source/tool pins, twelve saved executable/receipt identities, the startup archive, three independent manifests, original asset and exact51-symbol negative link. Full-game acceptance remains OPEN.'),encoding='utf-8',newline='\n')
    for path in forks|parents:
        rel=path.relative_to(F) if path.is_relative_to(F) else path.relative_to(P)
        if 'before' in str(rel) or path.suffix in ['.csv','.log','.txt']:continue
        raw=path.read_bytes();pub.need(b'\r' not in raw and raw.endswith(b'\n') and not raw.endswith(b'\n\n'),'authored LF/EOF '+str(path));pub.need(all(line==line.rstrip(b' \t') for line in raw.splitlines()),'authored whitespace '+str(path))
    fr,pr=pub.remote_probe(F,bases[F]),pub.remote_probe(P,bases[P])
    fh,fs=pub.commit_sync(F,sorted(forks),'Bind original action-history initialization and selective reset to native main ownership',fr)
    for name in ['PORT_STATUS.md','ROADMAP.md']:
        path=P/name;data=path.read_text(encoding='utf-8');needle='Latest measured working-tree native-input-state-06 (base fork `19e9e2e948de989a74ff2bd5a4e8c70176ea51c7`)'
        pub.need(data.count(needle)==1,'single live checkpoint marker')
        path.write_text(data.replace(needle,'Published native action-history checkpoint fork `'+fh+'` (integration06)'),encoding='utf-8',newline='\n')
    ph,ps=pub.commit_sync(P,sorted(parents),'Record native action-history source and measured binding verification',pr)
    for path,digest in result['sources'].items():pub.need(pub.sha(path)==digest,'measured source stable after publication')
    for path,digest in protected.items():pub.need(pub.sha(path)==digest,'protected bytes after publication')
    record=dict(fork_commit=fh,fork=fs,parent_commit=ph,parent=ps,verification_sha256=pub.sha(HERE/'validation-01/verification.json'),scope='Twelve CPU/file fixtures; native action-history binding only, full game acceptance OPEN')
    with (HERE/'PUBLICATION.json').open('x',encoding='utf-8',newline='\n') as stream:json.dump(record,stream,indent=2);stream.write('\n')
    print(json.dumps(record))
