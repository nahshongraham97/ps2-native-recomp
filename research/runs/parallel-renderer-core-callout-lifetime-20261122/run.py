from pathlib import Path
import subprocess,json,hashlib,sys,time
r=Path.cwd();d=r/'research/runs/parallel-renderer-core-callout-lifetime-20261122';v=d/sys.argv[1];v.mkdir(exist_ok=True);sha=lambda b:hashlib.sha256(b).hexdigest();cm=json.loads((r/'research/runs/parallel-renderer-plugin-0402-20261122/validation-01/build.json').read_text())['argv'][0]
files={p.relative_to(r).as_posix():sha(p.read_bytes()) for sub in ['runtime','recomp','tests','cmake'] for p in (r/sub).rglob('*') if p.is_file()};files['CMakeLists.txt']=sha((r/'CMakeLists.txt').read_bytes());(v/'SOURCE_FILES.json').write_text(json.dumps(files,indent=2)+'\n')
def run(n,a):
 t=time.time();p=subprocess.run(a,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);(v/(n+'.log')).write_bytes(p.stdout);x={'argv':a,'exit':p.returncode,'seconds':time.time()-t,'log_sha256':sha(p.stdout)};(v/(n+'.json')).write_text(json.dumps(x,indent=2)+'\n');return p,x
p,_=run('build',[cm,'--build',str(r/'build-renderer-0402'),'--target','test_native_retail_renderer_core','-j2']);assert p.returncode==0,p.stdout.decode(errors='replace')[-2000:]
exe=r/'build-renderer-0402/test_native_retail_renderer_core.exe';p,x=run('core',[str(exe),str(r/'research/runs/parallel-renderer-engine-init-20261122')]);x['executable_sha256']=sha(exe.read_bytes());x['source_manifest_sha256']=sha((v/'SOURCE_FILES.json').read_bytes());(v/'core.json').write_text(json.dumps(x,indent=2)+'\n');assert p.returncode==0,p.stdout.decode(errors='replace');assert all(sha((r/p).read_bytes())==h for p,h in files.items());print('PASS',sys.argv[1],x)
