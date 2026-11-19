"""Native business math plus separate offline reference. Never starts a game."""
import ctypes,datetime,hashlib,json,os,pathlib,re,subprocess,sys
FORK=pathlib.Path(__file__).resolve().parents[3];PARENT=FORK.parents[2];WORKSPACE=PARENT.parent
BUILD=FORK/'build_native_services_only_20261119-01'
BIN=WORKSPACE/'dependencies/compiler/llvm-mingw-20261006-ucrt-x86_64/bin'
CMAKE=WORKSPACE/'dependencies/cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe'
BACKUP=FORK.parent/'ps2-native-recomp-backup-20261110'
OUT=pathlib.Path(__file__).resolve().parent/'validation-01'
NATIVE_SOURCES=['runtime/include/mka/movie_eligibility_numeric.hpp','runtime/src/native/movie_eligibility_numeric.cpp',
    'runtime/include/mka/movie_clock_filter.hpp','runtime/include/mka/movie_clock_history.hpp',
    'runtime/include/mka/movie_clock_math.hpp','runtime/include/mka/movie_timecode.hpp',
    'runtime/include/mka/mpeg2_timing.hpp','tests/test_movie_eligibility_numeric.cpp',
    'tests/generated/movie_eligibility_numeric_plan.inc','CMakeLists.txt','cmake/native_media.cmake']
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
    model=PARENT/'tools/research/movie_eligibility_numeric_reference.py'
    verifier=PARENT/'tools/research/verify_movie_eligibility_numeric_trace.py'
    reference=PARENT/'tools/research/movie_eligibility_source_oracle.cpp'
    header=BACKUP/'runtime/include/ps2/fpu.hpp'
    assert sha(model)=='63f1e58350febd5589d712639856603655e06b7342539bece30aabb5224496f3'
    assert sha(verifier)=='0a1d0f3f083a064adbce941b45c4cb1387b29a5fcacd6e3f2b17e67444f9f348'
    assert sha(reference)=='3241b7e61c89c6a56dc3127a843d1be2b82477c681afe388ef5209b78fdd57dc'
    assert sha(header)=='c318a2e548f8b8473719c4cde9b414980c75ca53ac9ec52781b72c1792099c5c'
    paths=[*[FORK/s for s in NATIVE_SOURCES],model,verifier,reference,header]
    sources={str(path):sha(path) for path in paths}
    for path,h in PRESERVE.items():assert sha(FORK/path)==h
    write('provenance.json',dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        fork_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=FORK,text=True).strip(),
        parent_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=PARENT,text=True).strip(),
        source_before_sha256=sources,preserved_dirty_sha256=PRESERVE,
        runner_path=str(pathlib.Path(__file__).resolve()),runner_sha256=sha(__file__),
        compiler_sha256=sha(BIN/'clang++.exe'),cache_before_sha256=sha(BUILD/'CMakeCache.txt'),
        scope='Native eligibility business math. Separate offline helper is reference-only, never runtime linkage or hardware/game proof'))
    run('build.json',[CMAKE,'--build',BUILD,'--target','test_native_movie_eligibility_numeric','-j2'])
    exe=BUILD/'test_native_movie_eligibility_numeric.exe'
    execution=run('execution.json',[exe]);target_before=sha(exe)
    symbols=run('symbols.json',[BIN/'llvm-nm.exe','-C','--defined-only',exe])
    forbidden=[x for x in symbols['stdout'].splitlines() if re.search(r'ps2::|\b(g_ram|g_spr)\b|\bEEContext\b|\bIOPContext\b|\bSPU2\b|\bCDVD\b',x)]
    symbols.update(target_before_sha256=target_before,target_after_sha256=sha(exe),
        executed_target_sha256=execution['executable_sha256'],forbidden=forbidden)
    write('symbols.json',symbols)
    assert not forbidden and symbols['target_before_sha256']==symbols['target_after_sha256']==execution['executable_sha256']
    oracle_exe=BUILD/'movie_eligibility_source_oracle.exe'
    run('reference-build.json',[BIN/'clang++.exe','-std=c++20','-O2','-I',BACKUP/'runtime/include',
        '-I',FORK/'tests/generated',reference,'-o',oracle_exe])
    source_execution=run('source-execution.json',[oracle_exe])
    assert source_execution['executable_sha256']!=execution['executable_sha256']
    run('oracle-execution.json',[pathlib.Path(sys.executable),'-O',verifier,OUT/'execution.json',
        OUT/'complete-trace-oracle.json','--source-receipt',OUT/'source-execution.json'])
    oracle=json.loads((OUT/'complete-trace-oracle.json').read_bytes())
    assert oracle['trace_rows']==57 and oracle['source32_rows']==32 and oracle['source_oracle']['trace_rows']==32
    assert oracle['executable_sha256']==execution['executable_sha256']
    assert oracle['source_receipt_sha256']==sha(OUT/'source-execution.json')
    assert sources=={str(path):sha(path) for path in paths}
    for path,h in PRESERVE.items():assert sha(FORK/path)==h
    write('verification.json',dict(status='PASS_NATIVE_ELIGIBILITY_NUMERIC_COMPONENT',source_after_sha256=sources,
        executable_sha256=execution['executable_sha256'],offline_source_executable_sha256=source_execution['executable_sha256'],
        oracle=oracle,forbidden=forbidden,cache_after_sha256=sha(BUILD/'CMakeCache.txt'),
        limits=['57 planned cases only; no exhaustive native64/HW/game or historical cache proof',
                'Offline source helper is separate and absent from native target',
                'No adjusted target association, adaptive owner, actual clocks, player or output proof']))
    print(json.dumps(dict(status='PASS_NATIVE_ELIGIBILITY_NUMERIC_COMPONENT',trace_rows=57,source_reference_rows=32,
        negative_checks=oracle['negative_checks'],executable_sha256=execution['executable_sha256'],
        offline_source_executable_sha256=source_execution['executable_sha256'])))
