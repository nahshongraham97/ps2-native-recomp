"""Actual typed native forced output retirement; retained PCM tails; CPU/files only."""
import ast,ctypes,datetime,importlib.util,json,os,pathlib,re,subprocess,sys
F=pathlib.Path(__file__).resolve().parents[3];P=F.parents[2];R=P/'research/runs/native-movie-ingress-20261119'
BASE=F/'research/runs/native-output-activation-20261120/verify.py';spec=importlib.util.spec_from_file_location('_forced_output_prior',BASE);old=importlib.util.module_from_spec(spec);spec.loader.exec_module(old);c=old.c
CHANGED=['runtime/include/mka/host_voice.hpp','runtime/src/native/host_voice.cpp','runtime/include/mka/program_copy.hpp','runtime/src/native/program_copy.cpp','runtime/include/mka/runtime_preparation.hpp','runtime/include/mka/playback_owner.hpp','runtime/src/native/playback_owner.cpp','cmake/native_sound_resources.cmake']
NEW=['tests/test_forced_output_retirement.cpp','tests/test_forced_output_retirement_failure.cpp']
def prior_cases(ms2,ms4):
 tree=ast.parse(BASE.read_text());fun=next(n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name=='verify');nodes=[n for n in fun.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='cases' for t in n.targets)]
 scope=dict(ms2=ms2,ms4=ms4,prior_cases=old.prior_cases);exec(compile(ast.Module(body=nodes,type_ignores=[]),'prior21nativecases','exec'),scope);c.require(len(scope['cases'])==21,'actual prior21cases');return scope['cases']
def verify():
 need,sha,write=c.require,c.sha,c.write
 cache=(c.BUILD/'CMakeCache.txt').read_text(encoding='utf-8-sig');need(bool(re.search(r'^PS2_GENERATED:PATH=$',cache,re.M)) and bool(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$',cache,re.M)),'native Release empty generated input')
 prior_path=F/'research/runs/native-output-activation-20261120/validation-02/verification.json';prior=json.loads(prior_path.read_bytes());need(prior['status']=='PASS_NATIVE_LOGICAL_OUTPUT_AND_SHARED_CLOCK_ACTIVATION' and len(prior['source_after_sha256'])==519 and len(prior['fixtures'])==21,'actual prior corrected schema')
 changed={str(F/n) for n in CHANGED};untouched={n:h for n,h in prior['source_after_sha256'].items() if n not in changed};need(len(untouched)==511 and all(sha(n)==h for n,h in untouched.items()),'untouched prior511identities')
 review_path=R/'NATIVE_FORCED_OUTPUT_SOURCE_REVIEW_20261120.json';review=json.loads(review_path.read_bytes());need(review['status']=='SOURCE_REVIEW_NATIVE_FORCED_OUTPUT_RETIREMENT_VERIFIED' and review['source_only'] is True and set(review['input_sha256'])=={str(F/n) for n in CHANGED+NEW},'actual10input source review schema');need(all(sha(n)==h for n,h in review['input_sha256'].items()),'current source review')
 paths={pathlib.Path(n) for n in prior['source_after_sha256']};paths.update(F/n for n in CHANGED+NEW);paths.add(F/'tests/test_host_voice.cpp');paths.update([prior_path,pathlib.Path(__file__).resolve(),review_path,review_path.with_suffix('.md'),R/'NATIVE_FORCED_OUTPUT_PLAN_20261120.md',R/'NATIVE_FORCED_OUTPUT_IMPLEMENTATION_20261120.md'])
 for n in CHANGED:
  q=R/'native-forced-output-before-20261120'/n;need(sha(q)==prior['source_after_sha256'][str(F/n)],'exact before bytes '+n);paths.add(q)
 proof=R/'NATIVE_FORCED_OUTPUT_RETIREMENT_SOURCE_20261120.json';d=json.loads(proof.read_bytes());need(d['status']=='FORCED_NATIVE_OUTPUT_RETIREMENT_BOUNDARY_VERIFIED' and d['source_only'] is True and isinstance(d['input_sha256'],dict),'actual source ordering report schema');paths.update([proof,proof.with_suffix('.md'),R/'NATIVE_CURRENT_SOUND_NEXT_ROOT_CHECK_20261120.json'])
 for n,h in d['input_sha256'].items():
  q=pathlib.Path(n)
  if n in changed:q=R/'native-forced-output-before-20261120'/q.relative_to(F)
  need(sha(q)==h,'source proof input preserved '+str(q));paths.add(q)
 for e in d['existing_raw_exports']+d['new_raw_exports']:
  q=pathlib.Path(e['path']);need(sha(q)==e['sha256'],'original ordering raw bytes');paths.add(q)
 before={str(q):sha(q) for q in sorted(paths)}
 for n in CHANGED+NEW:
  data=(F/n).read_bytes();need(b'\r' not in data and data.endswith(b'\n') and not data.endswith(b'\n\n') and all(line==line.rstrip(b' \t') for line in data.splitlines()),'full LF/EOF/whitespace '+n)
 for n,h in c.PRESERVE.items():need(sha(F/n)==h,'protected dirty IOP')
 write('provenance.json',dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),fork_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=F,text=True).strip(),parent_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=P,text=True).strip(),source_before_sha256=before,unchanged_prior_pins=len(untouched),compiler_sha256=sha(c.BIN/'clang++.exe'),scope='Actual Copy forced transfer/Owner retained native tails/semantic cleanup, not source callback or complete command12/game'))
 ms2=F/'games/mkarmageddon/disc-files/SNDSPS2/MSLASSET.MS2';ms4=ms2.with_suffix('.MS4')
 cases=[('voice','test_native_host_voice',[ms2],r'PASS realannouncr freshpan/LUT/phase/explicitprofiles/signedDSP/hostresample/64voices/transactionalstop; no device/output/fidelity claim'),('forced-output','test_native_forced_output_retirement',[ms2],r'PASS_NATIVE_FORCED_OUTPUT_RETIREMENT: 4 actual Owner output retirement groups; software tails only'),('forced-failure','test_native_forced_output_retirement_failure',[ms2],r'PASS_NATIVE_FORCED_OUTPUT_RETIREMENT_FAILURE: [0-9]+ single and [0-9]+ context allocation failures; mixed-pool rollback; test-only allocator wrap')]+prior_cases(ms2,ms4)
 need(len(cases)==24 and len({x[1] for x in cases})==24,'24actual affected targets');c.execute('build.json',[c.CMAKE,'--build',c.BUILD,'--target',*[x[1] for x in cases],'-j2'])
 fixtures=[]
 for label,target,args,footer in cases:
  print('RUN '+label,flush=True);exe=c.BUILD/(target+'.exe');run=c.execute(label+'.json',[exe,*args]);need(bool(re.fullmatch(footer,run['stdout'].splitlines()[-1])),'complete assertions '+label)
  if label=='voice':required=['mka::sound::HostVoice::start','mka::sound::HostVoice::stop_for_resource_retirement']
  elif label in ['forced-output','forced-failure']:required=['mka::sound::NativePlaybackOwner::force_retire','mka::sound::NativeProgramCopy::transfer_forced_outputs','mka::sound::HostVoice::stop_for_resource_retirement']
  elif label=='host-clock':required=['mka::sound::NativeProgramHostClock::read']
  elif label=='output-activation':required=['mka::sound::NativePlaybackOwner::activate','mka::sound::NativePlaybackOwner::update_ordinary_context','mka::sound::NativeProgramHostClock::read']
  elif label in ['default-runtime','playback','reservation','owner-gaps']:required=['mka::sound::NativePlaybackOwner::play','mka::sound::NativeProgramCopy::clone','mka::sound::AudioBankService::acquire_runtime_program']
  elif label in ['service-runtime','runtime-gaps']:required=['mka::sound::AudioBankService::acquire_runtime_program']
  elif label.startswith('copy'):required=['mka::sound::NativeProgramCopy::clone']
  elif label=='program':required=['mka::sound::sample_start_operands']
  else:required=['mka::sound::PreparedBank::acquire_runtime_program']
  audit=c.symbols(label+'-symbols',exe,run['executable_sha256'],required,True);fixtures.append(dict(component=label,target=target,executed_target_sha256=run['executable_sha256'],audit=audit))
 ninja=c.BUILD/'build.ninja';text=ninja.read_text();rules=[]
 for label,target,_,_ in cases:
  match=re.search(r'^build '+re.escape(target+'.exe')+r':.*(?:\n[ \t].*)*',text,re.M);need(match is not None and 'ps2runtime' not in match.group() and 'source_oracle' not in match.group(),'native actual target')
  wraps=re.findall(r'--wrap=([^\s"]+)',match.group());need(len(wraps)=={'forced-failure':1,'default-runtime':1,'runtime-gaps':1,'copy-failure':1,'gates':1,'exceptions':1,'owner-gaps':5}.get(label,0),'designated test-only wraps '+label)
  if label=='forced-failure':need(wraps==['_Znwy'],'allocator wrapper isolated in force failure target')
  rules.append(dict(target=target,actual_build_rule=match.group(),test_only_wraps=wraps))
 write('linkage-audit.json',dict(ninja_sha256=sha(ninja),target_rules=rules));need(before=={n:sha(n) for n in before},'source/tool/review/input stability')
 frozen=F/'research/runs/native-adx-composition-20261119/validation-02/audio-oracle.json';need(sha(frozen)==prior['frozen_decoder_certificate_sha256'],'frozen decoder preserved')
 for n,h in c.PRESERVE.items():need(sha(F/n)==h,'dirty IOP preserved after execution')
 write('verification.json',dict(status='PASS_NATIVE_FORCED_OUTPUT_RETIREMENT_AND_RETAINED_TAILS',source_after_sha256=before,unchanged_prior_pins=len(untouched),fixtures=fixtures,frozen_decoder_certificate_sha256=sha(frozen),limits=['Actual native normal resource cleanup/pool only; source notifications/worker/command12/alternate/mode1script/full game remain OPEN','Pure Copy-provenance PCM tails retain no semantic Service/runtime/cache acquisition; source hardware duration/heard fidelity not claimed','Same aggregate mixer across live/retired with outside-lock disposal, pending prep blocked not fabricated cancellation','No game/physical output/input/GPU/local model/driver/settings/user card/save/key operation','No unchanged decoder/full-bank/unrelated numeric corpus rerun']))
 print(json.dumps(dict(status='PASS_NATIVE_FORCED_OUTPUT_RETIREMENT_AND_RETAINED_TAILS',fixtures=len(fixtures),pins=len(before),untouched=len(untouched))),flush=True)
if __name__=='__main__':
 ctypes.windll.kernel32.SetErrorMode(3)
 if len(sys.argv)!=2 or not re.fullmatch(r'validation-[0-9]+',sys.argv[1]):raise SystemExit('usage: verify.py validation-NN')
 c.OUT=pathlib.Path(__file__).resolve().parent/sys.argv[1];c.OUT.mkdir(exist_ok=False);c.ENV=dict(os.environ,PATH=str(c.BIN)+os.pathsep+os.environ['PATH'])
 try:verify()
 except Exception as error:c.write('aborted.json',dict(status='NATIVE_FORCED_OUTPUT_CHECK_FAILED',error=str(error)));raise
