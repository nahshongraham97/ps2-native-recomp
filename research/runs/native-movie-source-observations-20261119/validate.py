"""Native movie source observations: CPU/file fixtures, no device or game."""
import ctypes, datetime, importlib.util, json, os, pathlib, re, sys

FORK = pathlib.Path(__file__).resolve().parents[3]
PARENT = FORK.parents[2]
WORKSPACE = PARENT.parent
OUT = pathlib.Path(__file__).resolve().parent / 'validation-01'
COMMON = FORK/'research/runs/native-movie-kernel-policies-20261119/validate.py'
spec = importlib.util.spec_from_file_location('_source_observation_receipts', COMMON)
common = importlib.util.module_from_spec(spec); spec.loader.exec_module(common)
ASSETS = FORK/'games/mkarmageddon/disc-files/MOVIEPS2'
LIBS = WORKSPACE/'dependencies/blender/blender-4.5.15-windows-x64/blender.shared'
EVIDENCE = PARENT/'research/runs/native-movie-ingress-20261119'


if __name__ == '__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    common.OUT = OUT
    common.ENV = dict(os.environ, PATH=str(common.BIN)+os.pathsep+os.environ['PATH'])
    OUT.mkdir(parents=True, exist_ok=False)
    require, sha, write = common.require, common.sha, common.write
    proof = json.loads((EVIDENCE/'MOVIE_NATIVE_SOURCE_OBSERVATION_INGRESS_IMPLEMENTATION.json').read_bytes())
    for name, digest in proof['source_sha256'].items():
        require(sha(name) == digest, 'frozen ingress source')
    require(sha(PARENT/'tools/research/verify_movie_source_observations.py') ==
        '38936f9e14603218e59e424a3b5b95cfbf6d1c1e6e0153b5e20b1331d3974c3c', 'reviewed independent checker')
    files = list((FORK/'runtime/include/mka').glob('*.hpp'))
    files += list((FORK/'runtime/src/native').rglob('*.cpp'))
    files += list((FORK/'runtime/src/native').rglob('*.inc'))
    files += [FORK/x for x in common.NAMES]
    files += [FORK/x for x in ['tests/test_movie_source_observations.cpp', 'tests/test_movie_prefix_events.cpp',
        'tests/test_movie_program_end.cpp', 'tests/native_test_sha256.hpp', 'tests/native_adx_test_observer.hpp',
        'tests/data/movie_native_adx_source_oracle.hpp', 'tests/data/movie_decode_oracle.hpp', 'cmake/native_audio.cmake']]
    files += [pathlib.Path(x) for x in proof['source_sha256']]
    files += [COMMON, pathlib.Path(__file__).resolve(),
        PARENT/'tools/research/verify_movie_source_observations.py',
        PARENT/'tools/research/verify_native_adx_audio_assets.py',
        EVIDENCE/'MOVIE_NATIVE_SOURCE_OBSERVATION_INGRESS_IMPLEMENTATION.json']
    files += [EVIDENCE/x for x in ['MOVIE_NATIVE_SOURCE_OBSERVATION_DECODER_SOURCE_REVIEW.json',
        'MOVIE_NATIVE_SOURCE_OBSERVATION_POLICY_SOURCE_REVIEW.json',
        'MOVIE_NATIVE_SOURCE_OBSERVATION_ORACLE_SOURCE_REVIEW.json',
        'MOVIE_NATIVE_PREFIX_EVENTS_SOURCE_REVIEW.json', 'MOVIE_NATIVE_PROGRAM_END_SOURCE_REVIEW.json']]
    files += list(LIBS.glob('avcodec-*.dll'))+list(LIBS.glob('avformat-*.dll'))+list(LIBS.glob('avutil-*.dll'))
    original = PARENT/'research/inputs/modules/MOVIER.ovl'
    require(sha(original) == '3a34e3838042aa0bde0ce687a4e61e128b2eb965e60e65c34713e0a73891adb8', 'original table source')
    files.append(original)
    files = list(dict.fromkeys(files))
    before = {str(x): sha(x) for x in files}
    for path in files:
        if path.suffix in ['.cpp', '.hpp', '.py', '.cmake', '.inc']:
            require(b'\r\r' not in path.read_bytes(), 'no doubled CR source')
    for name, digest in common.PRESERVE.items():
        require(sha(FORK/name) == digest, 'preserved dirty IOP')
    cache_path = common.BUILD/'CMakeCache.txt'
    cache = cache_path.read_text(encoding='utf-8-sig')
    require(re.search(r'^PS2_GENERATED:PATH=$', cache, re.M) and
        re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$', cache, re.M), 'native-only Release')
    # This original saved fixture manifest has FOUR columns. The new corpus
    # fixtures require TWO; derive those columns explicitly, never reinterpret.
    saved = FORK/'research/runs/native-movie-source-20261119/validation-01/assets.tsv'
    rows = [x.split('\t') for x in saved.read_text().splitlines()]
    require(len(rows) == 38 and all(len(x) == 4 for x in rows) and len({x[0] for x in rows}) == 38,
        'original38 four-column source manifest')
    catalog = OUT/'assets.tsv'
    with catalog.open('x', encoding='utf-8', newline='\n') as output:
        output.write(''.join(row[0]+'\t'+row[0]+'\n' for row in rows))
    before[str(saved)] = sha(saved); before[str(catalog)] = sha(catalog)
    write('provenance.json', dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        source_before_sha256=before, compiler_path=str(common.BIN/'clang++.exe'),
        compiler_sha256=sha(common.BIN/'clang++.exe'), cache_before_sha256=sha(cache_path),
        scope='Native raw movie source ingress; CPU/file only; no header admission or device/game'))
    targets = ['test_native_movie_prefix_events', 'test_native_movie_program_end',
        'test_native_movie_source_observations', 'test_native_movie_kernel_policies',
        'test_native_movie_pcm_binding', 'test_native_movie_playback', 'mka_native_media']
    common.execute('build.json', [common.CMAKE, '--build', common.BUILD, '--target', *targets, '-j2'])
    runs = []
    for label, target, args, footer, required in [
        ('prefix-events', targets[0], [],
         'PASS native raw ES prefix events streaming/classes/ownership; no header admission or timeline binding',
         ['mka::video::NativeMoviePrefixEvents::']),
        ('program-end', targets[1], [ASSETS, catalog],
         'PASS native program-end grammar/owned observations; no lifecycle completion',
         ['mka::video::NativeMoviePesIndex::program_end']),
        ('kernel-policies', targets[3], [original],
         'PASS actual kernel policies with explicit fixture inputs; no retail profile/timeline admission',
         ['mka::video::NativeMovieKernelPolicies::observe_source']),
        ('pcm-regression', targets[4], [],
         'PASS explicit immutable PCM binding; no retail profile admission or playback',
         ['mka::video::NativeMoviePcmBinding::']),
        ('stream-regression', targets[5], [original],
         'PASS deterministic playback composition fakes/controller/owned queues; no device or retail policy binding',
         ['mka::video::NativeMovieStreamPlayback::']),
        ('actual-source-observations', targets[2], [LIBS, ASSETS, catalog],
         'PASS native source observations38 raw prefixes/output/program-end; no header admission or timeline binding',
         ['mka::video::NativeMovieDecoder::observation_identity', 'mka::video::NativeMoviePrefixEvents::'])]:
        print('RUN '+label, flush=True)
        result = common.execute(label+'.json', [common.BUILD/(target+'.exe'), *args])
        require(result['stdout'].splitlines()[-1] == footer, 'complete affected fixture assertions')
        audit = common.symbols(label+'-symbols', common.BUILD/(target+'.exe'), result['executable_sha256'], required, True)
        runs.append(dict(component=label, executed_target_sha256=result['executable_sha256'], audit=audit))
    print('RUN independent source comparison and negative checks', flush=True)
    common.execute('comparison-execution.json', [sys.executable, '-O',
        PARENT/'tools/research/verify_movie_source_observations.py',
        OUT/'actual-source-observations.json', ASSETS, OUT/'source-oracle.json'])
    oracle = json.loads((OUT/'source-oracle.json').read_bytes())
    archive = common.BUILD/'libmka_native_media.a'
    archive_audit = common.symbols('archive-symbols', archive, sha(archive),
        ['mka::video::NativeMovieKernelPolicies::observe_source',
         'mka::video::NativeMovieDecoder::observation_identity', 'mka::video::NativeMoviePrefixEvents::'], False)
    require(before == {name: sha(name) for name in before}, 'source/reviews/DLLs stable during verification')
    for name, digest in common.PRESERVE.items():
        require(sha(FORK/name) == digest, 'preserved IOP after verification')
    write('verification.json', dict(status='PASS_SCOPED_NATIVE_MOVIE_SOURCE_OBSERVATIONS',
        source_after_sha256=before, fixtures=runs, source_oracle_sha256=sha(OUT/'source-oracle.json'),
        native_archive_sha256=sha(archive), archive_audit=archive_audit,
        cache_after_sha256=sha(cache_path), limits=[
            'Actual raw source prefix/PES/B9/PCM comparisons, deterministic required context/endpoints only',
            'Retail header admission/GOP serial/SDK position/player clocks/mode/speaker/completion remain open',
            'Only3 historical full YUV goldens;35 full YUV hashes remain observations',
            'Archive compiled/inspected only; no real device/context/game/audio/model/server/driver/settings action']))
    print(json.dumps(dict(status='PASS_SCOPED_NATIVE_MOVIE_SOURCE_OBSERVATIONS',
        fixtures=len(runs), source_pins=len(before), oracle_status=oracle['status'])), flush=True)
