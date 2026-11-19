"""One scoped native integer fixture; never starts a game or device."""
import ctypes, datetime, hashlib, json, os, pathlib, re, subprocess, sys

FORK=pathlib.Path(__file__).resolve().parents[3]
PARENT=FORK.parents[2]
WORKSPACE=PARENT.parent
BUILD=FORK/'build_native_services_only_20261119-01'
BIN=WORKSPACE/'dependencies/compiler/llvm-mingw-20261006-ucrt-x86_64/bin'
CMAKE=WORKSPACE/'dependencies/cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe'
OUT=pathlib.Path(__file__).resolve().parent/'validation-01'
SOURCES=['runtime/include/mka/movie_clock_math.hpp','runtime/src/native/movie_clock_math.cpp',
         'tests/test_movie_clock_math.cpp','CMakeLists.txt','cmake/native_media.cmake']
PRESERVE={'runtime/src/iop/mcserv.cpp':'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',
          'runtime/src/iop/msl_native.cpp':'756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23'}
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def write(name,data):(OUT/name).write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8')
def run(name,args):
    tool=pathlib.Path(args[0]);before=sha(tool)
    r=subprocess.run(list(map(str,args)),cwd=FORK,env=ENV,capture_output=True,text=True,
                     creationflags=subprocess.CREATE_NO_WINDOW)
    data=dict(argv=list(map(str,args)),exit=r.returncode,stdout=r.stdout,stderr=r.stderr,
              executable_sha256=before,executable_after_sha256=sha(tool))
    write(name,data)
    if r.returncode or data['executable_after_sha256']!=before:raise RuntimeError(name+' failed')
    return data

if __name__=='__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    OUT.mkdir(parents=True,exist_ok=False)
    ENV=dict(os.environ,PATH=str(BIN)+os.pathsep+os.environ['PATH'])
    cache=(BUILD/'CMakeCache.txt').read_text(encoding='utf-8-sig')
    assert re.search(r'^PS2_GENERATED:PATH=$',cache,re.M)
    assert re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$',cache,re.M)
    source_before={s:sha(FORK/s) for s in SOURCES}
    for s,h in PRESERVE.items():assert sha(FORK/s)==h
    write('provenance.json',dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        fork_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=FORK,text=True).strip(),
        parent_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=PARENT,text=True).strip(),
        source_sha256=source_before,preserved_dirty_sha256=PRESERVE,runner_sha256=sha(__file__),
        cache_before_sha256=sha(BUILD/'CMakeCache.txt'),compiler_sha256=sha(BIN/'clang++.exe'),
        scope='Pure integer helpers only; no game, renderer, audio, model, driver or settings operation'))
    run('build.json',[CMAKE,'--build',BUILD,'--target','test_native_movie_clock_math','-j2'])
    exe=BUILD/'test_native_movie_clock_math.exe'
    execution=run('execution.json',[exe])
    target_before=sha(exe)
    symbols=run('symbols.json',[BIN/'llvm-nm.exe','-C','--defined-only',exe])
    forbidden=[line for line in symbols['stdout'].splitlines() if re.search(
        r'ps2::|\b(g_ram|g_spr)\b|\bEEContext\b|\bIOPContext\b|\bSPU2\b|\bCDVD\b',line)]
    symbols.update(target_before_sha256=target_before,target_after_sha256=sha(exe),
                   executed_target_sha256=execution['executable_sha256'],forbidden=forbidden)
    write('symbols.json',symbols)
    assert not forbidden
    assert symbols['target_before_sha256']==symbols['target_after_sha256']==execution['executable_sha256']
    source_after={s:sha(FORK/s) for s in SOURCES}
    assert source_after==source_before
    for s,h in PRESERVE.items():assert sha(FORK/s)==h
    verifier=PARENT/'tools/research/verify_movie_clock_math_trace.py'
    result=subprocess.run([sys.executable,str(verifier),str(OUT/'execution.json'),str(OUT/'complete-trace-oracle.json')],
        cwd=PARENT,env=ENV,capture_output=True,text=True,creationflags=subprocess.CREATE_NO_WINDOW)
    write('oracle-execution.json',dict(argv=[sys.executable,str(verifier),str(OUT/'execution.json'),str(OUT/'complete-trace-oracle.json')],
        exit=result.returncode,stdout=result.stdout,stderr=result.stderr,verifier_sha256=sha(verifier)))
    assert result.returncode==0
    oracle=json.loads((OUT/'complete-trace-oracle.json').read_text())
    write('verification.json',dict(status='PASS_NATIVE_CLOCK_MATH_COMPONENT',source_before_sha256=source_before,
        source_after_sha256=source_after,executable_sha256=execution['executable_sha256'],
        cache_after_sha256=sha(BUILD/'CMakeCache.txt'),oracle=oracle,forbidden=forbidden,
        preserved_dirty_sha256=PRESERVE,
        limits=['No full filter or clock selection/output mapping','No game build or gameplay acceptance']))
    print(json.dumps(dict(status='PASS_NATIVE_CLOCK_MATH_COMPONENT',trace_rows=oracle['trace_rows'],
        negative_checks=oracle['negative_checks'],executable_sha256=execution['executable_sha256'])))
