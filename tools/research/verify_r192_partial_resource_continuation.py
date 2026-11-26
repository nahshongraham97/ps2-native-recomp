"""Replay partial typed continuations and full isolated machine observations.

Guards are explicit constructed budget services, not native scheduler proof.
"""
from pathlib import Path
import argparse
import json
import re
import subprocess
import sys

import analyze_r192_builder_domain as auth


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['partial-proof','candidate-build','compiler','output']:
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();out=a.output.resolve()
    auth.need(out.is_relative_to(auth.W/'build/oracle-telemetry') and not out.exists(),'fresh_owned_output_required')
    receipt=a.partial_proof/'RESULT.json'
    auth.need(auth.pin(receipt)['sha256']=='15ec24a6dbb3479c1aafda6d476d60080e43f764a44fa00f0b3562b6bfaffac4','whole_partial_proof_changed')
    proof=json.loads(receipt.read_text())
    for pin in proof['source_pins']:auth.need(auth.pin(Path(pin['path']))==pin,'partial_source_changed')
    fixture=a.partial_proof/'partial_resource_differential.cpp'
    auth.need(any(auth.pin(fixture)==pin for pin in proof['artifacts']),'partial_reference_fixture_changed')
    prefix=fixture.read_text().split('int main(int argc,char**argv)',1)[0]
    old='bool preempt(Cpu&){throw std::runtime_error("offline producer budget exhausted");}'
    auth.need(prefix.count(old)==1,'original_guard_service_seam_changed')
    prefix=prefix.replace(old,'unsigned reference_calls=0,service_choice=0;std::int64_t refill=1;bool preempt(Cpu&c){++reference_calls;c.budget=static_cast<std::int32_t>(refill);return service_choice==0||(service_choice==2&&reference_calls%2);}')
    inputs=[Path(__file__),receipt,fixture,a.compiler]
    resumes=[]
    for program in proof['programs']:
        unit=Path(program['generated']['path']);raw=Path(program['original']['path'])
        auth.need(auth.pin(unit)==program['generated'] and auth.pin(raw)==program['original'],'original_partial_program_changed')
        inputs+=[unit,raw]
        targets=set(re.findall(r'c\.pc = (0x[0-9A-F]+)u \| kUnwind;',unit.read_text()))
        auth.need(len(targets)==1,'one_original_partial_loop_guard_required')
        target=next(iter(targets));pc=int(target,16)
        start='static void ref_'+program['name']+'(Cpu&c){'
        auth.need(prefix.count(start)==1,'partial_reference_entry_not_unique')
        prefix=prefix.replace(start,start+'if((c.pc&~1u)!=0x01c00000u){if((c.pc&~1u)=='+target+'u)goto L_'+f'{pc:08X}'+';throw std::runtime_error("unknown partial resume");}')
        resumes.append(dict(name=program['name'],source_resume_PC=f'{pc:08X}',source_word=raw.read_bytes()[pc-0x1c00000:pc-0x1c00000+4].hex()))
    header=auth.W/'tools/research/r192_aot_modules/partial_resource_continuation.hpp'
    projection=header.with_name('reference_partial_resource_projection.hpp')
    inputs += [header,projection,header.with_name('partial_resource_pipeline.hpp'),header.with_name('resource_pipeline_continuation.hpp')]
    pins=[auth.pin(x) for x in dict.fromkeys(inputs)]
    out.mkdir(parents=True);commands=[]
    source=out/'partial_continuation.cpp'
    source.write_text(prefix+'\n#include "'+projection.as_posix()+'"\n'+TEST.replace('@PROGRAMS@',a.partial_proof.resolve().as_posix()),encoding='utf8',newline='\n')
    args=[str(a.compiler),'-std=c++20','-mavx2','-ffp-contract=off','-O1','-static-libgcc','-static-libstdc++','-I'+str((a.candidate_build/'src/runtime/include').resolve())]
    def run(command,name):
        cp=subprocess.run(command,capture_output=True,timeout=240);log=out/(name+'.log');log.write_bytes(cp.stdout+cp.stderr)
        commands.append(dict(argv=command,exit=cp.returncode,log=auth.pin(log)));return cp
    exe=out/'partial_continuation.exe'
    auth.need(run([*args,str(source),'-o',str(exe)],'compile').returncode==0,'partial_continuation_compile_failed')
    checks=[]
    def check(binary,name,expected,diagnostic=None):
        cp=run([str(binary)],name);checks.append(dict(name=name,exit=cp.returncode,expected=expected,expected_diagnostic=diagnostic,stdout=cp.stdout.decode(errors='replace'),stderr=cp.stderr.decode(errors='replace')))
    check(exe,'positive',0)
    for name,old,new in [('wrong-guard-equality','if(budget<0&&service.invoke','if(budget<=0&&service.invoke'),
                         ('repeated-prelude','}else goto process_element;','}else{budget-=prepared_.prelude;goto process_element;}'),
                         ('lost-prior-remap','if(!current.remap_written&&effects_.remap_written)','if(false&&!current.remap_written&&effects_.remap_written)'),
                         ('lost-vector-sticky','current.vector.raised_sticky|=effects_.vector.raised_sticky;','current.vector.raised_sticky=0;')]:
        mutated=out/(name+'.hpp');text=header.read_text();auth.need(text.count(old)==1,'plant_seam_changed:'+name)
        text=text.replace('#include "partial_resource_pipeline.hpp"','#include "'+header.with_name('partial_resource_pipeline.hpp').as_posix()+'"').replace('#include "resource_pipeline_continuation.hpp"','#include "'+header.with_name('resource_pipeline_continuation.hpp').as_posix()+'"')
        mutated.write_text(text.replace(old,new),encoding='utf8',newline='\n')
        projected=out/(name+'-projection.hpp');projected.write_text(projection.read_text().replace('#include "partial_resource_continuation.hpp"','#include "'+mutated.as_posix()+'"'),encoding='utf8',newline='\n')
        cpp=out/(name+'.cpp');cpp.write_text(source.read_text().replace(projection.as_posix(),projected.as_posix()),encoding='utf8',newline='\n')
        binary=out/(name+'.exe');auth.need(run([*args,str(cpp),'-o',str(binary)],'compile-'+name).returncode==0,'partial_plant_compile_failed:'+name)
        check(binary,name,1)
    # Removing the reference check must be caught by a mismatched omitted
    # caller word, even though every existing successful vector binds it right.
    wrong=out/'invent-omitted-caller.hpp';text=projection.read_text()
    old='else if(entry.f[layout.filters[i].index]!=prepared.filter_limits[i])'
    auth.need(text.count(old)==1,'omitted_caller_plant_seam_changed')
    wrong.write_text(text.replace('#include "partial_resource_continuation.hpp"','#include "'+header.as_posix()+'"').replace(old,'else if(false)'),encoding='utf8',newline='\n')
    cpp=out/'invent-omitted-caller.cpp';cpp.write_text(source.read_text().replace(projection.as_posix(),wrong.as_posix()),encoding='utf8',newline='\n')
    binary=out/'invent-omitted-caller.exe';auth.need(run([*args,str(cpp),'-o',str(binary)],'compile-invent-omitted-caller').returncode==0,'omitted_caller_plant_compile_failed');check(binary,'invent-omitted-caller',2,'REFUSED invented omitted caller binding accepted')
    wrong=out/'unused-stream.hpp';text=header.read_text();auth.need(text.count('if(!referenced)continue;')==1,'unused_stream_plant_seam_changed')
    text=text.replace('#include "partial_resource_pipeline.hpp"','#include "'+header.with_name('partial_resource_pipeline.hpp').as_posix()+'"').replace('#include "resource_pipeline_continuation.hpp"','#include "'+header.with_name('resource_pipeline_continuation.hpp').as_posix()+'"')
    wrong.write_text(text.replace('if(!referenced)continue;',''),encoding='utf8',newline='\n')
    projected=out/'unused-stream-projection.hpp';projected.write_text(projection.read_text().replace('#include "partial_resource_continuation.hpp"','#include "'+wrong.as_posix()+'"'),encoding='utf8',newline='\n')
    cpp=out/'unused-stream.cpp';cpp.write_text(source.read_text().replace(projection.as_posix(),projected.as_posix()),encoding='utf8',newline='\n')
    binary=out/'unused-stream.exe';auth.need(run([*args,str(cpp),'-o',str(binary)],'compile-unused-stream').returncode==0,'unused_stream_plant_compile_failed');check(binary,'unused-stream',2,'REFUSED partial resource continuation: stream slice bounds')
    changes=[x['path'] for x in pins if auth.pin(Path(x['path']))!=x]
    passed=not changes and all(c['exit']==c['expected'] and (c['expected_diagnostic'] is None or c['expected_diagnostic'] in c['stderr']) for c in checks)
    result=dict(status='PASS_BOUNDED_PARTIAL_CONTINUATION_FULL_CPU' if passed else 'FAIL',invocation=[sys.executable,*sys.argv],source_pins=pins,input_changes=changes,original_resume_words=resumes,commands=commands,checks=checks,artifacts=[auth.pin(source),auth.pin(exe)],limits=['Five constructed original programs, including four source-positive partial shapes; not actual caller captures.','Every compared suspension/return covers fullRAM, ordered writes and1456Cpu bytes under explicit budget-only guard services.','Projection starts from isolated entry; do not use it to overwrite current global fields after actual interleaving.','Actual stream/code owner lifetime, scheduler side effects, currentCpu patch and runtime adoption remain OPEN. No native credit.'])
    target=out/'RESULT.json';target.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8',newline='\n')
    print(json.dumps(dict(status=result['status'],sha256=auth.pin(target)['sha256'],checks=checks)))
    return 0 if passed else 1


TEST=r'''
static bool service(void*,std::int64_t&budget){++native_calls;budget=refill;return service_choice==0||(service_choice==2&&native_calls%2);}
'''
# Keep the native service counter outside the original ps2 namespace.
TEST='static unsigned native_calls=0;\n'+TEST+r'''
int main(){try{unsigned sessions=0,boundaries=0,suspensions=0,guards=0,retired=0;
 for(auto&program:programs)for(unsigned count:{0u,3u,8u})for(unsigned alias:{0u,1u})for(unsigned numeric:{0u,1u,2u})for(unsigned prior_limit:{0u,0x3f800000u,0x7fffffffu})for(unsigned choice:{0u,1u,2u})for(unsigned equality:{0u,1u}){
  service_choice=choice;refill=7;reference_calls=native_calls=0;
  std::memset(g_ram,0x5a,kRamSize);std::ifstream f(std::string("@PROGRAMS@/")+program.name+".bin",std::ios::binary);f.read(reinterpret_cast<char*>(g_ram+0x1c00000),(program.total+4)*4);if(!f||f.peek()!=EOF)throw std::runtime_error("program size");
  constexpr unsigned ia=0x100000,ib=0x200000;const unsigned oa=alias?ia+352:0x300000,ob=alias?ib:0x400000;
  constexpr unsigned special[]={0u,0x80000000u,0x7fffffffu,0x7fc00000u,0xff800000u,1u,0xbf800000u,0x7f7fffffu};
  for(unsigned i=0;i<10;++i){for(unsigned j=0;j<88;++j)st32(ia+i*352+j*4,numeric?special[(i+j+numeric)%8]:0x3f000000+i*0x100000+j*0x10000);st32(ib+i*32,numeric?special[(i+numeric)%8]:0x3f000000+i*0x100000);}
  Cpu c;std::memset(&c,0xa5,sizeof c);std::memset(&c.r[0],0,16);c.pc=0x1c00000;c.r[4].d[0]=ia;c.r[5].d[0]=ib;c.r[6].d[0]=oa;c.r[7].d[0]=ob;c.r[8].d[0]=352;c.r[9].d[0]=32;c.r[10].d[0]=count;c.r[11].d[0]=0x1c00000+program.code*4;c.r[12].d[0]=0x1c00000;c.r[29].d[0]=0x1fff000;c.r[31].d[0]=0x100000;
  for(unsigned reg=1;reg<32;++reg){c.f[reg]=0x3f000000+reg;for(unsigned lane=0;lane<4;++lane)c.vu0.vf[reg].w[lane]=0x3f800000+reg*0x100+lane;}
  for(unsigned lane=0;lane<4;++lane)c.vu0.acc.w[lane]=special[lane+numeric];
  const unsigned step=numeric==0?0x3f000000:numeric==1?0x80000000:0x7f7fffff;
  c.fcr31=numeric==0?0x00800201:numeric==1?0xffffffff:0;c.f[12]=c.vu0.vf[1].w[0]=step;c.vu0.vf[0]={};c.vu0.vf[0].w[3]=0x3f800000;c.vu0.mac=0x5321;c.vu0.status=numeric?0xffff:0x1234;c.facc=0x5a5a5a5a;
  for(int reg:program.filter_regs)if(reg>=0)c.f[reg]=prior_limit;
  partial::Bindings bindings{};for(unsigned i=0;i<program.descriptor.body.effect_count;++i){const auto id=program.descriptor.body.effects[i].id;if(id!=5&&id!=6&&id!=26)continue;Vector raw{};const unsigned at=c.r[11].w[0]+program.constant_offset[i];if(program.constant_vf[i]>=0)std::memcpy(raw.data(),g_ram+(at&~15u),16);else raw[0]=ld32(at);bindings.constant_words[i]=raw;}
  for(unsigned i=0;i<program.descriptor.filter_count;++i)bindings.omitted_filter_limits[i]=c.f[program.filter_regs[i]];
  Streams streams{{Stream{{reinterpret_cast<std::byte*>(g_ram+ia),3520},{reinterpret_cast<std::byte*>(g_ram+oa),3520},352},Stream{{reinterpret_cast<std::byte*>(g_ram+ib),320},{reinterpret_cast<std::byte*>(g_ram+ob),320},32}},count};
  auto descriptor=program.descriptor;descriptor.body.scalar_step=descriptor.body.vector_step=step;
  partial::Continuation native(streams,descriptor,bindings);c.budget=static_cast<std::int32_t>(native.prepared().prelude+2-equality);const auto initial=c;std::int64_t native_budget=c.budget;
  auto original_memory=std::vector<unsigned char>(g_ram,g_ram+kRamSize),native_memory=original_memory;
  for(unsigned segment=0;;++segment){if(segment>32)throw std::runtime_error("unbounded continuation");
   std::memcpy(g_ram,original_memory.data(),kRamSize);events.clear();program.run(c);original_memory.assign(g_ram,g_ram+kRamSize);const auto expected_events=events;
   std::memcpy(g_ram,native_memory.data(),kRamSize);events.clear();const auto boundary=native.advance(native_budget,{nullptr,&service},{nullptr,&observe});native_budget=boundary.budget;native_memory.assign(g_ram,g_ram+kRamSize);
   const auto projected=partial::project(initial,0x1c00000,native.prepared(),boundary);
   if(events!=expected_events||std::memcmp(g_ram,original_memory.data(),kRamSize)||std::memcmp(&projected,&c,sizeof c)||reference_calls!=native_calls){unsigned byte=0;while(byte<sizeof c&&reinterpret_cast<const unsigned char*>(&projected)[byte]==reinterpret_cast<const unsigned char*>(&c)[byte])++byte;
    std::fprintf(stderr,"PARTIAL_CONTINUATION_DIFFERENCE %s count=%u alias=%u numeric=%u limit=%X choice=%u eq=%u segment=%u CpuByte=%u PC=%X/%X budget=%lld/%d guards=%u/%u writes=%zu/%zu\n",program.name,count,alias,numeric,prior_limit,choice,equality,segment,byte,projected.pc,c.pc,(long long)native_budget,c.budget,native_calls,reference_calls,events.size(),expected_events.size());return 1;}
   if(std::string(program.name)=="source_partial_load17"){
    auto invented=initial;invented.f[program.filter_regs[3]]^=1;bool refused=false;
    try{partial::project(invented,0x1c00000,native.prepared(),boundary);}catch(const std::runtime_error&e){refused=std::string(e.what()).find("omitted filter caller word mismatch")!=std::string::npos;}
    if(!refused)throw std::runtime_error("invented omitted caller binding accepted");
   }
   ++boundaries;if(boundary.phase==mka::emitted::resources::Phase::complete)break;++suspensions;
  }
  guards+=native_calls;bool refused=false;try{native.advance(100,{nullptr,&service});}catch(const std::runtime_error&e){refused=std::string(e.what()).find("retired")!=std::string::npos;}if(!refused)throw std::runtime_error("completed context reused");++retired;++sessions;
 }
 // An unused stream may be empty and retain a nonzero original stride. It
 // has no logical memory accesses and must never be subspanned beyond size0.
 std::array<std::byte,64>input{},output{};for(unsigned i=0;i<input.size();++i)input[i]=std::byte(i);
 partial::Descriptor d{};d.body.attribute_count=1;d.body.attributes[0]={0x300,0,0};Streams s{{Stream{input,output,16},Stream{{},{},32}},3};
 partial::Continuation unused(s,d,{});std::int64_t budget=0;service_choice=0;unsigned segments=0;
 for(;;){if(++segments>5)throw std::runtime_error("unused stream context did not complete");const auto b=unused.advance(budget,{nullptr,&service});budget=b.budget;if(b.phase==mka::emitted::resources::Phase::complete){if(b.accepted!=3||std::memcmp(input.data(),output.data(),48))throw std::runtime_error("unused stream identity result");break;}}
 std::printf("PASS partial_continuation_sessions=%u full_RAM_ordered_writes_full1456Cpu_boundaries=%u actual_suspensions=%u guard_calls=%u completed_reuse_refusals=%u unused_empty_nonzero_stride_stream=1 omitted_caller_binding_controls=1 actual_scheduler_admission=0\n",sessions,boundaries,suspensions,guards,retired);return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"REFUSED %s\n",e.what());return 2;}}
'''


if __name__=='__main__':raise SystemExit(main())
