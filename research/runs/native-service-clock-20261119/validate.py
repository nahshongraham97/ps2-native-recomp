"""Atomic native service-clock scalars only. No game, clocks, devices or drivers."""
import ctypes,datetime,hashlib,json,os,pathlib,re,subprocess,sys
FORK=pathlib.Path(__file__).resolve().parents[3];PARENT=FORK.parents[2];WORKSPACE=PARENT.parent
BUILD=FORK/'build_native_services_only_20261119-01'
BIN=WORKSPACE/'dependencies/compiler/llvm-mingw-20261006-ucrt-x86_64/bin'
CMAKE=WORKSPACE/'dependencies/cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe'
OUT=pathlib.Path(__file__).resolve().parent/'validation-01'
SOURCES=['runtime/include/mka/movie_service_clock.hpp','runtime/src/native/movie_service_clock.cpp',
    'runtime/include/mka/movie_clock_filter.hpp','runtime/include/mka/movie_clock_history.hpp',
    'runtime/include/mka/movie_clock_math.hpp','tests/test_movie_service_clock.cpp',
    'tests/generated/movie_service_clock_plan.inc','CMakeLists.txt','cmake/native_media.cmake']
PRESERVE={'runtime/src/iop/mcserv.cpp':'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',
    'runtime/src/iop/msl_native.cpp':'756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23'}
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def write(n,d):(OUT/n).write_text(json.dumps(d,indent=2)+'\n')
def run(n,args):
    args=list(map(str,args));before=sha(args[0])
    result=subprocess.run(args,cwd=FORK,env=ENV,capture_output=True,text=True,creationflags=subprocess.CREATE_NO_WINDOW)
    record=dict(argv=args,exit=result.returncode,stdout=result.stdout,stderr=result.stderr,
        executable_sha256=before,executable_after_sha256=sha(args[0]))
    write(n,record)
    if result.returncode or record['executable_after_sha256']!=before:raise RuntimeError(n+' failed')
    return record
if __name__=='__main__':
    ctypes.windll.kernel32.SetErrorMode(3);OUT.mkdir(parents=True,exist_ok=False)
    ENV=dict(os.environ,PATH=str(BIN)+os.pathsep+os.environ['PATH'])
    cache=(BUILD/'CMakeCache.txt').read_text(encoding='utf-8-sig')
    assert re.search(r'^PS2_GENERATED:PATH=$',cache,re.M) and re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$',cache,re.M)
    sources={s:sha(FORK/s) for s in SOURCES}
    for s,h in PRESERVE.items():assert sha(FORK/s)==h
    model=PARENT/'tools/research/movie_service_clock_reference.py';verifier=PARENT/'tools/research/verify_movie_service_clock_trace.py'
    assert sha(model)=='2f369540f5d0e2f560cd694261190fd6be699275e84a8c304916b24bc7e77f54'
    assert sha(verifier)=='3ed056380d6210c2a5153f4b9b9bbed2e63afef61e91a93e8fba7962992f9f8e'
    write('provenance.json',dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        fork_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=FORK,text=True).strip(),
        parent_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=PARENT,text=True).strip(),
        source_before_sha256=sources,preserved_dirty_sha256=PRESERVE,runner_sha256=sha(__file__),
        compiler_sha256=sha(BIN/'clang++.exe'),cache_before_sha256=sha(BUILD/'CMakeCache.txt'),
        model_sha256=sha(model),verifier_sha256=sha(verifier),
        scope='Pure value-input native service clock; no clock queries, device, player, game, drivers or settings'))
    run('build.json',[CMAKE,'--build',BUILD,'--target','test_native_movie_service_clock','-j2'])
    exe=BUILD/'test_native_movie_service_clock.exe'
    execution=run('execution.json',[exe]);target_before=sha(exe)
    symbols=run('symbols.json',[BIN/'llvm-nm.exe','-C','--defined-only',exe])
    forbidden=[x for x in symbols['stdout'].splitlines() if re.search(r'ps2::|\b(g_ram|g_spr)\b|\bEEContext\b|\bIOPContext\b|\bSPU2\b|\bCDVD\b',x)]
    symbols.update(target_before_sha256=target_before,target_after_sha256=sha(exe),
        executed_target_sha256=execution['executable_sha256'],forbidden=forbidden)
    write('symbols.json',symbols)
    assert not forbidden and symbols['target_before_sha256']==symbols['target_after_sha256']==execution['executable_sha256']
    run('oracle-execution.json',[pathlib.Path(sys.executable),'-O',verifier,OUT/'execution.json',OUT/'complete-trace-oracle.json'])
    oracle=json.loads((OUT/'complete-trace-oracle.json').read_bytes())
    assert oracle['executable_sha256']==execution['executable_sha256'] and oracle['trace_rows']==287
    assert sha(model)==oracle['model_sha256'] and sha(verifier)==oracle['verifier_sha256']
    assert sources=={s:sha(FORK/s) for s in SOURCES}
    for s,h in PRESERVE.items():assert sha(FORK/s)==h
    write('verification.json',dict(status='PASS_NATIVE_MOVIE_SERVICE_CLOCK_COMPONENT',source_after_sha256=sources,
        executable_sha256=execution['executable_sha256'],oracle=oracle,forbidden=forbidden,
        cache_after_sha256=sha(BUILD/'CMakeCache.txt'),
        limits=['No real clock/master-time, diagnostics-buffer, scheduler, player/output or game binding',
                'Fixed scalar storage; overflow rollback and moved-owner/lifetime assertions tested, no cadence/lifecycle proof']))
    print(json.dumps(dict(status='PASS_NATIVE_MOVIE_SERVICE_CLOCK_COMPONENT',trace_rows=oracle['trace_rows'],cases=oracle['cases'],
        negative_checks=oracle['negative_checks'],executable_sha256=execution['executable_sha256'])))
