"""Headless checks for actual loose system-art CPU data and existing consumers."""
from pathlib import Path
import argparse
import gzip
import hashlib
import json
import os
import re
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
for name in ['checkout', 'build', 'output']:
    parser.add_argument('--' + name, type=Path, required=True)
args = parser.parse_args()
record = Path(__file__).resolve().parent
canonical = record.parents[2]
parent = canonical.parents[2]
fork, build, output = [getattr(args, name).resolve() for name in ['checkout', 'build', 'output']]
assert output.is_relative_to(record)
output.mkdir(exist_ok=False)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def git(*args):
    return subprocess.check_output(['git', *args], cwd=fork, text=True).strip()


def save(path, value):
    path.write_text(json.dumps(value, indent=2) + '\n', newline='\n')


changed = json.loads((record / 'BEFORE.json').read_text())['paths']
names = set(git('ls-files', 'runtime', 'recomp', 'cmake', 'tests', 'CMakeLists.txt').splitlines()) | set(changed)
pins = {name: sha(fork / name) for name in sorted(names)}
assert len(pins) == 873
save(output / 'SOURCE_FILES.json', pins)
for name in changed:
    assert b'\r' not in (fork / name).read_bytes(), name
subprocess.run(['git', 'diff', '--check', '--', *changed], cwd=fork, check=True)
expected = json.loads((record.parent / 'native-retail-renderer-modules-20261122/clean-validation-01/VERIFICATION.json').read_text())['remaining_undefined_symbols']
assert len(expected) == 62 and len(set(expected)) == 62
head, status = git('rev-parse', 'HEAD'), git('status', '--porcelain=v1', '-uno')
dependencies = parent.parent / 'dependencies'
compiler = dependencies / 'compiler/llvm-mingw-20261006-ucrt-x86_64/bin'
cmake = dependencies / 'cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe'
environment = dict(os.environ)
environment['PATH'] = str(compiler) + os.pathsep + environment['PATH']


def run(name, argv, expected_exit=0):
    argv = list(map(str, argv))
    start = time.monotonic()
    result = subprocess.run(argv, cwd=fork, capture_output=True, env=environment,
                            creationflags=subprocess.CREATE_NO_WINDOW, timeout=300)
    raw = result.stdout + result.stderr
    (output / (name + '.log.gz')).write_bytes(gzip.compress(raw, mtime=0))
    value = dict(argv=argv, exit=result.returncode, seconds=time.monotonic() - start,
                 log_sha256=hashlib.sha256(raw).hexdigest(), source_manifest_sha256=sha(output / 'SOURCE_FILES.json'))
    if Path(argv[0]).suffix.lower() == '.exe':
        value['executable_sha256'] = sha(argv[0])
    save(output / (name + '.json'), value)
    print(name, result.returncode, flush=True)
    if expected_exit is not None:
        if result.returncode != expected_exit:
            print(raw.decode(errors='replace')[-4000:], flush=True)
        assert result.returncode == expected_exit, name
    return value, raw.decode(errors='replace')


asset_root = parent / 'private-inputs/native-ps2-loose-20261121'
asset = asset_root / 'art/gameart.ssf'
asset_before = sha(asset)
assert asset_before == '9900120ff8782b16e0d73ee885c8f4df6c82545b915dd10105e6694a03658a39'
font = output / 'original-font-zero.atlas'
font.write_bytes(asset.read_bytes()[0x800 + 0x3e880:0x800 + 0x3e880 + 0x8700])
historical_font = json.loads((record.parent / 'parallel-native-font-zero-20261122/validation-02/atlas-existing.json').read_text())['argv'][1]
assert sha(font) == sha(historical_font)
indexed_table = record.parent / 'native-indexed-art-program-coverage-20261120/validation-01/indexed-source.tsv'
section_table = record.parent / 'native-art-section-data-20261120/validation-01/source-0.tsv'
plans = [
    ('test_native_retail_system_art_cpu', [asset_root, output / 'disposable-errors']),
    ('test_native_font_atlas_system_art', [asset]),
    ('test_native_font_atlas', [font]),
    ('test_native_art_section_data', [output / 'disposable-section', asset_root, 'art/gameart.ssf',
                                     0, asset.stat().st_size, 1998848, 901120, section_table]),
    ('test_native_indexed_sprite_data', [output / 'decoded-owned', asset_root, 'art/gameart.ssf',
                                       0, asset.stat().st_size, 1998848, 901120, indexed_table]),
]
run('build', [cmake, '--build', build, '--target', *(name for name, _ in plans), '-j2'])
clients = []
for name, arguments in plans:
    before = sha(build / (name + '.exe'))
    result, _ = run(name, [build / (name + '.exe'), *arguments])
    assert result['executable_sha256'] == before
    clients.append(dict(name=name, sha256=before))
    _, symbols = run(name + '-symbols', [compiler / 'llvm-nm.exe', '-C', build / (name + '.exe')])
    assert not re.search(r'(?<![A-Za-z0-9_])(?:ps2|gs|spu2|sif|Cpu|recomp)::|\b(?:g_ram|g_spr)\b|SDL_OpenAudio', symbols), name
    if name == 'test_native_retail_system_art_cpu':
        assert re.search(r'\bT SDL_InitSubSystem\b', symbols) and '__imp_SDL_InitSubSystem' not in symbols
        assert 'prepare_system_art_cpu_section' in symbols and 'NativeArtSectionData::parse' in symbols

old_images = json.loads((record.parent / 'native-indexed-art-program-coverage-20261120/validation-01/verification.json').read_text())['decoded_images']
for image in old_images:
    assert sha(output / 'decoded-owned' / (image['name'] + '.rgba')) == image['sha256'], image['name']
negative, log = run('full-entry-negative', [cmake, '--build', build, '--target', 'test_native_startup_missing_bindings', '-j2'], None)
missing = re.findall(r'undefined symbol: ([^\r\n]+)', log)
assert negative['exit'] != 0 and len(missing) == len(set(missing)) == 62
assert sorted(missing) == sorted(expected)
assert not (build / 'test_native_startup_missing_bindings.exe').exists()
assert asset_before == sha(asset)
assert pins == {name: sha(fork / name) for name in pins}
configuration = {}
for line in (build / 'CMakeCache.txt').read_text().splitlines():
    if any(line.startswith(prefix) for prefix in ['CMAKE_BUILD_TYPE:', 'CMAKE_CXX_COMPILER:', 'CMAKE_CXX_FLAGS', 'PS2_GENERATED:']):
        key, value = line.split('=', 1)
        configuration[key] = value
save(output / 'VERIFICATION.json', dict(status='PASS_NATIVE_SYSTEM_ART_CPU_AND_EXISTING_CONSUMERS',
     source_commit=head, clean_source=not status, tracked_status=status, source_files=873,
     source_manifest_sha256=sha(output / 'SOURCE_FILES.json'), configuration=configuration,
     compiler_sha256=sha(compiler / 'clang++.exe'), cache_sha256=sha(build / 'CMakeCache.txt'),
     clients=clients, remaining_undefined_symbols=missing, original_asset_sha256=asset_before,
     original_font_fixture_sha256=sha(font), reference_tables={str(path): sha(path) for path in [indexed_table, section_table]},
     actual_decodes=dict(locales=5, textures=105, metrics=45, group_ranges=15),
     original_bar_images_unchanged=old_images, source_loaded=False, converted=False,
     cache_published=False, gpu_realized=False, game_run=False, physical_endpoints_opened=False,
     frames=[], audio=[], acceptance='OPEN',
     limits=['CPU-data dependency only; no Main binding closed.',
             'Actual16FC70/whole3011D0/3DE190/ordered section publication and native GPU realization remain required.',
             '62-name full-entry diagnostic is not the complete remaining-work inventory.']))
print('PASS five affected headless clients; exact62 required symbols; no game/endpoints/frames/audio', flush=True)
