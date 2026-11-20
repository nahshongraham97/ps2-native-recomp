"""Publish typed forced output/pool implementation and scoped evidence, fail closed."""
import ast,hashlib,json,pathlib,subprocess
F=pathlib.Path(__file__).resolve().parents[3];P=F.parents[2];RUN=pathlib.Path(__file__).resolve().parent;R=P/'research/runs/native-movie-ingress-20261119'
tree=ast.parse((F/'research/runs/native-service-runtime-20261120/publish.py').read_text());defs=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name in {'sha','need','git','names','publish'}];assert len(defs)==5
exec(compile(ast.Module(body=defs,type_ignores=[]),'checked-publication-helpers','exec'),globals())
def validate():
 v=json.loads((RUN/'validation-02/verification.json').read_bytes());need(v['status']=='PASS_NATIVE_FORCED_OUTPUT_RETIREMENT_AND_RETAINED_TAILS' and len(v['fixtures'])==24 and len(v['source_after_sha256'])==545 and v['unchanged_prior_pins']==511,'actual forced output verification schema')
 for n,h in v['source_after_sha256'].items():need(sha(n)==h,'current certificate pin '+n)
 for stem,status in [('NATIVE_FORCED_OUTPUT_SOURCE_REVIEW_20261120','SOURCE_REVIEW_NATIVE_FORCED_OUTPUT_RETIREMENT_VERIFIED'),('NATIVE_FORCED_OUTPUT_EXECUTION_REVIEW_20261120','SAVED_NATIVE_FORCED_OUTPUT_RETIREMENT_EXECUTION_VERIFIED'),('NATIVE_FORCED_OUTPUT_CLOSURE_20261120','NATIVE_FORCED_OUTPUT_SOURCE_AND_SAVED_RESULTS_VERIFIED')]:
  d=json.loads((R/(stem+'.json')).read_bytes());need(d['status']==status and isinstance(d['input_sha256'],dict),'exact review schema '+stem)
  for n,h in d['input_sha256'].items():need(sha(n)==h,'current review pin '+n)
 for x in v['fixtures']:need(sha(F/'build_native_services_only_20261119-01'/(x['target']+'.exe'))==x['executed_target_sha256'],'current executed binary '+x['component'])
 return v
for root,base in [(F,'fd197d6779cd05a3ca65a65b015d22c448cb8d18'),(P,'cc9c070bc42d4a877b8beb1811a8ea5a5f7d2222')]:
 need(git(root,'branch','--show-current')=='main' and git(root,'rev-parse','HEAD')==base,'expected local base');need(git(root,'ls-remote','--heads','origin','main').split()[0]==base,'expected remote base')
protected={'runtime/src/iop/mcserv.cpp':'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb','runtime/src/iop/msl_native.cpp':'756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23'}
for n,h in protected.items():need(sha(F/n)==h,'protected IOP')
inventory=P/'research/doc-audit-20261118/inventory.json';need(sha(inventory)=='26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6','preserved inventory');v=validate()
source=['runtime/include/mka/host_voice.hpp','runtime/src/native/host_voice.cpp','runtime/include/mka/program_copy.hpp','runtime/src/native/program_copy.cpp','runtime/include/mka/runtime_preparation.hpp','runtime/include/mka/playback_owner.hpp','runtime/src/native/playback_owner.cpp','cmake/native_sound_resources.cmake','tests/test_forced_output_retirement.cpp','tests/test_forced_output_retirement_failure.cpp']
files={F/n for n in source};files.update(q for q in RUN.rglob('*') if q.is_file() and q.suffix in {'.py','.md','.json'} and '__pycache__' not in q.parts)
for q in files:
 b=q.read_bytes();need(b'\r' not in b and b.endswith(b'\n') and not b.endswith(b'\n\n') and all(line==line.rstrip(b' \t') for line in b.splitlines()),'full authored LF/EOF preflight '+str(q))
approved={q.relative_to(F).as_posix() for q in files};need(names(F,'diff','--name-only','-z')-approved<=set(protected),'only authored fork tracked edits');head,fcount=publish(F,approved,'Retire native program resources while preserving real release tails')
prefix='Published fork '+chr(96)+head+chr(96)+': '
for n in ['PORT_STATUS.md','ROADMAP.md']:
 q=P/n;s=q.read_text();marker='Actual native forced output retirement and retained-tail mixing are implemented.';need(s.count(marker)==1,'single current forced result');s=s.replace(marker,prefix+marker[0].lower()+marker[1:]);s=s.replace('Independent source review passes; saved execution review/publication pending.','Independent source and saved execution reviews pass; this component is published.')
 if n=='PORT_STATUS.md':
  marker='| Current native source checkpoint | Published fork ';need(s.count(marker)==1,'single current checkpoint');s=s.replace(marker,'| Current native source checkpoint | '+prefix+'typed normal resource retirement with actual retained native tails/semantic cleanup;24 affected clients pass (545 stable pins,511 untouched prior;8single/11context allocator faults), independent source/results reviews; source worker/callback/notification/command12/renderer/full-game acceptance OPEN. Prior published fork ',1)
  rows=s.splitlines()
  for i,row in enumerate(rows):
   if row.startswith('| Native sound components |'):rows[i]='| Native sound components | Actual ordinary ownership/clock and typed resource retirement with retained native tails;24 clients pass | Component evidence only; source worker/callback/full command12/current-game binding and physical/all-game acceptance OPEN |'
  s='\n'.join(rows)+'\n'
 q.write_text(s,encoding='utf-8',newline='\n')
receipt=R/'NATIVE_FORCED_OUTPUT_PUBLICATION_20261120.json'
with receipt.open('x',encoding='utf-8',newline='\n') as out:
 json.dump(dict(status='NATIVE_FORCED_OUTPUT_COMPONENT_PUBLISHED',fork_commit=head,verification_sha256=sha(RUN/'validation-02/verification.json'),executed_fixtures=24,source_pins=545,untouched_prior_pins=511,fixture_binaries={x['component']:x['executed_target_sha256'] for x in v['fixtures']},protected_dirty_sha256=protected,limits=['Typed normal resource retirement; complete command12/source worker/notifications/alternate and full-game acceptance OPEN','Native tails are real software output, not hardware timing/heard fidelity','Decoder unchanged; no physical endpoint/game launch']),out,indent=2);out.write('\n')
files={P/n for n in ['PORT_STATUS.md','ROADMAP.md','.gitattributes']};files.update([receipt,R/'NATIVE_CURRENT_SOUND_NEXT_IMPLEMENTATION_20261120.md'])
for stem in ['NATIVE_FORCED_OUTPUT_PLAN_20261120','NATIVE_FORCED_OUTPUT_IMPLEMENTATION_20261120','NATIVE_FORCED_OUTPUT_SOURCE_REVIEW_20261120','NATIVE_FORCED_OUTPUT_EXECUTION_REVIEW_20261120','NATIVE_FORCED_OUTPUT_RESULTS_20261120','NATIVE_FORCED_OUTPUT_CLOSURE_20261120']:
 files.update(R/(stem+ext) for ext in ['.md','.json'] if (R/(stem+ext)).is_file())
for folder in ['native-forced-output-before-20261120','native-forced-output-before-encoding-correction-20261120']:
 files.update(q for q in (R/folder).rglob('*') if q.is_file())
approved={q.relative_to(P).as_posix() for q in files};need(names(P,'diff','--name-only','-z')-approved<={'research/doc-audit-20261118/inventory.json'},'only authored parent tracked edits');phead,pcount=publish(P,approved,'Record native retained-tail retirement and exact remaining sound binding')
validate()
for n,h in protected.items():need(sha(F/n)==h,'protected IOP after publication')
need(sha(inventory)=='26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6','inventory after publication')
print(json.dumps(dict(status='PUBLISHED_NATIVE_FORCED_OUTPUT_AND_DOCS',fork_commit=head,parent_commit=phead,fork_files=fcount,parent_files=pcount,fixtures=24,pins=545)),flush=True)
