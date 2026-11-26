"""Test the actual reference bridge projection against original AOT boundaries."""
from pathlib import Path
import argparse
import json
import re
import subprocess
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
import specialize_r192_stub_family as family


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['full-cpu-proof','candidate-build','compiler','output']:p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();out=a.output.resolve()
    family.require(out.is_relative_to((family.W/'build/oracle-telemetry').resolve()) and not out.exists(),'fresh_owned_output_required')
    proof=a.full_cpu_proof/'RESULT.json';family.require(family.pin(proof)['sha256']=='42dfbf045c06a9206eb64ac5cb839823f9b72a8754bf1a457630af034da73ffe','full_CPU_proof_changed')
    record=json.loads(proof.read_text());pin=next(r for r in record['runs'][0]['fixture_pins'] if Path(r['path']).suffix=='.cpp');fixture=Path(pin['path'])
    family.require(family.pin(fixture)['sha256']==pin['sha256'],'reference_fixture_changed')
    source=fixture.read_bytes().decode().replace('\r\n','\n');folder=family.W/'tools/research/r192_aot_modules';header=folder/'reference_family_projection.hpp'
    source,n=re.subn(r'#include "[^"\n]*semantic_continuation\.hpp"','@REFERENCE_INCLUDE@',source);family.require(n==1,'reference_include_seam_changed')
    start=source.index('static void require_full_cpu(');end=source.index('\nstatic void assert_state(',start)
    source=source[:start]+TEST+source[end:]
    inputs=[Path(__file__),Path(family.__file__),proof,fixture,header,a.compiler,*folder.glob('semantic_*.hpp'),
            *(Path(r['path']) for r in record['source_pins'] if Path(r['path']).suffix=='.bin'),*sorted((a.candidate_build/'src/runtime/include/ps2').glob('*.hpp'))]
    pins=[family.pin(x) for x in dict.fromkeys(inputs)];out.mkdir(parents=True);runs=[]
    variants=[('positive',None,None),
              ('planted-input-cursor','inputs.input_a+continuation.consumed()*inputs.stride_a','inputs.input_a+result.accepted*inputs.stride_a'),
              ('planted-upper-lane','ps2::set32(c.r[2],result.accepted);','ps2::set32(c.r[2],result.accepted);c.r[2].d[1]=0;'),
              ('planted-empty-remap','if(s.accepted_observed){c.f[9]','{c.f[9]'),
              ('planted-prior-vector-status','v.project_status(inputs.vector_status)','v.project_status(0)'),
              ('planted-accumulator','c.budget=continuation.budget();','c.budget=continuation.budget();c.vu0.acc={};')]
    for name,before,after in variants:
        directory=out/name;directory.mkdir();text=header.read_text()
        if before:
            family.require(text.count(before)==1,'projection_mutation_anchor_changed:'+name);text=text.replace(before,after)
        (directory/header.name).write_text(text,encoding='utf8',newline='\n')
        for path in folder.glob('semantic_*.hpp'):(directory/path.name).write_bytes(path.read_bytes())
        cpp=directory/'test.cpp';cpp.write_text(source.replace('@REFERENCE_INCLUDE@','#include "'+(directory/header.name).as_posix()+'"'),encoding='utf8',newline='\n');exe=directory/'test.exe'
        command=[str(a.compiler),'-std=c++20','-mavx2','-ffp-contract=off','-O2','-static-libgcc','-static-libstdc++','-I'+str((a.candidate_build/'src/runtime/include').resolve()),str(cpp),'-o',str(exe)]
        cp=subprocess.run(command,capture_output=True);(directory/'compile.log').write_bytes(cp.stdout+cp.stderr);family.require(cp.returncode==0,'projection_fixture_compile_failed:'+name)
        run=subprocess.run([str(exe)],capture_output=True);(directory/'test.log').write_bytes(run.stdout+run.stderr)
        runs.append(dict(name=name,compile_command=command,argv=[str(exe)],exit=run.returncode,expected_exit=0 if name=='positive' else 1,stdout=run.stdout.decode(errors='replace'),stderr=run.stderr.decode(errors='replace'),fixture_pins=[family.pin(cpp),family.pin(exe),*[family.pin(x) for x in directory.glob('*.hpp')]]))
    changes=[r['path'] for r in pins if family.pin(Path(r['path']))['sha256']!=r['sha256']]
    passed=not changes and all(r['exit']==r['expected_exit'] and ('PASS_FULL_CPU_REFERENCE_SCRATCH' if r['name']=='positive' else 'FULL_CPU_DIFFERENCE') in r['stdout']+r['stderr'] for r in runs)
    result=dict(status='PASS_ACTUAL_REFERENCE_PROJECTION' if passed else 'FAIL',runs=runs,source_pins=pins,input_changes=changes,
                scope='Authored reference projection function replaces test-only mapping; independent original compiled body remains the oracle. Same2295 fullCpu boundary comparisons and explicit parked-state plants. No candidate/adoption.')
    target=out/'RESULT.json';target.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8',newline='\n');print(json.dumps(dict(status=result['status'],result=str(target),sha256=family.pin(target)['sha256'])))
    return 0 if passed else 1


TEST=r'''
static void require_full_cpu(const sem::ClipContinuation&native,const Cpu&reference,const Cpu&initial,bool compare_budget){
 const auto inputs=mka::emitted::reference::FrameInputs::capture(initial);Cpu projected=initial;mka::emitted::reference::project(projected,inputs,native);
 projected.budget=compare_budget?projected.budget:reference.budget;projected.pc=reference.pc;projected.base=reference.base;
 if(std::memcmp(&projected,&reference,sizeof reference))throw std::runtime_error("FULL_CPU_DIFFERENCE authored reference projection");
 if(!mka::emitted::reference::parked_state_matches(projected,inputs,native))throw std::runtime_error("FULL_CPU_DIFFERENCE exact parked context refused");
 auto changed=projected;changed.budget^=7;if(!mka::emitted::reference::parked_state_matches(changed,inputs,native))throw std::runtime_error("FULL_CPU_DIFFERENCE scheduler budget treated as retained state");
 changed=projected;changed.vu0.vf[4].w[0]^=1;if(mka::emitted::reference::parked_state_matches(changed,inputs,native))throw std::runtime_error("FULL_CPU_DIFFERENCE changed shared VU admitted");
 changed=projected;changed.fcr31^=1;if(mka::emitted::reference::parked_state_matches(changed,inputs,native))throw std::runtime_error("FULL_CPU_DIFFERENCE changed scalar control admitted");
 changed=projected;changed.r[10].w[0]^=1;if(mka::emitted::reference::parked_state_matches(changed,inputs,native))throw std::runtime_error("FULL_CPU_DIFFERENCE changed cursor admitted");
 ++full_cpu_comparisons;
}
'''

if __name__=='__main__':raise SystemExit(main())
