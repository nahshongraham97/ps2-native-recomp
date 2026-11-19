"""Offline synthetic-only regression of actual EE generator raw-target guards."""
import argparse, hashlib, json, os, pathlib, re, struct, subprocess, datetime

def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(); p.add_argument('--build',default='build_native_gl_full'); a=p.parse_args()
 root=pathlib.Path(__file__).resolve().parents[1]; build=root/a.build
 cache=(build/'CMakeCache.txt').read_text(); compiler=pathlib.Path(re.search(r'^CMAKE_CXX_COMPILER:[^=]+=(.+)$',cache,re.M).group(1).strip())
 exe=build/'eerecomp.exe'; out=build/'diagnostics'/('codegen-raw-targets-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f')); out.mkdir(parents=True,exist_ok=False)
 env=os.environ.copy(); env['PATH']=str(compiler.parent)+os.pathsep+env.get('PATH','')
 cases={'jr':([0x01000008,0x25080001], 'JR',8,False),'return':([0x03e00008,0x27ff0001],'JR return',31,False),'jalr':([0x0100f809,0x25080001,0x03e00008,0],'JALR',8,False),'eret':([0x42000018],'ERET',None,False)}
 for name,insn,kind,reg in [('complex-jr',0x01000008,'JR',8),('complex-return',0x03e00008,'return',31),('complex-jalr',0x0100f809,'JALR',8)]:
  cases[name]=([0x08040004,insn,0,0,(9<<26)|(reg<<21)|(reg<<16)|1,0x03e00008,0],kind,reg,True)
 report={'scope':'synthetic offline generation and syntax only; no generated execution or retail acceptance','artifacts':str(out),'generator':{'path':str(exe),'sha256':sha(exe)},'compiler':{'path':str(compiler),'sha256':sha(compiler)},'source':{str(root/'recomp/codegen.cpp'):sha(root/'recomp/codegen.cpp'),str(pathlib.Path(__file__).resolve()):sha(pathlib.Path(__file__).resolve())},'cases':[]}
 failures=[]
 for name,(words,kind,reg,complex_) in cases.items():
  d=out/name; d.mkdir(); words=words+[0]*(32-len(words)); code=struct.pack('<32I',*words)
  ident=b'\x7fELF\x01\x01\x01'+bytes(9)
  elf=ident+struct.pack('<HHIIIIIHHHHHH',2,8,1,0x100000,52,0,0,52,32,1,40,0,0)+struct.pack('<8I',1,0x100,0x100000,0x100000,len(code),len(code),5,16)
  f=d/'synthetic.elf'; f.write_bytes(elf+bytes(0x100-len(elf))+code)
  r=subprocess.run([str(exe),str(f),'--out',str(d/'generated')],capture_output=True,text=True,env=env); (d/'generator.log').write_text(r.stdout+r.stderr)
  item={'command':[str(exe),str(f),'--out',str(d/'generated')],'generator_log_sha256':sha(d/'generator.log'),'syntax':[],'name':name,'elf_sha256':sha(f),'generator_exit':r.returncode,'checks':[],'generated':{}}; report['cases'].append(item)
  try:
   if r.returncode: raise AssertionError('generator failed')
   files=list((d/'generated').rglob('*.cpp')); assert files,'no generated cpp'
   text='\n'.join(x.read_text() for x in files)
   marker='misaligned raw EE '+kind+' target'; pos=text.find(marker); assert pos>=0,'missing exact guard '+marker
   start=text.rfind('{',0,pos); # guard itself has no brace; enclosing capture scope
   var='dst1' if complex_ else 't'
   capture=('const std::uint32_t '+var+' = '+('eret(c)' if reg is None else 'c.r['+str(reg)+'].w[0]'))
   cap=text.rfind(capture,0,pos); assert cap>=0,'missing raw capture'
   tail=text[pos:]; assign=tail.find('c.pc = '+var); assert assign>=0,'missing guarded PC assignment'
   prefix=text[cap:pos]; assert not any(x in prefix for x in ['c.pc =','entry =','call(c,','dispatch(c,','unsupported delay slot']), 'transfer or rejection before guard'; assert re.search(r'if \(\('+var+r'\) & 3u\) stop\(c, '+var, prefix),'guard not bound to captured target'
   if reg is not None:
    assert ('set32(c.r['+str(reg)+']' in prefix or 'c.r['+str(reg)+'].sd[0] =' in prefix),'missing captured-before-mutating delay slot'
   if kind=='JR': assert '>> 2' in tail[:tail.find('\n')], 'missing local-table path'
   item['checks']=['raw capture before mutating slot (ERET has no slot)' ,'alignment guard before PC assignment/local table/call/dispatch','exact named fatal guard']
   for src in files:
    item['generated'][str(src)] = sha(src)
    cmd=[str(compiler),'-std=c++23','-fsyntax-only','-I'+str(root/'runtime/include'),'-I'+str(src.parent),str(src)]
    s=subprocess.run(cmd,capture_output=True,text=True,env=env); (d/(src.name+'.syntax.log')).write_text(s.stdout+s.stderr)
    item['syntax'].append({'command':cmd,'exit':s.returncode,'log_sha256':sha(d/(src.name+'.syntax.log'))}); assert s.returncode==0,'syntax failed '+src.name
   item['status']='PASS'
  except AssertionError as e: item['status']='FAIL'; item['error']=str(e); failures.append(name)
 report['failures']=failures; (out/'results.json').write_text(json.dumps(report,indent=2)); print(json.dumps({'results':str(out/'results.json'),'failures':failures,'cases':[(x['name'],x['status'],x.get('error')) for x in report['cases']]},indent=2)); return bool(failures)
if __name__=='__main__': raise SystemExit(main())
