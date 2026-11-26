"""Bounded original EE-path VU0/VIF0 audit; no runtime or guest execution.

The original pinned analyser is rooted in actual generated entry maps. Raw
data/function references, decoder paths, constant-address stores and unknown
dynamic routes are separate evidence, never an inference from grep absence.
"""
from __future__ import annotations
import argparse
from collections import Counter
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import generate_r192_iop_aot as common
import build_r192_iop_v8_candidate as builder

W, require, sha = common.W, common.require, common.file_sha
MAIN_SHA = 'a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f'
CHANNELS = [0x10008000, 0x10009000, 0x1000a000, 0x1000b000, 0x1000b400,
            0x1000c000, 0x1000c400, 0x1000c800, 0x1000d000, 0x1000d400]
STORE_OPS = {0x28: ('SB', 1), 0x29: ('SH', 2), 0x2a: ('SWL', 4), 0x2b: ('SW', 4),
             0x2c: ('SDL', 8), 0x2d: ('SDR', 8), 0x2e: ('SWR', 4), 0x1f: ('SQ', 16),
             0x39: ('SWC1', 4), 0x3e: ('SQC2', 16), 0x3f: ('SD', 8)}

def reserve(extra=0):
    require(shutil.disk_usage(W).free > (12 << 30) + extra + (8 << 20), '12GiB_write_reserve_required')

HELPER = r'''
#include "analysis.hpp"
#include "snr2.hpp"
#include <fstream>
#include <iterator>
#include <map>
#include <cstdio>
int main(int argc,char**argv){try{
 if(argc!=6)return 2;using namespace recomp;std::ifstream f(argv[2],std::ios::binary);
 std::vector<uint8_t>bytes{std::istreambuf_iterator<char>(f),{}};
 Image image;if(std::string(argv[1])=="MWo3") {auto m=read_mwo3_module(bytes,"original");if(!m||!m->fixed)return 3;image=snr2_image(*m);}
 else image=Image::load(argv[2]);
 std::vector<uint32_t>seeds;std::ifstream in(argv[3]);std::string line;while(in>>line)seeds.push_back(std::stoul(line,nullptr,16));
 Search search{};search.whole_image=false;search.after_return=false;search.all_aligned=false;
 auto p=analyse(image,seeds,{},search);std::ofstream mask(argv[4],std::ios::binary);mask.write((const char*)p.code.data(),p.code.size());if(!mask)return 4;
 std::map<Op,size_t>counts;std::ofstream out(argv[5]);out<<"META\t"<<p.base<<'\t'<<p.end<<'\t'<<p.entries.size()<<'\t'<<p.functions.size()<<'\t'<<p.rejected.size()<<'\n';
 for(uint32_t pc=p.base;pc<p.end;pc+=4)if(p.is_code(pc)){const auto i=decode(*image.word(pc),pc);++counts[i.op];
  if(i.op==Op::Vcallms||i.op==Op::Vcallmsr)out<<"VCALL\t"<<pc<<'\t'<<i.word<<'\t'<<name(i.op)<<'\n';}
 for(const auto&[op,n]:counts)out<<"OP\t"<<name(op)<<'\t'<<n<<'\n';return 0;
}catch(const std::exception&e){std::fprintf(stderr,"REFUSED:%s\n",e.what());return 1;}}
'''


class Original:
    def __init__(self, path):
        self.path = path; self.raw = path.read_bytes(); self.mappings = []
        if self.raw[:4] == b'MWo3':
            self.base = struct.unpack_from('<I', self.raw, 8)[0]
            self.mappings = [(self.base, 0, len(self.raw))]; self.kind = 'MWo3'
        else:
            require(self.raw[:7] == b'\x7fELF\x01\x01\x01' and struct.unpack_from('<H', self.raw, 18)[0] == 8, 'ELF32_LE_MIPS_required')
            off = struct.unpack_from('<I', self.raw, 28)[0]; step, count = struct.unpack_from('<HH', self.raw, 42)
            for i in range(count):
                kind, f, va, _, size, *_ = struct.unpack_from('<8I', self.raw, off + i * step)
                if kind == 1 and size: self.mappings.append((va, f, size))
            self.kind = 'ELF'
    def read(self, va, size):
        found = [(o + va - a) for a, o, n in self.mappings if a <= va and size <= n - (va - a)]
        require(len(found) == 1, 'original_address_not_unique_file_backed')
        at = found[0]; require(at + size <= len(self.raw), 'original_file_truncated'); return self.raw[at:at + size]
    def word(self, va): return int.from_bytes(self.read(va, 4), 'little')
    def offset(self, va):
        found = [o + va - a for a, o, n in self.mappings if a <= va < a + n]
        require(len(found) == 1, 'original_mapping_unproved'); return found[0]
    def addresses(self, offset): return [a + offset - o for a, o, n in self.mappings if o <= offset < o + n]


def entry_maps(files):
    roots, detail = set(), []
    for path in files:
        text = path.read_text(); arrays = list(re.finditer(r'const std::uint16_t (\w*entries_\w+)\[(\d+)\] = \{(.*?)\};', text, re.S))
        require(len(arrays) == 1, 'one_generated_entry_array_required:' + path.name)
        a = arrays[0]; values = [int(x.strip().removesuffix('u')) for x in a[3].split(',') if x.strip()]
        require(len(values) == int(a[2]), 'generated_entry_extent')
        targets = {int(i): int(pc, 16) for i, pc in re.findall(r'case (\d+)u: goto L_([0-9a-fA-F]+);', text)}
        bases = {targets[v] - 4 * i for i, v in enumerate(values) if v}
        require(len(bases) == 1, 'generated_entry_PC_join_failed:' + path.name)
        base = bases.pop()
        for i, v in enumerate(values):
            if v:
                require(targets.get(v) == base + 4 * i and f'L_{base + 4*i:08X}:' in text, 'generated_actual_label_missing')
                roots.add(base + 4 * i)
        detail.append({'path': str(path), 'sha256': sha(path), 'base': base, 'bytes': len(values) * 4, 'active_entry_count': sum(bool(v) for v in values)})
    return roots, detail


def control(w):
    op, rs, rt, fn = w >> 26, w >> 21 & 31, w >> 16 & 31, w & 63
    return (op in (1, 2, 3, 4, 5, 6, 7, 20, 21, 22, 23) or op == 0 and fn in (8, 9) or op in (16, 17, 18) and rs == 8)


def update(regs, w):
    """Only exact low64 constants; unknown writes kill provenance."""
    op, rs, rt, rd, fn, shift = w >> 26, w >> 21 & 31, w >> 16 & 31, w >> 11 & 31, w & 63, w >> 6 & 31
    imm = w & 65535; simm = imm - 65536 if imm & 32768 else imm
    def set32(r, v): regs[r] = None if v is None else ((v & 0xffffffff) - (1 << 32) if v & 0x80000000 else v & 0xffffffff) & ((1 << 64) - 1)
    a, b = regs[rs], regs[rt]
    if op == 15: set32(rt, imm << 16)
    elif op in (8, 9): set32(rt, None if a is None else a + simm)
    elif op in (24, 25): regs[rt] = None if a is None else (a + simm) & ((1 << 64) - 1)
    elif op in (12, 13, 14): regs[rt] = None if a is None else (a & imm if op == 12 else a | imm if op == 13 else a ^ imm)
    elif op == 0 and fn in (0, 2, 3):
        v = None if b is None else ((b & 0xffffffff) << shift if fn == 0 else (b & 0xffffffff) >> shift if fn == 2 else ((b & 0xffffffff) - (1 << 32) if b & 0x80000000 else b & 0xffffffff) >> shift)
        set32(rd, v)
    elif op == 0 and fn in (0x21, 0x2d, 0x25, 0x24, 0x26, 0x23, 0x2f):
        if fn in (0x21, 0x2d, 0x25) and rs == 0: v = b
        elif fn in (0x21, 0x2d, 0x25) and rt == 0: v = a
        elif a is None or b is None: v = None
        else: v = a + b if fn in (0x21, 0x2d) else a - b if fn in (0x23, 0x2f) else a | b if fn == 0x25 else a & b if fn == 0x24 else a ^ b
        if fn in (0x21, 0x23): set32(rd, v)
        else: regs[rd] = None if v is None else v & ((1 << 64) - 1)
    elif op in STORE_OPS or op in (2, 3, 4, 5, 6, 7, 20, 21, 22, 23, 0x2f, 0x33): pass
    elif op == 0 and fn in (8, 9, 0xc, 0xd, 0xf, 0x11, 0x13, 0x18, 0x19, 0x1a, 0x1b):
        if fn == 9: regs[rd] = None
        if fn in (0x18, 0x19, 0x1a, 0x1b): regs[rd] = None
    elif op == 0: regs[rd] = None
    elif op in (16, 17, 18):
        if rs in (0, 1, 2): regs[rt] = None
        elif rs == 8: pass
        elif op == 16: regs[:] = [None] * 32
    elif op == 28: regs[:] = [None] * 32  # MMI may write several lanes/registers; no guessed scalar alias.
    else: regs[rt] = None
    regs[0] = 0


def region(address):
    # Match the pinned reference physical() exactly. Ordinary values such as
    # 0x51000001 are not KSEG aliases of the VU0 code bus.
    a = address & 0xffffffff
    if 0x80000000 <= a < 0xc0000000: a &= 0x1fffffff
    elif a >> 28 in (2, 3): a &= 0x0fffffff
    if 0x10003800 <= a < 0x10003c00: return 'VIF0_register'
    if 0x10004000 <= a < 0x10005000: return 'VIF0_FIFO'
    if 0x10008000 <= a < 0x10008100: return 'VIF0_DMA_channel'
    if 0x11000000 <= a < 0x11001000: return 'VU0_code_bus'
    if 0x11004000 <= a < 0x11005000: return 'VU0_data_bus'
    return None


def getter(image, words, pc):
    expected = [0x2c82000a, 0x10400006, None, 0x00041880, None, 0x00621821, 0x03e00008, 0x8c620000, 0x03e00008, 0x0000102d]
    if any(pc + 4 * i not in words for i in range(10)): return None
    actual = [words[pc + 4*i] for i in range(10)]
    if any(v is not None and actual[i] != v for i, v in enumerate(expected)): return None
    if actual[2] & 0xffff0000 != 0x3c020000 or actual[4] & 0xffff0000 != 0x24420000: return None
    low = actual[4] & 65535; low -= 65536 if low & 32768 else 0
    table = ((actual[2] & 65535) << 16) + low
    try: values = list(struct.unpack('<10I', image.read(table, 40)))
    except ValueError: return None
    if values != CHANNELS: return None
    return {'entry': pc, 'table': table, 'table_file_offset': image.offset(table), 'initial_table_words': [f'{x:08X}' for x in values],
            'original_words': [f'{x:08X}' for x in actual], 'span_sha256': hashlib.sha256(image.read(pc, 40)).hexdigest(),
            'contract': 'Unsigned low64 A0<10 then indexed LW; preserves other GPR exceptV0/V1; table mutation/lifetime not proved.'}


def local_constants(words, pc, window=128):
    """One bounded straight-line predecessor, not an arbitrary entry ABI."""
    start = pc
    for at in range(pc - 4, max(0, pc - window) - 1, -4):
        if at not in words: break
        if control(words[at]):
            start = at + 8; break
        start = at
    state = [None] * 32; state[0] = 0
    used = []
    for at in range(start, pc, 4):
        if at not in words or control(words[at]): return [None] * 32, []
        update(state, words[at]); used.append({'pc': at, 'word': f'{words[at]:08X}'})
    return state, used


def vif_initial_packet(words):
    commands = []; cursor = 0
    while cursor < len(words):
        command = words[cursor]; at = cursor; cursor += 1; op = (command >> 24) & 127
        # Only the original reset packet's proved no-launch grammar is admitted.
        if op not in (0, 1, 4, 5, 0x20): return {'status': 'OPEN', 'reason': 'unproved_VIF_packet_command', 'word_index': at}
        payload = 1 if op == 0x20 else 0
        if cursor + payload > len(words): return {'status': 'OPEN', 'reason': 'truncated_VIF_packet_payload'}
        commands.append({'word_index': at, 'word': f'{command:08X}', 'opcode': op, 'payload_words': payload})
        cursor += payload
    return {'status': 'INITIAL_RESET_PACKET_NO_MICRO_LAUNCH', 'commands': commands,
            'condition': 'Original constant data unchanged and VIF decoder freshly reset; no runtime immutability claim.'}


def scan(image, mask, base, roots, getters):
    words = {base + 4*i: image.word(base + 4*i) for i, b in enumerate(mask) if b}
    leaders = set(roots)
    for pc, w in words.items():
        if control(w):
            leaders.add(pc + 8)
            if w >> 26 in (1, 4, 5, 6, 7, 20, 21, 22, 23):
                immediate = w & 65535; immediate -= 65536 if immediate & 32768 else 0; leaders.add(pc + 4 + 4 * immediate)
            if w >> 26 == 2: leaders.add(((pc + 4) & 0xf0000000) | ((w & 0x3ffffff) << 2))
    regs = [None] * 32; regs[0] = 0; previous = None
    stores, calls, direct, constant_literals, indirect, control_writes, call_arguments = [], [], [], [], [], [], []
    for pc, w in sorted(words.items()):
        if pc in leaders or previous != pc - 4: regs = [None] * 32; regs[0] = 0
        op, rs, rt = w >> 26, w >> 21 & 31, w >> 16 & 31
        if op == 15 and region((w & 65535) << 16): constant_literals.append({'pc': pc, 'word': f'{w:08X}', 'literal': (w & 65535) << 16, 'not_a_store_proof': True})
        if op in STORE_OPS:
            value = regs[rs]; imm = w & 65535; imm -= 65536 if imm & 32768 else 0
            address = None if value is None else ((value & 0xffffffff) + imm) & 0xffffffff
            if address is not None and op in (0x1f, 0x3e): address &= ~15
            row = {'pc': pc, 'word': f'{w:08X}', 'file_offset': image.offset(pc), 'operation': STORE_OPS[op][0], 'width': STORE_OPS[op][1],
                   'base_register': rs, 'address': address, 'region': region(address) if address is not None else None,
                   'value_register': rt, 'source_value': regs[rt] if op not in (0x39, 0x3e) else None,
                   'scope': 'Effective address on the decoded local path if this instruction executes; no runtime reachability claim.'}
            stores.append(row)
            if row['region']: direct.append(row)
        if op == 0 and w & 63 in (8, 9) and not (w & 63 == 8 and rs == 31): indirect.append({'pc': pc, 'word': f'{w:08X}', 'register': rs})
        if op == 18 and rs == 6 and (w >> 11 & 31) in (27, 28, 31):
            local, proof = local_constants(words, pc)
            control_writes.append({'pc': pc, 'word': f'{w:08X}', 'control_register': w >> 11 & 31, 'source_register': rt,
                                   'source_value': local[rt], 'local_original_context': proof,
                                   'scope': 'Original CTC2 path; CMSAR0 config/FBRST reset/CMSAR1 VU1 route distinguished by pinned runtime source.'})
        if op in (2, 3):
            target = ((pc + 4) & 0xf0000000) | ((w & 0x3ffffff) << 2)
            local, proof = local_constants(words, pc)
            local[31] = pc + 8
            if pc + 4 in words and not control(words[pc + 4]): update(local, words[pc + 4])
            else: local = [None] * 32
            for argument in range(4, 8):
                if local[argument] is not None and region(local[argument]):
                    call_arguments.append({'pc': pc, 'word': f'{w:08X}', 'target': target, 'argument_register': argument,
                        'value': local[argument], 'region': region(local[argument]), 'original_context': proof,
                        'delay_word': f'{words[pc+4]:08X}' if pc + 4 in words else None,
                        'status': 'address-like literal passed to call; callee read/write/launch role OPEN'})
            if target in getters:
                at_call = local; delay = words.get(pc + 4)
                calls.append({'pc': pc, 'word': f'{w:08X}', 'delay_pc': pc + 4, 'delay_word': f'{delay:08X}' if delay is not None else None,
                    'target': target, 'A0_low64': at_call[4], 'kind': 'JAL' if op == 3 else 'J_tail',
                    'original_local_context': proof,
                    'proof': 'Bounded original straight-line predecessor plus executed delay; alternate interior entries/indirect routes remain OPEN.'})
        update(regs, w); previous = pc
        # Do not propagate a guessed ABI or state through a call/control bundle.
        if pc - 4 in words and control(words[pc - 4]): regs = [None] * 32; regs[0] = 0
    return {'words': words, 'stores': stores, 'target_stores': direct, 'getter_calls': calls, 'indirect': indirect,
            'address_like_literals': constant_literals, 'control_writes': control_writes, 'address_like_call_arguments': call_arguments}


def template_bits(context, call_pc):
    """Original raw-word construction only, not a fabricated ABI binding."""
    state = [(0, 0)] * 32; state[0] = (0xffffffff, 0)
    def combine(a, b, is_or):
        ma, va = a; mb, vb = b
        ones = ((va & ma) | (vb & mb)) if is_or else ((va & ma) & (vb & mb))
        zeros = (((~va) & ma) & ((~vb) & mb)) if is_or else (((~va) & ma) | ((~vb) & mb))
        return (ones | zeros) & 0xffffffff, ones & 0xffffffff
    reset_after_delay = False
    for row in context:
        w = int(row['word'], 16); op, rs, rt, rd, fn, s = w >> 26, w >> 21 & 31, w >> 16 & 31, w >> 11 & 31, w & 63, w >> 6 & 31
        was_delay = reset_after_delay
        if op == 3:
            reset_after_delay = int(row['pc'], 16) != call_pc
            continue  # Observe the final append argument AFTER its delay, before callee effects.
        elif op == 15: state[rt] = (0xffffffff, (w & 65535) << 16)
        elif op in (12, 13): state[rt] = combine(state[rs], (0xffffffff, w & 65535), op == 13)
        elif op == 0 and fn == 0: state[rd] = (((state[rt][0] << s) | ((1 << s) - 1)) & 0xffffffff, (state[rt][1] << s) & 0xffffffff)
        elif op == 0 and fn in (0x24, 0x25): state[rd] = combine(state[rs], state[rt], fn == 0x25)
        elif op == 0 and fn in (0x21, 0x2d) and (rs == 0 or rt == 0): state[rd] = state[rt if rs == 0 else rs]
        elif op in STORE_OPS: pass
        elif op == 0: state[rd] = (0, 0)
        else: state[rt] = (0, 0)
        if was_delay: state = [(0, 0)] * 32; reset_after_delay = False
        state[0] = (0xffffffff, 0)
    mask, value = state[4]
    compatible = [name for name, pattern in [('VCALLMS', 0x4a000038), ('VCALLMSR', 0x4a000039)] if not ((value ^ pattern) & mask & 0xfe00003f)]
    return {'A0_low32_known_mask': f'{mask:08X}', 'A0_low32_known_value': f'{value:08X}', 'compatible_VCALL_encodings': compatible,
            'scope': 'Per-site original OR/shift/literal low32 bit proof; no producer-block or whole grammar closure inferred.'}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('generation-proof', 'generator-stage', 'producer-proof', 'elf', 'compiler', 'output'):
        p.add_argument('--' + name, type=Path, required=True)
        if name.endswith('proof'): p.add_argument('--expected-' + name + '-sha', required=True)
    args = p.parse_args()
    for key, value in vars(args).items():
        if isinstance(value, Path): setattr(args, key, value.resolve())
    require(args.output.is_relative_to(W / 'build/oracle-telemetry') and not args.output.exists(), 'fresh_owned_output_required')
    require(sha(args.generation_proof) == args.expected_generation_proof_sha and sha(args.producer_proof) == args.expected_producer_proof_sha, 'receipt_pin_mismatch')
    generation = json.loads(args.generation_proof.read_text()); stage_path = args.generator_stage / 'RESULT.json'; stage = json.loads(stage_path.read_text())
    require(generation['status'] == 'OFFLINE_DISC_IMAGE_GENERATION_PASS' and sha(args.elf) == MAIN_SHA and sha(args.compiler) == stage['compiler_sha256'], 'original_generation_identity_required')
    require(generation['input_pins'].get(str(stage_path)) == sha(stage_path), 'generator_stage_not_original_compile_input')
    source_root = args.generator_stage / 'recomp'; normal = W / 'build/r192-compiled-konquest/src'
    source_files = [source_root / (name + suffix) for name in ('analysis', 'elf', 'r5900', 'snr2') for suffix in ('.cpp', '.hpp')]
    for path in source_files:
        expected = stage['source_pins'].get('src\\recomp\\' + path.name)
        if path.name == 'snr2.cpp': expected = stage['patched_reader_sha256']
        require(sha(path) == expected, 'original_generator_source_changed:' + path.name)
    original_paths = {'SLUS': args.elf, **{name.split('.')[0]: Path(row['copy']) for name, row in generation['original_copies'].items()}}
    groups = {'SLUS': [x for x in generation['generated_files']['NETR'] if re.fullmatch(r'unit_[0-9A-Fa-f]+\.cpp', Path(x['path']).name)],
              'NTGUI': [x for x in generation['generated_files']['NTGUI'] if re.fullmatch(r'unit_[0-9A-Fa-f]+\.cpp', Path(x['path']).name)]}
    for name in ('MOVIER', 'GAMER', 'NETR'): groups[name] = [x for x in generation['generated_files']['NETR'] if re.fullmatch('m_' + name + r'_unit_[0-9A-Fa-f]+\.cpp', Path(x['path']).name)]
    pins = [{'path': str(x), 'sha256': sha(x)} for x in [Path(__file__).resolve(), args.generation_proof, args.producer_proof, stage_path, args.compiler, *source_files,
        source_root / 'codegen.cpp', normal / 'runtime/include/ps2/os.hpp', normal / 'runtime/src/memory.cpp', normal / 'runtime/src/vif.cpp',
        normal / 'runtime/src/dmac.cpp', normal / 'runtime/src/cop.cpp']]
    require(sha(source_root / 'codegen.cpp') == stage['source_pins']['src\\recomp\\codegen.cpp'], 'original_instruction_emitter_changed')
    for name, rows in groups.items():
        require(rows, 'image_generated_source_missing:' + name)
        for row in rows: require(sha(Path(row['path'])) == row['sha256'], 'generated_source_changed'); pins.append({'path': row['path'], 'sha256': row['sha256']})
    for name, path in original_paths.items():
        expected = MAIN_SHA if name == 'SLUS' else generation['original_copies'][name + ('.ELF' if name == 'NTGUI' else '.OVL')]['sha256']
        require(sha(path) == expected, 'original_image_changed'); pins.append({'path': str(path), 'sha256': expected})
    reserve(4 << 20); args.output.mkdir(parents=True); (args.output / 'source-archive').mkdir(); (args.output / 'source-archive' / Path(__file__).name).write_bytes(Path(__file__).read_bytes())
    helper = args.output / 'original_decoder_audit.cpp'; helper.write_text(HELPER, encoding='utf-8', newline='\n'); exe = args.output / 'original_decoder_audit.exe'
    cmd = [str(args.compiler), '-std=c++20', '-O2', '-static-libgcc', '-static-libstdc++', '-I' + str(source_root), '-I' + str(normal / 'runtime/include'),
           str(helper), *[str(x) for x in source_files if x.suffix == '.cpp'], '-o', str(exe)]
    require(shutil.disk_usage(W).free > (12 << 30) + (128 << 20), 'reserved_disk_space_required')
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=180); (args.output / 'build.log').write_text(result.stdout + result.stderr, encoding='utf-8', newline='\n')
    require(result.returncode == 0, 'original_analyser_compile_failed')
    summaries, details = {}, {}; getters_by_image = {}
    for name in ('SLUS', 'MOVIER', 'GAMER', 'NETR', 'NTGUI'):
        image = Original(original_paths[name]); roots, maps = entry_maps([Path(x['path']) for x in groups[name]])
        reserve(12 << 20)
        seeds = args.output / (name + '-entries.txt'); seeds.write_text('\n'.join(f'{a:08X}' for a in sorted(roots)) + '\n', encoding='ascii', newline='\n')
        mask_path, meta_path = [args.output / (name + suffix) for suffix in ('.code-mask.bin', '.decode.tsv')]
        argv = [str(exe), image.kind, str(image.path), str(seeds), str(mask_path), str(meta_path)]
        run = subprocess.run(argv, capture_output=True, text=True, timeout=180); require(run.returncode == 0, 'original_code_mask_refused:' + name)
        lines = [line.split('\t') for line in meta_path.read_text().splitlines()]; meta = lines[0]; base, end = map(int, meta[1:3]); mask = mask_path.read_bytes()
        require(len(mask) * 4 == end - base, 'decoder_mask_extent')
        words = {base + 4*i: image.word(base + 4*i) for i, b in enumerate(mask) if b}
        local_getters = {}
        for pc in words:
            found = getter(image, words, pc)
            if found: local_getters[pc] = found
        getters_by_image[name] = local_getters
        own_getters = dict(getters_by_image.get('SLUS', {})) if name not in ('SLUS', 'NTGUI') else {}
        own_getters.update(local_getters)
        analysis = scan(image, mask, base, roots, own_getters)
        vcall = [{'pc': int(x[1]), 'word': f'{int(x[2]):08X}', 'operation': x[3], 'file_offset': image.offset(int(x[1]))} for x in lines if x[0] == 'VCALL']
        unknown = [s for s in analysis['stores'] if s['address'] is None]
        inventory = args.output / (name + '-unknown-stores.tsv')
        inventory.write_text('pc\tword\top\tbase_register\n' + ''.join(f'{x["pc"]:08X}\t{x["word"]}\t{x["operation"]}\t{x["base_register"]}\n' for x in unknown), encoding='ascii', newline='\n')
        indirect_path = args.output / (name + '-indirect-sites.tsv')
        indirect_path.write_text('pc\tword\tregister\n' + ''.join(f'{x["pc"]:08X}\t{x["word"]}\t{x["register"]}\n' for x in analysis['indirect']), encoding='ascii', newline='\n')
        row = {'image': name, 'input_sha256': sha(image.path), 'mapping': 'ELF PT_LOAD or original MWo3 VA-base', 'generated_units': maps,
            'generated_active_entry_roots': len(roots), 'reconstructed_code_words': len(words), 'mask_sha256': sha(mask_path),
            'VCALLMS_VCALLMSR_paths': vcall, 'generated_vcallms_helpers': sum(Path(x['path']).read_text().count('vcallms(') for x in groups[name]),
            'direct_target_stores': analysis['target_stores'], 'DMA_getter_calls': analysis['getter_calls'], 'indirect_transfer_count': len(analysis['indirect']),
            'unknown_store_count': len(unknown), 'unknown_store_inventory': {'path': str(inventory), 'sha256': sha(inventory)},
            'unknown_store_base_register_counts': dict(sorted(Counter(s['base_register'] for s in unknown).items())),
            'indirect_inventory': {'path': str(indirect_path), 'sha256': sha(indirect_path)},
            'address_like_literals_not_writer_proofs': analysis['address_like_literals'], 'decoder_run': argv,
            'CTC2_control_register_writes': analysis['control_writes'], 'address_like_call_arguments': analysis['address_like_call_arguments'],
            'open': ['Unrepresented code, indirect targets, dynamic producers and unknown stores remain OPEN; no global absence inferred.']}
        details[name] = row; summaries[name] = {k: row[k] for k in ('generated_active_entry_roots', 'reconstructed_code_words', 'unknown_store_count', 'indirect_transfer_count', 'generated_vcallms_helpers')}
        summaries[name].update(VCALLMS_VCALLMSR_count=len(vcall), direct_target_store_count=len(analysis['target_stores']), DMA_getter_calls=len(analysis['getter_calls']))
        # Prove the two observed FIFO SQ operands originate from original LQ
        # literals; unknown loads/aliases stay unproved instead of becoming0.
        packet_parts = []
        for store in analysis['target_stores']:
            if store['region'] != 'VIF0_FIFO' or store['operation'] != 'SQ': continue
            for at in range(store['pc'] - 4, max(base, store['pc'] - 32), -4):
                if at not in words: break
                q = words[at]
                if q >> 26 == 0x1e and q >> 16 & 31 == store['value_register']:
                    state, trace = local_constants(words, at); origin = state[q >> 21 & 31]
                    if origin is not None:
                        imm = q & 65535; imm -= 65536 if imm & 32768 else 0; address = ((origin + imm) & 0xffffffff) & ~15
                        try: payload = list(struct.unpack('<4I', image.read(address, 16)))
                        except ValueError: break
                        packet_parts.append({'store_pc': store['pc'], 'load_pc': at, 'load_word': f'{q:08X}', 'original_context': trace,
                            'source_address': address, 'source_file_offset': image.offset(address), 'initial_words': [f'{x:08X}' for x in payload]})
                    break
                if q >> 16 & 31 == store['value_register'] and q >> 26 not in STORE_OPS and not control(q): break
        row['VIF0_FIFO_initial_literal_parts'] = packet_parts
        if len(packet_parts) == 2 and packet_parts[1]['source_address'] == packet_parts[0]['source_address'] + 16:
            row['VIF0_FIFO_initial_packet'] = vif_initial_packet([int(w, 16) for p in packet_parts for w in p['initial_words']])
    producer = json.loads(args.producer_proof.read_text()); record = Path(producer['record']['path'])
    require(sha(record) == producer['record']['sha256'], 'original_producer_record_changed'); pins.append({'path': str(record), 'sha256': sha(record)})
    main_image = Original(args.elf); producer_sites = []
    for function in json.loads(record.read_text()):
        for site in function.get('raw_append_sites', []):
            require(all(main_image.word(int(x['pc'], 16)) == int(x['word'], 16) for x in site['original_context']), 'producer_original_context_word_mismatch')
            call_pc = int(site['pc'], 16); start = max(int(function['span'][0], 16), call_pc - 256)
            for at in range(call_pc - 4, start - 1, -4):
                if control(main_image.word(at)): start = at + 8; break
            extended = [{'pc': f'{at:08X}', 'word': f'{main_image.word(at):08X}'} for at in range(start, call_pc + 8, 4)]
            producer_sites.append({'function': function['entry'], 'pc': site['pc'], 'classification': site['classification'],
                                   'original_context': site['original_context'], 'extended_original_block': extended,
                                   'bit_proof': template_bits(extended, call_pc)})
    # Raw function values are data references, not calls. Each image keeps its
    # own VA/file mapping; overlays may reference the main getter explicitly.
    for name, row in details.items():
        image = Original(original_paths[name]); targets = set(getters_by_image[name])
        if name != 'NTGUI': targets.update(getters_by_image.get('SLUS', {}))
        refs = []
        for offset in range(0, len(image.raw) - 3, 4):
            value = struct.unpack_from('<I', image.raw, offset)[0]
            if value in targets: refs.append({'file_offset': offset, 'mapped_addresses': image.addresses(offset), 'value': f'{value:08X}',
                                               'status': 'raw function-reference candidate, not an indirect call proof'})
        row['raw_getter_function_value_references'] = refs
    # Inspect the actual callee use before turning an address-like argument
    # into a destination. Both selected examples store it as DATA into an
    # independently loaded cursor; neither proves a direct VU0 bus writer.
    argument_uses = []
    for name, start, expected, role in [
        ('SLUS', 0x30e408, [0x50600006, 0x8f83ab38, 0xac640000, 0x8f83ab3c, 0x24630004, 0xaf83ab3c],
         'A0 is the SW value at0030E410; destination R3 is loaded from GP cursor at0030E360/0030E404, not derived from A0. The11000000 argument is packet data; destination/consumers remain OPEN.'),
        ('NTGUI', 0x1c9408, [0x8c820000, 0xac450000, 0x24420004, 0x03e00008, 0xac820000],
         'A1 is the SW value at001C940C; destination is R2 loaded from0(A0). JR delay advances the cursor.51000001 is appended data, not a VU0 bus address.')]:
        image = Original(original_paths[name]); actual = [image.word(start + 4*i) for i in range(len(expected))]
        require(actual == expected, 'selected_argument_use_original_words_changed:' + name)
        target = 0x30e360 if name == 'SLUS' else start
        label = f'L_{target:08X}:'; matches = []
        for unit in groups[name]:
            text = Path(unit['path']).read_text(); at = text.find(label)
            if at >= 0: matches.append((unit, text, at))
        require(len(matches) == 1, 'selected_argument_callee_source_not_unique')
        unit, text, at = matches[0]; end = text.find(f'L_{(0x30e430 if name == 'SLUS' else 0x1c941c):08X}:', at)
        # Some adjacent functions share a fallthrough region without a label;
        # retain a bounded exact source excerpt rather than invent its extent.
        fragment = text[at:end] if at < end <= at + 12000 else text[at:at + 1200]
        argument_uses.append({'image': name, 'callee_entry': f'{target:08X}', 'original_span_start': f'{start:08X}',
            'original_words': [{'pc': f'{start+4*i:08X}', 'file_offset': image.offset(start+4*i), 'word': f'{w:08X}'} for i,w in enumerate(actual)],
            'generated_source': unit, 'generated_fragment': fragment, 'fragment_sha256': hashlib.sha256(fragment.encode()).hexdigest(),
            'finding': role, 'scope': 'Selected original argument-use proof; unknown buffer destination and subsequent launch consumers remain OPEN.'})
    controls = []
    require(region(0x51000001) is None and region(0xffffffff90008000) == 'VIF0_DMA_channel' and region(0x11000000) == 'VU0_code_bus', 'reference_physical_alias_control_failed')
    controls.append({'name': 'ordinary_data_not_a_KSEG_alias', 'status': 'PASS'})
    for name, words_, pc, expected in [('delay_argument', {0x1000: 0x0c04091e, 0x1004: 0x24040000}, 0x1000, 0),
                                       ('literal_argument', {0x1000: 0x24040002, 0x1004: 0x0c04091e, 0x1008: 0}, 0x1004, 2)]:
        # Decoder control mechanics are separately tested without reading any
        # synthetic instruction as original-file evidence.
        regs = [None] * 32; regs[0] = 0
        for at in sorted(words_):
            if at == pc:
                update(regs, words_[at + 4]); break
            update(regs, words_[at])
        require(regs[4] == expected, 'delay_slot_argument_control_failed'); controls.append({'name': name, 'status': 'PASS'})
    regs = [None] * 32; regs[0] = 0; regs[2] = 0x11000000
    update(regs, 0x70001009)  # Unknown MMI destination/provenance must kill constants.
    require(regs[2] is None, 'MMI_stale_constant_control_failed'); controls.append({'name': 'unknown_MMI_kills_stale_address', 'status': 'PASS'})
    planted = [{'pc': f'{0x1000+4*i:08X}', 'word': f'{word:08X}'} for i, word in enumerate((0x3c024a00, 0x34440038, 0x0c0bbff0, 0))]
    require(template_bits(planted, 0x1008)['compatible_VCALL_encodings'] == ['VCALLMS'], 'planted_VCALL_template_hidden')
    controls.append({'name': 'planted_VCALL_instruction_template_remains_possible', 'status': 'PASS'})
    changed = [x['path'] for x in pins if sha(Path(x['path'])) != x['sha256']]
    report = {'status': 'BOUNDED_ORIGINAL_VU0_ROUTE_AUDIT_COMPLETE' if not changed else 'INPUT_CHANGED', 'classification': 'source-only route evidence; no runtime/ownership/native credit',
        'native_credit': 0, 'source_pins': pins, 'build_command': cmd, 'helper_sha256': sha(exe), 'summary': summaries, 'images': details,
        'original_DMA_getters': {name: list(values.values()) for name, values in getters_by_image.items()}, 'input_changes': changed,
        'selected_address_like_argument_uses': argument_uses,
        'known_original_producer': {'proof': str(args.producer_proof), 'status': producer['status'], 'raw_append_sites': producer_sites,
            'unresolved': producer['unresolved'], 'whole_dynamic_opcode_universe': 'OPEN: appended constants/data are not executable words by default'}, 'controls': controls,
        'coverage_rule': 'Actual generated nonzero entry IDs are joined to emitted labels, then pinned original analyser/decoder computes instruction paths with whole-image heuristic scanning disabled. Raw code/data magic is not code admission.',
        'limits': ['Reconstructed decoder paths are source-discovery evidence, not executed paths or proof all original indirect/dynamic code has been found.',
            'Direct store constants require local original basic-block definitions; unbound writes are retained in inventories.',
            'DMA channel table bytes are original initial data, not proof of immutable runtime table or a channel launch.',
            'Address-like data constants such as11000000 used inside VIF packets are not direct VU0 code-bus writers.',
            'No game/VU/EE instruction execution, model, GUI, protected writes, rule or native-credit changes.']}
    reserve(4 << 20)
    path = args.output / 'RESULT.json'; path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'result': str(path), 'sha256': sha(path), 'status': report['status'], 'summary': summaries})); return 0 if not changed else 1


if __name__ == '__main__': raise SystemExit(main())
