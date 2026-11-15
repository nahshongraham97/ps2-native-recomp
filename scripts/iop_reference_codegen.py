#!/usr/bin/env python3
"""Generate IOP module bodies and import bridges from validated live captures.

Module bodies use the GoW compiler's ABI, adapted by runtime/src/native. The
registry retains the retail module name separately from the capture filename
and binds each body to its exact relocated base, text size, and FNV hash.
"""
import argparse
import importlib.util
import io
import json
import struct
from pathlib import Path

from iop_capture_manifest import fnv1a64, read_captures
from iop_irx_imports import scan_import_tables


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('captures', type=Path)
    parser.add_argument('donor_generator', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    captures = read_captures(args.captures)
    spec = importlib.util.spec_from_file_location('iop_reference_generator', args.donor_generator)
    if spec is None or spec.loader is None:
        parser.error('cannot load donor generator')
    generator = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(generator)
    # GoW overrides refer to its own IRX binaries and must never be applied to
    # Armageddon simply because a module has the same name.
    generator.OVERRIDES = {}
    original_emit = generator.emit
    original_emit_branch = generator.emit_branch

    def emit(word, address, delay):
        lines = original_emit(word, address, delay)
        if delay:
            guard = [
                '    if (n >= budget) {',
                f'        cpu.pc = seg | 0x{address:x}u;',
                '        cpu.branchPending = true;',
                f'        cpu.branchTarget = tk ? tg : (seg | 0x{address + 4:x}u);',
                '        stop = true;',
                '        return n;',
                '    }',
            ]
        else:
            guard = [
                '    if (n >= budget) {',
                f'        cpu.pc = seg | 0x{address:x}u;',
                '        stop = true;',
                '        return n;',
                '    }',
            ]
        return guard + lines

    def emit_branch(word, address):
        guard = [
            '    if (n >= budget) {',
            f'        cpu.pc = seg | 0x{address:x}u;',
            '        stop = true;',
            '        return n;',
            '    }',
        ]
        op, rs, rt, rd, _sa, fn, imm = generator.fields(word)
        link = f'seg | 0x{address + 8:x}u'
        if op == 0 and fn == 0x09:  # JALR reads rs before writing rd.
            lines = ['    ++n;', f'    tk = true; tg = {generator.g(rs)};']
            lines.extend(generator.commit(rd))
            if rd:
                lines.append(f'    cpu.gpr[{rd}] = {link};')
            return guard + lines
        if op == 1 and rt in (0x10, 0x11):
            cmp = '< 0' if rt == 0x10 else '>= 0'
            target = (address + 4 + (generator.sext16(imm) << 2)) & 0x1FFFFFFF
            lines = ['    ++n;', f'    tk = static_cast<int32_t>({generator.g(rs)}) {cmp}; tg = seg | 0x{target:x}u;']
            lines.extend(generator.commit(31))
            lines.append(f'    cpu.gpr[31] = {link};')
            return guard + lines
        return guard + original_emit_branch(word, address)

    generator.emit = emit
    generator.emit_branch = emit_branch
    args.output.mkdir(parents=True, exist_ok=True)
    registry = []
    for capture in captures:
        data = (args.captures / (capture['capture'] + '.bin')).read_bytes()
        # Captures are read once during manifest validation and again here.
        # Do not generate code from bytes changed between those operations.
        if len(data) != capture['size'] or f'{fnv1a64(data):016X}' != capture['fnv1a64']:
            raise ValueError(f"capture changed before generation: {capture['capture']}")
        words = struct.unpack(f'<{len(data) // 4}I', data)
        imports = []
        mapped_stubs = set()
        for table in scan_import_tables(data):
            for entry in table['imports']:
                offset = entry['image_offset']
                mapped_stubs.add(offset)
                imports.append({'address': capture['base'] + offset,
                                'library': table['library'],
                                # link_imports and invoke_native_import use
                                # the low 16 bits as the library version.
                                'version': table['version'] & 0xFFFF,
                                'version_word': table['version'],
                                'ordinal': entry['ordinal']})
        # The reference generator yields branch-in-delay-slot cases to its
        # interpreter. Record every such candidate so its output cannot be
        # mistaken for a complete strict-native module. Literal data in text
        # can produce false positives; runtime reachability is not inferred.
        fallback = []
        unmapped_stubs = []
        for i in range(len(words) - 1):
            import_stub = (words[i] == 0x03E00008 and
                           (words[i + 1] & 0xFFFF0000) == 0x24000000)
            if import_stub and i * 4 not in mapped_stubs:
                unmapped_stubs.append(capture['base'] + i * 4)
            if not import_stub and generator.is_branch(words[i]) and generator.is_branch(words[i + 1]):
                fallback.append(capture['base'] + 4 * i)
        ident = capture['capture'] + '_' + format(capture['base'], 'x')
        path = args.output / ('iopn_' + ident + '.cpp')
        generated = io.StringIO()
        function = generator.gen_module(ident, capture['base'], words, generated)
        body = generated.getvalue()
        # The donor emits a separate D label when a branch target is also a
        # delay-slot address, but its intra-chunk go() helper always targets L.
        # Redirect those static gotos to the label that actually exists.
        for i in range(1, len(words)):
            previous = words[i - 1]
            previous_is_import_stub = (
                previous == 0x03E00008 and
                (words[i] & 0xFFFF0000) == 0x24000000
            )
            if generator.is_branch(previous) and not previous_is_import_stub:
                body = body.replace(f'goto L{i:x};', f'goto D{i:x};')
        # This runtime can resume an IOP thread at a branch delay PC. Keep the
        # pending branch while the slot executes, then apply its target before
        # the generated fallthrough PC. A no-budget return must leave both the
        # delay PC and pending branch intact for the next native invocation.
        for i in range(1, len(words)):
            prev = words[i - 1]
            import_stub = prev == 0x03E00008 and (words[i] & 0xFFFF0000) == 0x24000000
            if not generator.is_branch(prev) or import_stub:
                continue
            label = f'D{i:x}:\n'
            at = body.find(label)
            if at < 0:
                continue
            next_label = body.find('\nL', at + len(label))
            next_delay = body.find('\nD', at + len(label))
            ends = [p for p in (next_label, next_delay) if p >= 0]
            end = min(ends) if ends else len(body)
            block_start = at + len(label)
            first_pc_line = body.find('    cpu.pc = seg | ', block_start, end)
            pc_line = body.rfind('    cpu.pc = seg | ', block_start, end)
            if first_pc_line < 0 or pc_line <= first_pc_line:
                raise ValueError(
                    f"cannot locate delay-slot fallthrough after 0x{capture['base'] + i * 4:08X}"
                )
            if body.find('    ++n;', first_pc_line, pc_line) < 0:
                raise ValueError(
                    f"delay-slot instruction missing before resume target at 0x{capture['base'] + i * 4:08X}"
                )
            transfer = ('    if (cpu.branchPending) {\n'
                        '        cpu.pc = cpu.branchTarget;\n'
                        '        cpu.branchPending = false;\n'
                        '        goto dispatch;\n'
                        '    }\n')
            body = body[:pc_line] + transfer + body[pc_line:]
        path.write_text(body, encoding='utf-8', newline='\n')
        # Emit a direct host-service bridge for mapped import stubs. This is
        # target-runtime C++, separate from the donor-ABI generated module.
        # The future dispatcher must select the matching module by identity
        # before calling this function; addresses alone are not sufficient.
        bridge_name = 'iop_imports_' + ident
        bridge_path = args.output / (bridge_name + '.cpp')
        if len({entry['address'] for entry in imports}) != len(imports):
            raise ValueError(f"duplicate import stub address: {capture['capture']}")
        with bridge_path.open('w', encoding='utf-8', newline='\n') as out:
            out.write('#include "iop/kernel.hpp"\n#include <cstdio>\n\nnamespace ps2::iop::aot_imports {\n')
            if 'MSL' in bridge_name:
                out.write('''static const char *libsd_func_name(std::uint32_t ordinal) {
    switch (ordinal) {
    case 4: return "sceSdInit";
    case 5: return "sceSdSetParam";
    case 6: return "sceSdGetParam";
    case 7: return "sceSdSetSwitch";
    case 8: return "sceSdGetSwitch";
    case 9: return "sceSdSetAddr";
    case 10: return "sceSdGetAddr";
    case 11: return "sceSdSetCoreAttr";
    case 12: return "sceSdGetCoreAttr";
    case 13: return "sceSdNote2Pitch";
    case 14: return "sceSdPitch2Note";
    case 15: return "sceSdProcBatch";
    case 16: return "sceSdProcBatchEx";
    case 17: return "sceSdVoiceTrans";
    case 18: return "sceSdBlockTrans";
    case 19: return "sceSdVoiceTransStatus";
    case 20: return "sceSdBlockTransStatus";
    case 21: return "sceSdSetTransCallback";
    case 22: return "sceSdSetIRQCallback";
    case 23: return "sceSdSetEffectAttr";
    case 24: return "sceSdGetEffectAttr";
    case 25: return "sceSdClearEffect";
    case 26: return "sceSdSetTransIntrHandler";
    case 27: return "sceSdSetSpu2IntrHandler";
    case 28: return "sceSdGetTransIntrArg";
    case 29: return "sceSdGetSpu2IntrArg";
    case 30: return "sceSdStopTrans";
    case 31: return "sceSdCleanEffect";
    case 32: return "sceSdSetEffectMode";
    case 33: return "sceSdSetEffectModeParams";
    default: return "unknown";
    }
}
static void trace_msl_libsd_call(std::uint32_t ordinal, std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3, Cpu &c, bool ok) {
    if (kernel::g_trace_modules) {
        const char *name = libsd_func_name(ordinal);
        if (!ok) {
            std::fprintf(stderr, "[msl-libsd] ordinal %u (%s) REJECTED a0=0x%08X a1=0x%08X a2=0x%08X a3=0x%08X\\n",
                         ordinal, name, a0, a1, a2, a3);
        } else {
            std::fprintf(stderr, "[msl-libsd] ordinal %u (%s) a0=0x%08X a1=0x%08X a2=0x%08X a3=0x%08X -> r2=0x%08X\\n",
                         ordinal, name, a0, a1, a2, a3, c.r[2]);
        }
    }
}
''')
            out.write(f'bool {bridge_name}(Cpu &c) {{\n    switch (phys(c.pc)) {{\n')
            for entry in imports:
                library = json.dumps(entry['library'])
                out.write(f"    case 0x{entry['address']:08X}u:\n")
                out.write(f"      if (!kernel::compiled_import_stub_matches(c.pc, {library}, 0x{entry['version']:04X}u, {entry['ordinal']}u)) return false;\n")
                out.write('    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;\n')
                if 'MSL' in bridge_name and entry['library'] == 'libsd':
                    out.write('      const auto a0 = c.r[4], a1 = c.r[5], a2 = c.r[6], a3 = c.r[7];\n')
                    out.write(f"      const bool ok = kernel::invoke_native_import({library}, 0x{entry['version']:04X}u, {entry['ordinal']}u, c);\n")
                    out.write(f"      trace_msl_libsd_call({entry['ordinal']}u, a0, a1, a2, a3, c, ok);\n")
                    out.write('      if (!ok) { c.pc = pc; c.npc = npc; return false; }\n')
                else:
                    out.write(f"      if (!kernel::invoke_native_import({library}, 0x{entry['version']:04X}u, {entry['ordinal']}u, c)) {{ c.pc = pc; c.npc = npc; return false; }}\n")
                out.write('      c.r[0] = 0; return true; }\n')
            out.write('    default: return false;\n    }\n}\n}\n')
        registry.append({**capture, 'function': function, 'source': path.name,
                         'import_function': bridge_name, 'import_source': bridge_path.name,
                         'imports': imports,
                         'unmapped_stub_candidates': unmapped_stubs,
                         'interpreter_fallback_candidates': fallback})
    (args.output / 'capture-registry.json').write_text(json.dumps(registry, indent=2) + '\n', encoding='utf-8')
    with (args.output / 'iop_import_registry.cpp').open('w', encoding='utf-8', newline='\n') as out:
        out.write('#include "iop/aot_import_registry.hpp"\n\nnamespace ps2::iop::aot_imports {\n')
        for entry in registry:
            out.write(f"bool {entry['import_function']}(Cpu &);\n")
        out.write('namespace {\nconst Module kModules[] = {\n')
        for entry in registry:
            out.write(f"    {{{json.dumps(entry['module'])}, 0x{entry['base']:08X}u, 0x{entry['size']:X}u, 0x{entry['fnv1a64']}ull, &{entry['import_function']}}},\n")
        if not registry:
            out.write('    {nullptr, 0u, 0u, 0ull, nullptr},\n')
        out.write('};\n}\n')
        out.write(f'const Module *modules(std::size_t &count) {{ count = {len(registry)}; return kModules; }}\n')
        out.write('}\n')
    with (args.output / 'iopn_registry.cpp').open('w', encoding='utf-8', newline='\n') as out:
        out.write('#include "native/iop_native.h"\n\n')
        out.write('namespace ps2x::iop::detail {\n')
        for entry in registry:
            out.write(f"std::uint32_t {entry['function']}(IopCpuState &, IopMemory &, std::uint32_t);\n")
        out.write('namespace {\nconst IopNativeModule kModules[] = {\n')
        for entry in registry:
            total_size = entry.get('total_size', entry['size'])
            out.write(f"    {{{json.dumps(entry['module'])}, 0x{entry['base']:08X}u, "
                      f"0x{entry['size']:X}u, 0x{total_size:X}u, 0x{entry['fnv1a64']}ull, &{entry['function']}}},\n")
        if not registry:
            out.write('    {nullptr, 0u, 0u, 0u, 0ull, nullptr},\n')
        out.write('};\n}\n')
        out.write('const IopNativeModule *iopNativeModules(std::size_t &count) {\n')
        out.write(f'    count = {len(registry)};\n    return kModules;\n')
        out.write('}\n}\n')
    print(f'generated {len(registry)} C++ module bodies and import bridges')


if __name__ == '__main__':
    main()
