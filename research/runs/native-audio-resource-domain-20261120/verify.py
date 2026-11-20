"""Native sound resource integration; no movie decoder corpus or endpoints."""
import ctypes, datetime, importlib.util, json, os, pathlib, re, subprocess, sys
FORK=pathlib.Path(__file__).resolve().parents[3]
PARENT=FORK.parents[2]
RECORDS=PARENT/'research/runs/native-movie-ingress-20261119'
COMMON=FORK/'research/runs/native-movie-kernel-policies-20261119/validate.py'
spec=importlib.util.spec_from_file_location('_audio_resources_evidence',COMMON)
common=importlib.util.module_from_spec(spec);spec.loader.exec_module(common)
CHANGED=['CMakeLists.txt','runtime/include/mka/sound_archive.hpp','runtime/src/native/sound_archive.cpp',
'runtime/include/mka/stream_source.hpp','runtime/src/native/stream_source.cpp',
'runtime/include/mka/audio_banks.hpp','runtime/src/native/audio_banks.cpp',
'runtime/include/mka/bank_preparation.hpp','runtime/src/native/bank_preparation.cpp',
'runtime/include/mka/audio_bank_protocol.hpp','runtime/src/native/audio_bank_protocol.cpp',
'runtime/include/mka/audio_bank_service.hpp','runtime/src/native/audio_bank_service.cpp',
'runtime/include/mka/sample_payload.hpp']
NEW=['cmake/native_sound_resources.cmake','runtime/include/mka/audio_resource_domain.hpp','runtime/src/native/audio_resource_domain.cpp',
     'tests/test_audio_resource_domain.cpp','tests/test_audio_resource_exception.cpp']
def verify():
 require,sha,write=common.require,common.sha,common.write
 cache=common.BUILD/'CMakeCache.txt';cache_text=cache.read_text(encoding='utf-8-sig')
 require(bool(re.search(r'^PS2_GENERATED:PATH=$',cache_text,re.M)),'native services build, no game target')
 require(bool(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$',cache_text,re.M)),'Release')
 prior_path=FORK/'research/runs/native-geometry-attributes-20261120/validation-02/verification.json'
 prior=json.loads(prior_path.read_bytes())
 require(prior['status']=='PASS_NATIVE_GEOMETRY_ATTRIBUTES_QUEUED_INTEGRATION' and isinstance(prior['fixtures'],list),'actual prior schema')
 changed={str(FORK/name) for name in CHANGED}
 untouched={name:digest for name,digest in prior['source_after_sha256'].items() if name not in changed}
 require(all(sha(name)==digest for name,digest in untouched.items()),'untouched prior source and evidence')
 frozen=FORK/'research/runs/native-adx-composition-20261119/validation-02/audio-oracle.json'
 require(sha(frozen)==prior['frozen_decoder_certificate_sha256'],'frozen decoder evidence unchanged')
 ms2=FORK/'games/mkarmageddon/disc-files/SNDSPS2/MSLASSET.MS2'
 ms4=ms2.with_suffix('.MS4')
 paths={pathlib.Path(name) for name in prior['source_after_sha256']}
 paths.update(FORK/name for name in CHANGED+NEW)
 paths.update([pathlib.Path(__file__).resolve(),COMMON,prior_path,ms2,ms4,frozen,
  RECORDS/'NATIVE_SOUND_QUIESCENCE_EXECUTOR_SOURCE_20261120.md',
  RECORDS/'NATIVE_AUDIO_RESOURCE_BINDING_REVIEW_20261120.md',
  RECORDS/'NATIVE_AUDIO_RESOURCE_BINDING_REVIEW_20261120.json',
  RECORDS/'NATIVE_AUDIO_RESOURCE_IMPLEMENTATION_REVIEW_20261120.md',
  RECORDS/'NATIVE_AUDIO_RESOURCE_IMPLEMENTATION_REVIEW_20261120.json',
  RECORDS/'NATIVE_AUDIO_RESOURCE_PREBUILD_CORRECTIONS_20261120.md',
  RECORDS/'NATIVE_AUDIO_RESOURCE_FINAL_PEER_20261120.md',
  RECORDS/'NATIVE_AUDIO_RESOURCE_FINAL_PEER_20261120.json'])
 # Pin affected existing fixture source as well as implementation/header closure.
 fixture_sources=['test_sound_archive.cpp','test_bank_asset.cpp','test_audio_banks.cpp',
 'test_audio_bank_protocol.cpp','test_bank_preparation.cpp','test_audio_bank_service.cpp',
 'test_program_executor.cpp','test_host_voice.cpp','test_playback_owner.cpp','test_stream_source.cpp']
 paths.update(FORK/'tests'/name for name in fixture_sources)
 before={str(path):sha(path) for path in sorted(paths)}
 for name in CHANGED+NEW:require(b'\r' not in (FORK/name).read_bytes(),'LF authored source '+name)
 for name,digest in common.PRESERVE.items():require(sha(FORK/name)==digest,'preserved IOP '+name)
 write('provenance.json',dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
  fork_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=FORK,text=True).strip(),
  parent_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=PARENT,text=True).strip(),
  source_before_sha256=before,unchanged_prior_pins=len(untouched),
  compiler_sha256=sha(common.BIN/'clang++.exe'),cache_before_sha256=sha(cache),
  scope='Native program/sample/backing barriers and actual immutable PCM publication; no full retail surround or physical output'))
 cases=[
 ('exceptions','test_native_audio_resource_exception',[ms2],r'^PASS injected native preparation exceptions/whole-candidate cleanup/exact-token retries/failure completion; no production fault switch$'),
 ('domain','test_native_audio_resource_domain',[ms2,common.OUT/'domain-owned'],r'^PASS native audio resource barriers/shared backings/private PCM publication/unload; no game or physical audio$'),
 ('archive','test_sound_archive',[ms2],r'^sound archive tests passed$'),
 ('asset','test_bank_asset',[ms2],r'^bank asset tests passed$'),
 ('banks','test_native_audio_banks',[ms2,ms4],r'^PASS audio bank host ownership, exact real lookup and retained source bytes; no preparation/playback claim$'),
 ('protocol','test_native_audio_bank_protocol',[ms2],r'^PASS verified audio bank framing and explicit host token binding; no preparation/completion/playback$'),
 ('preparation','test_native_bank_preparation',[ms2,common.OUT/'preparation-owned'],r'^PASS [0-9]+ actualbanks [0-9]+eager [0-9]+deferred [0-9]+skipprecreation; nativepreparation only nocallback/playback$'),
 ('service','test_native_audio_bank_service',[ms2,common.OUT/'service-owned'],r'^PASS nativebank sync/async completion ordering/exacttokens/eagerfailure; no transport/game/playback$'),
 ('program','test_native_program_executor',[ms2],r'^PASS actualannouncrtimeline/ownedclip/exactoperands/startACK/stop/unsupportedcontrols; no mix/audio execution$'),
 ('voice','test_native_host_voice',[ms2],r'^PASS realannouncr freshpan/LUT/phase/explicitprofiles/signedDSP/hostresample/64voices/transactionalstop; no device/output/fidelity claim$'),
 ('playback','test_native_playback_owner',[ms2],r'^PASS actualservice/fulltoken/deferredannouncr/HostVoiceACK/kind7/ownerstop/64voices/mix/refusedactiveunload; no boot/device/fidelity claim$'),
 ('stream','test_native_stream_source',['synthetic'],r'^\[stream-synthetic-positive\] raw ownership/modes/extents/truncation/sparse/concurrency passed; temporary cleanup verified$')]
 common.execute('build.json',[common.CMAKE,'--build',common.BUILD,'--target',*[row[1] for row in cases],'-j2'])
 fixtures=[]
 for label,target,args,footer in cases:
  print('RUN '+label,flush=True)
  executable=common.BUILD/(target+'.exe');run=common.execute(label+'.json',[executable,*args])
  require(bool(re.fullmatch(footer,run['stdout'].splitlines()[-1])),'complete '+label+' assertions')
  audit=common.symbols(label+'-symbols',executable,run['executable_sha256'],['mka::sound::AudioResourceDomain::acquire'],True)
  fixtures.append(dict(component=label,target=target,executed_target_sha256=run['executable_sha256'],audit=audit))
 archive=common.BUILD/'libmka_native_sound_resources.a'
 common.symbols('native-library-symbols',archive,sha(archive),['mka::sound::AudioResourceDomain::surround_movie','mka::sound::PreparedBank::prepare'],False)
 ninja=common.BUILD/'build.ninja';text=ninja.read_text();rules=[]
 for _,target,_,_ in cases:
  match=re.search(r'^build '+re.escape(target+'.exe')+r':.*(?:\n[ \t].*)*',text,re.M)
  require(match is not None and 'ps2runtime' not in match.group() and 'source_oracle' not in match.group(),'native-only target link rule')
  rules.append(dict(target=target,actual_build_rule=match.group()))
 write('linkage-audit.json',dict(ninja_sha256=sha(ninja),candidate_rules=rules))
 require(before=={name:sha(name) for name in before},'all source/tool/asset/evidence pins stable')
 for name,digest in common.PRESERVE.items():require(sha(FORK/name)==digest,'preserved IOP after run')
 write('verification.json',dict(status='PASS_NATIVE_AUDIO_RESOURCE_DOMAIN_INTEGRATION',
  source_after_sha256=before,unchanged_prior_pins=len(untouched),fixtures=fixtures,
  frozen_decoder_certificate_sha256=sha(frozen),
  limits=['Real source-file and native resource/PCM ownership assertions, not physical audio or complete game surround',
   'Source deferred alternate/callback/upload producer and LOGON actual audio/graphics surround binding OPEN',
   'Final file close is serialized; checked native close-error propagation still OPEN',
   'No movie decoder/old numeric corpus rerun; no game/endpoint/local model/server/driver/backup/card/save action']))
 print(json.dumps(dict(status='PASS_NATIVE_AUDIO_RESOURCE_DOMAIN_INTEGRATION',fixtures=len(fixtures),pins=len(before),unchanged_prior_pins=len(untouched))),flush=True)
if __name__=='__main__':
 ctypes.windll.kernel32.SetErrorMode(3)
 if len(sys.argv)!=2 or not re.fullmatch(r'validation-[0-9]+',sys.argv[1]):raise SystemExit('usage: verify.py validation-NN')
 common.OUT=pathlib.Path(__file__).resolve().parent/sys.argv[1];common.OUT.mkdir(exist_ok=False)
 common.ENV=dict(os.environ,PATH=str(common.BIN)+os.pathsep+os.environ['PATH'])
 try:verify()
 except Exception as error:
  common.write('aborted.json',dict(status='NATIVE_AUDIO_RESOURCE_DOMAIN_CHECK_FAILED',error=str(error)));raise
