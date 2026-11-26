"""Test typed vector side outputs against the complete compiled original family.

Uses the authored semantic core's two result observations. No candidate is
edited; the complete original compiled family remains the reference.
"""
from pathlib import Path
import argparse
import json
import re
import subprocess
import sys

sys.path.insert(0,str(Path(__file__).resolve().parent))
import specialize_r192_stub_family as family


def replace_once(text,before,after):
    family.require(text.count(before)==1,'vector_fixture_seam_changed:'+before[:60])
    return text.replace(before,after)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['timing-proof','candidate-build','compiler','output']:
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();out=a.output.resolve()
    family.require(out.is_relative_to((family.W/'build/oracle-telemetry').resolve()) and not out.exists(),'fresh_owned_output_required')
    receipt=a.timing_proof/'RESULT.json'
    family.require(family.pin(receipt)['sha256']=='e9287944fc2b50f706d70410a58b084df2c78079d05eab79a99febca7eeec571','timing_reference_receipt_changed')
    record=json.loads(receipt.read_text());positive=record['runs'][0]
    cpp_pin=next(r for r in positive['source_pins'] if Path(r['path']).suffix=='.cpp')
    cpp=Path(cpp_pin['path']);family.require(family.pin(cpp)['sha256']==cpp_pin['sha256'],'original_reference_fixture_changed')
    source=cpp.read_bytes().decode().replace('\r\n','\n')
    core=family.W/'tools/research/r192_aot_modules/semantic_pipeline.hpp'
    adapter=core.with_name('semantic_no_yield_adapter.hpp');vector=core.with_name('semantic_vector_effects.hpp')
    scratch=core.with_name('semantic_call_scratch.hpp')
    authored_core=core.read_text()
    source,n=re.subn(r'#include "[^"\n]*semantic_no_yield_adapter\.hpp"','@ADAPTER_INCLUDE@',source)
    family.require(n==1,'timing_include_changed')
    project='''const auto&v=result.pipeline.vector;
 std::memcpy(&c.vu0.vf[4],v.constant_product.data(),16);
 if(v.element_observed){std::memcpy(&c.vu0.vf[2],v.last_resource256.data(),16);std::memcpy(&c.vu0.vf[3],v.last_resource257.data(),16);}
 c.vu0.mac=v.last_mac;c.vu0.status=v.project_status(c.vu0.status);'''
    source=replace_once(source,'c.budget=result.remaining_budget;c.fcr31=result.scalar_control_status;c.pc=0x2e6180;',
                        'c.budget=result.remaining_budget;c.fcr31=result.scalar_control_status;'+project+'c.pc=0x2e6180;')
    source=replace_once(source,'good&=candidate.budget==reference.budget;',
                        'good&=candidate.budget==reference.budget;good&=!std::memcmp(&candidate.vu0,&reference.vu0,sizeof candidate.vu0);')
    source=replace_once(source,'c.budget=static_cast<int>(16+34*count);',
                        'c.budget=static_cast<int>(16+34*count);c.vu0.status=initial;c.vu0.mac=0xf1234567u;for(unsigned lane=0;lane<4;++lane){c.vu0.vf[2].w[lane]=special[(seed+lane)%8];c.vu0.vf[3].w[lane]=special[(seed+lane+1)%8];c.vu0.acc.w[lane]=special[(seed+lane+2)%8];}')
    source=replace_once(source,'  if(native.pipeline.accepted!=reference.r[2].w[0]',
                        '''  if(!native.pipeline.vector_effects_admitted)throw std::runtime_error("vector family not admitted");
  const auto&ve=native.pipeline.vector;Vu0Regs projected=c.vu0;
  std::memcpy(&projected.vf[4],ve.constant_product.data(),16);
  if(ve.element_observed){std::memcpy(&projected.vf[2],ve.last_resource256.data(),16);std::memcpy(&projected.vf[3],ve.last_resource257.data(),16);}
  projected.mac=ve.last_mac;projected.status=ve.project_status(c.vu0.status);
  if(std::memcmp(&projected,&reference.vu0,sizeof projected)){std::fprintf(stderr,"VECTOR_DIFFERENCE seed=%u count=%u alias=%u mac=%08X/%08X status=%08X/%08X\\n",seed,count,alias,projected.mac,reference.vu0.mac,projected.status,reference.vu0.status);return 1;}
  if(native.pipeline.accepted!=reference.r[2].w[0]''')
    source=source.replace('VU_status_adoption=OPEN','whole_VU_state_projection=PROVED accumulator_unchanged=PROVED runtime_adoption=NONE')
    inputs=[Path(__file__),core,adapter,vector,scratch,receipt,cpp,a.compiler,
            *[Path(r['path']) for r in record['source_pins'] if Path(r['path']).suffix=='.bin'],
            *sorted((a.candidate_build/'src/runtime/include/ps2').glob('*.hpp'))]
    pins=[family.pin(x) for x in dict.fromkeys(inputs)];out.mkdir(parents=True);runs=[]
    variants=[('positive',None,None,None),
              ('planted-zero-prelude','core','result.vector.prepared(prepared[1].vector);','if(streams.count)result.vector.prepared(prepared[1].vector);'),
              ('planted-rejected-element','core','result.vector.element(tag257,tag256);','if(!pipeline.filter_count||ps2::fp::key(tag769)<=ps2::fp::key(pipeline.filter_limit))result.vector.element(tag257,tag256);'),
              ('planted-high-status','vector','(previous&0xff0u)','previous'),
              ('planted-mac-sign','vector','(signs<<4)','(signs<<8)')]
    for name,where,before,after in variants:
        directory=out/name;directory.mkdir();contents={'core':authored_core,'vector':vector.read_text()}
        if where:contents[where]=replace_once(contents[where],before,after)
        (directory/core.name).write_text(contents['core'],encoding='utf8',newline='\n')
        (directory/vector.name).write_text(contents['vector'],encoding='utf8',newline='\n')
        (directory/adapter.name).write_bytes(adapter.read_bytes())
        (directory/scratch.name).write_bytes(scratch.read_bytes())
        generated=directory/'test.cpp';generated.write_text(source.replace('@ADAPTER_INCLUDE@','#include "'+(directory/adapter.name).as_posix()+'"'),encoding='utf8',newline='\n');exe=directory/'test.exe'
        command=[str(a.compiler),'-std=c++20','-mavx2','-ffp-contract=off','-O1','-static-libgcc','-static-libstdc++',
                 '-I'+str((a.candidate_build/'src/runtime/include').resolve()),str(generated),'-o',str(exe)]
        cp=subprocess.run(command,capture_output=True);(directory/'compile.log').write_bytes(cp.stdout+cp.stderr)
        family.require(cp.returncode==0,'vector_fixture_compile_failed:'+name)
        run=subprocess.run([str(exe)],capture_output=True);(directory/'test.log').write_bytes(run.stdout+run.stderr)
        runs.append(dict(name=name,compile_command=command,argv=[str(exe)],exit=run.returncode,expected_exit=0 if name=='positive' else 1,
                         stdout=run.stdout.decode(errors='replace'),stderr=run.stderr.decode(errors='replace'),
                         fixture_pins=[family.pin(directory/core.name),family.pin(directory/vector.name),family.pin(directory/scratch.name),family.pin(directory/adapter.name),family.pin(generated),family.pin(exe)]))
    changes=[r['path'] for r in pins if family.pin(Path(r['path']))['sha256']!=r['sha256']]
    passed=not changes and all(r['exit']==r['expected_exit'] for r in runs)
    result=dict(status='PASS_TYPED_VECTOR_SIDE_OUTPUTS' if passed else 'FAIL',runs=runs,source_pins=pins,input_changes=changes,
                observations=['Same432 original-program matrix and578 ordered-store constructed caller-frame comparison; literal entire Vu0Regs compared.',
                              'Zero-count preserves originalVF2/VF3 and accumulator, while original prelude changesVF4/MAC/status.',
                              'Rejected elements still update vector results/MAC/status before filtering; prior sticky/control masks source-defined.'],
                limits=['Authored core result observations are compiled in an owned fixture; no runtime candidate is changed.',
                        'Only exact captured[1,5,6,17] with one filter; other14 descriptor configurations gain no VU/timing claim.',
                        'Low-budget continuation and actual scheduler/runtime adoption remain OPEN.'])
    target=out/'RESULT.json';target.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8',newline='\n')
    print(json.dumps(dict(status=result['status'],result=str(target),sha256=family.pin(target)['sha256'])))
    return 0 if passed else 1


if __name__=='__main__':raise SystemExit(main())
