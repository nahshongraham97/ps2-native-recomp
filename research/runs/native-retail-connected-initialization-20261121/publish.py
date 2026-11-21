"""Publish scoped connected initialization and authoritative current evidence."""
from pathlib import Path
import importlib.util,json

F=Path(__file__).resolve().parents[3];P=F.parents[2];H=Path(__file__).resolve().parent
C=P/'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
spec=importlib.util.spec_from_file_location('publisher',F/'research/runs/native-connected-main-dependencies-20261121/publish.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
vpath=H/'validation-03/verification.json';v=json.loads(vpath.read_text())
additional_path=H/'validation-03/ADDITIONAL_MEMORY_GROUP.json'
additional=json.loads(additional_path.read_text())
assert v['status']=='PASS_CONNECTED_RETAIL_INITIALIZATION' and len(v['executed_clients'])==9
assert len(v['remaining_integration_symbols'])==33 and not v['game_executed'] and not v['physical_endpoints_opened']
assert additional['status']=='PASS_CHANGED_NATIVE_MEMORY_GROUP' and additional['primary_verification_sha256']==m.pub.sha(vpath)
for path,pin in {**v['sources'],**v['protected_inputs']}.items():assert m.pub.sha(path)==pin,path
clients=v['executed_clients']+[additional['executed_client']]
for row in clients:
    assert m.pub.sha(F/'build_native_services_only_20261119-01'/(row['target']+'.exe'))==row['sha256'],row['target']
assert m.pub.sha(F/'build_native_services_only_20261119-01/libmka_native_retail_startup.a')==v['startup_archive_sha256']
assert not (F/'build_native_services_only_20261119-01/test_native_startup_missing_bindings.exe').exists()
forks={F/n for n in v['authored_paths']}
for folder in [H,F/'research/runs/native-retail-connected-memory-20261121']:
    forks.update(p for p in folder.rglob('*') if p.is_file() and '__pycache__' not in p.parts and
        'disposable-initial-task-content' not in p.parts and p.suffix not in ['.exe','.obj','.a','.dll'] and p.name!='PUBLICATION.json')
parents={P/'PORT_STATUS.md',P/'ROADMAP.md'}
for folder in C.iterdir():
    if folder.is_dir() and folder.name.rsplit('-',1)[-1] in {'37','39','40','41','42','43','44','45'}:
        parents.update(p for p in folder.rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.suffix not in ['.exe','.obj','.a','.dll'])
for n in v['authored_paths']:
    raw=(F/n).read_bytes();assert b'\r' not in raw and raw.endswith(b'\n'),n
for root in [F,P]:assert m.pub.git(root,'branch','--show-current')=='main'
fr=m.pub.remote_probe(F,m.pub.git(F,'rev-parse','HEAD'))
pr=m.pub.remote_probe(P,m.pub.git(P,'rev-parse','HEAD'))
fh,fs=m.commit_sync(F,sorted(forks),'Connect native retail initialization, task bodies, allocation and frame retirement',fr)
marker='Latest verified working-tree connected initialization (base fork9d479eb8, parent88ed302f)'
for name in ['PORT_STATUS.md','ROADMAP.md']:
    path=P/name;s=path.read_text(encoding='utf-8');assert s.count(marker)==1
    path.write_text(s.replace(marker,'Published connected initialization checkpoint fork '+fh),encoding='utf-8',newline='\n')
ph,ps=m.commit_sync(P,sorted(parents),'Record connected native startup evidence and remaining boot work',pr)
result=dict(fork_commit=fh,fork=fs,parent_commit=ph,parent=ps,
    verification_sha256=m.pub.sha(vpath),additional_memory_group_sha256=m.pub.sha(additional_path),
    scope='Ten current native clients including actual1303F0/130350/four bodies/text frame cleanup;33 Main integrations and full game acceptance remain OPEN')
with (H/'PUBLICATION.json').open('x',encoding='utf-8',newline='\n') as stream:json.dump(result,stream,indent=2);stream.write('\n')
print(json.dumps(result))
