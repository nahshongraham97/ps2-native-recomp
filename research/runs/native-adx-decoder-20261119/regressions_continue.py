"""Correct source-factory fixture input; preserve the failed first command."""
import ctypes, datetime, hashlib, importlib.util, json, os, pathlib

FORK = pathlib.Path(__file__).resolve().parents[3]
PARENT = FORK.parents[2]
BASE = pathlib.Path(__file__).resolve().parent / 'validation-01'
OUT = BASE / 'regression-corrected-input'
COMMON = FORK / 'research/runs/native-movie-kernel-policies-20261119/validate.py'
spec = importlib.util.spec_from_file_location('_adx_regression_continue', COMMON)
common = importlib.util.module_from_spec(spec); spec.loader.exec_module(common)

if __name__ == '__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    OUT.mkdir(exist_ok=False)
    common.OUT = OUT
    common.ENV = dict(os.environ, PATH=str(common.BIN) + os.pathsep + os.environ['PATH'])
    require, sha, write = common.require, common.sha, common.write
    assets = FORK / 'games/mkarmageddon/disc-files/MOVIEPS2'
    libs = PARENT.parent / 'dependencies/blender/blender-4.5.15-windows-x64/blender.shared'
    prior = json.loads((BASE/'retail-source-factory.json').read_bytes())
    require(type(prior['exit']) is int and prior['exit'] == 1 and
        prior['stderr'].strip() == 'FAIL movie source: missing manifest field', 'exact retained fixture-input failure')
    measurement = json.loads((BASE/'measurement.json').read_bytes())
    checker = PARENT/'tools/research/verify_native_adx_decoder.py'
    paths = [pathlib.Path(x) for x in measurement['source_after_sha256'] if pathlib.Path(x) != checker]
    paths += [FORK/'tests/test_movie_source_failure.cpp', pathlib.Path(__file__).resolve()]
    before = {str(x): sha(x) for x in paths}
    for name, digest in measurement['source_after_sha256'].items():
        if pathlib.Path(name) != checker:
            require(before[name] == digest, 'same measured compiled sources')
    source = PARENT/'research/runs/native-movie-ingress-20261119/MOVIE_ADX_HEADER_NATIVE_BINDING_CONTRACT.json'
    records = json.loads(source.read_bytes())['assets']
    manifest_rows = []
    for item in records:
        raw = (assets/item['key']).read_bytes()
        require(len(raw) == item['source_bytes'] and hashlib.sha256(raw).hexdigest() == item['source_sha256'], 'original source identity')
        manifest_rows.append('\t'.join([item['key'], str(len(raw)), raw[:8].hex(), raw[-8:].hex()]))
    manifest = OUT/'verified38assets.tsv'
    with manifest.open('x', encoding='utf-8', newline='\n') as output:
        output.write('\n'.join(manifest_rows)+'\n')
    write('provenance.json', dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        source_before_sha256=before, manifest_sha256=sha(manifest), original_contract_sha256=sha(source),
        retained_failure_sha256=sha(BASE/'retail-source-factory.json'),
        change='Only fixture input now matches required key/bytes/first8hex/last8hex; no parser/assertion change'))
    runs = []
    for label, target, args in [
        ('retail-source-factory', 'test_native_movie_source', [libs, assets, manifest, OUT/'source-owned']),
        ('full-picture-association', 'test_native_movie_source_association', [libs, assets])]:
        print(json.dumps(dict(stage=label, scope='file/CPU only')), flush=True)
        run = common.execute(label+'.json', [common.BUILD/(target+'.exe'), *args])
        require(bool(run['stdout'].strip()), 'complete fixture output')
        runs.append(dict(component=label, executable_sha256=run['executable_sha256']))
    require(before == {str(x): sha(pathlib.Path(x)) for x in before}, 'regression sources stable')
    for name, digest in common.PRESERVE.items():
        require(sha(FORK/name) == digest, 'preserved dirty IOP source')
    write('verification.json', dict(status='PASS_CORRECTED_INPUT_SOURCE_FACTORY_AND_FULL_PICTURE_ASSOCIATION',
        source_after_sha256=before, runs=runs, manifest_sha256=sha(manifest),
        prior_successes={name: sha(BASE/(name+'.json')) for name in ['terminal-source-failure', 'decoder-owned-catalog']},
        prior_failed_input_record_sha256=sha(BASE/'retail-source-factory.json'),
        limits=['Prior failed command remains failed; successful source result is a new command with corrected input',
            'CPU/file fixtures only; no game/context/device/audible sample/model/driver/settings action']))
    print('PASS corrected-input source factory and complete picture association', flush=True)
