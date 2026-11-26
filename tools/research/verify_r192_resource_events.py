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


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['projection-proof','candidate-build','compiler','output']:
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();out=a.output.resolve();auth.need(out.is_relative_to((auth.W/'build/oracle-telemetry').resolve()) and not out.exists(),'fresh_owned_output_required')
    receipt=a.projection_proof/'RESULT.json';auth.need(auth.pin(receipt)['sha256']=='39cb6e2801d9a17f69d0485a33cf962b8c1b015d00548be8df6314713d4e5219','full_projection_proof_changed')
    record=json.loads(receipt.read_text());auth.need(record['status']=='PASS_BOUNDED_FULL_REFERENCE_PROJECTION' and not record['input_changes'],'full_projection_not_passed')
    original=a.projection_proof/'full_resource_projection.cpp';auth.need(any(auth.pin(original)==r for r in record['artifacts']),'original_boundary_fixture_changed')
    for pin in record['source_pins']:auth.need(auth.pin(Path(pin['path']))==pin,'full_projection_source_changed')
    header=auth.W/'tools/research/r192_aot_modules/resource_pipeline_events.hpp'
    source=original.read_text();source='#include "'+header.as_posix()+'"\n'+source
    source=once(source,'Continuation native(streams,descriptor);','EventContinuation native(streams,prepared);')
    source=once(source,'auto guard=[](void*p,std::int64_t&b)','auto guard=[](void*p,const Boundary&,std::int64_t&b)')
    # Retired-context test is the same native object, therefore its callback
    # must also use the event ABI; unrelated old Continuation controls remain.
    source=once(source,'native.advance(budget,{nullptr,[](void*,std::int64_t&){return false;}})','native.advance(budget,{nullptr,[](void*,const Boundary&,std::int64_t&){return false;}})')
    sources=[Path(__file__),Path(auth.__file__),receipt,original,header,a.compiler,*(Path(pin['path']) for pin in record['source_pins'])];pins=[auth.pin(x) for x in dict.fromkeys(sources)];out.mkdir(parents=True)
    cpp=out/'resource_event_regression.cpp';cpp.write_text(source,encoding='utf8',newline='\n')
    compiler=[str(a.compiler),'-std=c++20','-mavx2','-ffp-contract=off','-O1','-static-libgcc','-static-libstdc++','-I'+str((a.candidate_build/'src/runtime/include').resolve())];commands=[]
    def run(argv,name):
        cp=subprocess.run(argv,capture_output=True);log=out/(name+'.log');log.write_bytes(cp.stdout+cp.stderr);commands.append(dict(argv=argv,exit=cp.returncode,log=auth.pin(log)));return cp
    exe=out/'resource_event_regression.exe';auth.need(run([*compiler,str(cpp),'-o',str(exe)],'compile').returncode==0,'whole_event_regression_compile_failed');checks=[]
    def check(argv,name,expected):
        cp=run(argv,name);checks.append(dict(name=name,exit=cp.returncode,expected_exit=expected,stdout=cp.stdout.decode(errors='replace'),stderr=cp.stderr.decode(errors='replace')))
    check([str(exe)],'positive',0)
    check([str(exe),'--planted-callback-budget'],'wrong-callback-budget',1)
    mutations=[('guard-equality','if(budget<0){const Boundary','if(budget<=0){const Boundary'),
               ('repeated-prelude','else goto process_element;','else {budget-=static_cast<std::int64_t>(timing_.prelude);goto process_element;}'),
               ('lost-vector-sticky','current.vector.raised_sticky|=effects_.vector.raised_sticky;','current.vector.raised_sticky=0;'),
               ('lost-remap-state','if(!current.remap_written&&effects_.remap_written)','if(false&&!current.remap_written&&effects_.remap_written)')]
    for name,old,new in mutations:
        hpp=out/(name+'.hpp');body=once(header.read_text(),old,new);body=body.replace('#include "resource_pipeline_continuation.hpp"','#include "'+header.with_name('resource_pipeline_continuation.hpp').as_posix()+'"');hpp.write_text(body,encoding='utf8',newline='\n')
        planted=out/(name+'.cpp');planted.write_text(source.replace(header.as_posix(),hpp.as_posix()),encoding='utf8',newline='\n');target=out/(name+'.exe')
        auth.need(run([*compiler,str(planted),'-o',str(target)],'compile-'+name).returncode==0,'event_mutant_compile_failed:'+name);check([str(target)],name,1)
    changes=[pin['path'] for pin in pins if auth.pin(Path(pin['path']))!=pin];passed=not changes and all(c['exit']==c['expected_exit'] for c in checks)
    result=dict(schema='r192_typed_guard_events_whole_original_body_regression_v1',status='PASS_WHOLE_TYPED_EVENT_REGRESSION' if passed else 'FAIL',invocation=[sys.executable,*sys.argv],source_pins=pins,input_changes=changes,commands=commands,checks=checks,artifacts=[auth.pin(cpp),auth.pin(exe)],limits=['Constructed276originalprograms/4416sessions; fullCpu/RAM/orderedwrites/VU and5520trueguard suspensions at9936boundaries, not actual scheduler/native integration.','Guard service is constructed. Its budget/return control is distinct from actual captured service effects.'])
    dest=out/'RESULT.json';dest.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8',newline='\n');print(json.dumps(dict(status=result['status'],sha256=auth.pin(dest)['sha256'],result=str(dest))));return 0 if passed else 1


if __name__=='__main__':raise SystemExit(main())
