from pathlib import Path
import json,subprocess,gzip,hashlib,time,re
r=Path.cwd();p=r/'research/runs/parallel-scene-bootstrap-continuation-20261122';v=p/'validation-04';build=r/'build-camera-continuation';assert not (v/'camera.json').exists(), 'preserved execution record already exists';files=[]
for pat in ['CMakeLists.txt','cmake/*.cmake','runtime/include/**/*.hpp','runtime/src/native/*.cpp','tests/*.cpp']:files.extend(f for f in r.glob(pat) if f.is_file())
pins={f.relative_to(r).as_posix():hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(set(files))};(v/'SOURCE_FILES.json').write_text(json.dumps(pins,indent=2)+'\n',newline='\n');manifest=hashlib.sha256((v/'SOURCE_FILES.json').read_bytes()).hexdigest()
def run(name,args,expected=0):
 t=time.monotonic();c=subprocess.run([str(x) for x in args],capture_output=True,creationflags=subprocess.CREATE_NO_WINDOW);raw=c.stdout+b'\nSTDERR:\n'+c.stderr;(v/(name+'.log.gz')).write_bytes(gzip.compress(raw,mtime=0));receipt=dict(argv=[str(x) for x in args],exit=c.returncode,seconds=time.monotonic()-t,source_manifest_sha256=manifest,executable_sha256=hashlib.sha256(Path(args[0]).read_bytes()).hexdigest(),log_sha256=hashlib.sha256(raw).hexdigest(),game_run=False,physical_endpoints_opened=False);(v/(name+'.json')).write_text(json.dumps(receipt,indent=2)+'\n',newline='\n');print(name,c.returncode,raw.decode(errors='replace')[-500:],flush=True);assert (c.returncode==0)==(expected==0),name;return raw
for name,args in [('camera',[build/'test_native_retail_camera_bootstrap.exe',p]),('prefix',[build/'test_native_retail_scene_bootstrap_prefix.exe',p]),('stopwatch',[build/'test_native_retail_main_timing.exe']),('session',[build/'test_native_retail_game_session.exe'])]:run(name,args)
cmake=Path(r'C:/Users/nahsh/mk port/Armageddon-backup/dependencies/cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe');raw=run('full-entry-negative',[cmake,'--build',build,'--target','test_native_startup_missing_bindings','-j2'],1)
names=[l.split('undefined symbol: ',1)[1] for l in raw.decode().splitlines() if 'undefined symbol: ' in l];assert len(names)==37
(v/'NEGATIVE_NAMES.json').write_text(json.dumps(names,indent=2)+'\n',newline='\n')
