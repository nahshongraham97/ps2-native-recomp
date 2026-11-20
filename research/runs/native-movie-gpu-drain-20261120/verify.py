"""Native completion ownership; WGL/physical fixture compilation only."""
import ctypes,datetime,importlib.util,json,os,pathlib,re,subprocess,sys
FORK=pathlib.Path(__file__).resolve().parents[3];PARENT=FORK.parents[2]
RECORDS=PARENT/'research/runs/native-movie-ingress-20261119'
COMMON=FORK/'research/runs/native-movie-kernel-policies-20261119/validate.py'
spec=importlib.util.spec_from_file_location('_gpu_drain_evidence',COMMON);common=importlib.util.module_from_spec(spec);spec.loader.exec_module(common)
CHANGED=['runtime/include/mka/movie_gpu.hpp','runtime/include/mka/movie_gl_device.hpp',
'runtime/src/native/movie_gl_device.cpp','cmake/native_media.cmake',
 'tests/test_movie_gl_device.cpp','tests/test_native_movie_pipeline.cpp']
NEW=['runtime/include/mka/movie_gpu_completion.hpp','runtime/src/native/movie_gpu_completion.cpp','tests/test_movie_gpu_completion.cpp']
def verify():
 require,sha,write=common.require,common.sha,common.write
 cache=common.BUILD/'CMakeCache.txt';text=cache.read_text(encoding='utf-8-sig')
 require(bool(re.search(r'^PS2_GENERATED:PATH=$',text,re.M)) and bool(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$',text,re.M)),'native-only Release cache')
 prior_path=FORK/'research/runs/native-audio-resource-domain-20261120/validation-04/verification.json';prior=json.loads(prior_path.read_bytes())
 require(prior['status']=='PASS_NATIVE_AUDIO_RESOURCE_DOMAIN_INTEGRATION' and len(prior['fixtures'])==12,'actual prior schema')
 changed={str(FORK/name) for name in CHANGED}
 untouched={name:digest for name,digest in prior['source_after_sha256'].items() if name not in changed}
 require(all(sha(name)==digest for name,digest in untouched.items()),'unchanged prior source/evidence/tool/asset pins')
 review_path=RECORDS/'NATIVE_MOVIE_GPU_DRAIN_SOURCE_REVIEW_20261120.json';review=json.loads(review_path.read_bytes())
 require(review['status']=='SOURCE_REVIEW_NO_NEW_CONCRETE_DEFECT_AFTER_LISTED_CORRECTIONS' and isinstance(review['source_pins'],dict),'actual final source review schema')
 require(all(sha(PARENT/name)==digest for name,digest in review['source_pins'].items()),'current reviewed source hashes')
 require(all(sha(PARENT/name)==digest for name,digest in review['preserved_pre_consumer_pins'].items()),'exact preserved reviewed before bytes')
 paths={pathlib.Path(name) for name in prior['source_after_sha256']}
 paths.update(FORK/name for name in CHANGED+NEW)
 paths.update([pathlib.Path(__file__).resolve(),COMMON,prior_path,review_path,review_path.with_suffix('.md'),
 RECORDS/'NATIVE_MOVIE_RENDER_BOUNDARY_SOURCE_20261120.json',RECORDS/'NATIVE_MOVIE_RENDER_BOUNDARY_SOURCE_20261120.md'])
 before={str(path):sha(path) for path in sorted(paths)}
 for name in CHANGED+NEW:require(b'\r' not in (FORK/name).read_bytes(),'LF authored source '+name)
 for name,digest in common.PRESERVE.items():require(sha(FORK/name)==digest,'preserved dirty IOP')
 write('provenance.json',dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
 fork_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=FORK,text=True).strip(),
 parent_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=PARENT,text=True).strip(),
 source_before_sha256=before,unchanged_prior_pins=len(untouched),compiler_sha256=sha(common.BIN/'clang++.exe'),
 cache_before_sha256=sha(cache),scope='Native fence-port ownership execution plus WGL/physical fixture compilation ONLY; no physical endpoint'))
 targets=['test_native_movie_gpu_completion','mka_native_media','test_native_movie_gl_device','test_native_movie_pipeline']
 common.execute('build.json',[common.CMAKE,'--build',common.BUILD,'--target',*targets,'-j2'])
 executable=common.BUILD/'test_native_movie_gpu_completion.exe'
 # The only launched test is this exact CPU-only fence-port fixture.
 run=common.execute('completion.json',[executable])
 footer='PASS native GPU work/tail completion/sealed receipts/failed-source retention/host retirement; deterministic fence ports only'
 require(run['stdout'].splitlines()[-1]==footer,'complete ownership assertions')
 audit=common.symbols('completion-symbols',executable,run['executable_sha256'],
 ['mka::video::NativeMovieGpuWorkOwner::begin_drain','mka::video::NativeMovieGpuRetirementOwner::poll',
  'mka::video::MovieGpuRetirementRegistration::~MovieGpuRetirementRegistration'],True)
 library=common.BUILD/'libmka_native_gpu_completion.a'
 common.symbols('completion-library-symbols',library,sha(library),['mka::video::NativeMovieGpuWorkOwner::poll_drain'],False)
 media=common.BUILD/'libmka_native_media.a'
 common.symbols('wgl-library-symbols',media,sha(media),['mka::video::NativeMovieGlDevice::attach_current_context'],False)
 compiled=[]
 for label,target in [('physical-device','test_native_movie_gl_device'),('physical-pipeline','test_native_movie_pipeline')]:
  binary=common.BUILD/(target+'.exe')
  result=common.symbols(label+'-compile-only',binary,sha(binary),['mka::video::NativeMovieGpuWorkOwner::begin_drain'],False)
  require(result['execution_applicable'] is False,'physical executable NOT executed')
  compiled.append(dict(target=target,sha256=sha(binary),execution_applicable=False,audit=result))
 ninja=common.BUILD/'build.ninja';text=ninja.read_text();rules=[]
 for target in ['test_native_movie_gpu_completion.exe','libmka_native_gpu_completion.a','libmka_native_media.a',
  'test_native_movie_gl_device.exe','test_native_movie_pipeline.exe']:
  match=re.search(r'^build '+re.escape(target)+r':.*(?:\n[ \t].*)*',text,re.M)
  require(match is not None and 'ps2runtime' not in match.group() and 'source_oracle' not in match.group(),'native target linkage')
  rules.append(dict(target=target,actual_build_rule=match.group()))
 write('linkage-audit.json',dict(ninja_sha256=sha(ninja),target_rules=rules,
  executed_targets=['test_native_movie_gpu_completion.exe'],physical_targets_never_executed=compiled))
 require(before=={name:sha(name) for name in before},'stable current source/tools/evidence/assets')
 frozen=FORK/'research/runs/native-adx-composition-20261119/validation-02/audio-oracle.json'
 require(sha(frozen)==prior['frozen_decoder_certificate_sha256'],'frozen decoder unchanged')
 for name,digest in common.PRESERVE.items():require(sha(FORK/name)==digest,'preserved IOP after execution')
 write('verification.json',dict(status='PASS_NATIVE_MOVIE_GPU_COMPLETION_CPU_AND_WGL_BUILD',source_after_sha256=before,
 unchanged_prior_pins=len(untouched),fixtures=[dict(component='native-completion',executed_target_sha256=run['executable_sha256'],audit=audit)],
 compile_only_targets=compiled,frozen_decoder_certificate_sha256=sha(frozen),
 limits=['Deterministic native fence ports, not physical GPU timing/raster/context tests',
  'Actual WGL backend and physical fixture callers compiled/linked/symbol-audited but NEVER executed',
  'Device-local admission/tail proof does not establish whole-renderer admission or target/boundary draw',
  'Source current-sound context/cancellation, phased LOGON/audio/graphics owners/bootstrap/full-game acceptance OPEN',
  'No decoder/old numeric corpus rerun, game/physical endpoint/model/server/driver/backup/card/save action']))
 print(json.dumps(dict(status='PASS_NATIVE_MOVIE_GPU_COMPLETION_CPU_AND_WGL_BUILD',fixtures=1,compile_only=2,pins=len(before),unchanged_prior_pins=len(untouched))),flush=True)
if __name__=='__main__':
 ctypes.windll.kernel32.SetErrorMode(3)
 if len(sys.argv)!=2 or not re.fullmatch(r'validation-[0-9]+',sys.argv[1]):raise SystemExit('usage: verify.py validation-NN')
 common.OUT=pathlib.Path(__file__).resolve().parent/sys.argv[1];common.OUT.mkdir(exist_ok=False)
 common.ENV=dict(os.environ,PATH=str(common.BIN)+os.pathsep+os.environ['PATH'])
 try:verify()
 except Exception as error:
  common.write('aborted.json',dict(status='NATIVE_MOVIE_GPU_DRAIN_VERIFICATION_FAILED',error=str(error)));raise
