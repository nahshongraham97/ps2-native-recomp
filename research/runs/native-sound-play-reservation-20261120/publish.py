"""One-shot publication of reviewed native sound ownership and evidence."""
import hashlib,json,pathlib,subprocess
F=pathlib.Path(__file__).resolve().parents[3];P=F.parents[2]
R=P/'research/runs/native-movie-ingress-20261119';RUN=pathlib.Path(__file__).resolve().parent
FBASE='a562eca57577faa2479d1ff7213310c47dae1dad';PBASE='85fe74b7b8268373b590ebc265ec6b5993b36a4a'
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
    need(v['status']=='PASS_NATIVE_SOUND_PLAY_CONTEXT_RESERVATION_AND_SERVICE_GAPS' and len(v['fixtures'])==3 and len(v['source_after_sha256'])==350,'actual scoped validation')
    need(all(sha(n)==h for n,h in v['source_after_sha256'].items()),'350 exact current pins')
    final=json.loads((R/'NATIVE_SOUND_PLAY_RESERVATION_FINAL_SOURCE_20261120.json').read_bytes())
    need(final['review_status']=='PASS_BOUNDED_RESERVATION_CONTEXT_LIFETIME_SOURCE_REVIEW' and final['source_only'] is True,'final source scope')
    need(all(sha(P/row['path'])==row['sha256'] for row in final['inputs']),'final exact source inputs')
    execution=json.loads((R/'NATIVE_SOUND_PLAY_RESERVATION_EXECUTION_REVIEW_20261120.json').read_bytes())
    need(execution['review_status']=='PASS_SAVED_SOUND_PLAY_RESERVATION_EXECUTION_AND_GAP_FIXTURE_REVIEW' and execution['reviewer_reran_anything'] is False,'saved execution review')
    need(all(sha(P/row['path'])==row['sha256'] for row in execution['inputs']),'exact saved execution inputs')
    closure=json.loads((R/'NATIVE_SOUND_PLAY_RESERVATION_CORRECTION_CLOSURE_20261120.json').read_bytes())
    need(closure['status']=='PLAY_RESERVATION_CORRECTIONS_SOURCE_AND_SAVED_FIXTURE_IDENTITIES_VERIFIED','correction source closure')
    need(all(sha(P/n)==h for n,h in closure['inputs'].items()),'correction current and preserved input pins')
    return v
need(git(F,'branch','--show-current')==git(P,'branch','--show-current')=='main','main branches')
need(git(F,'rev-parse','HEAD')==FBASE and git(P,'rev-parse','HEAD')==PBASE,'expected initial heads')
for root,head in [(F,FBASE),(P,PBASE)]:need(git(root,'ls-remote','--heads','origin','main').split()[0]==head,'unchanged remote base')
preserved={'runtime/src/iop/mcserv.cpp':'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',
 'runtime/src/iop/msl_native.cpp':'756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23'}
inventory=P/'research/doc-audit-20261118/inventory.json';inventory_sha='26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6'
for n,h in preserved.items():need(sha(F/n)==h,'preserved dirty IOP')
need(sha(inventory)==inventory_sha,'preserved unrelated inventory')
v=validate()
source=['runtime/include/mka/playback_owner.hpp','runtime/src/native/playback_owner.cpp','tests/test_playback_owner.cpp',
 'cmake/native_sound_resources.cmake','tests/test_playback_reservation.cpp','tests/test_playback_reservation_gaps.cpp']
fork_paths={F/n for n in source};fork_paths.update(x for x in RUN.rglob('*') if x.is_file() and x.suffix in {'.py','.md','.json'} and '__pycache__' not in x.parts)
fork_names={x.relative_to(F).as_posix() for x in fork_paths}
need(names(F,'diff','--name-only','-z')-fork_names<=set(preserved),'no unrelated fork changes')
head,fcount=publish(F,fork_names,'Own native sound contexts and play reservations outside resource work locks')
for name in ['PORT_STATUS.md','ROADMAP.md']:
    q=P/name;s=q.read_text();marker='Native sound contexts and actual play reservations are implemented;'
    need(s.count(marker)==1,'current sound reservation summary')
    s=s.replace(marker,'Published fork `'+head+'`: native sound contexts and actual play reservations are implemented;',1)
    if name=='PORT_STATUS.md':
        marker='| Current native source checkpoint | Published fork '
        need(s.count(marker)==1,'single current checkpoint')
        s=s.replace(marker,'| Current native source checkpoint | Published fork `'+head+'`: actual native sound contexts/owned play reservations/short registry admission and mutation gates;3 affected fixtures pass (350 stable pins,340 untouched prior), actual nonzero existing voice continues during publication exclusion and Service gaps/64 pending reservations pass; current-context bootstrap/command12/deferred cancellation/renderer/game acceptance OPEN. Prior published fork ',1)
        rows=s.splitlines()
        for i,row in enumerate(rows):
            if row.startswith('| Native sound components |'):
                rows[i]='| Native sound components | Explicit owned sound contexts and stable preparation reservations; latest3 affected fixtures pass. Shared domain/publication/backing implementation previously passed12 fixtures at118ddb42 | Native component evidence only; source forced cleanup/deferred cancellation, heard identity, physical output and full-game parity remain OPEN |'
        s='\n'.join(rows)+'\n'
    q.write_text(s,encoding='utf-8',newline='\n')
result=R/'NATIVE_SOUND_PLAY_RESERVATION_RESULTS_20261120.md'
result.write_text('Published fork checkpoint: `'+head+'`. Component verification only; full native retail acceptance OPEN.\n\n'+result.read_text(),encoding='utf-8',newline='\n')
receipt=R/'NATIVE_SOUND_PLAY_RESERVATION_PUBLICATION_20261120.json'
with receipt.open('x',encoding='utf-8',newline='\n') as out:
    json.dump(dict(status='NATIVE_SOUND_PLAY_RESERVATION_COMPONENT_PUBLISHED',fork_commit=head,
     verification_sha256=sha(RUN/'validation-01/verification.json'),executed_fixtures=3,source_pins=350,unchanged_prior_pins=340,
     fixture_binaries={x['component']:x['executed_target_sha256'] for x in v['fixtures']},preserved_dirty_sha256=preserved,
     limits=['No retail command12/deferred cancellation/current-context bootstrap/physical audio or full-game acceptance']),out,indent=2);out.write('\n')
stems=['NATIVE_SOUND_PLAY_RESERVATION_PLAN_20261120','NATIVE_SOUND_PLAY_RESERVATION_RESULTS_20261120',
 'NATIVE_SOUND_PLAY_RESERVATION_SOURCE_20261120','NATIVE_SOUND_PLAY_RESERVATION_IMPLEMENTATION_REVIEW_20261120',
 'NATIVE_SOUND_PLAY_RESERVATION_FINAL_SOURCE_20261120','NATIVE_SOUND_PLAY_RESERVATION_CORRECTION_CLOSURE_20261120',
 'NATIVE_SOUND_PLAY_RESERVATION_EXECUTION_REVIEW_20261120','NATIVE_DEFERRED_PROGRAM_COMMIT_SOURCE_20261120']
parent_paths={P/n for n in ['PORT_STATUS.md','ROADMAP.md','.gitattributes']}
parent_paths.update(R/(stem+ext) for stem in stems for ext in ['.md','.json'] if (R/(stem+ext)).is_file());parent_paths.add(receipt)
parent_paths.update(x for x in (R/'native-sound-play-reservation-before-20261120').rglob('*') if x.is_file())
parent_names={x.relative_to(P).as_posix() for x in parent_paths}
need(names(P,'diff','--name-only','-z')-parent_names<={'research/doc-audit-20261118/inventory.json'},'no unrelated parent edits')
phead,pcount=publish(P,parent_names,'Record native sound reservation evidence and deferred program commit frontier')
validate()
for n,h in preserved.items():need(sha(F/n)==h,'IOP preserved after publication')
need(sha(inventory)==inventory_sha,'inventory preserved after publication')
print(json.dumps(dict(status='PUBLISHED_NATIVE_SOUND_PLAY_RESERVATION_COMPONENT_AND_DOCS',fork_commit=head,parent_commit=phead,
 fork_files=fcount,parent_files=pcount,fixtures=3,source_pins=350)),flush=True)
