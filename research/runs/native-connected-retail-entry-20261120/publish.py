"""Publish the measured startup batch; exclude ongoing constructor work."""
import ast, hashlib, json, pathlib, subprocess
F=pathlib.Path(__file__).resolve().parents[3]
P=F.parents[2]
S=P/'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
HERE=pathlib.Path(__file__).resolve().parent
# Reuse the previously verified exact-stage/remote semantics without executing
# any older publisher's top-level effects.
def extract(relative,names):
    source=(F/relative).read_bytes().decode('utf-8');tree=ast.parse(source)
    return '\n\n'.join(ast.get_source_segment(source,n) for n in tree.body if isinstance(n,ast.FunctionDef) and n.name in names)
exec(extract('research/runs/native-service-runtime-20261120/publish.py',{'sha','need','git','names'}))
commit_source=extract('research/runs/native-wave-allocation-20261120/publish.py',{'commit_sync'})
old="['git', 'diff', '--cached', '--check']"
new="['git', '-c', 'core.whitespace=blank-at-eol,blank-at-eof,space-before-tab,cr-at-eol', 'diff', '--cached', '--check']"
need(old in commit_source,'known staging checker')
exec(commit_source.replace(old,new,1))
exec(extract('research/runs/native-hud-health-geometry-20261120/publish.py',{'remote_probe'}))
def write(path,value):
    with path.open('x',encoding='utf-8',newline='\n') as stream:json.dump(value,stream,indent=2);stream.write('\n')
if __name__=='__main__':
    tree=ast.parse((HERE/'verify.py').read_bytes().decode('utf-8'))
    node=next(n for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='NAMES' for t in n.targets))
    code_names=ast.literal_eval(node.value)
    result=json.loads((HERE/'validation-03/verification.json').read_bytes().decode('utf-8'))
    need(result['status']=='PASS_CONNECTED_STARTUP_BUILD_AND_NATIVE_CPU_FILE_CHECKS' and result['complete_entry_linkable'] is False and len(result['missing_main_bodies'])==67,'measured exact scope')
    for path,expected in result['sources'].items():need(sha(path)==expected,'current measured source:'+path)
    review=json.loads((S/'EXECUTION_REVIEW.json').read_bytes().decode('utf-8'))
    need(review['status']=='SAVED_NATIVE_CONNECTED_RETAIL_ENTRY_COMPONENT_EXECUTION_VERIFIED','execution review')
    fork_expected='f0072538618c518435d213ab227d187395ade7a3';parent_expected='daf54bf0687d5adfe2f6407267a7b0a3b649fa0c'
    need(git(F,'rev-parse','HEAD')==fork_expected and git(P,'rev-parse','HEAD')==parent_expected,'base commits')
    need(git(F,'branch','--show-current')==git(P,'branch','--show-current')=='main','branches')
    protected={F/'runtime/src/iop/mcserv.cpp':'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',F/'runtime/src/iop/msl_native.cpp':'756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23',P/'research/doc-audit-20261118/inventory.json':'26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6'}
    for path,expected in protected.items():need(sha(path)==expected,'protected bytes')
    parents=[P/n for n in ['PORT_STATUS.md','ROADMAP.md','NATIVE_PORT_EXECUTION_CONTRACT.md']]
    parents += [q for q in S.rglob('*') if q.is_file() and not {'initial-constructors','controller-constructor'} & set(q.relative_to(S).parts)]
    unchanged={'host/keymap.hpp','host/keymap.cpp','runtime/include/mka/content_volume.hpp','runtime/src/native/content_volume.cpp','runtime/include/mka/retail_memory_owners.hpp','runtime/src/native/retail_memory_owners.cpp','third_party/sdl2/lib/cmake/SDL2/sdl2-config.cmake','tests/test_native_retail_startup.cpp','tests/test_native_startup_codegen.cpp'}
    forks=[F/n for n in code_names if n not in unchanged]+[F/'README.md']
    forks += [q for q in HERE.rglob('*') if q.is_file() and 'owned-fixtures' not in q.relative_to(HERE).parts and q.name!='PUBLICATION.json']
    forks=sorted(set(forks));parents=sorted(set(parents))
    for path in forks+parents:
        relative=path.relative_to(S) if path.is_relative_to(S) else path.relative_to(F) if path.is_relative_to(F) else path.relative_to(P)
        if 'before' in str(relative) or 'historical-review-source-resolution' in relative.parts:continue
        raw=path.read_bytes();need(b'\r' not in raw and raw.endswith(b'\n') and not raw.endswith(b'\n\n'),'authored LF/EOF:'+str(path));need(all(line==line.rstrip() for line in raw.splitlines()),'authored whitespace:'+str(path))
    fr=remote_probe(F,fork_expected);pr=remote_probe(P,parent_expected)
    fh,fs=commit_sync(F,forks,'Connect native CRT platform and source-ordered retail main; retain missing-body link gate',fr)
    for name in ['PORT_STATUS.md','ROADMAP.md']:
        path=P/name;text=path.read_bytes().decode('utf-8');anchor='Validation-03 passes five current native CPU/file checks';need(anchor in text,'doc checkpoint anchor');text=text.replace(anchor,'Published native source checkpoint fork '+chr(96)+fh+chr(96)+': validation-03 passes five current native CPU/file checks',1);path.write_bytes(text.encode('utf-8'))
    ph,ps=commit_sync(P,parents,'Record connected native startup checkpoint and exact unresolved game bodies',pr)
    for path,expected in protected.items():need(sha(path)==expected,'protected bytes after publication')
    write(HERE/'PUBLICATION.json',{'fork_commit':fh,'fork':fs,'parent_commit':ph,'parent':ps,'verification_sha256':sha(HERE/'validation-03/verification.json'),'scope':'Component checkpoint only; full entry blocked on67 actual main bodies.'})
    print(json.dumps({'fork_commit':fh,'fork':fs,'parent_commit':ph,'parent':ps}))
