"""Publish measured native archive and static constructor implementation only."""
import importlib.util,json
from pathlib import Path
F=Path(__file__).resolve().parents[3];P=F.parents[2]
HERE=Path(__file__).resolve().parent
S=P/'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
spec=importlib.util.spec_from_file_location('pub',F/'research/runs/native-connected-retail-entry-20261120/publish.py')
pub=importlib.util.module_from_spec(spec);spec.loader.exec_module(pub)
if __name__=='__main__':
    result=json.loads((HERE/'validation-01/verification.json').read_text(encoding='utf-8'))
    pub.need(result['status']=='PASS_NATIVE_PACKAGE_AND_STATIC_CONSTRUCTOR_CPU_FILE_CHECKS' and not result['complete_game_entry_linkable'] and len(result['remaining_integration_symbols'])==52,'actual measured owner scope')
    for path,digest in result['sources'].items():pub.need(pub.sha(path)==digest,'current measured source '+path)
    review=json.loads((S/'package-and-constructors-05/EXECUTION_REVIEW_20261121.json').read_text(encoding='utf-8'))
    pub.need(review['status']=='SCOPED_SAVED_EXECUTION_VERIFIED' and review['source_pins_checked']==len(result['sources'])==31 and review['fixture_receipts']['exit_zero']==11 and review['fixture_receipts']['executable_digest_matches_before_after_current']==11 and review['negative_link']['current_exactly_matches_saved_link_output'] is True and review['findings']==[],'actual independent execution review schema')
    source_review=json.loads((S/'remaining-constructors-20261121/FINAL_DELTA_REVIEW_20261121.json').read_text(encoding='utf-8'))
    pub.need(source_review['status']=='SCOPED_SOURCE_READY_FOR_CPU_FIXTURES' and source_review['blocking_source_defect_found'] is False,'actual integrated source review')
    for path,digest in source_review['input_sha256'].items():pub.need(pub.sha(P/path)==digest,'current source-review pin '+path)
    for path,digest in result['independent_manifest_sha256'].items():pub.need(pub.sha(path)==digest,'current independent manifest '+path)
    package_review=S/'package-bootstrap-20261121/DELTA_REVIEW_20261121.md'
    pub.need(package_review.is_file() and 'all 245 rows' in package_review.read_text(encoding='utf-8'),'actual corrected package source review')
    for row in result['fixtures']:pub.need(pub.sha(F/'build_native_services_only_20261119-01'/(row['target']+'.exe'))==row['sha256'],'actual retained fixture binary')
    pub.need(pub.sha(F/'build_native_services_only_20261119-01/libmka_native_retail_startup.a')==result['archive_sha256'],'actual retained startup archive')
    bases={F:'ca9f3aee3aa8364a58f7f9ec7f34bb6d597d4796',P:'be78f4b0777d8712bcbe93bb73cc118da9f4e600'}
    for root,expected in bases.items():pub.need(pub.git(root,'rev-parse','HEAD')==expected and pub.git(root,'branch','--show-current')=='main','expected checkpoint bases')
    protected={F/'runtime/src/iop/mcserv.cpp':'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',F/'runtime/src/iop/msl_native.cpp':'756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23',P/'research/doc-audit-20261118/inventory.json':'26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6'}
    for path,digest in protected.items():pub.need(pub.sha(path)==digest,'protected bytes')
    forks={Path(q) for q in result['sources']};forks.add(F/'README.md')
    forks.add(F/'research/runs/native-retail-algorithms-20261121/PUBLICATION.json')
    forks.update(q for q in HERE.rglob('*') if q.is_file() and '__pycache__' not in q.parts and q.name!='PUBLICATION.json' and not {'owned-content','disposable-package-errors'} & set(q.relative_to(HERE).parts))
    parents={P/'PORT_STATUS.md',P/'ROADMAP.md'}
    for folder in ['package-bootstrap-20261121','remaining-constructors-20261121','package-and-constructors-05','action-journal-20261121']:
        parents.update(q for q in (S/folder).rglob('*') if q.is_file() and '__pycache__' not in q.parts)
    for path in forks|parents:
        rel=path.relative_to(F) if path.is_relative_to(F) else path.relative_to(P)
        if 'before' in str(rel) or 'integration-01' in rel.parts or path.suffix in ['.csv','.log','.txt']:continue
        raw=path.read_bytes();pub.need(b'\r' not in raw and raw.endswith(b'\n') and not raw.endswith(b'\n\n'),'authored LF/EOF '+str(path));pub.need(all(line==line.rstrip(b' \t') for line in raw.splitlines()),'authored whitespace '+str(path))
    for path in [S/'package-and-constructors-05/RESULTS.md']:
        text=path.read_text(encoding='utf-8').replace('Independent saved-execution review is pending.','Independent saved-execution review verifies all31 source/tool pins, eleven executable receipts, all three independent manifests, the original package asset and the exact52-symbol negative link. This does not establish complete-game acceptance.')
        path.write_text(text,encoding='utf-8',newline='\n')
    fr,pr=pub.remote_probe(F,bases[F]),pub.remote_probe(P,bases[P])
    fh,fs=pub.commit_sync(F,sorted(forks),'Read retail PAK natively and initialize source static keys with safe callback ownership',fr)
    for name in ['PORT_STATUS.md','ROADMAP.md']:
        path=P/name;text=path.read_text(encoding='utf-8');needle='Latest measured working-tree package-and-constructors-05 (base fork `ca9f3aee3aa8364a58f7f9ec7f34bb6d597d4796`)'
        pub.need(needle in text,'single live checkpoint marker')
        text=text.replace(needle,'Published native package/constructor checkpoint fork `'+fh+'` (integration05)')
        path.write_text(text,encoding='utf-8',newline='\n')
    ph,ps=pub.commit_sync(P,sorted(parents),'Record original archive and static constructor verification with exact remaining native bindings',pr)
    for path,digest in result['sources'].items():pub.need(pub.sha(path)==digest,'measured source stable after publication')
    for path,digest in protected.items():pub.need(pub.sha(path)==digest,'protected bytes after publication')
    record=dict(fork_commit=fh,fork=fs,parent_commit=ph,parent=ps,verification_sha256=pub.sha(HERE/'validation-01/verification.json'),scope='Eleven CPU/file fixtures; native archive/static constructor scope only, complete game acceptance OPEN')
    with (HERE/'PUBLICATION.json').open('x',encoding='utf-8',newline='\n') as stream:json.dump(record,stream,indent=2);stream.write('\n')
    print(json.dumps(record))
