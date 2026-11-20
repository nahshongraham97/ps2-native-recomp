"""Actual native Service program ownership, CPU/files only; frozen movie codec."""
import ctypes,datetime,importlib.util,json,os,pathlib,re,subprocess,sys
F=pathlib.Path(__file__).resolve().parents[3];P=F.parents[2]
R=P/'research/runs/native-movie-ingress-20261119'
COMMON=F/'research/runs/native-movie-kernel-policies-20261119/validate.py'
spec=importlib.util.spec_from_file_location('_program_admission_evidence',COMMON)
c=importlib.util.module_from_spec(spec);spec.loader.exec_module(c)
CHANGED=['runtime/include/mka/audio_banks.hpp','runtime/src/native/audio_banks.cpp',
 'runtime/include/mka/audio_bank_protocol.hpp','runtime/src/native/audio_bank_protocol.cpp',
 'runtime/include/mka/audio_bank_service.hpp','runtime/src/native/audio_bank_service.cpp','cmake/native_sound_resources.cmake']
NEW=['tests/test_program_admission.cpp','tests/test_program_admission_gates.cpp']
def verify():
 need,sha,write=c.require,c.sha,c.write
 cache=(c.BUILD/'CMakeCache.txt').read_text(encoding='utf-8-sig')
 need(bool(re.search(r'^PS2_GENERATED:PATH=$',cache,re.M)) and bool(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$',cache,re.M)),'native Release no generated game')
 prior_path=F/'research/runs/native-deferred-program-work-20261120/validation-01/verification.json'
 prior=json.loads(prior_path.read_bytes())
 need(prior['status']=='PASS_NATIVE_DEFERRED_PROGRAM_WORK_AND_CORRECTED_SAMPLE_CACHE' and len(prior['fixtures'])==12 and len(prior['source_after_sha256'])==361,'prior actual schema')
 changed={str(F/n) for n in CHANGED}
 untouched={n:h for n,h in prior['source_after_sha256'].items() if n not in changed}
 need(all(sha(n)==h for n,h in untouched.items()),'untouched prior inputs')
 review_path=R/'NATIVE_PROGRAM_ADMISSION_SOURCE_REVIEW_20261120.json'
 review=json.loads(review_path.read_bytes())
 need(review['status']=='SOURCE_REVIEW_OWNED_PROGRAM_ADMISSION_NO_KNOWN_BLOCKER' and isinstance(review['input_sha256'],dict),'actual source review schema')
 need(set(review['input_sha256'])=={str(F/n) for n in CHANGED+NEW},'review actual source and fixture set')
 need(all(sha(n)==h for n,h in review['input_sha256'].items()),'source review current')
 paths={pathlib.Path(n) for n in prior['source_after_sha256']}
 paths.update(F/n for n in CHANGED+NEW)
 paths.update([pathlib.Path(__file__).resolve(),COMMON,prior_path,review_path,review_path.with_suffix('.md'),R/'NATIVE_PROGRAM_ADMISSION_PLAN_20261120.md'])
 before={str(n):sha(n) for n in sorted(paths)}
 for n in CHANGED+NEW:need(b'\r' not in (F/n).read_bytes(),'authored LF '+n)
 for n,h in c.PRESERVE.items():need(sha(F/n)==h,'preserved dirty IOP')
 write('provenance.json',dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
  fork_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=F,text=True).strip(),parent_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=P,text=True).strip(),
  source_before_sha256=before,unchanged_prior_pins=len(untouched),compiler_sha256=sha(c.BIN/'clang++.exe'),scope='Owned actual Service program admission, no device or game'))
 ms2=F/'games/mkarmageddon/disc-files/SNDSPS2/MSLASSET.MS2';ms4=ms2.with_suffix('.MS4')
 need(str(ms2) in before and str(ms4) in before,'pinned actual source assets')
 cases=[
  ('admission','test_native_program_admission',[ms2],r'PASS_NATIVE_PROGRAM_ADMISSION: 7 actual Service admission lifecycle cases; no scheduler/callback/context/hardware fidelity claim'),
  ('gates','test_native_program_admission_gates',[ms2],r'PASS native Service program admission unload gates/known refusal/unknown mutation; five CPU-only injected boundaries'),
  ('banks','test_native_audio_banks',[ms2,ms4],r'PASS audio bank host ownership, exact real lookup and retained source bytes; no preparation/playback claim'),
  ('protocol','test_native_audio_bank_protocol',[ms2],r'PASS verified audio bank framing and explicit host token binding; no preparation/completion/playback'),
  ('service','test_native_audio_bank_service',[ms2,c.OUT/'service-owned'],r'PASS nativebank sync/async completion ordering/exacttokens/eagerfailure; no transport/game/playback'),
  ('domain','test_native_audio_resource_domain',[ms2,c.OUT/'domain-owned'],r'PASS native audio resource barriers/shared backings/private PCM publication/unload; no game or physical audio'),
  ('exceptions','test_native_audio_resource_exception',[ms2],r'PASS injected native preparation exceptions/whole-candidate cleanup/exact-token retries/failure completion; no production fault switch'),
  ('playback','test_native_playback_owner',[ms2],r'PASS actualservice/fulltoken/deferredannouncr/HostVoiceACK/kind7/ownerstop/64voices/mix/refusedactiveunload; no boot/device/fidelity claim'),
  ('reservation','test_native_playback_reservation',[ms2],r'PASS actual ANNOUNCR pending play reservation/context identity/registry growth/nonzero concurrent voice/resource domain; no device/game/PCM fidelity claim'),
  ('owner-gaps','test_native_playback_reservation_gaps',[ms2],r'PASS native playback reservation pre/post-service gaps/64 slot growth/unpublished rollback/exact-bank and close uncertainty; injected CPU fixture only')]
 c.execute('build.json',[c.CMAKE,'--build',c.BUILD,'--target',*[t for _,t,_,_ in cases],'-j2'])
 fixtures=[]
 for label,target,args,footer in cases:
  print('RUN '+label,flush=True)
  exe=c.BUILD/(target+'.exe');run=c.execute(label+'.json',[exe,*args])
  need(bool(re.fullmatch(footer,run['stdout'].splitlines()[-1])),'complete fixture assertions '+label)
  audit=c.symbols(label+'-symbols',exe,run['executable_sha256'],['mka::sound::AudioBankService::admit_deferred_program'],True)
  fixtures.append(dict(component=label,target=target,executed_target_sha256=run['executable_sha256'],audit=audit))
 ninja=c.BUILD/'build.ninja';rules=[];text=ninja.read_text()
 for label,target,_,_ in cases:
  match=re.search(r'^build '+re.escape(target+'.exe')+r':.*(?:\n[ \t].*)*',text,re.M)
  need(match is not None and 'ps2runtime' not in match.group() and 'source_oracle' not in match.group(),'native-only target '+target)
  wraps=re.findall(r'--wrap=([^\s"]+)',match.group())
  need(len(wraps)=={'gates':1,'exceptions':1,'owner-gaps':5}.get(label,0),'designated test-only wraps '+label)
  rules.append(dict(target=target,actual_build_rule=match.group(),test_only_wraps=wraps))
 write('linkage-audit.json',dict(ninja_sha256=sha(ninja),target_rules=rules))
 need(before=={n:sha(n) for n in before},'source/asset/tool/review stable')
 frozen=F/'research/runs/native-adx-composition-20261119/validation-02/audio-oracle.json'
 need(sha(frozen)==prior['frozen_decoder_certificate_sha256'],'decoder frozen')
 for n,h in c.PRESERVE.items():need(sha(F/n)==h,'preserved dirty IOP after execution')
 write('verification.json',dict(status='PASS_NATIVE_SERVICE_OWNED_PROGRAM_ADMISSION',source_after_sha256=before,
  unchanged_prior_pins=len(untouched),fixtures=fixtures,frozen_decoder_certificate_sha256=sha(frozen),
  limits=['Actual owned fresh data admission, not source scheduler/callback/runtime descriptor/command12 or full game',
  'Same-bank overlap is a named native busy policy, not verified source unload wait/callback equivalence',
  'Unexpected unload mutation is quarantined, explicit whole Session close remains possible',
  'No hardware endpoints, game, local model/server, driver, save/key/card action; decoder and prior whole-bank corpus not rerun']))
 print(json.dumps(dict(status='PASS_NATIVE_SERVICE_OWNED_PROGRAM_ADMISSION',fixtures=len(fixtures),pins=len(before),untouched=len(untouched))),flush=True)
if __name__=='__main__':
 ctypes.windll.kernel32.SetErrorMode(3)
 if len(sys.argv)!=2 or not re.fullmatch(r'validation-[0-9]+',sys.argv[1]):raise SystemExit('usage: verify.py validation-NN')
 c.OUT=pathlib.Path(__file__).resolve().parent/sys.argv[1];c.OUT.mkdir(exist_ok=False)
 c.ENV=dict(os.environ,PATH=str(c.BIN)+os.pathsep+os.environ['PATH'])
 try:verify()
 except Exception as error:c.write('aborted.json',dict(status='NATIVE_PROGRAM_ADMISSION_CHECK_FAILED',error=str(error)));raise
