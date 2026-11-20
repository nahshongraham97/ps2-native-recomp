"""Affected owned-source/error regressions; CPU/files only, no endpoints."""
import ctypes, importlib.util, json, os, pathlib

FORK = pathlib.Path(__file__).resolve().parents[3]
PARENT = FORK.parents[2]
BASE = pathlib.Path(__file__).resolve().parent/'validation-01'
OUT = BASE/'regressions-01'
COMMON = FORK/'research/runs/native-movie-kernel-policies-20261119/validate.py'
spec = importlib.util.spec_from_file_location('_source_ingress_regressions', COMMON)
common = importlib.util.module_from_spec(spec); spec.loader.exec_module(common)


if __name__ == '__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    OUT.mkdir(exist_ok=False)
    common.OUT = OUT
    common.ENV = dict(os.environ, PATH=str(common.BIN)+os.pathsep+os.environ['PATH'])
    require, sha, write = common.require, common.sha, common.write
    verification = json.loads((BASE/'verification.json').read_bytes())
    require(verification['status'] == 'PASS_SCOPED_NATIVE_MOVIE_SOURCE_OBSERVATIONS', 'preceding scoped run')
    before = verification['source_after_sha256'].copy()
    require(before == {name: sha(name) for name in before}, 'same measured ingress sources and DLLs')
    tests = ['tests/test_movie_source_failure.cpp', 'tests/test_movie_decode.cpp', 'tests/test_movie_source.cpp']
    for path in [*[FORK/x for x in tests], pathlib.Path(__file__).resolve(), BASE/'verification.json']:
        before[str(path)] = sha(path)
    libs = PARENT.parent/'dependencies/blender/blender-4.5.15-windows-x64/blender.shared'
    assets = FORK/'games/mkarmageddon/disc-files/MOVIEPS2'
    manifest = FORK/'research/runs/native-movie-source-20261119/validation-01/assets.tsv'
    rows = [line.split('\t') for line in manifest.read_text().splitlines()]
    require(len(rows) == 38 and all(len(row) == 4 for row in rows), 'actual four-column factory manifest')
    write('provenance.json', dict(source_before_sha256=before,
        manifest_sha256=sha(manifest), scope='Affected terminal-error/decoder/source ownership only; no endpoints'))
    targets = ['test_native_movie_source_failure', 'test_native_movie_decode', 'test_native_movie_source']
    common.execute('build.json', [common.CMAKE, '--build', common.BUILD, '--target', *targets, '-j2'])
    runs = []
    for label, target, args, footer in [
        ('terminal-source-failure', targets[0], [libs, assets/'LOGON.SFD'],
         'PASS native decoder both-open source rejection and terminal truncation; no output/race-authentication claim'),
        ('decoder-owned-catalog', targets[1], [libs, assets, OUT/'decode-owned', '--catalog'],
         'PASS native filesystem/MPEG2/ADX decoder ownership and actual media output; no game/A-V-sync/presentation/fidelity claim'),
        ('retail-source-factory', targets[2], [libs, assets, manifest, OUT/'source-owned'], None)]:
        print('RUN '+label, flush=True)
        result = common.execute(label+'.json', [common.BUILD/(target+'.exe'), *args])
        if footer:
            require(result['stdout'].splitlines()[-1] == footer, 'complete affected regression footer')
        else:
            summary = json.loads(result['stdout'].splitlines()[-1])
            require(summary['kind'] == 'complete' and summary['assets'] == 38 and summary['full_goldens'] == 3 and
                summary['first_video_only'] == 35 and summary['missing_requests'] == 4, 'complete source factory assertions')
        audit = common.symbols(label+'-symbols', common.BUILD/(target+'.exe'), result['executable_sha256'],
            ['mka::video::NativeMovieDecoder::'], True)
        runs.append(dict(component=label, executed_target_sha256=result['executable_sha256'], audit=audit))
    require(before == {name: sha(name) for name in before}, 'stable ingress sources/DLLs and regression fixtures')
    for name, digest in common.PRESERVE.items():
        require(sha(FORK/name) == digest, 'preserved dirty IOP')
    write('verification.json', dict(status='PASS_NATIVE_MOVIE_INGRESS_AFFECTED_SOURCE_REGRESSIONS',
        source_after_sha256=before, fixtures=runs,
        limits=['CPU/file fixtures only; source ownership/error/first-frame and three full-media goldens',
                'No playback adapters or endpoints activated; no game/context/model/server/driver/settings action']))
    print('PASS affected native movie source error/ownership/factory regressions', flush=True)
