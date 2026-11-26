"""Bounded original caller-frame connection proof; adoption gates stay explicit.

Restores a captured saved frame into a CONSTRUCTED caller entry, then compares
the original fast path with a typed semantic call at its actual call seam.
Undefined volatile values are not invented for a whole-Cpu checksum.
"""
from pathlib import Path
import argparse
import csv
import hashlib
import json
import re
import subprocess
import sys

sys.path.insert(0,str(Path(__file__).resolve().parent))
import analyze_r192_builder_domain as audit
import verify_r192_semantic_pipeline as pipeline_test


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['elf','listing','references','boundary','candidate-build','compiler','output']:
        p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--source-fork',type=Path,default=audit.W.parent/'ps2-native-recomp')
    a=p.parse_args();out=a.output.resolve()
    audit.need(out.is_relative_to((audit.W/'build/oracle-telemetry').resolve()) and not out.exists(),'fresh_owned_output_required')
    audit.need(audit.pin(a.elf)['sha256']==audit.ELF_SHA and audit.pin(a.listing)['sha256']==audit.LISTING_SHA,'original_inputs_changed')
    original=audit.Original(a.elf.read_bytes());unit=a.candidate_build/'generated_ee/unit_0079.cpp'
    original_unit=a.source_fork/'build/r192-aot-closure/generated_ee/unit_0079.cpp'
    audit.need(unit.read_bytes()==original_unit.read_bytes(),'original_C_generated_caller_changed')
    text=unit.read_bytes().decode().replace('\r\n','\n')
    fast=text[text.index('L_002E60E0:'):text.index('L_002E6188:')]
    fast+=text[text.index('L_002E6DA0:'):text.index('L_002E6DF0:')]
    audit.need(fast.count('call(c, t, depth + 1u)')==1,'caller_call_seam_changed')
    fast=fast.replace('call(c, t, depth + 1u)','invoke(c, native)')
    fast+='\nL_002E6188:throw std::runtime_error("non-emitted original path outside this proof");\n'
    spans=[(0x2e60e0,0x2e6188),(0x2e6da0,0x2e6dec),(0x2e9d00,0x2e9d78)]
    words=[];calls=[]
    with a.listing.open(encoding='utf8',newline='') as f:
        for row in csv.DictReader(f,delimiter='\t'):
            pc=int(row['address'],16);word=int.from_bytes(bytes.fromhex(row['bytes']),'little')
            if word>>26==3 and ((pc+4)&0xf0000000)|((word&0x3ffffff)<<2)==0x2e60e0:
                audit.need(original.word(pc)==word,'caller_JAL_word_changed');calls.append(dict(pc=f'{pc:08X}',word=f'{word:08X}',function=row['function_entry'],delay_word=f'{original.word(pc+4):08X}'))
            if any(lo<=pc<hi for lo,hi in spans):
                audit.need(original.word(pc)==word,'caller_listing_word_changed');words.append(dict(pc=f'{pc:08X}',word=f'{word:08X}',mnemonic=row['mnemonic']))
    reference_receipt=a.references/'RESULT.json';audit.need(audit.pin(reference_receipt)['sha256']=='a7b1464c1c472611f3b537471473f6831af268cb6fcf1c6c4fb05835e4c29275','original_reference_receipt_changed')
    original_references=json.loads(reference_receipt.read_text());reference_source=a.references/'reference_programs.cpp'
    audit.need(audit.pin(reference_source)['sha256']==original_references['fixture_pins'][0]['sha256'],'original_reference_source_changed')
    raw_program=Path(original_references['programs'][0]['program_pin']['path']);audit.need(audit.pin(raw_program)['sha256']==original_references['programs'][0]['program_pin']['sha256'],'original_program_changed')
    r=reference_source.read_bytes().decode().replace('\r\n','\n');r=r.split('\n#include <fstream>\nstruct Program',1)[0]
    helper=pipeline_test.TEST[pipeline_test.TEST.index('namespace typed='):pipeline_test.TEST.index('static unsigned build(')]
    helper=helper[:helper.index('static bool compare(')]
    backend=audit.W/'tools/research/r192_aot_modules/semantic_pipeline.hpp'
    fixture=r+'\n#include <fstream>\n#include "'+backend.as_posix()+'"\n'+SLOW+helper.replace('static Program programs[]={@ROWS@};','static Program programs[]={{"regenerated",61,52,0,&reference_regenerated}};').replace('@BOUNDARY@',a.boundary.resolve().as_posix())
    words_array=','.join(f'0x{x:08x}u' for x in __import__('struct').unpack('<61I',raw_program.read_bytes()))
    fixture+=CALLER.replace('@ORIGINAL_CALLER@',fast).replace('@PROGRAM_WORDS@',words_array)
    out.mkdir(parents=True);cpp=out/'caller.cpp';cpp.write_text(fixture,encoding='utf8',newline='\n');exe=out/'caller.exe'
    inputs=[Path(__file__),Path(audit.__file__),Path(pipeline_test.__file__),backend,backend.with_name('semantic_vector_effects.hpp'),backend.with_name('semantic_call_scratch.hpp'),a.elf,a.listing,unit,original_unit,reference_receipt,reference_source,raw_program,a.compiler,
            *sorted(a.boundary.glob('*.bin')),*sorted((a.candidate_build/'src/runtime/include/ps2').glob('*.hpp'))]
    pins=[audit.pin(x) for x in inputs]
    command=[str(a.compiler),'-std=c++20','-mavx2','-ffp-contract=off','-O1','-static-libgcc','-static-libstdc++',
             '-I'+str((a.candidate_build/'src/runtime/include').resolve()),str(cpp),'-o',str(exe)]
    cp=subprocess.run(command,capture_output=True);(out/'compile.log').write_bytes(cp.stdout+cp.stderr);audit.need(cp.returncode==0,'caller_fixture_compile_failed')
    runs=[]
    for name,args,expected,reason in [('positive',[],0,'PASS defined_caller_frame'),
          ('planted-result',['--planted-result'],1,'CALLER_DEFINED_DIFFERENCE'),
          ('planted-preserved-upper',['--planted-preserved-upper'],1,'CALLER_DEFINED_DIFFERENCE'),
          ('planted-FCR-control',['--planted-FCR-control'],1,'CALLER_DEFINED_DIFFERENCE'),
          ('planted-target',['--planted-target'],2,'original callee identity outside observed family')]:
        run=subprocess.run([str(exe),*args],capture_output=True);(out/(name+'.log')).write_bytes(run.stdout+run.stderr)
        runs.append(dict(name=name,argv=[str(exe),*args],exit=run.returncode,expected_exit=expected,expected_reason=reason,
                         stdout=run.stdout.decode(errors='replace'),stderr=run.stderr.decode(errors='replace')))
    changed=[r['path'] for r in pins if audit.pin(Path(r['path']))['sha256']!=r['sha256']]
    passed=not changed and all(r['exit']==r['expected_exit'] and r['expected_reason'] in r['stdout']+r['stderr'] for r in runs)
    result=dict(status='PASS_DEFINED_CALLER_FRAME_ADOPTION_OPEN' if passed else 'FAIL',runs=runs,compile_command=command,source_pins=pins,fixture_pins=[audit.pin(cpp),audit.pin(exe)],input_changes=changed,
                original_word_pins=words,direct_callers=calls,
                projection={'memory':'All32MiB RAM plus ordered writes including RA64/saved-register128/FPR32 frame stores and all pipeline writes.',
                            'preserved':'GPR16..23/28..31 including upper128-bit lanes; FPR12/20..31;HI/LO/SA;original return PC;FCR control bits outside original overflow+condition writes.',
                            'volatile':'Not synthesized to match a whole-Cpu checksum. No claim about unused volatile FPR/GPR/VU fields.',
                            'seed':'CONSTRUCTED caller entry from actual saved frame and object/inputstep provenance. No observed caller-entry capture.'},
                adoption_gates=['FCR status/condition and VU arithmetic status/accumulator downstream liveness or exact defined projection remain unproved.',
                                'Budget decrement and original loop-guard preempt/event/refill/unwind scheduling are NOT implemented by typed kernel.',
                                'Indirect caller completeness and production state/thread lifecycle remain open. No disposable/runtime adoption or credit.'])
    target=out/'RESULT.json';target.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8',newline='\n');print(json.dumps(dict(status=result['status'],result=str(target),sha256=audit.pin(target)['sha256'])))
    return 0 if passed else 1


SLOW=r'''
namespace ps2{std::uint16_t slow_ld16(std::uint32_t){throw std::runtime_error("nonRAM halfword");}std::uint64_t slow_ld64(std::uint32_t){throw std::runtime_error("nonRAM frame ld64");}void slow_st64(std::uint32_t,std::uint64_t){throw std::runtime_error("nonRAM frame st64");}}
'''

CALLER=r'''
static typed::Pipeline admitted;
static void invoke(Cpu&c,bool native){
 constexpr unsigned program[]={@PROGRAM_WORDS@};const auto base=c.r[12].w[0];
 if(base>kRamSize-sizeof program||c.r[11].d[0]!=base+208||std::memcmp(g_ram+base,program,sizeof program))throw std::runtime_error("original callee identity outside observed family");
 if(!native){reference_regenerated(c);return;}auto result=typed::run(streams(c),admitted,{nullptr,&observe});set32(c.r[2],result.accepted);c.pc=0x2e6180;
}
static void frame_store64(unsigned address,std::uint64_t value){Write w{address,8,{}};std::memcpy(w.value.data(),&value,8);writes.push_back(w);st64(address,value);}
#define st64 frame_store64
#define st32 checked_st32
#define st128 checked_st128
static void caller(Cpu&c,bool native){unsigned depth=0;@ORIGINAL_CALLER@}
#undef st64
#undef st32
#undef st128
int main(int argc,char**argv){try{
 mode=argc>1?argv[1]:"";Cpu seed{};read("Cpu.bin",&seed,sizeof seed);read("RAM.bin",g_ram,kRamSize);original_object=seed.r[19].w[0];
 for(unsigned i=0;i<4;++i){unsigned r=original_object+208+i*48;auto&e=captured[i];e=effect(ld16(r),ld32(r+8));if(e.id==5)for(unsigned lane=0;lane<3;++lane)e.constant[lane]=ld32(r+16+lane*4);if(e.id==6)e.constant[0]=ld32(r+16);
  if(e.id==17){e.source_tag=ld32(r+16);e.remap.start=ld32(r+24);e.remap.extent=ld32(r+28);e.remap.upper=ld32(r+32);e.remap.lower=ld32(r+36);}}
 captured[1].constant[3]=captured[2].constant[0];filter_limit=ld32(original_object+84+20);admitted=pipeline("regenerated",seed.f[12],seed.r[11].w[0]);
 const unsigned frame=seed.r[29].w[0];if(frame>kRamSize-496||(frame&15u))throw std::runtime_error("saved frame extent");
 const unsigned registers[]={30,23,22,21,20,19,18,17,16};for(unsigned i=0;i<9;++i)ld128(frame+160-i*16,seed.r[registers[i]]);
 seed.r[31].d[0]=ld64(frame+176);for(unsigned i=0;i<7;++i)seed.f[20+i]=ld32(frame+i*4);seed.r[29].d[0]=frame+496;seed.r[4].d[0]=original_object;seed.pc=0x2e60e0;
 if(mode=="--planted-target")put(original_object+76,ld32(original_object+76)+16);
 std::vector<unsigned char> before(g_ram,g_ram+kRamSize),expected(kRamSize);Cpu reference=seed,candidate=seed;
 writes.clear();preempt_calls=0;yield_requested=false;caller(reference,false);auto trace=writes;auto calls=preempt_calls;std::memcpy(expected.data(),g_ram,kRamSize);
 std::memcpy(g_ram,before.data(),kRamSize);writes.clear();preempt_calls=0;
 if(mode=="--planted-result")admitted.filter_limit=0xff7fffff;
 caller(candidate,true);
 if(mode=="--planted-preserved-upper")candidate.r[16].d[1]^=1;
 if(mode=="--planted-FCR-control")candidate.fcr31^=1;
 bool good=!std::memcmp(expected.data(),g_ram,kRamSize)&&trace==writes&&calls==0&&candidate.pc==reference.pc;
 for(unsigned r:{16,17,18,19,20,21,22,23,28,29,30,31})good&=!std::memcmp(&candidate.r[r],&reference.r[r],16);
 for(unsigned r:{12,20,21,22,23,24,25,26,27,28,29,30,31})good&=candidate.f[r]==reference.f[r];
 good&=!std::memcmp(&candidate.hi,&reference.hi,16)&&!std::memcmp(&candidate.lo,&reference.lo,16)&&candidate.sa==reference.sa;
 constexpr unsigned original_scalar_status_writes=0x8010u|kFpuCondition;
 good&=((candidate.fcr31^reference.fcr31)&~original_scalar_status_writes)==0;
 if(!good){std::fprintf(stderr,"CALLER_DEFINED_DIFFERENCE writes=%zu/%zu return=%08X/%08X\n",writes.size(),trace.size(),candidate.pc,reference.pc);return 1;}
 std::printf("PASS defined_caller_frame full_RAM_ordered_writes=%zu direct_result_store=1 preserved_upper_lanes=1 constructed_saved_frame_seed=1 observed_caller_entry=0 FCR_status_difference=%08X budget_original=%d budget_candidate=%d scheduler_adoption=UNPROVED\n",writes.size(),candidate.fcr31^reference.fcr31,reference.budget,candidate.budget);return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"REFUSED %s\n",e.what());return 2;}}
'''

if __name__=='__main__':raise SystemExit(main())
