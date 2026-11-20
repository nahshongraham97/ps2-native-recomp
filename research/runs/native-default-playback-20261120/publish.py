"""Publish default native playback binding and scoped evidence, fail closed."""
import ast,hashlib,json,pathlib,subprocess
F=pathlib.Path(__file__).resolve().parents[3];P=F.parents[2];RUN=pathlib.Path(__file__).resolve().parent;R=P/'research/runs/native-movie-ingress-20261119'
previous=F/'research/runs/native-service-runtime-20261120/publish.py'
tree=ast.parse(previous.read_text());defs=[n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name in {'sha','need','git','names','publish'}];assert len(defs)==5
exec(compile(ast.Module(body=defs,type_ignores=[]),str(previous)+'-checked-helpers','exec'),globals())
def validate():
 v=json.loads((RUN/'validation-01/verification.json').read_bytes());need(v['status']=='PASS_NATIVE_DEFAULT_PLAYBACK_RUNTIME_COPY_BINDING' and len(v['fixtures'])==19 and len(v['source_after_sha256'])==493 and v['unchanged_prior_pins']==470,'actual default runtime verification')
 for n,h in v['source_after_sha256'].items():need(sha(n)==h,'current verification pin '+n)
 for stem,status in [('NATIVE_DEFAULT_PLAYBACK_SOURCE_REVIEW_20261120','SOURCE_REVIEW_NATIVE_DEFAULT_PLAYBACK_BINDING_VERIFIED'),('NATIVE_DEFAULT_PLAYBACK_EXECUTION_REVIEW_20261120','SAVED_NATIVE_DEFAULT_PLAYBACK_EXECUTION_VERIFIED'),('NATIVE_DEFAULT_PLAYBACK_CLOSURE_20261120','NATIVE_DEFAULT_PLAYBACK_SOURCE_AND_SAVED_RESULTS_VERIFIED')]:
  d=json.loads((R/(stem+'.json')).read_bytes());need(d['status']==status and isinstance(d['input_sha256'],dict),'actual review schema '+stem)
  for n,h in d['input_sha256'].items():need(sha(n)==h,'current review pin '+n)
 for x in v['fixtures']:
  exe=F/'build_native_services_only_20261119-01'/(x['target']+'.exe');need(sha(exe)==x['executed_target_sha256'],'actual executed binary current '+x['component'])
 return v
for root,base in [(F,'8ea307f38ac18316d5e222afa716304bc7071597'),(P,'ec464f88c2913ab245496b5743e906363bd41528')]:
 need(git(root,'branch','--show-current')=='main' and git(root,'rev-parse','HEAD')==base,'expected local base')
 need(git(root,'ls-remote','--heads','origin','main').split()[0]==base,'expected remote base')
preserved={'runtime/src/iop/mcserv.cpp':'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb','runtime/src/iop/msl_native.cpp':'756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23'}
inventory=P/'research/doc-audit-20261118/inventory.json';inventory_sha='26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6'
for n,h in preserved.items():need(sha(F/n)==h,'preserved dirty IOP')
need(sha(inventory)==inventory_sha,'preserved broad inventory');v=validate()
source=['runtime/include/mka/playback_owner.hpp','runtime/src/native/playback_owner.cpp','runtime/include/mka/program_copy.hpp','runtime/src/native/program_copy.cpp','tests/test_playback_owner.cpp','tests/test_playback_reservation.cpp','tests/test_playback_reservation_gaps.cpp','cmake/native_sound_resources.cmake','tests/test_default_playback_runtime.cpp']
files={F/n for n in source};files.update(q for q in RUN.rglob('*') if q.is_file() and q.suffix in {'.py','.md','.json'} and '__pycache__' not in q.parts)
approved={q.relative_to(F).as_posix() for q in files};need(names(F,'diff','--name-only','-z')-approved<=set(preserved),'only authored fork tracked edits')
head,fcount=publish(F,approved,'Bind default native playback to retained runtime copies and outputs')
prefix='Published fork '+chr(96)+head+chr(96)+': '
for name in ['PORT_STATUS.md','ROADMAP.md']:
 q=P/name;s=q.read_text();marker='Default NativePlaybackOwner now uses the retained Service/runtime/private-copy route.';need(s.count(marker)==1,'single actual default binding result');s=s.replace(marker,prefix+marker[0].lower()+marker[1:]);s=s.replace('Source review passes; saved execution review/publication are pending.','Independent source and saved execution reviews pass; this component is published.')
 if name=='PORT_STATUS.md':
  marker='| Current native source checkpoint | Published fork ';need(s.count(marker)==1,'single current checkpoint')
  s=s.replace(marker,'| Current native source checkpoint | '+prefix+'default Owner routes actual Service/runtime acquisition through private copies and native output lifetime;19 affected clients pass (493 stable pins,470 untouched prior;7 new default lifecycle groups), independent source/results reviews; source output admission/shared clock/list/forced1/callbacks/renderer/full-game acceptance OPEN. Prior published fork ',1)
  lines=s.splitlines()
  for i,row in enumerate(lines):
   if row.startswith('| Native sound components |'):lines[i]='| Native sound components | Actual default Service/runtime/private-copy and native output ownership;19 affected clients pass | Component evidence only; source output admission/shared clock/list/activation/callback/forced cleanup and physical/full-game acceptance OPEN |'
  s='\n'.join(lines)+'\n'
 q.write_text(s,encoding='utf-8',newline='\n')
receipt=R/'NATIVE_DEFAULT_PLAYBACK_PUBLICATION_20261120.json'
with receipt.open('x',encoding='utf-8',newline='\n') as out:
 json.dump(dict(status='NATIVE_DEFAULT_PLAYBACK_RUNTIME_COPY_COMPONENT_PUBLISHED',fork_commit=head,verification_sha256=sha(RUN/'validation-01/verification.json'),executed_fixtures=19,source_pins=493,unchanged_prior_pins=470,fixture_binaries={x['component']:x['executed_target_sha256'] for x in v['fixtures']},preserved_dirty_sha256=preserved,limits=['Actual default preactivated native helper; source output admission/shared clock/list/forced1/callbacks/full game remain OPEN','Offline decoder frozen, unchanged; no decoder corpus rerun']),out,indent=2);out.write('\n')
files={P/n for n in ['PORT_STATUS.md','ROADMAP.md']}
for stem in ['NATIVE_DEFAULT_PLAYBACK_SOURCE_REVIEW_20261120','NATIVE_DEFAULT_PLAYBACK_EXECUTION_REVIEW_20261120','NATIVE_DEFAULT_PLAYBACK_CLOSURE_20261120','NATIVE_DEFAULT_PLAYBACK_PLAN_20261120','NATIVE_DEFAULT_PLAYBACK_IMPLEMENTATION_20261120','NATIVE_DEFAULT_PLAYBACK_RESULTS_20261120']:
 files.update(R/(stem+ext) for ext in ['.md','.json'] if (R/(stem+ext)).is_file())
files.add(receipt);files.update(q for q in (R/'native-default-playback-before-20261120').rglob('*') if q.is_file())
approved={q.relative_to(P).as_posix() for q in files};need(names(P,'diff','--name-only','-z')-approved<={'research/doc-audit-20261118/inventory.json'},'only authored parent tracked edits')
phead,pcount=publish(P,approved,'Record default native playback binding and frozen decoder scope')
validate()
for n,h in preserved.items():need(sha(F/n)==h,'dirty source preserved after publication')
need(sha(inventory)==inventory_sha,'inventory preserved after publication')
print(json.dumps(dict(status='PUBLISHED_NATIVE_DEFAULT_PLAYBACK_AND_DOCS',fork_commit=head,parent_commit=phead,fork_files=fcount,parent_files=pcount,fixtures=19,source_pins=493)),flush=True)
