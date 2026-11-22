"""Verify the connected Main label correction without starting a game or device."""
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
parser.add_argument('round')
args = parser.parse_args()
assert re.fullmatch(r'[a-z0-9-]+', args.round)
record = Path(__file__).resolve().parent
fork = record.parents[2]
parent = fork.parents[2]
output = record / args.round
output.mkdir(exist_ok=False)
build = fork / 'build_native_services_only_20261119-01'
dependencies = parent.parent / 'dependencies'
cmake = dependencies / 'cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe'
compiler = dependencies / 'compiler/llvm-mingw-20261006-ucrt-x86_64/bin/clang++.exe'
def git(*argv):
    return subprocess.check_output(['git', *argv], cwd=fork, text=True).strip()
def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
source_commit = git('rev-parse', 'HEAD')
status = git('status', '--porcelain', '-uno')
changed = ['runtime/include/mka/retail_main.hpp', 'runtime/src/native/retail_main.cpp']
for name in changed:
    assert b'\r' not in (fork/name).read_bytes(), name
subprocess.run(['git', 'diff', '--check', '--', *changed], cwd=fork, check=True)
names = git('ls-files', 'runtime', 'recomp', 'cmake', 'tests', 'CMakeLists.txt').splitlines()
source_files = {name: digest(fork/name) for name in names}
(output/'SOURCE_FILES.json').write_text(json.dumps(source_files, indent=2)+'\n', newline='\n')
environment = dict(os.environ)
environment['PATH'] = str(compiler.parent) + os.pathsep + environment['PATH']
argv = list(map(str, [cmake, '--build', build, '--target', 'test_native_startup_missing_bindings', '-j2']))
start = time.monotonic()
process = subprocess.run(argv, cwd=fork, capture_output=True, env=environment,
                         creationflags=subprocess.CREATE_NO_WINDOW, timeout=300)
log = process.stdout + process.stderr
with (output/'LINK.log.gz').open('xb') as stream:
    stream.write(gzip.compress(log, mtime=0))
assert gzip.decompress((output/'LINK.log.gz').read_bytes()) == log
raw = dict(argv=argv, exit=process.returncode, seconds=time.monotonic()-start,
           source_commit=source_commit, tracked_status=status,
           log_sha256=hashlib.sha256(log).hexdigest(), compressed_log_sha256=digest(output/'LINK.log.gz'))
(output/'LINK.json').write_text(json.dumps(raw, indent=2)+'\n', newline='\n')
previous = json.loads((record.parent/'native-retail-play-admission-20261122/clean-validation-01/VERIFICATION.json').read_text())
prior = previous['remaining_undefined_symbols']
replacements = {'read_retail_audio_boot_word': 'read_retail_online_boot_word',
                'initialize_retail_audio_stage': 'initialize_retail_online_stage'}
expected = []
for name in prior:
    for old, new in replacements.items():
        name = name.replace(old, new)
    expected.append(name)
missing = re.findall(r'undefined symbol: ([^\r\n]+)', log.decode(errors='replace'))
assert len(prior) == len(expected) == len(missing) == 37
assert len(set(missing)) == len(missing)
assert sorted(missing) == sorted(expected), (set(missing)-set(expected), set(expected)-set(missing))
assert sum(a != b for a, b in zip(prior, expected)) == 2
assert process.returncode != 0
assert not (build/'test_native_startup_missing_bindings.exe').exists()
assert git('rev-parse', 'HEAD') == source_commit and git('status', '--porcelain', '-uno') == status
assert source_files == {name: digest(fork/name) for name in names}
configuration = {}
for line in (build/'CMakeCache.txt').read_text().splitlines():
    if line and not line.startswith(('#', '//')) and '=' in line:
        key, value = line.split('=', 1)
        if key.startswith(('CMAKE_BUILD_TYPE:', 'CMAKE_CXX_COMPILER:', 'CMAKE_CXX_FLAGS', 'PS2_GENERATED:')):
            configuration[key] = value
assert configuration['CMAKE_BUILD_TYPE:STRING'] == 'Release'
assert configuration['PS2_GENERATED:PATH'] == ''
receipt = dict(status='PASS_CONNECTED_ONLINE_LABEL_NEGATIVE_LINK', source_commit=source_commit,
               clean_source=not status, tracked_status=status, configuration=configuration,
               compiler_sha256=digest(compiler), cache_sha256=digest(build/'CMakeCache.txt'),
               source_manifest_sha256=digest(output/'SOURCE_FILES.json'), changed_runtime_paths=changed,
               exact_symbol_renames=replacements, remaining_undefined_symbols=missing,
               game_executable_produced=False, game_run=False, visible_window=False,
               physical_endpoints_opened=False, frames=[], audio=[], acceptance='OPEN',
               limits=['Connected compile/name correction only; no native online implementation',
                       'Prior audio component results reused, not rerun or extended by this check'])
(output/'VERIFICATION.json').write_text(json.dumps(receipt, indent=2)+'\n', newline='\n')
print(json.dumps(dict(status=receipt['status'], clean_source=receipt['clean_source'],
                      remaining_effects=len(missing), exact_name_changes=2, game_run=False)), flush=True)
