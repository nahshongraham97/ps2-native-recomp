"""Offline IRX image/relocation admission and existing IOP AOT generation.

Retail bytes stay in an explicitly owned ignored output. No game, live process,
card, original source/image, or protected candidate is modified.
"""
from __future__ import annotations
import argparse
from collections import Counter, deque
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys

W = Path(__file__).resolve().parents[2]
ISO_SHA = '48e395b56fe6888d9013d36b4b04d2a1decf2f52259063936a8e9f53e824966d'
DONOR_SHA = '036b90011252d8508c84272867bcdd74d9ed6f6d2855c6a6191e9812f91bf881'


def require(ok, why):
    if not ok:
        raise ValueError(why)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def file_sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for data in iter(lambda: stream.read(4 << 20), b''):
            h.update(data)
    return h.hexdigest()


def load_tool(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    require(spec is not None and spec.loader is not None, 'missing_source_tool')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def u32(data, at):
    require(0 <= at <= len(data)-4, 'out_of_bounds_u32')
    return struct.unpack_from('<I', data, at)[0]


def sections(data):
    at = u32(data, 32)
    size, count, strings = struct.unpack_from('<HHH', data, 46)
    require(size >= 40 and at+size*count <= len(data) and strings < count, 'invalid_ELF_sections')
    rows = [struct.unpack_from('<IIIIIIIIII', data, at+i*size) for i in range(count)]
    string_row = rows[strings]
    names = data[string_row[4]:string_row[4]+string_row[5]]
    out = []
    for index, row in enumerate(rows):
        end = names.find(b'\0', row[0])
        require(end >= row[0], 'invalid_section_name')
        out.append({'index': index, 'name': names[row[0]:end].decode('ascii'), 'type': row[1],
                    'flags': row[2], 'VA': row[3], 'file_offset': row[4], 'size': row[5]})
    return out


def relocate(data, image, base, records):
    result = bytearray(image)
    v2 = struct.unpack_from('<H', data, 16)[0] == 0xFF81
    joins = []
    high_at = None
    high_word = None
    for index, record in enumerate(records):
        at, kind = record['offset'], record['type']
        require(at % 4 == 0 and at+4 <= len(result), 'relocation_outside_image')
        word = u32(result, at)
        after = word
        if kind == 0:
            pass
        elif kind == 2:
            after = (word+base) & 0xFFFFFFFF
        elif kind == 4:
            after = (word & ~0x03FFFFFF) | (((word & 0x03FFFFFF)+(base >> 2)) & 0x03FFFFFF)
        elif kind == 5:
            if v2:
                require(index+1 < len(records), 'unpaired_v2_HI16')
                next_at = records[index+1]['offset']
                low = struct.unpack('<h', struct.pack('<H', u32(result, next_at) & 0xFFFF))[0]
                value = (low+(word << 16)+base) & 0xFFFFFFFF
                if value & 0x8000:
                    value = (value+0x10000) & 0xFFFFFFFF
                after = (word & ~0xFFFF) | (value >> 16)
            else:
                require(high_at is None, 'multiple_unpaired_HI16_not_proved')
                high_at, high_word = at, word
        elif kind == 6:
            low = struct.unpack('<h', struct.pack('<H', word & 0xFFFF))[0]
            if not v2 and high_at is not None:
                value = (low+(high_word << 16)+base) & 0xFFFFFFFF
                after = (word & ~0xFFFF) | (value & 0xFFFF)
                adjusted = (value+0x10000) & 0xFFFFFFFF if value & 0x8000 else value
                patched_high = (high_word & ~0xFFFF) | (adjusted >> 16)
                struct.pack_into('<I', result, high_at, patched_high)
                joins.append({'offset': high_at, 'type': 'paired_HI16', 'before': f'{high_word:08X}', 'after': f'{patched_high:08X}'})
                high_at = high_word = None
            else:
                after = (word & ~0xFFFF) | ((low+base) & 0xFFFF)
        else:
            raise ValueError('unsupported_relocation_type:'+str(kind))
        struct.pack_into('<I', result, at, after)
        joins.append({'offset': at, 'type': kind, 'before': f'{word:08X}', 'after': f'{after:08X}'})
    require(high_at is None, 'unpaired_HI16')
    return result, joins


def romdir(data):
    # Source: ps2dev PS2SDK iop/fs/romdrv/include/romdrv.h (16-byte records/alignment).
    require(len(data) >= 48, 'truncated_ROMFS')
    rows, cursor, image_at = [], 0, 0
    while cursor+16 <= len(data):
        name, extra, size = struct.unpack_from('<10sHI', data, cursor)
        if not any(data[cursor:cursor+16]):
            break
        name = name.split(b'\0', 1)[0].decode('ascii')
        require(name and all(32 <= ord(c) < 127 for c in name), 'invalid_ROMDIR_name')
        require(image_at+size <= len(data), 'truncated_ROMDIR_member')
        blob = data[image_at:image_at+size]
        rows.append({'name': name, 'directory_offset': cursor, 'image_offset': image_at, 'size': size,
                     'extinfo_size': extra, 'sha256': sha(blob), 'ELF': blob[:4] == b'\x7fELF'})
        image_at = (image_at+size+15) & ~15
        cursor += 16
    require(len(rows) >= 3 and [x['name'] for x in rows[:3]] == ['RESET', 'ROMDIR', 'EXTINFO'], 'unsupported_ROMFS_header')
    require(rows[0]['size'] == 0 and rows[1]['image_offset'] == 0 and rows[1]['size'] == cursor+16,
            'ROMDIR_self_extent_mismatch')
    require(sum(x['extinfo_size'] for x in rows) <= rows[2]['size'], 'EXTINFO_extent_mismatch')
    return rows


def code_cfg(text, base, entry, imports, exports, relocations, original_image, decoder):
    """Conservative call/return CFG; dynamic edges remain explicit obligations."""
    size = len(text)
    data = set()
    stubs = {}
    roots = {entry: {'ELF_entry'}}
    for table in imports:
        data.update(range(table['image_offset'], table['image_offset']+20, 4))
        for item in table['imports']:
            stubs[item['image_offset']] = {'library': table['library'], 'version': table['version'] & 0xFFFF,
                                          'ordinal': item['ordinal']}
    for table in exports:
        at = int(table['image_offset'], 0)
        data.update(range(at, at+20+4*(len(table['exports'])+1), 4))
        for item in table['exports']:
            target = int(item['image_target'], 0)
            if target < size:
                roots.setdefault(target, set()).add('IRX_export_'+str(item['ordinal']))
    for item in relocations:
        if item['type'] == 2:
            target = u32(original_image, item['offset'])
            if target < size and target % 4 == 0 and target not in data:
                roots.setdefault(target, set()).add('relocated_text_pointer_candidate')
    # Address-taken callbacks are commonly formed by LUI/ADDIU rather than a
    # literal R_MIPS32 table word. Derive these candidates from actual paired
    # relocation records/words, never from integer-value resemblance alone.
    high = None
    for item in relocations:
        if item['type'] == 5:
            high = (item['offset'], u32(original_image, item['offset']))
        elif item['type'] == 6:
            low_word = u32(original_image, item['offset'])
            signed_low = struct.unpack('<h', struct.pack('<H', low_word & 0xFFFF))[0]
            target = (((high[1] & 0xFFFF) << 16) + signed_low) & 0xFFFFFFFF if high else signed_low & 0xFFFFFFFF
            if target < size and target % 4 == 0 and target not in data:
                roots.setdefault(target, set()).add('relocated_HI16_LO16_text_address_candidate')
            high = None
    pending = deque(roots)
    seen, delays, edges, limits = set(), set(), [], []
    while pending:
        at = pending.popleft()
        if at in seen:
            continue
        if at < 0 or at+4 > size or at % 4:
            limits.append({'offset': at, 'reason': 'static_destination_outside_selected_text'})
            continue
        if at in data:
            limits.append({'offset': at, 'reason': 'CFG_enters_declared_IRX_table_data'})
            continue
        seen.add(at)
        word = u32(text, at)
        op, rs, rt, rd, sa, fn, imm = decoder.fields(word)
        if at in stubs:
            edges.append({'from': at, 'kind': 'native_import_dependency', **stubs[at]})
            continue
        if decoder.is_branch(word):
            if at+8 > size:
                limits.append({'offset': at, 'reason': 'truncated_branch_delay'})
                continue
            delays.add(at+4)
            if decoder.is_branch(u32(text, at+4)):
                limits.append({'offset': at, 'reason': 'branch_in_delay_unproved'})
                continue
            if op == 0:
                edges.append({'from': at, 'kind': 'ordinary_JR_RA_return' if fn == 8 and rs == 31 else 'unresolved_register_destination',
                              'source_register': rs, 'delay': at+4})
                if fn == 9:
                    pending.append(at+8)
                continue
            target = decoder.static_target(word, base+at)
            require(target is not None, 'missing_static_branch_target')
            offset = (target & 0x1FFFFFFF)-base
            edges.append({'from': at, 'kind': 'direct_call' if op == 3 else 'static_branch', 'to': offset, 'delay': at+4})
            pending.append(offset)
            if op not in (2,):
                pending.append(at+8)
            continue
        emitted = '\n'.join(decoder.emit(word, base+at, False))
        if 'iopRaiseException(cpu, 10u' in emitted:
            limits.append({'offset': at, 'reason': 'reserved_or_unsupported_original_instruction', 'word': f'{word:08X}'})
            continue
        if op == 0 and fn in (12, 13):
            edges.append({'from': at, 'kind': 'architectural_exception_boundary'})
            continue
        pending.append(at+4)
    return {'roots': [{'offset': at, 'reasons': sorted(why)} for at, why in sorted(roots.items())],
            'visited_offsets': sorted(seen), 'inline_delay_offsets': sorted(delays),
            'edges': edges, 'unresolved_or_exception_frontiers': limits,
            'declared_IRX_table_data_offsets': sorted(at for at in data if at < size),
            'scope': 'Conservative original-word CFG from ELF/export/relocated-pointer roots. Dynamic register destinations remain unresolved; no named function/ABI is invented.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--iso', type=Path, required=True)
    parser.add_argument('--module', default='MODULES/SDRDRV.IRX')
    parser.add_argument('--base', type=lambda x: int(x, 0), required=True)
    parser.add_argument('--donor-generator', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--reference-capture', type=Path)
    parser.add_argument('--reference-base', type=lambda x: int(x, 0))
    args = parser.parse_args()
    require(args.output.resolve().is_relative_to(W/'build') and not args.output.exists(), 'fresh_owned_output_required')
    require(0x20000 <= args.base <= 0x700000 and args.base % 0x100 == 0, 'invalid_IOP_load_base')
    require(args.module.upper().startswith('MODULES/'), 'online_NETGUI_translation_not_admitted')
    iso_before = args.iso.stat()
    require(file_sha(args.iso) == ISO_SHA, 'original_ISO_pin_mismatch')
    require(file_sha(args.donor_generator) == DONOR_SHA, 'offline_generator_pin_mismatch')
    disc = load_tool('r192_disc', W/'scripts/disc.py')
    irx = load_tool('r192_irx', W/'scripts/iop_irx_imports.py')
    manifest = load_tool('r192_iop_manifest', W/'scripts/iop_capture_manifest.py')
    args.output.mkdir(parents=True, exist_ok=False)
    image = disc.Image(str(args.iso))
    inventory = []
    selected = None
    for name, layer, sector, size in image.files():
        if not name.upper().endswith(('.IRX', '.IMG')):
            continue
        image.file.seek(sector*2048)
        blob = image.file.read(size)
        require(len(blob) == size, 'truncated_ISO_member')
        row = {'path': name, 'layer': layer.index, 'ISO_byte_offset': sector*2048, 'size': size, 'sha256': sha(blob)}
        if name.upper().endswith('.IMG'):
            row['ROMDIR'] = romdir(blob)
            row['ROMFS_source'] = 'https://ps2dev.github.io/ps2sdk/romdrv_8h_source.html'
        else:
            parsed, load, module_name = irx._parse_elf_load_image(blob)
            row.update(module_name=module_name, load=load, sections=sections(blob),
                       entry_offset=u32(blob, 24), import_tables=len(irx.scan_import_tables(parsed)),
                       export_tables=irx.scan_export_tables(parsed),
                       relocation_kinds=dict(Counter(x['type'] for x in irx.scan_relocations(blob, len(parsed)))))
        inventory.append(row)
        if name.upper() == args.module.upper():
            require(selected is None, 'ambiguous_selected_disc_module')
            selected = (blob, row)
    require(selected is not None, 'selected_IRX_missing')
    data, selected_record = selected
    original_image, load, module_name = irx._parse_elf_load_image(data)
    require(load['virtual_address'] == 0, 'nonzero_IRX_link_VA_not_proved')
    text_sections = [s for s in sections(data) if s['flags'] & 4 and s['size']]
    require(len(text_sections) == 1 and text_sections[0]['VA'] == 0, 'noncontiguous_IRX_text_not_proved')
    text_size = text_sections[0]['size']
    relocations = irx.scan_relocations(data, len(original_image))
    relocated, joins = relocate(data, original_image, args.base, relocations)
    text = relocated[:text_size]
    imports = irx.scan_import_tables(relocated)
    original_exports = irx.scan_export_tables(original_image)
    decoder = load_tool('r192_iop_decoder', args.donor_generator)
    decoder.OVERRIDES = {}
    cfg = code_cfg(text, args.base, u32(data, 24), imports, original_exports, relocations, original_image, decoder)
    reference = None
    if args.reference_capture:
        require(args.reference_base is not None, 'reference_base_required')
        expected, _ = relocate(data, original_image, args.reference_base, relocations)
        old = args.reference_capture.read_bytes()
        reference = {'path': str(args.reference_capture.resolve()), 'sha256': sha(old),
                     'base': args.reference_base, 'original_relocated_text_sha256': sha(expected[:text_size]),
                     'exact_match': old == expected[:text_size]}
        require(reference['exact_match'], 'historical_capture_original_IRX_join_failed')
    (args.output/'selected-original.IRX').write_bytes(data)
    (args.output/'relocated-load-image.bin').write_bytes(relocated)
    captures = args.output/'captures'; captures.mkdir()
    # Match the real runtime's filename loader identity. Keep the independent
    # IOPMOD header name (SDR_DRIVER for SDRDRV.IRX) in selected_record.
    name = Path(args.module).stem.upper()
    capture = f'{name}_B{args.base:08X}_H{manifest.fnv1a64(text):016X}'
    (captures/(capture+'.bin')).write_bytes(text)
    (captures/'module-identities.txt').write_text(f'{capture} {name}\n', encoding='utf-8', newline='\n')
    (captures/'modules.txt').write_text(f'{capture} {args.base:08X} {text_size:X} {len(relocated):X} {manifest.fnv1a64(text):016X}\n', encoding='ascii', newline='\n')
    generated = args.output/'generated'
    command = [sys.executable, str(W/'scripts/iop_reference_codegen.py'), str(captures), str(args.donor_generator), str(generated)]
    result = subprocess.run(command, capture_output=True, text=True)
    (args.output/'generate.log').write_text(result.stdout+result.stderr, encoding='utf-8', newline='\n')
    require(result.returncode == 0, 'IOP_AOT_generator_failed')
    registry = json.loads((generated/'capture-registry.json').read_text())
    require(len(registry) == 1 and not registry[0]['interpreter_fallback_candidates'] and not registry[0]['unmapped_stub_candidates'],
            'generated_interpreter_fallback_or_unbound_stub')
    words = list(struct.unpack('<'+'I'*(len(text)//4), text))
    descriptor = generated/'r192_iop_descriptor.cpp'
    lines = ['#include "iop/r192_aot.hpp"', '', 'namespace ps2x::iop::detail {',
             f"std::uint32_t {registry[0]['function']}(IopCpuState &, IopMemory &, std::uint32_t);", '}',
             'namespace ps2::iop::r192_aot {', 'namespace {', 'const std::uint32_t words[] = {']
    lines += ['    '+', '.join(f'0x{x:08X}u' for x in words[i:i+8])+',' for i in range(0, len(words), 8)]
    lines += ['};', 'const Import imports[] = {']
    for table in imports:
        for entry in table['imports']:
            lines.append(f'    {{{entry["image_offset"]}u, "{table["library"]}", {table["version"] & 0xFFFF}u, {entry["ordinal"]}u}},')
    lines += ['};', 'const Module module = {"%s", %du, %du, %du, words, imports, %du, &ps2x::iop::detail::%s};' %
              (name, args.base, text_size, len(relocated), len(registry[0]['imports']), registry[0]['function']),
              '}', 'const Module &selected_module() { return module; }', '}']
    descriptor.write_text('\n'.join(lines)+'\n', encoding='utf-8', newline='\n')
    after = args.iso.stat()
    require((iso_before.st_size, iso_before.st_mtime_ns) == (after.st_size, after.st_mtime_ns), 'original_ISO_changed')
    report = {'status': 'BOUNDED_IOP_MODULE_AOT_GENERATED', 'native_credit': 0,
              'source': {'path': str(Path(__file__).resolve()), 'sha256': file_sha(Path(__file__))},
              'ISO': {'path': str(args.iso.resolve()), 'sha256': ISO_SHA}, 'disc_IOP_domain': inventory,
              'selected': selected_record, 'runtime_loader_name': name, 'base': args.base, 'text_size': text_size, 'entry': args.base+u32(data, 24),
              'all_aligned_text_entry_PC_count': len(words), 'relocation_records': relocations, 'relocation_word_joins': joins,
              'original_code_CFG': cfg,
              'historical_capture_join': reference, 'original_unlinked_imports': imports, 'generated_registry': registry,
              'generator_command': command, 'donor_generator_sha256': DONOR_SHA,
              'compiler_scope': 'Build-time specialized R3000 C++ for every aligned text PC; existing queued-load/branch/COP0 helpers. No interpreter fallback candidate admitted.',
              'limits': ['IRX import/native-service implementations are separate dependencies.', 'Dynamic indirect destinations require a matching compiled module/import; no interpreter fallback.',
                         'R3000 queued-load semantics differ from the frozen R192 immediate-load oracle at exposed load boundaries; no deployment to protected candidate.',
                         'Single-module generation is not complete IOP closure. Disc IRXs and both IOPRP domains remain explicitly inventoried.'],
              'generated_files': [{'path': str(p.resolve()), 'sha256': file_sha(p), 'size': p.stat().st_size} for p in sorted(generated.iterdir()) if p.is_file()]}
    target = args.output/'RESULT.json'
    target.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8', newline='\n')
    print(json.dumps({'result': str(target.resolve()), 'sha256': file_sha(target), 'entry': report['entry'], 'text_words': len(words),
                      'disc_IRX': sum(x['path'].endswith('.IRX') for x in inventory), 'IOPRP_images': sum(x['path'].endswith('.IMG') for x in inventory)}))


if __name__ == '__main__':
    main()
