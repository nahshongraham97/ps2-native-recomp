"""Prove the bounded timing/FCR adapter against original compiled programs.

No candidate/launcher mutation. Source mutants are separate ignored files.
The admitted adapter is no-yield only; low-budget continuation stays OPEN.
"""
from pathlib import Path
import argparse
import json
import re
import subprocess
import struct
import sys

sys.path.insert(0,str(Path(__file__).resolve().parent))
import specialize_r192_stub_family as family


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['pipeline-proof','caller-proof','boundary','candidate-build','compiler','output']:
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();out=a.output.resolve()
    family.require(out.is_relative_to((family.W/'build/oracle-telemetry').resolve()) and not out.exists(),'fresh_owned_output_required')
    expected=[(a.pipeline_proof,'e4405690c5b7e8e98764afdcc68e62b1f8a9e3958b363bd90a7b28a9c9047089'),
              (a.caller_proof,'fdc7c4c115fdbd53648d02fc0c76336f8884dc9f08e5321641b93e9a96746827')]
    pinned=[];texts=[]
    for folder,sha in expected:
        receipt=folder/'RESULT.json';family.require(family.pin(receipt)['sha256']==sha,'frozen_reference_receipt_changed')
        record=json.loads(receipt.read_text());source=Path(record['fixture_pins'][0]['path'])
        family.require(family.pin(source)['sha256']==record['fixture_pins'][0]['sha256'],'original_reference_fixture_changed')
        pinned += [receipt,source];texts.append(source.read_bytes().decode().replace('\r\n','\n'))
    core=family.W/'tools/research/r192_aot_modules/semantic_pipeline.hpp'
    adapter=core.with_name('semantic_no_yield_adapter.hpp')
    vector=core.with_name('semantic_vector_effects.hpp')
    prefix=texts[0][:texts[0].index('\nint main(')]
    prefix,n=re.subn(r'#include "[^"\n]*semantic_pipeline\.hpp"','@ADAPTER_INCLUDE@',prefix)
    family.require(n==1,'native_core_include_seam_changed')
    caller=texts[1][texts[1].index('static typed::Pipeline admitted;'):]
    old='auto result=typed::run(streams(c),admitted,{nullptr,&observe});set32(c.r[2],result.accepted);c.pc=0x2e6180;'
    new='auto result=typed::no_yield::run(streams(c),admitted,c.budget,c.fcr31,{nullptr,&observe});set32(c.r[2],result.pipeline.accepted);c.budget=result.remaining_budget;c.fcr31=result.scalar_control_status;c.pc=0x2e6180;'
    family.require(caller.count(old)==1,'typed_caller_projection_seam_changed');caller=caller.replace(old,new)
    caller=caller.replace('int main(int argc,char**argv)','static int connected_caller(int argc,char**argv)')
    caller=caller.replace('constexpr unsigned original_scalar_status_writes=0x8010u|kFpuCondition;','constexpr unsigned original_scalar_status_writes=0u;')
    caller=caller.replace('good&=((candidate.fcr31^reference.fcr31)&~original_scalar_status_writes)==0;',
                          'good&=((candidate.fcr31^reference.fcr31)&~original_scalar_status_writes)==0;good&=candidate.budget==reference.budget;')
    caller=caller.replace('scheduler_adoption=UNPROVED','no_yield_timing_scalar_projection=PROVED low_budget_continuation=OPEN')
    literal=re.search(r'constexpr unsigned program\[\]=\{([^}]+)\};',caller)
    family.require(literal is not None,'original_caller_program_identity_missing')
    original_words=[int(word.strip().removesuffix('u'),16) for word in literal[1].split(',')]
    family.require(len(original_words)==61,'original_program_extent_changed')
    original_body=texts[0].split('static void reference_regenerated(Cpu&c){',1)[1].split('static void reference_identity',1)[0]
    budget_blocks=[int(n) for n in re.findall(r'c\.budget -= (\d+);',original_body)]
    family.require(budget_blocks==[12,16,16,2,2] and original_body.count('c.budget < 0 && preempt(c)')==1,
                   'original_timing_or_strict_guard_changed')
    identity='\nstatic constexpr unsigned reachable_original_words[]={'+','.join(hex(w)+'u' for w in original_words[:48])+'};\n'
    source=prefix+caller+identity+TEST
    inputs=[Path(__file__),core,adapter,vector,a.compiler,*pinned,*sorted(a.boundary.glob('*.bin')),
            *sorted((a.candidate_build/'src/runtime/include/ps2').glob('*.hpp'))]
    # Generated fixture contains immutable paths to the complete original
    # builder programs. Authenticate those source artifacts as dependencies.
    programs=json.loads((a.pipeline_proof/'RESULT.json').read_text())['source_pins']
    inputs += [Path(r['path']) for r in programs if Path(r['path']).name.endswith('.bin') and 'emitted-builder-actual-descriptor-06' in r['path']]
    original_binary=next(p for p in inputs if p.name=='regenerated.bin')
    family.require(original_binary.read_bytes()[:192]==struct.pack('<48I',*original_words[:48]),'original_body_fixture_join_failed')
    pins=[family.pin(p) for p in dict.fromkeys(inputs)]
    out.mkdir(parents=True);runs=[]
    variants=[('positive',None,None),
      ('planted-cost','16u+18u*count+16u*accepted','16u+18u*count+15u*accepted'),
      ('planted-equality','std::uint64_t(budget)<worst','std::uint64_t(budget)<=worst'),
      ('planted-overflow','if(count>(std::numeric_limits<std::uint64_t>::max()-16u)/34u)refuse("no-yield adapter: worst-cost arithmetic overflow");',''),
      ('planted-condition','if(effects.condition_written)','if(false&&effects.condition_written)'),
      ('planted-sticky','Bits result=original|effects.raised_flags;','Bits result=effects.raised_flags;')]
    for name,before,after in variants:
        directory=out/name;directory.mkdir();header=adapter.read_text()
        if before is not None:
            family.require(header.count(before)==1,'source_mutation_anchor_changed:'+name);header=header.replace(before,after)
        (directory/adapter.name).write_text(header,encoding='utf8',newline='\n');(directory/core.name).write_bytes(core.read_bytes())
        (directory/vector.name).write_bytes(vector.read_bytes())
        cpp=directory/'test.cpp';cpp.write_text(source.replace('@ADAPTER_INCLUDE@','#include "'+(directory/adapter.name).as_posix()+'"'),encoding='utf8',newline='\n');exe=directory/'test.exe'
        command=[str(a.compiler),'-std=c++20','-mavx2','-ffp-contract=off','-O1','-static-libgcc','-static-libstdc++',
                 '-I'+str((a.candidate_build/'src/runtime/include').resolve()),str(cpp),'-o',str(exe)]
        cp=subprocess.run(command,capture_output=True);(directory/'compile.log').write_bytes(cp.stdout+cp.stderr)
        family.require(cp.returncode==0,'adapter_fixture_compile_failed:'+name)
        run=subprocess.run([str(exe)],capture_output=True);(directory/'test.log').write_bytes(run.stdout+run.stderr)
        runs.append(dict(name=name,compile_command=command,argv=[str(exe)],exit=run.returncode,expected_exit=0 if name=='positive' else 1,
                         stdout=run.stdout.decode(errors='replace'),stderr=run.stderr.decode(errors='replace'),
                         mutated_anchor=before,source_pins=[family.pin(directory/adapter.name),family.pin(directory/core.name),family.pin(directory/vector.name),family.pin(cpp),family.pin(exe)]))
    changed=[r['path'] for r in pins if family.pin(Path(r['path']))['sha256']!=r['sha256']]
    passed=not changed and all(r['exit']==r['expected_exit'] for r in runs)
    result=dict(status='PASS_BOUNDED_NO_YIELD_TIMING_AND_SCALAR_STATUS' if passed else 'FAIL',runs=runs,source_pins=pins,input_changes=changed,
                contract={'family':'Exactly[1/tag769,5/tag256,6/tag258,17/tag257<-769] with exactlyone filter2/tag769, original constant-block adjacency.',
                          'admission':'uint64 checked worst16+34*N <= INT32_MAX and incomingbudget >= worst; prior descriptor/span gates unchanged.',
                          'charged':'16+18*N+16*accepted; checked accepted<=N; remainingbudget equals original.',
                          'scalar_status':'Literal FCR bits: original | raisedoverflow8010; last original CMP condition replaces00800000 only if count>0. Original-builder DIV flags excluded.',
                          'original_timing_evidence':{'compiled_reference_blocks':budget_blocks,'strict_guard':'budget < 0','reachable_words':48,'code_block_words':52,'complete_program_words':61,'unreachable_padding_not_semantic_evidence':True},
                          'scope':'Complete no-yield calls only. Equality checked against original strict budget<0 guards. Countmax/uint64overflow tests are arithmetic admission controls, not huge memory executions.'},
                open=['Low-budget continuation, actual events/refill/unwind scheduling and typed resumable state.',
                      'VU MAC/status/accumulator and volatile-value consumer analysis are not closed by FCR proof.',
                      'No candidate, normal launcher, protected input, ownership or credit changes.'])
    target=out/'RESULT.json';target.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8',newline='\n');print(json.dumps(dict(status=result['status'],result=str(target),sha256=family.pin(target)['sha256'])))
    return 0 if passed else 1


TEST=r'''
static bool expects_refusal(auto action,const char*reason){try{action();}catch(const std::exception&e){return std::string(e.what()).find(reason)!=std::string::npos;}return false;}
int main(int argc,char**argv){try{
 if(connected_caller(argc,argv))return 1;
 using namespace typed::no_yield;unsigned arithmetic_controls=0,comparisons=0,refusals=0,rejected_remap_controls=0;
 auto rejected=[&](auto action,const char*reason){if(!expects_refusal(action,reason))throw std::runtime_error(std::string("ARITHMETIC_CONTROL_DIFFERENCE ")+reason);++arithmetic_controls;};
 if(admit(0,16).worst_cost!=16||actual_cost(0,0)!=16)throw std::runtime_error("zero cost");++arithmetic_controls;
 rejected([&]{admit(0,15);},"insufficient worst-case budget");
 rejected([&]{admit(1,-1);},"insufficient worst-case budget");
 rejected([&]{admit(0x7fffffffull,0x7fffffff);},"worst cost exceeds signed budget domain");
 rejected([&]{admit(0x80000000ull,0x7fffffff);},"source count domain");
 rejected([&]{admit(0xffffffffffffffffull,0x7fffffff);},"worst-cost arithmetic overflow");
 rejected([&]{actual_cost(1,2);},"accepted count exceeds");
 rejected([&]{actual_cost(0xffffffffffffffffull,0);},"exact-cost arithmetic overflow");
 const auto maximum=(0x7fffffffull-16)/34;auto maximum_cost=16+34*maximum;
 if(admit(maximum,static_cast<int>(maximum_cost)).worst_cost!=maximum_cost||actual_cost(maximum,maximum)!=maximum_cost)throw std::runtime_error("maximum cost boundary");++arithmetic_controls;
 rejected([&]{admit(maximum+1,0x7fffffff);},"worst cost exceeds signed budget domain");
 rejected([&]{project_scalar(0,{0x10020,false,false});},"unproved scalar status bits");
 Cpu original_seed{};read("Cpu.bin",&original_seed,sizeof original_seed);read("RAM.bin",g_ram,kRamSize);captured_cpu=original_seed;captured_ram.assign(g_ram,g_ram+kRamSize);
 constexpr unsigned special[]={0u,0x80000000u,1u,0x7f7fffffu,0x7f800001u,0xffc00001u,0x3f800000u,0xc0200000u};
 constexpr unsigned prior_status[]={0u,0x00800000u,0x00038070u,0xf0838073u};
 for(unsigned seed=0;seed<9;++seed)for(unsigned count:{0u,1u,2u,8u})for(unsigned alias:{0u,1u,3u})for(unsigned initial:prior_status){
  std::memcpy(g_ram,captured_ram.data(),kRamSize);auto p=admitted;p.step=special[seed%8];p.filter_limit=special[(seed+1)%8];
  for(unsigned lane=0;lane<3;++lane)p.effects[1].constant[lane]=special[(seed+lane+2)%8];
  p.effects[2].constant[0]=special[(seed+4)%8];p.effects[1].constant[3]=p.effects[2].constant[0];
  p.effects[3].remap.upper=seed&1?-997:255;p.effects[3].remap.lower=seed&2?-23:17;p.effects[3].remap.start=special[(seed+3)%8];p.effects[3].remap.extent=special[(seed+6)%8];
  if(seed==8){p.step=0;p.filter_limit=0;p.effects[1].constant={};p.effects[2].constant={};p.effects[3].remap.start=0xff7fffffu;p.effects[3].remap.extent=0x7f7fffffu;}
  auto words=build(p);if(words!=61||ld32(original_object+0x394)!=52)throw std::runtime_error("numeric family extent");
  if(std::memcmp(reachable_original_words,g_ram+0x1c00000,sizeof reachable_original_words))throw std::runtime_error("numeric descriptor changed original reachable instructions");
  Cpu c{};c.r[4].d[0]=0x100000;c.r[5].d[0]=0x200000;c.r[6].d[0]=alias?0x100000+(alias==3?64:0):0x300000;c.r[7].d[0]=alias==1?0x200000:0x400000;
  c.r[8].d[0]=64;c.r[9].d[0]=16;c.r[10].d[0]=count;c.r[11].d[0]=0x1c00000+208;c.r[31].d[0]=0x100000;c.f[12]=c.vu0.vf[1].w[0]=p.step;c.vu0.vf[0].w[3]=0x3f800000;c.fcr31=initial;c.budget=static_cast<int>(16+34*count);
  for(unsigned i=0;i<8;++i){for(unsigned j=0;j<12;++j)put(c.r[4].w[0]+i*64+j*4,special[(i+j+seed)%8]);put(c.r[5].w[0]+i*16,special[(i+seed+3)%8]);}
  if(seed==8)for(unsigned i=0;i<8;++i){put(c.r[4].w[0]+i*64+12,0);put(c.r[5].w[0]+i*16,0x7f7fffffu);}
  std::vector<unsigned char> before(g_ram,g_ram+kRamSize),expected(kRamSize);Cpu reference=c;writes.clear();preempt_calls=0;yield_requested=false;reference_regenerated(reference);auto trace=writes;auto calls=preempt_calls;std::memcpy(expected.data(),g_ram,kRamSize);
  std::memcpy(g_ram,before.data(),kRamSize);writes.clear();auto input=count?streams(c):typed::Streams{};input.count=count;
  auto native=run(input,p,c.budget,c.fcr31,{nullptr,&observe});
  if(native.pipeline.accepted!=reference.r[2].w[0]||native.remaining_budget!=reference.budget||native.scalar_control_status!=reference.fcr31||calls||std::memcmp(expected.data(),g_ram,kRamSize)||trace!=writes){
   std::fprintf(stderr,"ADAPTER_DIFFERENCE seed=%u count=%u alias=%u status=%08X/%08X budget=%d/%d writes=%zu/%zu\n",seed,count,alias,native.scalar_control_status,reference.fcr31,native.remaining_budget,reference.budget,writes.size(),trace.size());return 1;}
  ++comparisons;
  if(seed==8&&count){if(native.pipeline.accepted||!writes.empty()||native.scalar_control_status!=(initial&~0x00800000u))throw std::runtime_error("rejected remap incorrectly raised flags");++rejected_remap_controls;}
  writes.clear();if(!expects_refusal([&]{run(input,p,c.budget-1,c.fcr31,{nullptr,&observe});},"insufficient worst-case budget")||!writes.empty())throw std::runtime_error("budget refusal after writes");++refusals;
 }
 // Actual original strict guard witnesses a low-budget yield before the first
 // stream access. This is a refusal control, NOT a solved continuation.
 std::memcpy(g_ram,captured_ram.data(),kRamSize);Cpu low=original_seed;low.r[10].d[0]=1;low.budget=13;writes.clear();preempt_calls=0;yield_requested=true;reference_regenerated(low);
 if(preempt_calls!=1||!(low.pc&kUnwind)||!writes.empty())throw std::runtime_error("original low-budget guard witness failed");
 std::printf("PASS no_yield_adapter original_equal_budget_literal_FCR_RAM_order_comparisons=%u arithmetic_boundary_controls=%u prewrite_insufficient_budget_refusals=%u rejected_remap_overflow_controls=%u original_low_budget_yield_control=1 connected_actual_frame_seed=1 low_budget_continuation=OPEN VU_status_adoption=OPEN\n",comparisons,arithmetic_controls,refusals,rejected_remap_controls);return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"ADAPTER_DIFFERENCE %s\n",e.what());return 1;}}
'''

if __name__=='__main__':raise SystemExit(main())
