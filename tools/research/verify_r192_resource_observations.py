"""Offline original-builder joins and typed shadows of real no-guard segments.

No runtime route changes. Physical binding uses the original allocator. Actual
pool words are explicit inputs; no emitted-opcode decoding or reference-post
register borrowing is used to construct native expectations.
"""
from pathlib import Path
import argparse
import json
import subprocess
import sys

sys.path.insert(0,str(Path(__file__).resolve().parent))
import analyze_r192_builder_domain as auth


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--observations',type=Path,action='append',required=True)
    p.add_argument('--expected-observation-sha',action='append',required=True)
    for name in ['builder-proof','candidate-build','compiler','output']:
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();out=a.output.resolve()
    auth.need(out.is_relative_to((auth.W/'build/oracle-telemetry').resolve()) and not out.exists(),'fresh_owned_output_required')
    auth.need(len(a.observations)==len(a.expected_observation_sha),'observation_hash_denominator')
    receipt=a.builder_proof/'RESULT.json'
    auth.need(auth.pin(receipt)['sha256']=='ffa154fa526bd0af42c3404f9454ee45a28a196b9936f82281fc61fa062a603c','original_builder_proof_changed')
    builder=a.builder_proof/'original_builder.cpp';record=json.loads(receipt.read_text())
    auth.need(any(auth.pin(builder)==r for r in record['source_pins']),'original_builder_source_changed')
    inputs=[Path(__file__),Path(auth.__file__),receipt,builder,a.compiler];cases=[];observed=[]
    for path,digest in zip(a.observations,a.expected_observation_sha):
        r=path/'RESULT.json';auth.need(auth.pin(r)['sha256']==digest,'observation_proof_changed')
        rec=json.loads(r.read_text());auth.need(rec['status']=='PASS_BOUNDED_ORIGINAL_PLAN_OBSERVATIONS' and not rec['input_changes'],'original_observation_not_passed')
        for pin in rec['observations']:
            auth.need(auth.pin(Path(pin['path']))==pin,'original_observation_bytes_changed');inputs.append(Path(pin['path']))
        inputs.append(r);observed.append(auth.pin(r))
        for key,status in rec['summary']['case_status'].items():
            meta=path/'observations'/('case-'+key+'.json');m=json.loads(meta.read_text())
            if status!='COMPLETE_USABLE_OBSERVATION':continue
            auth.need(not list(meta.parent.glob(meta.stem+'-segment-*-guard-*.bin')),'actual_scheduler_shadow_not_implemented')
            auth.need(not meta.with_name(meta.stem+'-segment-2-before-Cpu.bin').exists(),'multi_segment_shadow_not_implemented')
            cases.append(dict(name=path.name+'-'+meta.stem,prefix=meta.with_name(meta.stem+'-segment-1').as_posix(),object=m['object'],base=m['base'],words=m['source_total_words'],data=m['source_data_start_words']))
    auth.need(0<len(cases)<=12,'bounded_observation_cases_required')
    root=auth.W/'tools/research/r192_aot_modules';projection=root/'reference_resource_projection.hpp'
    inputs += [projection,root/'reference_resource_layout.hpp',root/'resource_pipeline_continuation.hpp',root/'semantic_resource_pipeline.hpp',root/'semantic_vector_effects.hpp',*(a.candidate_build/'src/runtime/include/ps2').glob('*.hpp')]
    pins=[auth.pin(x) for x in dict.fromkeys(inputs)];out.mkdir(parents=True)
    rows=','.join('{"'+c['name']+'","'+c['prefix']+'",'+','.join(str(c[k])+'u' for k in ['object','base','words','data'])+'}' for c in cases)
    prefix=builder.read_text().split('static void read(',1)[0];source=out/'actual_resource_shadow.cpp'
    source.write_text(prefix+'\n#include "'+projection.as_posix()+'"\n'+TEST.replace('@CASES@',rows),encoding='utf8',newline='\n')
    compiler=[str(a.compiler),'-std=c++20','-mavx2','-ffp-contract=off','-O1','-static-libgcc','-static-libstdc++','-I'+str((a.candidate_build/'src/runtime/include').resolve())];commands=[]
    def run(argv,name):
        cp=subprocess.run(argv,capture_output=True);log=out/(name+'.log');log.write_bytes(cp.stdout+cp.stderr)
        commands.append(dict(argv=argv,exit=cp.returncode,log=auth.pin(log)));return cp
    exe=out/'actual_resource_shadow.exe';auth.need(run([*compiler,str(source),'-o',str(exe)],'compile').returncode==0,'actual_resource_shadow_compile_failed');checks=[]
    for flag in ['', '--planted-code-word','--planted-pool-value','--planted-global-overwrite','--planted-untouched-VU','--planted-output-byte','--planted-budget']:
        cp=run([str(exe)]+([flag] if flag else []),flag.removeprefix('--planted-') or 'positive')
        checks.append(dict(name=flag or 'positive',exit=cp.returncode,expected_exit=1 if flag else 0,stdout=cp.stdout.decode(errors='replace'),stderr=cp.stderr.decode(errors='replace')))
    changes=[pin['path'] for pin in pins if auth.pin(Path(pin['path']))!=pin];passed=not changes and all(r['exit']==r['expected_exit'] for r in checks)
    result=dict(schema='r192_actual_resource_no_guard_shadow_v1',status='PASS_ACTUAL_NO_GUARD_TYPED_SHADOWS' if passed else 'FAIL',invocation=[sys.executable,*sys.argv],source_pins=pins,input_changes=changes,commands=commands,checks=checks,observations=observed,cases=cases,artifacts=[auth.pin(source),auth.pin(exe)],
        limits=['Offline shadow of observed original execution, not native runtime adoption.',
                'Completed first-entry segments without guards only; callbacks/resumptions require separate current-state evidence.',
                'All32MiB RAM/all1456 Cpu bytes compared. Actual runtime write order unmeasured; ordered writes have separate constructed original-body proof.',
                'Original producer joins executable words; two padding NOPs and data alignment holes are explicitly unexecuted/unwritten and excluded.',
                'Literal fourth vector words/scalars/remap/filter data are explicit allocator-addressed pool inputs, not assumed fresh descriptor values.',
                'Unrelated current context fields are preserved; private/shared VU changes reject. Reference post-Cpu never fills native expectations.',
                'Partial/clamp domains, orphan rehydration, unknown plans and general producer lifecycle remain OPEN.'])
    dest=out/'RESULT.json';dest.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8',newline='\n');print(json.dumps(dict(status=result['status'],result=str(dest),sha256=auth.pin(dest)['sha256'])));return 0 if passed else 1


TEST=r'''
using namespace mka::emitted::reference_resources;
struct Case{const char*name,*prefix;unsigned object,base,words,data;};
static void read_exact(const std::string&name,void*p,unsigned size){std::ifstream f(name,std::ios::binary);f.read(static_cast<char*>(p),size);if(!f||f.peek()!=EOF)throw std::runtime_error("capture length mismatch");}
static Descriptor descriptor(const Cpu&c,unsigned object){
 Descriptor d{};const auto owner=ld32(object+72);d.attribute_count=ld32(owner+536);d.effect_count=ld32(object+192);d.filter_count=ld32(object+80);
 if(!d.attribute_count||d.attribute_count>20||d.effect_count>13||d.filter_count>1)refuse("descriptor count unproved");
 const auto list=ld32(owner+540);if(list>kRamSize-12*d.attribute_count)refuse("attribute extent unproved");
 for(unsigned i=0;i<d.attribute_count;++i)d.attributes[i]={ld32(list+12*i),ld32(list+12*i+4),ld32(list+12*i+8)};
 for(unsigned i=0;i<d.effect_count;++i){auto&e=d.effects[i];const auto at=object+208+48*i;e.id=ld16(at);e.destination_tag=ld32(at+8);
  if(e.id==2||e.id==3||e.id==25||e.id==17)e.source_tag=ld32(at+16);
  for(unsigned k=0;k<3;++k)e.constant[k]=ld32(at+16+4*k);
  e.remap={std::int32_t(ld32(at+32)),std::int32_t(ld32(at+36)),ld32(at+24),ld32(at+28)};
 }
 for(unsigned i=0;i<d.filter_count;++i){const auto at=object+84+24*i;if(ld32(at+16))refuse("filter mode unproved");d.filters[i]={ld16(at),ld32(at+4),ld32(at+20)};}
 d.scalar_step=c.f[12];d.vector_step=c.vu0.vf[1].w[0];return d;
}
static unsigned pool_offset(const Layout&p,Slot slot){for(unsigned i=0;i<p.node_count;++i){const auto&n=p.nodes[i];if(n.slot.index==slot.index&&n.slot.bank==slot.bank){if(n.stream!=2)refuse("literal ownership unproved");return n.offset;}}refuse("literal slot absent");}
// Reference-only current-state patch. Constants/resources/status originate in
// typed algorithm side outputs. No source-post register value is borrowed.
static Cpu apply_current(const Cpu&current,const Cpu&entry,const Prepared&p,const Boundary&b){
 const auto layout=derive_layout(p.descriptor);const auto&e=b.effects;
 if(b.phase!=Phase::complete||b.consumed!=entry.r[10].w[0]||b.accepted>b.consumed||b.budget<INT32_MIN||b.budget>INT32_MAX)refuse("complete no-guard boundary unproved");
 if(std::memcmp(&current.vu0,&entry.vu0,sizeof current.vu0)||std::memcmp(current.f,entry.f,sizeof current.f)||current.facc!=entry.facc||current.fcr31!=entry.fcr31||std::memcmp(current.r,entry.r,sizeof current.r))refuse("unproved private or shared VU change");
 Cpu out;std::memcpy(&out,&current,sizeof out);
 auto scalar=[&](Slot s,Bits v){if(s.present()){if(s.bank!=1)refuse("scalar source binding bank");out.f[s.index]=v;}};
 auto vector=[&](Slot s,const Vector&v){if(s.present()){if(s.bank!=0)refuse("vector source binding bank");std::memcpy(&out.vu0.vf[s.index],v.data(),16);}};
 scalar(layout.zero,0);
 for(unsigned i=0;i<p.descriptor.effect_count;++i){const auto&s=layout.effects[i];if(s.constant.present()){if(s.constant.bank==0)vector(s.constant,p.vector_constants[i]);else scalar(s.constant,p.scalar_constants[i]);}scalar(s.upper,p.upper[i]);scalar(s.scale,p.scale[i]);scalar(s.start,p.descriptor.effects[i].remap.start);scalar(s.extent,p.descriptor.effects[i].remap.extent);if(e.remap_written)scalar(s.work,e.remap_word);}
 for(unsigned i=0;i<p.descriptor.filter_count;++i)scalar(layout.filters[i],p.descriptor.filters[i].limit);
 if(e.element_observed){for(unsigned i=0;i<p.descriptor.attribute_count;++i){const auto&s=layout.resources[i];const auto&v=e.last_resources[i];if(s.bank==0)vector(s,v.vector);else if(s.bank==1)scalar(s,v.scalar);else if(s.bank==2)out.r[s.index].sd[0]=std::int32_t(v.scalar);else if(s.bank==3)out.r[s.index].d[0]=v.integer;else refuse("resource source binding bank");}if(layout.retained.present())std::memcpy(&out.r[layout.retained.index],e.last_retained.data(),16);}
 if(e.scalar_accumulator_written)out.facc=e.scalar_accumulator;if(e.vector_accumulator_written)std::memcpy(&out.vu0.acc,e.vector_accumulator.data(),16);
 if(e.vector_written){out.vu0.mac=e.vector.last_mac;out.vu0.status=e.vector.project_status(current.vu0.status);}
 out.fcr31=current.fcr31|e.scalar_flags;if(e.condition_written)out.fcr31=(out.fcr31&~0x00800000u)|(e.condition?0x00800000u:0);
 out.r[2].d[0]=b.accepted;
 for(unsigned i=4;i<8;++i)out.r[i].d[0]=entry.r[i].d[0]+std::uint64_t(i<6?b.consumed:b.accepted)*entry.r[(i&1)?9:8].d[0];
 out.r[10].sd[0]=-1;out.budget=std::int32_t(b.budget);out.pc=entry.r[31].w[0];return out;
}
int main(int argc,char**argv){try{const std::string mode=argc>1?argv[1]:"";unsigned completed=0,code_words=0,ambient=0;const Case cases[]={@CASES@};
 for(const auto&test:cases){Cpu entry{},actual{};read_exact(std::string(test.prefix)+"-before-Cpu.bin",&entry,sizeof entry);read_exact(std::string(test.prefix)+"-after-Cpu.bin",&actual,sizeof actual);read_exact(std::string(test.prefix)+"-before-RAM.bin",g_ram,kRamSize);
  const std::vector<unsigned char>original(g_ram,g_ram+kRamSize);std::vector<unsigned char>expected(kRamSize);read_exact(std::string(test.prefix)+"-after-RAM.bin",expected.data(),kRamSize);
  if(entry.pc!=test.base||entry.r[31].d[0]!=0x2e6180||entry.r[19].d[0]!=test.object||actual.pc!=0x2e6180)refuse("original caller identity unbound");
  auto d=descriptor(entry,test.object);const auto plan=derive_layout(d);plan.require_intact();if(plan.total_words!=test.words||plan.data_start_words!=test.data||ld32(test.object+916)!=test.data)refuse("source layout mismatch");
  // Original producer executes offline, preserving the immutable captures.
  constexpr unsigned output=0x1c00000,arena=0x1c10000,blocks=0x1c20000,list=0x1c30000;
  for(unsigned i=0;i<d.attribute_count;++i)std::memcpy(g_ram+list+i*12,&d.attributes[i],12);st32(list+d.attribute_count*12,0xffffffff);std::memset(g_ram+output,0x95,8000);
  Cpu builder=entry;builder.r[4].d[0]=list;builder.r[5].d[0]=test.object;builder.r[6].d[0]=output;builder.r[7].d[0]=2000;builder.r[8].d[0]=arena;builder.r[9].d[0]=3803;builder.r[10].d[0]=blocks;builder.r[11].d[0]=400;builder.r[29].d[0]=0x1fff000;builder.r[31].d[0]=0x100000;builder.budget=10000000;reference(builder,0x2ed4b0);
  if(builder.pc!=0x100000||builder.r[2].w[0]!=test.words||ld32(test.object+916)!=test.data)refuse("original builder extent mismatch");
  const unsigned active_words=plan.intended[0]+plan.intended[1]+plan.intended[3]+plan.intended[4]+plan.intended[5]+plan.intended[6]-2;
  if(mode=="--planted-code-word")g_ram[output]^=1;
  for(unsigned i=0;i<active_words;++i){if(std::memcmp(g_ram+output+4*i,original.data()+test.base+4*i,4)){std::fprintf(stderr,"SOURCE_WORD_DIFFERENCE %s word=%u\n",test.name,i);return 1;}++code_words;}
  std::memcpy(g_ram,original.data(),kRamSize);
  auto literal=[&](unsigned offset,unsigned bytes){const std::uint64_t at=std::uint64_t(test.base)+4*plan.data_start_words+offset;if(at+bytes>kRamSize||offset>0x7fff)refuse("source literal bounds unproved");return unsigned(at);};
  for(unsigned i=0;i<d.effect_count;++i){const auto&s=plan.effects[i];if(s.constant.present()){const auto at=literal(s.constant_offset,s.constant.bank==0?16:4);if(s.constant.bank==0){std::memcpy(d.effects[i].constant.data(),g_ram+at,16);++ambient;}else d.effects[i].constant[0]=ld32(at);}}
  for(unsigned i=0;i<d.filter_count;++i)d.filters[i].limit=ld32(literal(pool_offset(plan,plan.filters[i]),4));
  Streams streams{};streams.count=entry.r[10].w[0];for(unsigned i=0;i<2;++i){const auto in=entry.r[4+i].d[0],to=entry.r[6+i].d[0],stride=entry.r[8+i].d[0];if(in>kRamSize||to>kRamSize)refuse("source stream bounds unproved");streams.streams[i]={{reinterpret_cast<std::byte*>(g_ram+in),kRamSize-in},{reinterpret_cast<std::byte*>(g_ram+to),kRamSize-to},std::size_t(stride)};}
  auto prepared=prepare(d,streams);
  for(unsigned i=0;i<d.effect_count;++i)if(d.effects[i].id==17){const auto&s=plan.effects[i];prepared.upper[i]=ld32(literal(pool_offset(plan,s.upper),4));prepared.scale[i]=ld32(literal(pool_offset(plan,s.scale),4));prepared.descriptor.effects[i].remap.start=ld32(literal(pool_offset(plan,s.start),4));prepared.descriptor.effects[i].remap.extent=ld32(literal(pool_offset(plan,s.extent),4));}
  if(mode=="--planted-pool-value")prepared.descriptor.scalar_step^=0x00800000;
  const auto timed=run_no_yield(streams,prepared,entry.budget);Boundary boundary{Phase::complete,streams.count,timed.result.accepted,0,timed.remaining_budget,timed.result};
  Cpu current;std::memcpy(&current,&entry,sizeof current);if(mode=="--planted-untouched-VU")current.vu0.vf[31].w[2]^=1;
  auto projected=apply_current(current,entry,prepared,boundary);
  // Independent stale-entry control: preserve unrelated CURRENT context.
  auto external=current;external.base^=0x10000;external.cop0[9]^=0x01000000;const auto preserved=apply_current(external,entry,prepared,boundary);
  if(preserved.base!=external.base||preserved.cop0[9]!=external.cop0[9])refuse("current external state restored from stale entry");
  if(mode=="--planted-global-overwrite")projected.base^=0x10000;if(mode=="--planted-budget")++projected.budget;if(mode=="--planted-output-byte")g_ram[entry.r[6].w[0]]^=1;
  if(std::memcmp(&projected,&actual,sizeof actual)||std::memcmp(g_ram,expected.data(),kRamSize)){unsigned cpu=0,ram=0;while(cpu<sizeof actual&&reinterpret_cast<const unsigned char*>(&projected)[cpu]==reinterpret_cast<const unsigned char*>(&actual)[cpu])++cpu;while(ram<kRamSize&&g_ram[ram]==expected[ram])++ram;std::fprintf(stderr,"ACTUAL_SHADOW_DIFFERENCE %s Cpu_byte=%u RAM_byte=%08X budget=%d/%d accepted=%u/%u\n",test.name,cpu,ram,projected.budget,actual.budget,boundary.accepted,actual.r[2].w[0]);return 1;}
  std::printf("PASS %s count=%u accepted=%u code_words=%u Cpu_bytes=%zu RAM_bytes=%u\n",test.name,streams.count,boundary.accepted,active_words,sizeof actual,kRamSize);++completed;
 }
 std::printf("PASS completed_actual_no_guard_shadows=%u original_builder_code_words=%u explicit_vector_fetches=%u actual_runtime_store_order=UNMEASURED native_adoption=0\n",completed,code_words,ambient);return 0;
}catch(const std::exception&e){std::fprintf(stderr,"REFUSED %s\n",e.what());return 1;}}
'''


if __name__=='__main__':raise SystemExit(main())
