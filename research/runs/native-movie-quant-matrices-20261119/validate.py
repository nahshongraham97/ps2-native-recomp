"""ID3 source-derived unit tests plus original38 native regressions; CPU only."""
import ctypes, datetime, importlib.util, json, os, pathlib, re, sys

FORK = pathlib.Path(__file__).resolve().parents[3]
PARENT = FORK.parents[2]
OUT = pathlib.Path(__file__).resolve().parent/'validation-01'
COMMON = FORK/'research/runs/native-movie-kernel-policies-20261119/validate.py'
spec = importlib.util.spec_from_file_location('_quant_matrix_receipts', COMMON)
common = importlib.util.module_from_spec(spec); spec.loader.exec_module(common)
EVIDENCE = PARENT/'research/runs/native-movie-ingress-20261119'
ASSETS = FORK/'games/mkarmageddon/disc-files/MOVIEPS2'


if __name__ == '__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    common.OUT = OUT
    common.ENV = dict(os.environ, PATH=str(common.BIN)+os.pathsep+os.environ['PATH'])
    OUT.mkdir(parents=True, exist_ok=False)
    require, sha, write = common.require, common.sha, common.write
    proof = json.loads((EVIDENCE/'MOVIE_NATIVE_HEADER_QUANT_MATRIX_EXTENSION_IMPLEMENTATION.json').read_bytes())
    for name, digest in proof['source_sha256'].items():
        require(sha(name) == digest, 'frozen quant-matrix unit implementation')
    checks = {'tools/research/verify_movie_header_units.py':
        '4b93d759660a442415a551a3450f45633d917bcab4208d2c826e4b356971c882',
        'tools/research/verify_movie_userdata_retention.py':
        '12efd7b326f60ad24c7b71802bc38969f7becd80fcd3b317dbf68e2c20c9dd0c'}
    for name, digest in checks.items():require(sha(PARENT/name) == digest, 'immutable original38 oracle')
    require(sha(FORK/'tests/test_movie_header_unit_corpus.cpp') ==
        '17434dfd36fbbb93dc04bc88a379b77a6288ebfad08af07fb75c4ddb1c0cc832', 'reviewed exhaustive corpus visitor')
    # The source-syntax index is a concurrent NEXT task. It has no CMake entry
    # or includes on this route; do not pin uncompiled in-progress files as
    # if they were inputs of the currently verified archive/executables.
    excluded = {'movie_source_syntax_index.hpp', 'movie_source_syntax_index.cpp'}
    require('movie_source_syntax_index' not in (FORK/'cmake/native_media.cmake').read_text(), 'next index is uncompiled')
    files = [x for x in (FORK/'runtime/include/mka').glob('*.hpp') if x.name not in excluded]
    files += [x for x in (FORK/'runtime/src/native').rglob('*.cpp') if x.name not in excluded]
    files += list((FORK/'runtime/src/native').rglob('*.inc'))
    files += [FORK/x for x in common.NAMES]
    files += [pathlib.Path(x) for x in proof['source_sha256']]
    files += [FORK/x for x in ['tests/test_movie_header_unit_corpus.cpp', 'tests/test_movie_userdata_retention.cpp',
        'tests/test_movie_userdata_retention_corpus.cpp', 'tests/native_test_sha256.hpp', 'cmake/native_audio.cmake']]
    files += [PARENT/x for x in checks]
    files += [COMMON, pathlib.Path(__file__).resolve(), PARENT/'tools/research/verify_native_adx_audio_assets.py']
    files += [EVIDENCE/x for x in ['MOVIE_NATIVE_HEADER_QUANT_MATRIX_EXTENSION_IMPLEMENTATION.json',
        'MOVIE_NATIVE_HEADER_QUANT_MATRIX_EXTENSION_SOURCE_CONTRACT.json',
        'MOVIE_NATIVE_HEADER_QUANT_MATRIX_EXTENSION_SOURCE_REVIEW.json',
        'MOVIE_NATIVE_HEADER_QUANT_MATRIX_EXTENSION_IMPLEMENTATION_REVIEW.json']]
    files = list(dict.fromkeys(files)); before = {str(x): sha(x) for x in files}
    for path in files:
        if path.suffix in ['.cpp', '.hpp', '.py', '.cmake', '.inc']:
            require(b'\r\r' not in path.read_bytes(), 'no doubled CR source')
    for name, digest in common.PRESERVE.items():require(sha(FORK/name) == digest, 'preserved dirty IOP')
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
        scope='ID3 synthetic format extension + original38 unit/current-retention regressions; no devices',
        excluded_uncompiled_next_task=sorted(excluded)))
    common.execute('build.json', [common.CMAKE, '--build', common.BUILD, '--target',
        'test_native_movie_header_unit', 'test_native_movie_header_unit_corpus',
        'test_native_movie_userdata_retention', 'test_native_movie_userdata_retention_corpus', 'mka_native_media', '-j2'])
    runs = []
    for label, target, args, footer, required in [
        ('quant-unit', 'test_native_movie_header_unit', [],
         'PASS native header unit checked fields/endpoints/ownership; no batch or admission binding', ['mka::video::parse_movie_header_unit(']),
        ('header-corpus', 'test_native_movie_header_unit_corpus', [ASSETS, catalog],
         'PASS native MPEG2 header units38 source bitfields/endpoints; no parser-batch/admission/player', ['mka::video::parse_movie_header_unit(']),
        ('retention-unit', 'test_native_movie_userdata_retention', [],
         'PASS native current userdata retention phase/bytes/ownership; no callback or record binding', ['mka::video::NativeMovieUserdataRetention::']),
        ('retention-corpus', 'test_native_movie_userdata_retention_corpus', [ASSETS, catalog],
         'PASS native current userdata retention38 owned bytes/phase; no callbacks or selected-output binding', ['mka::video::NativeMovieUserdataRetention::'])]:
        print('RUN '+label, flush=True)
        result = common.execute(label+'.json', [common.BUILD/(target+'.exe'), *args])
        require(result['stdout'].splitlines()[-1] == footer, 'complete affected assertions')
        audit = common.symbols(label+'-symbols', common.BUILD/(target+'.exe'), result['executable_sha256'], required, True)
        runs.append(dict(component=label, executed_target_sha256=result['executable_sha256'], audit=audit))
    for label, checker, receipt, status, count_key, count in [
        ('header-oracle', 'verify_movie_header_units.py', 'header-corpus.json', 'PASS_ORIGINAL38_HEADER_UNIT_FIELDS_AND_ENDPOINTS', 'units', 200337),
        ('retention-oracle', 'verify_movie_userdata_retention.py', 'retention-corpus.json', 'PASS_ORIGINAL38_CURRENT_USERDATA_RETENTION', 'total_events', 130400)]:
        print('RUN independent '+label, flush=True)
        common.execute(label+'-execution.json', [sys.executable, '-O', PARENT/'tools/research'/checker,
            ASSETS, OUT/receipt, OUT/(label+'.json')])
        oracle = json.loads((OUT/(label+'.json')).read_bytes())
        require(oracle['status'] == status and oracle['assets'] == 38 and oracle[count_key] == count and
            oracle['negative_checks'] == 32, 'complete immutable original38 comparison')
    archive = common.BUILD/'libmka_native_media.a'
    archive_audit = common.symbols('archive-symbols', archive, sha(archive),
        ['mka::video::parse_movie_header_unit(', 'mka::video::NativeMovieUserdataRetention::'], False)
    require(before == {name: sha(name) for name in before}, 'stable compiled source/reviews through verification')
    for name, digest in common.PRESERVE.items():require(sha(FORK/name) == digest, 'preserved IOP after verification')
    write('verification.json', dict(status='PASS_NATIVE_ID3_AND_ORIGINAL38_HEADER_RETENTION_REGRESSIONS',
        source_after_sha256=before, fixtures=runs, native_archive_sha256=sha(archive), archive_audit=archive_audit,
        cache_after_sha256=sha(cache_path), limits=[
            'ID3 has no original38 occurrences; its coverage is source-derived synthetic all16 presence patterns, zeros/varied/partials/lifetime/allocation',
            'Original38 remains200337 header units and130400 frame3/bound current-retention events; two independent32-negative comparisons',
            'Next source-syntax index uncompiled/uncredited; no callbacks/SDKselected/admission/clock/player/game claim',
            'Archive compiled/inspected only; no real endpoint/context/model/server/driver/settings action']))
    print(json.dumps(dict(status='PASS_NATIVE_ID3_AND_ORIGINAL38_HEADER_RETENTION_REGRESSIONS',
        source_pins=len(before), fixtures=len(runs), original38_negative_checks=64)), flush=True)
