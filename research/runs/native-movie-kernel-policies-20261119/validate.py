"""Concrete native PCM/ordinary-policy composition; fake endpoints only."""
import ctypes, datetime, hashlib, json, os, pathlib, re, subprocess

FORK = pathlib.Path(__file__).resolve().parents[3]
PARENT = FORK.parents[2]
WORKSPACE = PARENT.parent
BUILD = FORK / 'build_native_services_only_20261119-01'
BIN = WORKSPACE / 'dependencies/compiler/llvm-mingw-20261006-ucrt-x86_64/bin'
CMAKE = WORKSPACE / 'dependencies/cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe'
OUT = pathlib.Path(__file__).resolve().parent / 'validation-01'
NAMES = [
    'runtime/include/mka/movie_pcm_binding.hpp', 'runtime/src/native/movie_pcm_binding.cpp',
    'runtime/include/mka/movie_kernel_policies.hpp', 'runtime/src/native/movie_kernel_policies.cpp',
    'tests/test_movie_pcm_binding.cpp', 'tests/test_movie_kernel_policies.cpp',
    'runtime/include/mka/movie_playback.hpp', 'runtime/src/native/movie_playback.cpp',
    'tests/test_movie_playback.cpp', 'runtime/include/mka/movie_pcm_mix.hpp',
    'runtime/src/native/movie_pcm_mix.cpp', 'runtime/src/native/generated/movie_pcm_mix_tables.inc',
    'runtime/include/mka/movie_frame_eligibility.hpp', 'runtime/src/native/movie_frame_eligibility.cpp',
    'runtime/include/mka/movie_eligibility_numeric.hpp', 'runtime/src/native/movie_eligibility_numeric.cpp',
    'runtime/include/mka/movie_clock_filter.hpp', 'runtime/include/mka/movie_timecode.hpp',
    'runtime/include/mka/movie_decode.hpp', 'runtime/include/mka/movie_pes_index.hpp',
    'runtime/include/mka/movie_source.hpp', 'runtime/include/mka/movie_catalog.hpp',
    'runtime/include/mka/movie_gpu.hpp', 'runtime/include/mka/movie_color.hpp',
    'runtime/include/mka/audio_device.hpp', 'runtime/include/mka/movie_audio_submission.hpp',
    'runtime/include/mka/movie_audio_progress.hpp', 'runtime/src/native/movie_audio_progress.cpp',
    'runtime/src/native/movie_color.cpp', 'runtime/src/native/movie_gpu.cpp',
    'runtime/src/native/movie_gain.cpp', 'runtime/src/native/movie_control.cpp',
    'runtime/src/native/movie_catalog.cpp', 'cmake/native_media.cmake', 'CMakeLists.txt']
PRESERVE = {
    'runtime/src/iop/mcserv.cpp': 'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',
    'runtime/src/iop/msl_native.cpp': '756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23'}


def require(ok, message):
    if not ok:
        raise RuntimeError(message)


def sha(path):
    return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()


def write(name, value):
    with (OUT / name).open('x', encoding='utf-8', newline='\n') as output:
        json.dump(value, output, indent=2)
        output.write('\n')


def execute(name, args):
    args = list(map(str, args))
    before = sha(args[0])
    process = subprocess.run(args, cwd=FORK, env=ENV, text=True,
        capture_output=True, creationflags=subprocess.CREATE_NO_WINDOW)
    value = dict(argv=args, exit=process.returncode, stdout=process.stdout,
        stderr=process.stderr, executable_sha256=before, executable_after_sha256=sha(args[0]))
    write(name, value)
    require(process.returncode == 0 and sha(args[0]) == before, name + ' failed')
    return value


def symbols(name, target, expected, required, executed):
    before = sha(target)
    record = execute(name + '.json', [BIN / 'llvm-nm.exe', '-C', '--defined-only', target])
    forbidden = [line for line in record['stdout'].splitlines()
        if re.search(r'ps2::|\b(g_ram|g_spr|EEContext|IOPContext|SPU2|CDVD)\b', line)]
    require(not forbidden and all(s in record['stdout'] for s in required), 'scoped native symbols')
    require(before == sha(target) == expected, 'exact audit target')
    value = dict(target=str(target), target_before_sha256=before, target_after_sha256=sha(target),
        audited_target_expected_sha256=expected, execution_applicable=executed,
        tool_sha256=record['executable_sha256'], forbidden=forbidden, required_defined_symbols=required)
    if executed:
        value['executed_target_sha256'] = expected
    write(name + '-target.json', value)
    return value


if __name__ == '__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    OUT.mkdir(parents=True, exist_ok=False)
    ENV = dict(os.environ, PATH=str(BIN) + os.pathsep + os.environ['PATH'])
    cache_path = BUILD / 'CMakeCache.txt'
    cache = cache_path.read_text(encoding='utf-8-sig')
    require(bool(re.search(r'^PS2_GENERATED:PATH=$', cache, re.M)), 'no generated game')
    require(bool(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$', cache, re.M)), 'Release')
    original = PARENT / 'research/inputs/modules/MOVIER.ovl'
    require(sha(original) == '3a34e3838042aa0bde0ce687a4e61e128b2eb965e60e65c34713e0a73891adb8', 'original table bytes')
    paths = [*[FORK / name for name in NAMES], original, pathlib.Path(__file__).resolve()]
    before = {str(path): sha(path) for path in paths}
    for path in paths:
        if path.suffix in ['.cpp', '.hpp', '.py', '.inc', '.cmake']:
            require(b'\r\r' not in path.read_bytes(), 'no doubled-CR source')
    for name, expected in PRESERVE.items():
        require(sha(FORK / name) == expected, 'preserved IOP source ' + name)
    write('provenance.json', dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        fork_head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=FORK, text=True).strip(),
        parent_head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=PARENT, text=True).strip(),
        source_before_sha256=before, preserved_dirty_sha256=PRESERVE,
        compiler_path=str(BIN / 'clang++.exe'), compiler_sha256=sha(BIN / 'clang++.exe'),
        cache_before_sha256=sha(cache_path), scope='Concrete native policies and PCM; deterministic fake endpoints only'))
    execute('build.json', [CMAKE, '--build', BUILD, '--target', 'test_native_movie_pcm_binding',
        'test_native_movie_kernel_policies', 'test_native_movie_playback', 'mka_native_media', '-j2'])
    runs = []
    for name, exe, args, completion, required in [
        ('pcm-binding', 'test_native_movie_pcm_binding.exe', [],
         'PASS explicit immutable PCM binding; no retail profile admission or playback', ['mka::video::NativeMoviePcmBinding::']),
        ('kernel-policies', 'test_native_movie_kernel_policies.exe', [original],
         'PASS actual kernel policies with explicit fixture inputs; no retail profile/timeline admission',
         ['mka::video::NativeMovieKernelPolicies::', 'mka::video::NativeMovieStreamPlayback::', 'mka::video::NativeMoviePcmBinding::']),
        ('stream-regression', 'test_native_movie_playback.exe', [original],
         'PASS deterministic playback composition fakes/controller/owned queues; no device or retail policy binding',
         ['mka::video::NativeMovieStreamPlayback::'])]:
        run = execute(name + '.json', [BUILD / exe, *args])
        require(run['stdout'].strip() == completion, name + ' complete assertions')
        audit = symbols(name + '-symbols', BUILD / exe, run['executable_sha256'], required, True)
        runs.append(dict(component=name, fixture_sha256=run['executable_sha256'], audit=audit))
    archive = BUILD / 'libmka_native_media.a'
    audit = symbols('archive-symbols', archive, sha(archive),
        ['mka::video::NativeMovieKernelPolicies::', 'mka::video::NativeMoviePcmBinding::'], False)
    require(before == {str(path): sha(path) for path in paths}, 'source unchanged during verification')
    for name, expected in PRESERVE.items():
        require(sha(FORK / name) == expected, 'preserved IOP source after ' + name)
    write('verification.json', dict(status='PASS_SCOPED_CONCRETE_NATIVE_MOVIE_KERNEL_POLICIES',
        source_after_sha256=before, fixtures=runs, native_archive_sha256=sha(archive),
        archive_audit=audit, cache_after_sha256=sha(cache_path),
        limits=['Explicit generic PCM/ordinary eligibility and deterministic fake endpoint composition only',
            'No normal source/speaker/sink profile admission, source timeline/lifecycle provider or complete playback proof',
            'No game, real endpoint/GPU/context, audible samples, local model, driver or settings access']))
    print(json.dumps(dict(status='PASS_SCOPED_CONCRETE_NATIVE_MOVIE_KERNEL_POLICIES',
        fixtures=[dict(component=r['component'], fixture_sha256=r['fixture_sha256']) for r in runs],
        native_archive_sha256=sha(archive))))
