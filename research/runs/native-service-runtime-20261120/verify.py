"""Actual retained Service -> runtime -> copy native ownership; CPU/files only."""
import ast,ctypes,datetime,importlib.util,json,os,pathlib,re,subprocess,sys
F=pathlib.Path(__file__).resolve().parents[3];P=F.parents[2];R=P/'research/runs/native-movie-ingress-20261119'
BASE=F/'research/runs/native-program-copy-20261120/verify.py'
spec=importlib.util.spec_from_file_location('_service_runtime_prior',BASE);old=importlib.util.module_from_spec(spec);spec.loader.exec_module(old);c=old.c
CHANGED=['runtime/include/mka/runtime_preparation.hpp','runtime/src/native/bank_preparation.cpp','runtime/include/mka/audio_banks.hpp','runtime/src/native/audio_banks.cpp','runtime/include/mka/audio_bank_protocol.hpp','runtime/src/native/audio_bank_protocol.cpp','runtime/include/mka/audio_bank_service.hpp','runtime/src/native/audio_bank_service.cpp','cmake/native_sound_resources.cmake']
NEW=['tests/test_service_runtime.cpp','tests/test_service_runtime_gaps.cpp']
def prior_cases(ms2,ms4):
 tree=ast.parse(BASE.read_text());fun=next(n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name=='verify')
 nodes=[n for n in fun.body if isinstance(n,(ast.Assign,ast.AugAssign)) and ((isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='cases' for t in n.targets)) or (isinstance(n,ast.AugAssign) and isinstance(n.target,ast.Name) and n.target.id=='cases'))]
 a=F/'research/runs/native-program-admission-20261120/verify.py';b=F/'research/runs/native-deferred-program-work-20261120/verify.py'
 scope=dict(ms2=ms2,ms4=ms4,e=old.e,c=c,a=a,b=b);exec(compile(ast.Module(body=nodes,type_ignores=[]),'prior-affected-case-definitions','exec'),scope)
 need=c.require;need(len(scope['cases'])==17,'exact saved prior case definition');return [x for x in scope['cases'] if x[0]!='preparation']
def verify():
 need,sha,write=c.require,c.sha,c.write
 cache=(c.BUILD/'CMakeCache.txt').read_text(encoding='utf-8-sig');need(bool(re.search(r'^PS2_GENERATED:PATH=$',cache,re.M)) and bool(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$',cache,re.M)),'native Release empty generated input')
 prior_path=F/'research/runs/native-program-copy-20261120/validation-01/verification.json';prior=json.loads(prior_path.read_bytes());need(prior['status']=='PASS_NATIVE_PROGRAM_COPY_AND_ORDINARY_SCHEDULE_PRIMITIVES' and len(prior['source_after_sha256'])==445 and len(prior['fixtures'])==17,'actual prior schema')
 changed={str(F/n) for n in CHANGED};untouched={n:h for n,h in prior['source_after_sha256'].items() if n not in changed};need(all(sha(n)==h for n,h in untouched.items()),'untouched prior identities')
 review_path=R/'NATIVE_SERVICE_RUNTIME_SOURCE_REVIEW_20261120.json';review=json.loads(review_path.read_bytes());need(review['status']=='SOURCE_REVIEW_NATIVE_SERVICE_RUNTIME_ADMISSION_VERIFIED' and set(review['input_sha256'])=={str(F/n) for n in CHANGED+NEW},'actual eleven-input review schema');need(all(sha(n)==h for n,h in review['input_sha256'].items()),'final source review current')
 proof_path=R/'NATIVE_SERVICE_RUNTIME_ROOT_WORDS_20261120.json';proof=json.loads(proof_path.read_bytes());need(proof['status']=='OUTPUT_OWNER_COMMIT_AND_MODE0_RETENTION_RAW_WORD_ROWS_VERIFIED' and proof['word_rows']==1306 and len(proof['exports'])==20,'original source raw proof')
 paths={pathlib.Path(n) for n in prior['source_after_sha256']};paths.update(F/n for n in CHANGED+NEW)
 paths.update([pathlib.Path(__file__).resolve(),BASE,prior_path,review_path,review_path.with_suffix('.md'),proof_path,R/'NATIVE_SERVICE_RUNTIME_PLAN_20261120.md'])
 for stem in ['NATIVE_PROGRAM_OUTPUT_ADMISSION_SOURCE_20261120','NATIVE_PROGRAM_ACTIVATION_COMMIT_SOURCE_20261120']:
  q=R/(stem+'.json');d=json.loads(q.read_bytes());need(d['source_only'] is True and isinstance(d['input_sha256'],dict),'original source review schema');paths.update([q,q.with_suffix('.md')])
  for n,h in d['input_sha256'].items():
   q=pathlib.Path(n)
   if n in changed:q=R/'native-service-runtime-before-20261120'/q.relative_to(F)
   need(sha(q)==h,'preserved original source input');paths.add(q)
 for export in proof['exports']:
  q=pathlib.Path(export['path']);need(sha(q)==export['sha256'],'actual original export');paths.add(q)
 before={str(q):sha(q) for q in sorted(paths)}
 for n in CHANGED+NEW:need(b'\r' not in (F/n).read_bytes(),'authored LF '+n)
 for n,h in c.PRESERVE.items():need(sha(F/n)==h,'preserved dirty IOP')
 write('provenance.json',dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),fork_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=F,text=True).strip(),parent_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=P,text=True).strip(),source_before_sha256=before,unchanged_prior_pins=len(untouched),compiler_sha256=sha(c.BIN/'clang++.exe'),scope='Actual Service operation ownership across runtime reference/copy lifecycle; no source activation/game'))
 ms2=F/'games/mkarmageddon/disc-files/SNDSPS2/MSLASSET.MS2';ms4=ms2.with_suffix('.MS4')
 cases=[
  ('service-runtime','test_native_service_runtime',[ms2],r'PASS_NATIVE_SERVICE_RUNTIME: 5 actual Service runtime-reference/copy/output lifetime cases; no source callback/activation/device/fidelity claim'),
  ('runtime-gaps','test_native_service_runtime_gaps',[ms2],r'PASS native Service runtime pre/post acquisition guards and failure rollback: five CPU-only injected boundaries')]+prior_cases(ms2,ms4)
 need(len(cases)==18 and len({x[1] for x in cases})==18,'eighteen affected actual targets')
 c.execute('build.json',[c.CMAKE,'--build',c.BUILD,'--target',*[x[1] for x in cases],'-j2'])
 fixtures=[]
 for label,target,args,footer in cases:
  print('RUN '+label,flush=True);exe=c.BUILD/(target+'.exe');run=c.execute(label+'.json',[exe,*args]);need(bool(re.fullmatch(footer,run['stdout'].splitlines()[-1])),'complete assertions '+label)
  if label in ['service-runtime','runtime-gaps']:required=['mka::sound::AudioBankService::acquire_runtime_program']
  elif label.startswith('copy'):required=['mka::sound::NativeProgramCopy::clone']
  elif label in ['program','playback','reservation','owner-gaps']:required=['mka::sound::sample_start_operands']
  else:required=['mka::sound::PreparedBank::acquire_runtime_program']
  audit=c.symbols(label+'-symbols',exe,run['executable_sha256'],required,True);fixtures.append(dict(component=label,target=target,executed_target_sha256=run['executable_sha256'],audit=audit))
 ninja=c.BUILD/'build.ninja';text=ninja.read_text();rules=[]
 for label,target,_,_ in cases:
  match=re.search(r'^build '+re.escape(target+'.exe')+r':.*(?:\n[ \t].*)*',text,re.M);need(match is not None and 'ps2runtime' not in match.group() and 'source_oracle' not in match.group(),'native-only actual target')
  wraps=re.findall(r'--wrap=([^\s"]+)',match.group());need(len(wraps)=={'runtime-gaps':1,'copy-failure':1,'gates':1,'exceptions':1,'owner-gaps':5}.get(label,0),'designated test-only wraps '+label)
  if label=='runtime-gaps':need(wraps==['_ZN3mka5sound17AudioBankProtocol23acquire_runtime_programERKNS0_16AudioBankCommandERNS0_18PreparationFailureERNS0_20AudioProtocolFailureE'],'exact actual Protocol runtime call wrapped')
  rules.append(dict(target=target,actual_build_rule=match.group(),test_only_wraps=wraps))
 write('linkage-audit.json',dict(ninja_sha256=sha(ninja),target_rules=rules))
 need(before=={n:sha(n) for n in before},'source/tool/review/assets unchanged')
 frozen=F/'research/runs/native-adx-composition-20261119/validation-02/audio-oracle.json';need(sha(frozen)==prior['frozen_decoder_certificate_sha256'],'frozen decoder preserved')
 for n,h in c.PRESERVE.items():need(sha(F/n)==h,'dirty IOP preserved after execution')
 write('verification.json',dict(status='PASS_NATIVE_SERVICE_RUNTIME_REFERENCE_BINDING',source_after_sha256=before,unchanged_prior_pins=len(untouched),fixtures=fixtures,frozen_decoder_certificate_sha256=sha(frozen),limits=['Actual retained Service/Protocol/Session/runtime/copy ownership, not source output admission/clock/activation/callback or game acceptance','Native runtime ownership counters are not source bank+2C callback counts','Existing busy-load/work guards retain error precedence; runtime_reference_active applies to otherwise admissible teardown','Mode0 ownership stays retained until semantic release/copy cleanup; source forced1 and hardware tails remain unbound','No new donor/codec behavior, no whole-bank/decoder or unrelated numeric corpus rerun','No game/physical endpoints/local model/server/driver/settings/card/save/key operation']))
 print(json.dumps(dict(status='PASS_NATIVE_SERVICE_RUNTIME_REFERENCE_BINDING',fixtures=len(fixtures),pins=len(before),untouched=len(untouched))),flush=True)
if __name__=='__main__':
 ctypes.windll.kernel32.SetErrorMode(3)
 if len(sys.argv)!=2 or not re.fullmatch(r'validation-[0-9]+',sys.argv[1]):raise SystemExit('usage: verify.py validation-NN')
 c.OUT=pathlib.Path(__file__).resolve().parent/sys.argv[1];c.OUT.mkdir(exist_ok=False);c.ENV=dict(os.environ,PATH=str(c.BIN)+os.pathsep+os.environ['PATH'])
 try:verify()
 except Exception as error:c.write('aborted.json',dict(status='NATIVE_SERVICE_RUNTIME_CHECK_FAILED',error=str(error)));raise
