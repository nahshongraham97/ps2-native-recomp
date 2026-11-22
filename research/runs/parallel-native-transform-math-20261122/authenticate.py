from pathlib import Path
import subprocess,json,hashlib,re,struct,gzip,urllib.request
r=Path.cwd();d=r/'research/runs/parallel-native-transform-math-20261122';v=d/'validation-01';v.mkdir(exist_ok=True);sha=lambda b:hashlib.sha256(b).hexdigest();cc=Path(r'C:\Users\nahsh\mk port\Armageddon-backup\dependencies\compiler\llvm-mingw-20261006-ucrt-x86_64\bin\clang++.exe');ref=d/'parallel-ps2float.c';results={}
for name in ['compare-offline','check-hardware','primary-compare','fixed127-offline','vector-barrier']:
 args=[str(cc),'-std=c++20','-O2','-I',str(d)]
 if name=='primary-compare':args+=['-include',str(d/'primary-ps2float.h')]
 if name=='fixed127-offline':args+=['-I',str(r/'runtime/include')]
 args+=[str(d/(name+'.cpp')),str(ref)]
 if name=='primary-compare':args+=[str(d/'primary-ps2mul.cpp')]
 if name=='fixed127-offline':args+=[str(r/'runtime/src/native/geometry_companion_quantizer.cpp')]
 exe=r/'build-transform-math'/(name+'.exe');args+=['-o',str(exe)];q=subprocess.run([str(exe)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT);assert q.returncode==0;assert q.stdout==(v/(name+'.run.log')).read_bytes();results[name]={'build_argv':args,'build_exit_asserted_by_preserved_measure_script':0,'build_log_sha256':sha((v/(name+'.build.log')).read_bytes()),'executable_sha256':sha(exe.read_bytes()),'run_argv':[str(exe)],'run_exit':q.returncode,'run_log_sha256':sha(q.stdout)}
# All source rows are pinned to ELF independently; existing complete listings are reused.
a=r/'research/runs/parallel-renderer-plugin-0401-20261122';j=json.loads((a/'SOURCE_JOIN.json').read_text());elf=(r.parents[2]/'research/inputs/SLUS_214.10').read_bytes();assert sha(elf)==j['ELF_sha256'];source={}
for name in ['source-01/001a3800.asm','source-01/001a3760.asm','source-01/001a36c0.asm','source-01/001a3600.asm']:
 x=j['full_listings'][name];b=(a/name).read_bytes();assert sha(b)==x['sha256'] or sha(b.replace(b'\r\n',b'\n'))==x['sha256'];rows=[s.split('\t',2)for s in b.decode().splitlines()];off=x['file_offset'];assert sha(elf[off:off+len(rows)*4])==x['bytes_sha256'];source[name]=x
(d/'SOURCE_REUSE.json').write_text(json.dumps({'join_sha256':sha((a/'SOURCE_JOIN.json').read_bytes()),'ELF_sha256':sha(elf),'listings':source},indent=2)+'\n')
# Pinned network resources are independently re-read at exact commit, not mutable master.
net=json.loads((d/'NETWORK.json').read_text())
for n,x in net.items():
 if n=='autotests-list.json':continue
 assert sha((d/n).read_bytes())==x['sha256']
 u=x['url'].replace('/unknownbrackets/ps2autotests/master/','/unknownbrackets/ps2autotests/97469ffbed8631277b94e28d01dabd702aa97ef3/').replace('/libretro/parallel-ps2/master/','/libretro/parallel-ps2/f777604a56b7bf8eb812f7d055f2e4e063b648c7/')
 if '/PCSX2/pcsx2/master/' not in u:
  assert sha(urllib.request.urlopen(u).read())==x['sha256'];x['authenticated_pinned_url']=u
(d/'NETWORK.json').write_text(json.dumps(net,indent=2)+'\n')
pins={str(cc):sha(cc.read_bytes())}
for sub in [d,r/'runtime/include/mka',r/'runtime/src/native']:
 for p in sub.rglob('*'):
  if p.is_file() and (sub==d or p.name in ['geometry_companion_quantizer.cpp','geometry_companion_quantizer.hpp','material_translation.hpp']):pins[str(p)]=sha(p.read_bytes())
(v/'PINS.json').write_text(json.dumps(pins,indent=2)+'\n');(v/'RESULT.json').write_text(json.dumps({'status':'OPEN_NUMERIC_AUTHORITY_DISPUTE_NO_PRODUCTION_KERNEL','base':'fab3aee9c3b5edcce771affb93581f8628ce0b91','offline_clients':results,'source_reuse_sha256':sha((d/'SOURCE_REUSE.json').read_bytes()),'pins_sha256':sha((v/'PINS.json').read_bytes()),'limit':'Independent model agreement and published hardware190 cases do not establish disputed finite pair hardware result. No generic FPU/model enters accepted route.'},indent=2)+'\n')
