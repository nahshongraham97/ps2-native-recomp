"""Compile/replay the explicit StageA memory layer, including real bad stores."""
from pathlib import Path
import argparse,hashlib,json,shutil,subprocess,sys
ROOT=Path(__file__).resolve().parents[2]
def pin(q):return dict(path=str(q.resolve()),sha256=hashlib.sha256(q.read_bytes()).hexdigest())
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',type=Path,required=True);p.add_argument('--sdk-root',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 out=a.output.resolve();assert out.is_relative_to(ROOT/'build/oracle-telemetry') and not out.exists();assert shutil.disk_usage(out.parent).free>(12<<30)+(64<<20);out.mkdir(parents=True)
 inputs=[Path(__file__),ROOT/'runtime/include/mka/stage_a_guest_memory.hpp',ROOT/'tests/test_phase6_guest_memory.cpp',a.compiler,a.sdk_root/'ee/kernel/include/kernel.h'];pins=[pin(q) for q in inputs]
 exe=out/'test_guest_memory.exe';argv=[str(a.compiler),'-std=c++20','-O2','-static-libgcc','-static-libstdc++','-I'+str(ROOT/'runtime/include'),str(inputs[2]),'-o',str(exe)]
 compiled=subprocess.run(argv,capture_output=True,timeout=120);(out/'compile.log').write_bytes(compiled.stdout+compiled.stderr);checks=[]
 if compiled.returncode==0:
  for name,flag,expected in [('positive',None,0),('width','--plant-width',1),('order','--plant-order',1)]:
   run=subprocess.run([str(exe)]+([flag] if flag else []),capture_output=True,timeout=60);(out/(name+'.log')).write_bytes(run.stdout+run.stderr)
   checks.append(dict(name=name,exit=run.returncode,expected_exit=expected,stdout=run.stdout.decode(errors='replace'),stderr=run.stderr.decode(errors='replace')))
 changes=[r['path'] for r in pins if pin(Path(r['path']))!=r]
 result=dict(status='PASS_TYPED_GUEST_MEMORY' if compiled.returncode==0 and len(checks)==3 and all(c['exit']==c['expected_exit'] for c in checks) and not changes else 'MEMORY_CHECKS_FAILED',invocation=[sys.executable,*sys.argv],source_pins=pins,input_changes=changes,compile_argv=argv,compile_exit=compiled.returncode,checks=checks,artifacts=[pin(q) for q in out.iterdir() if q.is_file()],scope='StageA RAM/SPR/5aliases/8-64bit signed+unsigned/LE/alignment/order only. Device/ROM/uninstalled/arbitraryTLB addresses refuse. No native owner/struct field or production integration claim.')
 q=out/'RESULT.json';q.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(status=result['status'],checks=checks,sha256=pin(q)['sha256'])));return 0 if result['status']=='PASS_TYPED_GUEST_MEMORY' else 2
if __name__=='__main__':raise SystemExit(main())
