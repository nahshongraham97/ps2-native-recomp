"""Build one address-independent compiled kernel from an authenticated EE stub.

Known-shape implementation only. Exact 48 instructions plus 4 inactive padding
words are admitted at any aligned RAM address. This does not enumerate all
outputs of the original high-level constructor, infer ownership, or patch N.
The original eerecomp-produced C++ is the semantic source, not an interpreter.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import struct
import subprocess

W = Path(__file__).resolve().parents[2]
DUMP_SHA = '5cf739b2a2257c95429ea9b1828eb6e1e66b1a580befa40375771f7faca36192'
GENERATOR_SHA = '353f4816634174252a906909884f351c83060c50a20f09801560274de7be2c18'


def require(value, reason):
    if not value:
        raise ValueError(reason)


def pin(path):
    raw = path.read_bytes()
    return dict(path=str(path.resolve()), size=len(raw), sha256=hashlib.sha256(raw).hexdigest())


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-fork', type=Path, required=True)
    p.add_argument('--candidate-build', type=Path, required=True)
    p.add_argument('--dump', type=Path, required=True)
    p.add_argument('--compiler', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    out = a.output.resolve()
    require(out.is_relative_to((W/'build/oracle-telemetry').resolve()) and not out.exists(), 'fresh_owned_output_required')
    require(pin(a.dump)['sha256'] == DUMP_SHA, 'original_dump_identity_mismatch')
    generator = a.source_fork/'build/r192-aot-closure/out_tools/eerecomp.exe'
    require(pin(generator)['sha256'] == GENERATOR_SHA, 'original_v8_generator_changed')
    raw = a.dump.read_bytes(); words = struct.unpack_from('<52I', raw, 0x1140)
    require(words[46:48] == (0x03e00008, 0) and words[48:] == (0,0,0,0), 'original_return_or_padding_changed')
    out.mkdir(parents=True)
    generated = out/'original-aot'
    command = [str(generator.resolve()), '--live-image', str(a.dump.resolve()), '--base', '008A4000',
               '--entry', '008A5140', '--stamp', 'A2659FA9BD846661', '--dense-checks', '--out', str(generated)]
    result = subprocess.run(command, capture_output=True, text=True)
    (out/'generator.log').write_text(result.stdout+result.stderr, encoding='utf8', newline='\n')
    require(result.returncode == 0, 'original_AOT_generation_failed')
    unit = next(p for p in generated.glob('m_*unit_*.cpp') if 'L_008A5140:' in p.read_text())
    source = unit.read_text(); start = source.index('L_008A5140:'); stop = source.index('L_008A6880:', start)
    original = source[start:stop]
    require(original.count('preempt(c)') == 1 and original.count('0x008A5170u') == 1, 'unproved_preemption_shape')
    require(not re.search(r'\b(?:call|dispatch|unit_\w+)\s*\(', original), 'unbound_external_call')
    require(set(re.findall(r'0x[0-9A-Fa-f]{8}u', original)) == {'0x008A5170u'}, 'unbound_absolute_instruction_address')
    # C++ labels denote compiled control flow. Only the yielded guest resume PC
    # is relocated; data addresses/parameters are retained exactly from source.
    compiled = original.replace('0x008A5170u', '(descriptor.entry_base + 0x30u)')
    header = HEADER.replace('@WORDS@', ','.join(f'0x{x:08X}u' for x in words)).replace('@BODY@', compiled)
    kernel = out/'compiled_stub_family.hpp'; kernel.write_text(header, encoding='utf8', newline='\n')
    harness = out/'test.cpp'
    harness.write_text(HARNESS.replace('@KERNEL@', kernel.as_posix()).replace('@ORIGINAL@', original), encoding='utf8', newline='\n')
    binary = out/'test.exe'
    compile_command = [str(a.compiler), '-std=c++20', '-mavx2', '-ffp-contract=off', '-O1',
                       '-static-libgcc', '-static-libstdc++', '-I'+str((a.candidate_build/'src/runtime/include').resolve()),
                       str(harness), '-o', str(binary)]
    cp = subprocess.run(compile_command, capture_output=True, text=True)
    (out/'compile.log').write_text(cp.stdout+cp.stderr, encoding='utf8', newline='\n')
    require(cp.returncode == 0, 'family_and_reference_compile_failed')
    runs=[]
    for name, args in [('positive',[]),('planted-value',['--planted-value']),
                       ('planted-store-order',['--planted-store-order']),('planted-resume',['--planted-resume'])]:
        run=subprocess.run([str(binary),*args],capture_output=True,text=True)
        (out/(name+'.log')).write_text(run.stdout+run.stderr,encoding='utf8',newline='\n')
        runs.append(dict(name=name,argv=[str(binary),*args],exit=run.returncode,stdout=run.stdout,stderr=run.stderr))
    passed = runs[0]['exit']==0 and all(r['exit'] != 0 for r in runs[1:])
    record=dict(status='PASS_KNOWN_SHAPE_FAMILY' if passed else 'FAIL',
                semantic_source='Actual original v8 eerecomp C++ from pinned complete fresh dump; source-preserving PC relocation only.',
                original_word_count=48,admitted_word_count_including_padding=52,source_entry='008A5140',
                descriptor_contract={'entry_base':'aligned arbitrary RAM location containing exact52words',
                    'constant_table':'T3 == entry_base +52*4, matching original002E617C caller derivation withobject+394=52',
                    'entry_points':['entry_base','entry_base+0x30 (yield/resume)'],
                    'parameter_state':'original full Cpu; nonnegative32bit T2 count, bounded RAM input/output spans and strides',
                    'unknowns':'Other instruction shapes/word counts/callers/hardware-memory domains refuse by name'},
                constructor_provenance_limit='Original2EF5F0 and companion primitives recovered; full2ED4B0 assembly/callback/raw-append domains NOT closed. This is one exact code-identity family, not an authenticated exhaustive constructor family catalogue.',
                generator_command=command,compile_command=compile_command,runs=runs,
                source_pins=[pin(x) for x in [Path(__file__),a.dump,generator,unit,kernel,harness,binary]],
                scope='Standalone full Cpu/RAM/ordered-write boundary comparisons across relocations and yields; no N/runtime/ownership/native-credit mutation or all-path closure claim.')
    payload=(json.dumps(record,indent=2)+'\n').encode();(out/'RESULT.json').write_bytes(payload)
    print(json.dumps(dict(status=record['status'],result=str(out/'RESULT.json'),sha256=hashlib.sha256(payload).hexdigest())))
    return 0 if passed else 1


HEADER=r'''
#pragma once
#include "ps2/generated.hpp"
#include <array>
#include <algorithm>
#include <cstring>
#include <stdexcept>
namespace mka::emitted::known_clip48 {
inline constexpr std::array<std::uint32_t,52> pattern={@WORDS@};
struct Descriptor{std::uint32_t entry_base;};
inline Descriptor bind(std::uint32_t base,std::uint32_t declared_words){
  if((base&15)||base>ps2::kRamSize-244||declared_words!=52)throw std::runtime_error("clip48: unproved constructor extent");
  for(unsigned i=0;i<52;++i){std::uint32_t word;std::memcpy(&word,ps2::g_ram+base+i*4,4);if(word!=pattern[i])throw std::runtime_error("clip48: unknown instruction shape");}
  return {base};
}
inline void span(std::uint32_t pointer,std::uint64_t count,std::uint64_t stride,std::uint32_t width){
  if(pointer>ps2::kRamSize-width||stride>0x7fffffffu||count>0x7fffffffu||
     (count&&count-1>(ps2::kRamSize-width-pointer)/std::max<std::uint64_t>(1,stride)))throw std::runtime_error("clip48: unproved RAM extent");
}
inline void run(ps2::Cpu& c,Descriptor descriptor){
  using namespace ps2;
  bind(descriptor.entry_base,52);
  const auto pc=c.pc&~kUnwind;
  if(pc!=descriptor.entry_base&&pc!=descriptor.entry_base+0x30)throw std::runtime_error("clip48: unproved entry");
  if(c.r[11].d[0]!=descriptor.entry_base+208u)throw std::runtime_error("clip48: caller constant-table relation unproved");
  const std::uint64_t iterations=c.r[10].d[0]+(pc==descriptor.entry_base+0x30?1u:0u);
  if(iterations<c.r[10].d[0])throw std::runtime_error("clip48: count overflow");
  span(c.r[4].w[0],iterations,c.r[8].d[0],48);span(c.r[5].w[0],iterations,c.r[9].d[0],4);
  span(c.r[6].w[0],iterations,c.r[8].d[0],48);span(c.r[7].w[0],iterations,c.r[9].d[0],4);
  if(pc==descriptor.entry_base+0x30)goto L_008A5170;
@BODY@
}
}
'''

HARNESS=r'''
#include "ps2/generated.hpp"
#include <array>
#include <vector>
#include <cstring>
#include <cstdio>
#include <stdexcept>
#include <string>
namespace ps2 {
alignas(64) std::uint8_t g_ram[kRamSize+64],g_spr[kScratchpadSize];
bool g_watch_on=false;std::uint32_t g_watch_base=0,g_watch_end=0;
void watch_hit(std::uint32_t,std::uint32_t,const char*){}
void watch_span(std::uint32_t,std::uint32_t,const char*){}
void slow_ld128(std::uint32_t,Reg128&){throw std::runtime_error("non-RAM reference quad read");}
void slow_st128(std::uint32_t,const Reg128&){throw std::runtime_error("non-RAM reference quad write");}
std::uint32_t slow_ld32(std::uint32_t){throw std::runtime_error("non-RAM reference read");}
void slow_st32(std::uint32_t,std::uint32_t){throw std::runtime_error("non-RAM reference write");}
bool yield_requested=false;unsigned preempt_calls=0;
bool preempt(Cpu&){++preempt_calls;return yield_requested;}
struct Write{std::uint32_t address,width;std::array<unsigned char,16> value;bool operator==(const Write&)const=default;};
std::vector<Write> writes;bool plant_value=false,plant_order=false;
void checked_st128(std::uint32_t address,const Reg128&value){Write w{address,16,{}};std::memcpy(w.value.data(),&value,16);writes.push_back(w);st128(address,value);}
void checked_st32(std::uint32_t address,std::uint32_t value){if(plant_value)value^=1;Write w{address,4,{}};std::memcpy(w.value.data(),&value,4);writes.push_back(w);st32(address,value);}
}
#define st128 checked_st128
#define st32 checked_st32
#include "@KERNEL@"
using namespace ps2;
void original(Cpu&c,bool resume){if(resume)goto L_008A5170;@ORIGINAL@}
#undef st128
#undef st32
static void put(unsigned a,unsigned v){std::memcpy(g_ram+a,&v,4);}
int main(int argc,char**argv){
 using namespace mka::emitted::known_clip48;
 std::string mode=argc>1?argv[1]:"";unsigned cases=0,yields=0,accepted=0,rejected=0;
 std::vector<unsigned char> before(kRamSize),reference_ram(kRamSize);
 for(unsigned base:{0x8a5140u,0x800000u,0x1200000u})for(unsigned count:{0u,1u,2u,8u})for(unsigned seed=0;seed<8;++seed)for(bool do_yield:{false,true}){
  for(unsigned i=0;i<kRamSize;++i)g_ram[i]=static_cast<unsigned char>((i*7u+seed*29u)&255u);
  for(unsigned i=0;i<52;++i)put(base+i*4,pattern[i]);
  // Fresh observed constant words; clamp thresholds varied to exercise both branches.
  unsigned constants[9]={0,0x3ca3d70b,0,0xbca3d70b,0x437f0000,0x4087f92c,0,0x42700000,0x42700000};
  for(unsigned i=0;i<9;++i)put(base+208+i*4,constants[i]);
  put(base+240,seed&1?0x7f7fffff:0xbf800000);
  Cpu c{};auto* data=reinterpret_cast<unsigned char*>(&c);for(unsigned i=0;i<sizeof c;++i)data[i]=static_cast<unsigned char>(seed*13+i*11);
  c.r[0]={};c.vu0.vf[0]={};c.vu0.vf[0].w[3]=0x3f800000;
  c.pc=base;c.r[4].d[0]=0x100000;c.r[5].d[0]=0x200000;c.r[6].d[0]=seed&2?0x100000:0x300000;c.r[7].d[0]=seed&2?0x200000:0x400000;
  c.r[8].d[0]=64;c.r[9].d[0]=16;c.r[10].d[0]=count;c.r[11].d[0]=base+208;c.r[31].d[0]=0x123450;c.f[12]=0x3f800000;c.budget=do_yield?(seed&4?30:-1):100000;
  for(unsigned i=0;i<count;++i){put(0x200000+i*16,seed&1?0x3f800000:0x40000000);}
  std::memcpy(before.data(),g_ram,kRamSize);Cpu a=c,b=c;
  yield_requested=do_yield;preempt_calls=0;writes.clear();plant_value=false;original(a,false);
  const unsigned reference_preempts=preempt_calls;auto trace=writes;std::memcpy(reference_ram.data(),g_ram,kRamSize);
  std::memcpy(g_ram,before.data(),kRamSize);preempt_calls=0;writes.clear();plant_value=mode=="--planted-value";
  run(b,bind(base,52));plant_value=false;
  if(mode=="--planted-store-order"&&writes.size()>1)std::swap(writes[0],writes[1]);
  if(a.pc==(0x8a5170u|kUnwind))a.pc=(base+0x30u)|kUnwind;
  if(mode=="--planted-resume"&&(b.pc&kUnwind))b.pc^=4;
  if(std::memcmp(&a,&b,sizeof a)||std::memcmp(reference_ram.data(),g_ram,kRamSize)||trace!=writes||reference_preempts!=preempt_calls){std::fprintf(stderr,"DIFFERENCE base=%X count=%u seed=%u yield=%d writes=%zu/%zu\n",base,count,seed,do_yield,trace.size(),writes.size());return 1;}
  ++cases;if(b.pc&kUnwind)++yields;
  accepted+=trace.empty()?0:1;rejected+=trace.empty()?1:0;
  if(b.pc&kUnwind){
   // Retained post-delay count/CPU/RAM, not invented fresh arguments.
   a=b;a.budget=100000;b.budget=100000;
   std::memcpy(before.data(),g_ram,kRamSize);writes.clear();preempt_calls=0;yield_requested=false;original(a,true);
   const auto resume_trace=writes;const auto resume_preempts=preempt_calls;std::memcpy(reference_ram.data(),g_ram,kRamSize);
   std::memcpy(g_ram,before.data(),kRamSize);writes.clear();preempt_calls=0;run(b,bind(base,52));
   if(std::memcmp(&a,&b,sizeof a)||std::memcmp(reference_ram.data(),g_ram,kRamSize)||resume_trace!=writes||resume_preempts!=preempt_calls)return 4;
   ++cases;
  }
 }
 bool refused=false;try{bind(0x8a5140,51);}catch(const std::exception&){refused=true;}if(!refused)return 2;
 // Independent same-address code change must not bind to the compiled family.
 for(unsigned i=0;i<52;++i)put(0x8a5140+i*4,pattern[i]);put(0x8a5140,pattern[0]^0x10000);refused=false;try{bind(0x8a5140,52);}catch(const std::exception&){refused=true;}if(!refused)return 3;
 std::printf("PASS whole_stub_boundary_cases=%u yielded_paths=%u cases_with_writes=%u cases_without_writes=%u exact_shape_extent_mutants_rejected=2 all_constructor_outputs=UNPROVED\n",cases,yields,accepted,rejected);
}
'''

if __name__=='__main__':
    raise SystemExit(main())
