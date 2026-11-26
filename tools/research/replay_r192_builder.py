"""Compile/replay the actual original builder offline on a captured descriptor.

The fixture contains original AOT C++ bodies, not a word interpreter. It writes
only a fresh ignored output. The invocation is constructed from authenticated
captured descriptor/attribute data; it is not an observed builder invocation.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import struct
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import analyze_r192_builder_domain as audit


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ['elf', 'listing', 'candidate-build', 'boundary', 'compiler', 'output']:
        p.add_argument('--'+name, type=Path, required=True)
    a = p.parse_args(); out = a.output.resolve(); g = a.candidate_build/'generated_ee'
    audit.need(out.is_relative_to((audit.W/'build/oracle-telemetry').resolve()) and not out.exists(), 'fresh_owned_output_required')
    audit.need(audit.pin(a.elf)['sha256'] == audit.ELF_SHA and audit.pin(a.listing)['sha256'] == audit.LISTING_SHA, 'original_input_changed')
    sources = {name:(g/name).read_bytes().decode().replace('\r\n', '\n') for name in ['unit_0036.cpp', 'unit_007A.cpp', 'unit_007B.cpp', 'unit_007C.cpp']}
    for name, marker in [('unit_0036.cpp','L_001DBB20:'), ('unit_007A.cpp','L_002EAF20:'), ('unit_007C.cpp','L_002F0600:')]:
        audit.need(sources[name].count(marker) == 1, 'original_slice_end_missing')
    pieces = [sources['unit_0036.cpp'].split('L_001DB8A8:', 1)[1].split('L_001DBB20:', 1)[0],
              sources['unit_007A.cpp'].split('L_002EAE10:', 1)[1].split('L_002EAF20:', 1)[0],
              sources['unit_007B.cpp'].split('L_002ED300:', 1)[1].split('\n}\n', 1)[0],
              sources['unit_007C.cpp'].split('L_002F0000:', 1)[1].split('L_002F0600:', 1)[0]]
    starts = [0x1db8a8, 0x2eae10, 0x2ed300, 0x2f0000]
    text = '\n'.join(f'L_{start:08X}:'+piece for start, piece in zip(starts, pieces))
    labels = sorted(set(re.findall(r'^L_([0-9A-F]{8}):', text, re.M)))
    # Original direct-call targets are explicit in the generated call bundle;
    # retain original delay statements, link assignments and return checks.
    call_pattern = r'if \(depth < kMaxDepth\) unit_\w+\(c, \d+u, depth \+ 1u\); else \{ c.pc = (0x[0-9A-F]+u); return; \}'
    text, calls = re.subn(call_pattern, r'reference(c, \1, depth + 1u);', text)
    text = text.replace('call(c, t, depth + 1u)', 'reference(c, t, depth + 1u)')
    # Tail calls encode a unit/case instead of an explicit PC. Authenticate that
    # mapping against the original generated unit's switch before rewriting.
    case_maps = {}; case_sources = []
    for unit in set(re.findall(r'unit_([0-9A-Fa-f]+)\(c,', text)):
        src = (g/f'unit_{unit}.cpp').read_bytes().decode()
        case_sources.append(g/f'unit_{unit}.cpp')
        case_maps[unit] = dict(re.findall(r'case (\d+)u: goto L_([0-9A-F]{8});', src))
    def tail(m):
        unit, case = m.groups()
        audit.need(case in case_maps[unit], 'original_tail_case_mapping_missing')
        return f'reference(c, 0x{case_maps[unit][case]}u, depth); return;'
    text = re.sub(r'\[\[clang::musttail\]\] return unit_([0-9A-Fa-f]+)\(c, (\d+)u, depth\);', tail, text)
    audit.need(not re.search(r'\bunit_\w+\(', text), 'unmapped_original_call')
    switches = '\n'.join(f'case 0x{x}u:goto L_{x};' for x in labels)
    original = audit.Original(a.elf.read_bytes())
    import csv
    joined = []
    with a.listing.open(encoding='utf8', newline='') as f:
        for row in csv.DictReader(f, delimiter='\t'):
            pc = int(row['address'], 16)
            if any(lo <= pc < hi for lo, hi in [(0x1db8a8, 0x1dbb20), (0x2eae10, 0x2eaf20), (0x2ed300, 0x2f0600)]):
                word = int.from_bytes(bytes.fromhex(row['bytes']), 'little')
                audit.need(original.word(pc) == word, 'original_listing_word_changed')
                joined.append((pc, word))
    cpu = (a.boundary/'Cpu.bin').read_bytes(); ram = (a.boundary/'RAM.bin').read_bytes()
    audit.need(len(cpu) == 1456 and len(ram) == 32<<20, 'boundary_ABI_changed')
    u32 = lambda address:struct.unpack_from('<I', ram, address)[0]
    obj = struct.unpack_from('<I', cpu, 19*16)[0]; base = u32(obj+0x4c)
    owner = u32(obj+72); count = u32(owner+536); attributes = u32(owner+540)
    audit.need(count <= 32 and attributes+count*12 <= len(ram), 'captured_attribute_extent_unproved')
    data = [dict(id=u32(attributes+i*12), stream=u32(attributes+i*12+4), offset=u32(attributes+i*12+8)) for i in range(count)]
    out.mkdir(parents=True)
    fixture = HEADER.replace('@SWITCHES@', switches).replace('@BODY@', text)
    fixture += TEST.replace('@BOUNDARY@', a.boundary.resolve().as_posix()).replace('@OUTPUT@', out.as_posix())
    cpp = out/'original_builder.cpp'; cpp.write_text(fixture, encoding='utf8', newline='\n'); binary = out/'original_builder.exe'
    input_paths=list(dict.fromkeys([Path(__file__), Path(audit.__file__), a.elf, a.listing, a.compiler,
                 *(g/name for name in sources), *case_sources,
                 *(a.boundary/name for name in ['Cpu.bin','RAM.bin','SPR.bin']),
                 *(a.candidate_build/'src/runtime/include/ps2'/name for name in
                   ['cpu.hpp','fpu.hpp','vu.hpp','memory.hpp','mmi.hpp','generated.hpp'])]))
    input_pins=[audit.pin(x) for x in input_paths]
    command = [str(a.compiler), '-std=c++20', '-mavx2', '-ffp-contract=off', '-O1', '-static-libgcc', '-static-libstdc++',
               '-I'+str((a.candidate_build/'src/runtime/include').resolve()), str(cpp), '-o', str(binary)]
    cp = subprocess.run(command, capture_output=True); (out/'compile.log').write_bytes(cp.stdout+cp.stderr)
    audit.need(cp.returncode == 0, 'original_builder_fixture_compile_failed')
    runs=[]
    for name, args, expected in [('positive',[],0), ('planted-code-word',['--planted-code-word'],1),
                                 ('planted-constant-word',['--planted-constant-word'],1),
                                 ('planted-callback-table',['--planted-callback-table'],2)]:
        run = subprocess.run([str(binary), *args], capture_output=True)
        (out/(name+'.log')).write_bytes(run.stdout+run.stderr)
        runs.append(dict(name=name, argv=[str(binary), *args], exit=run.returncode, expected_exit=expected,
                         stdout=run.stdout.decode(errors='replace'), stderr=run.stderr.decode(errors='replace')))
    changed=[pin['path'] for pin in input_pins if audit.pin(Path(pin['path']))['sha256']!=pin['sha256']]
    passed=all(r['exit']==r['expected_exit'] for r in runs) and not changed
    record = dict(status='PASS_ORIGINAL_DESCRIPTOR_REGENERATION' if passed else 'FAIL', runs=runs,
                  original_builder_entry='002ED4B0', original_direct_call_bundles_preserved=calls,
                  original_word_joins=len(joined), original_word_join_sha256=hashlib.sha256(json.dumps(joined).encode()).hexdigest(),
                  captured_object=f'{obj:08X}', captured_entry=f'{base:08X}', captured_source_owner=f'{owner:08X}', attributes=data,
                  compile_command=command, source_pins=[*input_pins,audit.pin(cpp),audit.pin(binary)], input_changes=changed,
                  outputs=[audit.pin(x) for x in out.glob('*.bin')],
                  scope='Constructed offline invocation of actual compiled original builder on captured descriptor data. No observed builder invocation, game launch, runtime interpreter, descriptor-universe closure, native credit or candidate change.')
    dest = out/'RESULT.json'; dest.write_text(json.dumps(record, indent=2)+'\n', encoding='utf8', newline='\n')
    print(json.dumps(dict(status=record['status'], result=str(dest), sha256=audit.pin(dest)['sha256'])))
    return 0 if passed else 1


HEADER = r'''
#include "ps2/generated.hpp"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>
#include <stdexcept>
#include <string>
#include <map>
#include <array>
namespace ps2 {
alignas(64) std::uint8_t g_ram[kRamSize+64],g_spr[kScratchpadSize];
bool g_watch_on=false;std::uint32_t g_watch_base=0,g_watch_end=0;
void watch_hit(std::uint32_t,std::uint32_t,const char*){}
void watch_span(std::uint32_t,std::uint32_t,const char*){}
std::uint8_t slow_ld8(std::uint32_t){throw std::runtime_error("non-RAM ld8");}
std::uint16_t slow_ld16(std::uint32_t){throw std::runtime_error("non-RAM ld16");}
std::uint32_t slow_ld32(std::uint32_t){throw std::runtime_error("non-RAM ld32");}
std::uint64_t slow_ld64(std::uint32_t){throw std::runtime_error("non-RAM ld64");}
void slow_ld128(std::uint32_t,Reg128&){throw std::runtime_error("non-RAM ld128");}
void slow_st8(std::uint32_t,std::uint8_t){throw std::runtime_error("non-RAM st8");}
void slow_st16(std::uint32_t,std::uint16_t){throw std::runtime_error("non-RAM st16");}
void slow_st32(std::uint32_t,std::uint32_t){throw std::runtime_error("non-RAM st32");}
void slow_st64(std::uint32_t,std::uint64_t){throw std::runtime_error("non-RAM st64");}
void slow_st128(std::uint32_t,const Reg128&){throw std::runtime_error("non-RAM st128");}
bool preempt(Cpu&){throw std::runtime_error("offline producer budget exhausted");}
}
using namespace ps2;
static std::map<unsigned,unsigned> entries_observed;
static void reference(Cpu& c,std::uint32_t entry,unsigned depth=0){
 ++entries_observed[entry];
 if(depth>100)throw std::runtime_error("original builder call depth exceeded");
 switch(entry){@SWITCHES@ default:std::fprintf(stderr,"unproved original helper %08X\n",entry);throw std::runtime_error("unproved original helper");}
 @BODY@
}
'''

TEST = r'''
static void read(const char* name,void* destination,std::size_t size){std::ifstream f(std::string("@BOUNDARY@/")+name,std::ios::binary);f.read(static_cast<char*>(destination),size);if(!f||f.peek()!=EOF)throw std::runtime_error("boundary size mismatch");}
int main(int argc,char**argv){try{
 Cpu c{};read("Cpu.bin",&c,sizeof c);read("RAM.bin",g_ram,kRamSize);read("SPR.bin",g_spr,kScratchpadSize);
 const std::string mode=argc>1?argv[1]:"";
 const auto object=c.r[19].w[0],base=ld32(object+0x4c),owner=ld32(object+72),count=ld32(owner+536),attributes=ld32(owner+540);
 constexpr unsigned output=0x1c00000,arena=0x1c10000,blocks=0x1c20000,list=0x1c30000;
 if(count>32||attributes>kRamSize-count*12)throw std::runtime_error("attribute extent");
 std::vector<unsigned char> expected(g_ram+base,g_ram+base+244);
 std::vector<unsigned char> captured_ram(g_ram,g_ram+kRamSize);const Cpu captured_cpu=c;
 if(mode=="--planted-callback-table")st32(0x5caf94,0x2ee160);
 constexpr unsigned ids[]={1,25,2,3,26,5,6,17,22,21,24};
 constexpr unsigned callbacks[]={0x2ee290,0x2ee160,0x2ee160,0x2ee160,0x2edfa0,0x2edfa0,0x2edfa0,0x2edc50,0x2ee2d0,0x2ee2d0,0x2ee2d0};
 for(unsigned i=0;i<11;++i)if(ld32(0x5caf90+i*8)!=ids[i]||ld32(0x5caf94+i*8)!=callbacks[i])throw std::runtime_error("original callback table identity changed");
 std::memcpy(g_ram+list,g_ram+attributes,count*12);st32(list+count*12,0xffffffffu);
 c.r[4].d[0]=list;c.r[5].d[0]=object;c.r[6].d[0]=output;c.r[7].d[0]=2000;
 c.r[8].d[0]=arena;c.r[9].d[0]=3803;c.r[10].d[0]=blocks;c.r[11].d[0]=400;
 c.r[29].d[0]=0x1fff000;c.r[31].d[0]=0x100000;c.budget=10000000;
 reference(c,0x2ed4b0);
 auto words=c.r[2].w[0];if(words>2000||c.pc!=0x100000)throw std::runtime_error("builder return extent/continuation");
 auto save=[&](const std::string&name,unsigned n){std::ofstream f(std::string("@OUTPUT@/")+name+".bin",std::ios::binary);f.write(reinterpret_cast<char*>(g_ram+output),n*4);if(!f)throw std::runtime_error("fixture output write failed");};
 if(mode.empty())save("regenerated",words);
 if(mode=="--planted-code-word")st32(output,ld32(output)^0x10000);
 if(mode=="--planted-constant-word")st32(output+52*4,ld32(output+52*4)^1);
 unsigned differences=0;for(unsigned i=0;i<expected.size();++i)if(expected[i]!=g_ram[output+i]){if(differences<12)std::fprintf(stderr,"difference offset=%X expected=%02X actual=%02X\n",i,expected[i],g_ram[output+i]);++differences;}
 std::printf("original_builder_words=%u original_object_code_words=%u compared_code_and_constants_bytes=%zu differences=%u\n",words,ld32(object+0x394),expected.size(),differences);
 if(differences)return 1;
 for(unsigned target:{0x2ee290u,0x2ee160u,0x2edfa0u,0x2edc50u,0x2ee2d0u,0x2ed300u})std::printf("actual_descriptor_callback_%08X=%u\n",target,entries_observed[target]);
 // These are constructed descriptors using source-proved record fields, not
 // additional observed invocations or a claim that all sequences are covered.
 using Effect=std::array<unsigned,12>;
 std::vector<Effect> effects;
 for(unsigned i=0;i<4;++i){Effect e;std::memcpy(e.data(),captured_ram.data()+object+208+i*48,48);effects.push_back(e);}
 auto make=[](unsigned id,unsigned attribute,unsigned parameter){Effect e{};e[0]=id;e[2]=attribute;e[4]=parameter;return e;};
 std::vector<std::pair<std::string,std::vector<Effect>>> variants;
 variants.push_back({"identity",{}});variants.push_back({"effect1",{make(1,769,0)}});
 variants.push_back({"effect2",{make(2,256,256)}});variants.push_back({"effect3",{make(3,769,258)}});
 variants.push_back({"effect25",{make(25,769,258)}});
 Effect vector=effects[1];vector[0]=26;variants.push_back({"effect26_vector",{vector}});
 Effect scalar=effects[2];scalar[0]=26;variants.push_back({"effect26_scalar",{scalar}});
 variants.push_back({"effect5",{effects[1]}});variants.push_back({"effect6",{effects[2]}});variants.push_back({"effect17",{effects[3]}});
 for(unsigned id:{21,22,24})variants.push_back({"effect"+std::to_string(id),{make(id,769,0)}});
 std::vector<Effect> maximum;for(unsigned i=0;i<13;++i)maximum.push_back(effects[0]);variants.push_back({"thirteen_effect_sequence",maximum});
 unsigned accepted=0;
 for(const auto&[name,sequence]:variants){
  std::memcpy(g_ram,captured_ram.data(),kRamSize);c=captured_cpu;
  st32(object+192,sequence.size());st32(object+80,0);for(unsigned i=0;i<sequence.size();++i)std::memcpy(g_ram+object+208+i*48,sequence[i].data(),48);
  std::memcpy(g_ram+list,g_ram+attributes,count*12);st32(list+count*12,0xffffffffu);
  c.r[4].d[0]=list;c.r[5].d[0]=object;c.r[6].d[0]=output;c.r[7].d[0]=2000;c.r[8].d[0]=arena;c.r[9].d[0]=3803;c.r[10].d[0]=blocks;c.r[11].d[0]=400;
  c.r[29].d[0]=0x1fff000;c.r[31].d[0]=0x100000;c.budget=10000000;reference(c,0x2ed4b0);unsigned n=c.r[2].w[0];
  if(n==0||n>2000||c.pc!=0x100000)throw std::runtime_error("constructed source-valid descriptor rejected");
  save(name,n);std::printf("constructed_variant=%s words=%u code_words=%u\n",name.c_str(),n,ld32(object+0x394));++accepted;
 }
 std::memcpy(g_ram,captured_ram.data(),kRamSize);c=captured_cpu;st32(object+192,1);st16(object+208,0x7fff);st32(object+80,0);
 c.r[4].d[0]=list;c.r[5].d[0]=object;c.r[6].d[0]=output;c.r[7].d[0]=2000;c.r[8].d[0]=arena;c.r[9].d[0]=3803;c.r[10].d[0]=blocks;c.r[11].d[0]=400;
 c.r[29].d[0]=0x1fff000;c.r[31].d[0]=0x100000;c.budget=10000000;reference(c,0x2ed4b0);
 if(c.r[2].w[0]!=0||c.pc!=0x100000)throw std::runtime_error("unsupported effect not refused by original builder");
 std::memcpy(g_ram,captured_ram.data(),kRamSize);c=captured_cpu;st32(object+192,2);st32(object+80,0);
 std::memcpy(g_ram+object+208,effects[3].data(),48);std::memcpy(g_ram+object+256,effects[3].data(),48);
 std::memcpy(g_ram+list,g_ram+attributes,count*12);st32(list+count*12,0xffffffffu);
 c.r[4].d[0]=list;c.r[5].d[0]=object;c.r[6].d[0]=output;c.r[7].d[0]=2000;c.r[8].d[0]=arena;c.r[9].d[0]=3803;c.r[10].d[0]=blocks;c.r[11].d[0]=400;
 c.r[29].d[0]=0x1fff000;c.r[31].d[0]=0x100000;c.budget=10000000;reference(c,0x2ed4b0);
 if(c.r[2].w[0]!=0||c.pc!=0x100000)throw std::runtime_error("duplicate17 not refused by original builder");
 std::printf("PASS constructed_descriptor_variants=%u original_unsupported_effect_refusal=1 original_duplicate17_refusal=1 full_native_backend=UNPROVED\n",accepted);return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"REFUSED %s\n",e.what());return 2;}}
'''

if __name__ == '__main__':
    raise SystemExit(main())
