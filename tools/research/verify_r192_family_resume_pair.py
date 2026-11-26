"""Check an actually observed entry/resume pair against compiled original AOT.

The offline scheduler callback is explicitly a stop boundary; actual scheduler
effects are not substituted with invented CPU/RAM expectations.
"""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import sys

sys.path.insert(0,str(Path(__file__).resolve().parent))
import specialize_r192_stub_family as family


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['family-proof','candidate-build','boundary','compiler','output']:
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();out=a.output.resolve()
    family.require(out.is_relative_to((family.W/'build/oracle-telemetry').resolve()) and not out.exists(),'fresh_owned_output_required')
    out.mkdir(parents=True)
    pair=json.loads((a.boundary/'PAIR.json').read_text())
    family.require(pair['key']=='actual thread/SP/object' and pair['thread']>0 and pair['last_initial_observer_count']>0,'actual_pair_missing')
    kernel=a.family_proof/'compiled_stub_family.hpp';adapter=family.W/'tools/research/r192_aot_modules/family_dispatch.hpp'
    unit=next(p for p in (a.family_proof/'original-aot').glob('m_*unit_*.cpp') if 'L_008A5140:' in p.read_text())
    source=unit.read_text();original=source[source.index('L_008A5140:'):source.index('L_008A6880:')]
    prefix=family.HARNESS.replace('@KERNEL@',kernel.resolve().as_posix()).replace('@ORIGINAL@',original)
    prefix=prefix[:prefix.index('int main(int argc,char**argv){')]
    harness=prefix+'\n#include <fstream>\n#include "'+adapter.as_posix()+'"\n'+TEST.replace('@BOUNDARY@',a.boundary.resolve().as_posix())
    inputs=[Path(__file__),Path(family.__file__),adapter,kernel,unit,a.compiler,*sorted(a.boundary.iterdir()),
            *sorted((a.candidate_build/'src/runtime/include/ps2').glob('*.hpp'))]
    pins=[family.pin(p) for p in inputs if p.is_file()]
    cpp=out/'paired.cpp';cpp.write_text(harness,encoding='utf8',newline='\n');exe=out/'paired.exe'
    command=[str(a.compiler),'-std=c++20','-mavx2','-ffp-contract=off','-O1','-static-libgcc','-static-libstdc++',
             '-I'+str((a.candidate_build/'src/runtime/include').resolve()),'-I'+str(a.family_proof.resolve()),str(cpp),'-o',str(exe)]
    cp=subprocess.run(command,capture_output=True);(out/'compile.log').write_bytes(cp.stdout+cp.stderr)
    family.require(cp.returncode==0,'paired_fixture_compile_failed')
    runs=[]
    for name,args,expected,reason in [('positive',[],0,'PASS actual_entry_resume_pair'),
          ('planted-retained-register',['--planted-retained-register'],2,'retained resume state changed'),
          ('planted-thread',['--planted-thread'],2,'orphan emitted-family resume'),
          ('planted-code',['--planted-code'],2,'emitted code changed after yield')]:
        run=subprocess.run([str(exe),*args],capture_output=True);(out/(name+'.log')).write_bytes(run.stdout+run.stderr)
        runs.append(dict(name=name,argv=[str(exe),*args],exit=run.returncode,expected_exit=expected,expected_reason=reason,
                         stdout=run.stdout.decode(errors='replace'),stderr=run.stderr.decode(errors='replace')))
    changed=[r['path'] for r in pins if family.pin(Path(r['path']))['sha256']!=r['sha256']]
    passed=all(r['exit']==r['expected_exit'] and r['expected_reason'] in r['stdout']+r['stderr'] for r in runs) and not changed
    record=dict(status='PASS_OBSERVED_PAIR_RETAINED_STATE' if passed else 'UNPROVED_OBSERVED_PAIR_RETAINED_STATE',runs=runs,
                observed_pair=pair,compile_command=command,source_pins=pins,fixture_pins=[family.pin(cpp),family.pin(exe)],input_changes=changed,
                scope='Actual observer-keyed entry/resume snapshots. Compiled-original and family comparisons terminate at explicit test scheduler boundaries. Full actual scheduler behavior, thread termination/reuse and state-load lifecycle wiring, production adoption and universal-family closure remain OPEN.')
    dest=out/'RESULT.json';dest.write_text(json.dumps(record,indent=2)+'\n',encoding='utf8',newline='\n')
    print(json.dumps(dict(status=record['status'],result=str(dest),sha256=family.pin(dest)['sha256'])))
    return 0 if passed else 1


TEST=r'''
namespace ps2::kernel{int observed_thread=0;int current_thread(){return observed_thread;}}
static void read(const char*name,void*data,std::size_t size){std::ifstream f(std::string("@BOUNDARY@/")+name,std::ios::binary);f.read(static_cast<char*>(data),size);if(!f||f.peek()!=EOF)throw std::runtime_error("actual pair size mismatch");}
int main(int argc,char**argv){try{
 using mka::emitted::FamilyDispatcher;
 const std::string mode=argc>1?argv[1]:"";
 Cpu prior{},actual{};read("Cpu-prior.bin",&prior,sizeof prior);read("Cpu-resume.bin",&actual,sizeof actual);read("Thread-prior.bin",&kernel::observed_thread,4);
 read("RAM-prior.bin",g_ram,kRamSize);read("SPR-prior.bin",g_spr,kScratchpadSize);
 std::vector<unsigned char> before(g_ram,g_ram+kRamSize),expected(kRamSize);std::array<unsigned char,kScratchpadSize> spr;std::memcpy(spr.data(),g_spr,kScratchpadSize);
 Cpu reference=prior,compiled=prior;writes.clear();preempt_calls=0;yield_requested=true;original(reference,false);const auto trace=writes;auto calls=preempt_calls;std::memcpy(expected.data(),g_ram,kRamSize);
 if(calls!=1||!(reference.pc&kUnwind))throw std::runtime_error("actual paired prior does not reach exactly one scheduler boundary");
 std::memcpy(g_ram,before.data(),kRamSize);writes.clear();preempt_calls=0;FamilyDispatcher d;
 if(!d.try_dispatch(compiled)||std::memcmp(&compiled,&reference,sizeof reference)||std::memcmp(expected.data(),g_ram,kRamSize)||trace!=writes||preempt_calls!=calls||std::memcmp(spr.data(),g_spr,kScratchpadSize))throw std::runtime_error("paired prior compiled boundary difference");
 unsigned changed=0;auto* a=reinterpret_cast<unsigned char*>(&actual);auto* r=reinterpret_cast<unsigned char*>(&reference);
 for(unsigned i=0;i<sizeof actual;++i){if((i>=offsetof(Cpu,pc)&&i<offsetof(Cpu,pc)+4)||(i>=offsetof(Cpu,budget)&&i<offsetof(Cpu,budget)+4))continue;
  if(a[i]!=r[i]){if(changed<32)std::fprintf(stderr,"OBSERVED_RETAINED_CPU_DIFFERENCE offset=%u original_stop=%02X actual_resume=%02X\n",i,r[i],a[i]);++changed;}}
 std::printf("actual_prior_budget=%d count=%llu actual_resume_budget=%d count=%llu retained_CPU_byte_differences_excluding_PC_budget=%u\n",prior.budget,static_cast<unsigned long long>(prior.r[10].d[0]),actual.budget,static_cast<unsigned long long>(actual.r[10].d[0]),changed);
 if(changed)return 2;
 read("Thread-resume.bin",&kernel::observed_thread,4);read("RAM-resume.bin",g_ram,kRamSize);read("SPR-resume.bin",g_spr,kScratchpadSize);std::memcpy(before.data(),g_ram,kRamSize);std::memcpy(spr.data(),g_spr,kScratchpadSize);
 if(mode=="--planted-retained-register")actual.f[2]^=1;
 if(mode=="--planted-thread")++kernel::observed_thread;
 reference=actual;writes.clear();preempt_calls=0;yield_requested=true;original(reference,true);const auto resumed_trace=writes;auto resumed_calls=preempt_calls;std::memcpy(expected.data(),g_ram,kRamSize);
 std::memcpy(g_ram,before.data(),kRamSize);writes.clear();preempt_calls=0;
 if(mode=="--planted-code")g_ram[actual.r[12].w[0]]^=1;
 if(!d.try_dispatch(actual)||std::memcmp(&actual,&reference,sizeof actual)||std::memcmp(expected.data(),g_ram,kRamSize)||resumed_trace!=writes||preempt_calls!=resumed_calls||std::memcmp(spr.data(),g_spr,kScratchpadSize))throw std::runtime_error("actual paired dispatcher resume difference");
 std::printf("PASS actual_entry_resume_pair=1 full_Cpu_RAM_SPR_ordered_write_comparisons=2 initial_writes=%zu resume_writes=%zu original_stop_calls=%u resume_stop_calls=%u lifecycle_wiring=OPEN actual_scheduler_effects=UNPROVED\n",trace.size(),resumed_trace.size(),calls,resumed_calls);return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"REFUSED %s\n",e.what());return 2;}}
'''

if __name__=='__main__':raise SystemExit(main())
