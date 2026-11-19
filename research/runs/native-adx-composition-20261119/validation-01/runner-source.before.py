"""Native ADX composition and same-owned audio index; no device/game run."""
import ctypes, datetime, hashlib, importlib.util, json, os, pathlib, re, struct, sys
FORK = pathlib.Path(__file__).resolve().parents[3]
PARENT = FORK.parents[2]
COMMON = FORK / 'research/runs/native-movie-kernel-policies-20261119/validate.py'
OUT = pathlib.Path(__file__).resolve().parent / 'validation-01'
SOURCE = PARENT / 'research/runs/native-movie-ingress-20261119'
ASSETS = FORK / 'games/mkarmageddon/disc-files/MOVIEPS2'
spec = importlib.util.spec_from_file_location('_native_adx_composition_receipts', COMMON)
common = importlib.util.module_from_spec(spec)
spec.loader.exec_module(common)
require, sha, write = common.require, common.sha, common.write


def parse_json(raw):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, 'duplicate JSON key')
            result[key] = value
        return result
    def nonfinite(value):
        raise ValueError('nonfinite JSON constant: ' + value)
    return json.loads(raw, object_pairs_hook=unique, parse_constant=nonfinite)


def exclusive(path, raw):
    with path.open('xb') as file:
        file.write(raw)


def file_sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def same(actual, expected):
    require(type(actual) is type(expected), 'exact observed header type')
    if type(expected) is dict:
        require(actual.keys() == expected.keys(), 'complete observed header keys')
        for key in expected:
            same(actual[key], expected[key])
    elif type(expected) is list:
        require(len(actual) == len(expected), 'complete observed header cardinality')
        for a, e in zip(actual, expected):
            same(a, e)
    else:
        require(actual == expected, 'exact observed header scalar')


if __name__ == '__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    common.OUT = OUT
    common.ENV = dict(os.environ, PATH=str(common.BIN) + os.pathsep + os.environ['PATH'])
    OUT.mkdir(parents=True, exist_ok=False)
    fixtures = OUT / 'header-fixtures'
    fixtures.mkdir()
    checker = PARENT / 'tools/research/verify_native_adx_audio_assets.py'
    contract = SOURCE / 'MOVIE_ADX_HEADER_NATIVE_BINDING_CONTRACT.json'
    baseline = SOURCE / 'pes-index-corpus.json'
    require(sha(baseline) == '53effc8afc0fa03b9c08917126197b295c19e0fb8c3f1257abb4407fefa521da',
        'exact independently compared historical video corpus receipt')
    require(sha(checker) == '82b7f4153801a694c0a910781739d5f99fb51f69fff9755bde7d82ebaf1276c9',
        'frozen independent original-asset PCM comparator')
    cache_path = common.BUILD / 'CMakeCache.txt'
    cache = cache_path.read_text(encoding='utf-8-sig')
    require(bool(re.search(r'^PS2_GENERATED:PATH=$', cache, re.M)), 'no generated game')
    require(bool(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$', cache, re.M)), 'Release')
    extra = [
        'runtime/include/mka/movie_adx_blocks.hpp', 'runtime/src/native/movie_adx_blocks.cpp',
        'runtime/include/mka/movie_adx_header.hpp', 'runtime/src/native/movie_adx_header.cpp',
        'runtime/include/mka/movie_adx_stream.hpp', 'runtime/src/native/movie_adx_stream.cpp',
        'runtime/include/mka/movie_adx_profile.hpp', 'runtime/src/native/movie_adx_profile.cpp',
        'runtime/include/mka/movie_pes_index.hpp', 'runtime/src/native/movie_pes_index.cpp',
        'runtime/src/native/movie_assets.cpp', 'runtime/src/native/native_file.cpp',
        'tests/test_movie_adx_header.cpp', 'tests/test_movie_adx_header_assets.cpp',
        'tests/test_movie_adx_stream.cpp', 'tests/test_movie_adx_profile.cpp',
        'tests/test_movie_audio_pes_index.cpp', 'tests/test_movie_pes_index.cpp',
        'tests/test_movie_adx_audio_assets.cpp', 'tests/native_test_sha256.hpp']
    names = list(dict.fromkeys(common.NAMES + extra))
    paths = [*[FORK / name for name in names], COMMON, checker, contract, baseline,
        SOURCE / 'MOVIE_ADX_STREAM_END_SOURCE_CONTRACT.json',
        SOURCE / 'MOVIE_ADX_PREDICTOR_INPUT_SOURCE.json',
        SOURCE / 'MOVIE_ADX_COEFFICIENT_REFERENCE_RESULTS.md', pathlib.Path(__file__).resolve()]
    before = {str(path): sha(path) for path in paths}
    for path in paths:
        if path.suffix in ['.cpp', '.hpp', '.py', '.inc', '.cmake']:
            require(b'\r\r' not in path.read_bytes(), 'no doubled CR source')
    for name, digest in common.PRESERVE.items():
        require(sha(FORK / name) == digest, 'preserved IOP source')
    records = parse_json(contract.read_bytes())['assets']
    require(len(records) == 38 and len({a['key'] for a in records}) == 38, 'complete source asset inventory')
    expected_headers, manifest, catalog, asset_pins, input_pins = [], [], [], {}, {}
    for record in records:
        key = record['key']
        path = ASSETS / key
        stat = path.stat()
        require(stat.st_size == record['source_bytes'] and file_sha(path) == record['source_sha256'], 'original source ' + key)
        asset_pins[key] = dict(path=str(path), sha256=record['source_sha256'], bytes=stat.st_size, mtime_ns=stat.st_mtime_ns)
        catalog.append(key + '\t' + key)
        require(len(record['headers']) <= 1, 'unique actual ADX header')
        if not record['headers']:
            continue
        entry = record['headers'][0]
        with path.open('rb') as file:
            file.seek(entry['offset'])
            raw = file.read(entry['extent'])
        require(len(raw) == entry['extent'], 'actual header extent')
        extent = int.from_bytes(raw[2:4], 'big') + 4
        rate, count = struct.unpack('>II', raw[8:16])
        cutoff = int.from_bytes(raw[16:18], 'big')
        seeds = [list(struct.unpack('>hh', raw[24:28])), list(struct.unpack('>hh', raw[28:32]))]
        require(extent == entry['extent'] and rate == entry['rate'] and count == entry['sample_count'] and
            cutoff == entry['cutoff'] and seeds == entry['final_plane_histories'], 'independent source header fields')
        blob = fixtures / ('header-%03d.bin' % len(expected_headers))
        exclusive(blob, raw)
        fields = [extent, rate, count, cutoff, *seeds[0], *seeds[1]]
        manifest.append('\t'.join([key, blob.name, *map(str, fields)]))
        expected_headers.append(dict(key=key, data_start=extent, encoding=raw[4], block_bytes=raw[5],
            sample_bits=raw[6], source_planes=raw[7], sample_rate=rate, declared_samples=count,
            cutoff=cutoff, version=raw[18], flags=raw[19], histories=seeds, raw_header_hex=raw.hex()))
        input_pins[str(blob)] = sha(blob)
    require(len(expected_headers) == 29, 'complete actual29 header fixtures')
    header_table, catalog_table = OUT / 'headers.tsv', OUT / 'assets.tsv'
    exclusive(header_table, ('\n'.join(manifest) + '\n').encode('utf-8'))
    exclusive(catalog_table, ('\n'.join(catalog) + '\n').encode('utf-8'))
    input_pins.update({str(header_table): sha(header_table), str(catalog_table): sha(catalog_table)})
    write('provenance.json', dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        fork_head=common.subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=FORK, text=True).strip(),
        parent_head=common.subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=PARENT, text=True).strip(),
        source_before_sha256=before, fixture_input_sha256=input_pins, original_assets=asset_pins,
        preserved_dirty_sha256=common.PRESERVE, compiler_path=str(common.BIN / 'clang++.exe'),
        compiler_sha256=sha(common.BIN / 'clang++.exe'), cache_before_sha256=sha(cache_path),
        scope='Native codec/header/profile/stream/audio index with authentic original assets; no devices/game'))
    targets = ['test_native_movie_adx_header', 'test_native_movie_adx_header_assets',
        'test_native_movie_adx_stream', 'test_native_movie_adx_profile', 'test_native_movie_audio_pes_index',
        'test_native_movie_pes_index', 'test_native_movie_adx_audio_assets', 'mka_native_media']
    common.execute('build.json', [common.CMAKE, '--build', common.BUILD, '--target', *targets, '-j2'])
    results, audits = [], []
    for label, executable, args, footer, required in [
        ('header', targets[0], [], 'PASS owned explicit ADX header fields/history; no coefficient or packet admission', ['mka::video::parse_movie_adx_header(']),
        ('stream', targets[2], [], 'PASS owned native ADX stream staging/trim/marker transactions; no retail coefficient or movie admission', ['mka::video::NativeMovieAdxStream::']),
        ('profile', targets[3], [], 'PASS explicit source-derived ADX profile data; no hardware or decoder-mode admission', ['mka::video::NativeMovieAdxProfileResolver::']),
        ('audio-index', targets[4], [], 'PASS same-owned audio PES spans/readers; no decoder or timestamp-clock binding', ['mka::video::NativeMoviePesIndex::audio_streams(']),
        ('pes-regression', targets[5], [], 'SYNTHETIC PASS', ['mka::video::NativeMoviePesIndex::streams('])]:
        run = common.execute(label + '.json', [common.BUILD / (executable + '.exe'), *args])
        require(run['stdout'].strip() == footer, label + ' complete assertions')
        results.append(dict(component=label, fixture_sha256=run['executable_sha256']))
        audits.append(common.symbols(label + '-symbols', common.BUILD / (executable + '.exe'), run['executable_sha256'], required, True))
    headers = common.execute('real-headers.json', [common.BUILD / (targets[1] + '.exe'), fixtures, header_table])
    lines = headers['stdout'].splitlines()
    require(len(lines) == 30 and lines[-1] == 'PASS native ADX header real29 source fixtures; no coefficient/packet/mode admission', 'complete real header fixture')
    same([parse_json(line) for line in lines[:-1]], expected_headers)
    write('real-header-comparison.json', dict(status='PASS_ALL29_ORIGINAL_HEADERS_AND_FIELDS', assets=29,
        fixture_sha256=headers['executable_sha256'], source_contract_sha256=sha(contract), fixture_input_sha256=input_pins))
    corpus = common.execute('video-index-corpus.json', [common.BUILD / (targets[5] + '.exe'), '--corpus', ASSETS])
    old = parse_json(baseline.read_bytes())
    require(type(old.get('exit_code')) is int and old['exit_code'] == 0 and
        type(old.get('stdout')) is str and old['all_independent_fields_match'] is True and
        len(old['stdout'].splitlines()) == 38 and corpus['stdout'] == old['stdout'], 'complete unchanged38 video index corpus')
    audio = common.execute('real-audio.json', [common.BUILD / (targets[6] + '.exe'), ASSETS, catalog_table])
    common.execute('audio-oracle-execution.json', [sys.executable, '-O', checker, OUT / 'real-audio.json', ASSETS, OUT / 'audio-oracle.json'])
    oracle = parse_json((OUT / 'audio-oracle.json').read_bytes())
    require(oracle.get('status') == 'PASS_ORIGINAL_ASSET_NATIVE_ADX_CORPUS' and
        all(type(oracle.get(key)) is int and oracle[key] == count for key, count in
            [('assets', 38), ('audio', 29), ('video_only', 9)]) and
        oracle['executable_sha256'] == audio['executable_sha256'] and
        oracle['execution_receipt_sha256'] == sha(OUT / 'real-audio.json') and
        oracle['checker_sha256'] == before[str(checker)], 'actual complete audio-corpus oracle target/source/receipt')
    for label, target, run in [('real-headers', targets[1], headers), ('real-audio', targets[6], audio)]:
        audits.append(common.symbols(label + '-symbols', common.BUILD / (target + '.exe'), run['executable_sha256'], ['mka::video::parse_movie_adx_header('], True))
    archive = common.BUILD / 'libmka_native_media.a'
    archive_audit = common.symbols('archive-symbols', archive, sha(archive),
        ['mka::video::NativeMovieAdxStream::', 'mka::video::NativeMovieAdxProfileResolver::', 'mka::video::NativeMoviePesIndex::audio_streams('], False)
    require(before == {str(path): sha(path) for path in paths}, 'source unchanged during verification')
    require(input_pins == {path: sha(path) for path in input_pins}, 'fixture inputs unchanged')
    for key, item in asset_pins.items():
        path = pathlib.Path(item['path'])
        require(path.stat().st_size == item['bytes'] and path.stat().st_mtime_ns == item['mtime_ns'] and file_sha(path) == item['sha256'], 'original asset unchanged ' + key)
    for name, digest in common.PRESERVE.items():
        require(sha(FORK / name) == digest, 'preserved IOP source after')
    write('verification.json', dict(status='PASS_SCOPED_NATIVE_ADX_COMPOSITION_AND_REAL_ASSET_MODELS',
        source_after_sha256=before, fixture_input_sha256=input_pins, component_fixtures=results,
        header_fixture_sha256=headers['executable_sha256'], audio_fixture_sha256=audio['executable_sha256'],
        video_corpus_fixture_sha256=corpus['executable_sha256'], real_header_assets=29, unchanged_video_index_assets=38,
        audio_oracle=oracle, symbol_audits=audits, native_archive_sha256=sha(archive), archive_audit=archive_audit,
        cache_after_sha256=sha(cache_path), original_assets=asset_pins,
        limits=['Source-software coefficient model and original-byte asset codec checks, not original hardware/cache parity',
            'Actual NativeMovieDecoder AVPacket/extradata/timestamp wiring and global mode/sink/speaker/timing/player remain OPEN',
            'No game, real audio/GPU/context, audible samples, local model, drivers/settings or backup modification']))
    print(json.dumps(dict(status='PASS_SCOPED_NATIVE_ADX_COMPOSITION_AND_REAL_ASSET_MODELS',
        header_assets=29, unchanged_video_assets=38, audio_oracle=oracle.get('status'), native_archive_sha256=sha(archive))))
