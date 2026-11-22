"""Record changed connected native sound-state checks; never run a game."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import subprocess
import time

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--checkout',type=Path,required=True)
parser.add_argument('--build',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
record=Path(__file__).resolve().parent
fork=args.checkout.resolve()
parent=Path(__file__).resolve().parents[6]
output=args.output.resolve()
assert output.is_relative_to(record)
output.mkdir(exist_ok=False)

def git(*argv):
    return subprocess.check_output(['git',*argv],cwd=fork,text=True).strip()

paths=['runtime/include/mka/retail_main.hpp','runtime/include/mka/retail_current_sound_required.hpp',
       'runtime/src/native/retail_main_state.cpp','runtime/src/native/retail_sound_bank_main.cpp',
       'tests/test_retail_reset_sound.cpp']
pins={p:hashlib.sha256((fork/p).read_bytes()).hexdigest() for p in paths}
head=git('rev-parse','HEAD')
status=git('status','--porcelain','--untracked-files=no')
provenance=dict(source_commit=head,tracked_status=status,source_sha256=pins,
                platform='native Windows',shell_route='regular Windows PowerShell child; no WSL',
                build=str(args.build.resolve()),game_run=False,frames=[],audio=[])
(output/'PROVENANCE.json').write_text(json.dumps(provenance,indent=2)+'\n',newline='\n')
dependencies=parent.parent/'dependencies'
compiler=dependencies/'compiler/llvm-mingw-20261006-ucrt-x86_64/bin'
environment=dict(os.environ)
environment['PATH']=str(compiler)+os.pathsep+environment['PATH']
cmake=dependencies/'cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe'
build=args.build.resolve()

def run(name,command,expected=0):
    command=list(map(str,command))
    started=time.monotonic()
    p=subprocess.run(command,cwd=fork,capture_output=True,text=True,env=environment,
                     creationflags=subprocess.CREATE_NO_WINDOW,timeout=300)
    value=dict(argv=command,exit=p.returncode,stdout=p.stdout,stderr=p.stderr,
               seconds=time.monotonic()-started,source_commit=head,source_sha256=pins,
               game_run=False,physical_endpoints_opened=False,frames=[],audio=[])
    binary=Path(command[0])
    if binary.is_file():value['executable_sha256']=hashlib.sha256(binary.read_bytes()).hexdigest()
    (output/(name+'.json')).write_text(json.dumps(value,indent=2)+'\n',newline='\n')
    print(name,p.returncode,flush=True)
    print(p.stdout[-1800:],p.stderr[-400:],flush=True)
    if expected is not None:assert p.returncode == expected,name
    return value

targets=['test_native_retail_reset_sound','test_native_retail_initial_reset',
         'test_native_retail_initial_task','test_native_retail_startup',
         'test_native_retail_file_startup','test_native_retail_middleware_main']
run('build',[cmake,'--build',build,'--target',*targets,'-j2'])
assets=parent/'private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon'
empty=output/'empty-content'
empty.mkdir()
results=[]
for target in targets:
    extra=[assets,output/'negative-content'] if target==targets[0] else [empty]
    if target=='test_native_retail_startup':extra=[output/'startup-fixtures']
    if target=='test_native_retail_file_startup':extra=[output]
    results.append(run(target,[build/(target+'.exe'),*extra]))

gate=run('full-entry-negative-link',[cmake,'--build',build,'--target','test_native_startup_missing_bindings','-j2'],None)
missing=re.findall(r'undefined symbol: ([^\n]+)',gate['stdout']+gate['stderr'])
prior=json.loads((record.parent/'native-retail-reset-sound-binding-20261121/caller-validation-01/full-entry-negative-link.json').read_text())
expected=[s for s in prior['undefined_symbols'] if 'prepare_game_sound_state_12d7e0' not in s]
assert len(expected)==39 and len(missing)==39 and sorted(missing)==sorted(expected)
assert gate['exit'] != 0 and not (build/'test_native_startup_missing_bindings.exe').exists()
assert git('rev-parse','HEAD')==head and git('status','--porcelain','--untracked-files=no')==status
assert pins=={p:hashlib.sha256((fork/p).read_bytes()).hexdigest() for p in paths}
result=dict(status='PASS_CONNECTED_NATIVE_GAME_SOUND_PREPARATION',source_commit=head,
            clean_source=not status,source_sha256=pins,clients=[t for t in targets],
            binary_sha256=[r['executable_sha256'] for r in results],
            cache_sha256=hashlib.sha256((build/'CMakeCache.txt').read_bytes()).hexdigest(),
            remaining_undefined_symbols=missing,game_run=False,frames=[],audio=[],
            physical_endpoints_opened=False,
            limits=['Two context/program children and four other reset effects remain fixture-only in connected bank test',
                    'Populated reference producers/retirement, real context configuration and gameplay remain OPEN'])
(output/'VERIFICATION.json').write_text(json.dumps(result,indent=2)+'\n',newline='\n')
print(result['status'],flush=True)
