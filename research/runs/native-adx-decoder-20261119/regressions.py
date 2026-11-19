"""Rebuilt CPU/file decoder/source/association/terminal-error regressions only."""
import ctypes, datetime, importlib.util, json, os, pathlib, sys

FORK = pathlib.Path(__file__).resolve().parents[3]
PARENT = FORK.parents[2]
OUT = pathlib.Path(__file__).resolve().parent / 'validation-01'
COMMON = FORK / 'research/runs/native-movie-kernel-policies-20261119/validate.py'
spec = importlib.util.spec_from_file_location('_native_adx_regressions', COMMON)
common = importlib.util.module_from_spec(spec)
spec.loader.exec_module(common)

if __name__ == '__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    common.OUT = OUT
    common.ENV = dict(os.environ, PATH=str(common.BIN) + os.pathsep + os.environ['PATH'])
    require, sha, write = common.require, common.sha, common.write
    libs = PARENT.parent / 'dependencies/blender/blender-4.5.15-windows-x64/blender.shared'
    assets = FORK / 'games/mkarmageddon/disc-files/MOVIEPS2'
    measurement = json.loads((OUT / 'measurement.json').read_bytes())
    # The comparator changed after this measurement but was never executed by
    # that runner. Every compiled library/test source must retain its identity.
    checker = PARENT / 'tools/research/verify_native_adx_decoder.py'
    paths = [pathlib.Path(x) for x in measurement['source_after_sha256'] if pathlib.Path(x) != checker]
    paths += [FORK/'tests/test_movie_source_failure.cpp', pathlib.Path(__file__).resolve()]
    before = {str(x): sha(x) for x in paths}
    for name, digest in measurement['source_after_sha256'].items():
        if pathlib.Path(name) != checker:
            require(before[name] == digest, 'same measured compiled source')
    for name, digest in measurement['libraries_after_sha256'].items():
        require(sha(name) == digest, 'same measured DLL')
    write('regression-provenance.json', dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        source_before_sha256=before, measurement_sha256=sha(OUT/'measurement.json'),
        scope='Affected CPU/file fixtures only; device pipeline compiled only, no game/endpoints'))
    common.execute('source-failure-build.json', [common.CMAKE, '--build', common.BUILD,
        '--target', 'test_native_movie_source_failure', '-j2'])
    for label, target, arguments in [
        ('terminal-source-failure', 'test_native_movie_source_failure', [libs, assets/'LOGON.SFD']),
        ('decoder-owned-catalog', 'test_native_movie_decode', [libs, assets, OUT/'decode-owned', '--catalog']),
        ('retail-source-factory', 'test_native_movie_source', [libs, assets, OUT/'assets.tsv', OUT/'source-owned']),
        ('full-picture-association', 'test_native_movie_source_association', [libs, assets])]:
        print(json.dumps(dict(stage=label, scope='file/CPU only')), flush=True)
        run = common.execute(label+'.json', [common.BUILD/(target+'.exe'), *arguments])
        require(bool(run['stdout'].strip()), 'complete fixture output')
    require(before == {str(x): sha(pathlib.Path(x)) for x in before}, 'regression sources stable')
    for name, digest in measurement['libraries_after_sha256'].items():
        require(sha(name) == digest, 'regression DLLs stable')
    for name, digest in common.PRESERVE.items():
        require(sha(FORK/name) == digest, 'dirty IOP preserved')
    write('regression-results.json', dict(status='PASS_REBUILT_CPU_FILE_DECODER_SOURCE_ASSOCIATION_ERROR_FIXTURES',
        source_after_sha256=before,
        executed=['terminal-source-failure', 'decoder-owned-catalog', 'retail-source-factory', 'full-picture-association'],
        limits=['Fixtures assert exact video and independently certified native PCM expectations',
            'Read complete fixture receipts for assertion scope; this is not complete movie/game acceptance',
            'Inventory and device-bearing pipeline were compiled, not executed',
            'No real audio/GPU/context, audible samples, game, local model, driver or settings action']))
    print('PASS rebuilt CPU/file decoder/source/association/terminal-error regressions', flush=True)
