"""Native grouped admission/audio-output-time/descriptor integration; decoder fidelity frozen."""
import ctypes, datetime, importlib.util, json, os, pathlib, re, sys
FORK = pathlib.Path(__file__).resolve().parents[3]
PARENT = FORK.parents[2]
COMMON = FORK / 'research/runs/native-movie-kernel-policies-20261119/validate.py'
spec = importlib.util.spec_from_file_location('_acceptance_clock_receipts', COMMON)
common = importlib.util.module_from_spec(spec)
spec.loader.exec_module(common)

if __name__ == '__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    if len(sys.argv) != 2 or not re.fullmatch(r'validation-outer-audio-descriptor-[0-9]+', sys.argv[1]):
        raise SystemExit('usage: check_outer_audio_descriptor.py validation-outer-audio-descriptor-NN')
    out = pathlib.Path(__file__).resolve().parent / sys.argv[1]
    common.OUT = out
    common.ENV = dict(os.environ, PATH=str(common.BIN) + os.pathsep + os.environ['PATH'])
    require, sha, write = common.require, common.sha, common.write
    libraries = PARENT.parent / 'dependencies/blender/blender-4.5.15-windows-x64/blender.shared'
    assets = FORK / 'games/mkarmageddon/disc-files/MOVIEPS2'
    asset = assets / 'KRYPT/V_RAIDON.SFD'
    overlay = PARENT / 'research/inputs/modules/MOVIER.ovl'
    oracle = FORK / 'research/runs/native-adx-composition-20261119/validation-02/audio-oracle.json'
    receipt = oracle.with_name('real-audio.json')
    certified = json.loads(oracle.read_bytes())
    require(sha(oracle) == '46ed75bf100ca151b94d05698e5c8080f4df87b80fb5373f87960f1dfa32bd8c', 'frozen source certificate bytes')
    require(certified['status'] == 'PASS_ORIGINAL_ASSET_NATIVE_ADX_CORPUS', 'historical source certificate status')
    original = json.loads(receipt.read_bytes())
    require(certified['execution_receipt_sha256'] == sha(receipt) and original['exit'] == 0 and
        certified['executable_sha256'] == original['executable_sha256'] == original['executable_after_sha256'], 'historical receipt identity')
    require(certified['source_pins'][str(asset)] == sha(asset), 'same one original asset')
    rows = [json.loads(x) for x in original['stdout'].splitlines() if x.startswith('{')]
    require(len([x for x in rows if x['key'] == 'KRYPT/V_RAIDON.SFD' and x['audio_streams'] == 0]) == 1, 'explicit video-only source')
    tests = ['movie_audio_clock_anchor','movie_descriptor_rewrite','movie_timestamp_association','movie_outer_header_batch',
        'movie_playback','movie_service','movie_service_host_clock','movie_native_context','movie_kernel_policies',
        'movie_admission_feed','movie_service_acquisition','movie_frame_work']
    files = list((FORK / 'runtime/include/mka').glob('*.hpp'))
    files += list((FORK / 'runtime/src/native').rglob('*.cpp'))
    files += list((FORK / 'runtime/src/native').rglob('*.inc'))
    files += [FORK / ('tests/test_' + x + '.cpp') for x in tests]
    files += [FORK / 'CMakeLists.txt',
        FORK / 'cmake/native_media.cmake', FORK / 'cmake/native_audio.cmake',
        COMMON, pathlib.Path(__file__).resolve(), common.BIN / 'clang++.exe', overlay, asset, oracle, receipt]
    files += [libraries / n for n in ['avcodec-61.dll', 'avformat-61.dll', 'avutil-59.dll']]
    files = list(dict.fromkeys(files))
    before = {str(x): sha(x) for x in files}
    for name, digest in common.PRESERVE.items():
        require(sha(FORK / name) == digest, 'preserved IOP')
    def cache_contract():
        text = (common.BUILD / 'CMakeCache.txt').read_text(encoding='utf-8-sig')
        require(re.search(r'^PS2_GENERATED:PATH=$', text, re.M) and
            re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$', text, re.M), 'native-only Release')
    cache_contract()
    out.mkdir(exist_ok=False)
    write('provenance.json', dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        source_before_sha256=before, scope='New native grouped-header/audio-output-time/descriptor and adjusted timestamp integration; no media content/corpus verification'))
    try:
        cases = [
            ('anchor','test_native_movie_audio_clock_anchor',[],'PASS native output clock zero anchor/lifecycle/exact rational; no acoustic or retail clock claim','NativeMovieAudioClockAnchor::'),
            ('descriptor','test_native_movie_descriptor_rewrite',[],'PASS native descriptor rewrite cache and baseline literals; no retail admission or clock selection','NativeMovieDescriptorRewrite::'),
            ('timestamp','test_native_movie_timestamp_association',[],'PASS native source timestamp association origin/interpolation; no selected-picture or playback clock claim','NativeMovieTimestampAssociation::'),
            ('outer','test_native_movie_outer_header_batch',[libraries,assets,'KRYPT/V_RAIDON.SFD'],'PASS genuine outer parse batch admission/partial scope/rate provenance; no retail driver or playback','NativeMovieAdmissionFeed::'),
            ('playback','test_native_movie_playback',[overlay],'PASS deterministic playback composition fakes/controller/owned queues; no device or retail policy binding','NativeMovieStreamPlayback::'),
            ('service','test_native_movie_service',[overlay],'PASS native movie service concrete session/kernel/context lifecycle; no retail coordinator or profile admission','NativeMovieService::'),
            ('timing','test_native_movie_service_host_clock',[overlay],'PASS native service host timing binding cumulative elapsed/explicit audio; no master-policy or output anchor admission','NativeMovieServiceHostClock::'),
            ('context','test_native_movie_native_context',[],'PASS concrete source-owned movie context kernels/lifecycle; no default profile or native device/player admission','NativeMoviePlaybackContext::'),
            ('kernel','test_native_movie_kernel_policies',[overlay],'PASS actual kernel policies with explicit fixture inputs; no retail profile/timeline admission','NativeMovieKernelPolicies::'),
            ('feed','test_native_movie_admission_feed',[],'PASS native admission feed current headers/explicit parse entries/work candidates; no business admission or decoder execution','NativeMovieAdmissionFeed::'),
            ('acquisition','test_native_movie_service_acquisition',[libraries,assets,'KRYPT/V_RAIDON.SFD','12',overlay],'PASS native service acquisition associated video-only Session; no endpoint activation or playback','NativeMovieServiceAcquisition::'),
            ('work','test_native_movie_frame_work',[libraries,assets,'KRYPT/V_RAIDON.SFD'],'PASS actual native frame work source identities/outer scope/delayed selection/flush; no playback or content comparison','NativeMovieFrameWorkCoordinator::')]
        common.execute('build.json', [common.CMAKE, '--build', common.BUILD, '--target',
            *[x[1] for x in cases], '-j2'])
        fixtures = []
        for label, target, args, footer, symbol in cases:
            print('RUN ' + label, flush=True)
            exe = common.BUILD / (target + '.exe')
            result = common.execute(label + '.json', [exe, *args])
            require(result['stdout'].splitlines()[-1] == footer, 'complete ' + label + ' assertions')
            audit = common.symbols(label + '-symbols', exe, result['executable_sha256'], ['mka::video::' + symbol], True)
            fixtures.append(dict(component=label, executed_target_sha256=result['executable_sha256'], audit=audit))
        require(before == {name: sha(name) for name in before}, 'stable source/asset/library pins')
        cache_contract()
        for name, digest in common.PRESERVE.items():
            require(sha(FORK / name) == digest, 'preserved IOP after fixtures')
        write('verification.json', dict(status='PASS_NATIVE_GROUPED_ADMISSION_OUTPUT_TIME_DESCRIPTOR_INTEGRATION',
            source_after_sha256=before, fixtures=fixtures,
            limits=['Actual original first-read grouped header/admission plus synthetic malformed metadata protocol negatives',
                'Production Service fixture exercises actual Stream-owned audio-time delivery through deterministic output ports; no physical endpoint or acoustic claim',
                'One original video-only source work-protocol regression; no decoded content comparison',
                'Descriptor/timestamp literals verify source state composition, not complete retail control/profile/native output',
                'Offline decoder algorithms/content/corpus/golden verification frozen',
                'No physical GPU/audio endpoint, game, local model/server or driver/settings action']))
        print(json.dumps(dict(status='PASS_NATIVE_GROUPED_ADMISSION_OUTPUT_TIME_DESCRIPTOR_INTEGRATION', fixtures=len(fixtures), pins=len(before))), flush=True)
    except Exception as error:
        write('aborted.json', dict(status='NATIVE_GROUPED_ADMISSION_OUTPUT_TIME_DESCRIPTOR_CHECK_FAILED', error=str(error)))
        raise
