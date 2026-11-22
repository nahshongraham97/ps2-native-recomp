"""Verify the connected native bootstrap prefix without a game/window/endpoint."""
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
assert output.is_relative_to(record)
output.mkdir(exist_ok=False)
def git(*argv):return subprocess.check_output(['git','-c','core.longpaths=true',*argv],cwd=fork,text=True).strip()
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
head,status=git('rev-parse','HEAD'),git('status','--porcelain','-uno')
paths=git('ls-files','runtime','recomp','cmake','tests','CMakeLists.txt').splitlines()
pins={p:sha(fork/p) for p in paths}
assert len(pins)==835
(output/'SOURCE_FILES.json').write_text(json.dumps(pins,indent=2)+'\n',newline='\n')
changed=list(json.loads((record/'BEFORE.json').read_text())['pins'])
for name in changed:assert b'\r' not in (fork/name).read_bytes(),name
subprocess.run(['git','diff','--check','--',*changed],cwd=fork,check=True)
dependencies=parent.parent/'dependencies'
compiler=dependencies/'compiler/llvm-mingw-20261006-ucrt-x86_64/bin'
cmake=dependencies/'cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe'
environment=dict(os.environ);environment['PATH']=str(compiler)+os.pathsep+environment['PATH']
def run(name,argv,expected=0):
    argv=list(map(str,argv));start=time.monotonic()
    p=subprocess.run(argv,cwd=fork,capture_output=True,env=environment,creationflags=subprocess.CREATE_NO_WINDOW,timeout=300)
    raw=p.stdout+p.stderr
    packed=output/(name+'.log.gz');packed.write_bytes(gzip.compress(raw,mtime=0))
    assert gzip.decompress(packed.read_bytes())==raw
    value=dict(argv=argv,exit=p.returncode,seconds=time.monotonic()-start,
        source_commit=head,tracked_status=status,source_manifest_sha256=sha(output/'SOURCE_FILES.json'),
        log_sha256=hashlib.sha256(raw).hexdigest(),compressed_log_sha256=sha(packed),
        executable_sha256=sha(Path(argv[0])),game_run=False,visible_window=False,
        physical_endpoints_opened=False,frames=[],audio=[])
    (output/(name+'.json')).write_text(json.dumps(value,indent=2)+'\n',newline='\n')
    print(name,p.returncode,flush=True)
    if expected is not None and p.returncode!=expected:print(raw.decode(errors='replace')[-3200:],flush=True)
    if expected is not None:assert p.returncode==expected,name
    return value,raw.decode(errors='replace')
assets=parent/'private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon'
empty=output/'empty-content';empty.mkdir()
plans=[('test_native_retail_scene_bootstrap_prefix',[empty]),('test_native_retail_main_timing',[]),
       ('test_native_retail_sound_context',[assets]),('test_native_retail_reset_sound',[assets,output/'negative-content']),
       ('test_native_retail_initial_reset',[empty]),('test_native_retail_initial_task',[empty]),
       ('test_native_retail_startup',[output/'startup-fixtures']),('test_native_retail_file_startup',[output]),
       ('test_native_retail_middleware_main',[empty])]
run('build',[cmake,'--build',build,'--target',*(p[0] for p in plans),'-j2'])
_,symbols=run('prefix-symbols',[compiler/'llvm-nm.exe',build/'test_native_retail_scene_bootstrap_prefix.exe'])
assert re.search(r'\bT SDL_InitSubSystem\b',symbols) and '__imp_SDL_InitSubSystem' not in symbols
_,sound_symbols=run('sound-symbols',[compiler/'llvm-nm.exe',build/'test_native_retail_sound_context.exe'])
guards=['__wrap__ZN3mka5sound29open_native_game_sound_outputERNS0_18AudioDeviceFailureE',
        '__wrap__ZN3mka5sound17NativeAudioDevice4openERKNS0_17AudioDeviceConfigERNS0_18AudioDeviceFailureE']
assert all(value in sound_symbols for value in guards)
clients=[]
for name,arguments in plans:
    result,_=run(name,[build/(name+'.exe'),*arguments]);clients.append(dict(name=name,sha256=result['executable_sha256']))
negative,log=run('full-entry-negative',[cmake,'--build',build,'--target','test_native_startup_missing_bindings','-j2'],None)
missing=re.findall(r'undefined symbol: ([^\r\n]+)',log)
prior=json.loads((record.parent/'native-retail-play-routing-20261122/clean-validation-01/VERIFICATION.json').read_text())
old='mka::retail::main_binding::initialize_retail_post_scene_timing(mka::retail::NativeRetailMainState&, mka::retail::StartupFailure&)'
new='mka::retail::scene_bootstrap_required::continue_scene_reset_at_181a30(mka::retail::NativeRetailMainState&, mka::retail::StartupFailure&)'
expected=list(prior['remaining_undefined_symbols']);assert expected.count(old)==1 and new not in expected
expected[expected.index(old)]=new
assert negative['exit']!=0 and len(missing)==len(set(missing))==len(expected)==37
assert sorted(missing)==sorted(expected),(set(missing)-set(expected),set(expected)-set(missing))
assert not (build/'test_native_startup_missing_bindings.exe').exists()
assert git('rev-parse','HEAD')==head and git('status','--porcelain','-uno')==status
assert pins=={p:sha(fork/p) for p in pins}
configuration={}
for line in (build/'CMakeCache.txt').read_text().splitlines():
    if line and not line.startswith(('#','//')) and '=' in line:
        key,value=line.split('=',1)
        if key.startswith(('CMAKE_BUILD_TYPE:','CMAKE_CXX_COMPILER:','CMAKE_CXX_FLAGS','PS2_GENERATED:')):configuration[key]=value
assert configuration['CMAKE_BUILD_TYPE:STRING']=='Release' and configuration['PS2_GENERATED:PATH']==''
result=dict(status='PASS_CONNECTED_NATIVE_SCENE_BOOTSTRAP_PREFIX',source_commit=head,clean_source=not status,tracked_status=status,
    configuration=configuration,compiler_sha256=sha(compiler/'clang++.exe'),cache_sha256=sha(build/'CMakeCache.txt'),
    source_manifest_sha256=sha(output/'SOURCE_FILES.json'),clients=clients,remaining_undefined_symbols=missing,
    removed_required_effects=[old],added_required_effects=[new],required_suffix_fixture=True,
    source_join_receipt='../parallel-section-timing-20261122/SOURCE_JOIN.json',game_run=False,physical_endpoints_opened=False,
    frames=[],audio=[],acceptance='OPEN',limits=['Only actual phase/timing prefix is implemented; entire remaining1861B0 suffix is required',
        'No real camera/world/frame/raster or first-scene implementation is inferred from this prefix',
        'Other source/resource/audio/input/render/gameplay and same-scenario oracle gates remain OPEN'])
(output/'VERIFICATION.json').write_text(json.dumps(result,indent=2)+'\n',newline='\n')
print(result['status'],len(clients),'affected clients; exact37 effects with one required suffix substitution',flush=True)
