"""Compare source-positive partial programs with the typed capacity policy.

The original source builder and every original output/physical binding are
pinned. All generated reference decoding occurs offline in this test tool.
This does not admit actual caller/scheduler/ambient-owner bindings.
"""
from pathlib import Path
import argparse
import json
import re
import shutil
import subprocess
import sys

import analyze_r192_builder_domain as auth
import specialize_r192_stub_family as family


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['layout-proof','pipeline-proof','candidate-build','generator','compiler','output']:
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();out=a.output.resolve()
    auth.need(out.is_relative_to(auth.W/'build/oracle-telemetry') and not out.exists(),'fresh_owned_output_required')
    lp=a.layout_proof/'RESULT.json';pp=a.pipeline_proof/'RESULT.json'
    auth.need(auth.pin(lp)['sha256']=='a4fa1d05c3f8136d17c799cc4b2debc5593ec5691264bd691b1c7696ed6b13c3','original_layout_proof_changed')
    auth.need(auth.pin(pp)['sha256']=='6a22a60c45bd46e78b00a737f519015809de187055fb2aac00bd8025099769ab','original_pipeline_proof_changed')
    layout=json.loads(lp.read_text());prior=json.loads(pp.read_text())
    for row in layout['source_pins']+prior['source_pins']:auth.need(auth.pin(Path(row['path']))==row,'source_dependency_changed')
    auth.need(auth.pin(a.generator)['sha256']==family.GENERATOR_SHA,'original_v8_generator_changed')
    auth.need(auth.pin(a.compiler)['sha256']=='ee85bef2c733e24a26c22be3df94bd631cf9528a048fce91241c182a936737fe','compiler_changed')
    fixture=a.pipeline_proof/'resource_pipeline_differential.cpp'
    auth.need(any(auth.pin(fixture)==r for r in prior['artifacts']),'original_reference_prefix_changed')
    header=auth.W/'tools/research/r192_aot_modules/partial_resource_pipeline.hpp'
    inputs=[Path(__file__),Path(auth.__file__),Path(family.__file__),lp,pp,fixture,a.generator,a.compiler,
            header,header.with_name('reference_resource_layout.hpp'),header.with_name('semantic_resource_pipeline.hpp'),
            header.with_name('semantic_vector_effects.hpp'),*(a.candidate_build/'src/runtime/include/ps2').glob('*.hpp')]
    cases=[r for r in layout['rows'] if r['name'].startswith('source_partial_') or r['name']=='intact_main53_unique_tags']
    auth.need(len(cases)==5,'four_partial_one_intact_control_required')
    out.mkdir(parents=True);commands=[];functions=[];rows=[];proofs=[]
    def run(command,name,timeout=180):
        auth.need(shutil.disk_usage(out).free>(12<<30),'disk_reserve_required')
        cp=subprocess.run(list(map(str,command)),capture_output=True,timeout=timeout)
        log=out/(name+'.log');log.write_bytes(cp.stdout+cp.stderr)
        commands.append(dict(argv=list(map(str,command)),exit=cp.returncode,log=auth.pin(log)))
        return cp
    for case in cases:
        name=case['name'];raw=Path(case['raw_original_output']['path']);ob=Path(case['raw_observation']['path'])
        auth.need(auth.pin(raw)==case['raw_original_output'] and auth.pin(ob)==case['raw_observation'],'original_partial_output_changed')
        inputs += [raw,ob];observed=json.loads(ob.read_text());total=observed['return_words'];code=observed['source_data_start_words']
        data=raw.read_bytes();auth.need(len(data)==(total+4)*4,'original_partial_extent_changed')
        target=out/(name+'.bin');target.write_bytes(data)
        window=out/(name+'-window.bin');window.write_bytes(data+b'\xff'*max(0,1024-len(data)))
        generated=out/name
        argv=[a.generator,'--live-image',window,'--base','01C00000','--entry','01C00000','--stamp','A2659FA9BD846661','--dense-checks','--out',generated]
        auth.need(run(argv,name+'-generate').returncode==0,'original_partial_generation_refused:'+name)
        unit=next(p for p in generated.glob('m_*unit_*.cpp') if 'L_01C00000:' in p.read_text())
        text=unit.read_text();body=text[text.index('L_01C00000:'):].split('\n}\n',1)[0]
        auth.need(not re.search(r'\b(?:call|dispatch|unit_\w+)\s*\(',body),'unproved_original_external_call:'+name)
        functions.append('static void ref_'+name+'(Cpu&c){'+body+'}\n')
        nodes=case['node_mapping'];resource_vf=[n[1] if n[2]==0 else -1 for n in nodes[13:13+len(case['attributes'])]]
        # Derive per-effect literal positions from the measured ORIGINAL nodes.
        # Constants are exactly nodes with a nonzero data-stream owner base;
        # synthetic zero and remap work nodes do not fetch memory.
        after=13+len(case['attributes'])+int(any(a[0]==259 for a in case['attributes']))
        literal_nodes=[n for n in nodes[after:] if n[3]]
        at=0;constant_offsets=[0]*13;constant_vf=[-1]*13
        init='[](){mka::emitted::partial_resources::Descriptor p{};auto&d=p.body;'
        for i,attr in enumerate(case['attributes']):init+=f'd.attributes[{i}]={{'+','.join(map(str,attr))+'};'
        init+=f'd.attribute_count={len(case["attributes"])};d.effect_count={len(case["effects"])};p.filter_count={case["filters"]};'
        for i,e in enumerate(case['effects']):
            init+=f'd.effects[{i}].id={e[0]};d.effects[{i}].destination_tag={e[2]};d.effects[{i}].source_tag={e[4]};'
            if e[0] in (5,6,26):
                node=literal_nodes[at];at+=1;offset=node[4]&65535;offset=offset-65536 if offset&32768 else offset
                if node[2]==0:offset&=~15;constant_vf[i]=node[1]
                constant_offsets[i]=offset
                # Every binding must occur in actual original load statements.
                if node[2]==0:
                    expression=r'ld128\(\(c\.r\[11\]\.w\[0\]'+(rf' \+ {offset}u' if offset>0 else rf' - {-offset if offset!=-16 else 1}u' if offset<0 else '')+r'\) & ~15u, c\.vu0\.vf\['+str(node[1])+r'\]\);'
                    auth.need(re.search(expression,body),'original_vector_literal_binding_missing:'+name+':'+str(i))
                else:
                    auth.need(re.search(r'c\.f\['+str(node[1])+r'\] = ld32\(\(c\.r\[11\]\.w\[0\](?: \+ '+str(offset)+r'u)?\)\);',body),'original_scalar_literal_binding_missing:'+name)
            if e[0]==17:
                at+=4;init+=f'd.effects[{i}].remap={{255,0,{e[6]}u,{e[7]}u}};'
        filter_regs=[n[1] for n in literal_nodes[at:]]
        auth.need(len(filter_regs)==case['filters'],'original_filter_literal_denominator')
        for i,reg in enumerate(filter_regs):init+=f'p.filters[{i}]={{2,769,0x3f800000u}};'
        init+='return p;}()'
        rows.append('{"'+name+'",'+str(total)+','+str(code)+',&ref_'+name+','+init+',{'+','.join(map(str,resource_vf+[-1]*(20-len(resource_vf))))+'},{'+','.join(map(str,constant_vf))+'},{'+','.join(map(str,constant_offsets))+'},{'+','.join(map(str,filter_regs+[-1]*(4-len(filter_regs))))+'}}')
        proofs.append(dict(name=name,source_partial_components=case['partial_emission_components'],original=auth.pin(raw),generated=auth.pin(unit),literal_offsets=constant_offsets,filter_caller_slots=filter_regs,scope='Offline test transport only; actual native input owners remain unadmitted'))
    pins=[auth.pin(x) for x in dict.fromkeys(inputs)]
    prefix=fixture.read_text().split('#define st128 trace_st128',1)[0]
    text=prefix+'\n#define st128 trace_st128\n#define st32 trace_st32\n#define st64 trace_st64\n'+''.join(functions)+'\n#undef st128\n#undef st32\n#undef st64\n'
    text+='#include "'+header.as_posix()+'"\n'+TEST.replace('@ROWS@',',\n'.join(rows)).replace('@OUTPUT@',out.as_posix())
    cpp=out/'partial_resource_differential.cpp';cpp.write_text(text,encoding='utf8',newline='\n')
    binary=out/'partial_resource_differential.exe';args=[a.compiler,'-std=c++20','-mavx2','-ffp-contract=off','-O1','-static-libgcc','-static-libstdc++','-I'+str((a.candidate_build/'src/runtime/include').resolve()),cpp,'-o',binary]
    auth.need(run(args,'compile').returncode==0,'partial_native_compile_failed')
    checks=[]
    for plant in ['', '--plant-extra-store','--plant-extra-multiply','--plant-invent-filter','--plant-vector-input','--plant-fixed-filter-cost','--plant-order','--plant-flags']:
        cp=run([binary]+([plant] if plant else []),plant.lstrip('-') or 'positive')
        checks.append(dict(name=plant or 'positive',exit=cp.returncode,expected=1 if plant else 0,stdout=cp.stdout.decode(errors='replace'),stderr=cp.stderr.decode(errors='replace')))
    changes=[x['path'] for x in pins if auth.pin(Path(x['path']))!=x]
    passed=not changes and all(c['exit']==c['expected'] for c in checks)
    result=dict(status='PASS_BOUNDED_PARTIAL_RESOURCE_OUTPUTS' if passed else 'FAIL',invocation=[sys.executable,*sys.argv],source_pins=pins,input_changes=changes,commands=commands,checks=checks,programs=proofs,artifacts=[auth.pin(cpp),auth.pin(binary)],limits=['Four original positive-return truncation shapes plus one intact4-filter control; constructed cases, not observed calls.','Full RAM, ordered writes, return, scalar flags/accumulator, fullVU and no-yield budget comparison; not fullCpu/scheduler/caller admission.','Failed append words and omitted filter limits are explicit offline proved original-memory/caller inputs. Actual ownership/lifetime bindings remain OPEN.','Other source-positive truncations, continuations and full original producer domain remain OPEN. No runtime installation or native credit.'])
    target=out/'RESULT.json';target.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8',newline='\n')
    print(json.dumps(dict(status=result['status'],sha256=auth.pin(target)['sha256'],checks=checks)))
    return 0 if passed else 1


TEST=r'''
namespace partial=mka::emitted::partial_resources;
using namespace mka::emitted::resources;
struct Program{const char*name;unsigned total,code;void(*run)(Cpu&);partial::Descriptor descriptor;std::array<int,20>resource_vf;std::array<int,13>constant_vf,constant_offset;std::array<int,4>filter_regs;};
static Program programs[]={@ROWS@};
int main(int argc,char**argv){try{const std::string mode=argc>1?argv[1]:"";unsigned comparisons=0,refusals=0;
 for(auto&program:programs)for(unsigned count:{0u,1u,8u})for(unsigned alias:{0u,1u})for(unsigned numeric:{0u,1u,2u})for(unsigned prior_limit:{0u,0x3f800000u,0x7fffffffu}){
  std::memset(g_ram,0x5a,kRamSize);std::ifstream f(std::string("@OUTPUT@/")+program.name+".bin",std::ios::binary);f.read(reinterpret_cast<char*>(g_ram+0x1c00000),(program.total+4)*4);if(!f||f.peek()!=EOF)throw std::runtime_error("program size");
  constexpr unsigned ia=0x100000,ib=0x200000;const unsigned oa=alias?ia+352:0x300000,ob=alias?ib:0x400000;
  constexpr unsigned special[]={0u,0x80000000u,0x7fffffffu,0x7fc00000u,0xff800000u,1u,0xbf800000u,0x7f7fffffu};
  for(unsigned i=0;i<10;++i){for(unsigned j=0;j<88;++j)st32(ia+i*352+j*4,numeric?special[(i+j+numeric)%8]:0x3f000000+i*0x100000+j*0x10000);st32(ib+i*32,numeric?special[(i+numeric)%8]:0x3f000000+i*0x100000);}
  Cpu c{};c.pc=0x1c00000;c.r[4].d[0]=ia;c.r[5].d[0]=ib;c.r[6].d[0]=oa;c.r[7].d[0]=ob;c.r[8].d[0]=352;c.r[9].d[0]=32;c.r[10].d[0]=count;c.r[11].d[0]=0x1c00000+program.code*4;c.r[12].d[0]=0x1c00000;c.r[29].d[0]=0x1fff000;c.r[31].d[0]=0x100000;c.budget=10000000;
  for(unsigned reg=1;reg<32;++reg){c.f[reg]=0x3f000000+reg;for(unsigned lane=0;lane<4;++lane)c.vu0.vf[reg].w[lane]=0x3f800000+reg*0x100+lane;}
  for(unsigned lane=0;lane<4;++lane)c.vu0.acc.w[lane]=special[lane+numeric];
  const unsigned step=numeric==0?0x3f000000:numeric==1?0x80000000:0x7f7fffff;
  c.fcr31=numeric==0?0x00800201:numeric==1?0xffffffff:0;c.f[12]=c.vu0.vf[1].w[0]=step;c.vu0.vf[0].w[3]=0x3f800000;c.vu0.mac=0x5321;c.vu0.status=numeric?0xffff:0x1234;c.facc=0x5a5a5a5a;
  for(int reg:program.filter_regs)if(reg>=0)c.f[reg]=prior_limit;
  const auto initial=c;partial::Bindings bindings{};
  for(unsigned i=0;i<program.descriptor.body.effect_count;++i){const auto id=program.descriptor.body.effects[i].id;if(id!=5&&id!=6&&id!=26)continue;
   Vector raw{};const unsigned at=c.r[11].w[0]+program.constant_offset[i];if(program.constant_vf[i]>=0)std::memcpy(raw.data(),g_ram+(at&~15u),16);else raw[0]=ld32(at);bindings.constant_words[i]=raw;}
  for(unsigned i=0;i<program.descriptor.filter_count;++i)bindings.omitted_filter_limits[i]=c.f[program.filter_regs[i]];
  const auto before=std::vector<unsigned char>(g_ram,g_ram+kRamSize);events.clear();program.run(c);const auto expected=std::vector<unsigned char>(g_ram,g_ram+kRamSize);const auto original_events=events;
  if(c.pc!=0x100000||c.r[29].d[0]!=0x1fff000)throw std::runtime_error("original return");
  std::memcpy(g_ram,before.data(),kRamSize);events.clear();
  Streams streams{{Stream{{reinterpret_cast<std::byte*>(g_ram+ia),3520},{reinterpret_cast<std::byte*>(g_ram+oa),3520},352},Stream{{reinterpret_cast<std::byte*>(g_ram+ib),320},{reinterpret_cast<std::byte*>(g_ram+ob),320},32}},count};
  auto descriptor=program.descriptor;descriptor.body.scalar_step=descriptor.body.vector_step=step;
  if(mode=="--plant-vector-input"&&std::string(program.name)=="source_partial_data23")(*bindings.constant_words[5])[0]^=0x80000000u;
  auto prepared=partial::prepare(descriptor,streams,bindings);
  if(mode=="--plant-extra-store")prepared.stores=descriptor.body.attribute_count;
  if(mode=="--plant-extra-multiply"&&std::string(program.name)=="source_partial_multiply9")prepared.body.scalar_constants[8]=ps2::fp::mul((*bindings.constant_words[8])[0],step);
  if(mode=="--plant-invent-filter"&&descriptor.filter_count==4)prepared.filter_limits[3]=descriptor.filters[3].limit;
  auto out=partial::run_no_yield(streams,prepared,10000000,{nullptr,&observe});const auto&result=out.effects;
  if(mode=="--plant-fixed-filter-cost")out.cost+=3*(std::uint64_t(count)*descriptor.filter_count-out.filter_visits);
  if(mode=="--plant-order"&&events.size()>1)std::swap(events[0],events[1]);
  auto projected=initial.vu0;
  for(unsigned i=0;i<descriptor.body.effect_count;++i)if(program.constant_vf[i]>=0)std::memcpy(&projected.vf[program.constant_vf[i]],prepared.body.vector_constants[i].data(),16);
  if(result.element_observed)for(unsigned i=0;i<descriptor.body.attribute_count;++i)if(program.resource_vf[i]>=0)std::memcpy(&projected.vf[program.resource_vf[i]],result.last_resources[i].vector.data(),16);
  if(result.vector_written){projected.mac=result.vector.last_mac;projected.status=result.vector.project_status(initial.vu0.status);}
  if(result.vector_accumulator_written)std::memcpy(&projected.acc,result.vector_accumulator.data(),16);
  unsigned flags=initial.fcr31|result.scalar_flags;if(result.condition_written)flags=(flags&~0x00800000u)|(result.condition?0x00800000u:0);
  if(mode=="--plant-flags")flags^=1;
  if(result.accepted!=c.r[2].w[0]||flags!=c.fcr31||(result.scalar_accumulator_written?result.scalar_accumulator:initial.facc)!=c.facc||events!=original_events||10000000-static_cast<std::int64_t>(out.cost)!=c.budget||std::memcmp(&projected,&c.vu0,sizeof projected)||std::memcmp(g_ram,expected.data(),kRamSize)){
   unsigned first=0;while(first<kRamSize&&g_ram[first]==expected[first])++first;
   std::fprintf(stderr,"PARTIAL_DIFFERENCE %s count=%u alias=%u numeric=%u limit=%X return=%u/%u flags=%X/%X writes=%zu/%zu cost=%llu/%d VUdiff=%d first=%X\n",program.name,count,alias,numeric,prior_limit,result.accepted,c.r[2].w[0],flags,c.fcr31,events.size(),original_events.size(),(unsigned long long)out.cost,10000000-c.budget,std::memcmp(&projected,&c.vu0,sizeof projected),first);return 1;}
  const auto saved=events.size();bool refused=false;try{partial::run_no_yield(streams,prepared,0,{nullptr,&observe});}catch(const std::runtime_error&){refused=true;}
  if(!refused||events.size()!=saved||std::memcmp(g_ram,expected.data(),kRamSize))throw std::runtime_error("insufficient budget mutated output");++refusals;
  if(descriptor.filter_count==4&&std::string(program.name)=="source_partial_load17"){auto missing=bindings;missing.omitted_filter_limits[3].reset();bool refused=false;try{partial::prepare(descriptor,streams,missing);}catch(const std::runtime_error&e){refused=std::string(e.what()).find("omitted filter caller input unbound")!=std::string::npos;}if(!refused)throw std::runtime_error("invented filter caller input");++refusals;}
  ++comparisons;
 }
 std::printf("PASS partial_shapes=4 intact4filter_controls=1 whole_output_comparisons=%u prewrite_or_missing_input_refusals=%u full_RAM_ordered_width_value_return_FCR_facc_fullVU_budget=1 actual_owner_scheduler_admission=0\n",comparisons,refusals);return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"REFUSED %s\n",e.what());return 2;}}
'''


if __name__=='__main__':raise SystemExit(main())
