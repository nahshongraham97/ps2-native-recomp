"""Read-only, pinned R192 v8 EE-prefix/resource inspection; never restores a state."""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import struct
from pathlib import Path

EXE_SHA = '8ab43bda05e7a621b946883f3195a583f0c2455f0351bf8608282cd3e97b4617'
ELF_SHA = 'a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f'
STATE_SHA = '7802920f2bcd2114721abb3c017e774a67b86e8e04922c2a12d01eef81ab4984'
LISTING_SHA = '4611757f817e6c4ace87bb2d1203bca9c5d8523c33e0ae3e086890ab4b5ce148'
SOURCE_PINS = {
    'runtime/src/system.cpp': '7a75bfed5758f57d4979f919ffa9526a7af337fd7a86871fb9bdcc1c6ea93384',
    'runtime/src/quick_keys.inc': 'aa8956107e66f0f8df3db79850e36959a7d10625e622e7872a3512b6e5dbb77d',
    'runtime/include/ps2/cpu.hpp': '2eb8c482859337add6480f0a1f0ad79ab908dda6d878d575f84f554ecf81c0fe',
    'runtime/include/ps2/state.hpp': 'fc49043208f3ec9f7593f0aeb55de15090637c96c3de4d036563c5d9ab0b5aac',
    'host/mka_combat_hooks.hpp': '18256dfbf9bb5d7705da992a57a102d114c2ea65d3e31c3b85992f0988921bc9',
    'host/mka_unit_wrappers.cpp': '7f4a010293e6af5e99c4429e88ff0628a48d6a3fe2f8fcd831366a72e644cfe5',
}
CPU_SIZE = 1456
RAM_SIZE = 0x2000000
RAM_OFFSET = 20 + CPU_SIZE
FRAME_OFFSET = RAM_OFFSET + RAM_SIZE + 0x4000 + 16
WORKTREE = Path(__file__).resolve().parents[2]


def owned_output(path):
    path = path.resolve()
    root = (WORKTREE/'build/oracle-telemetry').resolve()
    require(path != root and path.is_relative_to(root), 'output_outside_owned_telemetry')
    require(not path.exists(), 'output_must_be_fresh')
    return path


def require(ok, reason):
    if not ok:
        raise ValueError(reason)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def pinned(path, expected):
    data = path.read_bytes()
    require(sha(data) == expected, 'pin_mismatch:' + str(path))
    return data


def u32(data, offset):
    require(0 <= offset <= len(data) - 4, 'out_of_bounds_u32')
    return struct.unpack_from('<I', data, offset)[0]


def prefix(data, cpu_size=CPU_SIZE):
    require(cpu_size == CPU_SIZE, 'unsupported_cpu_ABI')
    require(len(data) >= FRAME_OFFSET + 8, 'truncated_fixed_EE_prefix')
    require(data[:8] == b'PS2STATE', 'wrong_magic')
    require(u32(data, 8) == 8, 'unsupported_version')
    require(data[12:20] == b'ee\0\0\0\0\0\0', 'wrong_EE_tag')
    ram = memoryview(data)[RAM_OFFSET:RAM_OFFSET + RAM_SIZE]
    gp = u32(data, 20 + 28 * 16)
    require(gp == 0x6540F0, 'unexpected_GP_not_linked_original')
    g_address = gp - 0x4EA0
    g = u32(ram, g_address)
    require(0x100000 <= g <= RAM_SIZE - 0x160 and g % 4 == 0,
            'invalid_resource_context_pointer')
    fields = {}
    for offset in (0x64, 0x68, 0x9C, 0xA0, 0xA4, 0xA8):
        fields[f'{offset:02X}'] = {
            'EE_address': f'{g + offset:08X}', 'word': f'{u32(ram, g + offset):08X}',
            'float32': struct.unpack_from('<f', ram, g + offset)[0],
        }
    return ram, {
        'RAM_file_offset': RAM_OFFSET, 'RAM_size': RAM_SIZE, 'Cpu_size': CPU_SIZE,
        'Cpu_GP': f'{gp:08X}', 'G_pointer_address': f'{g_address:08X}', 'G': f'{g:08X}',
        'saved_frame_file_offset': FRAME_OFFSET,
        'saved_frame': struct.unpack_from('<Q', data, FRAME_OFFSET)[0],
        'resource_fields': fields, 'context_bytes': {f'{x:02X}': ram[g+x] for x in (12, 13, 14)},
        'mode': u32(ram, 0x64DEA4), 'arena_gate_flags': f'{u32(ram, 0x660080):08X}',
        'arena_P1': f'{u32(ram, 0x66015C):08X}', 'arena_P2': f'{u32(ram, 0x6601A4):08X}',
        'arena_health_00660180_raw': f'{u32(ram, 0x660180):08X}',
    }


def pe_witnesses(data):
    pe = u32(data, 0x3C)
    require(data[pe:pe+4] == b'PE\0\0', 'not_PE')
    count = struct.unpack_from('<H', data, pe+6)[0]
    optional_size = struct.unpack_from('<H', data, pe+20)[0]
    opt = pe + 24
    require(struct.unpack_from('<H', data, opt)[0] == 0x20B, 'not_PE32plus')
    base = struct.unpack_from('<Q', data, opt+24)[0]
    sections = []
    for i in range(count):
        at = opt + optional_size + i * 40
        virtual_size, rva, raw_size, raw_at = struct.unpack_from('<IIII', data, at+8)
        sections.append((rva, raw_size, raw_at))
    def offset(va, size):
        for rva, raw_size, raw_at in sections:
            delta = va-base-rva
            if 0 <= delta <= raw_size-size:
                return raw_at + delta
        raise ValueError('unmapped_PE_VA')
    expected = {
        0x14005C363: '48bf5053325354415445',  # PS2STATE literal
        0x14005C383: 'c744243408000000',  # format version
        0x14005C3E7: '41b8b0050000',  # Cpu pod byte count
        0x14005C3FD: '41b800000002',  # RAM byte count
        0x14005C413: '41b800400000',  # scratchpad byte count
        0x14005C429: '41b808000000',  # base
        0x14005C43F: '41b808000000',  # given
        0x14005C455: '41b808000000',  # frame
    }
    rows = []
    for va, hexd in expected.items():
        raw = bytes.fromhex(hexd)
        at = offset(va, len(raw))
        require(data[at:at+len(raw)] == raw, 'producer_instruction_mismatch')
        rows.append({'host_PE_VA': f'{va:016X}', 'file_offset': at, 'bytes': hexd})
    return rows


def elf_reader(data):
    require(data[:5] == b'\x7fELF\x01' and data[5] == 1, 'not_ELF32_LE')
    phoff = u32(data, 28)
    phsize, phnum = struct.unpack_from('<HH', data, 42)
    segments = []
    for i in range(phnum):
        row = struct.unpack_from('<IIIIIIII', data, phoff+i*phsize)
        if row[0] == 1:
            segments.append({'offset': row[1], 'VA': row[2], 'filesz': row[4], 'memsz': row[5]})
    def read(va, size):
        for s in segments:
            delta = va-s['VA']
            if 0 <= delta <= s['filesz']-size:
                at = s['offset']+delta
                return at, data[at:at+size]
        raise ValueError('ELF_not_file_backed')
    return read, segments


def original_evidence(data, listing):
    read, segments = elf_reader(data)
    shoff = u32(data,32)
    shsize, shnum = struct.unpack_from('<HH',data,46)
    reginfo = []
    for index in range(shnum):
        at = shoff+index*shsize
        if u32(data,at+4) == 0x70000006:
            offset,size = u32(data,at+16),u32(data,at+20)
            require(size == 24 and offset+size <= len(data),'invalid_MIPS_REGINFO')
            raw=data[offset:offset+size]
            require(u32(raw,20) == 0x6540F0,'original_linker_GP_mismatch')
            reginfo.append({'section_index':index,'file_offset':offset,'bytes_LE':raw.hex(),
                            'linked_GP':f'{u32(raw,20):08X}'})
    require(len(reginfo) == 1,'ambiguous_MIPS_REGINFO')
    _, descriptor = read(0x5ACEDC,12)
    require(descriptor[0] == 4 and u32(descriptor,4) == 0x5AC1F0 and u32(descriptor,8) == 758,
            'bank4_descriptor_mismatch')
    for slot,target in ((119,0x2318F0),(120,0x2318C0),(121,0x2318B0),
                        (647,0x22D870),(656,0x22D780),(657,0x22D770)):
        require(u32(read(0x5AC1F0+4*slot,4)[1],0) == target,'bank4_target_mismatch')
    selected = [(0x217040, 0x2170D4), (0x22D780, 0x22D7A0), (0x22D870, 0x22D890),
                (0x2318B0, 0x231928), (0x4361D0, 0x4362A0),
                (0x466A70, 0x466C34), (0x50F140, 0x50F170), (0x50F6B0, 0x50F7D8)]
    lines = {}
    with listing.open(encoding='utf-8', newline='') as stream:
        for row in csv.DictReader(stream, delimiter='\t'):
            try:
                va = int(row['address'], 16)
            except ValueError:
                continue
            if any(lo <= va < hi for lo, hi in selected):
                lines[va] = row
    spans = []
    for lo, hi in selected:
        at, raw = read(lo, hi-lo)
        words = []
        for delta in range(0, len(raw), 4):
            va = lo+delta
            row = lines.get(va)
            if row:
                require(bytes.fromhex(row['bytes'].replace(' ', '')) == raw[delta:delta+4],
                        'listing_original_mismatch')
            words.append({'VA': f'{va:08X}', 'file_offset': at+delta,
                          'word': f'{u32(raw,delta):08X}',
                          'listing': row, 'listed': row is not None})
        spans.append({'start': f'{lo:08X}', 'end_exclusive': f'{hi:08X}',
                      'file_offset': at, 'sha256': sha(raw), 'words': words})
    tables = []
    for va, size in [(0x5FEA20,4), (0x5ACEDC,12)] + [(0x5AC1F0+4*i,4) for i in (119,120,121,647,656,657)]:
        at, raw = read(va,size)
        tables.append({'VA': f'{va:08X}', 'file_offset': at, 'bytes_LE': raw.hex(),
                       'u32_words': [f'{u32(raw,i):08X}' for i in range(0,size,4)]})
    return {'PT_LOAD': segments, 'MIPS_REGINFO':reginfo, 'tables': tables, 'instruction_spans': spans,
            'coverage_scope':'Selected source-chain spans, not a complete whole-function decode. Missing listing rows remain original words only.',
            'selected_words':sum(len(s['words']) for s in spans),
            'listed_original_joined_words':sum(w['listed'] for s in spans for w in s['words'])}


def self_test():
    data = bytearray(FRAME_OFFSET+8)
    data[:20] = b'PS2STATE'+struct.pack('<I',8)+b'ee\0\0\0\0\0\0'
    struct.pack_into('<I',data,20+28*16,0x6540F0)
    struct.pack_into('<I',data,RAM_OFFSET+0x64F250,0xBBFD00)
    prefix(data)
    cases = []
    def rejects(name, candidate, reason, **kwargs):
        try:
            prefix(candidate, **kwargs)
        except ValueError as exc:
            require(str(exc) == reason, 'wrong_negative_reason:'+name)
            cases.append({'name': name, 'status': 'REJECTED', 'reason': str(exc)})
            return
        raise ValueError('negative_accepted:'+name)
    bad = bytearray(data); struct.pack_into('<I',bad,8,7)
    rejects('unsupported_version',bad,'unsupported_version')
    rejects('wrong_Cpu_ABI',data,'unsupported_cpu_ABI',cpu_size=1440)
    bad = bytearray(data); struct.pack_into('<I',bad,RAM_OFFSET+0x64F250,RAM_SIZE-4)
    rejects('out_of_bounds_G',bad,'invalid_resource_context_pointer')
    rejects('truncated_prefix',data[:-1],'truncated_fixed_EE_prefix')
    bad=bytearray(data); struct.pack_into('<I',bad,20+28*16,0)
    rejects('wrong_GP',bad,'unexpected_GP_not_linked_original')
    class WrongPinnedInput:
        def read_bytes(self):
            return b'wrong bytes'
        def __str__(self):
            return 'fabricated_input'
    try:
        pinned(WrongPinnedInput(),ELF_SHA)
    except ValueError as exc:
        require(str(exc) == 'pin_mismatch:fabricated_input','wrong_pin_negative_reason')
        cases.append({'name':'wrong_input_pin','status':'REJECTED','reason':str(exc)})
    else:
        raise ValueError('negative_accepted:wrong_input_pin')
    try:
        owned_output(WORKTREE.parent/'ps2-native-recomp/build/r192-hooks/new-inspection')
    except ValueError as exc:
        require(str(exc) == 'output_outside_owned_telemetry','wrong_output_negative_reason')
        cases.append({'name':'protected_output_path','status':'REJECTED','reason':str(exc)})
    else:
        raise ValueError('negative_accepted:protected_output_path')
    return cases


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    for name in ('state','producer','source-root','elf','listing','mko-root','overlays',
                 'abi-proof-dir','capture-log','output'):
        ap.add_argument('--'+name, type=Path, required=True)
    args = ap.parse_args()
    args.output = owned_output(args.output)
    original = pinned(args.elf, ELF_SHA)
    producer = pinned(args.producer, EXE_SHA)
    witnesses = pe_witnesses(producer)
    sources = []
    for relative, expected in SOURCE_PINS.items():
        path = args.source_root/relative
        pinned(path,expected)
        sources.append({'path': str(path.resolve()), 'sha256': expected})
    abi_command = args.abi_proof_dir/'ABI-command.json'
    abi_layout = args.abi_proof_dir/'abi-layout.log'
    abi = json.loads(abi_command.read_text(encoding='utf-8'))
    require(abi['exit_code'] == 0 and abi['source_header_sha256'] == SOURCE_PINS['runtime/include/ps2/cpu.hpp'],
            'unproved_Cpu_layout')
    require('sizeof(ps2::Cpu)==1456' in abi['stdin'] and '-fsyntax-only' in abi['argv'], 'wrong_layout_query')
    require('[sizeof=1456, dsize=1456, align=16,' in abi_layout.read_text(encoding='utf-8'), 'wrong_layout_result')
    pinned(Path(abi['compiler']), abi['compiler_sha256'])
    capture_log = args.capture_log.read_bytes()
    log_lines = capture_log.decode('utf-8',errors='replace').splitlines()
    events = [i for i,line in enumerate(log_lines) if '[state] quick save ' in line and args.state.name+' (' in line]
    require(len(events) == 1, 'ambiguous_capture_completion_log')
    event = events[0]
    require(event+1 < len(log_lines) and log_lines[event+1] == '[keys] Saved (F5)', 'missing_F5_success')
    require(any('[keys] Saving (F5)...' in line for line in log_lines[max(0,event-8):event]), 'missing_F5_start')
    pinned(args.listing, LISTING_SHA)
    before = args.state.stat()
    state = pinned(args.state, STATE_SHA)
    ram, observed = prefix(state)
    page = bytes(ram[0xB14000:0xB18000])
    variants = []
    for path in sorted(args.overlays.glob('00B14000_*.bin')):
        raw = path.read_bytes()
        require(len(raw) == 0x4000, 'retained_page_wrong_size')
        variants.append({'path': str(path.resolve()), 'sha256': sha(raw),
                         'exact_page_match': raw == page,
                         'word_00B17700': f'{u32(raw,0x3700):08X}'})
    require(len(variants) == 43, 'retained_page_count_not_43')
    recovered = []
    for name, size, command_offset, command, slot in [
        ('common_mega_power_pickup_handler',24,12,0x04000291,656),
        ('common_mega_life_pickup_handler',92,80,0x04000288,647),
    ]:
        path = args.mko_root/'funcs'/name
        raw = path.read_bytes()
        require(len(raw) == size and u32(raw,command_offset) == command, 'recovered_script_command_mismatch')
        recovered.append({'path': str(path.resolve()), 'sha256': sha(raw), 'bytes': raw.hex(),
                          'native_command_offset': command_offset, 'command': f'{command:08X}',
                          'bank':4, 'serialized_ID': command&0xFFFFFF, 'resolved_slot':slot,
                          'argument_last_word': f'{u32(raw,len(raw)-4):08X}',
                          'provenance_limit':'Existing recovered script bytes, not rejoined to the original MKO container here.'})
    after = args.state.stat()
    require((before.st_size,before.st_mtime_ns) == (after.st_size,after.st_mtime_ns), 'state_changed_during_read')
    result = {
        'status':'READ_ONLY_PARTIAL_EE_PREFIX_VERIFIED',
        'scope':'Fixed v8 EE/Cpu/RAM/scratch/counters prefix only; no full restore, subsystem parsing, state editing or gameplay execution.',
        'tool':{'path':str(Path(__file__).resolve()),'sha256':sha(Path(__file__).read_bytes())},
        'inputs':{'state':str(args.state.resolve()),'state_sha256':sha(state),'state_size':len(state),
                  'producer':str(args.producer.resolve()),'producer_sha256':sha(producer),
                  'ELF':str(args.elf.resolve()),'ELF_sha256':sha(original),
                  'listing':str(args.listing.resolve()),'listing_sha256':LISTING_SHA,'sources':sources},
        'producer_PE_witnesses':witnesses,
        'Cpu_layout_query':{'command':abi,'command_json_sha256':sha(abi_command.read_bytes()),
                            'layout_log':str(abi_layout.resolve()),'layout_log_sha256':sha(abi_layout.read_bytes()),
                            'scope':'Clang syntax-only record layout, no game/object/executable build.'},
        'capture_completion':{'log':str(args.capture_log.resolve()),'log_sha256':sha(capture_log),
                              'lines':[{'line':i+1,'text':log_lines[i]} for i in range(max(0,event-4),event+2)],
                              'state_end_tag_present':state[-8:]==b'end\0\0\0\0\0',
                              'claim':'Completed F5 publication, stable pinned saved file. Full state restore remains unvalidated.'},
        'ABI':'Exact pinned source Cpu layout plus pinned producer machine_state byte counts. No offset inferred backwards from GS/VU.',
        'observed':observed,
        'page':{'EE_range':['00B14000','00B18000'],'sha256':sha(page),
                'word_00B17700':f'{u32(ram,0xB17700):08X}', 'retained_count':len(variants),
                'exact_matches':sum(v['exact_page_match'] for v in variants),'variants':variants,
                'scope':'Latest owner capture only; no historical failed-run attribution; no page ingestion.'},
        'original':original_evidence(original,args.listing), 'recovered_script_commands':recovered,
        'resource_chain':{
            'power':{'bank':4,'slot':656,'wrapper':'0022D780','writer':'00466B10',
                     'current_offset':'A0','maximum_offset':'9C','current_store_PCs':['00466B20','00466B3C','00466B50','00466B6C'],
                     'maximum_getter':'00436280','refill_condition':'byte[G+0D] & 01 != 0'},
            'life':{'bank':4,'slot':647,'wrapper':'0022D870','writer':'00466B80',
                    'current_offset':'68','maximum_offset':'64','maximum_getter':'00436290'},
            'flag':{'clear_store_PC':'0050F724','set_store_PC':'0050F7A8','target':'byte[G+0D] mask01',
                    'source':'entry0050F140 argumentA1 record u32 at+14 mask02',
                    'source_record_producer_and_current_cause':'UNPROVED'},
        },
        'findings':[
            'Resolver217040 uses bank byte, serialized low24 ID minus1, bank descriptor count and table pointer.',
            'Bank4 slot656 wrapper22D780 tails to466B10: subtract F12 from G+A0, clamp0..G+9C, then refill to max iff byte[G+D]&0x01.',
            'Bank4 slot647 wrapper22D870 tails to466B80: clamped life adjustment G+68/G+64; distinct from arena field00660180.',
            '50F140 clears G+D mask01 and sets it iff record at argumentA1+14 has mask02; mask01 is bit0, not mask02.',
            'G is opaque resource context via linkedGP-4EA0. Snapshot flags show mechanism enabled; actor/script/caller responsible is not proved.',
            'Actual pinned hook source arena predicate excludes mode8/P2null snapshot; no new hook behavior specified or implemented.',
        ],
        'limitations':[
            'No exhaustive alias/memcpy writer proof; no live-process observation, causal time series, or future assist requirements.',
            'Power label follows recovered mega_power script and original dispatch/writer/getter chain; UI magic identity still needs observed reader/UI/caller correlation.',
            'Record+14 flag producer/type remains unresolved; recovered20-byte vars cannot authenticate an out-of-bounds +14 DWORD.',
            'Header/end sentinel and stable hash do not validate variable-length subsystems or full-state restoration.',
            'No host-assist cause established, no original61/72 corpus or persistent card accessed/modified.',
        ],
        'self_tests':self_test(), 'state_unchanged_during_read':True,
    }
    args.output.mkdir(parents=True,exist_ok=False)
    target=args.output/'RESULT.json'
    target.write_text(json.dumps(result,indent=2,allow_nan=False)+'\n',encoding='utf-8',newline='\n')
    print(json.dumps({'result':str(target.resolve()),'sha256':sha(target.read_bytes()),
                      'saved_frame':observed['saved_frame'],'page_matches':result['page']['exact_matches']}))


if __name__ == '__main__':
    main()
