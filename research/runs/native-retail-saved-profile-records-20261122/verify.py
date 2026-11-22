"""Measure the production saved-profile initialization/reset and affected callers."""
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
parser.add_argument('--reuse-others', type=Path)
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


def save(name, value):
    (output / (name + '.json')).write_text(json.dumps(value, indent=2) + '\n', newline='\n')


changed = json.loads((record / 'BEFORE.json').read_text())['paths']
names = set(git('ls-files', 'runtime', 'recomp', 'cmake', 'tests', 'CMakeLists.txt').splitlines()) | set(changed)
pins = {name: sha(fork / name) for name in sorted(names)}
assert len(pins) == 880
save('SOURCE_FILES', pins)
for name in changed:
    assert b'\r' not in (fork / name).read_bytes(), name
subprocess.run(['git', '-c', 'core.autocrlf=false', 'diff', '--check', '--', *changed], cwd=fork, check=True)
head, status = git('rev-parse', 'HEAD'), git('status', '--porcelain=v1', '-uno')
dependencies = parent.parent / 'dependencies'
compiler = dependencies / 'compiler/llvm-mingw-20261006-ucrt-x86_64/bin'
cmake = dependencies / 'cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe'
environment = dict(os.environ)
environment['PATH'] = str(compiler) + os.pathsep + environment['PATH']


def run(name, argv, expected=0):
    argv = list(map(str, argv))
    start = time.monotonic()
    result = subprocess.run(argv, cwd=fork, capture_output=True, env=environment,
                            creationflags=subprocess.CREATE_NO_WINDOW, timeout=300)
    raw = result.stdout + result.stderr
    (output / (name + '.log.gz')).write_bytes(gzip.compress(raw, mtime=0))
    receipt = dict(argv=argv, exit=result.returncode, seconds=time.monotonic() - start,
                   log_sha256=hashlib.sha256(raw).hexdigest(), source_manifest_sha256=sha(output / 'SOURCE_FILES.json'))
    if Path(argv[0]).suffix.lower() == '.exe':
        receipt['executable_sha256'] = sha(argv[0])
    save(name, receipt)
    print(name, result.returncode, flush=True)
    if expected is not None:
        if result.returncode != expected:
            print(raw.decode(errors='replace')[-5000:], flush=True)
        assert result.returncode == expected, name
    return receipt, raw.decode(errors='replace')


empty = output / 'empty-content'
empty.mkdir()
plans = [('test_native_retail_saved_profile_records', [output / 'record-fixtures']),
         ('test_native_retail_saved_profile_child', []),
         ('test_native_retail_game_session', []),
         ('test_native_retail_profile_aggregate_main', [empty]),
         ('test_native_retail_profile_input_reset', []),
         ('test_native_retail_initial_reset_arcade', [empty]),
         ('test_native_retail_main_frame_gates', [empty])]
reused = []
if args.reuse_others:
    prior_output = args.reuse_others.resolve()
    old_pins = json.loads((prior_output / 'SOURCE_FILES.json').read_text())
    assert old_pins.keys() == pins.keys()
    assert [name for name in pins if pins[name] != old_pins[name]] == ['tests/test_retail_saved_profile_records.cpp']
    old_receipt = json.loads((prior_output / 'VERIFICATION.json').read_text())
    assert old_receipt['status'] == 'PASS_NATIVE_SAVED_PROFILE_RECORDS_AND_RESET'
    assert sha(build / 'CMakeCache.txt') == old_receipt['cache_sha256']
    build_targets = ['test_native_retail_saved_profile_records']
else:
    build_targets = [name for name, _ in plans]
run('build', [cmake, '--build', build, '--target', *build_targets, '-j2'])
clients = []
for name, arguments in plans:
    before = sha(build / (name + '.exe'))
    if args.reuse_others and name != 'test_native_retail_saved_profile_records':
        receipt = json.loads((prior_output / (name + '.json')).read_text())
        assert receipt['exit'] == 0 and receipt['executable_sha256'] == before
        raw = gzip.decompress((prior_output / (name + '.log.gz')).read_bytes())
        assert hashlib.sha256(raw).hexdigest() == receipt['log_sha256']
        (output / (name + '.log.gz')).write_bytes(gzip.compress(raw, mtime=0))
        receipt = dict(receipt, reused_from=str(prior_output / (name + '.json')),
                       applicability='Only the other record-client test source changed; exact executable and log unchanged.')
        save(name, receipt)
        reused.append(name)
        print(name, 'authenticated unchanged result', flush=True)
    else:
        receipt, _ = run(name, [build / (name + '.exe'), *arguments])
    assert receipt['executable_sha256'] == before
    clients.append(dict(name=name, sha256=before))
_, symbols = run('records-symbols', [compiler / 'llvm-nm.exe', '-C', build / 'test_native_retail_saved_profile_records.exe'])
assert re.search(r'\bT SDL_InitSubSystem\b', symbols) and '__imp_SDL_InitSubSystem' not in symbols
assert not re.search(r'(?<![A-Za-z0-9_])(?:ps2|gs|spu2|sif|Cpu|recomp)::|\b(?:g_ram|g_spr)\b|SDL_OpenAudio', symbols)
for name in ['NativeRetailSavedProfileRecords::initialize_from_171b10(', 'NativeRetailSavedProfileRecords::reset_first_two_from_246070(',
             'main_binding::initialize_retail_saved_profile_records(', 'initial_reset_required::reset_saved_profile_children_246070(']:
    assert name in symbols, name

# Freeze newly exposed REQUIRED menu descendants from their actual compiled
# undefined ABI declarations before measuring the full-entry link result.
_, archive = run('production-required-menu-abi', [compiler / 'llvm-nm.exe', '-C', build / 'libmka_native_retail_startup.a'])
prior = json.loads((record.parent / 'native-retail-system-art-cpu-20261122/clean-validation-01/VERIFICATION.json').read_text())['remaining_undefined_symbols']
removed = [name for name in prior if 'main_binding::initialize_retail_saved_profile_records(' in name or 'initial_reset_required::reset_saved_profile_children_246070(' in name]
assert len(prior) == 62 and len(removed) == 2
new = []
for name in ['invoke_retail_profile_select_from_2f1630', 'observe_retail_profile_permission_from_4eee60',
             'publish_retail_profile_participant_from_189690', 'apply_retail_profile_participant_from_188720']:
    matches = set(re.findall(r'\bU (mka::retail::' + name + r'\([^\r\n]+)', archive))
    assert len(matches) == 1, (name, matches)
    new.extend(matches)
expected_names = sorted((set(prior) - set(removed)) | set(new))
assert len(expected_names) == 64
save('REQUIRED_EFFECTS_EXPECTED', dict(names=expected_names, resolved=removed, newly_exposed=new,
     scope='Diagnostic closure:62 minus2 actual bindings plus4 previously required menu descendants. Not the remaining-work inventory.'))
negative, log = run('full-entry-negative', [cmake, '--build', build, '--target', 'test_native_startup_missing_bindings', '-j2'], None)
missing = re.findall(r'undefined symbol: ([^\r\n]+)', log)
assert negative['exit'] != 0 and len(missing) == len(set(missing)) == 64
assert sorted(missing) == expected_names
assert not (build / 'test_native_startup_missing_bindings.exe').exists()
assert pins == {name: sha(fork / name) for name in pins}
configuration = {}
for line in (build / 'CMakeCache.txt').read_text().splitlines():
    if any(line.startswith(prefix) for prefix in ['CMAKE_BUILD_TYPE:', 'CMAKE_CXX_COMPILER:', 'CMAKE_CXX_FLAGS', 'PS2_GENERATED:']):
        key, value = line.split('=', 1)
        configuration[key] = value
save('VERIFICATION', dict(status='PASS_NATIVE_SAVED_PROFILE_RECORDS_AND_RESET', source_commit=head,
     clean_source=not status, tracked_status=status, source_files=880,
     source_manifest_sha256=sha(output / 'SOURCE_FILES.json'), configuration=configuration,
     compiler_sha256=sha(compiler / 'clang++.exe'), cache_sha256=sha(build / 'CMakeCache.txt'),
     clients=clients, reused_clients=reused, remaining_undefined_symbols=missing, resolved_bindings=removed,
     newly_exposed_required_effects=new, loaded_import='REQUIRED_REFUSAL',
     game_run=False, physical_endpoints_opened=False, frames=[], audio=[], acceptance='OPEN',
     limits=['Only source-created reset defaults, persistent parent/active records and reached reset effects implemented.',
             'Populated saved-child import/appearance/snapshot/save projection producers remain required.',
             'Fixture metadata/menu child callouts do not establish full producers or gameplay.']))
print('PASS seven affected headless clients and exact64 required-effect negative; full-game acceptance OPEN', flush=True)
