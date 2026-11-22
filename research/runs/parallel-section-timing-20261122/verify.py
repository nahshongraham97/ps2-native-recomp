"""CPU-only connected checks; no game, window or audio endpoint is initialized."""
from pathlib import Path
import subprocess,os,json,hashlib,re,time,sys
r=Path(__file__).resolve().parent
f=r.parents[2]
d=f.parents[2].parent/'dependencies'
b=f/'build-section-timing'
o=r/sys.argv[1]
o.mkdir(exist_ok=False)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
compiler=d/'compiler/llvm-mingw-20261006-ucrt-x86_64/bin'
env=dict(os.environ,PATH=str(compiler)+os.pathsep+os.environ['PATH'])
cmake=d/'cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe'
def run(name,argv,expected=0):
    argv=list(map(str,argv));started=time.time()
    p=subprocess.run(argv,cwd=f,env=env,capture_output=True,creationflags=subprocess.CREATE_NO_WINDOW)
    (o/(name+'.log')).write_bytes(p.stdout+p.stderr)
    v=dict(argv=argv,exit=p.returncode,seconds=time.time()-started,executable_sha256=sha(Path(argv[0])),log_sha256=sha(o/(name+'.log')))
    (o/(name+'.json')).write_text(json.dumps(v,indent=2)+'\n',newline='\n')
    if expected is not None: assert p.returncode==expected,(name,p.returncode)
    return p
run('source-join',[sys.executable,r/'join.py'])
run('build',[cmake,'--build',b,'--target','test_native_retail_scene_bootstrap_prefix','test_native_retail_main_timing','-j2'])
run('prefix',[b/'test_native_retail_scene_bootstrap_prefix.exe',r])
run('stopwatch',[b/'test_native_retail_main_timing.exe'])
for name in ['test_native_retail_scene_bootstrap_prefix','test_native_retail_main_timing']:
    p=run(name+'-symbols',[compiler/'llvm-nm.exe','-C',b/(name+'.exe')])
    assert not re.search(rb'(?<![A-Za-z0-9_])(?:ps2|gs|spu2|sif|Cpu|recomp)::|\b(?:g_ram|g_spr)\b|SDL_OpenAudio',p.stdout)
p=run('full-entry-negative',[cmake,'--build',b,'--target','test_native_startup_missing_bindings','-j2'],None)
assert p.returncode!=0
missing=re.findall(r'undefined symbol: ([^\r\n]+)',(p.stdout+p.stderr).decode(errors='replace'))
prior=json.loads((r.parent/'native-retail-online-stage-20261122/validation-01/VERIFICATION.json').read_text())
expected=prior['remaining_undefined_symbols']
old='mka::retail::main_binding::initialize_retail_post_scene_timing(mka::retail::NativeRetailMainState&, mka::retail::StartupFailure&)'
new='mka::retail::scene_bootstrap_required::continue_scene_reset_at_181a30(mka::retail::NativeRetailMainState&, mka::retail::StartupFailure&)'
assert old in expected
expected=[new if x==old else x for x in expected]
assert sorted(missing)==sorted(expected) and len(missing)==len(set(missing))==37,(missing,expected)
names=subprocess.check_output(['git','ls-files','runtime','recomp','cmake','tests','CMakeLists.txt'],cwd=f,text=True).splitlines()
names+=['runtime/src/native/retail_scene_bootstrap_prefix.cpp','runtime/include/mka/retail_scene_bootstrap_required.hpp','tests/test_retail_scene_bootstrap_prefix.cpp']
manifest={n:sha(f/n) for n in sorted(set(names))}
(o/'SOURCE_FILES.json').write_text(json.dumps(manifest,indent=2)+'\n',newline='\n')
result=dict(status='PASS_CONNECTED_SCENE_BOOTSTRAP_PREFIX',base_commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=f,text=True).strip(),source_manifest_sha256=sha(o/'SOURCE_FILES.json'),compiler_sha256=sha(compiler/'clang++.exe'),cache_sha256=sha(b/'CMakeCache.txt'),original_join_sha256=sha(r/'SOURCE_JOIN.json'),remaining_undefined_symbols=missing,scope='Tested production1861B0 phase1/122AD0 prefix;181A30 suffix is test-only fixture. Full entry refuses37 exact named effects; no game executable or endpoint execution. Same-scenario oracle comparison OPEN.')
(o/'VERIFICATION.json').write_text(json.dumps(result,indent=2)+'\n',newline='\n')
print(result['status'])
