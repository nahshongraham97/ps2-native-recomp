"""Scoped publication of corrected native cache and fresh program work."""
import hashlib,json,pathlib,subprocess
F=pathlib.Path(__file__).resolve().parents[3];P=F.parents[2]
R=P/'research/runs/native-movie-ingress-20261119';RUN=pathlib.Path(__file__).resolve().parent
FBASE='3c09588d3777d465285d3e78dd118206d4515660';PBASE='d6343fac4142e52b45c63eb1eff9a7750c77bf0b'
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

def pin_rows(values):
    for n,h in values.items():
        q=pathlib.Path(n);q=q if q.is_absolute() else P/q;need(sha(q)==h,'review pin '+str(q))
def validate():
    v=json.loads((RUN/'validation-01/verification.json').read_bytes())
    need(v['status']=='PASS_NATIVE_DEFERRED_PROGRAM_WORK_AND_CORRECTED_SAMPLE_CACHE' and len(v['fixtures'])==12 and len(v['source_after_sha256'])==361,'actual scoped validation')
    pin_rows(v['source_after_sha256'])
    source=json.loads((R/'NATIVE_DEFERRED_PROGRAM_FINAL_SOURCE_20261120.json').read_bytes())
    need(source['status']=='FINAL_DEFERRED_PROGRAM_SOURCE_CORRECTIONS_VERIFIED' and source['current_inputs_stable_at_report'] is True,'source closure')
    pin_rows(source['current_inputs_at_review_start']);pin_rows(source['first_review_preserved_before'])
    for stem,status in [('NATIVE_DEFERRED_PROGRAM_EXECUTION_REVIEW_20261120','SAVED_EXECUTION_AND_CURRENT_PINS_VERIFIED'),('NATIVE_DEFERRED_PROGRAM_CORRECTION_CLOSURE_20261120','DEFERRED_PROGRAM_CORRECTED_SOURCE_AND_SAVED_VALIDATION_JOIN_VERIFIED')]:
        d=json.loads((R/(stem+'.json')).read_bytes());need(d['status']==status,'saved review scope');pin_rows(d['input_sha256'])
    current=json.loads((R/'NATIVE_DEFERRED_PROGRAM_CURRENT_POLICY_20261120.json').read_bytes())
    need(current['status']=='CURRENT_NATIVE_CACHE_AND_PRIVATE_PROGRAM_POLICY_WITH_DISPROVED_HISTORY_INDEX','authoritative policy index')
    need(all(sha(P/x['path'])==x['sha256'] for x in current['records']),'preserved historical report bytes')
    return v
need(git(F,'branch','--show-current')==git(P,'branch','--show-current')=='main','main branches')
need(git(F,'rev-parse','HEAD')==FBASE and git(P,'rev-parse','HEAD')==PBASE,'expected initial heads')
for root,head in [(F,FBASE),(P,PBASE)]:need(git(root,'ls-remote','--heads','origin','main').split()[0]==head,'unchanged remote base')
preserved={'runtime/src/iop/mcserv.cpp':'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',
 'runtime/src/iop/msl_native.cpp':'756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23'}
inventory=P/'research/doc-audit-20261118/inventory.json';inventory_sha='26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6'
for n,h in preserved.items():need(sha(F/n)==h,'preserved IOP')
need(sha(inventory)==inventory_sha,'preserved inventory')
v=validate()
source=['.gitattributes','runtime/include/mka/bank_preparation.hpp','runtime/src/native/bank_preparation.cpp','tests/test_bank_preparation.cpp',
 'cmake/native_sound_resources.cmake','tests/test_deferred_program_work.cpp','tests/test_deferred_cache_policy.cpp']
fork_paths={F/n for n in source};fork_paths.update(x for x in RUN.rglob('*') if x.is_file() and x.suffix in {'.py','.md','.json','.tsv'} and '__pycache__' not in x.parts)
# Synthetic files made by fixtures are not source deliverables; only structured evidence is published.
fork_paths={x for x in fork_paths if not any(part.endswith('-owned') for part in x.relative_to(F).parts)}
# Full bank-policy TSV is an explicit measurement, not a synthetic fixture file.
policy=RUN/'validation-01/preparation-owned/bank-preparation.tsv';need(policy.is_file(),'bank policy measurement');fork_paths.add(policy)
fork_names={x.relative_to(F).as_posix() for x in fork_paths}
need(names(F,'diff','--name-only','-z')-fork_names<=set(preserved),'no unrelated fork tracked edits')
head,fcount=publish(F,fork_names,'Own cancellable native fresh program work and correct sample cache lifetime')
for name in ['PORT_STATUS.md','ROADMAP.md']:
    q=P/name;s=q.read_text();marker='Native fresh program work and corrected cache ownership are implemented;'
    need(s.count(marker)==1,'current deferred summary')
    s=s.replace(marker,'Published fork `'+head+'`: native fresh program work and corrected cache ownership are implemented;',1)
    if name=='PORT_STATUS.md':
        marker='| Current native source checkpoint | Published fork ';need(s.count(marker)==1,'single current source row')
        s=s.replace(marker,'| Current native source checkpoint | Published fork `'+head+'`: owned cancellable fresh-program work and corrected independent cache/PCM/occurrence ownership;12 affected fixtures pass (361 stable pins,346 untouched prior;344-bank policy), original flag40 polarity and superseded claims explicitly reconciled; full source Service job/callback/command12/renderer/game acceptance OPEN. Prior published fork ',1)
        rows=s.splitlines()
        for i,row in enumerate(rows):
            if row.startswith('| Native sound components |'):rows[i]='| Native sound components | Correct sample-cache polarity, owned cancellable fresh work, explicit native contexts and preparation reservations; latest12 affected clients pass | Native component evidence only; Service queue/callback/source runtime descriptor, command12, heard identity/physical output/full-game acceptance OPEN |'
        s='\n'.join(rows)+'\n'
    q.write_text(s,encoding='utf-8',newline='\n')
result=R/'NATIVE_DEFERRED_PROGRAM_RESULTS_20261120.md';result.write_text('Published fork checkpoint: `'+head+'`. Component verification only; native retail acceptance OPEN.\n\n'+result.read_text(),encoding='utf-8',newline='\n')
receipt=R/'NATIVE_DEFERRED_PROGRAM_PUBLICATION_20261120.json'
with receipt.open('x',encoding='utf-8',newline='\n') as out:
    json.dump(dict(status='NATIVE_DEFERRED_PROGRAM_COMPONENT_PUBLISHED',fork_commit=head,verification_sha256=sha(RUN/'validation-01/verification.json'),
     executed_fixtures=12,source_pins=361,unchanged_prior_pins=346,preparation_counts=dict(banks=344,eager=5500,deferred=4156,skipped_precreation=179),
     fixture_binaries={x['component']:x['executed_target_sha256'] for x in v['fixtures']},preserved_dirty_sha256=preserved,
     limits=['Fresh native data work, not full source async Service queue/callback/current-context command12 or physical/full-game acceptance']),out,indent=2);out.write('\n')
stems=['NATIVE_DEFERRED_CANDIDATE_OWNERSHIP_20261120','NATIVE_DEFERRED_CANDIDATE_POLICY_20261120','NATIVE_DEFERRED_CANDIDATE_CACHE_CORRECTION_20261120',
 'NATIVE_DEFERRED_CANDIDATE_CACHE_BRANCH_CORRECTION_20261120','NATIVE_DEFERRED_CANDIDATE_POLICY_CORRECTION_20261120','NATIVE_DEFERRED_CACHE_NATIVE_MODEL_REVIEW_20261120',
 'NATIVE_DEFERRED_PROGRAM_IMPLEMENTATION_REVIEW_20261120','NATIVE_DEFERRED_PROGRAM_FINAL_SOURCE_20261120','NATIVE_DEFERRED_PROGRAM_FIXTURE_REVIEW_20261120',
 'NATIVE_DEFERRED_PROGRAM_EXECUTION_REVIEW_20261120','NATIVE_DEFERRED_PROGRAM_CORRECTION_CLOSURE_20261120','NATIVE_DEFERRED_PROGRAM_PLAN_20261120',
 'NATIVE_DEFERRED_PROGRAM_RESULTS_20261120','NATIVE_DEFERRED_PROGRAM_CURRENT_POLICY_20261120']
parent_paths={P/n for n in ['PORT_STATUS.md','ROADMAP.md','.gitattributes']}
parent_paths.update(R/(stem+ext) for stem in stems for ext in ['.md','.json'] if (R/(stem+ext)).is_file())
parent_paths.update([receipt,R/'NATIVE_SAMPLE_CACHE_BRANCH_ROOT_CHECK_20261120.json'])
parent_paths.update(x for x in (R/'native-deferred-program-before-20261120').rglob('*') if x.is_file())
parent_names={x.relative_to(P).as_posix() for x in parent_paths}
need(names(P,'diff','--name-only','-z')-parent_names<={'research/doc-audit-20261118/inventory.json'},'no unrelated parent edits')
phead,pcount=publish(P,parent_names,'Record corrected cache branch policy and native fresh work verification')
validate()
for n,h in preserved.items():need(sha(F/n)==h,'IOP preserved after publication')
need(sha(inventory)==inventory_sha,'inventory preserved after publication')
print(json.dumps(dict(status='PUBLISHED_NATIVE_DEFERRED_PROGRAM_COMPONENT_AND_DOCS',fork_commit=head,parent_commit=phead,
 fork_files=fcount,parent_files=pcount,fixtures=12,source_pins=361)),flush=True)
