"""Typed event continuations at actual original preempt/segment boundaries.

The original service's captured RAM delta, budget and return are explicit
reference-service outputs. They prove no native scheduler and no store order.
All algorithm state is computed independently and patched onto CURRENT Cpu.
"""
from pathlib import Path
import argparse
import json
import subprocess
import sys

sys.path.insert(0,str(Path(__file__).resolve().parent))
import analyze_r192_builder_domain as auth
import verify_r192_resource_observations as shadow
from verify_r192_resource_events import historical_pins


def once(text,old,new):
    auth.need(text.count(old)==1,'guard_shadow_seam_missing:'+old[:60]);return text.replace(old,new)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['observations','shadow-proof','builder-proof','candidate-build','compiler','output']:
        p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--expected-observation-sha',required=True)
    p.add_argument('--expected-shadow-proof-sha',required=True)
    a=p.parse_args();out=a.output.resolve()
    auth.need(out.is_relative_to((auth.W/'build/oracle-telemetry').resolve()) and not out.exists(),'fresh_owned_output_required')
    proof=a.observations/'RESULT.json';auth.need(auth.pin(proof)['sha256']==a.expected_observation_sha,'actual_guard_observation_changed')
    record=json.loads(proof.read_text());auth.need(record['status']=='PASS_BOUNDED_ORIGINAL_PLAN_OBSERVATIONS' and not record['input_changes'],'actual_guard_observation_not_passed')
    prior=a.shadow_proof/'RESULT.json';auth.need(auth.pin(prior)['sha256']==a.expected_shadow_proof_sha,'actual_no_guard_proof_changed')
    no_guard=json.loads(prior.read_text());auth.need(no_guard['status']=='PASS_ACTUAL_NO_GUARD_TYPED_SHADOWS','actual_no_guard_shadow_not_passed')
    builder=a.builder_proof/'original_builder.cpp';auth.need(auth.pin(builder)['sha256']=='ad4dd9a564da533f8d7e9532452fc638cbed346049293b6aba765eeab05489c1','original_builder_changed')
    baseline=auth.W/'build/oracle-telemetry/emitted-resource-actual-event-shadow-03/RESULT.json'
    auth.need(auth.pin(baseline)['sha256']=='545e3f67e0dd9a13bf3501d4cec1a4a3e7728c5a10852c630a29a558788ca496','historical_actual_reference_changed')
    old=json.loads(baseline.read_text());subject_changes=[];historical=[]
    for row in old['source_pins']:
        if Path(row['path']).resolve()==Path(__file__).resolve():
            preserved=auth.W/'build/oracle-telemetry/emitted-resource-candidate04-original-subjects-01/verify_r192_resource_guard_observations.py'
            auth.need(row['sha256']=='5497e1577f15254116c6e7138355fd85a740c0cd3e8a750b85033cf43e865e8c' and auth.pin(preserved)['sha256']==row['sha256'],'historical_verifier_snapshot_changed')
            subject_changes.append(dict(historical=row,current=auth.pin(Path(__file__)),preserved=auth.pin(preserved),role='standalone_verification_subject_not_original'))
        else:historical.append(row)
    subject_changes+=historical_pins(historical)
    subject_changes+=historical_pins(no_guard['source_pins'])
    sources=[Path(__file__),Path(shadow.__file__),Path(auth.__file__),Path(sys.modules[historical_pins.__module__].__file__),proof,prior,baseline,builder,a.compiler]
    for pin in record['observations']:
        auth.need(auth.pin(Path(pin['path']))==pin,'actual_guard_capture_changed');sources.append(Path(pin['path']))
    cases=[]
    for key,status in record['summary']['case_status'].items():
        auth.need(status=='COMPLETE_USABLE_OBSERVATION','guard_case_not_complete')
        m=json.loads((a.observations/'observations'/('case-'+key+'.json')).read_text())
        cases.append(dict(name='case-'+key,prefix=(a.observations/'observations'/('case-'+key+'-segment-1')).resolve().as_posix(),object=m['object'],base=m['base'],words=m['source_total_words'],data=m['source_data_start_words']))
    auth.need(0<len(cases)<=6,'bounded_guard_case_denominator')
    root=auth.W/'tools/research/r192_aot_modules';header=root/'resource_pipeline_events.hpp';projection=root/'reference_resource_projection.hpp'
    original_system=a.candidate_build/'src/runtime/src/system.cpp';original_kernel=a.candidate_build/'src/runtime/src/kernel.cpp'
    auth.need(auth.pin(original_system)['sha256']=='7a75bfed5758f57d4979f919ffa9526a7af337fd7a86871fb9bdcc1c6ea93384','actual_original_preempt_source_changed')
    sources += [header,projection,root/'reference_resource_layout.hpp',root/'resource_pipeline_continuation.hpp',root/'semantic_resource_pipeline.hpp',root/'semantic_vector_effects.hpp',original_system,original_kernel,*(a.candidate_build/'src/runtime/include/ps2').glob('*.hpp')]
    pins=[auth.pin(x) for x in dict.fromkeys(sources)];out.mkdir(parents=True)
    helpers,main=shadow.TEST.split('int main(int argc,char**argv)',1)
    helpers=once(helpers,'b.phase!=Phase::complete||b.consumed!=entry.r[10].w[0]','(b.phase!=Phase::complete&&b.phase!=Phase::suspended)||b.consumed>entry.r[10].w[0]')
    start=' if(std::memcmp(&current.vu0,&entry.vu0,sizeof current.vu0)';line=next(s for s in helpers.splitlines() if s.startswith(start));helpers=once(helpers,line,'')
    helpers=once(helpers,'out.r[10].sd[0]=-1;out.budget=std::int32_t(b.budget);out.pc=entry.r[31].w[0];','out.r[10].sd[0]=b.phase==Phase::complete?-1:std::int32_t(b.remaining_after_pending);out.budget=std::int32_t(b.budget);out.pc=b.phase==Phase::complete?entry.r[31].w[0]:(entry.pc+4*layout.prelude_words)|kUnwind;')
    setup=main.split(' for(const auto&test:cases){',1)[1].split('  if(mode=="--planted-pool-value")',1)[0]
    setup=once(setup,'actual.pc!=0x2e6180','(actual.pc!=0x2e6180&&(actual.pc&kUnwind)==0)')
    setup=once(setup,'  if(mode=="--planted-code-word")g_ram[output]^=1;',TAIL_CHECK+'  if(mode=="--planted-code-word")g_ram[output]^=1;')
    rows=','.join('{"'+c['name']+'","'+c['prefix']+'",'+','.join(str(c[k])+'u' for k in ['object','base','words','data'])+'}' for c in cases)
    prefix=builder.read_text().split('static void read(',1)[0]
    source=out/'actual_guard_shadow.cpp';source.write_text(prefix+'\n#include "'+header.as_posix()+'"\n#include "'+projection.as_posix()+'"\n'+helpers+EVENTS+MAIN.replace('@CASES@',rows).replace('@SETUP@',setup),encoding='utf8',newline='\n')
    compiler=[str(a.compiler),'-std=c++20','-mavx2','-ffp-contract=off','-O1','-static-libgcc','-static-libstdc++','-I'+str((a.candidate_build/'src/runtime/include').resolve())];commands=[]
    def run(argv,name):
        cp=subprocess.run(argv,capture_output=True);log=out/(name+'.log');log.write_bytes(cp.stdout+cp.stderr);commands.append(dict(argv=argv,exit=cp.returncode,log=auth.pin(log)));return cp
    exe=out/'actual_guard_shadow.exe';auth.need(run([*compiler,str(source),'-o',str(exe)],'compile').returncode==0,'actual_guard_shadow_compile_failed');checks=[]
    for flag in ['', '--planted-private-VU','--planted-service-Cpu','--planted-service-alias','--planted-drop-service-effect','--planted-callback-return','--planted-service-budget','--planted-resume-PC','--planted-retention-gate']:
        cp=run([str(exe)]+([flag] if flag else []),flag.removeprefix('--planted-') or 'positive');checks.append(dict(name=flag or 'positive',exit=cp.returncode,expected_exit=1 if flag else 0,stdout=cp.stdout.decode(errors='replace'),stderr=cp.stderr.decode(errors='replace')))
    changes=[pin['path'] for pin in pins if auth.pin(Path(pin['path']))!=pin];passed=not changes and all(c['exit']==c['expected_exit'] for c in checks)
    result=dict(schema='r192_actual_original_service_conditional_event_shadow_v2',status='PASS_ACTUAL_SERVICE_CONDITIONAL_EVENT_SHADOWS' if passed else 'FAIL',invocation=[sys.executable,*sys.argv],source_pins=pins,native_subject_changes=subject_changes,immutable_original_inputs_checked=True,input_changes=changes,commands=commands,checks=checks,cases=cases,artifacts=[auth.pin(source),auth.pin(exe)],
        limits=['Actual original execution plus offline typed event shadows; no native runtime route installed.',
                'Original scheduler RAM delta/budget/return are explicit reference-service boundary outputs, not a native scheduler proof or ordered store trace.',
                'All algorithm output bytes and full1456 current-Cpu boundary effects are independently calculated; no reference post-Cpu values fill algorithm state.',
                'Private/sharedVU and owned memory/code/descriptor lifetime interference reject. Unrelated CURRENT context fields remain current.',
                'Three observed logical configurations do not admit other plans, partial/clamp paths or the whole producer universe.'])
    dest=out/'RESULT.json';dest.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8',newline='\n');print(json.dumps(dict(status=result['status'],result=str(dest),sha256=auth.pin(dest)['sha256'])));return 0 if passed else 1


TAIL_CHECK=r'''
  auto source_word=[&](unsigned i){unsigned value;std::memcpy(&value,original.data()+test.base+4*i,4);return value;};
  if(active_words<2||source_word(active_words-2)!=0x03e00008u||source_word(active_words-1)!=0||source_word(active_words)!=0||source_word(active_words+1)!=0)refuse("source executable JR/delay/padding separation unproved");
  unsigned static_branches=0;
  for(unsigned i=0;i<active_words;++i){const auto w=source_word(i),op=w>>26;
   if(op==2||op==3||(op==0&&(w&63)==9))refuse("unproved nonlocal direct/indirect emitted call");
   if(op==0&&(w&63)==8&&i!=active_words-2)refuse("unproved emitted JR outside return tail");
   if(op==1||(op>=4&&op<=7)||(op==0x11&&((w>>21)&31)==8)){const auto target=std::int64_t(i)+1+std::int16_t(w&0xffff);if(i+1>=active_words||target<0||target>=active_words)refuse("emitted branch/delay reaches excluded padding or data");++static_branches;}
  }
  std::printf("SOURCE_EXIT_JOIN %s active_words=%u included_JR_index=%u included_delay_index=%u excluded_NOPS=2 static_local_branches=%u\n",test.name,active_words,active_words-2,active_words-1,static_branches);
'''


EVENTS=r'''
static bool private_equal(const Cpu&a,const Cpu&b){return !std::memcmp(a.r,b.r,sizeof a.r)&&!std::memcmp(&a.hi,&b.hi,sizeof a.hi)&&!std::memcmp(&a.lo,&b.lo,sizeof a.lo)&&a.sa==b.sa&&!std::memcmp(a.f,b.f,sizeof a.f)&&a.facc==b.facc&&a.fcr31==b.fcr31&&!std::memcmp(&a.vu0,&b.vu0,sizeof a.vu0);}
struct Service {
 const Case*test;const Prepared*prepared;Cpu entry,current;std::string root,mode;unsigned segment=0,guards=0,changes=0;
 bool owned(unsigned address)const{
  auto contains=[&](std::uint64_t begin,std::uint64_t bytes){return address>=begin&&std::uint64_t(address)-begin<bytes;};
  if(contains(test->base,4ull*(test->words+4))||contains(test->object,928)||contains(entry.r[29].d[0],496))return true;
  const auto&d=prepared->descriptor;
  // Attributes and source constant reads must not be changed by this service.
  const auto owner=ld32(test->object+72),attributes=ld32(owner+540);
  if(contains(owner,544)||contains(attributes,12ull*d.attribute_count))return true;
  for(unsigned i=0;i<d.attribute_count;++i){const auto&a=d.attributes[i];const auto bytes=width(format(a.tag));
   const auto stride=entry.r[8+a.stream].d[0];const auto count=entry.r[10].w[0];
   const std::uint64_t length=count?std::uint64_t(count-1)*stride+bytes:0;
   if(contains(entry.r[4+a.stream].d[0]+a.offset,length)||contains(entry.r[6+a.stream].d[0]+a.offset,length))return true;
   if(a.tag==259&&mode!="--planted-retention-gate"){const auto retained=count?std::uint64_t(count-1)*stride+16:0;if(contains(entry.r[4+a.stream].d[0]+(a.offset&~15u),retained)||contains(entry.r[6+a.stream].d[0]+(a.offset&~15u),retained))return true;}
  }return false;
 }
 static bool invoke(void*raw,const Boundary&before,std::int64_t&budget){auto&s=*static_cast<Service*>(raw);if(++s.guards>2)refuse("actual guard capture cap exceeded");
  const auto path=s.root+"-segment-"+std::to_string(s.segment)+"-guard-"+std::to_string(s.guards);
  Cpu observed_before{},observed_after{};read_exact(path+"-before-Cpu.bin",&observed_before,sizeof observed_before);read_exact(path+"-after-Cpu.bin",&observed_after,sizeof observed_after);
  auto projected=apply_current(s.current,s.entry,*s.prepared,before);projected.pc=s.current.pc;
  if(std::memcmp(&projected,&observed_before,sizeof projected)){unsigned at=0;while(at<sizeof projected&&reinterpret_cast<const unsigned char*>(&projected)[at]==reinterpret_cast<const unsigned char*>(&observed_before)[at])++at;std::fprintf(stderr,"GUARD_CPU_DIFFERENCE %s guard=%u byte=%u\n",s.test->name,s.guards,at);refuse("original preempt input differs from native algorithm");}
  std::vector<unsigned char>ram_before(kRamSize),ram_after(kRamSize);read_exact(path+"-before-RAM.bin",ram_before.data(),kRamSize);read_exact(path+"-after-RAM.bin",ram_after.data(),kRamSize);
  if(std::memcmp(g_ram,ram_before.data(),kRamSize))refuse("original preempt RAM input differs from native algorithm");
  // Original preempt/refill owns budget. Every other Cpu field must match
  // independently calculated input: no copy of the post-Cpu fills state.
  auto service_cpu=projected;service_cpu.budget=observed_after.budget;
  if(s.mode=="--planted-service-Cpu")observed_after.vu0.vf[31].w[2]^=1;
  if(std::memcmp(&service_cpu,&observed_after,sizeof service_cpu))refuse("unproved original service private/shared CPU effect");
  if(s.mode=="--planted-service-alias")ram_after[s.entry.r[6].w[0]]^=1;
  unsigned changed=0;for(unsigned i=0;i<kRamSize;++i)if(ram_before[i]!=ram_after[i]){if(s.owned(i))refuse("original service aliases active owned frame/streams/code/descriptor");
   std::printf("SERVICE_BOUNDARY_BYTE %s guard=%u address=%08X before=%02X after=%02X\n",s.test->name,s.guards,i,ram_before[i],ram_after[i]);if(s.mode!="--planted-drop-service-effect")g_ram[i]=ram_after[i];++changed;}
  s.changes+=changed;s.current=service_cpu;budget=service_cpu.budget;if(s.mode=="--planted-service-budget")++budget;
  unsigned result=0;read_exact(path+"-result.bin",&result,4);if(result>1)refuse("original guard result unbound");if(s.mode=="--planted-callback-return")result^=1;
  return result!=0;
 }
};
'''
MAIN=r'''
int main(int argc,char**argv){try{const std::string mode=argc>1?argv[1]:"";unsigned completed=0,code_words=0,ambient=0,total_guards=0,total_segments=0,total_changes=0;const Case cases[]={@CASES@};
 for(const auto&test:cases){@SETUP@
  EventContinuation native(streams,prepared);std::string root=test.prefix;root.resize(root.size()-10);
  Service service{&test,&prepared,entry,entry,root,mode};
  {auto narrow=prepared;narrow.descriptor.attribute_count=1;narrow.descriptor.attributes[0]={259,0,12};auto control=service;control.prepared=&narrow;control.entry.r[4].d[0]=0x100000;control.entry.r[6].d[0]=0x200000;control.entry.r[8].d[0]=16;control.entry.r[10].d[0]=1;for(unsigned i=0;i<16;++i)if(!control.owned(0x100000+i)||!control.owned(0x200000+i))refuse("tag259 aligned16 retention extent gate missing");if(control.owned(0x100010)||control.owned(0x200010))refuse("tag259 retention extent overclaimed");}
  Cpu last{};std::vector<unsigned char>last_ram;
  for(unsigned segment=1;segment<=3;++segment){
   const auto path=root+"-segment-"+std::to_string(segment);Cpu current{},observed_after{};read_exact(path+"-before-Cpu.bin",&current,sizeof current);read_exact(path+"-after-Cpu.bin",&observed_after,sizeof observed_after);
   if(segment>1){if(mode=="--planted-private-VU")current.vu0.vf[31].w[2]^=1;
    if(!private_equal(last,current))refuse("unproved intersegment private/shared VU change");
    if(current.pc!=(last.pc&~kUnwind))refuse("actual natural resume PC differs from retained boundary");
   }
   read_exact(path+"-before-RAM.bin",g_ram,kRamSize);
   // Code identity/lifetime is checked every segment. Pure data constants
   // were snapshotted once; the reference does not reload them on resume.
   const unsigned active=plan.intended[0]+plan.intended[1]+plan.intended[3]+plan.intended[4]+plan.intended[5]+plan.intended[6]-2;
   if(std::memcmp(g_ram+test.base,original.data()+test.base,4*active))refuse("active code lifetime changed between segments");
   const auto logical=descriptor(current,test.object);if(std::memcmp(logical.attributes.data(),d.attributes.data(),12*d.attribute_count)||logical.attribute_count!=d.attribute_count||logical.effect_count!=d.effect_count||logical.filter_count!=d.filter_count)refuse("descriptor lifetime changed between segments");
   if(segment>1){for(unsigned address=0;address<kRamSize;++address)if(service.owned(address)&&g_ram[address]!=last_ram[address])refuse("unproved intersegment active-owned memory change");}
   service.segment=segment;service.current=current;
   const auto before_guards=service.guards;const auto boundary=native.advance(current.budget,{&service,Service::invoke});
   auto projected=apply_current(service.current,entry,prepared,boundary);
   if(mode=="--planted-resume-PC"&&boundary.phase==Phase::suspended)projected.pc+=4;
   std::vector<unsigned char>after_ram(kRamSize);read_exact(path+"-after-RAM.bin",after_ram.data(),kRamSize);
   if(std::memcmp(&projected,&observed_after,sizeof projected)||std::memcmp(g_ram,after_ram.data(),kRamSize)){unsigned at=0,ram=0;while(at<sizeof projected&&reinterpret_cast<const unsigned char*>(&projected)[at]==reinterpret_cast<const unsigned char*>(&observed_after)[at])++at;while(ram<kRamSize&&g_ram[ram]==after_ram[ram])++ram;std::fprintf(stderr,"SEGMENT_DIFFERENCE %s segment=%u Cpu_byte=%u RAM_byte=%08X\n",test.name,segment,at,ram);return 1;}
   ++total_segments;std::printf("PASS %s segment=%u consumed=%u accepted=%u suspended=%u budget=%lld callbacks=%u fullCpu=1456 fullRAM=33554432\n",test.name,segment,boundary.consumed,boundary.accepted,boundary.phase==Phase::suspended,(long long)boundary.budget,service.guards-before_guards);
   if(boundary.phase==Phase::complete){++completed;break;}last=projected;last_ram=std::move(after_ram);
   if(segment==3)refuse("actual event continuation did not complete within capture cap");
  }total_guards+=service.guards;total_changes+=service.changes;
 }
 if(completed!=std::size(cases))refuse("actual completed case denominator mismatch");
 std::printf("PASS completed_actual_cases=%u segments=%u actual_guard_boundaries=%u service_changed_bytes=%u source_code_words=%u native_scheduler=UNPROVED actual_store_order=UNMEASURED native_adoption=0\n",completed,total_segments,total_guards,total_changes,code_words);return 0;
}catch(const std::exception&e){std::fprintf(stderr,"REFUSED %s\n",e.what());return 1;}}
'''


if __name__=='__main__':raise SystemExit(main())
