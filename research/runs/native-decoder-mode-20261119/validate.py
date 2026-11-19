"""Native content-lifetime decoder-mode state, CPU only; no runtime/image loader."""
import ctypes, datetime, importlib.util, json, os, pathlib, re

FORK = pathlib.Path(__file__).resolve().parents[3]
PARENT = FORK.parents[2]
OUT = pathlib.Path(__file__).resolve().parent / 'validation-01'
COMMON = FORK / 'research/runs/native-movie-kernel-policies-20261119/validate.py'
spec = importlib.util.spec_from_file_location('_native_mode_receipts', COMMON)
common = importlib.util.module_from_spec(spec); spec.loader.exec_module(common)

if __name__ == '__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    common.OUT = OUT
    common.ENV = dict(os.environ, PATH=str(common.BIN) + os.pathsep + os.environ['PATH'])
    OUT.mkdir(parents=True, exist_ok=False)
    require, sha, write = common.require, common.sha, common.write
    source = PARENT/'research/runs/native-movie-ingress-20261119'
    original = PARENT/'research/inputs/modules/MOVIER.ovl'
    image = original.read_bytes()
    require(sha(original) == '3a34e3838042aa0bde0ce687a4e61e128b2eb965e60e65c34713e0a73891adb8' and
        len(image) == 0x85900 and image[0x57520:0x57524] == bytes(4),
        'exact original complete source artifact and file-backed initializer')
    paths = [FORK/n for n in ['runtime/include/mka/movie_decoder_mode.hpp',
        'runtime/src/native/movie_decoder_mode.cpp', 'tests/test_movie_decoder_mode.cpp',
        'cmake/native_media.cmake', 'CMakeLists.txt']]
    paths += [original, COMMON, source/'MOVIE_DECODER_MODE_NATIVE_STATE_CONTRACT.json',
        source/'MOVIE_DECODER_MODE_NATIVE_IMPLEMENTATION.json',
        source/'MOVIE_DECODER_MODE_NATIVE_SOURCE_REVIEW.json', pathlib.Path(__file__).resolve()]
    before = {str(x): sha(x) for x in paths}
    for path in paths:
        if path.suffix in ['.cpp', '.hpp', '.py', '.cmake']:
            require(b'\r' not in path.read_bytes(), 'normalized authored source')
    for name, digest in common.PRESERVE.items():
        require(sha(FORK/name) == digest, 'preserved dirty IOP source')
    cache_path = common.BUILD/'CMakeCache.txt'
    cache = cache_path.read_text(encoding='utf-8-sig')
    require(bool(re.search(r'^PS2_GENERATED:PATH=$', cache, re.M)) and
        bool(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$', cache, re.M)), 'native-only Release configuration')
    write('provenance.json', dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        source_before_sha256=before, compiler_path=str(common.BIN/'clang++.exe'),
        compiler_sha256=sha(common.BIN/'clang++.exe'), cache_before_sha256=sha(cache_path),
        scope='Native mutable scalar/epoch/capture only; original file read by research runner, never runtime module loader'))
    common.execute('build.json', [common.CMAKE, '--build', common.BUILD,
        '--target', 'test_native_movie_decoder_mode', '-j2'])
    exe = common.BUILD/'test_native_movie_decoder_mode.exe'
    result = common.execute('mode.json', [exe])
    require(result['stdout'].strip() == 'PASS native mutable decoder mode content epoch/capture; caller and processing chronology remain unbound',
        'complete literal/ownership/allocation/thread fixture')
    audit = common.symbols('mode-symbols', exe, result['executable_sha256'],
        ['mka::video::NativeMovieDecoderMode::'], True)
    require(before == {str(x): sha(x) for x in paths}, 'mode source/evidence stable through verification')
    for name, digest in common.PRESERVE.items():
        require(sha(FORK/name) == digest, 'preserved IOP after verification')
    write('verification.json', dict(status='PASS_SCOPED_NATIVE_MUTABLE_MOVIE_DECODER_MODE_EPOCH',
        source_after_sha256=before, executed_target_sha256=result['executable_sha256'], audit=audit,
        cache_after_sha256=sha(cache_path),
        limits=['Explicit native activation is a caller precondition, not authenticated normal-game ingress',
            'No processing-batch/cache chronology, speaker/sink/clock/player/game admission',
            'Generation overflow guard source reviewed, not dynamically exercised',
            'No game/device/context/audible sample/model/server/driver/settings action']))
    print(json.dumps(dict(status='PASS_SCOPED_NATIVE_MUTABLE_MOVIE_DECODER_MODE_EPOCH',
        executed_target_sha256=result['executable_sha256'])), flush=True)
