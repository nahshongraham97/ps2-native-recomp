"""Whole-pipeline native typed-data vs compiled-original differential tests.

Uses frozen, complete original-builder programs as references. No ISA interpreter
or runtime compiler is installed; no candidate or protected input is modified.
"""
from pathlib import Path
import argparse
import json
import subprocess
import sys
import struct

sys.path.insert(0,str(Path(__file__).resolve().parent))
import specialize_r192_stub_family as family


def reachable_words(raw, code_words):
    """Offline original-CFG authentication, not an accepted runtime decoder."""
    words=struct.unpack_from('<'+'I'*code_words,raw);todo=[0];seen=set()
    while todo:
        index=todo.pop()
        family.require(0<=index<code_words,'reference_CFG_outside_code_extent')
        if index in seen:continue
        seen.add(index);word=words[index];op=word>>26;rs=(word>>21)&31;rt=(word>>16)&31
        ret=op==0 and (word&63)==8
        branch=op in (4,5,20,21) or (op==17 and rs==8)
        family.require(op not in (1,2,3,6,7,22,23) and not(op==0 and(word&63)==9),'unsupported_reference_control')
        if ret or branch:
            family.require(index+1<code_words,'reference_delay_missing');seen.add(index+1)
            delay=words[index+1]
            family.require(delay>>26 not in (1,2,3,4,5,6,7,20,21,22,23) and not(delay>>26==0 and(delay&63) in (8,9)),'reference_control_in_delay')
        if ret:
            family.require(rs==31,'reference_nonRA_return');continue
        if branch:
            immediate=word&65535;immediate-=65536 if immediate&32768 else 0
            target=index+1+immediate
            if op in (4,20) and rs==rt:todo.append(target)
            elif op in (5,21) and rs==rt:todo.append(index+2)
            else:todo.extend([index+2,target])
        else:todo.append(index+1)
    return sorted(seen)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['references','boundary','candidate-build','compiler','output']:
        p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();out=a.output.resolve()
    family.require(out.is_relative_to((family.W/'build/oracle-telemetry').resolve()) and not out.exists(),'fresh_owned_output_required')
    control=struct.pack('<5I',0x10000002,0,0xffffffff,0x03e00008,0)
    family.require(reachable_words(control,5)==[0,1,3,4],'original_CFG_dead_padding_control_failed')
    for bad in [struct.pack('<2I',0x10007fff,0),struct.pack('<2I',0x00400008,0)]:
        try:reachable_words(bad,len(bad)//4)
        except ValueError:pass
        else:raise ValueError('original_CFG_unproved_exit_accepted')
    proof=a.references/'RESULT.json'
    family.require(family.pin(proof)['sha256']=='a7b1464c1c472611f3b537471473f6831af268cb6fcf1c6c4fb05835e4c29275','compiled_reference_proof_changed')
    record=json.loads(proof.read_text());source=a.references/'reference_programs.cpp'
    family.require(family.pin(source)['sha256']==record['fixture_pins'][0]['sha256'],'compiled_original_source_changed')
    text=source.read_bytes().decode().replace('\r\n','\n');prefix=text.split('\n#include <fstream>\nstruct Program',1)[0]
    backend=family.W/'tools/research/r192_aot_modules/semantic_pipeline.hpp'
    cfg=[]
    for r in record['programs']:
        family.require(r['code_words']<=64,'reference_CFG_mask_bound')
        selected=reachable_words(Path(r['program_pin']['path']).read_bytes(),r['code_words'])
        cfg.append(dict(name=r['name'],executed_word_indices=selected,nonreachable_code_block_word_indices=sorted(set(range(r['code_words']))-set(selected))))
    rows=','.join('{"'+r['name']+'",'+str(r['words'])+','+str(r['code_words'])+',0x'+format(sum(1<<i for i in c['executed_word_indices']),'x')+'ull,&reference_'+r['name']+'}' for r,c in zip(record['programs'],cfg))
    programs=Path(record['programs'][0]['program_pin']['path']).parent
    builder_receipt=programs/'RESULT.json';builder_source=programs/'original_builder.cpp'
    family.require(family.pin(builder_receipt)['sha256']=='ffa154fa526bd0af42c3404f9454ee45a28a196b9936f82281fc61fa062a603c','original_builder_receipt_changed')
    builder_record=json.loads(builder_receipt.read_text())
    family.require(any(Path(r['path']).resolve()==builder_source.resolve() and family.pin(builder_source)['sha256']==r['sha256'] for r in builder_record['source_pins']),'original_builder_source_changed')
    body=builder_source.read_bytes().decode().replace('\r\n','\n');body=body[body.index('static std::map<unsigned,unsigned> entries_observed;'):body.index('static void read(')]
    harness=prefix+'\n#include <fstream>\n#include <map>\n#include "'+backend.as_posix()+'"\n'+SLOW+body+TEST.replace('@ROWS@',rows).replace('@PROGRAMS@',programs.as_posix()).replace('@BOUNDARY@',a.boundary.resolve().as_posix())
    inputs=[Path(__file__),Path(family.__file__),backend,proof,source,builder_receipt,builder_source,a.compiler,*sorted(a.boundary.glob('*.bin')),
            *(Path(r['program_pin']['path']) for r in record['programs']),
            *sorted((a.candidate_build/'src/runtime/include/ps2').glob('*.hpp'))]
    pins=[family.pin(x) for x in inputs];out.mkdir(parents=True)
    cpp=out/'semantic_differential.cpp';cpp.write_text(harness,encoding='utf8',newline='\n');exe=out/'semantic_differential.exe'
    command=[str(a.compiler),'-std=c++20','-mavx2','-ffp-contract=off','-O2','-static-libgcc','-static-libstdc++',
             '-I'+str((a.candidate_build/'src/runtime/include').resolve()),str(cpp),'-o',str(exe)]
    cp=subprocess.run(command,capture_output=True);(out/'compile.log').write_bytes(cp.stdout+cp.stderr)
    family.require(cp.returncode==0,'typed_pipeline_fixture_compile_failed')
    runs=[]
    for name,args,expected,reason in [('positive',[],0,'PASS typed_pipeline'),
          ('planted-value',['--planted-value'],1,'PIPELINE_DIFFERENCE'),
          ('planted-order',['--planted-order'],1,'PIPELINE_DIFFERENCE'),
          ('planted-transient',['--planted-transient'],1,'PIPELINE_DIFFERENCE'),
          ('planted-alias-snapshot',['--planted-alias-snapshot'],1,'PIPELINE_DIFFERENCE')]:
        run=subprocess.run([str(exe),*args],capture_output=True);(out/(name+'.log')).write_bytes(run.stdout+run.stderr)
        runs.append(dict(name=name,argv=[str(exe),*args],exit=run.returncode,expected_exit=expected,expected_reason=reason,
                         stdout=run.stdout.decode(errors='replace'),stderr=run.stderr.decode(errors='replace')))
    changes=[r['path'] for r in pins if family.pin(Path(r['path']))['sha256']!=r['sha256']]
    passed=not changes and all(r['exit']==r['expected_exit'] and r['expected_reason'] in r['stdout']+r['stderr'] for r in runs)
    result=dict(status='PASS_BOUNDED_TYPED_WHOLE_PIPELINE' if passed else 'FAIL',compile_command=command,runs=runs,source_pins=pins,original_reference_CFG=cfg,
                original_CFG_controls=['constant_branch_prunes_dead_word','out_of_extent_branch_refused','nonRA_indirect_return_refused'],
                fixture_pins=[family.pin(cpp),family.pin(exe)],input_changes=changes,
                supported_domain={'resources':[[256,0,0,16],[259,0,16,4],[257,0,32,16],[258,0,12,4],[769,1,0,4]],
                                  'effects':'Exact15reference configurations; fixed tag bindings in backend; no arbitrary composition admission.',
                                  'filters':'Only0 or1; the singlefilter requires exact[1,5,6,17] and ID2/tag769.',
                                  'constant_tail':'Explicit raw fourthword of original128-bit constant fetch; no inferred zero padding.',
                                  'numeric_producer':'Actual compiled original2ED4B0 regenerates data constants; every source-reachable instruction and delayword must remain identical. Nonreachable alignment padding is explicitly excluded from instruction identity and retains owned0x95allocation bytes.',
                                  'zero_count':'All15original programs perform no stream access when count0; native empty spans return0 after descriptor admission. CPU constant-prelude flags/timing remain outside this API.',
                                  'numeric':'Original EE/VU bit semantics via pinned numerical helpers; no hostfloat substitution.',
                                  'memory':'Native spans, all accesses validated first; causal per-element reads and original ordered transient writes; tested aliases.'},
                limits=['No runtime or production integration, scheduler/yield bridge, guest Cpu projection or native credit.',
                        'Whole output bytes/order/return-count differential is separate from whole Cpu or actual scheduler equivalence.',
                        'Logical formats are source tags and explicit storage widths, not invented game classes or inferred semantic names.',
                        'The complete original-builder references establish bounded configurations; they do not establish universal constructor closure.'])
    dest=out/'RESULT.json';dest.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8',newline='\n')
    print(json.dumps(dict(status=result['status'],result=str(dest),sha256=family.pin(dest)['sha256'])))
    return 0 if passed else 1


SLOW=r'''
namespace ps2{
std::uint8_t slow_ld8(std::uint32_t){throw std::runtime_error("non-RAM original builder ld8");}
std::uint16_t slow_ld16(std::uint32_t){throw std::runtime_error("non-RAM original builder ld16");}
std::uint64_t slow_ld64(std::uint32_t){throw std::runtime_error("non-RAM original builder ld64");}
void slow_st8(std::uint32_t,std::uint8_t){throw std::runtime_error("non-RAM original builder st8");}
void slow_st16(std::uint32_t,std::uint16_t){throw std::runtime_error("non-RAM original builder st16");}
void slow_st64(std::uint32_t,std::uint64_t){throw std::runtime_error("non-RAM original builder st64");}
}
'''

TEST=r'''
namespace typed=mka::emitted::semantic;
struct Program{const char*name;unsigned words,code_words;std::uint64_t executed_words;void(*run)(Cpu&);};
static Program programs[]={@ROWS@};
static std::string mode;
static void put(unsigned a,unsigned v){std::memcpy(g_ram+a,&v,4);}
static unsigned original_object;
static std::array<typed::Effect,4> captured;
static unsigned filter_limit;
static std::vector<unsigned char> captured_ram;
static Cpu captured_cpu;
static void read(const char*name,void*data,std::size_t size){std::ifstream f(std::string("@BOUNDARY@/")+name,std::ios::binary);f.read(static_cast<char*>(data),size);if(!f||f.peek()!=EOF)throw std::runtime_error("captured boundary extent");}
static typed::Effect effect(unsigned id,unsigned destination,unsigned source=0){typed::Effect e;e.id=id;e.destination_tag=destination;e.source_tag=source;return e;}
static typed::Pipeline pipeline(const std::string&name,unsigned step,unsigned constant_base){
 typed::Pipeline p;p.step=step;
 if(name=="regenerated"){p.effect_count=4;std::copy(captured.begin(),captured.end(),p.effects.begin());p.filter_count=1;p.filter_id=2;p.filter_tag=769;p.filter_limit=filter_limit;return p;}
 if(name=="identity")return p;
 p.effect_count=1;
 if(name=="effect1")p.effects[0]=effect(1,769);
 else if(name=="effect2")p.effects[0]=effect(2,256,256);
 else if(name=="effect3")p.effects[0]=effect(3,769,258);
 else if(name=="effect25")p.effects[0]=effect(25,769,258);
 else if(name=="effect5"||name=="effect26_vector"){p.effects[0]=captured[1];p.effects[0].id=name=="effect5"?5:26;p.effects[0].constant[3]=ld32(constant_base+12);}
 else if(name=="effect6"||name=="effect26_scalar"){p.effects[0]=captured[2];p.effects[0].id=name=="effect6"?6:26;}
 else if(name=="effect17")p.effects[0]=captured[3];
 else if(name=="effect21")p.effects[0]=effect(21,769);
 else if(name=="effect22")p.effects[0]=effect(22,769);
 else if(name=="effect24")p.effects[0]=effect(24,769);
 else if(name=="thirteen_effect_sequence"){p.effect_count=13;for(auto&e:p.effects)e=effect(1,769);}
 else throw std::runtime_error("unknown typed reference family");
 return p;
}
static void observe(void*,const std::byte*address,std::span<const std::byte>value){
 Write w{static_cast<unsigned>(address-reinterpret_cast<std::byte*>(g_ram)),static_cast<unsigned>(value.size()),{}};
 std::memcpy(w.value.data(),value.data(),value.size());
 if(mode=="--planted-value"&&writes.empty())w.value[0]^=1;
 if(mode=="--planted-transient"&&writes.empty()){auto extra=w;extra.value[0]^=1;writes.push_back(extra);}
 writes.push_back(w);
}
static typed::Streams streams(const Cpu&c){
 auto span=[](unsigned address){if(address>=kRamSize)throw std::runtime_error("stream base outside fixture RAM");return std::span(reinterpret_cast<std::byte*>(g_ram+address),kRamSize-address);};
 return {span(c.r[4].w[0]),span(c.r[5].w[0]),span(c.r[6].w[0]),span(c.r[7].w[0]),std::size_t(c.r[8].d[0]),std::size_t(c.r[9].d[0]),c.r[10].w[0]};
}
static bool compare(Program program,Cpu c,typed::Pipeline p,unsigned seed,unsigned alias){
 std::vector<unsigned char> before(g_ram,g_ram+kRamSize),expected(kRamSize);Cpu reference=c;writes.clear();preempt_calls=0;yield_requested=false;
 program.run(reference);auto expected_writes=writes;std::memcpy(expected.data(),g_ram,kRamSize);auto expected_calls=preempt_calls;
 std::memcpy(g_ram,before.data(),kRamSize);writes.clear();auto inputs=streams(c);std::vector<std::byte> snapshot_a,snapshot_b;
 if(mode=="--planted-alias-snapshot"){snapshot_a.assign(inputs.input_a.begin(),inputs.input_a.end());snapshot_b.assign(inputs.input_b.begin(),inputs.input_b.end());inputs.input_a=snapshot_a;inputs.input_b=snapshot_b;}
 auto got=typed::run(inputs,p,{nullptr,&observe});
 if(mode=="--planted-order"&&writes.size()>1)std::swap(writes[0],writes[1]);
 if(got.accepted!=reference.r[2].w[0]||std::memcmp(expected.data(),g_ram,kRamSize)||expected_writes!=writes||expected_calls){
  std::fprintf(stderr,"PIPELINE_DIFFERENCE program=%s count=%u seed=%u alias=%u accepted=%u/%u writes=%zu/%zu\n",program.name,c.r[10].w[0],seed,alias,got.accepted,reference.r[2].w[0],writes.size(),expected_writes.size());return false;}
 return true;
}
template<class F>static bool refused(F f,const char* reason){try{f();}catch(const std::exception&e){return std::string(e.what()).find(reason)!=std::string::npos;}return false;}
static unsigned build(typed::Pipeline p){
 const unsigned object=original_object,owner=ld32(object+72),count=ld32(owner+536),attributes=ld32(owner+540);
 constexpr unsigned output=0x1c00000,list=0x1c30000;
 if(count>32||attributes>kRamSize-count*12)throw std::runtime_error("original attribute extent");
 std::memcpy(g_ram+list,g_ram+attributes,count*12);put(list+count*12,0xffffffffu);
 put(object+192,p.effect_count);put(object+80,p.filter_count);put(object+84+20,p.filter_limit);
 std::memset(g_ram+object+208,0,13*48);
 for(unsigned i=0;i<p.effect_count&&i<13;++i){const auto&e=p.effects[i];unsigned r=object+208+i*48;
  st16(r,e.id);put(r+8,e.destination_tag);
  if(e.id==2||e.id==3||e.id==25)put(r+16,e.source_tag);
  if(e.id==5||(e.id==26&&e.destination_tag==256))for(unsigned lane=0;lane<3;++lane)put(r+16+lane*4,e.constant[lane]);
  if(e.id==6||(e.id==26&&e.destination_tag==258))put(r+16,e.constant[0]);
  if(e.id==17){put(r+16,e.source_tag);put(r+24,e.remap.start);put(r+28,e.remap.extent);put(r+32,e.remap.upper);put(r+36,e.remap.lower);}
 }
 // Known-owned output allocation supplies an explicit nonzero tail witness,
 // rather than inventing zeros beyond an original constant block.
 std::memset(g_ram+output,0x95,4096);
 Cpu c=captured_cpu;c.r[4].d[0]=list;c.r[5].d[0]=object;c.r[6].d[0]=output;c.r[7].d[0]=2000;
 c.r[8].d[0]=0x1c10000;c.r[9].d[0]=3803;c.r[10].d[0]=0x1c20000;c.r[11].d[0]=400;
 c.r[29].d[0]=0x1fff000;c.r[31].d[0]=0x100000;c.budget=10000000;preempt_calls=0;yield_requested=false;
 reference(c,0x2ed4b0);if(c.pc!=0x100000||preempt_calls)throw std::runtime_error("original numeric producer continuation");return c.r[2].w[0];
}
int main(int argc,char**argv){try{
 mode=argc>1?argv[1]:"";Cpu actual{};read("Cpu.bin",&actual,sizeof actual);read("RAM.bin",g_ram,kRamSize);original_object=actual.r[19].w[0];
 captured_ram.assign(g_ram,g_ram+kRamSize);captured_cpu=actual;
 for(unsigned i=0;i<4;++i){const unsigned record=original_object+208+i*48;auto&e=captured[i];e=effect(ld16(record),ld32(record+8));
  if(e.id==5)for(unsigned lane=0;lane<3;++lane)e.constant[lane]=ld32(record+16+lane*4);
  if(e.id==6)e.constant[0]=ld32(record+16);
  if(e.id==17){e.source_tag=ld32(record+16);e.remap.start=ld32(record+24);e.remap.extent=ld32(record+28);e.remap.upper=ld32(record+32);e.remap.lower=ld32(record+36);}}
 captured[1].constant[3]=captured[2].constant[0];filter_limit=ld32(original_object+84+20);
 const auto actual_pipeline=pipeline("regenerated",actual.f[12],actual.r[11].w[0]);
 if(!compare(programs[0],actual,actual_pipeline,0,99))return 1;
 unsigned cases=1,alias_cases=0;
 constexpr unsigned special[]={0u,0x80000000u,1u,0x7f7fffffu,0x7f800001u,0xffc00001u,0x3f800000u,0xc0200000u};
 for(const auto&program:programs)for(unsigned count:{0u,1u,2u,8u})for(unsigned seed=0;seed<8;++seed)for(unsigned alias=0;alias<6;++alias){
  std::memset(g_ram,0,kRamSize);std::ifstream f(std::string("@PROGRAMS@/")+program.name+".bin",std::ios::binary);f.read(reinterpret_cast<char*>(g_ram+0x1c00000),program.words*4);if(!f||f.peek()!=EOF)return 2;
  const unsigned stride_a=seed&1?48:64,stride_b=seed&2?0:16;
  Cpu c{};c.pc=0x1c00000;c.r[4].d[0]=0x100000;c.r[5].d[0]=0x200000;c.r[6].d[0]=0x300000;c.r[7].d[0]=0x400000;
  if(alias==1){c.r[6].d[0]=c.r[4].d[0];c.r[7].d[0]=c.r[5].d[0];}
  if(alias==2)c.r[6].d[0]=c.r[4].d[0]+16;
  if(alias==3)c.r[6].d[0]=c.r[4].d[0]+stride_a;
  if(alias==4)c.r[6].d[0]=c.r[4].d[0]-stride_a;
  if(alias==5){c.r[6].d[0]=c.r[5].d[0];c.r[7].d[0]=c.r[4].d[0]+16;}
  c.r[8].d[0]=stride_a;c.r[9].d[0]=stride_b;c.r[10].d[0]=count;c.r[11].d[0]=0x1c00000+program.code_words*4;c.r[31].d[0]=0x100000;c.r[29].d[0]=0x1fff000;
  c.f[12]=c.vu0.vf[1].w[0]=special[seed];c.vu0.vf[0].w[3]=0x3f800000;c.budget=10000000;
  for(unsigned i=0;i<8;++i){for(unsigned j=0;j<12;++j)put(c.r[4].w[0]+i*stride_a+j*4,special[(i+j+seed)%8]);put(c.r[5].w[0]+i*stride_b,special[(i+seed+3)%8]);}
  auto p=pipeline(program.name,c.f[12],c.r[11].w[0]);if(!compare(program,c,p,seed,alias))return 1;++cases;alias_cases+=alias!=0;
 }
 unsigned zero_empty_cases=0;
 for(const auto&program:programs){
  std::memset(g_ram,0,kRamSize);std::ifstream f(std::string("@PROGRAMS@/")+program.name+".bin",std::ios::binary);f.read(reinterpret_cast<char*>(g_ram+0x1c00000),program.words*4);if(!f)return 2;
  Cpu c{};c.r[4].d[0]=c.r[5].d[0]=c.r[6].d[0]=c.r[7].d[0]=0xffffffffu;c.r[8].d[0]=c.r[9].d[0]=0xffffffffffffffffull;c.r[11].d[0]=0x1c00000+program.code_words*4;c.r[31].d[0]=0x100000;c.budget=10000000;
  auto p=pipeline(program.name,0x3f800000,c.r[11].w[0]);std::vector<unsigned char> before(g_ram,g_ram+kRamSize);writes.clear();preempt_calls=0;yield_requested=false;program.run(c);
  if(c.r[2].w[0]||c.pc!=0x100000||preempt_calls||!writes.empty()||std::memcmp(before.data(),g_ram,kRamSize))throw std::runtime_error("original zero-count stream-access claim failed");
  if(typed::run({},p,{nullptr,&observe}).accepted||!writes.empty()||std::memcmp(before.data(),g_ram,kRamSize))throw std::runtime_error("native empty zero-count mismatch");++zero_empty_cases;
 }
 unsigned numeric_cases=0;
 // Numeric descriptor changes go through the ACTUAL original builder. The
 // Every reachable instruction/delayword must remain byte-identical to its
 // precompiled reference. Nonreachable alignment padding may retain owned
 // allocation bytes; it is never claimed to be initialized original code.
 for(const auto&program:programs)for(unsigned seed=0;seed<8;++seed){
  std::memcpy(g_ram,captured_ram.data(),kRamSize);auto p=pipeline(program.name,special[seed],0x1c00000+program.code_words*4);
  for(unsigned i=0;i<p.effect_count;++i){auto&e=p.effects[i];
   if(e.id==5||(e.id==26&&e.destination_tag==256))for(unsigned lane=0;lane<3;++lane)e.constant[lane]=special[(seed+lane+2)%8];
   if(e.id==6||(e.id==26&&e.destination_tag==258))e.constant[0]=special[(seed+4)%8];
   if(e.id==17){e.remap.upper=seed&1?(-1000+int(seed)):255+int(seed);e.remap.lower=seed&2?-23:17;e.remap.start=special[(seed+3)%8];e.remap.extent=special[(seed+6)%8];}
  }
  if(p.filter_count)p.filter_limit=special[(seed+1)%8];
  auto n=build(p);if(n!=program.words||ld32(original_object+0x394)!=program.code_words)throw std::runtime_error("numeric descriptor changed original program extent");
  std::vector<unsigned char> frozen(program.words*4);std::ifstream f(std::string("@PROGRAMS@/")+program.name+".bin",std::ios::binary);f.read(reinterpret_cast<char*>(frozen.data()),frozen.size());
  if(!f)throw std::runtime_error("frozen original program read failed");
  for(unsigned i=0;i<program.code_words;++i)if((program.executed_words>>i)&1u)
   if(std::memcmp(frozen.data()+i*4,g_ram+0x1c00000+i*4,4))throw std::runtime_error("numeric descriptor changed original reachable instruction");
  for(unsigned i=0;i<p.effect_count;++i)if(p.effects[i].id==5||(p.effects[i].id==26&&p.effects[i].destination_tag==256))p.effects[i].constant[3]=ld32(0x1c00000+program.code_words*4+12);
  const std::vector<unsigned char> prepared(g_ram,g_ram+kRamSize);
  for(unsigned count:{0u,1u,8u})for(unsigned alias:{0u,1u,3u}){
   std::memcpy(g_ram,prepared.data(),kRamSize);Cpu c=captured_cpu;c.r[4].d[0]=0x100000;c.r[5].d[0]=0x200000;c.r[6].d[0]=alias?0x100000+(alias==3?64:0):0x300000;c.r[7].d[0]=alias==1?0x200000:0x400000;
   c.r[8].d[0]=64;c.r[9].d[0]=16;c.r[10].d[0]=count;c.r[11].d[0]=0x1c00000+program.code_words*4;c.r[31].d[0]=0x100000;c.f[12]=c.vu0.vf[1].w[0]=p.step;c.budget=10000000;
   for(unsigned i=0;i<8;++i){for(unsigned j=0;j<12;++j)put(c.r[4].w[0]+i*64+j*4,special[(i+j+seed)%8]);put(c.r[5].w[0]+i*16,special[(i+seed+3)%8]);}
   if(!compare(program,c,p,seed+100,alias))return 1;++numeric_cases;
  }
 }
 std::memcpy(g_ram,captured_ram.data(),kRamSize);auto s=streams(actual);unsigned refusals=0;
 auto rejection=[&](typed::Pipeline p,const char* reason){writes.clear();if(!refused([&]{typed::run(s,p,{nullptr,&observe});},reason)||!writes.empty())throw std::runtime_error("native refusal did not precede writes");++refusals;};
 auto p=actual_pipeline;p.effects[0].id=0x7fff;rejection(p,"unsupported original effect ID");
 p=actual_pipeline;p.effects[1]=p.effects[3];rejection(p,"duplicate17");
 p=actual_pipeline;p.filter_count=2;rejection(p,"filter cardinality");
 p=actual_pipeline;p.effects[0].destination_tag=768;rejection(p,"resource binding");
 p=actual_pipeline;p.effect_count=14;rejection(p,"13-record");
 p=actual_pipeline;std::swap(p.effects[0],p.effects[1]);rejection(p,"filter binding");
 std::printf("PASS typed_pipeline full_RAM_ordered_write_return_comparisons=%u actual_snapshot_seeded_pipeline=1 constructed_alias_cases=%u zero_count_empty_span_original_comparisons=%u original_numeric_producer_differentials=%u original_error_domain_refusals=%u whole_CPU_projection=UNPROVED runtime_adoption=NONE\n",cases+numeric_cases+zero_empty_cases,alias_cases,zero_empty_cases,numeric_cases,refusals);return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"REFUSED %s\n",e.what());return 2;}}
'''

if __name__=='__main__':raise SystemExit(main())
