"""Build relocatable IOP C++ from original IRX relocation records, offline.

No address-looking integer is promoted to a pointer. Only ELF relocation sites
and producer-owned PC expressions depend on the selected runtime load base.
Outputs contain private original bytes and must remain in an owned build dir.
"""
from __future__ import annotations
import argparse
import io
import json
from pathlib import Path
import re
import struct
import subprocess
import sys

import generate_r192_iop_aot as fixed

W = fixed.W
require = fixed.require


def relocation_program(data, image, records, text_size):
    """Validate source records and retain the loader's original addends."""
    require(struct.unpack_from('<H', data, 16)[0] == 0xFF80,
            'PIC_ELF_type_not_proved')
    scanner = fixed.load_tool('r192_pic_source_relocations', W / 'scripts/iop_irx_imports.py')
    require(records == scanner.scan_relocations(data, len(image)), 'PIC_invented_or_changed_relocation_record')
    by_site, pending, joins = {}, None, []
    for record in records:
        at, kind = record['offset'], record['type']
        require(at % 4 == 0 and at + 4 <= len(image), 'PIC_relocation_out_of_bounds')
        require(at not in by_site, 'PIC_duplicate_relocation_site')
        word = fixed.u32(image, at)
        op = word >> 26
        if kind == 0:
            continue
        row = {'offset': at, 'type': kind, 'original_word': f'{word:08X}'}
        by_site[at] = row
        if kind == 2:
            row['addend'] = word
            row['role'] = 'relocated_literal_data'
        elif kind == 4:
            require(at < text_size and op in (2, 3), 'PIC_J26_opcode_mismatch')
            row['addend'] = (word & 0x03FFFFFF) << 2
            require(row['addend'] < text_size and row['addend'] % 4 == 0,
                    'PIC_J26_target_not_compiled_text')
        elif kind == 5:
            require(pending is None and op == 15, 'PIC_HI16_pair_or_opcode_mismatch')
            pending = row
        elif kind == 6:
            require(op in (9, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,
                           0x28, 0x29, 0x2B), 'PIC_LO16_opcode_not_proved')
            low = struct.unpack('<h', struct.pack('<H', word & 0xFFFF))[0]
            value = low if pending is None else ((int(pending['original_word'], 16) & 0xFFFF) << 16) + low
            row['addend'] = value & 0xFFFFFFFF
            if pending is not None:
                pending['addend'] = row['addend']
                pending['paired_offset'] = at
                row['paired_offset'] = pending['offset']
                pending = None
        else:
            raise ValueError('PIC_unsupported_relocation_type:' + str(kind))
        joins.append(row)
    require(pending is None, 'PIC_unpaired_HI16')
    return by_site, joins


def generate_body(image, text_size, records, elf, donor, name, frozen_v8_cpu=None):
    sites, joins = relocation_program(elf, image, records, text_size)
    generator = fixed.load_tool('r192_pic_donor', donor)
    generator.OVERRIDES = {}
    if frozen_v8_cpu is not None:
        import r192_iop_v8_semantics
        r192_iop_v8_semantics.install(generator, frozen_v8_cpu)
    original_emit, original_branch = generator.emit, generator.emit_branch
    words = struct.unpack('<' + 'I' * (text_size // 4), image[:text_size])
    applied = set()

    def emit(word, address, delay):
        lines = original_emit(word, address, delay)
        row = sites.get(address)
        if row and row['type'] in (5, 6):
            require(row['original_word'] == f'{word:08X}', 'PIC_original_word_join_failed')
            op, rs, rt, _rd, _sa, _fn, imm = generator.fields(word)
            value = row['addend']
            if row['type'] == 5:
                before = f'0x{(imm << 16) & 0xFFFFFFFF:x}u'
                after = f'((moduleBase + 0x{value:x}u + 0x8000u) & 0xFFFF0000u)'
            else:
                before = f'{generator.g(rs)} + 0x{generator.sext16(imm) & 0xFFFFFFFF:x}u'
                after = (f'{generator.g(rs)} + static_cast<uint32_t>(static_cast<int32_t>('
                         f'static_cast<int16_t>((moduleBase + 0x{value:x}u) & 0xFFFFu)))')
            body = '\n'.join(lines)
            require(before in body, 'PIC_relocation_expression_join_failed')
            lines = body.replace(before, after).splitlines()
            applied.add(address)
        guard = ['    if (n >= budget) {', f'        cpu.pc = seg | 0x{address:x}u;']
        if delay:
            guard += ['        cpu.branchPending = true;',
                      f'        cpu.branchTarget = tk ? tg : (seg | 0x{address + 4:x}u);']
        return guard + ['        stop = true;', '        return n;', '    }'] + lines

    def branch(word, address):
        op, rs, rt, rd, _sa, fn, imm = generator.fields(word)
        link = f'seg | 0x{address + 8:x}u'
        if op == 0 and fn == 9:
            lines = ['    ++n;', f'    tk = true; tg = {generator.g(rs)};'] + generator.commit(rd)
            if rd:
                lines.append(f'    cpu.gpr[{rd}] = {link};')
        elif op == 1 and rt in (16, 17):
            cmp = '< 0' if rt == 16 else '>= 0'
            target = (address + 4 + (generator.sext16(imm) << 2)) & 0x1FFFFFFF
            lines = ['    ++n;', f'    tk = static_cast<int32_t>({generator.g(rs)}) {cmp}; tg = seg | 0x{target:x}u;']
            lines += generator.commit(31) + [f'    cpu.gpr[31] = {link};']
        else:
            lines = original_branch(word, address)
        row = sites.get(address)
        if row and row['type'] == 4:
            require(row['original_word'] == f'{word:08X}', 'PIC_J26_original_join_failed')
            before = f'(seg & 0xF0000000u) | 0x{row["addend"]:x}u'
            after = f'(seg & 0xF0000000u) | ((moduleBase + 0x{row["addend"]:x}u) & 0x0FFFFFFFu)'
            body = '\n'.join(lines)
            require(before in body, 'PIC_J26_expression_join_failed')
            lines = body.replace(before, after).splitlines()
            applied.add(address)
        return ['    if (n >= budget) {', f'        cpu.pc = seg | 0x{address:x}u;',
                '        stop = true;', '        return n;', '    }'] + lines

    generator.emit, generator.emit_branch = emit, branch
    out = io.StringIO()
    function = generator.gen_module(name, 0, words, out)
    body = out.getvalue()
    if frozen_v8_cpu is not None and 'std::fprintf' in body:
        body = '#include <cstdio>\n' + body
    # Identical branch-budget/delay continuation correction used by the pinned
    # fixed-base wrapper; it does not reinterpret any original instruction.
    for i in range(1, len(words)):
        prev = words[i - 1]
        stub = prev == 0x03E00008 and words[i] & 0xFFFF0000 == 0x24000000
        if not generator.is_branch(prev) or stub:
            continue
        body = body.replace(f'goto L{i:x};', f'goto D{i:x};')
        label = f'D{i:x}:\n'
        at = body.find(label)
        if at < 0:
            continue
        ends = [p for p in (body.find('\nL', at + len(label)), body.find('\nD', at + len(label))) if p >= 0]
        end = min(ends) if ends else len(body)
        first = body.find('    cpu.pc = seg | ', at + len(label), end)
        last = body.rfind('    cpu.pc = seg | ', at + len(label), end)
        require(first >= 0 and last > first and '    ++n;' in body[first:last], 'PIC_delay_resume_join_failed')
        body = body[:last] + ('    if (cpu.branchPending) {\n'
                             '        cpu.pc = cpu.branchTarget;\n'
                             '        cpu.branchPending = false;\n'
                             '        goto dispatch;\n    }\n') + body[last:]
    expected = {at for at, row in sites.items() if at < text_size and row['type'] in (4, 5, 6)}
    require(applied == expected, 'PIC_unconsumed_text_relocations')
    # PC/return/fault expressions are emitted by the compiler, not source
    # integer immediates. Each stays relative to the linked image at runtime.
    body = re.sub(r'seg \| 0x([0-9a-f]+)u', lambda m: f'seg | ((moduleBase + 0x{m[1]}u) & 0x1FFFFFFFu)', body)
    body = re.sub(r'\(cpu.pc & 0x1FFFFFFFu\) - 0x([0-9a-f]+)u',
                  lambda m: f'(cpu.pc & 0x1FFFFFFFu) - moduleBase - 0x{m[1]}u', body)
    body = body.replace('const uint32_t seg, bool &stop)', 'const uint32_t seg, bool &stop, const uint32_t moduleBase)')
    body = body.replace('uint32_t, uint32_t, bool &);', 'uint32_t, uint32_t, bool &, uint32_t);')
    body = body.replace('budget - n, seg, stop);', 'budget - n, seg, stop, moduleBase);')
    body = body.replace(f'{function}(IopCpuState &cpu, IopMemory &memory, uint32_t budget)',
                        f'{function}(IopCpuState &cpu, IopMemory &memory, uint32_t budget, uint32_t moduleBase)')
    require('const uint32_t moduleBase' in body and 'bool &, uint32_t)' in body,
            'PIC_base_parameter_ABI_join_failed')
    scanner = fixed.load_tool('r192_pic_data_tables', W / 'scripts/iop_irx_imports.py')
    data_offsets = {at for at, row in sites.items() if at < text_size and row['type'] == 2}
    for table in scanner.scan_import_tables(image):
        data_offsets.update(range(table['image_offset'], table['image_offset'] + 20, 4))
    for table in scanner.scan_export_tables(image):
        at = int(table['image_offset'], 0)
        data_offsets.update(range(at, at + 20 + 4 * (len(table['exports']) + 1), 4))
    for at in sorted(data_offsets):
        if at >= text_size:
            continue
        for prefix in ('L', 'D'):
            label = f'{prefix}{at // 4:x}:\n'
            body = body.replace(label, label + f'    cpu.pc = seg | (moduleBase + 0x{at:x}u); stop = true; return n; // ELF/IRX data is not executable admission.\n')
    return body, function, joins


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--original-irx', type=Path, required=True)
    parser.add_argument('--expected-irx-sha', required=True)
    parser.add_argument('--donor-generator', type=Path, required=True)
    parser.add_argument('--module-name', required=True)
    parser.add_argument('--template-symbol', default='selected_pic_template')
    parser.add_argument('--frozen-r192-v8-cpu', type=Path,
                        help='Opt in to separately source-pinned reference compatibility; queued R3000 remains default.')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    require(re.fullmatch(r'[A-Za-z_][A-Za-z_0-9]*', args.template_symbol), 'invalid_template_symbol')
    require(args.output.resolve().is_relative_to(W / 'build') and not args.output.exists(), 'fresh_owned_output_required')
    elf = args.original_irx.read_bytes()
    require(fixed.sha(elf) == args.expected_irx_sha.lower(), 'original_IRX_pin_mismatch')
    require(fixed.file_sha(args.donor_generator) == fixed.DONOR_SHA, 'offline_generator_pin_mismatch')
    irx = fixed.load_tool('r192_pic_irx', W / 'scripts/iop_irx_imports.py')
    image, load, header_name = irx._parse_elf_load_image(elf)
    text = [s for s in fixed.sections(elf) if s['flags'] & 4 and s['size']]
    require(load['virtual_address'] == 0 and len(text) == 1 and text[0]['VA'] == 0, 'PIC_noncontiguous_text_not_proved')
    records = irx.scan_relocations(elf, len(image))
    semantics_record = {'mode': 'R3000_queued_loads', 'native_credit': 0}
    if args.frozen_r192_v8_cpu:
        import r192_iop_v8_semantics
        r192_iop_v8_semantics.validate_source(args.frozen_r192_v8_cpu)
        semantics_record = {'mode': r192_iop_v8_semantics.MODE,
                            'reference_cpu_sha256': r192_iop_v8_semantics.REFERENCE_CPU_SHA256,
                            'reference_kernel_sha256': r192_iop_v8_semantics.REFERENCE_KERNEL_SHA256,
                            'projection_source_sha256': fixed.file_sha(Path(r192_iop_v8_semantics.__file__)),
                            'reference_cpu': str(args.frozen_r192_v8_cpu.resolve()), 'native_credit': 0}
    body, function, joins = generate_body(image, text[0]['size'], records, elf, args.donor_generator,
                                        'r192_pic_' + fixed.sha(elf)[:16], args.frozen_r192_v8_cpu)
    args.output.mkdir(parents=True, exist_ok=False)
    target = args.output / 'iop_pic.cpp'
    target.write_text(body, encoding='utf-8', newline='\n')
    (args.output / 'original.IRX').write_bytes(elf)
    imports = irx.scan_import_tables(image)
    lines = ['#include "iop/r192_pic.hpp"', 'namespace ps2x::iop::detail {',
             f'uint32_t {function}(IopCpuState &, IopMemory &, uint32_t, uint32_t);', '}',
             'namespace ps2::iop::r192_aot { namespace {', 'const uint32_t words[] = {']
    words = struct.unpack('<' + 'I' * (text[0]['size'] // 4), image[:text[0]['size']])
    lines += ['    ' + ', '.join(f'0x{x:08X}u' for x in words[i:i+8]) + ',' for i in range(0, len(words), 8)]
    lines += ['};', 'const Import imports[] = {']
    import_count = 0
    for table in imports:
        for entry in table['imports']:
            lines.append(f'    {{{entry["image_offset"]}u, {json.dumps(table["library"])}, {table["version"] & 0xFFFF}u, {entry["ordinal"]}u}},')
            import_count += 1
    if not import_count:
        lines.append('    {0u, nullptr, 0u, 0u},')
    lines += ['};', 'const Relocation relocations[] = {']
    for row in joins:
        lines.append(f'    {{{row["offset"]}u, {row["type"]}u, 0x{row["original_word"]}u, 0x{row["addend"]:08X}u}},')
    if not joins:
        lines.append('    {0u, 0u, 0u, 0u},')
    lines += ['};', f'const PicTemplate source = {{{json.dumps(args.module_name)}, {text[0]["size"]}u, {len(image)}u, words, imports, {import_count}u, relocations, {len(joins)}u, &ps2x::iop::detail::{function}}};',
              '}', f'const PicTemplate &{args.template_symbol}() {{ return source; }}', '}']
    descriptor = args.output / 'iop_pic_descriptor.cpp'
    descriptor.write_text('\n'.join(lines) + '\n', encoding='utf-8', newline='\n')
    report = {'status': 'PIC_CPP_GENERATED_CAPABILITY_ONLY', 'native_credit': 0,
              'source': {'path': str(Path(__file__).resolve()), 'sha256': fixed.file_sha(Path(__file__))},
              'original': {'path': str(args.original_irx.resolve()), 'sha256': fixed.sha(elf), 'header_name': header_name},
              'load': load, 'text_size': text[0]['size'], 'entry_offset': fixed.u32(elf, 24),
              'function': function, 'template_symbol': args.template_symbol,
              'semantics': semantics_record,
              'cpp': {'path': str(target.resolve()), 'sha256': fixed.file_sha(target)},
              'descriptor': {'path': str(descriptor.resolve()), 'sha256': fixed.file_sha(descriptor)},
              'relocations': joins, 'donor_sha256': fixed.DONOR_SHA,
              'limits': ['No runtime import bindings or ownership credit.', 'R_MIPS32 words are literal data, not proved executable entries.',
                         'R3000 queued loads retain the measured difference from the frozen R192 immediate-load reference.']}
    result = args.output / 'RESULT.json'
    result.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'result': str(result), 'sha256': fixed.file_sha(result)}))


if __name__ == '__main__':
    main()
