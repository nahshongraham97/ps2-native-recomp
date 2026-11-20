"""Publish the verified native bank runtime constructor/cache ownership."""
import hashlib,json,pathlib,subprocess
F=pathlib.Path(__file__).resolve().parents[3];P=F.parents[2];RUN=pathlib.Path(__file__).resolve().parent;R=P/'research/runs/native-movie-ingress-20261119'
FBASE='5fa0750944e6b5c68fba3c474dfc699b8df340d3';PBASE='fa7098703ac06002d05ed39e5ac0111061ad9abd'
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
    need(v['status']=='PASS_NATIVE_BANK_RUNTIME_PREPARATION_OWNERSHIP' and len(v['fixtures'])==15 and len(v['source_after_sha256'])==415,'actual final component certificate')
    for n,h in v['source_after_sha256'].items():need(sha(n)==h,'current certificate pin '+n)
    for stem,status in [('NATIVE_RUNTIME_PREPARATION_SOURCE_REVIEW_20261120','SOURCE_REVIEW_NATIVE_RUNTIME_CACHE_OWNER_VERIFIED'),('NATIVE_RUNTIME_PREPARATION_EXECUTION_REVIEW_20261120','SAVED_NATIVE_RUNTIME_PREPARATION_EXECUTION_VERIFIED'),('NATIVE_RUNTIME_PREPARATION_CLOSURE_20261120','NATIVE_RUNTIME_CACHE_SOURCE_AND_SAVED_RESULTS_VERIFIED')]:
        d=json.loads((R/(stem+'.json')).read_bytes());need(d['status']==status and isinstance(d['input_sha256'],dict),'exact source/result review schema '+stem)
        for n,h in d['input_sha256'].items():need(sha(n)==h,'current review pin '+n)
    return v
for root,base in [(F,FBASE),(P,PBASE)]:
    need(git(root,'branch','--show-current')=='main' and git(root,'rev-parse','HEAD')==base,'expected local base')
    need(git(root,'ls-remote','--heads','origin','main').split()[0]==base,'expected remote base')
preserved={'runtime/src/iop/mcserv.cpp':'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb','runtime/src/iop/msl_native.cpp':'756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23'}
inventory=P/'research/doc-audit-20261118/inventory.json';inventory_sha='26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6'
for n,h in preserved.items():need(sha(F/n)==h,'preserved dirty source')
need(sha(inventory)==inventory_sha,'preserved inventory')
v=validate()
source=['runtime/include/mka/bank_preparation.hpp','runtime/src/native/bank_preparation.cpp','cmake/native_sound_resources.cmake','runtime/include/mka/runtime_preparation.hpp','tests/test_runtime_preparation.cpp','.gitattributes']
fork_paths={F/n for n in source}
fork_paths.update(x for x in RUN.rglob('*') if x.is_file() and x.suffix in {'.py','.md','.json'} and '__pycache__' not in x.parts and not any(part.endswith('-owned') for part in x.relative_to(F).parts))
policy=RUN/'validation-01/preparation-owned/bank-preparation.tsv';need(policy.is_file(),'actual bank policy measurement');fork_paths.add(policy)
fork_names={x.relative_to(F).as_posix() for x in fork_paths};need(names(F,'diff','--name-only','-z')-fork_names<=set(preserved),'no unrelated fork edits')
head,fcount=publish(F,fork_names,'Own native bank runtime preparations and semantic cache references')
for name in ['PORT_STATUS.md','ROADMAP.md']:
    q=P/name;s=q.read_text();marker='Actual bank-owned runtime preparation/cache/semantic-reference ownership is implemented and all15 affected clients pass'
    need(s.count(marker)==1,'single actual runtime result');s=s.replace(marker,'Published fork `'+head+'`: actual bank-owned runtime preparation/cache/semantic-reference ownership is implemented and all15 affected clients pass',1)
    if name=='PORT_STATUS.md':
        marker='| Current native source checkpoint | Published fork ';need(s.count(marker)==1,'single current checkpoint')
        s=s.replace(marker,'| Current native source checkpoint | Published fork `'+head+'`: actual bank-owned runtime constructor/cache/semantic references, eager0/deferred1 and last-release policy;15 affected clients pass (415 stable pins,376 untouched prior;344-bank policy), source/execution reviews; clone/output activation, source Service/worker callback/command12/renderer/full-game acceptance OPEN. Prior published fork ',1)
        rows=s.splitlines()
        for i,row in enumerate(rows):
            if row.startswith('| Native sound components |'):rows[i]='| Native sound components | Bank-owned native runtime cache/references, retained Service tickets, corrected sample ownership and explicit play contexts;15 affected clients pass | Native component evidence only; per-play clone/output activation/source callbacks/command12, heard identity/physical output/full-game acceptance OPEN |'
        s='\n'.join(rows)+'\n'
    q.write_text(s,encoding='utf-8',newline='\n')
receipt=R/'NATIVE_RUNTIME_PREPARATION_PUBLICATION_20261120.json'
with receipt.open('x',encoding='utf-8',newline='\n') as out:
    json.dump(dict(status='NATIVE_BANK_RUNTIME_PREPARATION_COMPONENT_PUBLISHED',fork_commit=head,verification_sha256=sha(RUN/'validation-01/verification.json'),executed_fixtures=15,source_pins=415,unchanged_prior_pins=376,fixture_binaries={x['component']:x['executed_target_sha256'] for x in v['fixtures']},raw_policy_sha256=sha(policy),raw_policy_exact_bytes_preserved=True,preserved_dirty_sha256=preserved,limits=['Actual native constructor/cache/semantic references only; clone/activation/source worker callback/command12/full-game acceptance OPEN']),out,indent=2);out.write('\n')
parent_paths={P/n for n in ['PORT_STATUS.md','ROADMAP.md','.gitattributes']}
for stem in ['NATIVE_RUNTIME_PREPARATION_OWNER_SOURCE_20261120','NATIVE_RUNTIME_ACTIVATION_SOURCE_20261120','NATIVE_RUNTIME_PREPARATION_SOURCE_REVIEW_20261120','NATIVE_RUNTIME_PREPARATION_EXECUTION_REVIEW_20261120','NATIVE_RUNTIME_PREPARATION_CLOSURE_20261120','NATIVE_RUNTIME_PREPARATION_PLAN_20261120','NATIVE_RUNTIME_PREPARATION_RESULTS_20261120']:
    parent_paths.update(R/(stem+ext) for ext in ['.md','.json'] if (R/(stem+ext)).is_file())
parent_paths.update([receipt,R/'NATIVE_RUNTIME_PREPARATION_ROOT_WORDS_20261120.json',R/'NATIVE_PROGRAM_JOB_CURRENT_BINDING_20261120.md'])
for folder in ['native-runtime-preparation-before-20261120','native-runtime-activation-words-20261120','native-runtime-preparation-owner-words-20261120']:
    parent_paths.update(x for x in (R/folder).rglob('*') if x.is_file())
parent_names={x.relative_to(P).as_posix() for x in parent_paths};need(names(P,'diff','--name-only','-z')-parent_names<={'research/doc-audit-20261118/inventory.json'},'no unrelated parent edits')
phead,pcount=publish(P,parent_names,'Record native runtime cache ownership and actual activation boundaries')
validate()
for n,h in preserved.items():need(sha(F/n)==h,'preserved source after publication')
need(sha(inventory)==inventory_sha,'preserved inventory after publication')
print(json.dumps(dict(status='PUBLISHED_NATIVE_RUNTIME_PREPARATION_AND_DOCS',fork_commit=head,parent_commit=phead,fork_files=fcount,parent_files=pcount,fixtures=15,source_pins=415)),flush=True)
