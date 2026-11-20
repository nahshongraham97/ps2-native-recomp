"""Changed native playback context/output-position tests only; no codec rerun."""
import ctypes,datetime,importlib.util,json,os,pathlib,re
FORK=pathlib.Path(__file__).resolve().parents[3]
PARENT=FORK.parents[2]
OUT=pathlib.Path(__file__).resolve().parent/'validation-context-01'
COMMON=FORK/'research/runs/native-movie-kernel-policies-20261119/validate.py'
spec=importlib.util.spec_from_file_location('_context_receipts',COMMON)
common=importlib.util.module_from_spec(spec);spec.loader.exec_module(common)
if __name__=='__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    common.OUT=OUT;common.ENV=dict(os.environ,PATH=str(common.BIN)+os.pathsep+os.environ['PATH'])
    require,sha,write=common.require,common.sha,common.write
    require('movie_service.cpp' not in (FORK/'cmake/native_media.cmake').read_text(),'next native application service is uncompiled')
    excluded={'movie_service.hpp','movie_service.cpp'}
    files=[x for x in (FORK/'runtime/include/mka').glob('*.hpp') if x.name not in excluded]
    files+=[x for x in (FORK/'runtime/src/native').rglob('*.cpp') if x.name not in excluded]
    files+=list((FORK/'runtime/src/native').rglob('*.inc'))
    files += [FORK/x for x in ['tests/test_movie_native_context.cpp','tests/test_movie_output_position.cpp','CMakeLists.txt','cmake/native_media.cmake','cmake/native_audio.cmake']]
    evidence=PARENT/'research/runs/native-movie-ingress-20261119'
    files += [evidence/x for x in ['NATIVE_PLAYBACK_CONTEXT_IMPLEMENTATION.json','NATIVE_PLAYBACK_CONTEXT_SOURCE_REVIEW.json','OUTPUT_POSITION_IMPLEMENTATION.json','OUTPUT_POSITION_ROOT_REVIEW.json']]
    files += [COMMON,pathlib.Path(__file__).resolve(),common.BIN/'clang++.exe']
    files=list(dict.fromkeys(files));before={str(x):sha(x) for x in files}
    for name,digest in common.PRESERVE.items():require(sha(FORK/name)==digest,'preserved IOP')
    def cache_contract():
        text=(common.BUILD/'CMakeCache.txt').read_text(encoding='utf-8-sig')
        require(re.search(r'^PS2_GENERATED:PATH=$',text,re.M) and re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$',text,re.M),'native-only Release')
    cache_contract();OUT.mkdir(exist_ok=False)
    write('provenance.json',dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),source_before_sha256=before,excluded_uncompiled=sorted(excluded),scope='Two new source-owned playback lifecycle/output tests; no decoder or device'))
    common.execute('build.json',[common.CMAKE,'--build',common.BUILD,'--target','test_native_movie_native_context','test_native_movie_output_position','-j2'])
    runs=[]
    for label,target,footer,symbol in [
        ('context','test_native_movie_native_context','PASS concrete source-owned movie context kernels/lifecycle; no default profile or native device/player admission','mka::video::NativeMoviePlaybackContext::'),
        ('position','test_native_movie_output_position','PASS native explicit-work output position ordering/identity/lifetime; no automatic decoder/admission binding','mka::video::NativeMovieOutputPosition::')]:
        print('RUN '+label,flush=True)
        result=common.execute(label+'.json',[common.BUILD/(target+'.exe')])
        require(result['stdout'].splitlines()[-1]==footer,'complete new playback assertions')
        audit=common.symbols(label+'-symbols',common.BUILD/(target+'.exe'),result['executable_sha256'],[symbol],True)
        runs.append(dict(component=label,executed_target_sha256=result['executable_sha256'],audit=audit))
    require(before=={name:sha(name) for name in before},'stable compiled playback context inputs');cache_contract()
    for name,digest in common.PRESERVE.items():require(sha(FORK/name)==digest,'preserved IOP after')
    write('verification.json',dict(status='PASS_FOCUSED_NATIVE_PLAYBACK_CONTEXT_POSITION',source_after_sha256=before,fixtures=runs,
        limits=['Concrete business context and explicit-work position owner only; native application producer binding remainsinprogress','No decoder source/corpus rerun, game, hardware, local model/server, driver/settings action']))
    print(json.dumps(dict(status='PASS_FOCUSED_NATIVE_PLAYBACK_CONTEXT_POSITION',fixtures=len(runs),pins=len(before))),flush=True)
