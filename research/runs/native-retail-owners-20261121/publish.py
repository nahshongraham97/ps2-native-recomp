"""Publish only measured owner integration; unbuilt mode/audio drafts excluded."""
import importlib.util,json
from pathlib import Path
F=Path(__file__).resolve().parents[3];P=F.parents[2]
HERE=Path(__file__).resolve().parent
S=P/'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
spec=importlib.util.spec_from_file_location('pub',F/'research/runs/native-connected-retail-entry-20261120/publish.py')
pub=importlib.util.module_from_spec(spec);spec.loader.exec_module(pub)
if __name__=='__main__':
    result=json.loads((HERE/'validation-01/verification.json').read_text(encoding='utf-8'))
    pub.need(result['status']=='PASS_NATIVE_RETAIL_OWNERS_CPU_CHECKS' and not result['complete_game_entry_linkable'] and len(result['remaining_integration_symbols'])==64,'actual measured owner scope')
    for path,digest in result['sources'].items():pub.need(pub.sha(path)==digest,'current measured source '+path)
    review=json.loads((S/'constructor-integration-03/EXECUTION_REVIEW_20261121.json').read_text(encoding='utf-8'))
    pub.need(review['status']=='SAVED_NATIVE_CPU_CHECKPOINT_VERIFIED_ENTRY_OPEN' and review['source_pin_mismatches']==0 and review['fixture_pin_mismatches']==0 and review['source_pins_checked']==len(result['sources']),'independent complete saved result review')
    for row in result['fixtures']:pub.need(pub.sha(F/'build_native_services_only_20261119-01'/(row['target']+'.exe'))==row['sha256'],'actual retained fixture binary')
    pub.need(pub.sha(F/'build_native_services_only_20261119-01/libmka_native_retail_startup.a')==result['archive_sha256'],'actual retained startup archive')
    bases={F:'7143a45f4f018d6010658d5746d3ac709bf4a4ee',P:'6ef8dacb277c0680cfa056fcf803fb8caff3778a'}
    for root,expected in bases.items():pub.need(pub.git(root,'rev-parse','HEAD')==expected and pub.git(root,'branch','--show-current')=='main','expected checkpoint bases')
    protected={F/'runtime/src/iop/mcserv.cpp':'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',F/'runtime/src/iop/msl_native.cpp':'756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23',P/'research/doc-audit-20261118/inventory.json':'26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6'}
    for path,digest in protected.items():pub.need(pub.sha(path)==digest,'protected bytes')
    forks={Path(q) for q in result['sources']};forks.add(F/'README.md')
    forks.add(F/'research/runs/native-retail-constructors-20261120/PUBLICATION.json')
    forks.update(q for q in HERE.rglob('*') if q.is_file() and '__pycache__' not in q.parts and q.name!='PUBLICATION.json')
    parents={P/'PORT_STATUS.md',P/'ROADMAP.md'}
    for folder in ['dispatch-constructor','record-constructors','service-constructors-20261121','input-main-20261121','constructor-integration-03']:
        parents.update(q for q in (S/folder).rglob('*') if q.is_file() and '__pycache__' not in q.parts)
    parents.update(q for q in (P/'research/runs/native-donor-replacements-20261120').rglob('*') if q.is_file() and '__pycache__' not in q.parts)
    for path in forks|parents:
        rel=path.relative_to(F) if path.is_relative_to(F) else path.relative_to(P)
        if 'before' in str(rel) or 'integration-01' in rel.parts or path.suffix in ['.csv','.log','.txt']:continue
        raw=path.read_bytes();pub.need(b'\r' not in raw and raw.endswith(b'\n') and not raw.endswith(b'\n\n'),'authored LF/EOF '+str(path));pub.need(all(line==line.rstrip(b' \t') for line in raw.splitlines()),'authored whitespace '+str(path))
    for path in [S/'constructor-integration-03/RESULTS.md']:
        text=path.read_text(encoding='utf-8').replace('Independent saved-result review is pending.','Independent saved-result review verifies all15 source/tool pins, five executable receipts and the exact64-symbol negative link. This does not establish complete-game acceptance.')
        path.write_text(text,encoding='utf-8',newline='\n')
    fr,pr=pub.remote_probe(F,bases[F]),pub.remote_probe(P,bases[P])
    fh,fs=pub.commit_sync(F,sorted(forks),'Connect native profile and prefix constructor owners; preserve original input role identities',fr)
    for name in ['PORT_STATUS.md','ROADMAP.md']:
        path=P/name;text=path.read_text(encoding='utf-8');needle='Current working-tree constructor integration-03 (base fork `7143a45f4f018d6010658d5746d3ac709bf4a4ee`)'
        pub.need(needle in text,'single live checkpoint marker')
        text=text.replace(needle,'Published native owner checkpoint fork `'+fh+'` (constructor integration-03)')
        path.write_text(text,encoding='utf-8',newline='\n')
    ph,ps=pub.commit_sync(P,sorted(parents),'Record native owner evidence and source-backed donor reuse decision',pr)
    for path,digest in result['sources'].items():pub.need(pub.sha(path)==digest,'measured source stable after publication')
    for path,digest in protected.items():pub.need(pub.sha(path)==digest,'protected bytes after publication')
    record=dict(fork_commit=fh,fork=fs,parent_commit=ph,parent=ps,verification_sha256=pub.sha(HERE/'validation-01/verification.json'),scope='Five CPU/component fixtures; whole native entry and game acceptance OPEN')
    with (HERE/'PUBLICATION.json').open('x',encoding='utf-8',newline='\n') as stream:json.dump(record,stream,indent=2);stream.write('\n')
    print(json.dumps(record))
