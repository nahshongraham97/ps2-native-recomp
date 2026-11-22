from pathlib import Path
import json,subprocess,time,hashlib,sys
r=Path(__file__).resolve().parents[3];d=Path(__file__).parent;v=d/sys.argv[1];v.mkdir(exist_ok=True)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
files={p.relative_to(r).as_posix():sha(p) for sub in ['runtime','recomp','tests','cmake'] for p in (r/sub).rglob('*') if p.is_file()};files['CMakeLists.txt']=sha(r/'CMakeLists.txt');(v/'SOURCE_FILES.json').write_bytes((json.dumps(files,indent=2)+'\n').encode())
cmake=json.loads((d/'validation-01/configure.json').read_text())['argv'][0]
a=[cmake,'--build',str(r/'build-renderer-engine'),'--target','test_native_retail_renderer_core','test_native_retail_renderer_allocator','test_native_retail_game_session','-j2'];t=time.time();p=subprocess.run(a,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(v/'build.log').write_bytes(p.stdout);(v/'build.json').write_bytes((json.dumps({'argv':a,'exit':p.returncode,'seconds':time.time()-t},indent=2)+'\n').encode());print('build',p.returncode,flush=True)
if p.returncode:print(p.stdout.decode(errors='replace')[-5000:]);raise SystemExit(1)
for n,target,args in [('core','test_native_retail_renderer_core',[str(d)]),('allocator','test_native_retail_renderer_allocator',[str(d)]),('session','test_native_retail_game_session',[])]:
 exe=r/'build-renderer-engine'/(target+'.exe');a=[str(exe)]+args;p=subprocess.run(a,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(v/(n+'.log')).write_bytes(p.stdout);(v/(n+'.json')).write_bytes((json.dumps({'argv':a,'exit':p.returncode,'executable_sha256':sha(exe),'source_manifest_sha256':sha(v/'SOURCE_FILES.json'),'log_sha256':hashlib.sha256(p.stdout).hexdigest()},indent=2)+'\n').encode());print(n,p.returncode,p.stdout.decode(errors='replace'),flush=True)
 if p.returncode:raise SystemExit(1)
assert all(sha(r/p)==h for p,h in files.items());print('SOURCE_UNCHANGED',len(files))
