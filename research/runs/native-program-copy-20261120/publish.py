"""Exact-path publication of verified native copies; no component rerun."""
import hashlib,json,pathlib,subprocess
F=pathlib.Path(__file__).resolve().parents[3];P=F.parents[2];RUN=pathlib.Path(__file__).resolve().parent;R=P/'research/runs/native-movie-ingress-20261119'
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()

def need(ok,message):
    if not ok:raise RuntimeError(message)

def git(root,*args,raw=False):
    value=subprocess.check_output(['git',*args],cwd=root)
    return value if raw else value.decode().strip()

def names(root,*args):return set(git(root,*args,raw=True).decode().strip('\0').split('\0'))-{''}

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
def validate():
    v=json.loads((RUN/'validation-01/verification.json').read_bytes())
    need(v['status']=='PASS_NATIVE_PROGRAM_COPY_AND_ORDINARY_SCHEDULE_PRIMITIVES' and len(v['fixtures'])==17 and len(v['source_after_sha256'])==445,'actual component schema')
    for n,h in v['source_after_sha256'].items():need(sha(n)==h,'current certificate pin '+n)
    for stem,status in [('NATIVE_PROGRAM_COPY_FINAL_SOURCE_REVIEW_20261120','SOURCE_REVIEW_NATIVE_PROGRAM_COPY_NO_KNOWN_BLOCKER'),('NATIVE_PROGRAM_COPY_EXECUTION_REVIEW_20261120','SAVED_NATIVE_PROGRAM_COPY_EXECUTION_VERIFIED'),('NATIVE_PROGRAM_COPY_CLOSURE_20261120','NATIVE_PROGRAM_COPY_SOURCE_AND_SAVED_RESULTS_VERIFIED')]:
        d=json.loads((R/(stem+'.json')).read_bytes());need(d['status']==status and isinstance(d['input_sha256'],dict),'review schema '+stem)
        for n,h in d['input_sha256'].items():need(sha(n)==h,'current review pin '+n)
    return v
for root,base in [(F,'2df46c0f7179199a6c4e4eab721575bbfe44fe15'),(P,'359d3449f4c3b5f826b747e4a1bd319b62b52e4f')]:
    need(git(root,'branch','--show-current')=='main' and git(root,'rev-parse','HEAD')==base,'expected local base')
    need(git(root,'ls-remote','--heads','origin','main').split()[0]==base,'expected remote base')
preserved={'runtime/src/iop/mcserv.cpp':'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb','runtime/src/iop/msl_native.cpp':'756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23'}
inventory=P/'research/doc-audit-20261118/inventory.json';inventory_sha='26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6'
for n,h in preserved.items():need(sha(F/n)==h,'preserved dirty IOP')
need(sha(inventory)==inventory_sha,'preserved inventory')
v=validate()
source=['runtime/include/mka/bank_preparation.hpp','runtime/src/native/bank_preparation.cpp','runtime/include/mka/program_executor.hpp','runtime/src/native/program_executor.cpp','cmake/native_sound_resources.cmake','runtime/include/mka/program_copy.hpp','runtime/src/native/program_copy.cpp','tests/test_program_copy.cpp','tests/test_program_copy_failure.cpp','.gitattributes']
files={F/n for n in source};files.update(q for q in RUN.rglob('*') if q.is_file() and q.suffix in {'.py','.md','.json'} and '__pycache__' not in q.parts and not any(x.endswith('-owned') for x in q.relative_to(F).parts))
policy=RUN/'validation-01/preparation-owned/bank-preparation.tsv';need(policy.is_file(),'measured policy TSV');files.add(policy)
approved={q.relative_to(F).as_posix() for q in files};need(names(F,'diff','--name-only','-z')-approved<=set(preserved),'only authored fork paths')
head,fcount=publish(F,approved,'Own native private program copies and independent normal output state')
prefix='Published fork '+chr(96)+head+chr(96)+': '
for name in ['PORT_STATUS.md','ROADMAP.md']:
    q=P/name;s=q.read_text();marker='Independent native per-play command/normal-instance copies, real native voice ownership/mixing and ordinary reset/deadline plans are implemented'
    need(s.count(marker)==1,'single current copy result');s=s.replace(marker,prefix+marker[0].lower()+marker[1:],1)
    if name=='PORT_STATUS.md':
        marker='| Current native source checkpoint | Published fork ';need(s.count(marker)==1,'single current source row')
        s=s.replace(marker,'| Current native source checkpoint | '+prefix+'actual native copies/voice ownership and ordinary reset/deadline plans;17 affected clients pass (445 stable pins,410 untouched prior;7 allocation faults and actual eager/deferred/344-bank metadata), independent source/results reviews; actual output admission/identity-list/clock-backed activation/source callbacks/command12/renderer/full-game acceptance OPEN. Prior published fork ',1)
        rows=s.splitlines()
        for i,row in enumerate(rows):
            if row.startswith('| Native sound components |'):rows[i]='| Native sound components | Actual native per-play copies/voice owners, bank runtime cache, Service tickets and contexts;17 affected clients pass | Component evidence only; output admission/source activation/callbacks/command12, physical/heard/full-game acceptance OPEN |'
        s='\n'.join(rows)+'\n'
    q.write_text(s,encoding='utf-8',newline='\n')
receipt=R/'NATIVE_PROGRAM_COPY_PUBLICATION_20261120.json'
with receipt.open('x',encoding='utf-8',newline='\n') as out:
    json.dump(dict(status='NATIVE_PROGRAM_COPY_COMPONENT_PUBLISHED',fork_commit=head,verification_sha256=sha(RUN/'validation-01/verification.json'),executed_fixtures=17,source_pins=445,unchanged_prior_pins=410,allocation_failures_observed=7,fixture_binaries={x['component']:x['executed_target_sha256'] for x in v['fixtures']},raw_policy_sha256=sha(policy),preserved_dirty_sha256=preserved,limits=['Actual private normal copies/outputs and plan primitives, not source output admission/identity/activation/callback/full game']),out,indent=2);out.write('\n')
files={P/n for n in ['PORT_STATUS.md','ROADMAP.md','.gitattributes']}
for stem in ['NATIVE_NORMAL_PLAY_INSTANCE_SOURCE_20261120','NATIVE_NORMAL_PROGRAM_ACTIVATION_SOURCE_20261120','NATIVE_PROGRAM_COPY_SOURCE_REVIEW_20261120','NATIVE_PROGRAM_COPY_FINAL_SOURCE_REVIEW_20261120','NATIVE_PROGRAM_COPY_EXECUTION_REVIEW_20261120','NATIVE_PROGRAM_COPY_CLOSURE_20261120','NATIVE_PROGRAM_COPY_PLAN_20261120','NATIVE_PROGRAM_COPY_RESULTS_20261120']:
    files.update(R/(stem+ext) for ext in ['.md','.json'] if (R/(stem+ext)).is_file())
files.update([receipt,R/'NATIVE_PROGRAM_COPY_ROOT_WORDS_20261120.json',R/'NATIVE_PROGRAM_JOB_CURRENT_BINDING_20261120.md'])
for folder in ['native-program-copy-before-20261120','native-normal-play-instance-words-20261120','native-normal-program-activation-words-20261120']:files.update(q for q in (R/folder).rglob('*') if q.is_file())
approved={q.relative_to(P).as_posix() for q in files};need(names(P,'diff','--name-only','-z')-approved<={'research/doc-audit-20261118/inventory.json'},'only authored parent paths')
phead,pcount=publish(P,approved,'Record native program copy outputs and source activation requirements')
validate()
for n,h in preserved.items():need(sha(F/n)==h,'IOP preserved after publication')
need(sha(inventory)==inventory_sha,'inventory preserved after publication')
print(json.dumps(dict(status='PUBLISHED_NATIVE_PROGRAM_COPY_AND_DOCS',fork_commit=head,parent_commit=phead,fork_files=fcount,parent_files=pcount,fixtures=17,source_pins=445)),flush=True)
