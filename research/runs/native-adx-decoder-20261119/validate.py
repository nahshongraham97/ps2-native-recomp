"""Actual native ADX decoder boundary: file/CPU fixtures only, no devices/game."""
import ctypes, datetime, hashlib, importlib.util, json, os, pathlib, re, subprocess

FORK = pathlib.Path(__file__).resolve().parents[3]
PARENT = FORK.parents[2]
WORKSPACE = PARENT.parent
COMMON = FORK / 'research/runs/native-movie-kernel-policies-20261119/validate.py'
OUT = pathlib.Path(__file__).resolve().parent / 'validation-01'
LIBS = WORKSPACE / 'dependencies/blender/blender-4.5.15-windows-x64/blender.shared'
ASSETS = FORK / 'games/mkarmageddon/disc-files/MOVIEPS2'
SOURCE = PARENT / 'research/runs/native-movie-ingress-20261119'
spec = importlib.util.spec_from_file_location('_adx_decoder_receipts', COMMON)
common = importlib.util.module_from_spec(spec)
spec.loader.exec_module(common)
require, sha, write = common.require, common.sha, common.write


def file_sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


if __name__ == '__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    common.OUT = OUT
    common.ENV = dict(os.environ, PATH=str(common.BIN) + os.pathsep + os.environ['PATH'])
    OUT.mkdir(parents=True, exist_ok=False)
    cache_path = common.BUILD / 'CMakeCache.txt'
    cache = cache_path.read_text(encoding='utf-8-sig')
    require(bool(re.search(r'^PS2_GENERATED:PATH=$', cache, re.M)), 'no generated game')
    require(bool(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$', cache, re.M)), 'Release')
    extra = ['runtime/src/native/movie_decode.cpp', 'runtime/src/native/movie_source.cpp',
        'runtime/src/native/movie_assets.cpp', 'runtime/src/native/native_file.cpp',
        'runtime/src/native/movie_pes_index.cpp', 'runtime/include/mka/movie_adx_blocks.hpp',
        'runtime/src/native/movie_adx_blocks.cpp', 'runtime/include/mka/movie_adx_header.hpp',
        'runtime/src/native/movie_adx_header.cpp', 'runtime/include/mka/movie_adx_stream.hpp',
        'runtime/src/native/movie_adx_stream.cpp', 'runtime/include/mka/movie_adx_profile.hpp',
        'runtime/src/native/movie_adx_profile.cpp', 'tests/test_movie_decode_adx.cpp',
        'tests/test_movie_audio_demux_probe.cpp', 'tests/native_adx_test_observer.hpp',
        'tests/data/movie_native_adx_source_oracle.hpp', 'tests/data/movie_decode_oracle.hpp',
        'tests/native_test_sha256.hpp', 'tests/test_movie_decode.cpp', 'tests/test_movie_source.cpp',
        'tests/test_movie_source_association.cpp', 'tests/test_native_movie_inventory.cpp',
        'tests/test_native_movie_pipeline.cpp']
    names = list(dict.fromkeys(common.NAMES + extra))
    certificate = FORK / 'research/runs/native-adx-composition-20261119/validation-02/audio-oracle.json'
    original_receipt = certificate.with_name('real-audio.json')
    contract = SOURCE / 'MOVIE_ADX_HEADER_NATIVE_BINDING_CONTRACT.json'
    paths = [*[FORK / x for x in names], COMMON, certificate, original_receipt, contract,
        SOURCE / 'MOVIE_NATIVE_ADX_DECODER_REVIEW01_CORRECTIONS.json',
        SOURCE / 'MOVIE_AUDIO_DEMUX_PROBE_REVIEW01_CORRECTION.json',
        SOURCE / 'MOVIE_NATIVE_ADX_DECODER_REVIEW02_CORRECTIONS.json',
        SOURCE / 'MOVIE_AUDIO_DEMUX_PROBE_METADATA_ARTIFACTS.json',
        PARENT / 'tools/research/verify_native_adx_decoder.py',
        pathlib.Path(__file__).resolve()]
    before = {str(path): sha(path) for path in paths}
    for path in paths:
        if path.suffix in ['.cpp', '.hpp', '.py', '.cmake', '.inc']:
            require(b'\r\r' not in path.read_bytes(), 'no doubled CR source')
    for name, digest in common.PRESERVE.items():
        require(sha(FORK / name) == digest, 'preserved IOP source')
    cert = json.loads(certificate.read_bytes())
    require(cert['status'] == 'PASS_ORIGINAL_ASSET_NATIVE_ADX_CORPUS' and
        sha(original_receipt) == cert['execution_receipt_sha256'] ==
        '0f85cce649803a742ddab928fc1383bea0b82d700cc2d01086cf8e383a058422',
        'original-source audio certificate/receipt')
    records = json.loads(contract.read_bytes())['assets']
    require(len(records) == 38 and len({a['key'] for a in records}) == 38, 'source corpus38')
    assets = {}
    for item in records:
        path = ASSETS / item['key']
        stat = path.stat()
        require(stat.st_size == item['source_bytes'] and file_sha(path) == item['source_sha256'],
            'full original input ' + item['key'])
        assets[item['key']] = dict(path=str(path), sha256=item['source_sha256'],
            bytes=stat.st_size, mtime_ns=stat.st_mtime_ns)
    libraries = {str(LIBS / name): sha(LIBS / name)
        for name in ['avutil-59.dll', 'avcodec-61.dll', 'avformat-61.dll']}
    manifest = OUT / 'assets.tsv'
    with manifest.open('x', encoding='utf-8', newline='\n') as output:
        output.write(''.join(x['key'] + '\t' + x['key'] + '\n' for x in records))
    write('provenance.json', dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        fork_head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=FORK, text=True).strip(),
        parent_head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=PARENT, text=True).strip(),
        source_before_sha256=before, original_assets=assets, library_before_sha256=libraries,
        manifest_sha256=sha(manifest), preserved_dirty_sha256=common.PRESERVE,
        compiler_path=str(common.BIN / 'clang++.exe'), compiler_sha256=sha(common.BIN / 'clang++.exe'),
        cache_before_sha256=sha(cache_path),
        scope='Actual native ADX decode plus independent demux; file/CPU only, no game/devices/models'))
    targets = ['test_native_movie_audio_demux_probe', 'test_native_movie_decode_adx',
        'test_native_movie_decode', 'test_native_movie_source', 'test_native_movie_source_association',
        'test_native_movie_inventory', 'test_native_movie_pipeline', 'mka_native_media']
    common.execute('build.json', [common.CMAKE, '--build', common.BUILD, '--target', *targets, '-j2'])
    runs = {}
    for name, target in [('probe', targets[0]), ('decoder', targets[1])]:
        print(json.dumps(dict(stage=name, scope='file/CPU only')), flush=True)
        args = [common.BUILD / (target + '.exe'), LIBS, ASSETS, manifest]
        if name == 'probe':
            args.append(OUT / 'probe-packets')
        runs[name] = common.execute(name + '.json', args)
    require(before == {str(path): sha(path) for path in paths}, 'source stable through measurement')
    require(libraries == {x: sha(pathlib.Path(x)) for x in libraries}, 'DLL bytes stable')
    for item in assets.values():
        path = pathlib.Path(item['path']); stat = path.stat()
        require(stat.st_size == item['bytes'] and stat.st_mtime_ns == item['mtime_ns'] and
            file_sha(path) == item['sha256'], 'original input stable after measurement')
    for name, digest in common.PRESERVE.items():
        require(sha(FORK / name) == digest, 'preserved IOP after measurement')
    write('measurement.json', dict(status='EXECUTED_ONLY_AWAITING_INDEPENDENT_COMPARISON',
        source_after_sha256=before, libraries_after_sha256=libraries,
        targets={k: v['executable_sha256'] for k, v in runs.items()},
        limits=['No independent comparison or regression execution credited here',
            'Native archive and device-bearing pipeline compiled only; not executed',
            'No game/device/context/audible sample/model/driver/settings action']))
    print('MEASURED decoder and independent demux; comparison and regressions remain pending', flush=True)
