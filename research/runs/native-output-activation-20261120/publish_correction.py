"""Publish native logical output/shared clock binding and scoped evidence, fail closed."""
import ast,hashlib,json,pathlib,subprocess
F=pathlib.Path(__file__).resolve().parents[3];P=F.parents[2];RUN=pathlib.Path(__file__).resolve().parent;R=P/'research/runs/native-movie-ingress-20261119'
previous=F/'research/runs/native-service-runtime-20261120/publish.py'
tree=ast.parse(previous.read_text());defs=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name in {'sha','need','git','names','publish'}];assert len(defs)==5
exec(compile(ast.Module(body=defs,type_ignores=[]),str(previous)+'-checked-helpers','exec'),globals())
def validate():
 v=json.loads((RUN/'validation-02/verification.json').read_bytes());need(v['status']=='PASS_NATIVE_LOGICAL_OUTPUT_AND_SHARED_CLOCK_ACTIVATION' and len(v['fixtures'])==21 and len(v['source_after_sha256'])==519 and v['unchanged_prior_pins']==486,'actual default runtime verification')
 for n,h in v['source_after_sha256'].items():need(sha(n)==h,'current verification pin '+n)
 for stem,status in [('NATIVE_OUTPUT_ACTIVATION_SOURCE_REVIEW_02_20261120','SOURCE_REVIEW_NATIVE_OUTPUT_ACTIVATION_VERIFIED'),('NATIVE_OUTPUT_ACTIVATION_EXECUTION_REVIEW_02_20261120','SAVED_NATIVE_OUTPUT_ACTIVATION_EXECUTION_VERIFIED'),('NATIVE_OUTPUT_ACTIVATION_CLOSURE_02_20261120','NATIVE_OUTPUT_ACTIVATION_SOURCE_AND_SAVED_RESULTS_VERIFIED')]:
  d=json.loads((R/(stem+'.json')).read_bytes());need(d['status']==status and isinstance(d['input_sha256'],dict),'actual review schema '+stem)
  for n,h in d['input_sha256'].items():need(sha(n)==h,'current review pin '+n)
 for x in v['fixtures']:
  exe=F/'build_native_services_only_20261119-01'/(x['target']+'.exe');need(sha(exe)==x['executed_target_sha256'],'actual executed binary current '+x['component'])
 return v
for root,base in [(F,'edab8fe4cac857b322d8b997251588a9acdaeef2'),(P,'947313662a5fcb4be0686396b6394a5bff30eda3')]:
 need(git(root,'branch','--show-current')=='main' and git(root,'rev-parse','HEAD')==base,'expected local base')
 need(git(root,'ls-remote','--heads','origin','main').split()[0]==base,'expected remote base')
preserved={'runtime/src/iop/mcserv.cpp':'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb','runtime/src/iop/msl_native.cpp':'756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23'}
inventory=P/'research/doc-audit-20261118/inventory.json';inventory_sha='26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6'
for n,h in preserved.items():need(sha(F/n)==h,'preserved dirty IOP')
need(sha(inventory)==inventory_sha,'preserved broad inventory');v=validate()
source=['.gitattributes','runtime/include/mka/playback_owner.hpp','runtime/src/native/playback_owner.cpp','tests/test_playback_owner.cpp','tests/test_playback_reservation.cpp','tests/test_playback_reservation_gaps.cpp','tests/test_default_playback_runtime.cpp','cmake/native_sound_resources.cmake','runtime/include/mka/program_host_clock.hpp','runtime/src/native/program_host_clock.cpp','tests/test_program_host_clock.cpp','tests/test_program_output_activation.cpp']
files={F/n for n in source};files.update(q for q in RUN.rglob('*') if q.is_file() and '__pycache__' not in q.parts and (q.suffix in {'.py','.md','.json'} or 'before-whitespace-correction' in q.parts))
approved={q.relative_to(F).as_posix() for q in files};need(names(F,'diff','--name-only','-z')-approved<=set(preserved),'only authored fork tracked edits')
head,fcount=publish(F,approved,'Implement native logical program owners and shared sound clock activation')
prefix='Published fork '+chr(96)+head+chr(96)+': '
for name in ['PORT_STATUS.md','ROADMAP.md']:
 q=P/name;s=q.read_text();marker='Native logical output admission and shared clock activation are implemented.';need(s.count(marker)==1,'single actual default binding result');s=s.replace(marker,prefix+marker[0].lower()+marker[1:]);s=s.replace('Corrected source review passes; saved execution review/publication pending.','Independent source and saved execution reviews pass; this component is published.')
 if name=='PORT_STATUS.md':
  marker='| Current native source checkpoint | Published fork ';need(s.count(marker)==1,'single current checkpoint')
  s=s.replace(marker,'| Current native source checkpoint | '+prefix+'native logical output roles/signed priority/exact identity and shared-clock ordinary activation/update;21 affected clients verified (519 stable pins,486 untouched prior;1 corrected rebuild/run+20 revalidated executions,7 actual activation groups), independent source/results reviews; original absolute epoch/other passes/callbacks/forced1/renderer/full-game acceptance OPEN. Prior published fork ',1)
  lines=s.splitlines()
  for i,row in enumerate(lines):
   if row.startswith('| Native sound components |'):lines[i]='| Native sound components | Actual native role/priority/shared-clock ordinary activation and retained outputs;21 affected clients pass | Component evidence only; source callbacks/other passes/forced cleanup and physical/full-game acceptance OPEN |'
  s='\n'.join(lines)+'\n'
 q.write_text(s,encoding='utf-8',newline='\n')
receipt=R/'NATIVE_OUTPUT_ACTIVATION_PUBLICATION_20261120.json'
with receipt.open('x',encoding='utf-8',newline='\n') as out:
 json.dump(dict(status='NATIVE_OUTPUT_ACTIVATION_COMPONENT_PUBLISHED',fork_commit=head,verification_sha256=sha(RUN/'validation-02/verification.json'),verified_fixtures=21,executed_this_run=1,reused_unchanged_executions=20,source_pins=519,unchanged_prior_pins=486,fixture_binaries={x['component']:x['executed_target_sha256'] for x in v['fixtures']},preserved_dirty_sha256=preserved,limits=['Actual ordinary native role/clock/due-command activation; original absolute epoch/other passes/forced1/callbacks/full game remain OPEN','Offline decoder frozen, unchanged; no decoder corpus rerun']),out,indent=2);out.write('\n')
files={P/n for n in ['PORT_STATUS.md','ROADMAP.md']}
for stem in ['NATIVE_OUTPUT_ACTIVATION_SOURCE_REVIEW_02_20261120','NATIVE_OUTPUT_ACTIVATION_EXECUTION_REVIEW_02_20261120','NATIVE_OUTPUT_ACTIVATION_CLOSURE_02_20261120','NATIVE_OUTPUT_ACTIVATION_PLAN_20261120','NATIVE_OUTPUT_ACTIVATION_IMPLEMENTATION_20261120','NATIVE_OUTPUT_ACTIVATION_RESULTS_02_20261120']:
 files.update(R/(stem+ext) for ext in ['.md','.json'] if (R/(stem+ext)).is_file())
files.update([receipt,R/'NATIVE_OUTPUT_ACTIVATION_ROOT_WORDS_20261120.json',R/'NATIVE_PROGRAM_JOB_CURRENT_BINDING_20261120.md']);files.update(R/(stem+ext) for stem in ['NATIVE_OUTPUT_ACTIVATION_SOURCE_REVIEW_20261120','NATIVE_OUTPUT_ACTIVATION_EXECUTION_REVIEW_20261120','NATIVE_OUTPUT_ACTIVATION_CLOSURE_20261120','NATIVE_OUTPUT_ACTIVATION_RESULTS_20261120'] for ext in ['.md','.json']);files.update(q for q in (R/'native-output-activation-before-20261120').rglob('*') if q.is_file())
approved={q.relative_to(P).as_posix() for q in files};need(names(P,'diff','--name-only','-z')-approved<={'research/doc-audit-20261118/inventory.json'},'only authored parent tracked edits')
phead,pcount=publish(P,approved,'Record native logical output/shared clock component and remaining game gates')
validate()
for n,h in preserved.items():need(sha(F/n)==h,'dirty source preserved after publication')
need(sha(inventory)==inventory_sha,'inventory preserved after publication')
print(json.dumps(dict(status='PUBLISHED_NATIVE_OUTPUT_ACTIVATION_AND_DOCS',fork_commit=head,parent_commit=phead,fork_files=fcount,parent_files=pcount,fixtures=21,source_pins=519)),flush=True)
