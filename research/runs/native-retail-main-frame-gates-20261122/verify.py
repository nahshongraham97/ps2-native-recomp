"""Check three connected Main frame gates on actual native owners."""
from pathlib import Path
import argparse
import gzip
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
assert output.is_relative_to(record);output.mkdir(exist_ok=False)
def git(*argv):return subprocess.check_output(['git','-c','core.longpaths=true',*argv],cwd=fork,text=True).strip()
def sha(path):
    result=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda:stream.read(1024*1024),b''):result.update(chunk)
    return result.hexdigest()
head,status=git('rev-parse','HEAD'),git('status','--porcelain','-uno')
names=git('ls-files','runtime','recomp','cmake','tests','CMakeLists.txt').splitlines()
pins={name:sha(fork/name) for name in names};assert len(pins)==845
(output/'SOURCE_FILES.json').write_text(json.dumps(pins,indent=2)+'\n',newline='\n')
changed=list(json.loads((record/'BEFORE.json').read_text())['pins'])
for name in changed:assert b'\r' not in (fork/name).read_bytes(),name
subprocess.run(['git','diff','--check','--',*changed],cwd=fork,check=True)
source=json.loads((record/'SOURCE_JOIN.json').read_text())
assert source['status']=='PASS_ORIGINAL_MAIN_BRANCHES_AND_WORK_BUFFER_BODIES'
assert sha(parent/'private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/SLUS_214.10')==source['elf_sha256']
for function in source['functions']:assert sha(fork/function['path'])==function['sha256']
corpus=fork/'research/runs/parallel-sound-metadata-resolver-20261122/expected-metadata.tsv'
expected=json.loads((record/'REQUIRED_EFFECTS_EXPECTED.json').read_text())

dependencies=parent.parent/'dependencies';compiler=dependencies/'compiler/llvm-mingw-20261006-ucrt-x86_64/bin'
cmake=dependencies/'cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe'
environment=dict(os.environ);environment['PATH']=str(compiler)+os.pathsep+environment['PATH']
def run(name,argv,expected=0):
    argv=list(map(str,argv));started=time.monotonic()
    p=subprocess.run(argv,cwd=fork,capture_output=True,env=environment,creationflags=subprocess.CREATE_NO_WINDOW,timeout=300)
    raw=p.stdout+p.stderr;packed=output/(name+'.log.gz');packed.write_bytes(gzip.compress(raw,mtime=0))
    assert gzip.decompress(packed.read_bytes())==raw
    value=dict(argv=argv,exit=p.returncode,seconds=time.monotonic()-started,source_commit=head,tracked_status=status,
        source_manifest_sha256=sha(output/'SOURCE_FILES.json'),executable_sha256=sha(argv[0]),
        log_sha256=hashlib.sha256(raw).hexdigest(),compressed_log_sha256=sha(packed),
        game_run=False,visible_window=False,physical_endpoints_opened=False,frames=[],audio=[])
    (output/(name+'.json')).write_text(json.dumps(value,indent=2)+'\n',newline='\n')
    print(name,p.returncode,flush=True)
    if expected is not None and p.returncode!=expected:print(raw.decode(errors='replace')[-3200:],flush=True)
    if expected is not None:assert p.returncode==expected,name
    return value,raw.decode(errors='replace')
assets=parent/'private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon'
asset_paths=[assets/'sndsps2/MSLASSET.MS2',assets/'sndsps2/MSLASSET.MS4']
asset_pins={str(path):sha(path) for path in asset_paths}
empty=output/'empty-content';empty.mkdir()
plans=[('test_native_retail_main_frame_gates',[empty]),('test_native_retail_task_domain',[]),
       ('test_native_retail_game_session',[]),('test_native_retail_camera_bootstrap',[empty]),
       ('test_native_retail_sound_context',[assets]),('test_native_retail_initial_reset',[empty])]
run('build',[cmake,'--build',build,'--target',*(name for name,_ in plans),'-j2'])
for target in ['test_native_retail_main_frame_gates','test_native_retail_camera_bootstrap']:
    _,symbols=run(target+'-symbols',[compiler/'llvm-nm.exe','-C',build/(target+'.exe')])
    assert re.search(r'\bT SDL_InitSubSystem\b',symbols) and '__imp_SDL_InitSubSystem' not in symbols
    assert not re.search(r'(?<![A-Za-z0-9_])(?:ps2|gs|spu2|sif|Cpu|recomp)::|\b(?:g_ram|g_spr)\b|SDL_OpenAudio',symbols)
    if target=='test_native_retail_main_frame_gates':
        for name in ['main_binding::retail_frame_processing_inhibited(', 'main_binding::retail_ordinary_counter_inhibited(',
                     'main_binding::retail_render_enabled(', 'NativeRetailTaskDomain::root_wait_inhibited(']:assert name in symbols,name
_,sound_symbols=run('sound-symbols',[compiler/'llvm-nm.exe',build/'test_native_retail_sound_context.exe'])
for name in ['__wrap__ZN3mka5sound29open_native_game_sound_outputERNS0_18AudioDeviceFailureE',
             '__wrap__ZN3mka5sound17NativeAudioDevice4openERKNS0_17AudioDeviceConfigERNS0_18AudioDeviceFailureE']:assert name in sound_symbols
clients=[]
for name,arguments in plans:
    result,_=run(name,[build/(name+'.exe'),*arguments]);clients.append(dict(name=name,sha256=result['executable_sha256']))
assert asset_pins=={str(path):sha(path) for path in asset_paths}
negative,log=run('full-entry-negative',[cmake,'--build',build,'--target','test_native_startup_missing_bindings','-j2'],None)
missing=re.findall(r'undefined symbol: ([^\r\n]+)',log)
assert negative['exit']!=0 and len(missing)==len(set(missing))==34
assert sorted(missing)==sorted(expected['remaining_undefined_symbols'])
assert not (build/'test_native_startup_missing_bindings.exe').exists()
assert head==git('rev-parse','HEAD') and status==git('status','--porcelain','-uno')
assert pins=={name:sha(fork/name) for name in pins}
configuration={}
for line in (build/'CMakeCache.txt').read_text().splitlines():
    if line and not line.startswith(('#','//')) and '=' in line:
        key,value=line.split('=',1)
        if key.startswith(('CMAKE_BUILD_TYPE:','CMAKE_CXX_COMPILER:','CMAKE_CXX_FLAGS','PS2_GENERATED:')):configuration[key]=value
assert configuration['CMAKE_BUILD_TYPE:STRING']=='Release' and configuration['PS2_GENERATED:PATH']==''
receipt=dict(status='PASS_CONNECTED_NATIVE_MAIN_FRAME_GATES',source_commit=head,clean_source=not status,tracked_status=status,
    configuration=configuration,compiler_sha256=sha(compiler/'clang++.exe'),cache_sha256=sha(build/'CMakeCache.txt'),
    source_manifest_sha256=sha(output/'SOURCE_FILES.json'),clients=clients,remaining_undefined_symbols=missing,
    assets_before_and_after=asset_pins,source_join_sha256=sha(record/'SOURCE_JOIN.json'),
    game_run=False,physical_endpoints_opened=False,frames=[],audio=[],acceptance='OPEN',
    limits=['Three Main read gates only; nonzero bit5/root-wait producers remain unimplemented and not injected',
            'Actual current-world null/nonnull and suspension distinction executed using explicit world identity fixture',
            'Renamed frame work-buffer implementation, renderer/input/scene/audio/full-game gates remain required'])
(output/'VERIFICATION.json').write_text(json.dumps(receipt,indent=2)+'\n',newline='\n')
print(receipt['status'],len(clients),'affected clients; exact34 required effects after three Main reads and one classification rename',flush=True)
