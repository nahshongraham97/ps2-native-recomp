"""Affected native playback ingress/service fixtures; frozen decoder is not run."""
import ctypes, datetime, importlib.util, json, os, pathlib, re, sys

FORK = pathlib.Path(__file__).resolve().parents[3]
PARENT = FORK.parents[2]
COMMON = FORK / 'research/runs/native-movie-kernel-policies-20261119/validate.py'
spec = importlib.util.spec_from_file_location('_ingress_receipts', COMMON)
common = importlib.util.module_from_spec(spec)
spec.loader.exec_module(common)

if __name__ == '__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    if len(sys.argv) != 2 or not re.fullmatch(r'validation-ingress-[0-9]+', sys.argv[1]):
        raise SystemExit('usage: check_ingress.py validation-ingress-NN')
    out = pathlib.Path(__file__).resolve().parent / sys.argv[1]
    common.OUT = out
    common.ENV = dict(os.environ, PATH=str(common.BIN) + os.pathsep + os.environ['PATH'])
    require, sha, write = common.require, common.sha, common.write
    evidence = PARENT / 'research/runs/native-movie-ingress-20261119'
    files = list((FORK / 'runtime/include/mka').glob('*.hpp'))
    files += list((FORK / 'runtime/src/native').rglob('*.cpp'))
    files += list((FORK / 'runtime/src/native').rglob('*.inc'))
    files += [FORK / x for x in ['CMakeLists.txt', 'cmake/native_media.cmake', 'cmake/native_audio.cmake',
        'tests/test_movie_native_context.cpp', 'tests/test_movie_output_position.cpp',
        'tests/test_movie_service.cpp', 'tests/test_movie_admission_feed.cpp']]
    files += [PARENT / 'research/inputs/modules/MOVIER.ovl', COMMON, pathlib.Path(__file__).resolve(), common.BIN / 'clang++.exe']
    files = list(dict.fromkeys(files))
    before = {str(x): sha(x) for x in files}
    for name, digest in common.PRESERVE.items():
        require(sha(FORK / name) == digest, 'preserved IOP input')
    def cache_contract():
        text = (common.BUILD / 'CMakeCache.txt').read_text(encoding='utf-8-sig')
        require(re.search(r'^PS2_GENERATED:PATH=$', text, re.M) and
            re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$', text, re.M), 'native-only Release build')
    cache_contract()
    out.mkdir(exist_ok=False)
    write('provenance.json', dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        source_before_sha256=before, scope='Four affected native playback fixtures; no decoder/corpus/device/game'))
    try:
        targets = ['test_native_movie_native_context', 'test_native_movie_output_position',
            'test_native_movie_admission_feed', 'test_native_movie_service']
        common.execute('build.json', [common.CMAKE, '--build', common.BUILD, '--target', *targets, '-j2'])
        fixtures = []
        for label, target, args, footer, symbol in [
            ('context', targets[0], [], 'PASS concrete source-owned movie context kernels/lifecycle; no default profile or native device/player admission', 'NativeMoviePlaybackContext::'),
            ('position', targets[1], [], 'PASS native explicit-work output position ordering/identity/lifetime; no automatic decoder/admission binding', 'NativeMovieOutputPosition::'),
            ('feed', targets[2], [], 'PASS native admission feed current headers/explicit parse entries/work candidates; no business admission or decoder execution', 'NativeMovieAdmissionFeed::'),
            ('service', targets[3], [PARENT / 'research/inputs/modules/MOVIER.ovl'], 'PASS native movie service concrete session/kernel/context lifecycle; no retail coordinator or profile admission', 'NativeMovieService::')]:
            print('RUN ' + label, flush=True)
            exe = common.BUILD / (target + '.exe')
            result = common.execute(label + '.json', [exe, *args])
            require(result['stdout'].splitlines()[-1] == footer, 'complete ' + label + ' assertions')
            audit = common.symbols(label + '-symbols', exe, result['executable_sha256'],
                ['mka::video::' + symbol], True)
            fixtures.append(dict(component=label, executed_target_sha256=result['executable_sha256'], audit=audit))
        require(before == {name: sha(name) for name in before}, 'stable affected native source/input pins')
        cache_contract()
        for name, digest in common.PRESERVE.items():
            require(sha(FORK / name) == digest, 'preserved IOP after fixtures')
        write('verification.json', dict(status='PASS_FOCUSED_NATIVE_PLAYBACK_INGRESS_SERVICE',
            source_after_sha256=before, fixtures=fixtures,
            limits=['No offline decoder or corpus execution; b6d93925 remains frozen',
                'Real acquisition/profile/clock/work/finish producers and complete playback/game acceptance remain open',
                'No physical device, game, local inference/server or driver/settings action']))
        print(json.dumps(dict(status='PASS_FOCUSED_NATIVE_PLAYBACK_INGRESS_SERVICE', fixtures=len(fixtures), pins=len(before))), flush=True)
    except Exception as error:
        write('aborted.json', dict(status='AFFECTED_PLAYBACK_CHECK_FAILED', error=str(error)))
        raise
