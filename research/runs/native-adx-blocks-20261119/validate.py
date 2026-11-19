"""Native asset block codec only; independent integer model, no devices."""
import ctypes, datetime, importlib.util, json, os, pathlib, re, sys
FORK = pathlib.Path(__file__).resolve().parents[3]
PARENT = FORK.parents[2]
COMMON = FORK / 'research/runs/native-movie-kernel-policies-20261119/validate.py'
OUT = pathlib.Path(__file__).resolve().parent / 'validation-01'
spec = importlib.util.spec_from_file_location('_native_codec_receipts', COMMON)
common = importlib.util.module_from_spec(spec)
spec.loader.exec_module(common)
require, sha, write = common.require, common.sha, common.write
if __name__ == '__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    common.OUT = OUT
    common.ENV = dict(os.environ, PATH=str(common.BIN) + os.pathsep + os.environ['PATH'])
    OUT.mkdir(parents=True, exist_ok=False)
    cache_path = common.BUILD / 'CMakeCache.txt'
    cache = cache_path.read_text(encoding='utf-8-sig')
    require(bool(re.search(r'^PS2_GENERATED:PATH=$', cache, re.M)), 'no generated game')
    require(bool(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$', cache, re.M)), 'Release')
    source_run = PARENT / 'research/runs/native-movie-ingress-20261119'
    checker = PARENT / 'tools/research/verify_movie_adx_blocks_trace.py'
    paths = [*[FORK / n for n in common.NAMES],
        FORK / 'runtime/include/mka/movie_adx_blocks.hpp',
        FORK / 'runtime/src/native/movie_adx_blocks.cpp',
        FORK / 'tests/test_movie_adx_blocks.cpp',
        FORK / 'runtime/src/native/movie_decode.cpp',
        PARENT / 'research/inputs/modules/MOVIER.ovl', checker, COMMON,
        source_run / 'MOVIE_ADX_BLOCK_TRACE_PLAN.md', source_run / 'MOVIE_ADX_BLOCK_TRACE_PLAN.json',
        source_run / 'MOVIE_ADX_PREDICTOR_INPUT_SOURCE.json',
        source_run / 'adx-predictor-input-source.words.txt',
        source_run / 'MOVIE_ADX_HISTORY_MAPPING_CORRECTION.json', pathlib.Path(__file__).resolve()]
    before = {str(path): sha(path) for path in paths}
    require(sha(checker) == '604358d3fd6339ee2b5ca7e94f7a90c834bd9038d04d960f9ea2bebefb420bee', 'corrected source-bound checker')
    for path in paths:
        if path.suffix in ['.cpp', '.hpp', '.py', '.inc', '.cmake']:
            require(b'\r\r' not in path.read_bytes(), 'source has no doubled CR')
    for name, digest in common.PRESERVE.items():
        require(sha(FORK / name) == digest, 'preserved IOP source ' + name)
    write('provenance.json', dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        fork_head=common.subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=FORK, text=True).strip(),
        parent_head=common.subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=PARENT, text=True).strip(),
        source_before_sha256=before, preserved_dirty_sha256=common.PRESERVE,
        compiler_path=str(common.BIN / 'clang++.exe'), compiler_sha256=sha(common.BIN / 'clang++.exe'),
        cache_before_sha256=sha(cache_path),
        scope='Explicit native ADX block profile only; no header/coef/mode admission, no devices/reference helper'))
    common.execute('build.json', [common.CMAKE, '--build', common.BUILD, '--target',
        'test_native_movie_adx_blocks', 'mka_native_media', '-j2'])
    fixture = common.execute('adx-blocks.json', [common.BUILD / 'test_native_movie_adx_blocks.exe', '--dump-plan'])
    common.execute('oracle-execution.json', [sys.executable, '-O', checker,
        OUT / 'adx-blocks.json', OUT / 'oracle.json'])
    oracle = json.loads((OUT / 'oracle.json').read_bytes())
    require(oracle['status'] == 'PASS_COMPLETE_NATIVE_ADX_BLOCK76_PLAN' and
        oracle['cases'] == 76 and oracle['samples'] == 2432 and
        oracle['executable_sha256'] == fixture['executable_sha256'], 'actual complete typed trace oracle')
    executable_audit = common.symbols('adx-blocks-symbols', common.BUILD / 'test_native_movie_adx_blocks.exe',
        fixture['executable_sha256'], ['mka::video::decode_movie_adx_blocks('], True)
    archive = common.BUILD / 'libmka_native_media.a'
    archive_audit = common.symbols('archive-symbols', archive, sha(archive),
        ['mka::video::decode_movie_adx_blocks('], False)
    require(before == {str(path): sha(path) for path in paths}, 'source unchanged during verification')
    for name, digest in common.PRESERVE.items():
        require(sha(FORK / name) == digest, 'preserved IOP source after ' + name)
    write('verification.json', dict(status='PASS_SCOPED_EXPLICIT_NATIVE_ADX_BLOCK_CODEC',
        source_after_sha256=before, fixture_sha256=fixture['executable_sha256'], oracle=oracle,
        executed_target_audit=executable_audit, native_archive_sha256=sha(archive), archive_audit=archive_audit,
        cache_after_sha256=sha(cache_path),
        limits=['75 source32-safe synthetic cases and one explicit native-wide extension, not whole decoded retail assets',
            'Header/actual coefficient/end-marker trim/mode/speaker/clock/lifecycle bindings remain OPEN',
            'No backup/reference FPU helper linked into native codec or archive',
            'No game, real audio/GPU/context, audible samples, local model, drivers/settings or backup modification']))
    print(json.dumps(dict(status='PASS_SCOPED_EXPLICIT_NATIVE_ADX_BLOCK_CODEC',
        fixture_sha256=fixture['executable_sha256'], cases=oracle['cases'], samples=oracle['samples'],
        domain=oracle['domain'], negative_checks=oracle['negative_checks'], native_archive_sha256=sha(archive))))
