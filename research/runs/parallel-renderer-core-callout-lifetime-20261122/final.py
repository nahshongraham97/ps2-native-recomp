from pathlib import Path
import json,subprocess,hashlib,re,gzip
r=Path.cwd();d=r/'research/runs/parallel-renderer-core-callout-lifetime-20261122';v=d/'final';v.mkdir(exist_ok=True);sha=lambda b:hashlib.sha256(b).hexdigest();cm=json.loads((d/'corrected-02/build.json').read_text())['argv'][0]
def run(n,a):
 p=subprocess.run(a,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(v/(n+'.log')).write_bytes(p.stdout);x={'argv':a,'exit':p.returncode,'log_sha256':sha(p.stdout)};(v/(n+'.json')).write_text(json.dumps(x,indent=2)+'\n');return p,x
p,_=run('build-matrix',[cm,'--build',str(r/'build-renderer-0402'),'--target','test_native_retail_renderer_matrix','-j2']);assert p.returncode==0,p.stdout.decode(errors='replace')
exe=r/'build-renderer-0402/test_native_retail_renderer_matrix.exe';p,x=run('matrix',[str(exe),str(r/'research/runs/parallel-renderer-plugin-0402-20261122')]);assert p.returncode==0,p.stdout;x['executable_sha256']=sha(exe.read_bytes());(v/'matrix.json').write_text(json.dumps(x,indent=2)+'\n')
p,n=run('negative',[cm,'--build',str(r/'build-renderer-0402'),'--target','test_native_startup_missing_bindings','-j2']);assert p.returncode!=0
actual=[s.strip() for s in re.findall(r'undefined symbol: (.*)',p.stdout.decode(errors='replace'))];prior=json.loads((r/'research/runs/parallel-renderer-plugin-0402-20261122/validation-03/VERIFICATION.json').read_text())['remaining_required_names'];assert actual==prior and len(actual)==61
cache=(r/'build-renderer-0402/CMakeCache.txt').read_text();nm=re.search(r'CMAKE_NM:FILEPATH=(.*)',cache)[1];compiler=re.search(r'CMAKE_CXX_COMPILER:STRING=(.*)',cache)[1];pins={}
for name in ['core','matrix']:
 ex=r/('build-renderer-0402/test_native_retail_renderer_'+name+'.exe');p,_=run(name+'-symbols',[nm,'--demangle',str(ex)]);s=p.stdout.decode(errors='replace');assert p.returncode==0 and re.search(r' T SDL_InitSubSystem',s) and '__imp_SDL_InitSubSystem' not in s
 assert 'NativeRetailRendererCore::construct_registered' in s and 'NativeRetailRendererCore::initialize_core' in s
for p in [Path(nm),Path(compiler),Path(cm),r/'build-renderer-0402/CMakeCache.txt',r/'runtime/src/native/retail_main.cpp',r/'runtime/src/native/retail_renderer_core.cpp']:
 pins[str(p)]=sha(p.read_bytes())
core=json.loads((d/'corrected-02/core.json').read_text());assert sha(Path(core['argv'][0]).read_bytes())==core['executable_sha256']
files=json.loads((d/'corrected-02/SOURCE_FILES.json').read_text());assert all(sha((r/p).read_bytes())==h for p,h in files.items())
for p in (r/'third_party/sdl2').rglob('*'):
 if p.is_file():pins[str(p)]=sha(p.read_bytes())
(v/'PINS.json').write_text(json.dumps(pins,indent=2)+'\n');result={'status':'PASS_CORE_OPERATION_SESSION_LEASE','base':'37a3a82e980e38c4702cb1aecd2529dadba06363','source_files':len(files),'source_manifest_sha256':sha((d/'corrected-02/SOURCE_FILES.json').read_bytes()),'core':core,'matrix':x,'required_names':actual,'pins_sha256':sha((v/'PINS.json').read_bytes()),'before_binary_sha256':sha((r/'build-renderer-0402/core.before-callout-lifetime.exe').read_bytes()),'diagnostic_binary_sha256':sha((r/'build-renderer-0402/core.safe-diagnostic.exe').read_bytes()),'corrected01_binary_sha256':sha((r/'build-renderer-0402/core.corrected-01.exe').read_bytes())};(v/'VERIFICATION.json').write_text(json.dumps(result,indent=2)+'\n');print(result['status'],len(files),len(actual))
