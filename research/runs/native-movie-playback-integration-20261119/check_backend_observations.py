"""Affected native body/service observations/production ingress only."""
import ctypes, datetime, importlib.util, json, os, pathlib, re, sys
FORK = pathlib.Path(__file__).resolve().parents[3]
PARENT = FORK.parents[2]
COMMON = FORK / 'research/runs/native-movie-kernel-policies-20261119/validate.py'
spec = importlib.util.spec_from_file_location('_backend_observation_receipts', COMMON)
common = importlib.util.module_from_spec(spec)
spec.loader.exec_module(common)

if __name__ == '__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    if len(sys.argv) != 2 or not re.fullmatch(r'validation-backend-observations-[0-9]+', sys.argv[1]):
        raise SystemExit('usage: check_backend_observations.py validation-backend-observations-NN')
    out = pathlib.Path(__file__).resolve().parent / sys.argv[1]
    common.OUT = out
    common.ENV = dict(os.environ, PATH=str(common.BIN) + os.pathsep + os.environ['PATH'])
    require, sha, write = common.require, common.sha, common.write
    evidence = PARENT / 'research/runs/native-movie-ingress-20261119'
    libraries = PARENT.parent / 'dependencies/blender/blender-4.5.15-windows-x64/blender.shared'
    assets = FORK / 'games/mkarmageddon/disc-files/MOVIEPS2'
    asset = assets / 'KRYPT/V_RAIDON.SFD'
    overlay = PARENT / 'research/inputs/modules/MOVIER.ovl'
    oracle = FORK / 'research/runs/native-adx-composition-20261119/validation-02/audio-oracle.json'
    receipt = oracle.with_name('real-audio.json')
    certified = json.loads(oracle.read_bytes())
    require(sha(oracle) == '46ed75bf100ca151b94d05698e5c8080f4df87b80fb5373f87960f1dfa32bd8c', 'frozen source certificate bytes')
    require(certified['status'] == 'PASS_ORIGINAL_ASSET_NATIVE_ADX_CORPUS', 'frozen source certificate status')
    source_receipt = json.loads(receipt.read_bytes())
    require(certified['execution_receipt_sha256'] == sha(receipt) and source_receipt['exit'] == 0 and
        certified['executable_sha256'] == source_receipt['executable_sha256'] == source_receipt['executable_after_sha256'], 'historical source certificate receipt join')
    require(certified['source_pins'][str(asset)] == sha(asset), 'single actual source matches frozen asset identity')
    rows = [json.loads(x) for x in json.loads(receipt.read_bytes())['stdout'].splitlines() if x.startswith('{')]
    matching = [x for x in rows if x['key'] == 'KRYPT/V_RAIDON.SFD']
    require(len(matching) == 1 and matching[0]['audio_streams'] == 0, 'actual video-only source; no audio endpoint')
    files = list((FORK / 'runtime/include/mka').glob('*.hpp'))
    files += list((FORK / 'runtime/src/native').rglob('*.cpp'))
    files += list((FORK / 'runtime/src/native').rglob('*.inc'))
    files += [FORK / x for x in ['CMakeLists.txt', 'cmake/native_media.cmake', 'cmake/native_audio.cmake',
        'tests/test_movie_admission_feed.cpp', 'tests/test_movie_service.cpp', 'tests/test_movie_service_acquisition.cpp']]
    files += [COMMON, pathlib.Path(__file__).resolve(), common.BIN / 'clang++.exe',
        overlay, asset, oracle, receipt]
    files += [libraries / n for n in ['avcodec-61.dll', 'avformat-61.dll', 'avutil-59.dll']]
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
        source_before_sha256=before, scope='Three affected native body/service snapshot/production-source ingress fixtures; no decoder corpus or endpoints'))
    try:
        targets = ['test_native_movie_admission_feed', 'test_native_movie_service', 'test_native_movie_service_acquisition']
        common.execute('build.json', [common.CMAKE, '--build', common.BUILD, '--target', *targets, '-j2'])
        fixtures = []
        for label, target, args, footer, symbol in [
            ('feed', targets[0], [], 'PASS native admission feed current headers/explicit parse entries/work candidates; no business admission or decoder execution', 'NativeMovieAdmissionFeed::picture_body_extent'),
            ('service', targets[1], [overlay], 'PASS native movie service concrete session/kernel/context lifecycle; no retail coordinator or profile admission', 'NativeMovieService::'),
            ('acquisition', targets[2], [libraries, assets, 'KRYPT/V_RAIDON.SFD', '12', overlay], 'PASS native service acquisition associated video-only Session; no endpoint activation or playback', 'NativeMovieServiceAcquisition::')]:
            print('RUN ' + label, flush=True)
            exe = common.BUILD / (target + '.exe')
            result = common.execute(label + '.json', [exe, *args])
            require(result['stdout'].splitlines()[-1] == footer, 'complete ' + label + ' assertions')
            audit = common.symbols(label + '-symbols', exe, result['executable_sha256'], ['mka::video::' + symbol], True)
            fixtures.append(dict(component=label, executed_target_sha256=result['executable_sha256'], audit=audit))
        require(before == {name: sha(name) for name in before}, 'stable native source/asset/library pins')
        cache_contract()
        for name, digest in common.PRESERVE.items():
            require(sha(FORK / name) == digest, 'preserved IOP after fixtures')
        write('verification.json', dict(status='PASS_NATIVE_BACKEND_OBSERVATIONS_BODY_INGRESS',
            source_after_sha256=before, fixtures=fixtures,
            limits=['Actual native Session queue/clock/drain observations and non-consuming data extents; one actual-source header/body ingress; no presentation or complete playback',
                'No offline decoder/corpus/golden rerun; decoder checkpoint b6d93925 unchanged',
                'No physical audio/GPU endpoint, game, local inference/server or driver/settings action',
                'Native game/host settings/profile and backend work/clock/finish producers remain open']))
        print(json.dumps(dict(status='PASS_NATIVE_BACKEND_OBSERVATIONS_BODY_INGRESS', fixtures=len(fixtures), pins=len(before))), flush=True)
    except Exception as error:
        write('aborted.json', dict(status='BACKEND_OBSERVATIONS_BODY_CHECK_FAILED', error=str(error)))
        raise
