"""Actual default native playback runtime/copy binding; CPU/files only."""
import ast,ctypes,datetime,importlib.util,json,os,pathlib,re,subprocess,sys
F=pathlib.Path(__file__).resolve().parents[3];P=F.parents[2];R=P/'research/runs/native-movie-ingress-20261119'
BASE=F/'research/runs/native-service-runtime-20261120/verify.py'
spec=importlib.util.spec_from_file_location('_default_playback_prior',BASE);old=importlib.util.module_from_spec(spec);spec.loader.exec_module(old);c=old.c
CHANGED=['runtime/include/mka/playback_owner.hpp','runtime/src/native/playback_owner.cpp','runtime/include/mka/program_copy.hpp','runtime/src/native/program_copy.cpp','tests/test_playback_owner.cpp','tests/test_playback_reservation.cpp','tests/test_playback_reservation_gaps.cpp','cmake/native_sound_resources.cmake']
NEW=['tests/test_default_playback_runtime.cpp']
def prior_cases(ms2,ms4):
 tree=ast.parse(BASE.read_text());fun=next(n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name=='verify')
 nodes=[n for n in fun.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='cases' for t in n.targets)]
 scope=dict(ms2=ms2,ms4=ms4,prior_cases=old.prior_cases);exec(compile(ast.Module(body=nodes,type_ignores=[]),'prior-service-runtime-cases','exec'),scope)
 c.require(len(scope['cases'])==18,'actual prior18 cases');return scope['cases']
def verify():
 need,sha,write=c.require,c.sha,c.write
 cache=(c.BUILD/'CMakeCache.txt').read_text(encoding='utf-8-sig');need(bool(re.search(r'^PS2_GENERATED:PATH=$',cache,re.M)) and bool(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$',cache,re.M)),'native Release empty generated input')
 prior_path=F/'research/runs/native-service-runtime-20261120/validation-01/verification.json';prior=json.loads(prior_path.read_bytes());need(prior['status']=='PASS_NATIVE_SERVICE_RUNTIME_REFERENCE_BINDING' and len(prior['source_after_sha256'])==478 and len(prior['fixtures'])==18,'actual prior schema')
 changed={str(F/n) for n in CHANGED};untouched={n:h for n,h in prior['source_after_sha256'].items() if n not in changed};need(all(sha(n)==h for n,h in untouched.items()),'untouched prior identities')
 review_path=R/'NATIVE_DEFAULT_PLAYBACK_SOURCE_REVIEW_20261120.json';review=json.loads(review_path.read_bytes());need(review['status']=='SOURCE_REVIEW_NATIVE_DEFAULT_PLAYBACK_BINDING_VERIFIED' and set(review['input_sha256'])=={str(F/n) for n in CHANGED+NEW},'actual nine-input source review schema');need(all(sha(n)==h for n,h in review['input_sha256'].items()),'final source review current')
 paths={pathlib.Path(n) for n in prior['source_after_sha256']};paths.update(F/n for n in CHANGED+NEW);paths.update([pathlib.Path(__file__).resolve(),BASE,prior_path,review_path,review_path.with_suffix('.md'),R/'NATIVE_DEFAULT_PLAYBACK_PLAN_20261120.md',R/'NATIVE_DEFAULT_PLAYBACK_IMPLEMENTATION_20261120.md'])
 for n in CHANGED:
  q=R/'native-default-playback-before-20261120'/n;need(sha(q)==prior['source_after_sha256'][str(F/n)],'exact original before source '+n);paths.add(q)
 for stem in ['NATIVE_PROGRAM_OUTPUT_ADMISSION_SOURCE_20261120','NATIVE_PROGRAM_ACTIVATION_COMMIT_SOURCE_20261120','NATIVE_SERVICE_RUNTIME_ROOT_WORDS_20261120']:
  q=R/(stem+'.json');paths.add(q)
 before={str(q):sha(q) for q in sorted(paths)}
 for n in CHANGED+NEW:need(b'\r' not in (F/n).read_bytes(),'authored LF '+n)
 for n,h in c.PRESERVE.items():need(sha(F/n)==h,'preserved dirty IOP')
 write('provenance.json',dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),fork_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=F,text=True).strip(),parent_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=P,text=True).strip(),source_before_sha256=before,unchanged_prior_pins=len(untouched),compiler_sha256=sha(c.BIN/'clang++.exe'),scope='Actual default Owner -> Service runtime reference -> private copy/voices; no source activation/game'))
 ms2=F/'games/mkarmageddon/disc-files/SNDSPS2/MSLASSET.MS2';ms4=ms2.with_suffix('.MS4')
 cases=[('default-runtime','test_native_default_playback_runtime',[ms2],r'PASS_NATIVE_DEFAULT_PLAYBACK_RUNTIME: 7 actual default runtime/copy/flag/retirement cases; no source activation/callback/device/fidelity claim')]+prior_cases(ms2,ms4)
 need(len(cases)==19 and len({x[1] for x in cases})==19,'nineteen actual affected targets')
 c.execute('build.json',[c.CMAKE,'--build',c.BUILD,'--target',*[x[1] for x in cases],'-j2'])
 fixtures=[]
 for label,target,args,footer in cases:
  print('RUN '+label,flush=True);exe=c.BUILD/(target+'.exe');run=c.execute(label+'.json',[exe,*args]);need(bool(re.fullmatch(footer,run['stdout'].splitlines()[-1])),'complete assertions '+label)
  if label in ['default-runtime','playback','reservation','owner-gaps']:required=['mka::sound::NativePlaybackOwner::play','mka::sound::NativeProgramCopy::clone','mka::sound::AudioBankService::acquire_runtime_program']
  elif label in ['service-runtime','runtime-gaps']:required=['mka::sound::AudioBankService::acquire_runtime_program']
  elif label.startswith('copy'):required=['mka::sound::NativeProgramCopy::clone']
  elif label=='program':required=['mka::sound::sample_start_operands']
  else:required=['mka::sound::PreparedBank::acquire_runtime_program']
  audit=c.symbols(label+'-symbols',exe,run['executable_sha256'],required,True);fixtures.append(dict(component=label,target=target,executed_target_sha256=run['executable_sha256'],audit=audit))
 ninja=c.BUILD/'build.ninja';text=ninja.read_text();rules=[]
 for label,target,_,_ in cases:
  match=re.search(r'^build '+re.escape(target+'.exe')+r':.*(?:\n[ \t].*)*',text,re.M);need(match is not None and 'ps2runtime' not in match.group() and 'source_oracle' not in match.group(),'native-only actual target')
  wraps=re.findall(r'--wrap=([^\s"]+)',match.group());need(len(wraps)=={'default-runtime':1,'runtime-gaps':1,'copy-failure':1,'gates':1,'exceptions':1,'owner-gaps':5}.get(label,0),'designated test-only wraps '+label)
  if label=='default-runtime':need(wraps==['_ZN3mka5sound17AudioBankProtocol6unloadERKNS0_16AudioBankCommandERNS0_20AudioProtocolFailureE'],'real lower Protocol unload wrapped')
  if label=='owner-gaps':need('_ZN3mka5sound16AudioBankService23acquire_runtime_programERKNS0_16AudioBankCommandERNS0_18PreparationFailureERNS0_18BankServiceFailureE' in wraps,'actual default runtime producer wrapped')
  rules.append(dict(target=target,actual_build_rule=match.group(),test_only_wraps=wraps))
 write('linkage-audit.json',dict(ninja_sha256=sha(ninja),target_rules=rules))
 need(before=={n:sha(n) for n in before},'source/tool/review/assets unchanged')
 frozen=F/'research/runs/native-adx-composition-20261119/validation-02/audio-oracle.json';need(sha(frozen)==prior['frozen_decoder_certificate_sha256'],'frozen decoder preserved')
 for n,h in c.PRESERVE.items():need(sha(F/n)==h,'dirty IOP preserved after execution')
 write('verification.json',dict(status='PASS_NATIVE_DEFAULT_PLAYBACK_RUNTIME_COPY_BINDING',source_after_sha256=before,unchanged_prior_pins=len(untouched),fixtures=fixtures,frozen_decoder_certificate_sha256=sha(frozen),limits=['Actual default preactivated native helper, not source output admission/clock/list/activation/callback or game acceptance','Native semantic/Service ownership counters are not source bank+2C callback counts','Mode0 retains copy/reference through real native idle retirement; source forced1/hardware tail remains unbound','Unsupported combined mode1/alternate flags fail before public handle publication','One aggregate mixer transaction and outside-registry resource retirement; no whole-bank/decoder/unrelated numeric corpus rerun','No game/physical endpoints/local model/server/driver/settings/card/save/key operation']))
 print(json.dumps(dict(status='PASS_NATIVE_DEFAULT_PLAYBACK_RUNTIME_COPY_BINDING',fixtures=len(fixtures),pins=len(before),untouched=len(untouched))),flush=True)
if __name__=='__main__':
 ctypes.windll.kernel32.SetErrorMode(3)
 if len(sys.argv)!=2 or not re.fullmatch(r'validation-[0-9]+',sys.argv[1]):raise SystemExit('usage: verify.py validation-NN')
 c.OUT=pathlib.Path(__file__).resolve().parent/sys.argv[1];c.OUT.mkdir(exist_ok=False);c.ENV=dict(os.environ,PATH=str(c.BIN)+os.pathsep+os.environ['PATH'])
 try:verify()
 except Exception as error:c.write('aborted.json',dict(status='NATIVE_DEFAULT_PLAYBACK_CHECK_FAILED',error=str(error)));raise
