"""Focused new playback integration checks; frozen decoder is not rerun."""
import ctypes,datetime,importlib.util,json,os,pathlib,re
FORK=pathlib.Path(__file__).resolve().parents[3]
PARENT=FORK.parents[2]
OUT=pathlib.Path(__file__).resolve().parent/'validation-01'
COMMON=FORK/'research/runs/native-movie-kernel-policies-20261119/validate.py'
spec=importlib.util.spec_from_file_location('_playback_receipts',COMMON)
common=importlib.util.module_from_spec(spec);spec.loader.exec_module(common)
if __name__=='__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    common.OUT=OUT;common.ENV=dict(os.environ,PATH=str(common.BIN)+os.pathsep+os.environ['PATH'])
    require,sha,write=common.require,common.sha,common.write
    cmake=FORK/'cmake/native_media.cmake'
    require('movie_native_context' not in cmake.read_text(),'concurrent next context is not a compiled input')
    excluded={'movie_native_context.hpp','movie_native_context.cpp'}
    files=[x for x in (FORK/'runtime/include/mka').glob('*.hpp') if x.name not in excluded]
    files+=[x for x in (FORK/'runtime/src/native').rglob('*.cpp') if x.name not in excluded]
    files+=list((FORK/'runtime/src/native').rglob('*.inc'))
    files += [FORK/x for x in ['tests/test_movie_kernel_policies.cpp','tests/test_movie_playback_session.cpp',
        'CMakeLists.txt','cmake/native_media.cmake','cmake/native_audio.cmake']]
    files += [COMMON,pathlib.Path(__file__).resolve(),common.BIN/'clang++.exe']
    evidence=PARENT/'research/runs/native-movie-ingress-20261119'
    files += [evidence/x for x in ['PLAYBACK_POLICY_RATE_IMPLEMENTATION.json','PLAYBACK_POLICY_RATE_SOURCE_REVIEW.json',
        'PLAYBACK_SESSION_IMPLEMENTATION.json','PLAYBACK_SESSION_ROOT_REVIEW.json']]
    original=PARENT/'research/inputs/modules/MOVIER.ovl'
    require(sha(original)=='3a34e3838042aa0bde0ce687a4e61e128b2eb965e60e65c34713e0a73891adb8','original color source')
    files.append(original);files=list(dict.fromkeys(files));before={str(x):sha(x) for x in files}
    for name,digest in common.PRESERVE.items():require(sha(FORK/name)==digest,'preserved IOP')
    def cache_contract():
        text=(common.BUILD/'CMakeCache.txt').read_text(encoding='utf-8-sig')
        require(re.search(r'^PS2_GENERATED:PATH=$',text,re.M) and re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$',text,re.M),'native-only Release')
    cache_contract();OUT.mkdir(exist_ok=False)
    write('provenance.json',dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),source_before_sha256=before,
        scope='Only changed playback session/kernel integration; deterministic ports; no decoder execution or hardware',excluded_uncompiled=sorted(excluded)))
    common.execute('build.json',[common.CMAKE,'--build',common.BUILD,'--target','test_native_movie_kernel_policies','test_native_movie_playback_session','-j2'])
    runs=[]
    for label,target,footer,symbols in [
        ('kernel','test_native_movie_kernel_policies','PASS actual kernel policies with explicit fixture inputs; no retail profile/timeline admission',['mka::video::NativeMovieKernelPolicies::']),
        ('session','test_native_movie_playback_session','PASS native playback session real stream/controller lifecycle with deterministic ports; no devices or retail policy admission',['mka::video::NativeMoviePlaybackSession::'])]:
        print('RUN '+label,flush=True)
        result=common.execute(label+'.json',[common.BUILD/(target+'.exe'),original])
        require(result['stdout'].splitlines()[-1]==footer,'complete playback integration assertions')
        audit=common.symbols(label+'-symbols',common.BUILD/(target+'.exe'),result['executable_sha256'],symbols,True)
        runs.append(dict(component=label,executed_target_sha256=result['executable_sha256'],audit=audit))
    require(before=={name:sha(name) for name in before},'stable compiled playback inputs');cache_contract()
    for name,digest in common.PRESERVE.items():require(sha(FORK/name)==digest,'preserved IOP after')
    write('verification.json',dict(status='PASS_FOCUSED_NATIVE_PLAYBACK_SESSION_RATE_BINDING',source_after_sha256=before,fixtures=runs,
        limits=['Focused new native session/kernel integration only; deterministic endpoints/source policies',
        'Offline decoder checkpoint remains frozen; no decoder corpus rerun',
        'Concrete context/native application producer integration remains in progress; no playback/game acceptance']))
    print(json.dumps(dict(status='PASS_FOCUSED_NATIVE_PLAYBACK_SESSION_RATE_BINDING',fixtures=len(runs),pins=len(before))),flush=True)
