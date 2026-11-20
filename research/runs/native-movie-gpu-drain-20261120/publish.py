"""Publish reviewed native GPU completion source/evidence without unrelated work."""
import hashlib,json,pathlib,subprocess
F=pathlib.Path(__file__).resolve().parents[3];P=F.parents[2];R=P/'research/runs/native-movie-ingress-20261119';RUN=pathlib.Path(__file__).resolve().parent
FBASE='118ddb42f72da0b7083e49f6a10b0da0cf8f0f9f';PBASE='15e922c5ba7a19162b22d3fd9b43b78a5ab5001c'
def sha(path):return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()
def git(root,*args,raw=False):
 value=subprocess.check_output(['git',*args],cwd=root);return value if raw else value.decode().strip()
def names(root,*args):return set(git(root,*args,raw=True).decode().strip('\0').split('\0'))-{''}
def need(ok,message):
 if not ok:raise RuntimeError(message)
def validate():
 v=json.loads((RUN/'validation-01/verification.json').read_bytes());need(v['status']=='PASS_NATIVE_MOVIE_GPU_COMPLETION_CPU_AND_WGL_BUILD' and len(v['fixtures'])==1 and len(v['compile_only_targets'])==2,'verified execution scope')
 need(all(sha(path)==digest for path,digest in v['source_after_sha256'].items()),'exact verified current source')
 for stem,key,status in [('NATIVE_MOVIE_GPU_DRAIN_SOURCE_REVIEW_20261120','status','SOURCE_REVIEW_NO_NEW_CONCRETE_DEFECT_AFTER_LISTED_CORRECTIONS'),('NATIVE_MOVIE_GPU_DRAIN_EXECUTION_REVIEW_20261120','status','SAVED_VALIDATION_01_IDENTITIES_AND_EXECUTION_SCOPE_VERIFIED'),('NATIVE_MOVIE_GPU_DRAIN_FIXTURE_CLOSURE_20261120','review_status','PASS_LIMITED_CORRECTION_SOURCE_AND_SAVED_RESULT_REVIEW')]:
  d=json.loads((R/(stem+'.json')).read_bytes());need(d[key]==status,'review status '+stem)
  for name,digest in d.get('source_pins',{}).items():need(sha(P/name)==digest,'reviewed source '+name)
  for row in d.get('inputs',[]):
   path=pathlib.Path(row['path']);path=path if path.is_absolute() else P/path;need(sha(path)==row['sha256'],'review input '+str(path))
 return v
def publish(root,approved,title):
 approved=sorted(approved);need(not names(root,'diff','--cached','--name-only','-z'),'empty initial index')
 subprocess.run(['git','-c','core.autocrlf=false','add','--',*approved],cwd=root,check=True,stdout=subprocess.DEVNULL)
 staged=names(root,'diff','--cached','--name-only','-z');need(staged and staged==(names(root,'diff','HEAD','--name-only','-z') & set(approved)),'exact approved changed staged set')
 subprocess.run(['git','diff','--cached','--check'],cwd=root,check=True)
 for name in staged:need(git(root,'cat-file','blob',':'+name,raw=True)==(root/name).read_bytes(),'exact staged bytes '+name)
 subprocess.run(['git','commit','-m',title],cwd=root,check=True,stdout=subprocess.DEVNULL)
 head=git(root,'rev-parse','HEAD');subprocess.run(['git','push','origin','main'],cwd=root,check=True)
 need(git(root,'ls-remote','--heads','origin','main').split()[0]==head,'remote main identity')
 return head,len(staged)
need(git(F,'branch','--show-current')==git(P,'branch','--show-current')=='main','main branches')
need(git(F,'rev-parse','HEAD')==FBASE and git(P,'rev-parse','HEAD')==PBASE,'expected initial heads')
for root,head in [(F,FBASE),(P,PBASE)]:need(git(root,'ls-remote','--heads','origin','main').split()[0]==head,'current remote base')
preserved={'runtime/src/iop/mcserv.cpp':'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb','runtime/src/iop/msl_native.cpp':'756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23'}
for name,digest in preserved.items():need(sha(F/name)==digest,'preserved dirty IOP')
inventory=P/'research/doc-audit-20261118/inventory.json';inventory_sha='26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6';need(sha(inventory)==inventory_sha,'preserved inventory')
v=validate()
source=['runtime/include/mka/movie_gpu.hpp','runtime/include/mka/movie_gl_device.hpp','runtime/src/native/movie_gl_device.cpp','cmake/native_media.cmake','tests/test_movie_gl_device.cpp','tests/test_native_movie_pipeline.cpp','runtime/include/mka/movie_gpu_completion.hpp','runtime/src/native/movie_gpu_completion.cpp','tests/test_movie_gpu_completion.cpp']
fork_paths={F/name for name in source};fork_paths.update(path for path in RUN.rglob('*') if path.is_file() and path.suffix in {'.py','.md','.json'} and '__pycache__' not in path.parts)
fork_names={path.relative_to(F).as_posix() for path in fork_paths}
need(names(F,'diff','--name-only','-z')-fork_names <= set(preserved),'no unrelated fork edits')
head,fcount=publish(F,fork_names,'Own native GPU completion receipts and host-polled movie retirement')
# Pin published source in parent; no circular source-commit self-identification.
for name in ['PORT_STATUS.md','ROADMAP.md']:
 path=P/name;text=path.read_text();marker='Native movie GPU completion ownership is implemented and validation-01 passes:';need(text.count(marker)==1,'live GPU paragraph')
 text=text.replace(marker,'Published fork `'+head+'`: native movie GPU completion ownership is implemented and validation-01 passes:',1)
 if name=='PORT_STATUS.md':
  marker='| Current native source checkpoint | Published fork ';need(text.count(marker)==1,'source checkpoint owner')
  text=text.replace(marker,'| Current native source checkpoint | Published fork `'+head+'`: actual standalone WGL completion/retirement ownership; CPU fence-port fixture passes (339 stable pins,324 untouched prior), physical callers compiled only; renderer-wide admission/target/boundary draw, sound-context cancellation and phased LOGON/physical output/game acceptance OPEN. Prior published fork ',1)
 path.write_text(text,encoding='utf-8',newline='\n')
result=R/'NATIVE_MOVIE_GPU_DRAIN_RESULTS_20261120.md';result.write_text('Published fork source checkpoint: `'+head+'`. Component scope only; full-game acceptance OPEN.\n\n'+result.read_text(),encoding='utf-8',newline='\n')
receipt=R/'NATIVE_MOVIE_GPU_DRAIN_PUBLICATION_20261120.json';receipt.write_text(json.dumps({'status':'NATIVE_GPU_COMPLETION_COMPONENT_PUBLISHED','fork_commit':head,'verification_sha256':sha(RUN/'validation-01/verification.json'),'executed_cpu_fixtures':1,'compile_only_physical_fixtures':2,'source_pins':339,'unchanged_prior_pins':324,'preserved_dirty_sha256':preserved,'limits':['No physical GPU/game execution or full-game acceptance']},indent=2)+'\n',encoding='utf-8',newline='\n')
reports=['NATIVE_MOVIE_GPU_DRAIN_SOURCE_REVIEW_20261120','NATIVE_MOVIE_GPU_DRAIN_FIXTURE_PEER_20261120','NATIVE_MOVIE_GPU_DRAIN_FIXTURE_CLOSURE_20261120','NATIVE_MOVIE_GPU_DRAIN_EXECUTION_REVIEW_20261120','NATIVE_MOVIE_RENDER_BOUNDARY_SOURCE_20261120','NATIVE_MOVIE_BOUNDARY_ALPHA_SCOPE_20261120','NATIVE_MOVIE_SURROUND_BINDING_SOURCE_20261120','NATIVE_CURRENT_SOUND_RESOURCE_SOURCE_20261120','NATIVE_SOUND_CONTEXT_CANCELLATION_SOURCE_20261120']
parent_paths={P/name for name in ['PORT_STATUS.md','ROADMAP.md','.gitattributes']}
parent_paths.update(R/(stem+suffix) for stem in reports for suffix in ['.md','.json'])
parent_paths.update(R/name for name in ['NATIVE_MOVIE_GPU_DRAIN_PLAN_20261120.md','NATIVE_MOVIE_GPU_DRAIN_RESULTS_20261120.md','NATIVE_MOVIE_GPU_SOURCE_RECONCILIATION_20261120.json','NATIVE_SOUND_CONTEXT_RAW_WORD_CHECK_20261120.json','NATIVE_MOVIE_SURROUND_CALL_WORD_CHECK_20261120.json',receipt.name])
for name in ['native-movie-gpu-drain-before-20261120','native-movie-render-boundary-words-20261120','native-current-sound-resource-words-20261120','native-sound-context-cancellation-words-20261120']:
 parent_paths.update(path for path in (R/name).rglob('*') if path.is_file())
surround=json.loads((R/'NATIVE_MOVIE_SURROUND_BINDING_SOURCE_20261120.json').read_bytes());parent_paths.update(P/row['path'] for row in surround['raw_export_checks'])
parent_names={path.relative_to(P).as_posix() for path in parent_paths}
need(all((P/name).is_file() for name in parent_names),'approved parent files exist')
need(names(P,'diff','--name-only','-z')-parent_names <= {'research/doc-audit-20261118/inventory.json'},'no unrelated parent content edits')
phead,pcount=publish(P,parent_names,'Record native GPU completion checkpoint and precise remaining movie boundaries')
validate()
for name,digest in preserved.items():need(sha(F/name)==digest,'IOP retained after publication')
need(sha(inventory)==inventory_sha,'inventory retained after publication')
print(json.dumps({'status':'PUBLISHED_NATIVE_GPU_COMPLETION_SOURCE_AND_DOCS','fork_commit':head,'parent_commit':phead,'fork_files':fcount,'parent_files':pcount,'cpu_fixtures':1,'physical_compile_only':2}),flush=True)
