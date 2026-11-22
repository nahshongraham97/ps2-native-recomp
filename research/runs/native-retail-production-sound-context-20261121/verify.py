"""Build/check the changed Main sound pump without any physical endpoints."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--checkout', type=Path, required=True)
parser.add_argument('--build', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
record = Path(__file__).resolve().parent
canonical = record.parents[2]
parent = canonical.parents[2]
fork, build, output = (p.resolve() for p in (args.checkout, args.build, args.output))
assert output.is_relative_to(record)
output.mkdir(exist_ok=False)

def git(*argv):
    return subprocess.check_output(['git', *argv], cwd=fork, text=True).strip()

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

head = git('rev-parse', 'HEAD')
status = git('status', '--porcelain', '--untracked-files=no')
paths = git('ls-files', 'runtime', 'recomp', 'cmake', 'tests', 'CMakeLists.txt').splitlines()
pins = {p: digest(fork/p) for p in paths if (fork/p).is_file()}
(output/'SOURCE_FILES.json').write_text(json.dumps(pins, indent=2)+'\n', newline='\n')
changed = ['runtime/include/mka/retail_current_sound_required.hpp',
           'runtime/src/native/retail_sound_bank_main.cpp', 'tests/test_retail_reset_sound.cpp']
for path in changed:
    assert b'\r' not in (fork/path).read_bytes(), path
assert subprocess.run(['git', 'diff', '--check', '--', *changed], cwd=fork).returncode == 0
dependencies = parent.parent/'dependencies'
compiler = dependencies/'compiler/llvm-mingw-20261006-ucrt-x86_64/bin'
cmake = dependencies/'cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe'
environment = dict(os.environ)
environment['PATH'] = str(compiler)+os.pathsep+environment['PATH']

def run(name, command, expected=0):
    command = list(map(str, command))
    start = time.monotonic()
    p = subprocess.run(command, cwd=fork, capture_output=True, text=True, env=environment,
        creationflags=subprocess.CREATE_NO_WINDOW, timeout=300)
    value = dict(argv=command, exit=p.returncode, stdout=p.stdout, stderr=p.stderr,
        seconds=time.monotonic()-start, source_commit=head, tracked_status=status,
        source_manifest_sha256=digest(output/'SOURCE_FILES.json'),
        game_run=False, physical_endpoints_opened=False, frames=[], audio=[])
    binary = Path(command[0])
    if binary.is_file():
        value['executable_sha256'] = digest(binary)
    (output/(name+'.json')).write_text(json.dumps(value, indent=2)+'\n', newline='\n')
    print(name, p.returncode, flush=True)
    print(p.stdout[-1500:] if p.returncode else p.stdout[-400:], flush=True)
    if p.returncode:
        print(p.stderr[-1000:], flush=True)
    if expected is not None:
        assert p.returncode == expected, name
    return value

targets = ['test_native_retail_reset_sound', 'test_native_retail_initial_reset',
           'test_native_retail_initial_task', 'test_native_retail_middleware_main']
run('build', [cmake, '--build', build, '--target', *targets, '-j2'])
assets = parent/'private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon'
empty = output/'empty-content'
empty.mkdir()
plans = [(targets[0], [assets, output/'negative-content']),
         *[(name, [empty]) for name in targets[1:]]]
clients = []
for name, extra in plans:
    result = run(name, [build/(name+'.exe'), *extra])
    clients.append(dict(name=name, sha256=result['executable_sha256']))

gate = run('full-entry-negative-link', [cmake, '--build', build, '--target',
    'test_native_startup_missing_bindings', '-j2'], None)
missing = re.findall(r'undefined symbol: ([^\n]+)', gate['stdout']+gate['stderr'])
prior = json.loads((record.parent/'native-retail-sound-curves-20261121/clean-validation-01/full-entry-negative-link.json').read_text())
previous = re.findall(r'undefined symbol: ([^\n]+)', prior['stdout']+prior['stderr'])
assert gate['exit'] != 0 and len(missing) == len(previous) == 39
assert sorted(missing) == sorted(previous)
assert not (build/'test_native_startup_missing_bindings.exe').exists()
assert git('rev-parse', 'HEAD') == head
assert git('status', '--porcelain', '--untracked-files=no') == status
assert pins == {p: digest(fork/p) for p in pins}
configuration = {}
for line in (build/'CMakeCache.txt').read_text().splitlines():
    if line and not line.startswith(('#', '//')) and '=' in line:
        key, value = line.split('=', 1)
        if key.startswith(('CMAKE_BUILD_TYPE:', 'CMAKE_CXX_COMPILER:', 'CMAKE_CXX_FLAGS',
                           'CMAKE_EXE_LINKER_FLAGS', 'PS2', 'MKA', 'BUILD_')):
            configuration[key] = value
assert configuration['CMAKE_BUILD_TYPE:STRING'] == 'Release'
receipt = dict(status='PASS_NATIVE_SOUND_FRAME_ATTEMPT_COUNTER', source_commit=head,
    clean_source=not status, tracked_status=status, configuration=configuration, clients=clients,
    source_manifest_sha256=digest(output/'SOURCE_FILES.json'), cache_sha256=digest(build/'CMakeCache.txt'),
    compiler_sha256=digest(compiler/'clang++.exe'), remaining_undefined_symbols=missing,
    game_run=False, physical_endpoints_opened=False, frames=[], audio=[],
    limits=['Production context/status effects remain required, unimplemented',
            'Native output/envelope/scheduling/flags9 and full-game acceptance remain OPEN'])
(output/'VERIFICATION.json').write_text(json.dumps(receipt, indent=2)+'\n', newline='\n')
print(receipt['status'], len(clients), 'clients; exact39 required effects; no game/audio/window')
