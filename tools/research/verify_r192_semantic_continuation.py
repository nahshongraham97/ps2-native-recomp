"""Prove typed loop suspension against original compiled whole-family code.

Scheduler scripts are constructed controls. One low-budget stop is anchored to
an actual original call/return capture; one resume starts from an actual capture.
No game execution, runtime integration or protected writes occur here.
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
    family.require(text.count(before)==1,'continuation_source_seam_changed:'+before[:60])
    return text.replace(before,after)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['vector-proof','observations','preempt-observations','resume-boundary','candidate-build','compiler','output']:
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();out=a.output.resolve()
    family.require(out.is_relative_to((family.W/'build/oracle-telemetry').resolve()) and not out.exists(),'fresh_owned_output_required')
    proof=a.vector_proof/'RESULT.json';family.require(family.pin(proof)['sha256']=='a57fd9d7fe9c7d363bc3f256bca5b9af6b953e0847490da2244a1a08f7603397','vector_proof_changed')
    observation=a.observations/'RESULT.json';family.require(family.pin(observation)['sha256']=='0219eacefa312ccf3c4734c623d7e8c1032b8339b275fa540fd0e64d9bd95cb9','actual_observation_receipt_changed')
    obs=json.loads(observation.read_text())
    for row in obs['observation_pins']:family.require(family.pin(Path(row['path']))['sha256']==row['sha256'],'actual_observation_artifact_changed')
    preempt_proof=a.preempt_observations/'RESULT.json'
    family.require(family.pin(preempt_proof)['sha256']=='6c97ec2c4c4e28bea7a0188ea7f10ec3f4450ee0cb8d2029178331dcf10668d3','preempt_observation_receipt_changed')
    preempt=json.loads(preempt_proof.read_text())
    for row in preempt['observation_pins']:family.require(family.pin(Path(row['path']))['sha256']==row['sha256'],'preempt_observation_artifact_changed')
    event_folder=next(Path(r['path']).parent for r in preempt['observation_pins'] if Path(r['path']).name=='case-3-preempt-before-Cpu.bin')
    for name in ['before-Cpu','before-RAM','after-Cpu','after-RAM']:
        family.require((event_folder/('case-3-'+name+'.bin')).read_bytes()==(a.observations/'observations'/('case-3-'+name+'.bin')).read_bytes(),'independent_observation_join_failed:'+name)
    record=json.loads(proof.read_text());pin=next(r for r in record['runs'][0]['fixture_pins'] if Path(r['path']).suffix=='.cpp')
    fixture=Path(pin['path']);family.require(family.pin(fixture)['sha256']==pin['sha256'],'original_vector_fixture_changed')
    source=fixture.read_bytes().decode().replace('\r\n','\n')
    source=once(source,'bool preempt(Cpu&){++preempt_calls;return yield_requested;}','bool preempt(Cpu&);')
    source=once(source,'static void reference_regenerated(Cpu&c){L_01C00000:',
                'static bool reference_resume=false;\nstatic void reference_regenerated(Cpu&c){if(reference_resume)goto L_01C00030;L_01C00000:')
    source=once(source,'\nint main(int argc,char**argv)','\nstatic int prior_vector_matrix(int argc,char**argv)')
    source,n=re.subn(r'#include "[^"\n]*semantic_no_yield_adapter\.hpp"','@CONTINUATION_INCLUDE@',source)
    family.require(n==1,'vector_include_changed')
    source+=TEST.replace('@ACTUAL@',(a.observations/'observations/case-3').resolve().as_posix()).replace('@PREEMPT@',(event_folder/'case-3').as_posix()).replace('@RESUME@',a.resume_boundary.resolve().as_posix())
    folder=family.W/'tools/research/r192_aot_modules';header=folder/'semantic_continuation.hpp'
    inputs=[Path(__file__),Path(family.__file__),proof,fixture,observation,preempt_proof,a.compiler,*sorted(folder.glob('semantic_*.hpp')),
            *(Path(r['path']) for r in obs['observation_pins']),*sorted(a.resume_boundary.glob('*.bin')),
            *(Path(r['path']) for r in preempt['observation_pins']),
            *sorted((a.candidate_build/'src/runtime/include/ps2').glob('*.hpp'))]
    # All reference program files and original capture files used by the frozen
    # fixture are retained as explicit input dependencies.
    inputs += [Path(r['path']) for r in record['source_pins'] if Path(r['path']).suffix=='.bin']
    pins=[family.pin(x) for x in dict.fromkeys(inputs)];out.mkdir(parents=True);runs=[]
    variants=[('positive',None,None),
              ('planted-prelude-cost','charge(12);charge(2);','charge(11);charge(2);'),
              ('planted-equality','if(budget_>=0)return false;','if(budget_>0)return false;'),
              ('planted-resume-charge','}else if(phase_!=ContinuationPhase::suspended)refuse("semantic continuation: resume phase unproved");', '}else if(phase_!=ContinuationPhase::suspended)refuse("semantic continuation: resume phase unproved");else charge(2);'),
              ('planted-consumed','++consumed_;charge(2);','consumed_+=2;charge(2);'),
              ('planted-sticky','result_.scalar.raised_flags|=element.scalar.raised_flags;','result_.scalar.raised_flags=element.scalar.raised_flags;')]
    for name,before,after in variants:
        directory=out/name;directory.mkdir();text=header.read_text()
        if before is not None:text=once(text,before,after)
        (directory/header.name).write_text(text,encoding='utf8',newline='\n')
        for path in folder.glob('semantic_*.hpp'):
            if path.name!=header.name:(directory/path.name).write_bytes(path.read_bytes())
        cpp=directory/'test.cpp';cpp.write_text(source.replace('@CONTINUATION_INCLUDE@','#include "'+(directory/header.name).as_posix()+'"'),encoding='utf8',newline='\n');exe=directory/'test.exe'
        args=[str(a.compiler),'-std=c++20','-mavx2','-ffp-contract=off','-O2','-static-libgcc','-static-libstdc++','-I'+str((a.candidate_build/'src/runtime/include').resolve()),str(cpp),'-o',str(exe)]
        cp=subprocess.run(args,capture_output=True);(directory/'compile.log').write_bytes(cp.stdout+cp.stderr);family.require(cp.returncode==0,'continuation_fixture_compile_failed:'+name)
        run=subprocess.run([str(exe)],capture_output=True);(directory/'test.log').write_bytes(run.stdout+run.stderr)
        runs.append(dict(name=name,compile_command=args,argv=[str(exe)],exit=run.returncode,expected_exit=0 if name=='positive' else 1,
                         stdout=run.stdout.decode(errors='replace'),stderr=run.stderr.decode(errors='replace'),
                         fixture_pins=[family.pin(cpp),family.pin(exe),*[family.pin(x) for x in directory.glob('*.hpp')]]))
    changed=[r['path'] for r in pins if family.pin(Path(r['path']))['sha256']!=r['sha256']]
    passed=not changed and all(r['exit']==r['expected_exit'] and ('PASS_TYPED_CONTINUATION' if r['name']=='positive' else 'CONTINUATION_DIFFERENCE') in r['stdout']+r['stderr'] for r in runs)
    result=dict(status='PASS_BOUNDED_TYPED_CONTINUATION_REFERENCE' if passed else 'FAIL',runs=runs,source_pins=pins,input_changes=changed,
                scope=['Exact captured four-effect/one-filter native pipeline, native span cursors and typed prepared data only.',
                       'All constructed scheduler callback scripts compare original guard order, budget/counts/accepted/writes at each event.',
                       'Actual low-budget semantic prefix is compared to the genuine before-preempt capture. Original callback RAM/CPU differences are recorded separately; no whitelist or callback implementation claim.',
                       'Actual resume entry is separately anchored; next stop callback is constructed.',
                       'Original compiled body is unchanged except test-only entry selection of its existing source resume label.'],
                open=['No runtime replacement, production task lifetime/cancellation/state-load integration or complete scheduler implementation.',
                      'No universal emitted-family, ownership/native credit, hardware/MMIO or generic VM/JIT claim.'])
    target=out/'RESULT.json';target.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8',newline='\n')
    print(json.dumps(dict(status=result['status'],result=str(target),sha256=family.pin(target)['sha256'])))
    return 0 if passed else 1


TEST=r'''
#include <functional>
#include <type_traits>
namespace sem=mka::emitted::semantic;
static_assert(!std::is_copy_constructible_v<sem::ClipContinuation>&&!std::is_move_constructible_v<sem::ClipContinuation>);
struct Event {unsigned consumed,accepted,remaining;int before,after;unsigned writes;bool suspended;bool operator==(const Event&)const=default;};
struct Schedule {unsigned total{},ordinal{},mode{};int actual_refill{};std::vector<Event> events;};
static Schedule* reference_schedule=nullptr;
static sem::GuardDecision decide(Schedule&s,sem::GuardObservation observation,int&budget){
 const auto before=budget;bool suspend=true;
 if(s.mode==1){budget=113;suspend=false;}else if(s.mode==2){suspend=(s.ordinal&1u)!=0;if(!suspend)budget=33;}
 else if(s.mode==3)budget=s.actual_refill;
 s.events.push_back({observation.consumed,observation.accepted,observation.remaining,before,budget,static_cast<unsigned>(writes.size()),suspend});++s.ordinal;
 return suspend?sem::GuardDecision::suspend:sem::GuardDecision::keep_running;
}
bool ps2::preempt(Cpu&c){++preempt_calls;if(!reference_schedule)throw std::runtime_error("test scheduler absent");
 const unsigned remaining=c.r[10].w[0]+1,consumed=reference_schedule->total-remaining;
 return decide(*reference_schedule,{consumed,c.r[2].w[0],remaining},c.budget)==sem::GuardDecision::suspend;}
static sem::GuardDecision native_guard(void*context,sem::GuardObservation observation,int&budget){return decide(*static_cast<Schedule*>(context),observation,budget);}
struct Reentry {sem::ClipContinuation* continuation;bool refused=false;};
static sem::GuardDecision reentry_guard(void*context,sem::GuardObservation,int&){auto&s=*static_cast<Reentry*>(context);try{s.continuation->run(1000);}catch(const std::exception&e){s.refused=std::string(e.what()).find("reentrant context")!=std::string::npos;}return sem::GuardDecision::suspend;}
static void file_read(const std::string&path,void*destination,std::size_t bytes){std::ifstream f(path,std::ios::binary);f.read(static_cast<char*>(destination),bytes);if(!f||f.peek()!=EOF)throw std::runtime_error("continuation input extent");}
static void project_vector(Vu0Regs&destination,const Vu0Regs&initial,const sem::Result&result){
 destination=initial;const auto&v=result.vector;std::memcpy(&destination.vf[4],v.constant_product.data(),16);
 if(v.element_observed){std::memcpy(&destination.vf[2],v.last_resource256.data(),16);std::memcpy(&destination.vf[3],v.last_resource257.data(),16);}
 destination.mac=v.last_mac;destination.status=v.project_status(initial.status);
}
static void assert_state(const sem::ClipContinuation&native,const Cpu&reference,const Cpu&initial,bool completed,bool check_pc=true,bool check_budget=true){
 Vu0Regs vector{};project_vector(vector,initial.vu0,native.result());
 if(std::memcmp(&vector,&reference.vu0,sizeof vector)||sem::no_yield::project_scalar(initial.fcr31,native.result().scalar)!=reference.fcr31||(check_budget&&native.budget()!=reference.budget)||native.result().accepted!=reference.r[2].w[0])throw std::runtime_error("typed scalar/vector/budget/return state");
 if(reference.r[10].sd[0]!=static_cast<std::int64_t>(native.remaining())-1)throw std::runtime_error("source count decrement ordering");
 if(reference.r[4].d[0]!=initial.r[4].d[0]+native.consumed()*initial.r[8].d[0]||reference.r[5].d[0]!=initial.r[5].d[0]+native.consumed()*initial.r[9].d[0]||reference.r[6].d[0]!=initial.r[6].d[0]+native.result().accepted*initial.r[8].d[0]||reference.r[7].d[0]!=initial.r[7].d[0]+native.result().accepted*initial.r[9].d[0])throw std::runtime_error("typed input/output cursors");
 if(check_pc){if(completed){if(reference.pc!=initial.r[31].w[0])throw std::runtime_error("original return continuation");}
 else if((reference.pc&~kUnwind)!=0x1c00030||!(reference.pc&kUnwind))throw std::runtime_error("original resume boundary");}
}
static unsigned cases=0,segments=0,events=0,snapshot_constants=0,refusals=0;
static void compare_case(Cpu initial,sem::Pipeline p,unsigned schedule_mode,bool change_constants=false){
 std::vector<unsigned char> reference_ram(g_ram,g_ram+kRamSize),native_ram=reference_ram;
 sem::ClipContinuation native(initial.r[10].w[0]?streams(initial):sem::Streams{},p);Cpu reference=initial;
 Schedule rs{initial.r[10].w[0],0,schedule_mode},ns=rs;
 int incoming=initial.budget;bool complete=false;
 for(unsigned segment=0;segment<=initial.r[10].w[0]+2;++segment){
  std::memcpy(g_ram,reference_ram.data(),kRamSize);writes.clear();rs.events.clear();reference_schedule=&rs;reference_resume=segment!=0;reference.budget=incoming;preempt_calls=0;reference_regenerated(reference);const auto trace=writes;std::memcpy(reference_ram.data(),g_ram,kRamSize);const bool expected_complete=reference.pc==initial.r[31].w[0];
  std::memcpy(g_ram,native_ram.data(),kRamSize);writes.clear();ns.events.clear();auto phase=native.run(incoming,{&ns,&native_guard},{nullptr,&observe});std::memcpy(native_ram.data(),g_ram,kRamSize);
  complete=phase==sem::ContinuationPhase::complete;
  if(complete!=expected_complete||reference_ram!=native_ram||trace!=writes||rs.events!=ns.events)throw std::runtime_error("original RAM/order/guard callback difference");
  assert_state(native,reference,initial,complete);++segments;events+=rs.events.size();
  if(complete)break;
  if(change_constants&&segment==0){
   // Both actual memory copies change; original resume keeps its prepared
   // values. The external descriptor object also changes after construction.
   for(unsigned i=0;i<9;++i){unsigned value=0xffc00001u;std::memcpy(reference_ram.data()+initial.r[11].w[0]+i*4,&value,4);std::memcpy(native_ram.data()+initial.r[11].w[0]+i*4,&value,4);}
   p.effects[1].constant.fill(0x7f7fffff);p.effects[2].constant.fill(0x7f7fffff);++snapshot_constants;
  }
  incoming=segment&1?14:33;
 }
 if(!complete)throw std::runtime_error("bounded typed continuation did not complete");
 try{native.run(1000);}catch(const std::exception&e){if(std::string(e.what()).find("completed context")!=std::string::npos){++refusals;++cases;return;}throw;}
 throw std::runtime_error("completed continuation resumed");
}
static sem::Pipeline load_pipeline(const Cpu&c){
 const auto object=c.r[19].w[0];for(unsigned i=0;i<4;++i){unsigned r=object+208+i*48;auto&e=captured[i];e=effect(ld16(r),ld32(r+8));if(e.id==5)for(unsigned lane=0;lane<3;++lane)e.constant[lane]=ld32(r+16+lane*4);if(e.id==6)e.constant[0]=ld32(r+16);if(e.id==17){e.source_tag=ld32(r+16);e.remap.start=ld32(r+24);e.remap.extent=ld32(r+28);e.remap.upper=ld32(r+32);e.remap.lower=ld32(r+36);}}
 captured[1].constant[3]=captured[2].constant[0];filter_limit=ld32(object+104);return pipeline("regenerated",c.f[12],c.r[11].w[0]);
}
int main(){try{
 Cpu original_seed{};read("Cpu.bin",&original_seed,sizeof original_seed);read("RAM.bin",g_ram,kRamSize);captured_cpu=original_seed;captured_ram.assign(g_ram,g_ram+kRamSize);original_object=original_seed.r[19].w[0];admitted=load_pipeline(original_seed);
 constexpr unsigned special[]={0u,0x80000000u,1u,0x7f7fffffu,0x7f800001u,0xffc00001u,0x3f800000u,0xc0200000u};
 constexpr unsigned status[]={0u,0x00800000u,0x00038070u,0xf0838073u};
 for(unsigned seed=0;seed<9;++seed)for(unsigned count:{0u,1u,2u,8u})for(unsigned alias:{0u,1u,3u})for(unsigned prior:status)for(unsigned schedule=0;schedule<3;++schedule){
  std::memcpy(g_ram,captured_ram.data(),kRamSize);auto p=admitted;p.step=special[seed%8];p.filter_limit=special[(seed+1)%8];for(unsigned lane=0;lane<3;++lane)p.effects[1].constant[lane]=special[(seed+lane+2)%8];p.effects[2].constant[0]=special[(seed+4)%8];p.effects[1].constant[3]=p.effects[2].constant[0];p.effects[3].remap.start=special[(seed+3)%8];p.effects[3].remap.extent=special[(seed+6)%8];
  if(seed==8){p.step=0;p.filter_limit=0;p.effects[1].constant={};p.effects[2].constant={};p.effects[3].remap.start=0xff7fffff;p.effects[3].remap.extent=0x7f7fffff;}
  if(build(p)!=61||std::memcmp(reachable_original_words,g_ram+0x1c00000,sizeof reachable_original_words))throw std::runtime_error("original numeric program identity");
  Cpu c{};c.r[4].d[0]=0x100000;c.r[5].d[0]=0x200000;c.r[6].d[0]=alias?0x100000+(alias==3?64:0):0x300000;c.r[7].d[0]=alias==1?0x200000:0x400000;c.r[8].d[0]=64;c.r[9].d[0]=16;c.r[10].d[0]=count;c.r[11].d[0]=0x1c00000+208;c.r[31].d[0]=0x2e6180;c.f[12]=c.vu0.vf[1].w[0]=p.step;c.f[4]=special[(seed+2)%8];c.fcr31=c.vu0.status=prior;c.vu0.mac=0xf1234567;c.budget=schedule==0?14:schedule==1?0:33;
  for(unsigned lane=0;lane<4;++lane){c.vu0.vf[2].w[lane]=special[(seed+lane)%8];c.vu0.vf[3].w[lane]=special[(seed+lane+1)%8];c.vu0.acc.w[lane]=special[(seed+lane+2)%8];}
  for(unsigned i=0;i<8;++i){for(unsigned j=0;j<12;++j)put(c.r[4].w[0]+i*64+j*4,special[(i+j+seed)%8]);put(c.r[5].w[0]+i*16,seed==8?0x7f7fffff:special[(i+seed+3)%8]);if(seed==8)put(c.r[4].w[0]+i*64+12,0);}
  compare_case(c,p,schedule,schedule==0&&count>1);
 }
 auto refused=[&](auto action,const char*reason){try{action();}catch(const std::exception&e){if(std::string(e.what()).find(reason)!=std::string::npos){++refusals;return;}throw;}throw std::runtime_error("required continuation refusal absent");};
 std::memcpy(g_ram,captured_ram.data(),kRamSize);writes.clear();sem::Streams empty{};sem::ClipContinuation bad_budget(empty,admitted);refused([&]{bad_budget.run(-2147483647-1);},"budget arithmetic domain");
 auto one=streams(original_seed);one.count=1;sem::ClipContinuation unbound(one,admitted);refused([&]{unbound.run(0);},"scheduler unbound");
 auto invalid=admitted;invalid.filter_count=2;refused([&]{sem::ClipContinuation rejected(one,invalid);},"filter cardinality");
 auto bad_count=one;bad_count.count=0x80000000;refused([&]{sem::ClipContinuation rejected(bad_count,admitted);},"count/stride/alignment");
 auto bad_extent=one;bad_extent.input_a=bad_extent.input_a.first(47);refused([&]{sem::ClipContinuation rejected(bad_extent,admitted);},"stream extent");
 auto bad_stride=one;bad_stride.stride_a=49;refused([&]{sem::ClipContinuation rejected(bad_stride,admitted);},"count/stride/alignment");
 sem::ClipContinuation bad_decision(one,admitted);refused([&]{bad_decision.run(0,{nullptr,[](void*,sem::GuardObservation,int&){return static_cast<sem::GuardDecision>(9);}});},"scheduler decision unproved");
 sem::ClipContinuation reentrant(one,admitted);Reentry recursion{&reentrant};if(reentrant.run(0,{&recursion,&reentry_guard})!=sem::ContinuationPhase::suspended||!recursion.refused)throw std::runtime_error("reentry guard failed");++refusals;
 if(!writes.empty())throw std::runtime_error("admission refusal wrote stream data");
 std::printf("PASS_CONSTRUCTED_TYPED_CONTINUATION whole_calls=%u boundaries=%u guard_events=%u snapshotted_constant_mutations=%u named_refusals=%u\n",cases,segments,events,snapshot_constants,refusals);
 // Actual original entry/stop. Only the observed callback result (refilled
 // budget and suspension) is supplied; no scheduler implementation is claimed.
 Cpu actual_before{},actual_after{};file_read("@ACTUAL@-before-Cpu.bin",&actual_before,sizeof actual_before);file_read("@ACTUAL@-after-Cpu.bin",&actual_after,sizeof actual_after);file_read("@ACTUAL@-before-RAM.bin",g_ram,kRamSize);
 auto actual_pipeline=load_pipeline(actual_before);const auto object=actual_before.r[19].w[0],base=actual_before.r[12].w[0],sample_object=captured_cpu.r[19].w[0],sample_base=captured_cpu.r[12].w[0];
 if(std::memcmp(g_ram+base,captured_ram.data()+sample_base,244)||std::memcmp(g_ram+object+208,captured_ram.data()+sample_object+208,192)||std::memcmp(g_ram+object+84,captured_ram.data()+sample_object+84,24))throw std::runtime_error("actual original-builder descriptor/code identity");
 std::vector<unsigned char> initial_actual(g_ram,g_ram+kRamSize),observed(kRamSize),callback_ram(kRamSize);
 Cpu event_before{},event_after{};file_read("@PREEMPT@-preempt-before-Cpu.bin",&event_before,sizeof event_before);file_read("@PREEMPT@-preempt-after-Cpu.bin",&event_after,sizeof event_after);file_read("@PREEMPT@-preempt-before-RAM.bin",observed.data(),kRamSize);file_read("@PREEMPT@-preempt-after-RAM.bin",callback_ram.data(),kRamSize);bool event_result{};file_read("@PREEMPT@-preempt-result.bin",&event_result,1);
 Cpu original_actual=actual_before;Schedule original_schedule{80,0,0};reference_schedule=&original_schedule;reference_resume=false;writes.clear();reference_regenerated(original_actual);const auto original_trace=writes;
 if(std::memcmp(observed.data(),g_ram,kRamSize)||original_actual.budget!=event_before.budget)throw std::runtime_error("actual pre-callback original prefix join");
 auto joined_original=original_actual;joined_original.pc=event_before.pc;joined_original.base=event_before.base;if(std::memcmp(&joined_original,&event_before,sizeof event_before))throw std::runtime_error("actual pre-callback full original CPU join");
 std::memcpy(g_ram,initial_actual.data(),kRamSize);sem::ClipContinuation actual_native(streams(actual_before),actual_pipeline);Schedule actual_schedule{actual_before.r[10].w[0],0,3,event_after.budget};writes.clear();auto phase=actual_native.run(actual_before.budget,{&actual_schedule,&native_guard},{nullptr,&observe});auto actual_trace=writes;
 if(phase!=sem::ContinuationPhase::suspended||actual_native.consumed()!=56||actual_native.result().accepted!=56||actual_trace.size()!=392||actual_trace!=original_trace||std::memcmp(observed.data(),g_ram,kRamSize)||actual_schedule.events.size()!=1||actual_schedule.events[0].before!=-25||actual_schedule.events[0].before!=event_before.budget||!event_result)throw std::runtime_error("actual low-budget prefix transport");
 assert_state(actual_native,event_before,actual_before,false,false,false);
 auto retained_event=event_after;retained_event.budget=event_before.budget;if(std::memcmp(&retained_event,&event_before,sizeof event_before))throw std::runtime_error("original event changed retained CPU beyond budget");
 std::vector<unsigned char> after_ram(kRamSize);file_read("@ACTUAL@-after-RAM.bin",after_ram.data(),kRamSize);if(after_ram!=callback_ram)throw std::runtime_error("original return changed post-event RAM");
 auto returned_event=event_after;returned_event.pc=(base+0x30)|kUnwind;if(std::memcmp(&returned_event,&actual_after,sizeof actual_after))throw std::runtime_error("original post-event unwind continuation");
 unsigned event_changed_bytes=0;for(unsigned i=0;i<kRamSize;++i)event_changed_bytes+=observed[i]!=callback_ram[i];
 // Genuine natural resume input. Its next stopping callback is constructed,
 // as the next physical callback outcome was not captured by this observer.
 Cpu resume{};file_read("@RESUME@/Cpu-resume.bin",&resume,sizeof resume);file_read("@RESUME@/RAM-resume.bin",g_ram,kRamSize);if(resume.pc!=base+0x30)throw std::runtime_error("actual original resume entry");assert_state(actual_native,resume,actual_before,false,false,false);
 std::vector<unsigned char> resume_ram(g_ram,g_ram+kRamSize);Cpu ref=resume;Schedule rs{80,0,0},ns=rs;reference_schedule=&rs;reference_resume=true;writes.clear();reference_regenerated(ref);auto resumed_trace=writes;std::memcpy(observed.data(),g_ram,kRamSize);
 std::memcpy(g_ram,resume_ram.data(),kRamSize);writes.clear();phase=actual_native.run(resume.budget,{&ns,&native_guard},{nullptr,&observe});
 if(phase!=sem::ContinuationPhase::suspended||actual_native.consumed()!=57||writes.size()!=7||resumed_trace!=writes||rs.events!=ns.events||std::memcmp(observed.data(),g_ram,kRamSize))throw std::runtime_error("actual resume entry next constructed stop");assert_state(actual_native,ref,actual_before,false);
 std::printf("PASS_TYPED_CONTINUATION constructed_whole_calls=%u stop_or_return_RAM_order_VU_FCR_cursor_comparisons=%u guard_events=%u snapshotted_constant_mutations=%u named_refusals=%u actual_prefix_elements=56 actual_prefix_ordered_writes=392 actual_callback_changed_RAM_bytes=%u actual_resume_entry_next_constructed_stop_writes=7 actual_scheduler_implementation=OPEN native_runtime_adoption=NONE\n",cases,segments,events,snapshot_constants,refusals,event_changed_bytes);return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"CONTINUATION_DIFFERENCE %s\n",e.what());return 1;}}
'''


if __name__=='__main__':raise SystemExit(main())
