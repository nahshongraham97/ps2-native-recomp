"""Check source-derived algorithm scratch against full reference Cpu boundaries.

Cpu projection exists only in this reference test. The native core/state remain
typed data. Before the core phase is released, hooks can be tested in owned
copies; that mode is explicitly marked prospective in the receipt.
"""
from pathlib import Path
import argparse
import json
import re
import subprocess
import sys

sys.path.insert(0,str(Path(__file__).resolve().parent))
import specialize_r192_stub_family as family


def once(text,before,after):
    family.require(text.count(before)==1,'scratch_source_seam_changed:'+before[:60])
    return text.replace(before,after)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['continuation-proof','candidate-build','compiler','output']:
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();out=a.output.resolve()
    family.require(out.is_relative_to((family.W/'build/oracle-telemetry').resolve()) and not out.exists(),'fresh_owned_output_required')
    receipt=a.continuation_proof/'RESULT.json';family.require(family.pin(receipt)['sha256']=='c073a76072641fc7fa2fc363d5f84cf32d6b8357dca31f1e260ef407a0b58836','continuation_proof_changed')
    record=json.loads(receipt.read_text());cpp_pin=next(r for r in record['runs'][0]['fixture_pins'] if Path(r['path']).suffix=='.cpp')
    reference=Path(cpp_pin['path']);family.require(family.pin(reference)['sha256']==cpp_pin['sha256'],'reference_continuation_fixture_changed')
    source=reference.read_bytes().decode().replace('\r\n','\n')
    folder=family.W/'tools/research/r192_aot_modules';core=folder/'semantic_pipeline.hpp';continuation=folder/'semantic_continuation.hpp';scratch=folder/'semantic_call_scratch.hpp'
    core_text=core.read_text();continuation_text=continuation.read_text();prospective='CallScratch scratch;' not in core_text
    if prospective:
        core_text=once(core_text,'#include "semantic_vector_effects.hpp"','#include "semantic_vector_effects.hpp"\n#include "semantic_call_scratch.hpp"')
        core_text=once(core_text,'VectorEffects vector;bool vector_effects_admitted{};','VectorEffects vector;CallScratch scratch;bool vector_effects_admitted{};')
        core_text=once(core_text,'if(result.vector_effects_admitted)result.vector.prepared(prepared[1].vector);',
                       'if(result.vector_effects_admitted){result.vector.prepared(prepared[1].vector);result.scratch.prepared={prepared[2].scalar,0u,prepared[3].upper,prepared[3].scale,prepared[3].start,prepared[3].extent,pipeline.filter_limit};}')
        core_text=once(core_text,'if(result.vector_effects_admitted)result.vector.element(tag257,tag256);',
                       'if(result.vector_effects_admitted){result.vector.element(tag257,tag256);result.scratch.element(retained_middle,tag259,tag258,tag769);}')
        core_text=once(core_text,'remapped=ps2::fp::cvt_w_s(ps2::fp::ee::msub(scalar_flags,ps2::fp::ee::add(scalar_flags,v.upper,0),value,v.scale));}',
                       'const Bits accumulator=ps2::fp::ee::add(scalar_flags,v.upper,0);remapped=ps2::fp::cvt_w_s(ps2::fp::ee::msub(scalar_flags,accumulator,value,v.scale));if(result.vector_effects_admitted)result.scratch.accepted(remapped,accumulator);}')
        continuation_text=once(continuation_text,'v.last_mac=next.last_mac;v.last_flags=next.last_flags;v.raised_sticky|=next.raised_sticky;',
                               'v.last_mac=next.last_mac;v.last_flags=next.last_flags;v.raised_sticky|=next.raised_sticky;if(!result_.scratch.merge_element(element.scratch))refuse("semantic continuation: scratch invariant changed");')
    source,n=re.subn(r'#include "[^"\n]*semantic_continuation\.hpp"','@CONTINUATION_INCLUDE@',source)
    family.require(n==1,'continuation_fixture_include_changed')
    source=once(source,'static void assert_state(',PROJECT+'\nstatic void assert_state(')
    source=once(source,' Vu0Regs vector{};project_vector(vector,initial.vu0,native.result());',
                ' require_full_cpu(native,reference,initial,check_budget);\n Vu0Regs vector{};project_vector(vector,initial.vu0,native.result());')
    head,tail=source.split('int main(){try{',1)
    tail=once(tail,'Cpu c{};c.r[4].d[0]=0x100000;',
              'Cpu c=original_seed;for(unsigned reg=1;reg<32;++reg)for(unsigned lane=0;lane<4;++lane)c.r[reg].w[lane]=0x9e3779b9u*(reg+lane+seed+1);for(unsigned f=0;f<32;++f)c.f[f]=special[(seed+f)%8];c.facc=special[(seed+5)%8];c.r[4].d[0]=0x100000;')
    source=head+'int main(){try{'+tail
    source=once(source,'int main(){try{','static int original_boundary_suite(){try{')
    source+=CANCEL
    inputs=[Path(__file__),Path(family.__file__),receipt,reference,a.compiler,*sorted(folder.glob('semantic_*.hpp')),
            *(Path(r['path']) for r in record['source_pins'] if Path(r['path']).suffix=='.bin'),*sorted((a.candidate_build/'src/runtime/include/ps2').glob('*.hpp'))]
    pins=[family.pin(x) for x in dict.fromkeys(inputs)];out.mkdir(parents=True);runs=[]
    variants=[('positive',None,None,None),
              ('planted-product','core','{prepared[2].scalar,0u,prepared[3].upper','{prepared[2].scalar^1u,0u,prepared[3].upper'),
              ('planted-zero','core','{prepared[2].scalar,0u,prepared[3].upper','{prepared[2].scalar,1u,prepared[3].upper'),
              ('planted-retained-block','scratch','retained_middle=retained;','retained_middle={};'),
              ('planted-rejected-remap','scratch','if(next.accepted_observed)accepted(next.remap_word,next.remap_accumulator);','accepted(next.remap_word,next.remap_accumulator);'),
              ('planted-accepted-mask','scratch','accepted_observed=true;','accepted_observed=false;')]
    for name,where,before,after in variants:
        directory=out/name;directory.mkdir();contents={'core':core_text,'continuation':continuation_text,'scratch':scratch.read_text()}
        if where:contents[where]=once(contents[where],before,after)
        for path in folder.glob('semantic_*.hpp'):
            text=contents['core'] if path==core else contents['continuation'] if path==continuation else contents['scratch'] if path==scratch else path.read_text()
            (directory/path.name).write_text(text,encoding='utf8',newline='\n')
        cpp=directory/'test.cpp';cpp.write_text(source.replace('@CONTINUATION_INCLUDE@','#include "'+(directory/continuation.name).as_posix()+'"'),encoding='utf8',newline='\n');exe=directory/'test.exe'
        args=[str(a.compiler),'-std=c++20','-mavx2','-ffp-contract=off','-O2','-static-libgcc','-static-libstdc++','-I'+str((a.candidate_build/'src/runtime/include').resolve()),str(cpp),'-o',str(exe)]
        cp=subprocess.run(args,capture_output=True);(directory/'compile.log').write_bytes(cp.stdout+cp.stderr);family.require(cp.returncode==0,'scratch_fixture_compile_failed:'+name)
        run=subprocess.run([str(exe)],capture_output=True);(directory/'test.log').write_bytes(run.stdout+run.stderr)
        runs.append(dict(name=name,compile_command=args,argv=[str(exe)],exit=run.returncode,expected_exit=0 if name=='positive' else 1,
                         stdout=run.stdout.decode(errors='replace'),stderr=run.stderr.decode(errors='replace'),
                         fixture_pins=[family.pin(cpp),family.pin(exe),*[family.pin(x) for x in directory.glob('*.hpp')]]))
    changed=[r['path'] for r in pins if family.pin(Path(r['path']))['sha256']!=r['sha256']]
    passed=not changed and all(r['exit']==r['expected_exit'] and ('PASS_FULL_CPU_REFERENCE_SCRATCH' if r['name']=='positive' else 'FULL_CPU_DIFFERENCE') in r['stdout']+r['stderr'] for r in runs)
    result=dict(status=('PASS_PROSPECTIVE_FULL_CPU_REFERENCE_SCRATCH' if prospective else 'PASS_FULL_CPU_REFERENCE_SCRATCH') if passed else 'FAIL',
                core_hooks_mode='owned_prospective_copy' if prospective else 'authored_source',runs=runs,source_pins=pins,input_changes=changed,
                scope=['Full Cpu reference comparison at original guard/return boundaries, excluding dispatcher-ownedPC/base; callback-before vs callback-after budget stages remain explicit.',
                       'Typed intermediates and preserve-if-not-executed masks; no register-bank object in native core or continuation.',
                       'All1296 constructed calls/2292 boundaries plus actual prefix/resume evidence from frozen continuation proof, with varied upper lanes/FPR initial values.',
                       '65544 exact original EE subtraction cancellations cover special/raw NaN/overflow input patterns; source helper algebra establishes x-x=positivezero/no flags.'],
                open=['No reference-only preferred-family adapter or production runtime adoption in this test.',
                      'Actual scheduler external effects, sharedVU interleaving and lifetime/admission policies remain separate.'])
    target=out/'RESULT.json';target.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8',newline='\n')
    print(json.dumps(dict(status=result['status'],result=str(target),sha256=family.pin(target)['sha256'])))
    return 0 if passed else 1


PROJECT=r'''
static unsigned full_cpu_comparisons=0;
static void require_full_cpu(const sem::ClipContinuation&native,const Cpu&reference,const Cpu&initial,bool compare_budget){
 Cpu projected=initial;const auto&s=native.result().scratch;
 if(s.element_observed)std::memcpy(&projected.r[1],s.retained_middle.data(),16);
 set32(projected.r[2],native.result().accepted);
 projected.r[4].d[0]=initial.r[4].d[0]+native.consumed()*initial.r[8].d[0];projected.r[5].d[0]=initial.r[5].d[0]+native.consumed()*initial.r[9].d[0];
 projected.r[6].d[0]=initial.r[6].d[0]+native.result().accepted*initial.r[8].d[0];projected.r[7].d[0]=initial.r[7].d[0]+native.result().accepted*initial.r[9].d[0];set32(projected.r[10],native.remaining()-1);
 projected.f[3]=s.prepared.scalar_product;projected.f[4]=s.prepared.zero;projected.f[5]=s.prepared.remap_upper;projected.f[6]=s.prepared.remap_scale;projected.f[7]=s.prepared.remap_start;projected.f[8]=s.prepared.remap_extent;projected.f[10]=s.prepared.filter_limit;
 if(s.element_observed){projected.f[0]=s.resource259;projected.f[1]=s.resource258;projected.f[2]=s.resource769;}
 if(s.accepted_observed){projected.f[9]=s.remap_word;projected.facc=s.remap_accumulator;}
 project_vector(projected.vu0,initial.vu0,native.result());projected.fcr31=sem::no_yield::project_scalar(initial.fcr31,native.result().scalar);
 projected.budget=compare_budget?native.budget():reference.budget;projected.pc=reference.pc;projected.base=reference.base;
 if(std::memcmp(&projected,&reference,sizeof reference)){const auto*a=reinterpret_cast<const unsigned char*>(&projected),*b=reinterpret_cast<const unsigned char*>(&reference);unsigned diffs=0;for(unsigned i=0;i<sizeof reference;++i)if(a[i]!=b[i]&&diffs++<12)std::fprintf(stderr,"FULL_CPU_DIFFERENCE byte=%u native=%02X original=%02X consumed=%u accepted=%u\n",i,a[i],b[i],native.consumed(),native.result().accepted);throw std::runtime_error("FULL_CPU_DIFFERENCE source-derived scratch projection");}
 ++full_cpu_comparisons;
}
'''

CANCEL=r'''
int main(){unsigned cancellation_tests=0;const unsigned special[]={0u,0x80000000u,1u,0x7f7fffffu,0x7fffffffu,0xffc00001u,0x7fc00000u,0xff800000u};
 for(unsigned i=0;i<65544;++i){const unsigned x=i<8?special[i]:i*0x9e3779b9u;unsigned flags=i*0x45d9f3bu;const auto old=flags;if(fp::ee::sub(flags,x,x)!=0||flags!=old){std::fprintf(stderr,"FULL_CPU_DIFFERENCE original cancellation property\n");return 1;}++cancellation_tests;}
 const int result=original_boundary_suite();if(!result)std::printf("PASS_FULL_CPU_REFERENCE_SCRATCH full_Cpu_except_dispatch_PC_base_comparisons=%u original_cancellation_tests=%u source_typed_side_outputs=1 callback_budget_stage_separate=1 native_runtime_adoption=NONE\n",full_cpu_comparisons,cancellation_tests);return result;
}
'''


if __name__=='__main__':raise SystemExit(main())
