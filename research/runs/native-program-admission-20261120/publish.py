"""Publish exactly the verified native Service admission component and evidence."""
import hashlib,json,pathlib,subprocess
F=pathlib.Path(__file__).resolve().parents[3];P=F.parents[2];RUN=pathlib.Path(__file__).resolve().parent
R=P/'research/runs/native-movie-ingress-20261119'
FBASE='9415cb997e82a4731041a22cdbf82928df01df39';PBASE='77e1e16c2f25ce88dc3ac9e231cf287709477782'
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
    v=json.loads((RUN/'validation-02/verification.json').read_bytes())
    need(v['status']=='PASS_NATIVE_SERVICE_OWNED_PROGRAM_ADMISSION' and len(v['fixtures'])==10 and len(v['source_after_sha256'])==379,'actual ten-client verification')
    need(v['saved_executions_reused']==3 and v['new_executions']==7 and v['new_builds']==0,'saved continuation schema')
    for n,h in v['source_after_sha256'].items():need(sha(n)==h,'current pin '+n)
    for stem,status in [('NATIVE_PROGRAM_ADMISSION_SOURCE_REVIEW_20261120','SOURCE_REVIEW_OWNED_PROGRAM_ADMISSION_NO_KNOWN_BLOCKER'),('NATIVE_PROGRAM_ADMISSION_EXECUTION_REVIEW_20261120','SAVED_EXECUTION_OWNED_PROGRAM_ADMISSION_VERIFIED'),('NATIVE_PROGRAM_ADMISSION_CLOSURE_20261120','SAVED_ADMISSION_CORRECTION_AND_SCOPE_VERIFIED')]:
        d=json.loads((R/(stem+'.json')).read_bytes());need(d['status']==status and isinstance(d['input_sha256'],dict),'exact review schema '+stem)
        for n,h in d['input_sha256'].items():need(sha(n)==h,'current review pin '+n)
    return v
need(git(F,'branch','--show-current')==git(P,'branch','--show-current')=='main','main branch')
for root,base in [(F,FBASE),(P,PBASE)]:
    need(git(root,'rev-parse','HEAD')==base,'expected initial local base')
    need(git(root,'ls-remote','--heads','origin','main').split()[0]==base,'expected remote base')
preserved={'runtime/src/iop/mcserv.cpp':'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb','runtime/src/iop/msl_native.cpp':'756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23'}
inventory=P/'research/doc-audit-20261118/inventory.json';inventory_sha='26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6'
for n,h in preserved.items():need(sha(F/n)==h,'preserved dirty source')
need(sha(inventory)==inventory_sha,'preserved broad inventory')
v=validate()
source=['runtime/include/mka/audio_banks.hpp','runtime/src/native/audio_banks.cpp','runtime/include/mka/audio_bank_protocol.hpp','runtime/src/native/audio_bank_protocol.cpp','runtime/include/mka/audio_bank_service.hpp','runtime/src/native/audio_bank_service.cpp','cmake/native_sound_resources.cmake','tests/test_program_admission.cpp','tests/test_program_admission_gates.cpp']
fork_paths={F/n for n in source}
fork_paths.update(x for x in RUN.rglob('*') if x.is_file() and x.suffix in {'.py','.md','.json'} and '__pycache__' not in x.parts and not any(part.endswith('-owned') for part in x.relative_to(F).parts))
fork_names={x.relative_to(F).as_posix() for x in fork_paths}
need(names(F,'diff','--name-only','-z')-fork_names<=set(preserved),'no unrelated fork edits')
head,fcount=publish(F,fork_names,'Retain native Service program admission through worker retirement')
for name in ['PORT_STATUS.md','ROADMAP.md']:
    q=P/name;s=q.read_text();marker='Owned Service/Protocol/Session deferred-program admission is implemented and all10 affected clients pass'
    need(s.count(marker)==1,'single current implementation statement')
    s=s.replace(marker,'Published fork `'+head+'`: owned Service/Protocol/Session deferred-program admission is implemented and all10 affected clients pass',1)
    if name=='PORT_STATUS.md':
        marker='| Current native source checkpoint | Published fork ';need(s.count(marker)==1,'single current checkpoint row')
        s=s.replace(marker,'| Current native source checkpoint | Published fork `'+head+'`: actual Service-owned deferred-program admission, exact immutable requests and retained bank/operation ownership through worker retirement;10 clients pass (379 stable pins,354 untouched prior;7 lifecycle+5 mutation cases); source job/callback/command12/renderer/game acceptance OPEN. Prior published fork ',1)
        rows=s.splitlines()
        for i,row in enumerate(rows):
            if row.startswith('| Native sound components |'):rows[i]='| Native sound components | Actual retained Service program tickets, corrected cache ownership, explicit native contexts and play reservations;10 affected clients pass | Native component evidence only; source job/callback/runtime descriptor/command12, heard identity/physical output/full-game acceptance OPEN |'
        s='\n'.join(rows)+'\n'
    q.write_text(s,encoding='utf-8',newline='\n')
receipt=R/'NATIVE_PROGRAM_ADMISSION_PUBLICATION_20261120.json'
with receipt.open('x',encoding='utf-8',newline='\n') as out:
    json.dump(dict(status='NATIVE_SERVICE_PROGRAM_ADMISSION_COMPONENT_PUBLISHED',fork_commit=head,verification_sha256=sha(RUN/'validation-02/verification.json'),executed_fixtures=10,source_pins=379,unchanged_prior_pins=354,saved_executions=3,new_executions=7,fixture_binaries={x['component']:x['executed_target_sha256'] for x in v['fixtures']},preserved_dirty_sha256=preserved,limits=['Owned actual Service admission only; source queue/callback/descriptor/command12 and full-game acceptance OPEN']),out,indent=2);out.write('\n')
parent_paths={P/n for n in ['PORT_STATUS.md','ROADMAP.md','.gitattributes']}
parent_paths.update(R/(stem+ext) for stem in ['NATIVE_PROGRAM_ADMISSION_SOURCE_REVIEW_20261120','NATIVE_PROGRAM_ADMISSION_EXECUTION_REVIEW_20261120','NATIVE_PROGRAM_ADMISSION_CLOSURE_20261120','NATIVE_PROGRAM_ADMISSION_PLAN_20261120','NATIVE_PROGRAM_ADMISSION_RESULTS_20261120','NATIVE_DEFERRED_PROGRAM_CURRENT_POLICY_20261120'] for ext in ['.md','.json'] if (R/(stem+ext)).is_file())
parent_paths.add(receipt);parent_paths.update(x for x in (R/'native-program-admission-before-20261120').rglob('*') if x.is_file())
parent_names={x.relative_to(P).as_posix() for x in parent_paths}
need(names(P,'diff','--name-only','-z')-parent_names<={'research/doc-audit-20261118/inventory.json'},'no unrelated parent edits')
phead,pcount=publish(P,parent_names,'Record Service admission ownership and checked cancellation lifetime')
validate()
for n,h in preserved.items():need(sha(F/n)==h,'dirty IOP preserved after publication')
need(sha(inventory)==inventory_sha,'inventory preserved after publication')
print(json.dumps(dict(status='PUBLISHED_NATIVE_PROGRAM_ADMISSION_AND_DOCS',fork_commit=head,parent_commit=phead,fork_files=fcount,parent_files=pcount,fixtures=10,source_pins=379)),flush=True)
