"""One-shot scoped publication of verified pending lifecycle and evidence."""
import hashlib,json,pathlib,subprocess
F=pathlib.Path(__file__).resolve().parents[3];P=F.parents[2]
R=P/'research/runs/native-movie-ingress-20261119';RUN=pathlib.Path(__file__).resolve().parent
FBASE='fc2080a67fe9837fcfc47f2157c59421dc59cf85';PBASE='7ea8bb3f80ffc7729de8e9338d856ac06acfd6c1'
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def need(ok,message):
    if not ok:raise RuntimeError(message)
def git(root,*args,raw=False):
    value=subprocess.check_output(['git',*args],cwd=root)
    return value if raw else value.decode().strip()
def names(root,*args):return set(git(root,*args,raw=True).decode().strip('\0').split('\0'))-{''}
def validate():
    v=json.loads((RUN/'validation-03/verification.json').read_bytes())
    need(v['status']=='PASS_NATIVE_LOGON_PENDING_AND_RETAINED_TASK_CLEANUP' and len(v['fixtures'])==5 and len(v['source_after_sha256'])==344,'actual validation scope')
    need(all(sha(n)==h for n,h in v['source_after_sha256'].items()),'344 stable source/tool/asset/evidence pins')
    final=json.loads((R/'NATIVE_LOGON_PENDING_FINAL_SOURCE_20261120.json').read_bytes())
    need(final['status']=='FINAL_TYPED_FAILURE_OWNERSHIP_SOURCE_VERIFIED' and final['input_pins_stable_at_report'],'final source review status')
    need(all(sha(P/n)==h for n,h in final['production_and_test_pins_at_start'].items()),'final reviewed current source')
    review=json.loads((R/'NATIVE_LOGON_PENDING_EXECUTION_REVIEW_20261120.json').read_bytes())
    need(review['review_status']=='PASS_SAVED_NATIVE_LOGON_PENDING_EXECUTION_IDENTITY' and review['reviewer_reran_anything'] is False,'saved execution review scope')
    need(all(sha(P/row['path'])==row['sha256'] for row in review['inputs']),'saved execution review exact input identities')
    mapping=json.loads((R/'NATIVE_SOUND_CONTEXT_IMPLEMENTATION_MAP_20261120.json').read_bytes())
    need(mapping['review_status']=='IMPLEMENTABLE_TYPED_CONTEXT_AND_DEFERRED_JOB_SEAMS_FULL_COMMAND12_BINDING_OPEN','sound source map scope')
    need(all(sha(P/row['path'])==row['sha256'] for row in mapping['inputs']),'sound source map current inputs')
    return v
def publish(root,approved,title):
    approved=set(approved);need(not names(root,'diff','--cached','--name-only','-z'),'empty initial index')
    subprocess.run(['git','-c','core.autocrlf=false','add','--',*sorted(approved)],cwd=root,check=True,stdout=subprocess.DEVNULL)
    staged=names(root,'diff','--cached','--name-only','-z')
    need(staged and staged==(names(root,'diff','HEAD','--name-only','-z')&approved),'exact approved changed staged paths')
    subprocess.run(['git','diff','--cached','--check'],cwd=root,check=True)
    for name in staged:need(git(root,'cat-file','blob',':'+name,raw=True)==(root/name).read_bytes(),'exact staged working bytes '+name)
    subprocess.run(['git','commit','-m',title],cwd=root,check=True,stdout=subprocess.DEVNULL)
    head=git(root,'rev-parse','HEAD');subprocess.run(['git','push','origin','main'],cwd=root,check=True)
    need(git(root,'ls-remote','--heads','origin','main').split()[0]==head,'remote main identity')
    return head,len(staged)
need(git(F,'branch','--show-current')==git(P,'branch','--show-current')=='main','main branches')
need(git(F,'rev-parse','HEAD')==FBASE and git(P,'rev-parse','HEAD')==PBASE,'expected initial heads')
for root,head in [(F,FBASE),(P,PBASE)]:need(git(root,'ls-remote','--heads','origin','main').split()[0]==head,'unchanged remote base')
preserved={'runtime/src/iop/mcserv.cpp':'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',
 'runtime/src/iop/msl_native.cpp':'756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23'}
inventory=P/'research/doc-audit-20261118/inventory.json';inventory_sha='26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6'
for name,h in preserved.items():need(sha(F/name)==h,'preserved dirty IOP')
need(sha(inventory)==inventory_sha,'preserved unrelated inventory')
v=validate()
source=['cmake/native_game.cmake','recomp/native_closure_codegen.cpp','runtime/include/mka/frontend_scheduler.hpp',
 'runtime/src/native/frontend_scheduler.cpp','runtime/include/mka/logon_frontend_task.hpp','runtime/src/native/logon_frontend_task.cpp',
 'tests/test_native_closure_codegen.cpp','tests/test_native_logon_runtime.cpp','tests/test_logon_frontend_task.cpp',
 'runtime/include/mka/logon_surround.hpp','tests/logon_surround_probe.hpp','tests/test_logon_pending.cpp']
fork_paths={F/n for n in source};fork_paths.update(x for x in RUN.rglob('*') if x.is_file() and x.suffix in {'.py','.md','.json'} and '__pycache__' not in x.parts)
fork_names={x.relative_to(F).as_posix() for x in fork_paths}
need(names(F,'diff','--name-only','-z')-fork_names<=set(preserved),'no unrelated fork tracked edits')
head,fcount=publish(F,fork_names,'Retain native movie setup and cleanup across pending task phases')
for name in ['PORT_STATUS.md','ROADMAP.md']:
    path=P/name;text=path.read_text();marker='Typed pending LOGON and retained task cleanup are implemented; validation-03'
    need(text.count(marker)==1,'current pending lifecycle summary')
    text=text.replace(marker,'Published fork `'+head+'`: typed pending LOGON and retained task cleanup are implemented; validation-03',1)
    if name=='PORT_STATUS.md':
        marker='| Current native source checkpoint | Published fork '
        need(text.count(marker)==1,'single current source checkpoint row')
        text=text.replace(marker,'| Current native source checkpoint | Published fork `'+head+'`: actual AOT LOGON typed pending/retained cleanup and scheduler retirement;5 affected fixtures pass (344 stable pins,330 untouched prior;28 caller+21 queued scenarios), source/saved-execution reviews; actual current-sound and whole-renderer/native output/game acceptance OPEN. Prior published fork ',1)
    path.write_text(text,encoding='utf-8',newline='\n')
result=R/'NATIVE_LOGON_PENDING_RESULTS_20261120.md'
result.write_text('Published fork checkpoint: `'+head+'`. Component verification only; native retail acceptance OPEN.\n\n'+result.read_text(),encoding='utf-8',newline='\n')
receipt=R/'NATIVE_LOGON_PENDING_PUBLICATION_20261120.json'
with receipt.open('x',encoding='utf-8',newline='\n') as out:
    json.dump(dict(status='NATIVE_LOGON_PENDING_COMPONENT_PUBLISHED',fork_commit=head,
      verification_sha256=sha(RUN/'validation-03/verification.json'),executed_fixtures=5,source_pins=344,unchanged_prior_pins=330,
      caller_scenarios=28,queued_scenarios=21,fixture_binaries={x['component']:x['executed_target_sha256'] for x in v['fixtures']},
      preserved_dirty_sha256=preserved,limits=['No actual retail surround implementation or physical/full-game playback acceptance']),out,indent=2);out.write('\n')
stems=['NATIVE_LOGON_PENDING_PLAN_20261120','NATIVE_LOGON_PENDING_RESULTS_20261120','NATIVE_LOGON_PENDING_SOURCE_REVIEW_20261120',
 'NATIVE_LOGON_PENDING_IMPLEMENTATION_REVIEW_20261120','NATIVE_LOGON_PENDING_CLOSURE_20261120',
 'NATIVE_LOGON_PENDING_FINAL_SOURCE_20261120','NATIVE_LOGON_PENDING_EXECUTION_REVIEW_20261120','NATIVE_SOUND_CONTEXT_IMPLEMENTATION_MAP_20261120']
parent_paths={P/n for n in ['PORT_STATUS.md','ROADMAP.md','.gitattributes']}
parent_paths.update(R/(s+ext) for s in stems for ext in ['.md','.json'] if (R/(s+ext)).is_file());parent_paths.add(receipt)
for folder in ['native-logon-pending-before-20261120','native-logon-pending-before-20261120-01']:
    parent_paths.update(x for x in (R/folder).rglob('*') if x.is_file())
parent_names={x.relative_to(P).as_posix() for x in parent_paths}
need(names(P,'diff','--name-only','-z')-parent_names<={'research/doc-audit-20261118/inventory.json'},'no unrelated parent edits')
phead,pcount=publish(P,parent_names,'Record pending movie lifecycle evidence and remaining native sound binding')
validate()
for name,h in preserved.items():need(sha(F/name)==h,'IOP preserved after publication')
need(sha(inventory)==inventory_sha,'inventory preserved after publication')
print(json.dumps(dict(status='PUBLISHED_NATIVE_LOGON_PENDING_COMPONENT_AND_DOCS',fork_commit=head,parent_commit=phead,
 fork_files=fcount,parent_files=pcount,fixtures=5,source_pins=344)),flush=True)
