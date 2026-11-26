"""Replay typed resource continuations against complete original AOT bodies.

The guard service is constructed and explicit. Original resume labels and all
reference bodies come from the pinned whole-program producer proof. No game
launch, actual scheduler claim, machine-context VM or runtime compiler.
"""
from pathlib import Path
import argparse
import json
import re
import subprocess
import sys

sys.path.insert(0,str(Path(__file__).resolve().parent))
import analyze_r192_builder_domain as original

PROOF='6a22a60c45bd46e78b00a737f519015809de187055fb2aac00bd8025099769ab'


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['pipeline-proof','candidate-build','compiler','output']:
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();out=a.output.resolve()
    original.need(out.is_relative_to((original.W/'build/oracle-telemetry').resolve()) and not out.exists(),'fresh_owned_output_required')
    receipt=a.pipeline_proof/'RESULT.json';original.need(original.pin(receipt)['sha256']==PROOF,'whole_resource_pipeline_proof_changed')
    result=json.loads(receipt.read_text());original.need(result['status']=='PASS_BOUNDED_RESOURCE_OUTPUT_PIPELINES' and not result['input_changes'],'whole_resource_pipeline_not_passed')
    for pin in result['source_pins']:original.need(original.pin(Path(pin['path']))==pin,'pipeline_proof_source_changed')
    fixture=a.pipeline_proof/'resource_pipeline_differential.cpp'
    original.need(any(original.pin(fixture)==r for r in result['artifacts']),'original_reference_fixture_changed')
    text=fixture.read_text();prefix=text[:text.index('int main(int argc,char**argv)')]
    old='bool preempt(Cpu&){throw std::runtime_error("offline producer budget exhausted");}'
    original.need(prefix.count(old)==1,'original_guard_service_seam_missing')
    prefix=prefix.replace(old,'std::int64_t test_refill=7;unsigned test_guard_calls=0;bool preempt(Cpu&c){++test_guard_calls;c.budget=test_refill;return true;}')
    resume_rows=[];remap_rows=[];inputs=[Path(__file__),Path(original.__file__),receipt,fixture,a.compiler]
    for case in result['programs']:
        unit=Path(case['original_generated']['path']);raw=Path(case['original_output']['path'])
        original.need(original.pin(unit)==case['original_generated'] and original.pin(raw)==case['original_output'],'compiled_original_program_changed')
        inputs += [unit,raw]
        generated=unit.read_text()
        resumed=sorted(set(re.findall(r'c\.pc = (0x[0-9A-F]+)u \| kUnwind;',generated)))
        original.need(len(resumed)==1,'one_source_loop_guard_required')
        pc=int(resumed[0],16);offset=pc-0x1c00000
        original.need(offset>=0 and offset%4==0 and offset<case['code_words']*4,'original_resume_outside_pinned_code')
        start='static void ref_'+case['name']+'(Cpu&c){'
        original.need(prefix.count(start)==1,'reference_function_entry_not_unique')
        prefix=prefix.replace(start,start+'if((c.pc&~1u)!=0x01c00000u){if((c.pc&~1u)=='+resumed[0]+'u)goto L_'+format(pc,'08X')+';throw std::runtime_error("unknown original resume");}')
        remap=re.findall(r'c\.f\[(\d+)\] = fp::cvt_w_s\(c\.f\[\1\]\);',generated)
        original.need(len(remap)==sum(e[0]==17 for e in case['effects']),'remap_working_scalar_transport_unproved')
        remap_rows.append(int(remap[0]) if remap else -1)
        resume_rows.append(dict(program=case['name'],pc=f'{pc:08X}',word=raw.read_bytes()[offset:offset+4].hex(),
                                word_encoding='original_little_endian_bytes',remap_scalar=int(remap[0]) if remap else None))
    header=original.W/'tools/research/r192_aot_modules/resource_pipeline_continuation.hpp'
    inputs += [header,header.with_name('semantic_resource_pipeline.hpp'),header.with_name('semantic_vector_effects.hpp'),
               *(a.candidate_build/'src/runtime/include/ps2').glob('*.hpp')]
    pins=[original.pin(x) for x in dict.fromkeys(inputs)]
    out.mkdir(parents=True)
    tail=TEST.replace('@REMAP@',','.join(map(str,remap_rows))).replace('@PROGRAMS@',a.pipeline_proof.resolve().as_posix())
    source=out/'resource_continuation.cpp'
    source.write_text(prefix+'\n#include "'+header.as_posix()+'"\n'+tail,encoding='utf8',newline='\n')
    compiler=[str(a.compiler),'-std=c++20','-mavx2','-ffp-contract=off','-O1','-static-libgcc','-static-libstdc++','-I'+str((a.candidate_build/'src/runtime/include').resolve())]
    commands=[]
    def run(command,name):
        cp=subprocess.run(command,capture_output=True);log=out/(name+'.log');log.write_bytes(cp.stdout+cp.stderr)
        commands.append(dict(argv=command,exit=cp.returncode,log=original.pin(log)));return cp
    exe=out/'resource_continuation.exe'
    original.need(run([*compiler,str(source),'-o',str(exe)],'compile').returncode==0,'continuation_compile_failed')
    checks=[]
    cp=run([str(exe)],'positive');checks.append(dict(name='positive',exit=cp.returncode,expected_exit=0,stdout=cp.stdout.decode(errors='replace'),stderr=cp.stderr.decode(errors='replace')))
    for name,old,new in [('wrong-guard-equality','if(budget<0&&service.invoke','if(budget<=0&&service.invoke'),
                         ('repeated-prelude','goto process_element;','budget-=static_cast<std::int64_t>(timing_.prelude);goto process_element;'),
                         ('lost-vector-sticky','current.vector.raised_sticky|=effects_.vector.raised_sticky;','current.vector.raised_sticky=0;'),
                         ('lost-remap-state','if(!current.remap_written&&effects_.remap_written)','if(false&& !current.remap_written&&effects_.remap_written)')]:
        copy=out/(name+'.hpp');raw=header.read_text();original.need(raw.count(old)==1,'plant_source_seam_missing:'+name)
        copy.write_text(raw.replace('#include "semantic_resource_pipeline.hpp"','#include "'+header.with_name('semantic_resource_pipeline.hpp').as_posix()+'"').replace(old,new),encoding='utf8',newline='\n')
        planted=out/(name+'.cpp');planted.write_text(source.read_text().replace(header.as_posix(),copy.as_posix()),encoding='utf8',newline='\n')
        target=out/(name+'.exe');original.need(run([*compiler,str(planted),'-o',str(target)],'compile-'+name).returncode==0,'plant_compile_failed:'+name)
        cp=run([str(target)],name);checks.append(dict(name=name,exit=cp.returncode,expected_exit=1,stdout=cp.stdout.decode(errors='replace'),stderr=cp.stderr.decode(errors='replace')))
    cp=run([str(exe),'--planted-callback-budget'],'wrong-callback-budget');checks.append(dict(name='wrong-callback-budget',exit=cp.returncode,expected_exit=1,stdout=cp.stdout.decode(errors='replace'),stderr=cp.stderr.decode(errors='replace')))
    changes=[x['path'] for x in pins if original.pin(Path(x['path']))!=x]
    passed=not changes and all(x['exit']==x['expected_exit'] for x in checks)
    record=dict(schema='r192_typed_resource_continuation_v1',status='PASS_BOUNDED_TYPED_CONTINUATION_BOUNDARIES' if passed else 'FAIL',
        invocation=[sys.executable,*sys.argv],source_pins=pins,input_changes=changes,original_resume_labels=resume_rows,
        commands=commands,checks=checks,artifacts=[original.pin(source),original.pin(exe)],
        limits=['All276 selected original-builder programs remain constructed inputs, not actual caller observations.',
                'Explicit constructed budget-refilling guard service; no actual scheduler/device effects or full Cpu/frame/context projection admission.',
                'No runtime integration, ownership/native credit, saved-state rehydration or whole producer-universe closure.'])
    dest=out/'RESULT.json';dest.write_text(json.dumps(record,indent=2)+'\n',encoding='utf8',newline='\n')
    print(json.dumps(dict(status=record['status'],sha256=original.pin(dest)['sha256'],result=str(dest))))
    return 0 if passed else 1


TEST=r'''
#include <type_traits>
static_assert(!std::is_copy_constructible_v<Continuation> && !std::is_move_constructible_v<Continuation>);
static constexpr int remap_registers[]={@REMAP@};
int main(int argc,char**argv){try{const std::string mode=argc>1?argv[1]:"";unsigned sessions=0,boundaries=0,suspensions=0,callback_total=0;
 for(unsigned pi=0;pi<std::size(programs);++pi){const auto&program=programs[pi];
  for(unsigned count:{0u,3u})for(unsigned alias:{0u,1u})for(unsigned numeric:{0u,1u})for(unsigned equality:{0u,1u}){
   std::memset(g_ram,0x5a,kRamSize);std::ifstream f(std::string("@PROGRAMS@/")+program.name+".bin",std::ios::binary);
   f.read(reinterpret_cast<char*>(g_ram+0x1c00000),(program.total+4)*4);if(!f||f.peek()!=EOF)throw std::runtime_error("program input");
   constexpr unsigned ia=0x100000,ib=0x200000;const unsigned oa=alias?ia+96:0x300000,ob=alias?ib:0x400000;
   constexpr unsigned special[]={0u,0x80000000u,0x7fffffffu,0x7fc00000u,0xff800000u,1u,0xbf800000u,0x7f7fffffu};
   for(unsigned i=0;i<6;++i){for(unsigned j=0;j<24;++j)st32(ia+i*96+j*4,numeric?special[(i+j+1)%8]:0x3f000000+i*0x100000+j*0x10000);st32(ib+i*16,numeric?special[(i+1)%8]:0x3f000000+i*0x100000);}
   auto descriptor=program.descriptor;const unsigned step=numeric?0x7f7fffff:0x3f000000;descriptor.scalar_step=descriptor.vector_step=step;
   Streams streams{{Stream{{reinterpret_cast<std::byte*>(g_ram+ia),576},{reinterpret_cast<std::byte*>(g_ram+oa),576},96},Stream{{reinterpret_cast<std::byte*>(g_ram+ib),96},{reinterpret_cast<std::byte*>(g_ram+ob),96},16}},count};
   Cpu c{};c.pc=0x1c00000;c.r[4].d[0]=ia;c.r[5].d[0]=ib;c.r[6].d[0]=oa;c.r[7].d[0]=ob;c.r[8].d[0]=96;c.r[9].d[0]=16;c.r[10].d[0]=count;c.r[11].d[0]=0x1c00000+program.code*4;c.r[12].d[0]=0x1c00000;c.r[29].d[0]=0x1fff000;c.r[31].d[0]=0x100000;c.fcr31=0x00800201;c.f[12]=step;c.vu0.vf[0].w[3]=0x3f800000;c.vu0.vf[1].w[0]=step;c.facc=0x5a5a5a5a;c.vu0.mac=0x8b3a;c.vu0.status=numeric?0xffff:0;
   const auto initial=c;const auto prepared=prepare(descriptor,streams);Continuation native(streams,descriptor);
   std::int64_t budget=timing(descriptor).prelude+(equality?2:1);c.budget=budget;test_refill=7;unsigned segment=0;
   for(;;){
    if(++segment>count+3)throw std::runtime_error("continuation did not converge");
    const std::vector<unsigned char>before(g_ram,g_ram+kRamSize);events.clear();test_guard_calls=0;program.run(c);
    const auto expected_events=events;const std::vector<unsigned char>expected(g_ram,g_ram+kRamSize);const bool suspended=(c.pc&1u)!=0;
    if((!suspended&&c.pc!=0x100000)||c.r[29].d[0]!=0x1fff000)throw std::runtime_error("original unknown exit");
    std::memcpy(g_ram,before.data(),kRamSize);events.clear();unsigned callback_calls=0;
    struct Service{unsigned*count;bool plant;}service{&callback_calls,mode=="--planted-callback-budget"};
    auto guard=[](void*p,std::int64_t&b){auto&s=*static_cast<Service*>(p);++*s.count;b=test_refill+(s.plant?1:0);return true;};
    const auto boundary=native.advance(budget,{&service,guard},{nullptr,&observe});budget=boundary.budget;const auto&effects=boundary.effects;
    auto vu=initial.vu0;
    for(unsigned i=0;i<descriptor.effect_count;++i)if(program.constant_vf[i]>=0)std::memcpy(&vu.vf[program.constant_vf[i]],prepared.vector_constants[i].data(),16);
    if(effects.element_observed)for(unsigned i=0;i<descriptor.attribute_count;++i)if(program.resource_vf[i]>=0)std::memcpy(&vu.vf[program.resource_vf[i]],effects.last_resources[i].vector.data(),16);
    if(effects.vector_written){vu.mac=effects.vector.last_mac;vu.status=effects.vector.project_status(initial.vu0.status);}
    if(effects.vector_accumulator_written)std::memcpy(&vu.acc,effects.vector_accumulator.data(),16);
    unsigned flags=initial.fcr31|effects.scalar_flags;if(effects.condition_written)flags=(flags&~0x00800000u)|(effects.condition?0x00800000u:0);
    const bool remap_diff=remap_registers[pi]>=0&&(effects.remap_written?effects.remap_word:initial.f[remap_registers[pi]])!=c.f[remap_registers[pi]];
    if((boundary.phase==Phase::suspended)!=suspended||callback_calls!=test_guard_calls||budget!=c.budget||boundary.accepted!=c.r[2].w[0]||boundary.consumed!=(c.r[4].d[0]-ia)/96||
       (suspended&&boundary.remaining_after_pending!=c.r[10].w[0])||flags!=c.fcr31||remap_diff||(effects.scalar_accumulator_written?effects.scalar_accumulator:initial.facc)!=c.facc||
       std::memcmp(&vu,&c.vu0,sizeof vu)||events!=expected_events||std::memcmp(g_ram,expected.data(),kRamSize)){
      std::fprintf(stderr,"CONTINUATION_BOUNDARY_DIFFERENCE program=%s count=%u alias=%u numeric=%u equality=%u segment=%u suspended=%u/%u callbacks=%u/%u budget=%lld/%lld accepted=%u/%u remap_difference=%u\n",program.name,count,alias,numeric,equality,segment,boundary.phase==Phase::suspended,suspended,callback_calls,test_guard_calls,(long long)budget,(long long)c.budget,boundary.accepted,c.r[2].w[0],remap_diff);return 1;}
    ++boundaries;suspensions+=suspended;callback_total+=callback_calls;if(!suspended)break;
   }
   bool retired=false;try{native.advance(budget,{nullptr,[](void*,std::int64_t&){return false;}});}catch(const std::runtime_error&e){retired=std::string(e.what()).find("retired")!=std::string::npos;}if(!retired)throw std::runtime_error("completed continuation reused");
   ++sessions;
  }
 }
 unsigned domain_controls=0;alignas(16)std::array<std::byte,32>input{},output{};unsigned half=0x3f000000;std::memcpy(input.data(),&half,4);
 Descriptor d{};d.attribute_count=1;d.attributes[0]={769,0,0};d.effect_count=1;d.effects[0].id=1;d.effects[0].destination_tag=769;d.scalar_step=0x3f800000;
 Streams small{{Stream{input,output,4},Stream{}},1};
 auto unused=[](void*p,std::int64_t&){if(p)++*static_cast<unsigned*>(p);return true;};
 auto refusal=[&](auto action,const char*reason){try{action();}catch(const std::runtime_error&e){if(std::string(e.what()).find(reason)!=std::string::npos){++domain_controls;return;}}throw std::runtime_error("domain control accepted or wrong reason");};
 {Continuation x(small,d);unsigned calls=0;const auto b=x.advance(4,{&calls,unused});if(b.phase!=Phase::complete||calls||b.budget!=-14)throw std::runtime_error("exact zero guard changed");++domain_controls;}
 {output.fill(std::byte{0x95});const auto before=output;Continuation x(small,d);refusal([&]{x.advance(INT64_MIN,{nullptr,unused});},"budget arithmetic overflow");if(output!=before)throw std::runtime_error("initial overflow wrote");refusal([&]{x.advance(0,{nullptr,unused});},"failed context");}
 {output.fill(std::byte{0x95});const auto before=output;Continuation x(small,d);const auto b=x.advance(0,{nullptr,[](void*,std::int64_t&v){v=0;return true;}});if(b.phase!=Phase::suspended)throw std::runtime_error("overflow pending seam");refusal([&]{x.advance(INT64_MIN,{nullptr,unused});},"budget arithmetic overflow");if(output!=before)throw std::runtime_error("resume overflow wrote");}
 {Continuation x(small,d);refusal([&]{x.advance(0,{});},"guard service absent");}
 {Continuation x(small,d);refusal([&]{x.advance(0,{&x,[](void*p,std::int64_t&){static_cast<Continuation*>(p)->advance(0,{nullptr,[](void*,std::int64_t&){return false;}});return true;}});},"recursive advance");refusal([&]{x.advance(0,{nullptr,unused});},"failed context");}
 {Continuation x(small,d);refusal([&]{x.advance(0,{nullptr,[](void*,std::int64_t&)->bool{throw std::runtime_error("constructed callback fault");}});},"constructed callback fault");refusal([&]{x.advance(0,{nullptr,unused});},"failed context");}
 {Streams zero{};Continuation x(zero,d);unsigned calls=0;const auto b=x.advance(4,{&calls,unused});if(b.phase!=Phase::complete||b.accepted||calls||b.budget!=-2)throw std::runtime_error("zero-count empty span");++domain_controls;}
 {auto constants=d;constants.effects[0].id=6;constants.effects[0].constant[0]=half;constants.scalar_step=half;Continuation x(small,constants);x.advance(0,{nullptr,[](void*,std::int64_t&v){v=7;return true;}});constants.scalar_step=0x7fffffff;constants.effects[0].constant[0]=0x7fffffff;const auto b=x.advance(10000,{nullptr,unused});unsigned actual;std::memcpy(&actual,output.data(),4);if(b.phase!=Phase::complete||actual!=0x3f400000)throw std::runtime_error("owned prepared snapshot changed");++domain_controls;}
 std::printf("PASS original_whole_programs=%zu typed_sessions=%u boundary_comparisons=%u true_suspensions=%u guard_callbacks=%u retired_refusals=%u additional_domain_lifetime_controls=%u full_RAM_ordered_writes_FCR_facc_fullVU_budget=1 full_Cpu_actual_scheduler_admission=0\n",std::size(programs),sessions,boundaries,suspensions,callback_total,sessions,domain_controls);return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"REFUSED %s\n",e.what());return 2;}}
'''


if __name__=='__main__':
    raise SystemExit(main())
