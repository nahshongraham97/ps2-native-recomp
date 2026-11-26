"""Standalone caller-bound family dispatcher tests; never patches a candidate."""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import subprocess
import sys

sys.path.insert(0,str(Path(__file__).resolve().parent))
import specialize_r192_stub_family as family


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--family-proof',type=Path,required=True)
    p.add_argument('--candidate-build',type=Path,required=True)
    p.add_argument('--elf',type=Path,required=True)
    p.add_argument('--compiler',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--actual-boundary',type=Path)
    a=p.parse_args();out=a.output.resolve()
    family.require(out.is_relative_to((family.W/'build/oracle-telemetry').resolve()) and not out.exists(),'fresh_owned_output_required')
    elf=a.elf.read_bytes();family.require(hashlib.sha256(elf).hexdigest()=='a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f','ELF_changed')
    ph=struct.unpack_from('<I',elf,28)[0];step,n=struct.unpack_from('<HH',elf,42)
    words=[]
    for pc in range(0x2e612c,0x2e6180,4):
        matches=[]
        for i in range(n):
            k,o,v,_,s,*_=struct.unpack_from('<8I',elf,ph+i*step)
            if k==1 and v<=pc<pc+4<=v+s:matches.append(struct.unpack_from('<I',elf,o+pc-v)[0])
        family.require(len(matches)==1,'caller_mapping_ambiguous');words.append(matches[0])
    out.mkdir(parents=True)
    kernel=a.family_proof/'compiled_stub_family.hpp'
    unit=next(x for x in (a.family_proof/'original-aot').glob('m_*unit_*.cpp') if 'L_008A5140:' in x.read_text())
    source=unit.read_text();original=source[source.index('L_008A5140:'):source.index('L_008A6880:')]
    adapter=family.W/'tools/research/r192_aot_modules/family_dispatch.hpp'
    harness=family.HARNESS.replace('@KERNEL@',kernel.resolve().as_posix()).replace('@ORIGINAL@',original)
    harness=harness.replace('int main(int argc,char**argv){', 'int legacy_main(int argc,char**argv){')
    harness+='\n#include <fstream>\n#include "'+adapter.as_posix()+'"\n'+TEST.replace('@CALLER_WORDS@',','.join(f'0x{x:08X}u' for x in words)).replace('@ACTUAL_PATH@',a.actual_boundary.resolve().as_posix() if a.actual_boundary else '')
    cpp=out/'test.cpp';cpp.write_text(harness,encoding='utf8',newline='\n');binary=out/'test.exe'
    command=[str(a.compiler),'-std=c++20','-mavx2','-ffp-contract=off','-O1','-static-libgcc','-static-libstdc++',
             '-I'+str((a.candidate_build/'src/runtime/include').resolve()),'-I'+str(a.family_proof.resolve()),str(cpp),'-o',str(binary)]
    compiled=subprocess.run(command,capture_output=True,text=True);(out/'compile.log').write_text(compiled.stdout+compiled.stderr,encoding='utf8',newline='\n')
    family.require(compiled.returncode==0,'dispatcher_fixture_compile_failed')
    run=subprocess.run([str(binary)],capture_output=True,text=True);(out/'test.log').write_text(run.stdout+run.stderr,encoding='utf8',newline='\n')
    result=dict(status='PASS_STANDALONE_DISPATCH' if run.returncode==0 else 'FAIL',exit=run.returncode,stdout=run.stdout,stderr=run.stderr,
                source_pins=[family.pin(x) for x in [Path(__file__),adapter,kernel,unit,a.elf,cpp,binary]],compile_command=command,
                caller_word_pins=[dict(pc=f'{0x2e612c+i*4:08X}',word=f'{word:08X}') for i,word in enumerate(words)],
                actual_boundary_pins=[family.pin(x) for x in sorted(a.actual_boundary.glob('*.bin'))] if a.actual_boundary else [],
                scope='Standalone fixture only; authentic original caller setup words/field relations and tested original yield continuation. No N write/adoption, no unknown-family fallback or universal-constructor claim.')
    raw=(json.dumps(result,indent=2)+'\n').encode();(out/'RESULT.json').write_bytes(raw)
    print(json.dumps(dict(status=result['status'],result=str(out/'RESULT.json'),sha256=hashlib.sha256(raw).hexdigest())))
    return run.returncode


TEST=r'''
using mka::emitted::FamilyDispatcher;
using namespace mka::emitted::known_clip48;
namespace ps2::kernel { int test_thread=1; int current_thread(){return test_thread;} }
static Cpu setup(){
 std::memset(g_ram,0,kRamSize);for(unsigned i=0;i<52;++i)put(0x8a5140+i*4,pattern[i]);
 constexpr unsigned caller[]={@CALLER_WORDS@};for(unsigned i=0;i<21;++i)put(0x2e612c+i*4,caller[i]);
 unsigned constants[9]={0,0x3ca3d70b,0,0xbca3d70b,0x437f0000,0x4087f92c,0,0x42700000,0x42700000};
 for(unsigned i=0;i<9;++i)put(0x8a5210+i*4,constants[i]);
 constexpr unsigned object=0x500000;put(object,2);put(object+4,0);put(object+8,0x100000);put(object+0x10,0x200000);
 put(object+0x28,0x300000);put(object+0x2c,64);put(object+0x30,0x400000);put(object+0x34,16);
 put(object+0x4c,0x8a5140);put(object+0x394,52);
 Cpu c{};c.pc=0x8a5140;c.r[4].d[0]=0x100000;c.r[5].d[0]=0x200000;c.r[6].d[0]=0x300000;c.r[7].d[0]=0x400000;
 c.r[8].d[0]=64;c.r[9].d[0]=16;c.r[10].d[0]=2;c.r[11].d[0]=0x8a5210;c.r[12].d[0]=0x8a5140;
 c.r[19].d[0]=object;c.r[29].d[0]=0x1fff000;c.r[31].d[0]=0x2e6180;c.f[12]=c.f[20]=c.vu0.vf[1].w[0]=0x3f800000;c.vu0.vf[0].w[3]=0x3f800000;c.budget=10000;
 kernel::test_thread=1;
 yield_requested=false;writes.clear();preempt_calls=0;return c;
}
template<class F>static bool rejected(F f,const char* needle){try{f();}catch(const std::exception&e){return std::string(e.what()).find(needle)!=std::string::npos;}return false;}
int main(){
 unsigned controls=0;std::vector<unsigned char> before(kRamSize),expected(kRamSize);
 {Cpu c=setup(),reference=c;std::memcpy(before.data(),g_ram,kRamSize);original(reference,false);auto trace=writes;std::memcpy(expected.data(),g_ram,kRamSize);std::memcpy(g_ram,before.data(),kRamSize);writes.clear();FamilyDispatcher d;
  if(!d.try_dispatch(c)||std::memcmp(&c,&reference,sizeof c)||trace!=writes||std::memcmp(expected.data(),g_ram,kRamSize))return 1;++controls;}
 {Cpu c=setup();FamilyDispatcher d;c.budget=-1;yield_requested=true;if(!d.try_dispatch(c)||!(c.pc&kUnwind))return 2;
  c.pc&=~kUnwind;c.budget=10000;yield_requested=false;Cpu reference=c;std::memcpy(before.data(),g_ram,kRamSize);writes.clear();original(reference,true);auto trace=writes;std::memcpy(expected.data(),g_ram,kRamSize);
  std::memcpy(g_ram,before.data(),kRamSize);writes.clear();if(!d.try_dispatch(c)||std::memcmp(&c,&reference,sizeof c)||trace!=writes||std::memcmp(expected.data(),g_ram,kRamSize))return 3;++controls;}
 {Cpu c=setup();FamilyDispatcher d;put(0x2e617c,0);if(!rejected([&]{d.try_dispatch(c);},"caller instruction"))return 4;++controls;}
 {Cpu c=setup();FamilyDispatcher d;c.r[4].d[0]+=16;if(!rejected([&]{d.try_dispatch(c);},"parameter relation"))return 5;++controls;}
 {Cpu c=setup(),saved=c;FamilyDispatcher d;put(0x500394,51);if(d.try_dispatch(c)||std::memcmp(&saved,&c,sizeof c))return 6;++controls;}
 {Cpu c=setup();FamilyDispatcher d;c.pc+=0x30;if(!rejected([&]{d.try_dispatch(c);},"orphan"))return 7;++controls;}
 {Cpu c=setup();FamilyDispatcher d;c.r[29].d[0]=kRamSize-4;if(!rejected([&]{d.try_dispatch(c);},"stack context"))return 8;++controls;}
 {Cpu c=setup();FamilyDispatcher d;c.budget=-1;yield_requested=true;d.try_dispatch(c);c.pc&=~kUnwind;c.budget=10000;put(0x8a5140,pattern[0]^0x10000);
  if(!rejected([&]{d.try_dispatch(c);},"code changed"))return 9;put(0x8a5140,pattern[0]);if(!rejected([&]{d.try_dispatch(c);},"orphan"))return 10;++controls;}
 {Cpu c=setup();FamilyDispatcher d;c.budget=-1;yield_requested=true;d.try_dispatch(c);c.pc&=~kUnwind;c.budget=10000;c.f[2]^=1;
  if(!rejected([&]{d.try_dispatch(c);},"resume state changed"))return 11;++controls;}
 {Cpu c=setup();FamilyDispatcher d;c.r[31].d[0]=0x100000;if(d.try_dispatch(c))return 12;++controls;}
 {Cpu c=setup();FamilyDispatcher d;put(0x8a5140,pattern[0]^0x10000);put(0x2e617c,0);if(d.try_dispatch(c))return 15;++controls;}
 {Cpu first=setup(),second=first;FamilyDispatcher d;first.budget=second.budget=-1;yield_requested=true;
  if(!d.try_dispatch(first))return 16;kernel::test_thread=2;if(!d.try_dispatch(second))return 17;
  first.pc&=~kUnwind;first.budget=10000;second.pc&=~kUnwind;second.budget=10000;yield_requested=false;
  kernel::test_thread=1;if(!d.try_dispatch(first))return 18;kernel::test_thread=2;if(!d.try_dispatch(second))return 19;++controls;}
 {Cpu first=setup(),nested=first;FamilyDispatcher d;first.budget=nested.budget=-1;nested.r[29].d[0]-=0x200;yield_requested=true;
  d.try_dispatch(first);d.try_dispatch(nested);first.pc&=~kUnwind;first.budget=10000;nested.pc&=~kUnwind;nested.budget=10000;yield_requested=false;
  if(!d.try_dispatch(first)||!d.try_dispatch(nested))return 20;++controls;}
 {Cpu c=setup(),collision=c;FamilyDispatcher d;c.budget=-1;yield_requested=true;d.try_dispatch(c);
  if(!rejected([&]{d.try_dispatch(collision);},"context collision"))return 21;
  d.reset_after_machine_state_replacement();c.pc&=~kUnwind;c.budget=10000;
  if(!rejected([&]{d.try_dispatch(c);},"orphan"))return 22;++controls;}
 {Cpu c=setup();FamilyDispatcher d;c.budget=-1;yield_requested=true;d.try_dispatch(c);d.retire_guest_thread(1);c.pc&=~kUnwind;c.budget=10000;
  if(!rejected([&]{d.try_dispatch(c);},"orphan"))return 23;++controls;}
 if(std::string("@ACTUAL_PATH@").size()){
  auto read=[](const char*name,void*data,std::size_t size){std::ifstream f(std::string("@ACTUAL_PATH@/")+name,std::ios::binary);f.read(static_cast<char*>(data),size);if(!f||f.peek()!=EOF)throw std::runtime_error("actual boundary size mismatch");};
  Cpu c{};read("Cpu.bin",&c,sizeof c);read("RAM.bin",g_ram,kRamSize);read("SPR.bin",g_spr,kScratchpadSize);read("Thread.bin",&kernel::test_thread,4);
  Cpu reference=c;std::memcpy(before.data(),g_ram,kRamSize);std::array<unsigned char,kScratchpadSize> spr;std::memcpy(spr.data(),g_spr,kScratchpadSize);
  writes.clear();preempt_calls=0;yield_requested=false;original(reference,false);const auto trace=writes;const auto reference_preempts=preempt_calls;std::memcpy(expected.data(),g_ram,kRamSize);
  std::memcpy(g_ram,before.data(),kRamSize);writes.clear();preempt_calls=0;FamilyDispatcher d;
  if(!d.try_dispatch(c)||std::memcmp(&c,&reference,sizeof c)||std::memcmp(expected.data(),g_ram,kRamSize)||std::memcmp(spr.data(),g_spr,kScratchpadSize)||trace!=writes||preempt_calls!=reference_preempts)return 13;
  if(reference_preempts){std::fprintf(stderr,"ACTUAL_BOUNDARY_SCHEDULER_SUBOBLIGATION_UNPROVED calls=%u\n",reference_preempts);return 14;}
  std::printf("PASS actual_production_boundary=1 original_A0_object_load_then_S3_alias_verified=1 full_Cpu_RAM_SPR_ordered_writes=%zu scheduler_calls=%u no_native_credit=1\n",writes.size(),reference_preempts);++controls;
  read("Cpu-resume.bin",&c,sizeof c);read("RAM-resume.bin",g_ram,kRamSize);read("SPR-resume.bin",g_spr,kScratchpadSize);read("Thread-resume.bin",&kernel::test_thread,4);
  // Stop at the first scheduler boundary. The fixture cannot reproduce
  // run_due_events/refill, so it must not claim the ensuing real schedule.
  reference=c;std::memcpy(before.data(),g_ram,kRamSize);std::memcpy(spr.data(),g_spr,kScratchpadSize);writes.clear();preempt_calls=0;yield_requested=true;
  original(reference,true);auto resume_trace=writes;auto resume_calls=preempt_calls;std::memcpy(expected.data(),g_ram,kRamSize);std::memcpy(g_ram,before.data(),kRamSize);writes.clear();preempt_calls=0;
  run(c,bind(c.r[12].w[0],52));
  if(std::memcmp(&c,&reference,sizeof c)||std::memcmp(expected.data(),g_ram,kRamSize)||std::memcmp(spr.data(),g_spr,kScratchpadSize)||resume_trace!=writes||preempt_calls!=resume_calls)return 24;
  if(resume_calls!=1||!(c.pc&kUnwind))return 25;
  std::printf("PASS actual_natural_resume_seed_compiled_segment_to_first_scheduler_boundary=1 ordered_writes=%zu conditional_preempt_fixture_calls=%u real_scheduler_effects=UNPROVED dispatcher_lifetime_pairing=UNPROVED\n",writes.size(),resume_calls);++controls;
 }
 std::printf("PASS caller_bound_dispatch_controls=%u constructed_positive_entry_resume=2 task_and_nested_stack_non_eviction=2 lifecycle_controls=2 collision_refusal=1 foreign_shape_unhandled=1 game_launches=0\n",controls);
}
'''

if __name__=='__main__':raise SystemExit(main())
