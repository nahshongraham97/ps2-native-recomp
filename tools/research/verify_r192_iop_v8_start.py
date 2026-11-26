"""Compare whole original SDRDRV start through real, known native HLE bodies.

The test compiles unchanged original kernel.cpp solely to inspect its private
registration identities. Unknown synthesized callbacks never execute. The
candidate runtime/source/card are not modified; every run is an isolated process.
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('runtime-root', 'generated', 'compiler', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--expected-runtime-sha', required=True)
    parser.add_argument('--private-dispatch-domain', type=Path)
    parser.add_argument('--expected-private-domain-sha')
    args = parser.parse_args()
    require(args.output.resolve().is_relative_to(W / 'build') and not args.output.exists(), 'fresh_owned_output_required')
    measured = json.loads((args.generated / 'RESULT.json').read_text())
    require(measured['semantics']['mode'] == semantics.MODE and
            measured['semantics']['reference_cpu_sha256'] == semantics.REFERENCE_CPU_SHA256, 'explicit_frozen_v8_mode_required')
    semantics.validate_source(args.runtime_root / 'src/runtime/src/iop/cpu.cpp')
    require(measured['original']['sha256'] == '85a465ab251bde0c3d33151b3a67ec2780d57cc40ebb7df9d5ecb7bad3396b16' and
            sha(args.generated / 'original.IRX') == measured['original']['sha256'], 'original_representative_IRX_pin_mismatch')
    for name in ('cpp', 'descriptor'):
        require(sha(Path(measured[name]['path'])) == measured[name]['sha256'], 'generated_source_changed')
    archive = args.runtime_root / 'out/libps2runtime.a'
    require(sha(archive) == args.expected_runtime_sha.lower(), 'original_runtime_pin_mismatch')
    args.output.mkdir(parents=True, exist_ok=False)
    sources = [W / 'tests/test_r192_iop_v8_start.cpp', W / 'runtime/src/iop/r192_aot.cpp',
               W / 'runtime/src/iop/r192_pic.cpp', args.generated / 'iop_pic.cpp', args.generated / 'iop_pic_descriptor.cpp']
    kernel = args.runtime_root / 'src/runtime/src/iop/kernel.cpp'
    extra_objects = []
    extra_compile = []
    extra_pin_paths = []
    if args.private_dispatch_domain:
        import build_r192_iop_v8_candidate as builder
        require(args.expected_private_domain_sha and sha(args.private_dispatch_domain) == args.expected_private_domain_sha.lower(), 'private_domain_pin_mismatch')
        domain = json.loads(args.private_dispatch_domain.read_text())
        require(domain['semantics'] == semantics.MODE and domain['summary']['compiled_PIC_bodies'] == 70, 'complete_frozen_v8_domain_required')
        for row in domain['records']:
            for item in row['object_compilations']:
                obj = Path(item['object']); require(sha(obj) == item['object_sha256'], 'private_PIC_object_changed')
                extra_objects.append(obj)
        obj = Path(domain['registry']['path']).with_suffix('.obj')
        require(sha(obj) == domain['registry']['object_sha256'], 'private_registry_object_changed')
        extra_objects.append(obj)
        patched = args.output / 'private-kernel.cpp'
        patched.write_text(builder.prepare_kernel(kernel.read_bytes(), W / 'runtime/src/iop/r192_v8_dispatch.inc'), encoding='utf-8', newline='\n')
        kernel = patched
        wrapper = args.output / 'private-cpu.cpp'; wrapper.write_text(builder.cpu_wrapper_source(), encoding='utf-8', newline='\n')
        original_cpu = args.runtime_root / 'src/runtime/src/iop/cpu.cpp'
        text = original_cpu.read_text()
        changes = [('#include "iop.hpp"', '#include "iop/iop.hpp"'), ('#include "kernel.hpp"', '#include "iop/kernel.hpp"'),
                   ('alignas(64) std::uint8_t g_ram[kRamSize + 16];', ''), ('Cpu g_cpu;', ''),
                   ('std::uint32_t fetch(std::uint32_t pc)', 'std::uint32_t reference_fetch(std::uint32_t pc)'),
                   ('const std::uint32_t w = fetch(pc);', 'const std::uint32_t w = reference_fetch(pc);'),
                   ('void execute(Cpu &c, std::int64_t &budget)', 'void reference_execute(Cpu &c, std::int64_t &budget)')]
        for before, after in changes:
            require(text.count(before) == 1, 'independent_reference_rename_anchor_changed'); text = text.replace(before, after)
        reference = args.output / 'independent-original-cpu.cpp'; reference.write_text(text, encoding='utf-8', newline='\n')
        selected = args.output / 'selected-template.cpp'
        selected.write_text('#include "iop/r192_pic.hpp"\nnamespace ps2::iop::r192_aot {\nconst PicTemplate &selected_pic_template() { return *find_pic_template("SDRDRV", "85a465ab251bde0c3d33151b3a67ec2780d57cc40ebb7df9d5ecb7bad3396b16"); }\n}\n', encoding='utf-8', newline='\n')
        sources = sources[:3] + [wrapper, reference, selected]
        extra_compile = ['-DR192_USE_PRIVATE_AOT', '-DR192_AOT_TEST_SERVICE_OBSERVER', '-I' + str(args.runtime_root / 'src/runtime/src/iop')]
        extra_pin_paths = [Path(builder.__file__), W / 'runtime/src/iop/r192_v8_dispatch.inc', args.private_dispatch_domain, *extra_objects]
    pins = [{'path': str(p.resolve()), 'sha256': sha(p)} for p in sources + [Path(__file__), Path(semantics.__file__), kernel,
            args.runtime_root / 'src/runtime/src/iop/cpu.cpp', W / 'runtime/src/iop/r192_aot.hpp',
            W / 'runtime/src/iop/r192_pic.hpp', args.generated / 'original.IRX'] + extra_pin_paths]
    exe = args.output / 'test-v8-whole-start.exe'
    command = [str(args.compiler), '-std=c++20', '-O2', '-mavx2', '-static-libgcc', '-static-libstdc++',
        '-DR192_REFERENCE_KERNEL_SOURCE="' + kernel.as_posix() + '"',
        '-I' + str(args.runtime_root / 'src/runtime/include'), '-I' + str(args.runtime_root / 'src/runtime/src'),
        '-I' + str(W / 'runtime/src'), *extra_compile, *(str(p) for p in sources), *(str(p) for p in extra_objects), str(archive),
        '-Wl,--wrap=_ZN3ps23iop7executeERNS0_3CpuERx', '-Wl,--wrap=_ZN3ps23iop8call_hleERNS0_3CpuEj',
        '-Wl,--wrap=_ZN3ps23iop6write8Ejh', '-Wl,--wrap=_ZN3ps23iop7write16Ejt', '-Wl,--wrap=_ZN3ps23iop7write32Ejj',
        '-lbcrypt', '-lwinmm', str(args.runtime_root / 'src/third_party/sdl2/lib/libSDL2.dll.a'),
        str(args.runtime_root / 'src/third_party/wgpu-native/lib/libwgpu_native.a'), '-lws2_32', '-luserenv', '-lntdll',
        '-lsynchronization', '-lpthread', '-lm', '-lkernel32', '-luser32', '-lgdi32', '-lwinspool', '-lshell32',
        '-lole32', '-loleaut32', '-luuid', '-lcomdlg32', '-ladvapi32', '-o', str(exe)]
    if args.private_dispatch_domain:
        response = args.output / 'build.rsp'
        response.write_text('\n'.join(builder.quote_response(x) for x in command[1:]) + '\n', encoding='utf-8', newline='\n')
        actual_build_command = [command[0], '@' + str(response)]
    else:
        actual_build_command = command
    process = subprocess.run(actual_build_command, capture_output=True, text=True)
    (args.output / 'build.log').write_text(process.stdout + process.stderr, encoding='utf-8', newline='\n')
    require(process.returncode == 0, 'whole_start_reference_test_build_failed')
    env = {k: v for k, v in os.environ.items() if not k.startswith(('PS2_', 'MKA_', 'SDL_'))}
    env['PATH'] = str(args.runtime_root / 'out') + os.pathsep + str(args.compiler.parent) + os.pathsep + env.get('PATH', '')
    rows = []
    placements = [(0x27000, 0), (0x1A7000, 0), (0x7D7000, 0), (0x27000, 0x80000000), (0x27000, 0xA0000000)]
    for base, alias in placements:
        directory = args.output / ('base-' + format(base, 'x') + '-alias-' + format(alias, 'x')); directory.mkdir()
        row = {'base': base, 'execution_alias': alias, 'classification': 'reference compatibility capability only', 'runs': {}}
        for mode in ('reference', 'compiled'):
            output = directory / mode
            argv = [str(exe), str(args.generated / 'original.IRX'), hex(base), mode, str(output)]
            if alias:
                argv.append(hex(alias))
            began = time.monotonic()
            p = subprocess.run(argv, capture_output=True, text=True, env=env, timeout=600)
            (directory / (mode + '.log')).write_text(p.stdout + p.stderr, encoding='utf-8', newline='\n')
            result = output.with_suffix('.json')
            row['runs'][mode] = {'argv': argv, 'exit_code': p.returncode, 'seconds': time.monotonic() - began,
                                 'measurement': json.loads(result.read_text()) if result.exists() else None}
        comparisons = []
        for suffix in ('cpu', 'ram', 'trace', 'iop-state'):
            a, b = directory / ('reference.' + suffix), directory / ('compiled.' + suffix)
            comparisons.append({'artifact': suffix, 'reference_sha256': sha(a) if a.exists() else None,
                'compiled_sha256': sha(b) if b.exists() else None,
                'equal': a.exists() and b.exists() and a.read_bytes() == b.read_bytes()})
        row['comparisons'] = comparisons
        row['status'] = 'PASS' if all(x['exit_code'] == 0 and x['measurement'] and x['measurement']['status'] == 'PASS' for x in row['runs'].values()) and all(x['equal'] for x in comparisons) else 'REFUSED'
        rows.append(row)
    changed = [p['path'] for p in pins if sha(Path(p['path'])) != p['sha256']]
    require(sha(archive) == args.expected_runtime_sha.lower(), 'original_runtime_changed')
    control_prefix = args.output / 'actual-unknown-import'
    control_argv = [str(exe), str(args.generated / 'original.IRX'), '0x27000', 'compiled', str(control_prefix), '0', '--planted-unknown-import']
    control_run = subprocess.run(control_argv, capture_output=True, text=True, env=env, timeout=120)
    (args.output / 'actual-unknown-import.log').write_text(control_run.stdout + control_run.stderr, encoding='utf-8', newline='\n')
    control = json.loads(control_prefix.with_suffix('.json').read_text()) if control_prefix.with_suffix('.json').exists() else None
    refused = bool(control_run.returncode == 1 and control and 'unknown_or_synthesized_HLE' in control['refusal'] and
                   control['synthesized_success_callbacks_executed'] == 0 and control['native_service_calls'] == 0)
    passed = not changed and all(r['status'] == 'PASS' for r in rows) and refused
    receipt = {'status': 'WHOLE_START_REFERENCE_COMPATIBILITY_PASS' if passed else 'WHOLE_START_REFERENCE_COMPATIBILITY_REFUSED',
        'classification': 'reference compatibility capability only', 'native_credit': 0,
        'source_pins': pins, 'reference_runtime_sha256': args.expected_runtime_sha.lower(), 'build_command': command,
        'compiler_sha256': sha(args.compiler), 'test_binary_sha256': sha(exe), 'cases': rows, 'input_changes': changed,
        'unknown_import_control': {'argv': control_argv, 'exit_code': control_run.returncode,
                                   'measurement': control, 'status': 'REJECTED' if refused else 'NOT_REJECTED'},
        'limits': ['Original module start return only; scheduler on_exit closure explicitly excluded from serialization, not claimed executed.',
                   'Unknown synthesized HLE callback and nested interpreter execution on the compiled service path are refused.',
                   'Exact per-instruction CPU/RAM fingerprints plus ordered writes and actual original IOP serializer compared.',
                   'Frozen-v8 reference compatibility does not establish queued R3000 hardware semantics or native ownership credit.']}
    target = args.output / 'RESULT.json'; target.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'result': str(target), 'sha256': sha(target), 'status': receipt['status']}))
    return 0 if passed else 1


if __name__ == '__main__':
    raise SystemExit(main())
