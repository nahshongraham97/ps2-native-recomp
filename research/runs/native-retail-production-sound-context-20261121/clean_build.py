"""Prepare an isolated clean checkout and record its native build configuration."""
from pathlib import Path
import json
import os
import subprocess
import time

record = Path(__file__).resolve().parent
fork = record.parents[2]
parent = fork.parents[2]
dependencies = parent.parent/'dependencies'
source = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=fork, text=True).strip()
assert source == 'c0e5d1cf155c8f88660e2afba77179118f375ee6'
clean = fork.parent/'ps2-native-recomp-clean-context-21'
assert not clean.exists()
subprocess.run(['git', '-c', 'core.autocrlf=false', '-c', 'core.longpaths=true', 'worktree', 'add', '--detach', str(clean), source], cwd=fork, check=True)
assert not subprocess.check_output(['git', '-c', 'core.longpaths=true', 'status', '--porcelain', '-uno'], cwd=clean, text=True).strip()
compiler = dependencies/'compiler/llvm-mingw-20261006-ucrt-x86_64/bin'
environment = dict(os.environ)
environment['PATH'] = str(compiler)+os.pathsep+environment['PATH']
cmake = dependencies/'cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe'
build = clean/'build-native'
argv = [str(cmake), '-S', str(clean), '-B', str(build), '-G', 'Ninja',
    '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_CXX_COMPILER='+str(compiler/'clang++.exe'),
    '-DCMAKE_MAKE_PROGRAM='+str(dependencies/'ninja/ninja.exe'),
    '-DSDL2_DIR='+str(fork/'third_party/sdl2/lib/cmake/SDL2'),
    '-DMKA_NATIVE_MEDIA_HEADERS='+str(dependencies/'native-media/ffmpeg-7.1.4')]
start = time.monotonic()
p = subprocess.run(argv, capture_output=True, text=True, cwd=clean, env=environment,
    creationflags=subprocess.CREATE_NO_WINDOW, timeout=300)
(record/'CLEAN_CONFIGURE.json').write_text(json.dumps(dict(argv=argv, exit=p.returncode,
    stdout=p.stdout, stderr=p.stderr, seconds=time.monotonic()-start, source_commit=source,
    clean_checkout=str(clean), native_endpoints_opened=False), indent=2)+'\n', newline='\n')
print('clean configure', p.returncode, flush=True)
if p.returncode:
    print(p.stdout[-3000:], p.stderr[-3000:])
assert p.returncode == 0
subprocess.run(['python', str(record/'verify.py'), '--checkout', str(clean), '--build', str(build),
    '--output', str(record/'clean-validation-01')], cwd=fork, check=True, env=environment)
