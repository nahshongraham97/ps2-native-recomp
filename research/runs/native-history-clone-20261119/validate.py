"""Native history and clone checks only; no game/device/model launch."""
import ctypes,datetime,hashlib,json,os,pathlib,re,subprocess,sys
FORK=pathlib.Path(__file__).resolve().parents[3];PARENT=FORK.parents[2];WORKSPACE=PARENT.parent
BUILD=FORK/'build_native_services_only_20261119-01'
BIN=WORKSPACE/'dependencies/compiler/llvm-mingw-20261006-ucrt-x86_64/bin'
CMAKE=WORKSPACE/'dependencies/cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe'
OUT=pathlib.Path(__file__).resolve().parent/'validation-01'
SOURCES=['runtime/include/mka/movie_clock_history.hpp','runtime/src/native/movie_clock_history.cpp',
    'tests/test_movie_clock_history.cpp','tests/test_movie_clock_history_clone.cpp','CMakeLists.txt','cmake/native_media.cmake']
PRESERVE={'runtime/src/iop/mcserv.cpp':'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',
    'runtime/src/iop/msl_native.cpp':'756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23'}
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def write(n,d):(OUT/n).write_text(json.dumps(d,indent=2)+'\n')
def run(n,args):
    args=list(map(str,args));before=sha(args[0])
    r=subprocess.run(args,cwd=FORK,env=ENV,capture_output=True,text=True,creationflags=subprocess.CREATE_NO_WINDOW)
    record=dict(argv=args,exit=r.returncode,stdout=r.stdout,stderr=r.stderr,
        executable_sha256=before,executable_after_sha256=sha(args[0]))
    write(n,record)
    assert r.returncode==0 and record['executable_after_sha256']==before,n
    return record
if __name__=='__main__':
    ctypes.windll.kernel32.SetErrorMode(3);OUT.mkdir(parents=True,exist_ok=False)
    ENV=dict(os.environ,PATH=str(BIN)+os.pathsep+os.environ['PATH'])
    cache=(BUILD/'CMakeCache.txt').read_text(encoding='utf-8-sig')
    assert re.search(r'^PS2_GENERATED:PATH=$',cache,re.M) and re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$',cache,re.M)
    sources={s:sha(FORK/s) for s in SOURCES}
    for s,h in PRESERVE.items():assert sha(FORK/s)==h
    write('provenance.json',dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        fork_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=FORK,text=True).strip(),
        parent_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=PARENT,text=True).strip(),
        source_before_sha256=sources,preserved_dirty_sha256=PRESERVE,runner_sha256=sha(__file__),
        compiler_sha256=sha(BIN/'clang++.exe'),cache_before_sha256=sha(BUILD/'CMakeCache.txt'),
        scope='Pure history/ownership fixtures; no game, devices, drivers or settings'))
    cases=[('history','test_native_movie_clock_history','verify_movie_clock_history_trace.py'),
           ('clone','test_native_movie_clock_history_clone','verify_movie_history_clone_trace.py')]
    run('build.json',[CMAKE,'--build',BUILD,'--target',*[c[1] for c in cases],'-j2'])
    results={}
    for name,target,checker in cases:
        exe=BUILD/(target+'.exe');execution=run(name+'-execution.json',[exe]);target_before=sha(exe)
        symbols=run(name+'-symbols.json',[BIN/'llvm-nm.exe','-C','--defined-only',exe])
        forbidden=[x for x in symbols['stdout'].splitlines() if re.search(r'ps2::|\b(g_ram|g_spr)\b|\bEEContext\b|\bIOPContext\b|\bSPU2\b|\bCDVD\b',x)]
        symbols.update(target_before_sha256=target_before,target_after_sha256=sha(exe),
            executed_target_sha256=execution['executable_sha256'],forbidden=forbidden)
        write(name+'-symbols.json',symbols)
        assert not forbidden and symbols['target_before_sha256']==symbols['target_after_sha256']==execution['executable_sha256']
        verifier=PARENT/'tools/research'/checker
        oracle_run=run(name+'-oracle-execution.json',[pathlib.Path(sys.executable),verifier,
            OUT/(name+'-execution.json'),OUT/(name+'-complete-trace-oracle.json')])
        oracle=json.loads((OUT/(name+'-complete-trace-oracle.json')).read_bytes())
        assert oracle['executable_sha256']==execution['executable_sha256']
        results[name]=dict(executable_sha256=execution['executable_sha256'],oracle=oracle,
            checker_sha256=sha(verifier),forbidden=forbidden)
    assert sources=={s:sha(FORK/s) for s in SOURCES}
    for s,h in PRESERVE.items():assert sha(FORK/s)==h
    write('verification.json',dict(status='PASS_NATIVE_HISTORY_CLONE_COMPONENT',cases=results,
        source_after_sha256=sources,cache_after_sha256=sha(BUILD/'CMakeCache.txt'),
        limits=['Owned history/staging only; full filter/player/game open','Allocation failure branches source-reviewed, not injected']))
    print(json.dumps({k:dict(rows=v['oracle']['trace_rows'],negative_checks=v['oracle']['negative_checks'],
        executable_sha256=v['executable_sha256']) for k,v in results.items()}))
