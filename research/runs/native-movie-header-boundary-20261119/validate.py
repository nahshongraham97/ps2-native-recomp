"""Source-derived native header-boundary checks; CPU only, no device."""
import ctypes, datetime, importlib.util, json, os, pathlib, re

FORK = pathlib.Path(__file__).resolve().parents[3]
PARENT = FORK.parents[2]
OUT = pathlib.Path(__file__).resolve().parent/'validation-01'
COMMON = FORK/'research/runs/native-movie-kernel-policies-20261119/validate.py'
spec = importlib.util.spec_from_file_location('_header_boundary_receipts', COMMON)
common = importlib.util.module_from_spec(spec); spec.loader.exec_module(common)
EVIDENCE = PARENT/'research/runs/native-movie-ingress-20261119'


if __name__ == '__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    common.OUT = OUT
    common.ENV = dict(os.environ, PATH=str(common.BIN)+os.pathsep+os.environ['PATH'])
    OUT.mkdir(exist_ok=False, parents=True)
    require, sha, write = common.require, common.sha, common.write
    proof = json.loads((EVIDENCE/'MOVIE_NATIVE_HEADER_BOUNDARY_IMPLEMENTATION.json').read_bytes())
    for name, digest in proof['source_sha256'].items():
        require(sha(FORK/name) == digest, 'frozen new boundary source')
    original = PARENT/'research/inputs/modules/MOVIER.ovl'
    require(sha(original) == '3a34e3838042aa0bde0ce687a4e61e128b2eb965e60e65c34713e0a73891adb8', 'actual original source')
    files = list((FORK/'runtime/include/mka').glob('*.hpp'))
    files += list((FORK/'runtime/src/native').rglob('*.cpp'))
    files += list((FORK/'runtime/src/native').rglob('*.inc'))
    files += [FORK/x for x in common.NAMES]
    files += [FORK/x for x in proof['source_sha256']]
    files += [COMMON, pathlib.Path(__file__).resolve(), original,
        FORK/'tests/test_movie_prefix_events.cpp', FORK/'cmake/native_audio.cmake']
    files += [EVIDENCE/x for x in ['MOVIE_NATIVE_HEADER_BOUNDARY_IMPLEMENTATION.json',
        'MOVIE_NATIVE_HEADER_BOUNDARY_SOURCE_REVIEW.json', 'MOVIE_NATIVE_HEADER_BOUNDARY_ROOT_BUILD_SOURCE_REVIEW.json',
        'MOVIE_NATIVE_HEADER_BATCH_BOUNDARY_SOURCE_CONTRACT.md', 'MOVIE_NATIVE_HEADER_BATCH_BOUNDARY_SOURCE_CONTRACT.json',
        'header-batch-boundary-source-words/batch.words.txt']]
    files = list(dict.fromkeys(files)); before = {str(x): sha(x) for x in files}
    for path in files:
        if path.suffix in ['.cpp', '.hpp', '.py', '.cmake', '.inc']:
            require(b'\r\r' not in path.read_bytes(), 'no doubled CR source')
    for name, digest in common.PRESERVE.items():
        require(sha(FORK/name) == digest, 'preserved dirty IOP')
    cache_path = common.BUILD/'CMakeCache.txt'
    cache = cache_path.read_text(encoding='utf-8-sig')
    require(re.search(r'^PS2_GENERATED:PATH=$', cache, re.M) and
        re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$', cache, re.M), 'native-only Release')
    write('provenance.json', dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        source_before_sha256=before, compiler_path=str(common.BIN/'clang++.exe'),
        compiler_sha256=sha(common.BIN/'clang++.exe'), cache_before_sha256=sha(cache_path),
        scope='Pure source-derived header-boundary owner/cache/owned bytes only; no parser, device or game'))
    common.execute('build.json', [common.CMAKE, '--build', common.BUILD, '--target',
        'test_native_movie_header_boundary', 'test_native_movie_prefix_events', 'mka_native_media', '-j2'])
    runs = []
    for label, target, footer, required in [
        ('header-boundary', 'test_native_movie_header_boundary',
         'PASS native source header-boundary proposals/cache/ownership; no parser admission or timeline binding',
         ['mka::video::NativeMovieHeaderBoundary::']),
        ('prefix-regression', 'test_native_movie_prefix_events',
         'PASS native raw ES prefix events streaming/classes/ownership; no header admission or timeline binding',
         ['mka::video::NativeMoviePrefixEvents::'])]:
        result = common.execute(label+'.json', [common.BUILD/(target+'.exe')])
        require(result['stdout'].strip() == footer, 'complete source-derived assertions')
        audit = common.symbols(label+'-symbols', common.BUILD/(target+'.exe'), result['executable_sha256'], required, True)
        runs.append(dict(component=label, executed_target_sha256=result['executable_sha256'], audit=audit))
    archive = common.BUILD/'libmka_native_media.a'
    archive_audit = common.symbols('archive-symbols', archive, sha(archive),
        ['mka::video::NativeMovieHeaderBoundary::', 'mka::video::NativeMoviePrefixEvents::'], False)
    require(before == {name: sha(name) for name in before}, 'source/review stable during verification')
    for name, digest in common.PRESERVE.items():
        require(sha(FORK/name) == digest, 'preserved IOP after verification')
    write('verification.json', dict(status='PASS_SCOPED_NATIVE_HEADER_BOUNDARY',
        source_after_sha256=before, fixtures=runs, native_archive_sha256=sha(archive), archive_audit=archive_audit,
        cache_after_sha256=sha(cache_path), limits=[
            'Source-derived literal C/V/lookahead cases; not a captured retail source-window cadence oracle',
            'No actual parser/admission/GOP serial/SDK position/final flush/player clock or completion binding',
            'Native archive compiled/inspected only; no real endpoint/context/game/model/server/driver/settings action']))
    print(json.dumps(dict(status='PASS_SCOPED_NATIVE_HEADER_BOUNDARY', source_pins=len(before),
        fixtures=[dict(component=x['component'], executed_target_sha256=x['executed_target_sha256']) for x in runs])), flush=True)
