"""Falsify the reference-only preferred-family adapter on actual entry inputs.

Actual compiled original bodies are the oracle. Scheduler callbacks here are
explicit constructed services; the later disposable probe uses the real one.
"""
from pathlib import Path
import argparse
import json
import re
import struct
import subprocess
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
import specialize_r192_stub_family as family
import analyze_r192_builder_domain as original


def contract_source(elf,proof,output):
    family.require(family.pin(elf)['sha256']==original.ELF_SHA,'original_ELF_changed')
    record=json.loads(proof.read_text());rows=record['source_pins']
    ram=next(Path(r['path']) for r in rows if Path(r['path']).name=='RAM.bin')
    cpu=ram.with_name('Cpu.bin');program=next(Path(r['path']) for r in rows if Path(r['path']).name=='regenerated.bin')
    for path in [ram,cpu,program]:family.require(any(Path(r['path'])==path and family.pin(path)['sha256']==r['sha256'] for r in rows),'contract_source_pin_missing:'+str(path))
    data=ram.read_bytes();object=struct.unpack_from('<I',cpu.read_bytes(),19*16)[0]
    code=struct.unpack_from('<52I',program.read_bytes());reader=original.Original(elf.read_bytes());caller=[reader.word(pc) for pc in range(0x2e612c,0x2e6180,4)]
    text='// Authenticated original identity/data contract; no executable operation stream.\n'
    text+='static const mka::emitted::reference::Contract contract={\n'
    for values in [code,caller,data[object+208:object+400],data[object+84:object+104]]:text+=' {'+','.join(hex(x)+'u' for x in values)+'},\n'
    text+='};\n';output.write_text(text,encoding='utf8',newline='\n');return [elf,ram,cpu,program]


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['full-cpu-proof','projection-proof','observations','elf','candidate-build','compiler','output']:p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();out=a.output.resolve();family.require(out.is_relative_to((family.W/'build/oracle-telemetry').resolve()) and not out.exists(),'fresh_owned_output_required')
    proof=a.full_cpu_proof/'RESULT.json';family.require(family.pin(proof)['sha256']=='42dfbf045c06a9206eb64ac5cb839823f9b72a8754bf1a457630af034da73ffe','full_CPU_proof_changed')
    projection_proof=a.projection_proof/'RESULT.json';family.require(family.pin(projection_proof)['sha256']=='7aa6a86e05c0b151d16742deba5fcc62568fc74346c67145eb5ec5c325f84a56','actual_projection_proof_changed')
    observation=a.observations/'RESULT.json';family.require(family.pin(observation)['sha256']=='0219eacefa312ccf3c4734c623d7e8c1032b8339b275fa540fd0e64d9bd95cb9','actual_observation_receipt_changed')
    obs=json.loads(observation.read_text())
    for row in obs['observation_pins']:family.require(family.pin(Path(row['path']))['sha256']==row['sha256'],'observation_artifact_changed')
    record=json.loads(proof.read_text());pin=next(r for r in record['runs'][0]['fixture_pins'] if Path(r['path']).suffix=='.cpp');fixture=Path(pin['path']);family.require(family.pin(fixture)['sha256']==pin['sha256'],'reference_fixture_changed')
    source=fixture.read_bytes().decode().replace('\r\n','\n');folder=family.W/'tools/research/r192_aot_modules';adapter=folder/'reference_family_adapter.hpp'
    source,n=re.subn(r'#include "[^"\n]*semantic_continuation\.hpp"','@ADAPTER_INCLUDE@',source);family.require(n==1,'reference_include_changed')
    start=source.index('bool ps2::preempt(Cpu&c){');end=source.index('\nstatic sem::GuardDecision native_guard',start)
    source=source[:start]+SCHEDULER+source[end:]
    source=source.replace('int main(){unsigned cancellation_tests=0;','static int prior_full_cpu_main(){unsigned cancellation_tests=0;')
    family.require('int main(' not in source,'unrenamed_reference_main')
    out.mkdir(parents=True);contract=out/'reference_contract.hpp';contract_inputs=contract_source(a.elf,proof,contract)
    source+='\n#include "'+contract.as_posix()+'"\n'+TEST.replace('@DATA@',(a.observations/'observations').resolve().as_posix()).replace('refused(','adapter_refused(').replace('refusals','adapter_refusals')
    inputs=[Path(__file__),Path(family.__file__),Path(original.__file__),proof,projection_proof,observation,fixture,adapter,folder/'reference_family_projection.hpp',a.compiler,*folder.glob('semantic_*.hpp'),
            *contract_inputs,*(Path(r['path']) for r in obs['observation_pins']),*sorted((a.candidate_build/'src/runtime/include/ps2').glob('*.hpp')),a.candidate_build/'src/runtime/src/kernel.hpp']
    pins=[family.pin(x) for x in dict.fromkeys(inputs)];runs=[]
    variants=[('positive',None,None),
              ('planted-live-step','c.f[12]!=found->second->step','false'),
              ('planted-parked-state','if(!parked_state_matches(c,found->second->frame,found->second->continuation))','if(false)'),
              ('planted-code-lifetime','if(found!=pending_.end()){found->second->cancelled=true;refuse("code identity changed during suspension");}','if(false){found->second->cancelled=true;refuse("code identity changed during suspension");}'),
              ('planted-orphan','if(found==pending_.end())refuse("orphan resume requires proved typed rehydration");','if(found==pending_.end())return false;'),
              ('planted-callback-frame','if(std::memcmp(&retained,&before,sizeof before))','if(false)')]
    for name,before,after in variants:
        directory=out/name;directory.mkdir();text=adapter.read_text()
        if before:family.require(text.count(before)==1,'adapter_mutation_anchor_changed:'+name);text=text.replace(before,after)
        (directory/adapter.name).write_text(text,encoding='utf8',newline='\n')
        for path in [folder/'reference_family_projection.hpp',*folder.glob('semantic_*.hpp')]:(directory/path.name).write_bytes(path.read_bytes())
        cpp=directory/'test.cpp';cpp.write_text(source.replace('@ADAPTER_INCLUDE@','#include "'+(directory/adapter.name).as_posix()+'"'),encoding='utf8',newline='\n');exe=directory/'test.exe'
        command=[str(a.compiler),'-std=c++20','-mavx2','-ffp-contract=off','-O1','-static-libgcc','-static-libstdc++','-I'+str((a.candidate_build/'src/runtime/include').resolve()),'-I'+str((a.candidate_build/'src/runtime/src').resolve()),str(cpp),'-o',str(exe)]
        cp=subprocess.run(command,capture_output=True);(directory/'compile.log').write_bytes(cp.stdout+cp.stderr);family.require(cp.returncode==0,'adapter_fixture_compile_failed:'+name)
        run=subprocess.run([str(exe)],capture_output=True);(directory/'test.log').write_bytes(run.stdout+run.stderr)
        runs.append(dict(name=name,compile_command=command,argv=[str(exe)],exit=run.returncode,expected_exit=0 if name=='positive' else 1,stdout=run.stdout.decode(errors='replace'),stderr=run.stderr.decode(errors='replace'),fixture_pins=[family.pin(cpp),family.pin(exe),*[family.pin(x) for x in directory.glob('*.hpp')]]))
    changed=[r['path'] for r in pins if family.pin(Path(r['path']))['sha256']!=r['sha256']]
    passed=not changed and all(r['exit']==r['expected_exit'] and ('PASS_REFERENCE_ADAPTER' if r['name']=='positive' else 'ADAPTER_TEST_DIFFERENCE') in r['stdout']+r['stderr'] for r in runs)
    result=dict(status='PASS_BOUNDED_REFERENCE_ADAPTER' if passed else 'FAIL',runs=runs,source_pins=pins,contract_pin=family.pin(contract),input_changes=changed,
                scope=['Actual captured entry inputs, independently compiled original body for no-yield/stop/resume comparisons.',
                       'Constructed reference scheduler writes arbitrary sentinel RAM; adapter preserves it, not a two-byte whitelist.',
                       'GlobalCPU/step/stride/frame/opaque/coherence/code/owner/orphan/sharedVU/retirement/state-reset gates refuse by name.',
                       'Native runtime or original scheduler replacement is not claimed; actual candidate probe is a separate operation.'])
    target=out/'RESULT.json';target.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8',newline='\n');print(json.dumps(dict(status=result['status'],result=str(target),sha256=family.pin(target)['sha256'])))
    return 0 if passed else 1


SCHEDULER=r'''
namespace ps2 {Cpu g_cpu;namespace kernel {static int test_thread=1;int current_thread(){return test_thread;}}}
static mka::emitted::reference::Adapter*test_adapter=nullptr;
static bool callback_clobber=false,callback_retire=false,callback_code_change=false;
static unsigned callback_ordinal=0;
bool ps2::preempt(Cpu&c){++preempt_calls;++callback_ordinal;
 checked_st32(0x1d00000,0x12340000u+callback_ordinal);c.budget=20000;
 if(callback_clobber)c.r[16].d[1]^=1;
 if(callback_retire&&test_adapter)test_adapter->retire_thread(kernel::current_thread());
 if(callback_code_change)g_ram[c.r[12].w[0]]^=1;
 return true;
}
'''

TEST=r'''
static void observed_read(const std::string&name,void*destination,std::size_t bytes){std::ifstream file(std::string("@DATA@/")+name,std::ios::binary);file.read(static_cast<char*>(destination),bytes);if(!file||file.peek()!=EOF)throw std::runtime_error("test observation extent");}
static unsigned comparisons=0,refusals=0;
static void require(bool condition,const char*message){if(!condition)throw std::runtime_error(std::string("ADAPTER_TEST_DIFFERENCE ")+message);}
static void refused(auto action,const char*reason){try{action();}catch(const std::exception&e){if(std::string(e.what()).find(reason)!=std::string::npos){++refusals;return;}throw;}throw std::runtime_error(std::string("ADAPTER_TEST_DIFFERENCE expected refusal ")+reason);}
int main(){try{
 namespace ref=mka::emitted::reference;Cpu high{},low{};observed_read("case-1-before-Cpu.bin",&high,sizeof high);observed_read("case-3-before-Cpu.bin",&low,sizeof low);
 std::vector<unsigned char> high_ram(kRamSize),low_ram(kRamSize);observed_read("case-1-before-RAM.bin",high_ram.data(),kRamSize);observed_read("case-3-before-RAM.bin",low_ram.data(),kRamSize);
 auto run_pair=[&](ref::Adapter&adapter,Cpu&reference,std::vector<unsigned char>&reference_ram,std::vector<unsigned char>&native_ram,bool resume){
  const auto base=g_cpu.r[12].w[0];std::memcpy(g_ram,reference_ram.data(),kRamSize);writes.clear();callback_ordinal=0;reference_resume=resume;reference_regenerated(reference);const auto trace=writes;std::memcpy(reference_ram.data(),g_ram,kRamSize);
  const auto expected_pc=reference.pc==0x2e6180?0x2e6180:(base+0x30)|kUnwind;require(reference.pc==0x2e6180||reference.pc==0x1c00031,"source return/resume edge");
  std::memcpy(g_ram,native_ram.data(),kRamSize);writes.clear();callback_ordinal=0;require(adapter.try_dispatch(g_cpu),"known source family not selected");std::memcpy(native_ram.data(),g_ram,kRamSize);
  Cpu mapped=reference;mapped.pc=g_cpu.pc;mapped.base=g_cpu.base;
  require(expected_pc==g_cpu.pc&&!std::memcmp(&mapped,&g_cpu,sizeof g_cpu)&&reference_ram==native_ram&&trace==writes,"full original/reference adapter CPU RAM ordered writes");++comparisons;
 };
 {ref::Adapter adapter(contract,{nullptr,&observe});g_cpu=high;Cpu reference=high;auto a=high_ram,b=high_ram;run_pair(adapter,reference,a,b,false);require(adapter.pending()==0,"completed context retained");}
 {ref::Adapter adapter(contract,{nullptr,&observe});g_cpu=low;Cpu reference=low;auto a=low_ram,b=low_ram;run_pair(adapter,reference,a,b,false);require(adapter.pending()==1,"real low-budget context absent");
  g_cpu.pc&=~kUnwind;g_cpu.budget=reference.budget=33;run_pair(adapter,reference,a,b,true);require(adapter.pending()==1,"second suspension absent");
  g_cpu.pc&=~kUnwind;g_cpu.budget=reference.budget=20000;run_pair(adapter,reference,a,b,true);require(adapter.pending()==0,"resumed context not completed");}
 auto fresh=[&]{std::memcpy(g_ram,high_ram.data(),kRamSize);g_cpu=high;writes.clear();};
 fresh();{ref::Adapter adapter(contract);Cpu private_cpu=high;refused([&]{adapter.try_dispatch(private_cpu);},"global CPU/timebase");require(writes.empty(),"private CPU refusal wrote RAM");}
 fresh();{ref::Adapter adapter(contract);g_ram[g_cpu.r[12].w[0]]^=1;require(!adapter.try_dispatch(g_cpu)&&!adapter.pending(),"unknown code admitted");}
 fresh();{ref::Adapter adapter(contract);g_ram[0x2e612c]^=1;refused([&]{adapter.try_dispatch(g_cpu);},"caller words changed");}
 fresh();{ref::Adapter adapter(contract);g_ram[g_cpu.r[19].w[0]+208+4]^=1;refused([&]{adapter.try_dispatch(g_cpu);},"opaque binding");}
 fresh();{ref::Adapter adapter(contract);g_ram[g_cpu.r[12].w[0]+208]^=1;refused([&]{adapter.try_dispatch(g_cpu);},"constant-block coherence");}
 fresh();{ref::Adapter adapter(contract);const auto object=g_cpu.r[19].w[0];put(object,1);set32(g_cpu.r[10],1);put(object+4,0);put(object+40,g_cpu.r[29].w[0]);set32(g_cpu.r[6],g_cpu.r[29].w[0]);set32(g_cpu.r[7],ld32(object+48));refused([&]{adapter.try_dispatch(g_cpu);},"output aliases");}
 auto suspended=[&](ref::Adapter&adapter){std::memcpy(g_ram,low_ram.data(),kRamSize);g_cpu=low;callback_ordinal=0;writes.clear();require(adapter.try_dispatch(g_cpu)&&adapter.pending()==1,"test suspension missing");g_cpu.pc&=~kUnwind;g_cpu.budget=33;writes.clear();};
 {ref::Adapter adapter(contract);suspended(adapter);g_cpu.f[12]^=1;refused([&]{adapter.try_dispatch(g_cpu);},"live step/stride");require(writes.empty(),"step refusal wrote stream data");}
 {ref::Adapter adapter(contract);suspended(adapter);g_cpu.r[8].w[0]+=16;refused([&]{adapter.try_dispatch(g_cpu);},"live step/stride");}
 {ref::Adapter adapter(contract);suspended(adapter);g_cpu.vu0.vf[4].w[0]^=1;refused([&]{adapter.try_dispatch(g_cpu);},"retained scratch/shared VU");}
 {ref::Adapter adapter(contract);suspended(adapter);g_cpu.r[31].w[0]+=4;refused([&]{adapter.try_dispatch(g_cpu);},"return continuation changed");}
 {ref::Adapter adapter(contract);suspended(adapter);put(g_cpu.r[19].w[0]+76,g_cpu.r[12].w[0]+16);refused([&]{adapter.try_dispatch(g_cpu);},"object code pointer changed");}
 {ref::Adapter adapter(contract);suspended(adapter);g_cpu.r[29].w[0]-=16;refused([&]{adapter.try_dispatch(g_cpu);},"orphan resume");}
 {ref::Adapter adapter(contract);suspended(adapter);ps2::kernel::test_thread=2;refused([&]{adapter.try_dispatch(g_cpu);},"orphan resume");ps2::kernel::test_thread=1;}
 {ref::Adapter adapter(contract);suspended(adapter);g_ram[g_cpu.r[12].w[0]]^=1;refused([&]{adapter.try_dispatch(g_cpu);},"code identity changed");}
 {ref::Adapter adapter(contract);suspended(adapter);adapter.retire_thread(1);require(!adapter.pending(),"retired context retained");refused([&]{adapter.try_dispatch(g_cpu);},"orphan resume");}
 {ref::Adapter adapter(contract);suspended(adapter);adapter.reset_after_machine_state_replacement();require(!adapter.pending(),"state replacement retained context");refused([&]{adapter.try_dispatch(g_cpu);},"orphan resume");}
 {ref::Adapter adapter(contract);std::memcpy(g_ram,low_ram.data(),kRamSize);g_cpu=low;g_cpu.pc=g_cpu.r[12].w[0]+0x30;refused([&]{adapter.try_dispatch(g_cpu);},"orphan resume");}
 {ref::Adapter adapter(contract);std::memcpy(g_ram,low_ram.data(),kRamSize);g_cpu=low;callback_clobber=true;refused([&]{adapter.try_dispatch(g_cpu);},"callback changed private CPU");callback_clobber=false;}
 {ref::Adapter adapter(contract);std::memcpy(g_ram,low_ram.data(),kRamSize);g_cpu=low;callback_retire=true;test_adapter=&adapter;refused([&]{adapter.try_dispatch(g_cpu);},"retired during original scheduler");callback_retire=false;test_adapter=nullptr;require(!adapter.pending(),"active retirement leaked context");}
 {ref::Adapter adapter(contract);std::memcpy(g_ram,low_ram.data(),kRamSize);g_cpu=low;callback_code_change=true;refused([&]{adapter.try_dispatch(g_cpu);},"code identity changed inside original callback");callback_code_change=false;require(!adapter.pending(),"changed callback code retained context");}
 std::printf("PASS_REFERENCE_ADAPTER original_full_CPU_RAM_order_comparisons=%u named_admission_lifetime_refusals=%u actual_entry_inputs=2 constructed_scheduler_service=1 runtime_adoption=NONE\n",comparisons,refusals);return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"ADAPTER_TEST_DIFFERENCE %s\n",e.what());return 1;}}
'''

if __name__=='__main__':raise SystemExit(main())
