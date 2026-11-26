"""Authenticate and test the original 002EF5F0 emitted quad-load primitive.

This compiles a reusable template specialization, not an EE interpreter/JIT.
It DOES NOT close the complete 48-word 008A5140 program or the high-level
002ED4B0 constructor. Those have unjoined arithmetic/branches/callbacks.
Reference code is actual original generated C++, privately copied for testing.
"""
from pathlib import Path
import argparse
import csv
import hashlib
import json
import re
import struct
import subprocess

W = Path(__file__).resolve().parents[2]
ELF_SHA = 'a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f'
LISTING_SHA = '4611757f817e6c4ace87bb2d1203bca9c5d8523c33e0ae3e086890ab4b5ce148'


def require(value, reason):
    if not value:
        raise ValueError(reason)


def pin(path):
    data = path.read_bytes()
    return dict(path=str(path.resolve()), size=len(data), sha256=hashlib.sha256(data).hexdigest())


def elf_word(elf, pc):
    phoff = struct.unpack_from('<I', elf, 28)[0]
    step, count = struct.unpack_from('<HH', elf, 42)
    for i in range(count):
        kind, off, va, _, size, *_ = struct.unpack_from('<8I', elf, phoff+i*step)
        if kind == 1 and va <= pc and pc+4 <= va+size:
            return struct.unpack_from('<I', elf, off+pc-va)[0]
    raise ValueError('original_word_not_file_backed:'+hex(pc))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source-fork', type=Path, required=True)
    p.add_argument('--candidate-build', type=Path, required=True)
    p.add_argument('--elf', type=Path, required=True)
    p.add_argument('--listing', type=Path, required=True)
    p.add_argument('--fresh-dump', type=Path, required=True)
    p.add_argument('--compiler', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    out = a.output.resolve()
    require(out.is_relative_to((W/'build/oracle-telemetry').resolve()) and not out.exists(), 'fresh_owned_output_required')
    elf = a.elf.read_bytes()
    require(hashlib.sha256(elf).hexdigest() == ELF_SHA, 'original_ELF_changed')
    require(pin(a.listing)['sha256'] == LISTING_SHA, 'original_listing_changed')
    words = []
    with a.listing.open(encoding='utf8', newline='') as stream:
        for row in csv.DictReader(stream, delimiter='\t'):
            pc = int(row['address'], 16)
            if 0x2ef5f0 <= pc <= 0x2ef6f8 or pc in {0x2ed7d0, 0x2e612c, 0x2e6178, 0x2e617c, 0x2e8390, 0x2e8394}:
                word = int.from_bytes(bytes.fromhex(row['bytes']), 'little')
                require(elf_word(elf, pc) == word, 'listing_ELF_word_mismatch')
                words.append(dict(pc=f'{pc:08X}', word=f'{word:08X}', text=row['mnemonic']))
    require(len([r for r in words if 0x2ef5f0 <= int(r['pc'],16) <= 0x2ef6f8]) == 67, 'emitter_body_not_complete')
    unit = a.candidate_build/'generated_ee/unit_007B.cpp'
    text = unit.read_text(encoding='utf8')
    begin, end = text.index('L_002EF5F0:'), text.index('L_002EF700:')
    original = text[begin:end]
    require(not re.search(r'\bunit_\w+\(', original), 'unexpected_reference_callee')
    dump = a.fresh_dump.read_bytes()
    require(len(dump) == 0x4000 and struct.unpack_from('<I', dump, 0x1140)[0] == 0xD9640000, 'fresh_quad_load_word_changed')
    out.mkdir(parents=True)
    generator = a.source_fork/'build/r192-aot-closure/out_tools/eerecomp.exe'
    generated = out/'original-stub-aot'
    command = [str(generator.resolve()), '--live-image', str(a.fresh_dump.resolve()), '--base', '008A4000',
               '--entry', '008A5140', '--stamp', 'A2659FA9BD846661', '--dense-checks', '--out', str(generated)]
    result = subprocess.run(command, capture_output=True, text=True)
    (out/'generator.log').write_text(result.stdout+result.stderr, encoding='utf8', newline='\n')
    require(result.returncode == 0, 'original_stub_generation_failed')
    references = list(generated.glob('m_*unit_*.cpp'))
    stub = next(x for x in references if 'L_008A5140:' in x.read_text())
    body = stub.read_text().split('L_008A5140:',1)[1]
    # One exact emitted instruction's generated semantic statement. Budget and
    # control-flow projection are NOT claimed as full-stub equivalence.
    first = next(line.strip() for line in body.splitlines() if 'ld128(' in line)
    require(first == 'ld128((c.r[11].w[0]) & ~15u, c.vu0.vf[4]);', 'unexpected_original_first_load_AST')
    family = W/'tools/research/r192_aot_modules/quad_load_family.hpp'
    harness = out/'test.cpp'
    harness.write_text(HARNESS.replace('@FAMILY@', family.as_posix()).replace('@EMITTER@', original)
                       .replace('@FRESH@', first), encoding='utf8', newline='\n')
    binary = out/'test.exe'
    compile_command = [str(a.compiler), '-std=c++20', '-mavx2', '-ffp-contract=off', '-O1',
                       '-static-libgcc', '-static-libstdc++', '-I'+str((a.candidate_build/'src/runtime/include').resolve()),
                       str(harness), '-o', str(binary)]
    compiled = subprocess.run(compile_command, capture_output=True, text=True)
    (out/'compile.log').write_text(compiled.stdout+compiled.stderr, encoding='utf8', newline='\n')
    require(compiled.returncode == 0, 'reference_and_family_compile_failed')
    tested = subprocess.run([str(binary)], capture_output=True, text=True)
    (out/'test.log').write_text(tested.stdout+tested.stderr, encoding='utf8', newline='\n')
    record = dict(status='PASS_PRIMITIVE_ONLY' if tested.returncode == 0 else 'FAIL',
                  source_pins=[pin(x) for x in [Path(__file__), family, a.elf, a.listing, unit, a.fresh_dump, generator]],
                  original_words=words, descriptor_contract={
                      'emitter_entry':'002EF5F0', 'destination_type':{'0':'VU vector','2/3/4':'GPR quad'},
                      'destination_register':'original operand+4, admitted0..31',
                      'base_register':'operand+0C -> memory operand+4 -> base operand+4, admitted0..31',
                      'raw_offset':'LHU operand+10; original load sign-extends16 then aligns address down16',
                      'capacity':'active context throughGP-55A0 -> +E98; capacity+0,current+8,word-buffer+18',
                      'word_formula':'(type0?D8000000:78000000)|(base<<21)|(dest<<16)|rawOffset',
                      'unknowns':'invalid types/indices and non-RAM specialization inputs refuse'},
                  generator_command=command, compile_command=compile_command,
                  test_exit=tested.returncode, test_stdout=tested.stdout, test_stderr=tested.stderr,
                  test_binary=pin(binary), reference_first_instruction=first,
                  boundary_scope='Complete original compiled emitter RAM/store-order check plus ONE fresh emitted-load semantic statement. Not full fresh48word function E2; no native credit/runtime adoption.',
                  complete_fresh_program_status='UNPROVED; 002ED4B0 composition/branches/raw append/callback domains remain open',
                  runtime_changed=False)
    payload=(json.dumps(record,indent=2)+'\n').encode();(out/'RESULT.json').write_bytes(payload)
    print(json.dumps(dict(status=record['status'],sha256=hashlib.sha256(payload).hexdigest(),result=str(out/'RESULT.json'))))
    return tested.returncode


HARNESS = r'''
#include "@FAMILY@"
#include <array>
#include <cstdio>
#include <cstring>
#include <vector>
namespace ps2 {
alignas(64) std::uint8_t g_ram[kRamSize+64],g_spr[kScratchpadSize];
bool g_watch_on=true; std::uint32_t g_watch_base=0,g_watch_end=kRamSize;
struct Write{std::uint32_t address,value;};std::vector<Write> writes;
void watch_hit(std::uint32_t a,std::uint32_t v,const char*){writes.push_back({a,v});}
void watch_span(std::uint32_t,std::uint32_t,const char*){throw std::runtime_error("unexpected span write");}
std::uint32_t slow_ld32(std::uint32_t){throw std::runtime_error("unexpected non-RAM read");}
std::uint16_t slow_ld16(std::uint32_t){throw std::runtime_error("unexpected non-RAM halfword read");}
void slow_st32(std::uint32_t,std::uint32_t){throw std::runtime_error("unexpected non-RAM write");}
void slow_ld128(std::uint32_t,Reg128&){throw std::runtime_error("unexpected non-RAM quad read");}
}
using namespace ps2;
void original_emitter(Cpu& c){ @EMITTER@ }
void original_fresh_load(Cpu& c){ @FRESH@ }
static void put(unsigned a,unsigned v){std::memcpy(g_ram+a,&v,4);}
int main(){
  using namespace mka::emitted;
  std::vector<unsigned char> before(kRamSize),expected(kRamSize);
  unsigned producer_cases=0,transport_cases=0;
  for(unsigned type: {0u,2u,3u,4u})for(unsigned dest:{0u,4u,31u})for(unsigned base:{0u,11u,31u})for(unsigned imm:{0u,0x8000u,0xffffu})for(unsigned capacity:{0u,1u}){
    std::memset(g_ram,0,kRamSize);Cpu c{};c.r[4].w[0]=0x40000;c.r[28].w[0]=0x6540f0;c.r[31].w[0]=0x1234;c.budget=10000;
    put(0x6540f0-21920,0x10000);put(0x10000+3736,0x20000);put(0x20000,capacity);put(0x20008,0);put(0x20018,0x30000);
    put(0x40004,dest);put(0x40008,type);put(0x4000c,0x40100);put(0x40010,imm);put(0x40104,0x40200);put(0x40204,base);
    std::memcpy(expected.data(),g_ram,kRamSize);writes.clear();original_emitter(c);
    unsigned cursor=0;std::array<unsigned,1> output{};const bool appended=append_quad_load(std::span<unsigned>(output.data(),capacity),cursor,{static_cast<QuadDestination>(type),dest,base,static_cast<unsigned short>(imm)});
    if(appended){std::memcpy(expected.data()+0x30000,output.data(),4);std::memcpy(expected.data()+0x20008,&cursor,4);}
    if(std::memcmp(expected.data(),g_ram,kRamSize)||c.pc!=0x1234||writes.size()!=(capacity?2u:0u)||
       (capacity&&(writes[0].address!=0x30000||writes[0].value!=output[0]||writes[1].address!=0x20008||writes[1].value!=1)))return 1;
    ++producer_cases;
  }
  for(unsigned seed=0;seed<256;++seed){
    Cpu original{};auto* bytes=reinterpret_cast<unsigned char*>(&original);for(unsigned i=0;i<sizeof original;++i)bytes[i]=static_cast<unsigned char>((i*29+seed*17)^seed);
    original.r[11].w[0]=0x10000+(seed&15);for(unsigned i=0;i<16;++i)g_ram[0x10000+i]=static_cast<unsigned char>(seed*23+i*7);
    Cpu compiled=original;std::memcpy(before.data(),g_ram,kRamSize);
    original_fresh_load(original);quad_load<QuadDestination::vector,4,11,0>(compiled);
    if(std::memcmp(&original,&compiled,sizeof original)||std::memcmp(before.data(),g_ram,kRamSize))return 2;
    ++transport_cases;
  }
  bool invalid=false;try{original_quad_load_word({static_cast<QuadDestination>(1),4,11,0});}catch(const std::exception&){invalid=true;}if(!invalid)return 3;
  invalid=false;try{original_quad_load_word({QuadDestination::vector,32,11,0});}catch(const std::exception&){invalid=true;}if(!invalid)return 4;
  Cpu bad{};bad.r[11].w[0]=0xffffffff;invalid=false;try{quad_load<QuadDestination::vector,4,11,0>(bad);}catch(const std::exception&){invalid=true;}if(!invalid)return 5;
  std::printf("PASS original_emitter_memory_order_cases=%u fresh_load_full_CPU_RAM_cases=%u invalid_contracts_rejected=3 full_fresh_program_E2=UNPROVED\n",producer_cases,transport_cases);
}
'''

if __name__=='__main__':
    raise SystemExit(main())
