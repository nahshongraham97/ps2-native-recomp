"""Specialize constructed instruction fixtures and compare actual v8 execute.

No Python expected-value model is used. The independent reference is the pinned
original runtime interpreter; DUT functions are statically emitted C++. This is
reference compatibility capability evidence, not hardware/native-port credit.
"""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import subprocess
import time

import generate_r192_iop_aot as fixed
import r192_iop_v8_semantics as semantics

W = fixed.W
require = fixed.require
sha = fixed.file_sha


def fixtures(donor, source):
    generator = fixed.load_tool('v8_fixture_generator', donor)
    original = generator.emit
    semantics.install(generator, source)
    rows = []
    def add(name, word, memory=False):
        rows.append((name, word, memory))
    for fn in (0, 2, 3, 4, 6, 7, 16, 17, 18, 19, 24, 25, 26, 27, 32, 33, 34, 35, 36, 37, 38, 39, 42, 43):
        rs, rt, rd, sa = 4, 5, 2, 0
        if fn in (0, 2, 3): rs, sa = 0, 31
        if fn in (16, 18): rs, rt = 0, 0
        if fn in (17, 19): rt, rd = 0, 0
        if fn in (24, 25, 26, 27): rd = 0
        word = (rs << 21) | (rt << 16) | (rd << 11) | (sa << 6) | fn
        add('SPECIAL_' + format(fn, '02x'), word)
        if rd: add('SPECIAL_zero_dst_' + format(fn, '02x'), word & ~(31 << 11))
        if rs: add('SPECIAL_zero_src_' + format(fn, '02x'), word & ~(31 << 21))
    for op in range(8, 16):
        for imm in (0, 1, 0x7FFF, 0x8000, 0xFFFF):
            word = (op << 26) | ((0 if op == 15 else 4) << 21) | (2 << 16) | imm
            add('IMM_' + format(op, '02x'), word)
        add('IMM_zero_dst_' + format(op, '02x'), (op << 26) | ((0 if op == 15 else 4) << 21) | 0x8000)
        add('IMM_zero_src_' + format(op, '02x'), (op << 26) | (2 << 16) | 0xFFFF)
    for op in (0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x28, 0x29, 0x2A, 0x2B, 0x2E):
        for rt in (0, 2, 4, 5): add('MEM_' + format(op, '02x'), (op << 26) | (4 << 21) | (rt << 16), True)
    for rs in (0, 4):
        for rd in (8, 12, 13, 14, 15):
            for rt in (0, 2): add('COP0_' + str(rs) + '_' + str(rd), (16 << 26) | (rs << 21) | (rt << 16) | (rd << 11))
    add('RFE', 0x42000010); add('SYNC_reference_noop', 15); add('CACHE_reference_noop', 47 << 26)
    add('SYSCALL_reference_diagnostic', 12); add('BREAK_reference_diagnostic', 13)
    pair = [('LWL_pair', (0x22 << 26) | (4 << 21) | (2 << 16) | 3, True),
            ('LWR_pair', (0x26 << 26) | (4 << 21) | (2 << 16), True)]
    lines = ['#pragma once', '#include <cstdio>', 'namespace fixtures {', 'using namespace ps2x::iop::detail;',
             'using Function = uint32_t (*)(IopCpuState &, IopMemory &);',
             'struct Case { const char *name; uint32_t word; bool memory; Function fn; };']
    def emit(index, word, function):
        lines.append(f'inline uint32_t case_{index}(IopCpuState &cpu, IopMemory &memory) {{')
        lines.append('    uint32_t n = 0; const uint32_t seg = 0; bool stop = false;')
        lines.extend(function(word, 0x30000, False)); lines.append('    return n;\n}')
    for i, row in enumerate(rows + pair): emit(str(i), row[1], generator.emit)
    plant_word = (0x23 << 26) | (4 << 21) | (2 << 16)
    emit('queue_plant', plant_word, original)
    lines += ['inline const Case cases[] = {']
    for i, (name, word, memory) in enumerate(rows):
        lines.append(f'    {{{json.dumps(name)}, 0x{word:08X}u, {str(memory).lower()}, &case_{i}}},')
    lines += ['};', 'inline constexpr unsigned count = sizeof(cases) / sizeof(cases[0]);', 'inline const Case unaligned_pair[] = {']
    for i, (name, word, memory) in enumerate(pair, len(rows)):
        lines.append(f'    {{{json.dumps(name)}, 0x{word:08X}u, true, &case_{i}}},')
    lines += ['};', 'inline constexpr Function queued_plant = &case_queue_plant;',
              f'inline constexpr uint32_t queued_plant_word = 0x{plant_word:08X}u;', '}']
    return '\n'.join(lines) + '\n', [{'name': n, 'word': f'{w:08X}', 'memory': m} for n, w, m in rows]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('runtime-root', 'compiler', 'donor-generator', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--expected-runtime-sha', required=True)
    args = parser.parse_args()
    require(args.output.resolve().is_relative_to(W / 'build') and not args.output.exists(), 'fresh_owned_output_required')
    cpu = args.runtime_root / 'src/runtime/src/iop/cpu.cpp'
    semantics.validate_source(cpu)
    require(sha(args.donor_generator) == fixed.DONOR_SHA, 'donor_generator_pin_mismatch')
    archive = args.runtime_root / 'out/libps2runtime.a'
    require(sha(archive) == args.expected_runtime_sha.lower(), 'reference_runtime_pin_mismatch')
    pins = [{'path': str(p.resolve()), 'sha256': sha(p)} for p in [Path(__file__), Path(semantics.__file__),
            W / 'tests/test_r192_iop_v8_semantics.cpp', W / 'runtime/src/native/iop_native.h',
            W / 'runtime/src/native/iop_native_support.h', cpu, cpu.with_name('kernel.cpp'), args.runtime_root / 'src/runtime/src/iop/iop.hpp', args.donor_generator]]
    args.output.mkdir(parents=True, exist_ok=False)
    header, manifest = fixtures(args.donor_generator, cpu)
    target = args.output / 'v8-fixtures.hpp'; target.write_text(header, encoding='utf-8', newline='\n')
    exe = args.output / 'test-v8-semantics.exe'
    command = [str(args.compiler), '-std=c++20', '-O2', '-mavx2', '-static-libgcc', '-static-libstdc++',
        '-I' + str(args.runtime_root / 'src/runtime/include'), '-I' + str(args.runtime_root / 'src/runtime/src'),
        '-I' + str(W / 'runtime/src'), '-I' + str(args.output), str(W / 'tests/test_r192_iop_v8_semantics.cpp'), str(archive),
        '-Wl,--wrap=_ZN3ps23iop6write8Ejh', '-Wl,--wrap=_ZN3ps23iop7write16Ejt', '-Wl,--wrap=_ZN3ps23iop7write32Ejj',
        '-lbcrypt', '-lwinmm', str(args.runtime_root / 'src/third_party/sdl2/lib/libSDL2.dll.a'),
        str(args.runtime_root / 'src/third_party/wgpu-native/lib/libwgpu_native.a'), '-lws2_32', '-luserenv', '-lntdll',
        '-lsynchronization', '-lpthread', '-lm', '-lkernel32', '-luser32', '-lgdi32', '-lwinspool', '-lshell32',
        '-lole32', '-loleaut32', '-luuid', '-lcomdlg32', '-ladvapi32', '-o', str(exe)]
    build = subprocess.run(command, capture_output=True, text=True)
    (args.output / 'build.log').write_text(build.stdout + build.stderr, encoding='utf-8', newline='\n')
    require(build.returncode == 0, 'v8_semantics_test_build_failed')
    env = {k: v for k, v in os.environ.items() if not k.startswith(('PS2_', 'MKA_', 'SDL_'))}
    env['PATH'] = str(args.runtime_root / 'out') + os.pathsep + str(args.compiler.parent) + os.pathsep + env.get('PATH', '')
    measurement = args.output / 'measurement.json'
    command_run = [str(exe), str(measurement)]
    began = time.monotonic(); run = subprocess.run(command_run, env=env, capture_output=True, text=True, timeout=600)
    (args.output / 'run.log').write_text(run.stdout + run.stderr, encoding='utf-8', newline='\n')
    measured = json.loads(measurement.read_text()) if measurement.exists() else None
    changed = [p['path'] for p in pins if sha(Path(p['path'])) != p['sha256']]
    require(sha(archive) == args.expected_runtime_sha.lower(), 'reference_runtime_changed')
    passed = run.returncode == 0 and measured and measured['status'] == 'PASS' and not changed
    receipt = {'status': 'V8_REFERENCE_COMPATIBILITY_PASS' if passed else 'V8_REFERENCE_COMPATIBILITY_REFUSED',
        'classification': 'reference compatibility capability only', 'native_credit': 0, 'source_pins': pins,
        'reference_runtime_sha256': args.expected_runtime_sha.lower(), 'fixture_header_sha256': sha(target), 'fixture_manifest': manifest,
        'compiler': {'path': str(args.compiler), 'sha256': sha(args.compiler)}, 'build_command': command,
        'test_binary_sha256': sha(exe), 'run_command': command_run, 'run_exit_code': run.returncode,
        'seconds': time.monotonic() - began, 'measurement': measured, 'input_changes': changed,
        'limits': ['Constructed original-v8 instruction behavior only, not retail ownership/native credit.',
                   'Branches/calls and host-service boundaries are separately verified with actual original module code.',
                   'Immediate loads, wrapping arithmetic/alignment behavior are frozen reference compatibility, not hardware correction.']}
    out = args.output / 'RESULT.json'; out.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'result': str(out), 'sha256': sha(out), 'status': receipt['status']}))
    return 0 if passed else 1


if __name__ == '__main__':
    raise SystemExit(main())
