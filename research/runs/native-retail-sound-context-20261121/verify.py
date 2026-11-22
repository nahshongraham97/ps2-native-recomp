"""Verify native bank/playback wiring without a game/window/audio endpoint."""
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
canonical=record.parents[2]
parent=canonical.parents[2]
fork=args.checkout.resolve()
build=args.build.resolve()
output=args.output.resolve()
assert output.is_relative_to(record)
output.mkdir(exist_ok=False)

def git(*argv):
    return subprocess.check_output(['git',*argv],cwd=fork,text=True).strip()

head=git('rev-parse','HEAD')
status=git('status','--porcelain','--untracked-files=no')
paths=['runtime/include/mka/audio_banks.hpp','runtime/include/mka/playback_owner.hpp',
       'runtime/src/native/playback_owner.cpp','tests/test_native_bank_playback.cpp',
       'tests/test_retail_reset_sound.cpp','cmake/native_sound_resources.cmake']
pins={p:hashlib.sha256((fork/p).read_bytes()).hexdigest() for p in paths}
dependencies=parent.parent/'dependencies'
compiler=dependencies/'compiler/llvm-mingw-20261006-ucrt-x86_64/bin'
environment=dict(os.environ)
environment['PATH']=str(compiler)+os.pathsep+environment['PATH']
cmake=dependencies/'cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe'

def run(name,command,expected=0):
    command=list(map(str,command))
    started=time.monotonic()
    p=subprocess.run(command,cwd=fork,capture_output=True,text=True,env=environment,
                     creationflags=subprocess.CREATE_NO_WINDOW,timeout=300)
    value=dict(argv=command,exit=p.returncode,stdout=p.stdout,stderr=p.stderr,
               seconds=time.monotonic()-started,source_commit=head,source_sha256=pins,
               tracked_status=status,game_run=False,physical_endpoints_opened=False,frames=[],audio=[])
    binary=Path(command[0])
    if binary.is_file():value['executable_sha256']=hashlib.sha256(binary.read_bytes()).hexdigest()
    (output/(name+'.json')).write_text(json.dumps(value,indent=2)+'\n',newline='\n')
    print(name,p.returncode,flush=True)
    if p.returncode:print(p.stdout[-2500:],p.stderr[-500:],flush=True)
    else:print(p.stdout[-500:],flush=True)
    if expected is not None:assert p.returncode==expected,name
    return value

sound=['test_native_bank_playback','test_native_playback_owner','test_native_playback_reservation',
       'test_native_playback_reservation_gaps','test_native_default_playback_runtime',
       'test_native_program_output_activation','test_native_forced_output_retirement',
       'test_native_forced_output_retirement_failure']
main=['test_native_retail_reset_sound','test_native_retail_initial_reset','test_native_retail_initial_task',
      'test_native_retail_startup','test_native_retail_file_startup','test_native_retail_middleware_main']
run('build',[cmake,'--build',build,'--target',*sound,*main,'-j2'])
assets=parent/'private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon'
empty=output/'empty-content'
empty.mkdir()
plans=[(target,[assets/'sndsps2/mslasset.ms2']) for target in sound]
plans += [(main[0],[assets,output/'negative-content']), (main[1],[empty]), (main[2],[empty]),
          (main[3],[output/'startup-fixtures']), (main[4],[output]), (main[5],[empty])]
results=[]
for target,extra in plans:results.append(run(target,[build/(target+'.exe'),*extra]))

symbols=run('native-client-symbols',[compiler/'llvm-nm.exe',build/(sound[0]+'.exe')])
expected_wraps=['__wrap__ZN3mka5sound17AudioBankProtocol6createEONS0_16AudioBankSessionERNS0_20AudioProtocolFailureE',
                '__wrap__ZN3mka5sound16AudioBankService6createEONS0_17AudioBankProtocolERNS0_18BankServiceFailureE',
                '__wrap__ZN3mka5sound23decode_audio_bank_frameEPKhyRNS0_20AudioProtocolFailureE']
assert all(name in symbols['stdout'] for name in expected_wraps)
gate=run('full-entry-negative-link',[cmake,'--build',build,'--target','test_native_startup_missing_bindings','-j2'],None)
missing=re.findall(r'undefined symbol: ([^\n]+)',gate['stdout']+gate['stderr'])
prior=json.loads((record.parent/'native-retail-sound-state-20261121/clean-validation-01/full-entry-negative-link.json').read_text())
previous=re.findall(r'undefined symbol: ([^\n]+)',prior['stdout']+prior['stderr'])
assert gate['exit'] != 0 and len(missing)==len(previous)==39 and sorted(missing)==sorted(previous)
assert not (build/'test_native_startup_missing_bindings.exe').exists()

configuration={}
for line in (build/'CMakeCache.txt').read_text().splitlines():
    if line and not line.startswith(('#','//')) and '=' in line:
        key,value=line.split('=',1)
        if key.startswith(('CMAKE_BUILD_TYPE:','CMAKE_CXX_COMPILER:','CMAKE_CXX_FLAGS',
                           'CMAKE_EXE_LINKER_FLAGS','PS2','MKA','BUILD_')):configuration[key]=value
assert configuration['CMAKE_BUILD_TYPE:STRING']=='Release'
assert git('rev-parse','HEAD')==head and git('status','--porcelain','--untracked-files=no')==status
assert pins=={p:hashlib.sha256((fork/p).read_bytes()).hexdigest() for p in paths}
value=dict(status='PASS_NATIVE_BANK_PLAYBACK_AND_CONNECTED_RESET',source_commit=head,source_sha256=pins,
           clean_source=not status,configuration=configuration,
           cache_sha256=hashlib.sha256((build/'CMakeCache.txt').read_bytes()).hexdigest(),
           compiler_sha256=hashlib.sha256((compiler/'clang++.exe').read_bytes()).hexdigest(),
           clients=[dict(name=t,sha256=r['executable_sha256']) for (t,_),r in zip(plans,results)],
           compatibility_abort_guard_symbols=expected_wraps,remaining_undefined_symbols=missing,
           game_run=False,physical_endpoints_opened=False,frames=[],audio=[],
           limits=['Context creation/update remain fixture effects in connected client; retail gain/output policy not inferred',
                   'Full native-only boot/gameplay/input/render/audio acceptance remains OPEN'])
(output/'VERIFICATION.json').write_text(json.dumps(value,indent=2)+'\n',newline='\n')
print(value['status'],flush=True)
