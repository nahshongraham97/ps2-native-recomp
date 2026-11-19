"""Source-bound native downmix and explicit plane-policy verification."""
import ctypes, datetime, importlib.util, json, os, pathlib, re, sys

FORK = pathlib.Path(__file__).resolve().parents[3]
PARENT = FORK.parents[2]
WORKSPACE = PARENT.parent
COMMON = FORK / 'research/runs/native-movie-kernel-policies-20261119/validate.py'
OUT = pathlib.Path(__file__).resolve().parent / 'validation-01'
spec = importlib.util.spec_from_file_location('_native_movie_fixture_receipts', COMMON)
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
    original = PARENT / 'research/inputs/modules/MOVIER.ovl'
    checker = PARENT / 'tools/research/verify_movie_downmix_trace.py'
    contract = PARENT / 'research/runs/native-movie-ingress-20261119/MOVIE_NATIVE_DOWNMIX_SOURCE_CONTRACT.json'
    words = contract.with_name('native-downmix-source.words.txt')
    names = list(dict.fromkeys(common.NAMES + [
        'runtime/include/mka/movie_pcm_downmix.hpp', 'runtime/src/native/movie_pcm_downmix.cpp',
        'tests/test_movie_pcm_downmix.cpp']))
    paths = [*[FORK / name for name in names], original, checker, contract, words, COMMON,
        pathlib.Path(__file__).resolve()]
    before = {str(path): sha(path) for path in paths}
    require(sha(original) == '3a34e3838042aa0bde0ce687a4e61e128b2eb965e60e65c34713e0a73891adb8', 'original image')
    for name, digest in common.PRESERVE.items():
        require(sha(FORK / name) == digest, 'preserve IOP source ' + name)
    for path in paths:
        if path.suffix in ['.cpp', '.hpp', '.inc', '.cmake', '.py']:
            require(b'\r\r' not in path.read_bytes(), 'no doubled CR source')
    write('provenance.json', dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        fork_head=common.subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=FORK, text=True).strip(),
        parent_head=common.subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=PARENT, text=True).strip(),
        source_before_sha256=before, preserved_dirty_sha256=common.PRESERVE,
        compiler_path=str(common.BIN / 'clang++.exe'), compiler_sha256=sha(common.BIN / 'clang++.exe'),
        cache_before_sha256=sha(cache_path),
        scope='Plain downmix sum classes plus explicit plane policy through native binding/policies; no devices'))
    common.execute('build.json', [common.CMAKE, '--build', common.BUILD, '--target',
        'test_native_movie_pcm_downmix', 'test_native_movie_pcm_binding',
        'test_native_movie_kernel_policies', 'mka_native_media', '-j2'])
    downmix = common.execute('downmix.json', [common.BUILD / 'test_native_movie_pcm_downmix.exe', '--dump-sums'])
    common.execute('oracle-execution.json', [sys.executable, '-O', checker, OUT / 'downmix.json', OUT / 'oracle.json'])
    oracle = json.loads((OUT / 'oracle.json').read_bytes())
    require(oracle['status'] == 'PASS_COMPLETE_SIGNED16_DOWNMIX_SUM_CLASSES' and
        oracle['sum_classes'] == 131071 and oracle['executable_sha256'] == downmix['executable_sha256'],
        'complete original-source-bound downmix oracle')
    audits = [common.symbols('downmix-symbols', common.BUILD / 'test_native_movie_pcm_downmix.exe',
        downmix['executable_sha256'], ['mka::video::movie_pcm_downmix_duplicate('], True)]
    fixtures = [dict(component='downmix', fixture_sha256=downmix['executable_sha256'])]
    for label, executable, args, footer, required in [
        ('pcm-binding', 'test_native_movie_pcm_binding.exe', [],
         'PASS explicit immutable PCM binding; no retail profile admission or playback',
         ['mka::video::NativeMoviePcmBinding::', 'mka::video::movie_pcm_downmix_duplicate(']),
        ('kernel-policies', 'test_native_movie_kernel_policies.exe', [original],
         'PASS actual kernel policies with explicit fixture inputs; no retail profile/timeline admission',
         ['mka::video::NativeMovieKernelPolicies::', 'mka::video::movie_pcm_downmix_duplicate('])]:
        run = common.execute(label + '.json', [common.BUILD / executable, *args])
        require(run['stdout'].strip() == footer, label + ' complete assertions')
        fixtures.append(dict(component=label, fixture_sha256=run['executable_sha256']))
        audits.append(common.symbols(label + '-symbols', common.BUILD / executable,
            run['executable_sha256'], required, True))
    archive = common.BUILD / 'libmka_native_media.a'
    archive_audit = common.symbols('archive-symbols', archive, sha(archive),
        ['mka::video::NativeMoviePcmBinding::', 'mka::video::movie_pcm_downmix_duplicate('], False)
    require(before == {str(path): sha(path) for path in paths}, 'source unchanged during execution')
    for name, digest in common.PRESERVE.items():
        require(sha(FORK / name) == digest, 'preserved IOP source after ' + name)
    write('verification.json', dict(status='PASS_SCOPED_NATIVE_DOWNMIX_AND_EXPLICIT_PLANE_POLICY',
        source_after_sha256=before, fixtures=fixtures, oracle=oracle, symbol_audits=audits,
        native_archive_sha256=sha(archive), archive_audit=archive_audit,
        cache_after_sha256=sha(cache_path),
        limits=['Exhaustive signed16 SUM classes, not literal all-pair execution or encoded codec equivalence',
            'Explicit conditional plane profile only; actual initial selector, sink/speaker and timeline/lifecycle remain OPEN',
            'No game, real audio/GPU/context, audible samples, model, driver/settings or backup modification']))
    print(json.dumps(dict(status='PASS_SCOPED_NATIVE_DOWNMIX_AND_EXPLICIT_PLANE_POLICY',
        fixtures=fixtures, sum_classes=oracle['sum_classes'], negative_checks=oracle['negative_checks'],
        native_archive_sha256=sha(archive))))
