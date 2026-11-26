"""Standalone actual-boundary and lifecycle controls for the reference adapter.

The real source-plan capture supplies inputs and independent expectations.
Scheduler RAM/budget/return are explicit reference-service outputs in this
fixture, not a native scheduler implementation. No game process is launched.
"""
from pathlib import Path
import argparse
import json
import subprocess
import sys

sys.path.insert(0,str(Path(__file__).resolve().parent))
import specialize_r192_stub_family as auth
from capture_r192_family_boundary import pin


def dependency_join(actual,expected):
    joins=[]
    for row in actual:
        key=Path(row['path']).resolve();bound=expected.get(key)
        auth.require(bound is not None,'build_dependency_pin_missing:'+str(key))
        auth.require(row['sha256']==bound['sha256'],'build_dependency_bytes_differ:'+str(key))
        joins.append(dict(source=row,build_binding=bound))
    return joins


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['adapter-build','observations','compiler','output']:
        p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--expected-build-sha',required=True)
    a=p.parse_args();out=a.output.resolve();auth.require(out.is_relative_to((auth.W/'build/oracle-telemetry').resolve()) and not out.exists(),'fresh_owned_output_required')
    proof=a.observations/'RESULT.json';auth.require(pin(proof)['sha256']=='bc94ab802cc7e2108829d8f626d105868332fd54dc838a3829db10e9d90360da','actual_boundary_capture_changed')
    observed=json.loads(proof.read_text());sources=[Path(__file__),Path(auth.__file__),Path(sys.modules[pin.__module__].__file__),proof,a.compiler]
    for row in observed['observations']:auth.require(pin(Path(row['path']))==row,'actual_boundary_bytes_changed');sources.append(Path(row['path']))
    factory=a.adapter_build/'BUILD.json';auth.require(pin(factory)['sha256']==a.expected_build_sha,'supplied_build_receipt_changed')
    build=json.loads(factory.read_text());auth.require(build['status']=='BOUNDED_THREE_PLAN_RESOURCE_ADAPTER_BUILT' and not build['input_changes'],'measured_adapter_contract_required')
    auth.require(pin(Path(build['candidate']))['sha256']==build['candidate_sha256'],'supplied_build_binary_changed')
    for row in build['private_sources']+build['private_objects']:auth.require(pin(Path(row['path']))==row,'supplied_build_private_artifact_changed')
    contract=a.adapter_build/'source/resource_contract.hpp';auth.require(any(pin(contract)==r for r in build['private_sources']),'source_authenticated_contract_changed');sources += [factory,contract]
    normal=auth.W/'build/r192-compiled-konquest';root=auth.W/'tools/research/r192_aot_modules';header=root/'reference_resource_event_adapter.hpp'
    original=auth.W/'build/oracle-telemetry/emitted-builder-actual-descriptor-06/original_builder.cpp'
    auth.require(pin(original)['sha256']=='ad4dd9a564da533f8d7e9532452fc638cbed346049293b6aba765eeab05489c1','original_memory_fixture_changed')
    prefix=original.read_text().split('static std::map<unsigned,unsigned> entries_observed;',1)[0]
    prefix=prefix.replace('bool preempt(Cpu&){throw std::runtime_error("offline producer budget exhausted");}','bool preempt(Cpu&);Cpu g_cpu;')
    auth.require('Cpu g_cpu;' in prefix,'original_preempt_test_seam_changed')
    rows=[]
    for i in range(1,4):rows.append('"'+(a.observations/'observations'/('case-'+str(i))).resolve().as_posix()+'"')
    dependencies=[header,root/'resource_pipeline_events.hpp',root/'resource_pipeline_continuation.hpp',root/'reference_resource_layout.hpp',root/'semantic_resource_pipeline.hpp',root/'semantic_vector_effects.hpp',a.compiler,*(normal/'src/runtime/include/ps2').glob('*.hpp'),normal/'src/runtime/src/kernel.hpp']
    # Direct authored headers/compiler must match THIS build. The pinned
    # source-proof/lineage chain supplies the original ABI header identities
    # already authenticated by that same factory at production compilation.
    direct={Path(row['path']).resolve():dict(row,origin='supplied BUILD.source_pins') for row in build['source_pins']}
    bound=dict(direct)
    event=Path(build['event_proof']['path']) if 'event_proof' in build else auth.W/'build/oracle-telemetry/emitted-resource-actual-event-shadow-03/RESULT.json'
    old_freeze=auth.W/'build/oracle-telemetry/emitted-reference-preferred-probe-01/FREEZE_BUILD_LINK.json'
    for source in [event,old_freeze]:auth.require(source.resolve() in direct and pin(source)['sha256']==direct[source.resolve()]['sha256'],'build_evidence_chain_changed:'+str(source))
    for row in json.loads(event.read_text())['source_pins']:
        key=Path(row['path']).resolve()
        if key not in bound:bound[key]=dict(row,origin='supplied BUILD -> authenticated event proof')
    lineage_pin=json.loads(old_freeze.read_text())['original_N_lineage_record'];lineage=Path(lineage_pin['path']);auth.require(pin(lineage)==lineage_pin,'build_original_ABI_lineage_changed')
    original_source=json.loads(lineage.read_text())['v8_source']
    for name,digest in original_source['files'].items():
        key=(Path(original_source['root'])/name).resolve()
        if key not in bound:bound[key]=dict(path=str(key),sha256=digest,origin='supplied BUILD -> authenticated lifecycle freeze -> original v8 lineage')
    actual_dependencies=[pin(x) for x in dict.fromkeys(dependencies)]
    # Do not let an indirect proof hide a missing direct production source pin.
    direct_dependencies=[pin(x) for x in dependencies[:7]]
    dependency_join(direct_dependencies,direct)
    joined=dependency_join(actual_dependencies,bound)
    provenance_controls=[]
    for label,path,missing in [('wrong_adapter_header',header,False),('missing_adapter_header',header,True),('wrong_compiler',a.compiler,False),('wrong_event_dependency',root/'resource_pipeline_events.hpp',False)]:
        mutant=dict(bound);key=path.resolve()
        if missing:mutant.pop(key)
        else:mutant[key]=dict(mutant[key],sha256='0'*64)
        try:dependency_join(actual_dependencies,mutant)
        except ValueError as error:
            auth.require(str(error).startswith('build_dependency_pin_missing:' if missing else 'build_dependency_bytes_differ:'),'wrong_provenance_refusal_reason')
            provenance_controls.append(dict(name=label,status='REJECTED',reason=str(error)))
        else:raise ValueError('provenance_mutant_accepted:'+label)
    sources += [original,*dependencies,event,old_freeze,lineage,Path(build['candidate']),*[Path(row['path']) for row in build['private_sources']+build['private_objects']]];pins=[pin(x) for x in dict.fromkeys(sources)]
    out.mkdir(parents=True);cpp=out/'adapter_controls.cpp';cpp.write_text(prefix+'\n#include "'+header.as_posix()+'"\n#include "'+contract.resolve().as_posix()+'"\n'+TEST.replace('@CASES@',','.join(rows)),encoding='utf8',newline='\n')
    compiler=[str(a.compiler),'-std=c++20','-mavx2','-ffp-contract=off','-O1','-static-libgcc','-static-libstdc++','-I'+str(normal/'src/runtime/include'),'-I'+str(normal/'src/runtime/src')];commands=[]
    def run(argv,name):
        cp=subprocess.run(argv,capture_output=True);log=out/(name+'.log');log.write_bytes(cp.stdout+cp.stderr);commands.append(dict(argv=argv,exit=cp.returncode,log=pin(log)));return cp
    exe=out/'adapter_controls.exe';auth.require(run([*compiler,str(cpp),'-o',str(exe)],'compile').returncode==0,'connected_adapter_controls_compile_failed');checks=[]
    flags=['','reset-during-guard','retire-during-guard','private-VU','owned-RAM','changed-code','changed-key','changed-resume-PC','changed-caller','service-private','output-alias','unknown-code','orphan-resume']
    for name in flags:
        cp=run([str(exe)]+(['--planted-'+name] if name else []),name or 'positive');checks.append(dict(name=name or 'positive',exit=cp.returncode,expected_exit=1 if name else 0,stdout=cp.stdout.decode(errors='replace'),stderr=cp.stderr.decode(errors='replace')))
    # Restore the precise old escape to AOT in a private header only. The
    # changed-PC control must then fail for the missing named refusal.
    mutant=out/'undo-retained-PC-gate.hpp';text=header.read_text();needle='if(found!=pending_.end())fail("retained resume PC is not admitted");'
    auth.require(text.count(needle)==1,'retained_PC_mutation_seam_changed');text=text.replace(needle,'/* planted old AOT escape */')
    for name in ['resource_pipeline_events.hpp','reference_resource_layout.hpp']:text=text.replace('#include "'+name+'"','#include "'+(root/name).as_posix()+'"')
    mutant.write_text(text,encoding='utf8',newline='\n');mutant_cpp=out/'undo-retained-PC-gate.cpp';mutant_cpp.write_text(cpp.read_text().replace(header.as_posix(),mutant.as_posix()),encoding='utf8',newline='\n');mutant_exe=out/'undo-retained-PC-gate.exe'
    auth.require(run([*compiler,str(mutant_cpp),'-o',str(mutant_exe)],'compile-undo-retained-PC-gate').returncode==0,'retained_PC_mutant_compile_failed')
    cp=run([str(mutant_exe),'--planted-changed-resume-PC'],'undo-retained-PC-gate');auth.require(cp.returncode==2 and b'TEST_PROVED_PLAN_NOT_SELECTED' in cp.stderr,'retained_PC_old_escape_not_detected')
    checks.append(dict(name='undo-retained-PC-gate',exit=cp.returncode,expected_exit=2,stdout=cp.stdout.decode(errors='replace'),stderr=cp.stderr.decode(errors='replace')))
    changes=[r['path'] for r in pins if pin(Path(r['path']))!=r];passed=not changes and all(c['exit']==c['expected_exit'] for c in checks)
    result=dict(status='PASS_CONNECTED_RESOURCE_ADAPTER_CONTROLS' if passed else 'FAIL',schema='r192_three_plan_reference_adapter_controls_v2',invocation=[sys.executable,*sys.argv],supplied_build=pin(factory),production_candidate=pin(Path(build['candidate'])),build_dependency_join=joined,provenance_controls=provenance_controls,test_tool_is_linked_into_production_PE=False,source_pins=pins,input_changes=changes,commands=commands,checks=checks,artifacts=[pin(cpp),pin(exe)],scope='Standalone adapter replay of actual source-boundary inputs, including all3natural suspend/resume calls and reference-service boundary effects. Tested source dependencies/compiler match supplied BUILD. The test verifier is not linked into production PE. Separate from actual connected game run; no native scheduler/ownership/credit.')
    dest=out/'RESULT.json';dest.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8',newline='\n');print(json.dumps(dict(status=result['status'],sha256=pin(dest)['sha256'],result=str(dest))));return 0 if passed else 1


TEST=r'''
using namespace ps2;using mka::emitted::resource_reference::Adapter;
static int current_test_thread=1;namespace ps2::kernel {int current_thread(){return current_test_thread;}}
static Adapter*active_adapter=nullptr;static std::string root,mode;static unsigned segment=0,guard_count=0;
static void read_exact(const std::string&name,void*p,unsigned size){std::ifstream f(name,std::ios::binary);f.read(static_cast<char*>(p),size);if(!f||f.peek()!=EOF)throw std::runtime_error("test input length mismatch");}
namespace ps2 {bool preempt(Cpu&c){
 const auto path=root+"-segment-"+std::to_string(segment)+"-guard-"+std::to_string(++guard_count);Cpu before{},after{};read_exact(path+"-before-Cpu.bin",&before,sizeof before);read_exact(path+"-after-Cpu.bin",&after,sizeof after);
 if(std::memcmp(&c,&before,sizeof c))throw std::runtime_error("TEST_GUARD_INPUT_CPU_DIFFERENCE");std::vector<unsigned char>ram(kRamSize);read_exact(path+"-before-RAM.bin",ram.data(),kRamSize);if(std::memcmp(g_ram,ram.data(),kRamSize))throw std::runtime_error("TEST_GUARD_INPUT_RAM_DIFFERENCE");
 // Only the source service's observed budget is used from post-Cpu. All
 // other fields must equal the independently computed preempt input.
 c.budget=after.budget;if(std::memcmp(&c,&after,sizeof c))throw std::runtime_error("TEST_SERVICE_UNPROVED_CPU_EFFECT");read_exact(path+"-after-RAM.bin",g_ram,kRamSize);
 unsigned result;read_exact(path+"-result.bin",&result,4);if(result>1)throw std::runtime_error("TEST_INVALID_GUARD_RESULT");
 if(mode=="--planted-reset-during-guard")active_adapter->reset();if(mode=="--planted-retire-during-guard")active_adapter->retire(current_test_thread);if(mode=="--planted-service-private")c.vu0.vf[31].w[2]^=1;
 return result!=0;
}}
int main(int argc,char**argv){mode=argc>1?argv[1]:"";unsigned completed=0,boundaries=0,guards=0;const char*cases[]={@CASES@};try{
 for(const auto*name:cases){Adapter adapter(resource_contract);active_adapter=&adapter;root=name;guard_count=0;current_test_thread=1;
  for(segment=1;segment<=2;++segment){const auto path=root+"-segment-"+std::to_string(segment);read_exact(path+"-before-Cpu.bin",&g_cpu,sizeof g_cpu);read_exact(path+"-before-RAM.bin",g_ram,kRamSize);
   if(segment==1){if(mode=="--planted-changed-caller")g_ram[0x2e612c]^=1;
    if(mode=="--planted-output-alias"){const auto object=g_cpu.r[19].w[0],target=ld32(object+76);st32(object+40,target-ld32(object+4)*ld32(object+44));g_cpu.r[6].d[0]=target;}
    if(mode=="--planted-unknown-code"){g_ram[g_cpu.pc]^=1;const Cpu before=g_cpu;const std::vector<unsigned char>ram(g_ram,g_ram+kRamSize);if(adapter.try_dispatch(g_cpu)||adapter.pending()||std::memcmp(&before,&g_cpu,sizeof before)||std::memcmp(g_ram,ram.data(),kRamSize))throw std::runtime_error("TEST_UNKNOWN_CODE_DID_NOT_RETAIN_AOT");std::printf("REJECTED unknown-code: UNADMITTED original compiled AOT route unchanged\n");return 1;}
    if(mode=="--planted-orphan-resume"){read_exact(root+"-segment-2-before-Cpu.bin",&g_cpu,sizeof g_cpu);read_exact(root+"-segment-2-before-RAM.bin",g_ram,kRamSize);const Cpu before=g_cpu;const std::vector<unsigned char>ram(g_ram,g_ram+kRamSize);if(adapter.try_dispatch(g_cpu)||adapter.pending()||std::memcmp(&before,&g_cpu,sizeof before)||std::memcmp(g_ram,ram.data(),kRamSize))throw std::runtime_error("TEST_ORPHAN_WAS_REHYDRATED");std::printf("REJECTED orphan-resume: UNADMITTED original compiled AOT route unchanged\n");return 1;}
   }else {if(mode=="--planted-private-VU")g_cpu.vu0.vf[31].w[2]^=1;if(mode=="--planted-owned-RAM")g_ram[g_cpu.r[19].w[0]+900]^=1;if(mode=="--planted-changed-code")g_ram[ld32(g_cpu.r[19].w[0]+76)]^=1;if(mode=="--planted-changed-key")g_cpu.r[29].d[0]+=16;if(mode=="--planted-changed-resume-PC")g_cpu.pc+=4;}
   if(!adapter.try_dispatch(g_cpu))throw std::runtime_error("TEST_PROVED_PLAN_NOT_SELECTED");Cpu expected{};read_exact(path+"-after-Cpu.bin",&expected,sizeof expected);std::vector<unsigned char>ram(kRamSize);read_exact(path+"-after-RAM.bin",ram.data(),kRamSize);
   if(std::memcmp(&g_cpu,&expected,sizeof expected)||std::memcmp(g_ram,ram.data(),kRamSize))throw std::runtime_error("TEST_ACTUAL_FULL_CPU_RAM_DIFFERENCE");++boundaries;
  }
  if(adapter.pending())throw std::runtime_error("TEST_COMPLETED_CONTEXT_RETAINED");++completed;guards+=guard_count;std::printf("PASS actual_adapter_case=%s complete=1 boundaries=2 guards=%u\n",name,guard_count);
 }
 if(!mode.empty())throw std::runtime_error("TEST_MUTANT_ACCEPTED");std::printf("PASS actual_calls=%u fullCpu_RAM_boundaries=%u actual_guard_boundaries=%u live_scheduler=REFERENCE_SERVICE_FIXTURE\n",completed,boundaries,guards);return 0;
}catch(const std::exception&e){const std::string reason=e.what();const std::pair<const char*,const char*>expected[]={
 {"--planted-reset-during-guard","context revoked during original callback"},{"--planted-retire-during-guard","context revoked during original callback"},{"--planted-private-VU","retained private/shared VU changed"},{"--planted-owned-RAM","active-owned memory changed during suspension"},{"--planted-changed-code","retained executable words changed"},{"--planted-changed-key","retained context key changed"},{"--planted-changed-resume-PC","retained resume PC is not admitted"},{"--planted-changed-caller","known plan caller/frame/data binding unproved"},{"--planted-service-private","original callback changed unproved private/shared state"},{"--planted-output-alias","output aliases code/descriptor/frame"}};
 for(const auto&[flag,part]:expected)if(mode==flag&&reason.find(part)!=std::string::npos){std::printf("REJECTED %s reason=%s\n",flag,e.what());return 1;}
 std::fprintf(stderr,"UNEXPECTED_REFUSAL %s\n",e.what());return 2;}}
'''


if __name__=='__main__':raise SystemExit(main())
