"""Pure equal-rate source retirement. No native device/game or timer."""
import ctypes,datetime,hashlib,json,os,pathlib,re,subprocess
FORK=pathlib.Path(__file__).resolve().parents[3];PARENT=FORK.parents[2];WORKSPACE=PARENT.parent
BUILD=FORK/'build_native_services_only_20261119-01'
BIN=WORKSPACE/'dependencies/compiler/llvm-mingw-20261006-ucrt-x86_64/bin'
CMAKE=WORKSPACE/'dependencies/cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe'
OUT=pathlib.Path(__file__).resolve().parent/'validation-01'
NAMES=['runtime/include/mka/movie_audio_progress.hpp','runtime/src/native/movie_audio_progress.cpp',
    'tests/test_movie_audio_progress.cpp','runtime/include/mka/movie_playback.hpp',
    'runtime/include/mka/audio_device.hpp','cmake/native_media.cmake','CMakeLists.txt']
PRESERVE={'runtime/src/iop/mcserv.cpp':'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',
    'runtime/src/iop/msl_native.cpp':'756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23'}
def req(ok,message):
    if not ok:raise RuntimeError(message)
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def write(name,x):
    with (OUT/name).open('x',encoding='utf-8',newline='\n') as f:json.dump(x,f,indent=2);f.write('\n')
def run(name,args):
    args=list(map(str,args));before=sha(args[0]);r=subprocess.run(args,cwd=FORK,env=ENV,
        capture_output=True,text=True,creationflags=subprocess.CREATE_NO_WINDOW)
    x=dict(argv=args,exit=r.returncode,stdout=r.stdout,stderr=r.stderr,
        executable_sha256=before,executable_after_sha256=sha(args[0]))
    write(name,x);req(r.returncode==0 and sha(args[0])==before,name+' failed');return x
if __name__=='__main__':
    ctypes.windll.kernel32.SetErrorMode(3);OUT.mkdir(parents=True,exist_ok=False)
    ENV=dict(os.environ,PATH=str(BIN)+os.pathsep+os.environ['PATH'])
    cache=(BUILD/'CMakeCache.txt').read_text(encoding='utf-8-sig')
    req(bool(re.search(r'^PS2_GENERATED:PATH=$',cache,re.M)),'no generated game')
    req(bool(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$',cache,re.M)),'Release')
    paths=[*[FORK/n for n in NAMES],pathlib.Path(__file__).resolve()];before={str(p):sha(p) for p in paths}
    for n,h in PRESERVE.items():req(sha(FORK/n)==h,'preserve '+n)
    write('provenance.json',dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        fork_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=FORK,text=True).strip(),
        parent_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=PARENT,text=True).strip(),
        source_before_sha256=before,preserved_dirty_sha256=PRESERVE,
        compiler_path=str(BIN/'clang++.exe'),compiler_sha256=sha(BIN/'clang++.exe'),
        cache_before_sha256=sha(BUILD/'CMakeCache.txt'),
        scope='Pure equal-rate, identity-bound client-buffer retirement; no native device or clock'))
    run('build.json',[CMAKE,'--build',BUILD,'--target','test_native_movie_audio_progress','-j2'])
    fixture=BUILD/'test_native_movie_audio_progress.exe';execution=run('execution.json',[fixture])
    lines=execution['stdout'].splitlines()
    req(len(lines)==2 and lines[1]=='PASS pure source retirement; no device/clock/player binding','complete scoped fixture')
    req(json.loads(lines[0])==dict(case='partial',submitted=7,padding=2,retired=5,source_boundary=105),'literal partial retirement')
    target_before=sha(fixture);symbols=run('symbols.json',[BIN/'llvm-nm.exe','-C','--defined-only',fixture])
    forbidden=[s for s in symbols['stdout'].splitlines() if re.search(r'ps2::|\b(g_ram|g_spr|EEContext|IOPContext|SPU2|CDVD)\b',s)]
    req(not forbidden and 'mka::video::NativeMovieAudioProgress::' in symbols['stdout'],'pure component symbols')
    req(target_before==sha(fixture)==execution['executable_sha256'],'executed and audited identity')
    write('symbol-target.json',dict(target=str(fixture),target_before_sha256=target_before,target_after_sha256=sha(fixture),
        executed_target_sha256=execution['executable_sha256'],execution_applicable=True,
        tool_sha256=symbols['executable_sha256'],forbidden=forbidden))
    req(before=={str(p):sha(p) for p in paths},'source changed during run')
    for n,h in PRESERVE.items():req(sha(FORK/n)==h,'preserve after '+n)
    write('verification.json',dict(status='PASS_PURE_EQUAL_RATE_SOURCE_RETIREMENT',source_after_sha256=before,
        fixture_sha256=execution['executable_sha256'],execution_receipt_sha256=sha(OUT/'execution.json'),
        forbidden=forbidden,cache_after_sha256=sha(BUILD/'CMakeCache.txt'),
        limits=['Scoped literal/lifetime/negative tests, not exhaustive native64 domain or allocation-fault injection',
                'Source identity/rate supplied by trusted same-owned-source coordinator, not authenticated from receipts',
                'No resampling, CRI transport timing parity, acoustic completion, native timeline/player or game proof']))
    print(json.dumps(dict(status='PASS_PURE_EQUAL_RATE_SOURCE_RETIREMENT',fixture_sha256=execution['executable_sha256'])))
