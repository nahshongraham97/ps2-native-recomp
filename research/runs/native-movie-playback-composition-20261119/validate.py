"""Isolated deterministic playback composition. Never launches game/devices."""
import ctypes,datetime,hashlib,json,os,pathlib,re,subprocess
FORK=pathlib.Path(__file__).resolve().parents[3]
PARENT=FORK.parents[2];WORKSPACE=PARENT.parent
BUILD=FORK/'build_native_services_only_20261119-01'
BIN=WORKSPACE/'dependencies/compiler/llvm-mingw-20261006-ucrt-x86_64/bin'
CMAKE=WORKSPACE/'dependencies/cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe'
OUT=pathlib.Path(__file__).resolve().parent/'validation-01'
SOURCE_NAMES=[
    'runtime/include/mka/movie_playback.hpp','runtime/src/native/movie_playback.cpp',
    'runtime/src/native/movie_playback_adapters.cpp','runtime/include/mka/audio_device.hpp',
    'runtime/src/native/audio_device.cpp','tests/test_movie_playback.cpp',
    'tests/test_native_audio_device.cpp','runtime/include/mka/movie_control.hpp',
    'runtime/src/native/movie_control.cpp','runtime/src/native/movie_gain.cpp',
    'runtime/src/native/movie_color.cpp','runtime/src/native/movie_gpu.cpp',
    'runtime/src/native/movie_catalog.cpp','cmake/native_media.cmake','cmake/native_audio.cmake',
    'CMakeLists.txt']
PRESERVE={'runtime/src/iop/mcserv.cpp':'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',
    'runtime/src/iop/msl_native.cpp':'756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23'}
def require(ok,message):
    if not ok:raise RuntimeError(message)
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def write(name,value):
    with (OUT/name).open('x',encoding='utf-8',newline='\n') as f:json.dump(value,f,indent=2);f.write('\n')
def run(name,args):
    args=list(map(str,args));before=sha(args[0])
    r=subprocess.run(args,cwd=FORK,env=ENV,capture_output=True,text=True,
        creationflags=subprocess.CREATE_NO_WINDOW)
    d=dict(argv=args,exit=r.returncode,stdout=r.stdout,stderr=r.stderr,
        executable_sha256=before,executable_after_sha256=sha(args[0]))
    write(name,d)
    require(r.returncode==0 and before==d['executable_after_sha256'],name+' failed')
    return d
def symbol_check(name,target,expected,required,executed=False):
    before=sha(target)
    r=run(name,[BIN/'llvm-nm.exe','-C','--defined-only',target])
    forbidden=[s for s in r['stdout'].splitlines() if re.search(
        r'ps2::|\b(g_ram|g_spr|EEContext|IOPContext|SPU2|CDVD)\b',s)]
    result=dict(target=str(target),target_before_sha256=before,target_after_sha256=sha(target),
        audited_target_expected_sha256=expected,execution_applicable=executed,
        tool_sha256=r['executable_sha256'],forbidden=forbidden)
    if executed:result['executed_target_sha256']=expected
    require(not forbidden and result['target_after_sha256']==before==expected,'symbol target identity')
    require(all(s in r['stdout'] for s in required),'expected compiled component symbols')
    result['required_defined_symbols']=required
    write(name.replace('.json','-target.json'),result)
    return result
if __name__=='__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    OUT.mkdir(parents=True,exist_ok=False)
    ENV=dict(os.environ,PATH=str(BIN)+os.pathsep+os.environ['PATH'])
    cache=(BUILD/'CMakeCache.txt').read_text(encoding='utf-8-sig')
    require(bool(re.search(r'^PS2_GENERATED:PATH=$',cache,re.M)),'no generated game')
    require(bool(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$',cache,re.M)),'Release build')
    overlay=PARENT/'research/inputs/modules/MOVIER.ovl'
    require(sha(overlay)=='3a34e3838042aa0bde0ce687a4e61e128b2eb965e60e65c34713e0a73891adb8','original data identity')
    paths=[*[FORK/n for n in SOURCE_NAMES],overlay,pathlib.Path(__file__).resolve()]
    source_before={str(p):sha(p) for p in paths}
    for name,h in PRESERVE.items():require(sha(FORK/name)==h,'preserve '+name)
    write('provenance.json',dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        fork_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=FORK,text=True).strip(),
        parent_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=PARENT,text=True).strip(),
        source_before_sha256=source_before,preserved_dirty_sha256=PRESERVE,
        compiler_path=str(BIN/'clang++.exe'),compiler_sha256=sha(BIN/'clang++.exe'),
        cache_before_sha256=sha(BUILD/'CMakeCache.txt'),
        scope='Fake endpoint ownership/lifecycle; compile real native adapters. No player policy/game/device activation'))
    run('build.json',[CMAKE,'--build',BUILD,'--target','test_native_movie_playback',
        'mka_native_media','test_native_audio_device','test_native_movie_control','-j2'])
    fixture=BUILD/'test_native_movie_playback.exe'
    execution=run('execution.json',[fixture,overlay])
    require(execution['stdout'].strip()=='PASS deterministic playback composition fakes/controller/owned queues; no device or retail policy binding','exact scoped fixture completion')
    audio=run('audio-pure.json',[BUILD/'test_native_audio_device.exe'])
    control=run('control.json',[BUILD/'test_native_movie_control.exe'])
    symbol=symbol_check('symbols.json',fixture,execution['executable_sha256'],
        ['mka::video::NativeMovieStreamPlayback::','mka::video::NativeMovieController::'],executed=True)
    archive=BUILD/'libmka_native_media.a'
    adapters=symbol_check('adapter-symbols.json',archive,sha(archive),
        ['mka::video::own_movie_playback_source(', 'mka::video::own_movie_playback_audio('])
    require(source_before=={str(p):sha(p) for p in paths},'source changed during run')
    for name,h in PRESERVE.items():require(sha(FORK/name)==h,'preserve after '+name)
    write('verification.json',dict(status='PASS_ISOLATED_NATIVE_MOVIE_STREAM_COMPOSITION',
        source_after_sha256=source_before,fixture_sha256=execution['executable_sha256'],
        fixture_receipt_sha256=sha(OUT/'execution.json'),
        audio_pure_executable_sha256=audio['executable_sha256'],
        control_executable_sha256=control['executable_sha256'],native_archive_sha256=sha(archive),
        symbol_audits=[symbol,adapters],cache_after_sha256=sha(BUILD/'CMakeCache.txt'),
        limits=['Deterministic fake endpoints only; no actual output or retail timing/audio policy binding',
                'No WASAPI activation, sound, GPU/context, codec playback, window, game or driver/settings operations',
                'Native adapters compiled, not executed; stream_format live-owner getter execution remains open']))
    print(json.dumps(dict(status='PASS_ISOLATED_NATIVE_MOVIE_STREAM_COMPOSITION',
        fixture_sha256=execution['executable_sha256'],native_archive_sha256=sha(archive))))
