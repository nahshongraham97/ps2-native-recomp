"""Full CPU/RAM non-mutation checks for the read-only code-identity observer."""
from pathlib import Path
import argparse
import json
import subprocess

import verify_r192_resource_word_equality as auth

ROOT=Path(__file__).resolve().parents[2]

HOST=r'''
namespace ps2 {alignas(64) std::uint8_t g_ram[kRamSize+16];}
static void put(unsigned at,unsigned value){std::memcpy(ps2::g_ram+at,&value,4);}
int main(){
 try{
  unsigned checked=0;ResourceCodeObserver observer;
  for(const auto&c:cases){
   constexpr unsigned base=0x1c00000,object=0x1c80000,owner=0x1c90000,attributes=0x1ca0000;
   std::memset(ps2::g_ram,0x95,ps2::kRamSize);
   std::ifstream file(c.file,std::ios::binary);std::vector<char> raw((std::istreambuf_iterator<char>(file)),{});
   std::memcpy(ps2::g_ram+base,raw.data(),raw.size());
   put(object+76,base);put(object+72,owner);put(object+916,c.pool);put(object+192,c.e.size());put(object+80,c.f.size());
   put(owner+536,c.a.size());put(owner+540,attributes);
   for(unsigned i=0;i<c.a.size();++i){put(attributes+12*i,c.a[i].tag);put(attributes+12*i+4,c.a[i].stream);put(attributes+12*i+8,c.a[i].offset);}
   for(unsigned i=0;i<c.e.size();++i){put(object+208+48*i,c.e[i].id);put(object+216+48*i,c.e[i].destination_tag);put(object+224+48*i,c.e[i].source_tag);}
   for(unsigned i=0;i<c.f.size();++i){put(object+84+24*i,c.f[i].id);put(object+88+24*i,c.f[i].source_tag);put(object+100+24*i,0);put(object+104+24*i,0);}
   ps2::Cpu cpu;std::memset(&cpu,0xa5,sizeof cpu);cpu.r[31].d[0]=0x2e6180;cpu.r[19].d[0]=object;cpu.pc=cpu.base=base;
   const auto before=cpu;const std::vector<std::uint8_t> memory(ps2::g_ram,ps2::g_ram+ps2::kRamSize);
   const auto entries=observer.observed_entries();observer.inspect(cpu);
   if(std::memcmp(&cpu,&before,sizeof cpu)||std::memcmp(ps2::g_ram,memory.data(),memory.size())){std::cerr<<"REFUSED observer_changed_CPU_or_RAM "<<c.name<<"\n";return 1;}
   cpu.base=0x818000;const auto other_window=cpu;observer.inspect(cpu);
   if(observer.observed_entries()!=entries+2){std::cerr<<"REFUSED observer_distinct_image_base_missed "<<c.name<<"\n";return 1;}
   if(std::memcmp(&cpu,&other_window,sizeof cpu)||std::memcmp(ps2::g_ram,memory.data(),memory.size())){std::cerr<<"REFUSED observer_changed_CPU_or_RAM "<<c.name<<"\n";return 1;}
   ++checked;
  }
  std::cout<<"PASS read_only_observer full_CPU_and_32MiB_RAM_cases="<<checked<<" runtime_admission=0\n";
 }catch(const std::exception&e){std::cerr<<"REFUSED "<<e.what()<<"\n";return 2;}
}
'''


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['word-proof','candidate-build','compiler','output']:p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();out=a.output.resolve();proof=a.word_proof/'RESULT.json'
    auth.auth.need(out.is_relative_to(ROOT/'build/oracle-telemetry') and not out.exists(),'fresh_owned_output_required')
    auth.auth.need(auth.digest(proof)=='78bbfc99ff7f2413ce68c05ea6081b0e9244b5209585e0303f833ee7f7e65f30','original_word_proof_changed')
    record=json.loads(proof.read_text());fixture=(a.word_proof/'resource_word_equality.cpp').resolve()
    auth.auth.need(any(Path(row['path'])==fixture and auth.digest(fixture)==row['sha256'] for row in record['artifacts']),'original_word_fixture_changed')
    header=ROOT/'tools/research/r192_aot_modules/reference_resource_code_observer.hpp'
    sources=[Path(__file__),Path(auth.__file__),proof,fixture,header,a.compiler,*[Path(row['path']) for row in record['source_pins']]]
    before=[dict(path=str(q.resolve()),sha256=auth.digest(q)) for q in dict.fromkeys(sources)]
    prefix=fixture.read_text().split('int main(){',1)[0];out.mkdir(parents=True)
    cpp=out/'observer_nonmutation.cpp';cpp.write_text('#include "'+header.as_posix()+'"\n'+prefix+'\n'+HOST,newline='\n')
    flags=[str(a.compiler),'-std=c++20','-O2','-mavx2','-ffp-contract=off','-static-libgcc','-static-libstdc++','-I'+str((a.candidate_build/'src/runtime/include').resolve())];commands=[]
    def run(argv,name):
        cp=subprocess.run(argv,capture_output=True,timeout=120);q=out/(name+'.log');q.write_bytes(cp.stdout+cp.stderr);commands.append(dict(argv=argv,exit=cp.returncode,log=str(q),log_sha256=auth.digest(q)));return cp
    exe=out/'observer_nonmutation.exe';compile=run([*flags,str(cpp),'-o',str(exe)],'compile');auth.auth.need(compile.returncode==0,'observer_nonmutation_compile_failed')
    positive=run([str(exe)],'positive');auth.auth.need(positive.returncode==0 and b'full_CPU_and_32MiB_RAM_cases=281' in positive.stdout,'observer_nonmutation_failed')
    # A private compiled error makes the observer write unrelated reference RAM.
    text=header.read_text();needle='++calls_;Descriptor d{};';auth.auth.need(text.count(needle)==1,'observer_mutation_seam_changed');text=text.replace(needle,'++calls_;ps2::g_ram[32]^=1;Descriptor d{};')
    for h in ['reference_resource_word_equality.hpp']:
        text=text.replace('#include "'+h+'"','#include "'+(header.parent/h).as_posix()+'"')
    mutant=out/'mutating_observer.hpp';mutant.write_text(text,newline='\n');badcpp=out/'mutating_observer.cpp';badcpp.write_text(cpp.read_text().replace(header.as_posix(),mutant.as_posix()),newline='\n');badexe=out/'mutating_observer.exe'
    auth.auth.need(run([*flags,str(badcpp),'-o',str(badexe)],'compile-mutation').returncode==0,'observer_mutant_compile_failed')
    plant=run([str(badexe)],'mutation');auth.auth.need(plant.returncode==1 and b'observer_changed_CPU_or_RAM' in plant.stderr,'mutating_observer_not_rejected')
    old_gate=header.read_text();needle='if(pc!=base){';auth.auth.need(old_gate.count(needle)==1,'observer_old_gate_mutation_seam_changed');old_gate=old_gate.replace(needle,'if(c.base!=base)return;'+needle)
    old_gate=old_gate.replace('#include "reference_resource_word_equality.hpp"','#include "'+(header.parent/'reference_resource_word_equality.hpp').as_posix()+'"')
    gate_header=out/'wrong_image_base.hpp';gate_header.write_text(old_gate,newline='\n');gate_cpp=out/'wrong_image_base.cpp';gate_cpp.write_text(cpp.read_text().replace(header.as_posix(),gate_header.as_posix()),newline='\n');gate_exe=out/'wrong_image_base.exe'
    auth.auth.need(run([*flags,str(gate_cpp),'-o',str(gate_exe)],'compile-image-base-mutation').returncode==0,'observer_gate_mutant_compile_failed')
    gate=run([str(gate_exe)],'image-base-mutation');auth.auth.need(gate.returncode==1 and b'observer_distinct_image_base_missed' in gate.stderr,'wrong_image_base_gate_not_rejected')
    changes=[row['path'] for row in before if auth.digest(row['path'])!=row['sha256']];auth.auth.need(not changes,'observer_test_input_changed')
    result=dict(status='PASS_READ_ONLY_OBSERVER_FULL_STATE_CONTROLS',source_pins=before,input_changes=changes,invocation=__import__('sys').argv,commands=commands,positive_stdout=positive.stdout.decode(),compiled_mutations=['unrelated_RAM_write_REJECTED','wrong_image_base_gate_REJECTED'],observations=positive.stderr.decode(),scope='281 constructed original-code/candidate-descriptor inputs, each with equal and distinct image bases (562 calls), compare complete Cpu and32MiBRAM before/after observer only; not game/runtime/owner admission.')
    q=out/'RESULT.json';q.write_text(json.dumps(result,indent=2)+'\n',newline='\n');print(json.dumps(dict(status=result['status'],result_sha256=auth.digest(q))))


if __name__=='__main__':main()
