"""Actual typed native policy routes with deterministic endpoints only."""
import ctypes, datetime, importlib.util, json, os, pathlib, re

FORK = pathlib.Path(__file__).resolve().parents[3]
PARENT = FORK.parents[2]
OUT = pathlib.Path(__file__).resolve().parent / 'validation-01'
COMMON = FORK / 'research/runs/native-movie-kernel-policies-20261119/validate.py'
spec = importlib.util.spec_from_file_location('_native_eligibility_route_receipts', COMMON)
common = importlib.util.module_from_spec(spec); spec.loader.exec_module(common)

if __name__ == '__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    common.OUT = OUT
    common.ENV = dict(os.environ, PATH=str(common.BIN) + os.pathsep + os.environ['PATH'])
    OUT.mkdir(parents=True, exist_ok=False)
    require, sha, write = common.require, common.sha, common.write
    source = PARENT/'research/runs/native-movie-ingress-20261119'
    extra = ['runtime/include/mka/movie_service_clock.hpp', 'runtime/src/native/movie_service_clock.cpp',
        'runtime/include/mka/movie_decoder_mode.hpp', 'runtime/src/native/movie_decoder_mode.cpp',
        'runtime/include/mka/movie_pcm_downmix.hpp', 'runtime/src/native/movie_pcm_downmix.cpp']
    paths = [FORK/x for x in dict.fromkeys(common.NAMES+extra)]
    original = PARENT/'research/inputs/modules/MOVIER.ovl'
    main = PARENT/'research/inputs/SLUS_214.10'
    require(sha(original) == '3a34e3838042aa0bde0ce687a4e61e128b2eb965e60e65c34713e0a73891adb8', 'original source/table')
    require(sha(main) == 'a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f', 'original main source')
    correction = source/'MOVIE_NATIVE_ELIGIBILITY_ROUTES_REVIEW01_CORRECTIONS.json'
    initial = source/'MOVIE_NATIVE_ELIGIBILITY_ROUTES_IMPLEMENTATION.json'
    proof = json.loads(initial.read_bytes())
    for name, digest in proof['source_proof_pins'].items():
        require(sha(name) == digest, 'frozen source proof')
        paths.append(pathlib.Path(name))
    for name, digest in json.loads(correction.read_bytes())['after'].items():
        require(sha(name) == digest, 'corrected frozen policy source')
    paths += [COMMON, original, main, correction, initial,
        source/'MOVIE_NATIVE_ELIGIBILITY_ROUTES_SOURCE_REVIEW.json', pathlib.Path(__file__).resolve()]
    paths = list(dict.fromkeys(paths))
    before = {str(x): sha(x) for x in paths}
    for path in paths:
        if path.suffix in ['.cpp', '.hpp', '.py', '.cmake', '.inc']:
            require(b'\r\r' not in path.read_bytes(), 'no doubled-CR source')
    for name, digest in common.PRESERVE.items():
        require(sha(FORK/name) == digest, 'preserved dirty IOP')
    cache_path = common.BUILD/'CMakeCache.txt'
    cache = cache_path.read_text(encoding='utf-8-sig')
    require(bool(re.search(r'^PS2_GENERATED:PATH=$', cache, re.M)) and
        bool(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$', cache, re.M)), 'native-only Release')
    write('provenance.json', dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        source_before_sha256=before, compiler_path=str(common.BIN/'clang++.exe'),
        compiler_sha256=sha(common.BIN/'clang++.exe'), cache_before_sha256=sha(cache_path),
        scope='Typed ordinary/direct/retained policies and real stream owner; deterministic endpoints only'))
    targets = ['test_native_movie_kernel_policies', 'test_native_movie_pcm_binding',
        'test_native_movie_playback', 'mka_native_media']
    common.execute('build.json', [common.CMAKE, '--build', common.BUILD, '--target', *targets, '-j2'])
    runs = []
    for label, target, args, footer, required in [
        ('typed-policies', targets[0], [original],
         'PASS actual kernel policies with explicit fixture inputs; no retail profile/timeline admission',
         ['mka::video::NativeMovieKernelPolicies::', 'mka::video::MovieEligibilityServiceBinding::']),
        ('pcm-regression', targets[1], [],
         'PASS explicit immutable PCM binding; no retail profile admission or playback',
         ['mka::video::NativeMoviePcmBinding::']),
        ('stream-regression', targets[2], [original],
         'PASS deterministic playback composition fakes/controller/owned queues; no device or retail policy binding',
         ['mka::video::NativeMovieStreamPlayback::'])]:
        result = common.execute(label+'.json', [common.BUILD/(target+'.exe'), *args])
        require(result['stdout'].strip() == footer, 'complete affected fixture assertions')
        audit = common.symbols(label+'-symbols', common.BUILD/(target+'.exe'), result['executable_sha256'], required, True)
        runs.append(dict(component=label, executed_target_sha256=result['executable_sha256'], audit=audit))
    archive = common.BUILD/'libmka_native_media.a'
    archive_audit = common.symbols('archive-symbols', archive, sha(archive),
        ['mka::video::NativeMovieKernelPolicies::', 'mka::video::MovieEligibilityServiceBinding::',
         'mka::video::NativeMovieDecoderMode::'], False)
    require(before == {str(x): sha(x) for x in paths}, 'source/evidence stable through verification')
    for name, digest in common.PRESERVE.items():
        require(sha(FORK/name) == digest, 'preserved IOP after verification')
    write('verification.json', dict(status='PASS_SCOPED_TYPED_NATIVE_MOVIE_ELIGIBILITY_ROUTES',
        source_after_sha256=before, fixtures=runs, native_archive_sha256=sha(archive), archive_audit=archive_audit,
        cache_after_sha256=sha(cache_path),
        limits=['Deterministic provider/endpoints with actual native kernels/stream owner; no normal retail provider binding',
            'Actual selected header/output ledger, initial settings, source clocks/origins/cadence/lifecycle remain open',
            'Archive compiled/inspected only; no real device/context/game/audio/model/server/driver/settings action']))
    print(json.dumps(dict(status='PASS_SCOPED_TYPED_NATIVE_MOVIE_ELIGIBILITY_ROUTES',
        fixtures=[dict(component=x['component'], executed_target_sha256=x['executed_target_sha256']) for x in runs])), flush=True)
