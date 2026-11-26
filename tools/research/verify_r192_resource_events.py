"""Whole original-body regression for the typed guard-event continuation.

Reuses the frozen276-program/4416-session reference fixture. Constructed guard
services are not actual scheduler evidence; actual observations are separate.
"""
from pathlib import Path
import argparse
import json
import subprocess
import sys

sys.path.insert(0,str(Path(__file__).resolve().parent))
import analyze_r192_builder_domain as auth


def once(text,old,new):
    auth.need(text.count(old)==1,'event_regression_seam_changed:'+old[:60]);return text.replace(old,new)


def historical_pins(rows):
    # Only these named authored native subjects may change from the preserved
    # reference-era version. Retail/ABI/captures/original generated bodies and
    # every other dependency retain exact historical pins.
    subjects={
        (auth.W/'tools/research/r192_aot_modules/resource_pipeline_continuation.hpp').resolve():'9cd2f7ff06a22f3b68a94649131418727d988bdee41eb52f8e3e60d325df0572',
        (auth.W/'tools/research/r192_aot_modules/resource_pipeline_events.hpp').resolve():'1cac3d8c2b24ae01baa5cdf41ee54132269775167016a60d43c90383d4c98dbf',
    }
    changed=[]
    for row in rows:
        path=Path(row['path']);actual=auth.pin(path)
        if actual!=row:
            auth.need(path.resolve() in subjects and row['sha256']==subjects[path.resolve()],'immutable_original_dependency_changed:'+str(path))
            changed.append(dict(historical=row,current=actual,role='authored_native_test_subject',reason='Explicit used-stream slicing correction; original reference inputs are unchanged.'))
    return changed


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['projection-proof','candidate-build','compiler','output']:
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();out=a.output.resolve();auth.need(out.is_relative_to((auth.W/'build/oracle-telemetry').resolve()) and not out.exists(),'fresh_owned_output_required')
    receipt=a.projection_proof/'RESULT.json';auth.need(auth.pin(receipt)['sha256']=='39cb6e2801d9a17f69d0485a33cf962b8c1b015d00548be8df6314713d4e5219','full_projection_proof_changed')
    record=json.loads(receipt.read_text());auth.need(record['status']=='PASS_BOUNDED_FULL_REFERENCE_PROJECTION' and not record['input_changes'],'full_projection_not_passed')
    original=a.projection_proof/'full_resource_projection.cpp';auth.need(any(auth.pin(original)==r for r in record['artifacts']),'original_boundary_fixture_changed')
    subject_changes=historical_pins(record['source_pins'])
    header=auth.W/'tools/research/r192_aot_modules/resource_pipeline_events.hpp'
    prior_event=auth.W/'build/oracle-telemetry/emitted-resource-event-regression-01/RESULT.json'
    auth.need(auth.pin(prior_event)['sha256']=='aacabd77fd2b45d80aa7c4229afa7f22ee4425a2e038ee55be417cc4d5a6c1df','historical_event_reference_changed')
    old_event=next(row for row in json.loads(prior_event.read_text())['source_pins'] if Path(row['path']).resolve()==header.resolve())
    subject_changes+=historical_pins([old_event])
    source=original.read_text();source='#include "'+header.as_posix()+'"\n'+source
    source=once(source,'Continuation native(streams,descriptor);','EventContinuation native(streams,prepared);')
    source=once(source,'auto guard=[](void*p,std::int64_t&b)','auto guard=[](void*p,const Boundary&,std::int64_t&b)')
    # Retired-context test is the same native object, therefore its callback
    # must also use the event ABI; unrelated old Continuation controls remain.
    source=once(source,'native.advance(budget,{nullptr,[](void*,std::int64_t&){return false;}})','native.advance(budget,{nullptr,[](void*,const Boundary&,std::int64_t&){return false;}})')
    sources=[Path(__file__),Path(auth.__file__),receipt,original,header,prior_event,a.compiler,*(Path(pin['path']) for pin in record['source_pins'])];pins=[auth.pin(x) for x in dict.fromkeys(sources)];out.mkdir(parents=True)
    cpp=out/'resource_event_regression.cpp';cpp.write_text(source,encoding='utf8',newline='\n')
    compiler=[str(a.compiler),'-std=c++20','-mavx2','-ffp-contract=off','-O1','-static-libgcc','-static-libstdc++','-I'+str((a.candidate_build/'src/runtime/include').resolve())];commands=[]
    def run(argv,name):
        cp=subprocess.run(argv,capture_output=True);log=out/(name+'.log');log.write_bytes(cp.stdout+cp.stderr);commands.append(dict(argv=argv,exit=cp.returncode,log=auth.pin(log)));return cp
    exe=out/'resource_event_regression.exe';auth.need(run([*compiler,str(cpp),'-o',str(exe)],'compile').returncode==0,'whole_event_regression_compile_failed');checks=[]
    def check(argv,name,expected):
        cp=run(argv,name);checks.append(dict(name=name,exit=cp.returncode,expected_exit=expected,stdout=cp.stdout.decode(errors='replace'),stderr=cp.stderr.decode(errors='replace')))
    check([str(exe)],'positive',0)
    check([str(exe),'--planted-callback-budget'],'wrong-callback-budget',1)
    # Both intact continuation classes must accept unused empty spans with a
    # nonzero stride without forming an invalid subspan. Used bad offsets must
    # refuse before slicing; restoring the old unconditional loop must fail.
    stream_cpp=out/'stream_transport.cpp';stream_cpp.write_text('#include "'+header.as_posix()+'"\n'+STREAM_TEST,encoding='utf8',newline='\n')
    stream_exe=out/'stream_transport.exe';auth.need(run([*compiler,str(stream_cpp),'-o',str(stream_exe)],'compile-stream-transport').returncode==0,'stream_transport_compile_failed');check([str(stream_exe)],'stream-transport-positive',0)
    continuation=header.with_name('resource_pipeline_continuation.hpp');mutant_cont=out/'undo-stream-skip.hpp'
    body=once(continuation.read_text(),'if(!referenced)continue;','if(false&&!referenced)continue;').replace('#include "semantic_resource_pipeline.hpp"','#include "'+continuation.with_name('semantic_resource_pipeline.hpp').as_posix()+'"')
    mutant_cont.write_text(body,encoding='utf8',newline='\n');mutant_event=out/'undo-stream-skip-event.hpp';mutant_event.write_text(header.read_text().replace('#include "resource_pipeline_continuation.hpp"','#include "'+mutant_cont.as_posix()+'"'),encoding='utf8',newline='\n')
    mutant_cpp=out/'undo-stream-skip.cpp';mutant_cpp.write_text('#include "'+mutant_event.as_posix()+'"\n'+STREAM_TEST,encoding='utf8',newline='\n');mutant_exe=out/'undo-stream-skip.exe'
    auth.need(run([*compiler,str(mutant_cpp),'-o',str(mutant_exe)],'compile-undo-stream-skip').returncode==0,'stream_skip_mutant_compile_failed');check([str(mutant_exe)],'undo-stream-skip',2)
    mutations=[('guard-equality','if(budget<0){const Boundary','if(budget<=0){const Boundary'),
               ('repeated-prelude','else goto process_element;','else {budget-=static_cast<std::int64_t>(timing_.prelude);goto process_element;}'),
               ('lost-vector-sticky','current.vector.raised_sticky|=effects_.vector.raised_sticky;','current.vector.raised_sticky=0;'),
               ('lost-remap-state','if(!current.remap_written&&effects_.remap_written)','if(false&&!current.remap_written&&effects_.remap_written)')]
    for name,old,new in mutations:
        hpp=out/(name+'.hpp');body=once(header.read_text(),old,new);body=body.replace('#include "resource_pipeline_continuation.hpp"','#include "'+header.with_name('resource_pipeline_continuation.hpp').as_posix()+'"');hpp.write_text(body,encoding='utf8',newline='\n')
        planted=out/(name+'.cpp');planted.write_text(source.replace(header.as_posix(),hpp.as_posix()),encoding='utf8',newline='\n');target=out/(name+'.exe')
        auth.need(run([*compiler,str(planted),'-o',str(target)],'compile-'+name).returncode==0,'event_mutant_compile_failed:'+name);check([str(target)],name,1)
    changes=[pin['path'] for pin in pins if auth.pin(Path(pin['path']))!=pin];passed=not changes and all(c['exit']==c['expected_exit'] for c in checks)
    result=dict(schema='r192_typed_guard_events_whole_original_body_regression_v2',status='PASS_WHOLE_TYPED_EVENT_REGRESSION' if passed else 'FAIL',invocation=[sys.executable,*sys.argv],source_pins=pins,native_subject_changes=subject_changes,immutable_original_inputs_checked=True,input_changes=changes,commands=commands,checks=checks,artifacts=[auth.pin(cpp),auth.pin(exe),auth.pin(stream_cpp),auth.pin(stream_exe)],limits=['Constructed276originalprograms/4416sessions; fullCpu/RAM/orderedwrites/VU and5520trueguard suspensions at9936boundaries, not actual scheduler/native integration.','Guard service is constructed. Its budget/return control is distinct from actual captured service effects.'])
    dest=out/'RESULT.json';dest.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8',newline='\n');print(json.dumps(dict(status=result['status'],sha256=auth.pin(dest)['sha256'],result=str(dest))));return 0 if passed else 1


STREAM_TEST=r'''
#include <cstdio>
#include <string>
using namespace mka::emitted::resources;
int main(){try{unsigned cases=0,refusals=0;
 for(unsigned used:{0u,1u})for(unsigned kind:{0u,1u}){
  alignas(16)std::array<std::byte,16>input{},output{};const unsigned values[]={0x3f000000,0x3fc00000,0x40200000};std::memcpy(input.data(),values,12);
  Descriptor d{};d.attribute_count=1;d.attributes[0]={769,used,0};d.effect_count=1;d.effects[0].id=1;d.effects[0].destination_tag=769;d.scalar_step=0x3f800000;
  Streams s{};s.count=3;s.streams[used]={input,output,4};s.streams[1-used]={{},{},SIZE_MAX};
  auto prepared=prepare(d,s);Boundary b{};
  if(kind){EventContinuation native(s,prepared);b=native.advance(10000,{nullptr,[](void*,const Boundary&,std::int64_t&){return false;}});}
  else{Continuation native(s,d);b=native.advance(10000,{nullptr,[](void*,std::int64_t&){return false;}});}
  if(b.phase!=Phase::complete||b.consumed!=3||b.accepted!=3)throw std::runtime_error("unused-stream positive boundary differs");
  for(unsigned i=0;i<3;++i){unsigned actual;std::memcpy(&actual,output.data()+4*i,4);unsigned flags=0;const auto expected=ps2::fp::ee::add(flags,values[i],d.scalar_step);if(actual!=expected)throw std::runtime_error("unused-stream output differs");}
  auto bad=s;bad.streams[used].input={};bool refused=false;try{element_streams(bad,d,1,0);}catch(const std::runtime_error&e){refused=std::string(e.what()).find("stream slice bounds")!=std::string::npos;}if(!refused)throw std::runtime_error("used bad stream offset not refused");++refusals;
  bad=s;bad.streams[used].stride=SIZE_MAX;refused=false;try{element_streams(bad,d,2,0);}catch(const std::runtime_error&e){refused=std::string(e.what()).find("arithmetic overflow")!=std::string::npos;}if(!refused)throw std::runtime_error("slice arithmetic overflow not refused");++refusals;++cases;
 }
 std::printf("PASS unused_empty_nonzero_stride_both_stream_indices_both_continuations=%u used_slice_bounds_overflow_refusals=%u\n",cases,refusals);return 0;
}catch(const std::exception&e){std::fprintf(stderr,"REFUSED %s\n",e.what());return 2;}}
'''


if __name__=='__main__':raise SystemExit(main())
