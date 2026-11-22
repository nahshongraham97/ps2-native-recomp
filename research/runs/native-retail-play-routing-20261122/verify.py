"""Compile and check the native Main sound context/worker without endpoints."""
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
fork,build,output=(p.resolve() for p in (args.checkout,args.build,args.output))
assert output.is_relative_to(record)
output.mkdir(exist_ok=False)
def git(*args):return subprocess.check_output(['git','-c','core.longpaths=true',*args],cwd=fork,text=True).strip()
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
head,status=git('rev-parse','HEAD'),git('status','--porcelain','-uno')
added=[]
paths=sorted(set(git('ls-files','runtime','recomp','cmake','tests','CMakeLists.txt').splitlines()+added))
pins={p:digest(fork/p) for p in paths if (fork/p).is_file()}
(output/'SOURCE_FILES.json').write_text(json.dumps(pins,indent=2)+'\n',newline='\n')
changed=list(json.loads((record/'BEFORE.json').read_text())['pins'])
for p in changed:assert b'\r' not in (fork/p).read_bytes(),p
subprocess.run(['git','diff','--check','--',*changed],cwd=fork,check=True)
dependencies=parent.parent/'dependencies'
compiler=dependencies/'compiler/llvm-mingw-20261006-ucrt-x86_64/bin'
cmake=dependencies/'cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe'
environment=dict(os.environ)
environment['PATH']=str(compiler)+os.pathsep+environment['PATH']
def run(name,command,expected=0):
    command=list(map(str,command));started=time.monotonic()
    p=subprocess.run(command,cwd=fork,capture_output=True,text=True,env=environment,
        creationflags=subprocess.CREATE_NO_WINDOW,timeout=300)
    value=dict(argv=command,exit=p.returncode,stdout=p.stdout,stderr=p.stderr,
        seconds=time.monotonic()-started,source_commit=head,tracked_status=status,
        source_manifest_sha256=digest(output/'SOURCE_FILES.json'),game_run=False,
        physical_endpoints_opened=False,frames=[],audio=[])
    executable=Path(command[0])
    if executable.is_file():value['executable_sha256']=digest(executable)
    (output/(name+'.json')).write_text(json.dumps(value,indent=2)+'\n',newline='\n')
    print(name,p.returncode,flush=True)
    print(p.stdout[-2000:] if p.returncode else p.stdout[-450:],flush=True)
    if p.returncode:print(p.stderr[-1600:],flush=True)
    if expected is not None:assert p.returncode==expected,name
    return value

sound=['test_native_held_playback','test_native_sound_curves','test_native_host_voice','test_native_program_copy','test_native_program_copy_failure',
    'test_native_bank_playback','test_native_playback_owner','test_native_playback_reservation',
    'test_native_playback_reservation_gaps','test_native_default_playback_runtime','test_native_program_output_activation',
    'test_native_forced_output_retirement','test_native_forced_output_retirement_failure']
main=['test_native_retail_reset_sound','test_native_retail_initial_reset','test_native_retail_initial_task',
    'test_native_retail_startup','test_native_retail_file_startup','test_native_retail_middleware_main']
extra=['test_native_runtime_program_work','test_native_runtime_preparation','test_native_sound_output_plan','test_native_retail_sound_context','test_native_program_host_clock','test_native_audio_device']
run('build',[cmake,'--build',build,'--target',*extra,*sound,*main,'-j2'])
symbols=run('context-symbols',[compiler/'llvm-nm.exe',build/'test_native_retail_sound_context.exe'])
guards=['__wrap__ZN3mka5sound29open_native_game_sound_outputERNS0_18AudioDeviceFailureE',
        '__wrap__ZN3mka5sound17NativeAudioDevice4openERKNS0_17AudioDeviceConfigERNS0_18AudioDeviceFailureE']
guards += ['__wrap__ZN3mka5sound18RuntimeProgramWork5buildERNS0_18PreparationFailureE','__wrap__Znwy','__wrap__ZN3mka5sound17NativeProgramCopyD1Ev']
assert all(s in symbols['stdout'] for s in guards)
assets=parent/'private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon'
empty=output/'empty-content';empty.mkdir()
plans=[(extra[0],[assets/'sndsps2/mslasset.ms2']),(extra[1],[assets/'sndsps2/mslasset.ms2']),
       (extra[2],[]),(extra[3],[assets]),(extra[4],[]),(extra[5],[])]
plans += [(target,[assets/'sndsps2/mslasset.ms2']) for target in sound]
plans += [(main[0],[assets,output/'negative-content']),(main[1],[empty]),(main[2],[empty]),
    (main[3],[output/'startup-fixtures']),(main[4],[output]),(main[5],[empty])]
clients=[]
for name,arguments in plans:
    result=run(name,[build/(name+'.exe'),*arguments]);clients.append(dict(name=name,sha256=result['executable_sha256']))
gate=run('full-entry-negative-link',[cmake,'--build',build,'--target','test_native_startup_missing_bindings','-j2'],None)
missing=re.findall(r'undefined symbol: ([^\n]+)',gate['stdout']+gate['stderr'])
prior=json.loads((record.parent/'native-retail-online-stage-20261122/validation-01/VERIFICATION.json').read_text())
previous=prior['remaining_undefined_symbols']
removed=[]
assert len(previous)==37
assert gate['exit']!=0 and len(missing)==37 and sorted(missing)==sorted(previous)
assert not (build/'test_native_startup_missing_bindings.exe').exists()
assert git('rev-parse','HEAD')==head and git('status','--porcelain','-uno')==status
assert pins=={p:digest(fork/p) for p in pins}
configuration={}
for line in (build/'CMakeCache.txt').read_text().splitlines():
    if line and not line.startswith(('#','//')) and '=' in line:
        key,value=line.split('=',1)
        if key.startswith(('CMAKE_BUILD_TYPE:','CMAKE_CXX_COMPILER:','CMAKE_CXX_FLAGS',
            'CMAKE_EXE_LINKER_FLAGS','PS2','MKA','BUILD_')):configuration[key]=value
assert configuration['CMAKE_BUILD_TYPE:STRING']=='Release'
value=dict(status='PASS_NATIVE_RETAIL_DIRECT_WORKER_COMPONENTS',source_commit=head,clean_source=not status,
    tracked_status=status,configuration=configuration,compiler_sha256=digest(compiler/'clang++.exe'),
    cache_sha256=digest(build/'CMakeCache.txt'),source_manifest_sha256=digest(output/'SOURCE_FILES.json'),
    clients=clients,physical_output_abort_guard_symbols=guards,removed_required_effects=removed,
    remaining_undefined_symbols=missing,game_run=False,physical_endpoints_opened=False,frames=[],audio=[],
    limits=['Actual Main ready-reference/context/status/Session/QPC worker/mixer execute; physical output, allocation faults and destructor gap probe are wrapped'
            ,'Removed IOP allocator/arena fields are replaced by actual native diagnostics, not synthetic original counters',
            'Windows output selection is compiled/pure-policy tested, not a physical stream test',
            'Original direct/worker routing and fresh constructor retry verified for native normal/cache paths; semantic parameters, alternate sample implementations, controls/envelope, broad reset/mode and full game remain OPEN'])
(output/'VERIFICATION.json').write_text(json.dumps(value,indent=2)+'\n',newline='\n')
held_symbols=run('held-symbols',[compiler/'llvm-nm.exe',build/'test_native_held_playback.exe'])
assert '__wrap__Znwy' in held_symbols['stdout'] and '__wrap__ZN3mka5sound17NativeProgramCopyD1Ev' in held_symbols['stdout']
print(value['status'],len(clients),'clients; exact37 effects; no game/window/audio endpoint')
