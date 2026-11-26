"""Allocator-derived full reference Cpu projection at original boundaries.

This test consumes immutable original-builder/continuation witnesses. Physical
bindings come from the source allocator, not runtime instruction decoding.
It establishes neither actual-caller ownership nor scheduler/runtime adoption.
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
    for n in ['continuation-proof','layout-proof','candidate-build','compiler','output']:
        p.add_argument('--'+n,type=Path,required=True)
    a=p.parse_args();out=a.output.resolve()
    auth.need(out.is_relative_to((auth.W/'build/oracle-telemetry').resolve()) and not out.exists(),'fresh_owned_output_required')
    receipts=[a.continuation_proof/'RESULT.json',a.layout_proof/'RESULT.json']
    expected=['a4f3166fe1812d298977f68bb5dfc7808476bdcb54a0a98d9122f8ca15c1edc2','a4fa1d05c3f8136d17c799cc4b2debc5593ec5691264bd691b1c7696ed6b13c3']
    records=[]
    for path,digest in zip(receipts,expected):
        auth.need(auth.pin(path)['sha256']==digest,'source_proof_changed');record=json.loads(path.read_text())
        auth.need(record['status'].startswith('PASS_') and not record['input_changes'],'source_proof_not_passed')
        for pin in record['source_pins']:auth.need(auth.pin(Path(pin['path']))==pin,'proof_source_changed')
        records.append(record)
    projection=auth.W/'tools/research/r192_aot_modules/reference_resource_projection.hpp'
    layout=projection.with_name('reference_resource_layout.hpp')
    original_cpp=a.continuation_proof/'resource_continuation.cpp'
    auth.need(any(auth.pin(original_cpp)==r for r in records[0]['artifacts']),'compiled_original_boundary_fixture_changed')
    text=original_cpp.read_text();marker='int main(int argc,char**argv)'
    auth.need(text.count(marker)==1,'boundary_main_seam_missing')
    text=text.replace(marker,'#include "'+projection.as_posix()+'"\n'+marker)
    seed='Cpu c{};c.pc='
    auth.need(text.count(seed)==1,'original_initial_cpu_seam_missing')
    text=text.replace(seed,'Cpu c{};for(unsigned rr=0;rr<32;++rr){c.r[rr].d[0]=0x1122334455660000ull+rr;c.r[rr].d[1]=0xa5a55a5afedc0000ull+rr;c.f[rr]=special[rr%8];for(unsigned ll=0;ll<4;++ll)c.vu0.vf[rr].w[ll]=0x3f800000+rr*0x100+ll;}c.vu0.vf[0]={};c.pc=')
    boundary='const auto boundary=native.advance(budget,{&service,guard},{nullptr,&observe});budget=boundary.budget;const auto&effects=boundary.effects;'
    auth.need(text.count(boundary)==1,'native_boundary_projection_seam_missing')
    text=text.replace(boundary,boundary+'const auto projected=mka::emitted::reference_resources::project(initial,0x1c00000,prepared,boundary);')
    comparison='std::memcmp(&vu,&c.vu0,sizeof vu)||events!=expected_events||std::memcmp(g_ram,expected.data(),kRamSize)'
    auth.need(text.count(comparison)==1,'whole_cpu_comparison_seam_missing')
    text=text.replace(comparison,'std::memcmp(&projected,&c,sizeof c)||'+comparison)
    text=text.replace('std::fprintf(stderr,"CONTINUATION_BOUNDARY_DIFFERENCE','unsigned first=0;while(first<sizeof c&&reinterpret_cast<const unsigned char*>(&projected)[first]==reinterpret_cast<const unsigned char*>(&c)[first])++first;std::fprintf(stderr,"FULL_CPU_FIRST_DIFFERENCE byte=%u\\n",first);std::fprintf(stderr,"CONTINUATION_BOUNDARY_DIFFERENCE')
    text=text.replace('full_Cpu_actual_scheduler_admission=0','full1456_Cpu_allocator_projection=1 actual_scheduler_runtime_admission=0')
    control_point=' std::printf("PASS original_whole_programs='
    auth.need(text.count(control_point)==1,'projection_admission_control_seam_missing')
    text=text.replace(control_point,CONTRACT_CONTROLS+control_point)
    inputs=[Path(__file__),Path(auth.__file__),*receipts,original_cpp,projection,layout,a.compiler]
    for record in records:inputs += [Path(pin['path']) for pin in record['source_pins']]
    for row in records[1]['rows']:inputs.append(Path(row['raw_observation']['path']))
    pins=[auth.pin(x) for x in dict.fromkeys(inputs)]
    out.mkdir(parents=True)
    cpp=out/'full_resource_projection.cpp';cpp.write_text(text,encoding='utf8',newline='\n')
    compiler=[str(a.compiler),'-std=c++20','-mavx2','-ffp-contract=off','-O1','-static-libgcc','-static-libstdc++','-I'+str((a.candidate_build/'src/runtime/include').resolve())]
    commands=[]
    def run(argv,name):
        cp=subprocess.run(argv,capture_output=True);log=out/(name+'.log');log.write_bytes(cp.stdout+cp.stderr)
        commands.append(dict(argv=argv,exit=cp.returncode,log=auth.pin(log)));return cp
    exe=out/'full_resource_projection.exe'
    auth.need(run([*compiler,str(cpp),'-o',str(exe)],'compile').returncode==0,'full_projection_compile_failed')
    checks=[]
    cp=run([str(exe)],'positive');checks.append(dict(name='positive',exit=cp.returncode,expected_exit=0,stdout=cp.stdout.decode(errors='replace'),stderr=cp.stderr.decode(errors='replace')))
    mutations=[('wrong-sign-extension','out.r[s.index].sd[0]=static_cast<std::int32_t>(v.scalar);','out.r[s.index].d[0]=v.scalar;'),
               ('wrong-resume-pc','code_base+4*layout.prelude_words','code_base+4*layout.prelude_words+4'),
               ('overwritten-upper-lane','out.r[2].d[0]=boundary.accepted;','out.r[2].d[0]=boundary.accepted;out.r[2].d[1]=0;'),
               ('lost-retained-upper64','e.last_retained.data(),16','e.last_retained.data(),8')]
    for name,old,new in mutations:
        raw=projection.read_text();auth.need(raw.count(old)==1,'projection_plant_seam_missing:'+name)
        copy=out/(name+'.hpp');raw=raw.replace('#include "reference_resource_layout.hpp"','#include "'+layout.as_posix()+'"').replace('#include "resource_pipeline_continuation.hpp"','#include "'+projection.with_name('resource_pipeline_continuation.hpp').as_posix()+'"')
        copy.write_text(raw.replace(old,new),encoding='utf8',newline='\n')
        planted=out/(name+'.cpp');planted.write_text(text.replace(projection.as_posix(),copy.as_posix()),encoding='utf8',newline='\n');target=out/(name+'.exe')
        auth.need(run([*compiler,str(planted),'-o',str(target)],'compile-'+name).returncode==0,'projection_plant_compile_failed:'+name)
        cp=run([str(target)],name);checks.append(dict(name=name,exit=cp.returncode,expected_exit=1,stdout=cp.stdout.decode(errors='replace'),stderr=cp.stderr.decode(errors='replace')))
    # Independently join the C++ allocator model to all281 original node and
    # block observations, including the source-supported partial descriptors.
    rows=[]
    for row in records[1]['rows']:
        observation=json.loads(Path(row['raw_observation']['path']).read_text())
        attributes=','.join('{'+','.join(map(str,x))+'}' for x in row['attributes'])
        effects=[]
        for e in row['effects']:effects.append('{'+','.join([str(e[0]),str(e[2]),str(e[4])])+'}')
        filters=','.join('{2,769,0x3f800000}' for _ in range(row['filters']))
        nodes=','.join('{'+','.join(str(x)+'u' for x in n)+'}' for n in observation['nodes'])
        lengths=','.join(str(x) for x in row['original_lengths'])
        rows.append('{"'+row['name']+'",{'+attributes+'},{'+','.join(effects)+'},{'+filters+'},{'+nodes+'},{'+lengths+'},'+str(observation['source_data_start_words'])+','+str(observation['return_words'])+','+('true' if row['intact_block_layout'] else 'false')+'}')
    allocation=out/'allocator_nodes.cpp';allocation.write_text('#include "'+layout.as_posix()+'"\n'+ALLOCATOR.replace('@ROWS@',',\n'.join(rows)),encoding='utf8',newline='\n')
    allocation_exe=out/'allocator_nodes.exe';auth.need(run([*compiler,str(allocation),'-o',str(allocation_exe)],'compile-allocator').returncode==0,'allocator_node_checker_compile_failed')
    cp=run([str(allocation_exe)],'allocator-positive');checks.append(dict(name='allocator-positive',exit=cp.returncode,expected_exit=0,stdout=cp.stdout.decode(errors='replace'),stderr=cp.stderr.decode(errors='replace')))
    changes=[x['path'] for x in pins if auth.pin(Path(x['path']))!=x]
    passed=not changes and all(x['exit']==x['expected_exit'] for x in checks)
    result=dict(schema='r192_allocator_full_reference_projection_v1',status='PASS_BOUNDED_FULL_REFERENCE_PROJECTION' if passed else 'FAIL',invocation=[sys.executable,*sys.argv],
                source_pins=pins,input_changes=changes,commands=commands,checks=checks,artifacts=[auth.pin(x) for x in [cpp,exe,allocation,allocation_exe]],
                limits=['Reference-only Cpu projection; no accepted native machine context, runtime adoption or ownership/credit change.',
                        'All276 whole program boundaries are constructed with explicit guard services, not actual scheduler/caller/lifetime observations.',
                        'Runtime physical binding must use this source allocator/descriptor data, never original emitted-word decoding. Ambient constant fourth inputs and partial-emission domains remain explicit.'])
    dest=out/'RESULT.json';dest.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8',newline='\n');print(json.dumps(dict(status=result['status'],sha256=auth.pin(dest)['sha256'],result=str(dest))))
    return 0 if passed else 1


ALLOCATOR=r'''
#include <algorithm>
#include <vector>
#include <cstdio>
using namespace mka::emitted::reference_resources;
struct Case{const char*name;std::vector<Attribute>attributes;std::vector<Effect>effects;std::vector<Filter>filters;std::vector<std::array<unsigned,5>>nodes;std::array<unsigned,7>used;unsigned data_start,total;bool intact;};
int main(){try{const Case cases[]={@ROWS@};unsigned comparisons=0,partials=0;
for(const auto&c:cases){auto p=derive_layout(c.attributes,c.effects,c.filters);if(p.node_count!=c.nodes.size()||p.intact()!=c.intact)throw std::runtime_error("allocator shape mismatch");
 for(unsigned i=0;i<p.node_count;++i){const auto&n=p.nodes[i];std::array<unsigned,5>actual{n.tag,unsigned(n.slot.index),unsigned(n.slot.bank),n.stream<0?0:0x1c10000u+3740u+unsigned(n.stream)*16u,n.offset};if(actual!=c.nodes[i]){std::fprintf(stderr,"NODE_MISMATCH %s index=%u\n",c.name,i);return 1;}++comparisons;}
 for(unsigned i=0;i<7;++i)if(std::min(p.intended[i],p.capacity[i])!=c.used[i])throw std::runtime_error("allocator block lengths mismatch");
 if(c.intact){p.require_intact();if(p.total_words!=c.total||p.data_start_words!=c.data_start)throw std::runtime_error("intact source data placement mismatch");}
 else{bool refused=false;try{p.require_intact();}catch(const std::runtime_error&){refused=true;}if(!refused)throw std::runtime_error("source partial admitted as intact");++partials;}
}
std::printf("PASS original_producer_layouts=%zu physical_node_rows=%u source_positive_partial_layout_refusals=%u runtime_admission=0\n",std::size(cases),comparisons,partials);return 0;
}catch(const std::exception&e){std::fprintf(stderr,"REFUSED %s\n",e.what());return 2;}}
'''

CONTRACT_CONTROLS=r'''
 {
  const auto&d=programs[20].descriptor;Streams zero{};const auto prepared=prepare(d,zero);const auto plan=mka::emitted::reference_resources::derive_layout(d);
  Cpu entry{};entry.pc=0x1c00000;entry.r[11].d[0]=0x1c00000+plan.data_start_words*4;entry.f[12]=d.scalar_step;entry.vu0.vf[1].w[0]=d.vector_step;Boundary b{};b.phase=Phase::complete;
  mka::emitted::reference_resources::project(entry,0x1c00000,prepared,b);
  unsigned rejected=0;
  auto reject=[&](Cpu bad,const char*reason){try{mka::emitted::reference_resources::project(bad,0x1c00000,prepared,b);}catch(const std::runtime_error&e){if(std::string(e.what()).find(reason)!=std::string::npos){++rejected;return;}}throw std::runtime_error("projection entry contract control accepted or wrong reason");};
  auto bad=entry;bad.pc+=4;reject(bad,"entry/data-base");bad=entry;bad.r[11].d[0]+=16;reject(bad,"entry/data-base");
  bad=entry;bad.f[12]^=1;reject(bad,"step/stride");bad=entry;bad.vu0.vf[1].w[0]^=1;reject(bad,"step/stride");bad=entry;bad.r[8].d[0]=0x80000000;reject(bad,"step/stride");
  bad=entry;bad.r[10].d[0]=0x100000000ull;reject(bad,"count/phase");
  for(std::int64_t value:{std::int64_t(INT32_MIN)-1,std::int64_t(INT32_MAX)+1}){auto invalid=b;invalid.budget=value;bool refused=false;try{mka::emitted::reference_resources::project(entry,0x1c00000,prepared,invalid);}catch(const std::runtime_error&e){refused=std::string(e.what()).find("int32 budget representation")!=std::string::npos;}if(!refused)throw std::runtime_error("out-of-range reference budget accepted");++rejected;}
  std::printf("PASS reference_projection_entry_contract_refusals=%u\n",rejected);
 }
'''


if __name__=='__main__':raise SystemExit(main())
