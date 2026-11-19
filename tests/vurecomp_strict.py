"""Actual offline vurecomp CLI strictness; synthetic-only captures, no runtime oracle."""
import pathlib,struct,hashlib,json,subprocess,os,datetime,re

def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def capture(upper=0x400002ff,lower=0x8000033c):
 b=bytearray(16384); struct.pack_into('<4I',b,0,lower,upper,0x8000033c,0x2ff)
 h=0xcbf29ce484222325
 for pc in (0,8):
  for v in (pc,*struct.unpack_from('<2I',b,pc)):
   for x in struct.pack('<I',v): h=((h^x)*0x100000001b3)&0xffffffffffffffff
 return f'{h:016X}_0000.bin',bytes(b)
def main():
 root=pathlib.Path(__file__).resolve().parents[1]; build=root/'build_native_gl_full'; exe=build/'vurecomp.exe'
 cache=(build/'CMakeCache.txt').read_text(); compiler=pathlib.Path(re.search(r'^CMAKE_CXX_COMPILER:[^=]+=(.+)$',cache,re.M).group(1).strip()); env=os.environ.copy();env['PATH']=str(compiler.parent)+os.pathsep+env.get('PATH','')
 d=build/'diagnostics'/('vurecomp-strict-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f'));d.mkdir(parents=True,exist_ok=False)
 good=capture(); cases=[('valid',[good],None),('valid-compat',[good],None),('upper-primary',[capture(0x40000030)],'unsupported upper opcode'),('upper-extended',[capture(0x4000033f)],'unsupported upper opcode'),('lower',[capture(lower=0x7e000000)],'unsupported lower opcode'),('short',[(good[0],good[1][:-1])],'exactly 16384'),('oversize',[(good[0],good[1]+b'x')],'exactly 16384'),('bad-name',[('garbage.bin',good[1])],'malformed capture'),('bad-entry',[(good[0].replace('_0000','_0001'),good[1])],'malformed capture'),('key-mismatch',[('0000000000000000_0000.bin',good[1])],'key mismatch'),('empty',[],'at least one'),('missing',None,'capture/output error'),('mixed',[good,capture(0x40000030)],'unsupported upper opcode')]
 results=[]
 for name,files,marker in cases:
  c=d/name;c.mkdir(); inp=c/'inputs';out=c/'output';out.mkdir(); sentinel=out/'vu1_keep.cpp';sentinel.write_bytes(b'preserve existing output\n')
  if files is not None:
   inp.mkdir()
   for filename,data in files:(inp/filename).write_bytes(data)
  cmd=[str(exe),str(inp),str(out)]+(['--require-complete'] if name=='valid-compat' else [])
  r=subprocess.run(cmd,env=env,text=True,capture_output=True);log=c/'run.log';log.write_text(r.stdout+r.stderr)
  if marker:
   ok=r.returncode!=0 and marker in r.stderr and sentinel.read_bytes()==b'preserve existing output\n' and len(list(out.iterdir()))==1
  else:
   generated=list(out.glob('*.cpp'));ok=r.returncode==0 and bool(generated) and not sentinel.exists()
   if ok:
    body='\n'.join(x.read_text() for x in generated);ok='void vu1_' in body and 'unsupported' not in body
  results.append({'name':name,'pass':ok,'command':cmd,'exit':r.returncode,'expected_stderr':marker,'log_sha256':sha(log),'files':{str(p):sha(p) for p in c.rglob('*') if p.is_file()}})
 report={'scope':'synthetic CLI generation/rejection only, no microprogram execution or math oracle','executable':{'path':str(exe),'sha256':sha(exe)},'source':{str(root/p):sha(root/p) for p in ['recomp/vurecomp.cpp','recomp/vurecomp_gen.cpp','recomp/vurecomp_gen.hpp','tests/vurecomp_strict.py']},'cases':results}
 (d/'results.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'results':str(d/'results.json'),'failed':[x['name'] for x in results if not x['pass']],'cases':len(results)},indent=2));return any(not x['pass'] for x in results)
if __name__=='__main__':raise SystemExit(main())
