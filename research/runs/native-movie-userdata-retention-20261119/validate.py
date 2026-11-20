"""Original38 native current userdata retention: CPU/file only."""
import ctypes, datetime, importlib.util, json, os, pathlib, re, sys

FORK = pathlib.Path(__file__).resolve().parents[3]
PARENT = FORK.parents[2]
OUT = pathlib.Path(__file__).resolve().parent/'validation-01'
COMMON = FORK/'research/runs/native-movie-kernel-policies-20261119/validate.py'
spec = importlib.util.spec_from_file_location('_userdata_retention_receipts', COMMON)
common = importlib.util.module_from_spec(spec); spec.loader.exec_module(common)
EVIDENCE = PARENT/'research/runs/native-movie-ingress-20261119'
ASSETS = FORK/'games/mkarmageddon/disc-files/MOVIEPS2'


if __name__ == '__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    common.OUT = OUT
    common.ENV = dict(os.environ, PATH=str(common.BIN)+os.pathsep+os.environ['PATH'])
    OUT.mkdir(parents=True, exist_ok=False)
    require, sha, write = common.require, common.sha, common.write
    proof = json.loads((EVIDENCE/'MOVIE_NATIVE_USERDATA_RETENTION_IMPLEMENTATION.json').read_bytes())
    for name, digest in proof['source_sha256'].items():
        require(sha(FORK/name) == digest, 'frozen retention implementation')
    require(sha(PARENT/'tools/research/verify_movie_userdata_retention.py') ==
        '12efd7b326f60ad24c7b71802bc38969f7becd80fcd3b317dbf68e2c20c9dd0c', 'reviewed independent checker')
    require(sha(FORK/'tests/test_movie_userdata_retention_corpus.cpp') ==
        'df967f8df2e6b2274ba4397dfee3368110a93b387c692354d369815014fe3425', 'frozen current-retention corpus fixture')
    original = PARENT/'research/inputs/modules/MOVIER.ovl'
    require(sha(original) == '3a34e3838042aa0bde0ce687a4e61e128b2eb965e60e65c34713e0a73891adb8', 'original source')
    files = list((FORK/'runtime/include/mka').glob('*.hpp'))
    files += list((FORK/'runtime/src/native').rglob('*.cpp'))
    files += list((FORK/'runtime/src/native').rglob('*.inc'))
    files += [FORK/x for x in common.NAMES]
    files += [FORK/x for x in proof['source_sha256']]
    files += [COMMON, pathlib.Path(__file__).resolve(), original,
        FORK/'tests/test_movie_userdata_retention_corpus.cpp', FORK/'tests/native_test_sha256.hpp',
        FORK/'cmake/native_audio.cmake', PARENT/'tools/research/verify_movie_userdata_retention.py',
        PARENT/'tools/research/verify_native_adx_audio_assets.py',
        PARENT/'tools/research/verify_movie_header_units.py']
    files += [EVIDENCE/x for x in ['MOVIE_NATIVE_USERDATA_RETENTION_IMPLEMENTATION.json',
        'MOVIE_NATIVE_USERDATA_RETENTION_SOURCE_REVIEW.json', 'MOVIE_NATIVE_USERDATA_RETENTION_ORACLE_PLAN.json',
        'MOVIE_NATIVE_USERDATA_RETENTION_ORACLE_SOURCE_REVIEW.json',
        'MOVIE_NATIVE_USERDATA_RETENTION_CORPUS_SOURCE_REVIEW.json',
        'MOVIE_NATIVE_HEADER_USERDATA_BINDING_SOURCE_CONTRACT.json',
        'MOVIE_NATIVE_HEADER_USERDATA_RECORD_LIFECYCLE_SOURCE_CONTRACT.json',
        'MOVIE_NATIVE_HEADER_USERDATA_CORRECTIONS_SOURCE_REVIEW.json']]
    files = list(dict.fromkeys(files)); before = {str(x): sha(x) for x in files}
    for path in files:
        if path.suffix in ['.cpp', '.hpp', '.py', '.cmake', '.inc']:
            require(b'\r\r' not in path.read_bytes(), 'no doubled CR source')
    for name, digest in common.PRESERVE.items():
        require(sha(FORK/name) == digest, 'preserved dirty IOP')
    cache_path = common.BUILD/'CMakeCache.txt'; cache = cache_path.read_text(encoding='utf-8-sig')
    require(re.search(r'^PS2_GENERATED:PATH=$', cache, re.M) and
        re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$', cache, re.M), 'native-only Release')
    saved = FORK/'research/runs/native-movie-source-20261119/validation-01/assets.tsv'
    rows = [x.split('\t') for x in saved.read_text().splitlines()]
    require(len(rows) == 38 and all(len(x) == 4 for x in rows), 'original four-column source catalog')
    catalog = OUT/'assets.tsv'
    with catalog.open('x', encoding='utf-8', newline='\n') as output:
        output.write(''.join(row[0]+'\t'+row[0]+'\n' for row in rows))
    before[str(saved)] = sha(saved); before[str(catalog)] = sha(catalog)
    write('provenance.json', dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        source_before_sha256=before, compiler_path=str(common.BIN/'clang++.exe'),
        compiler_sha256=sha(common.BIN/'clang++.exe'), cache_before_sha256=sha(cache_path),
        scope='Current owned B2 retention phase/bytes only; CPU/files, no callbacks or selected-output'))
    common.execute('build.json', [common.CMAKE, '--build', common.BUILD, '--target',
        'test_native_movie_userdata_retention', 'test_native_movie_userdata_retention_corpus', 'mka_native_media', '-j2'])
    runs = []
    for label, target, args, footer in [
        ('unit', 'test_native_movie_userdata_retention', [],
         'PASS native current userdata retention phase/bytes/ownership; no callback or record binding'),
        ('corpus', 'test_native_movie_userdata_retention_corpus', [ASSETS, catalog],
         'PASS native current userdata retention38 owned bytes/phase; no callbacks or selected-output binding')]:
        print('RUN '+label, flush=True)
        result = common.execute(label+'.json', [common.BUILD/(target+'.exe'), *args])
        require(result['stdout'].splitlines()[-1] == footer, 'complete retention fixture assertions')
        audit = common.symbols(label+'-symbols', common.BUILD/(target+'.exe'), result['executable_sha256'],
            ['mka::video::NativeMovieUserdataRetention::'], True)
        runs.append(dict(component=label, executed_target_sha256=result['executable_sha256'], audit=audit))
    print('RUN independent original38 current-retention events and checker negatives', flush=True)
    common.execute('comparison-execution.json', [sys.executable, '-O',
        PARENT/'tools/research/verify_movie_userdata_retention.py', ASSETS, OUT/'corpus.json', OUT/'source-oracle.json'])
    oracle = json.loads((OUT/'source-oracle.json').read_bytes())
    require(oracle['status'] == 'PASS_ORIGINAL38_CURRENT_USERDATA_RETENTION' and
        oracle['assets'] == 38 and oracle['total_events'] == 130400 and oracle['negative_checks'] == 32,
        'complete independent normal38 retention comparison')
    archive = common.BUILD/'libmka_native_media.a'
    archive_audit = common.symbols('archive-symbols', archive, sha(archive),
        ['mka::video::NativeMovieUserdataRetention::'], False)
    require(before == {name: sha(name) for name in before}, 'stable source/reviews through verification')
    for name, digest in common.PRESERVE.items():
        require(sha(FORK/name) == digest, 'preserved IOP after verification')
    write('verification.json', dict(status='PASS_SCOPED_NATIVE_USERDATA_RETENTION_ORIGINAL38',
        source_after_sha256=before, fixtures=runs, native_archive_sha256=sha(archive), archive_audit=archive_audit,
        cache_after_sha256=sha(cache_path), oracle_sha256=sha(OUT/'source-oracle.json'), limits=[
            'Original38 current-retention states only; frame structure3, explicit ordinary-factory bound fixture',
            'No source advancement/callback/selected-work/visibility/admission/clock/player composition',
            'Archive compiled/inspected only; no real endpoint/context/game/model/server/driver/settings action']))
    print(json.dumps(dict(status='PASS_SCOPED_NATIVE_USERDATA_RETENTION_ORIGINAL38', source_pins=len(before),
        assets=oracle['assets'], events=oracle['total_events'], negatives=oracle['negative_checks'])), flush=True)
