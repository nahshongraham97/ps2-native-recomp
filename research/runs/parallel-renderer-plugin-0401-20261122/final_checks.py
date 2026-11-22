from pathlib import Path
import json,subprocess,hashlib,re
r=Path.cwd();d=r/'research/runs/parallel-renderer-plugin-0401-20261122';v=d/'validation-06';v.mkdir();sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
files={p.relative_to(r).as_posix():sha(p) for sub in ['runtime','recomp','tests','cmake'] for p in (r/sub).rglob('*') if p.is_file()};files['CMakeLists.txt']=sha(r/'CMakeLists.txt');(v/'SOURCE_FILES.json').write_bytes((json.dumps(files,indent=2)+'\n').encode());cmake=json.loads((d/'validation-01/configure.json').read_text())['argv'][0]
def run(name,args):
 p=subprocess.run(args,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(v/(name+'.log')).write_bytes(p.stdout);x={'argv':args,'exit':p.returncode,'log_sha256':hashlib.sha256(p.stdout).hexdigest()};(v/(name+'.json')).write_bytes((json.dumps(x,indent=2)+'\n').encode());return p,x
p,x=run('build',[cmake,'--build',str(r/'build-renderer-0401'),'--target','test_native_retail_renderer_core','-j2']);assert p.returncode==0,p.stdout.decode(errors='replace')[-2500:]
exe=r/'build-renderer-0401/test_native_retail_renderer_core.exe';p,x=run('core',[str(exe),str(r/'research/runs/parallel-renderer-engine-init-20261122')]);assert p.returncode==0,p.stdout;x['executable_sha256']=sha(exe);x['source_manifest_sha256']=sha(v/'SOURCE_FILES.json');(v/'core.json').write_bytes((json.dumps(x,indent=2)+'\n').encode())
p,negative=run('full-entry-negative',[cmake,'--build',str(r/'build-renderer-0401'),'--target','test_native_startup_missing_bindings','-j2']);assert p.returncode!=0
actual=[s.strip() for s in re.findall(r'undefined symbol: (.*)',p.stdout.decode(errors='replace'))];prior=set(json.loads((r/'research/runs/parallel-renderer-engine-init-20261122/validation-06/VERIFICATION.json').read_text())['remaining_undefined_symbols']);removed={s for s in prior if 'core_plugin_0401(' in s};assert len(removed)==2;prior-=removed
new={s for s in actual if 'renderer_required::transform_' in s};assert len(new)==4;assert len(actual)==62 and set(actual)==prior|new,(len(actual),set(actual)^prior,new)
cache=r/'build-renderer-0401/CMakeCache.txt';nm=re.search(r'CMAKE_NM:FILEPATH=(.*)',cache.read_text())[1];compiler=re.search(r'CMAKE_CXX_COMPILER:STRING=(.*)',cache.read_text())[1]
p,_=run('core-symbols',[nm,'--demangle',str(exe)]);symbols=p.stdout.decode(errors='replace');assert p.returncode==0 and re.search(r' T SDL_InitSubSystem',symbols) and '__imp_SDL_InitSubSystem' not in symbols
for n in ['NativeRetailRendererTransforms::point','NativeRetailRendererTransforms::set_callbacks','construct_core_plugin_0401','destroy_core_plugin_0401']:assert n in symbols,n
reuse={}
old=json.loads((d/'validation-04/SOURCE_FILES.json').read_text());assert set(p for p,h in files.items() if old[p]!=h)=={'tests/test_retail_renderer_core.cpp','runtime/src/native/retail_renderer_transforms.cpp'}
for n in ['allocator','session']:
 receipt=json.loads((d/('validation-04/'+n+'.json')).read_text());assert receipt['exit']==0 and sha(Path(receipt['argv'][0]))==receipt['executable_sha256'];reuse[n]=receipt
 p,_=run(n+'-symbols',[nm,'--demangle',receipt['argv'][0]]);assert p.returncode==0 and 'NativeRetailRendererTransforms::' not in p.stdout.decode(errors='replace') and 'NativeRetailRendererCore::' not in p.stdout.decode(errors='replace')
assert all(sha(r/p)==h for p,h in files.items())
pins={str(Path(compiler)):sha(Path(compiler)),str(Path(nm)):sha(Path(nm)),str(cache):sha(cache)}
for p in (r/'third_party/sdl2').rglob('*'):
 if p.is_file():pins[str(p)]=sha(p)
(v/'ADDITIONAL_PINS.json').write_bytes((json.dumps(pins,indent=2)+'\n').encode());result={'status':'PASS_TYPED_0401_MODULE_SETTER_DISPATCH_RETIREMENT_ONLY','base':'feb05096e43fb0ae37bac6407d2567569ddb03e0','source_files':len(files),'source_manifest_sha256':sha(v/'SOURCE_FILES.json'),'core':x,'unchanged_allocator_session_from04':reuse,'additional_pins_sha256':sha(v/'ADDITIONAL_PINS.json'),'removed_required':sorted(removed),'new_required_math':sorted(new),'all_required':actual,'remaining_count':len(actual),'limits':'Four exact native math defaults required; all other factories/whole1B72B0/ENTIRE225D40 required. Math bodies and other factories/tails in component client are explicit fixtures; no EngineOpen/Start/game/endpoint acceptance.'};(v/'VERIFICATION.json').write_bytes((json.dumps(result,indent=2)+'\n').encode());print(result['status'],len(files),len(actual));print(x)
