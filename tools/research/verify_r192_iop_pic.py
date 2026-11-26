"""Compile one relocatable IRX body and compare at actual-loader test bases.

Uses the frozen runtime's real IRX loader and interpreter. No game loop, card,
protected binary/source, or original disc is modified. Results are capability
only; pending-load hardware semantics still differ from the frozen oracle.
"""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import subprocess
import time

import generate_r192_iop_aot as fixed
import generate_r192_iop_pic as pic

W = fixed.W
require = fixed.require
sha = fixed.file_sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('runtime-root', 'generated', 'compiler', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--expected-runtime-sha', required=True)
    args = parser.parse_args()
    require(args.output.resolve().is_relative_to(W / 'build') and not args.output.exists(), 'fresh_owned_output_required')
    require(args.generated.resolve().is_relative_to(W / 'build'), 'owned_generated_source_required')
    report = json.loads((args.generated / 'RESULT.json').read_text())
    for name in ('cpp', 'descriptor'):
        require(sha(Path(report[name]['path'])) == report[name]['sha256'], 'generated_PIC_source_changed')
    elf = (args.generated / 'original.IRX').read_bytes()
    require(fixed.sha(elf) == report['original']['sha256'] == '85a465ab251bde0c3d33151b3a67ec2780d57cc40ebb7df9d5ecb7bad3396b16',
            'representative_original_IRX_pin_mismatch')
    archive = args.runtime_root / 'out/libps2runtime.a'
    require(sha(archive) == args.expected_runtime_sha.lower(), 'reference_runtime_pin_mismatch')
    irx = fixed.load_tool('r192_pic_test_irx', W / 'scripts/iop_irx_imports.py')
    image, _, _ = irx._parse_elf_load_image(elf)
    records = irx.scan_relocations(elf, len(image))
    controls = []
    for name, bad in [('invented_pointer', records + [{'offset': 0xDC8, 'type': 5}]),
                      ('wrong_relocation_site', [dict(x, offset=x['offset'] + 4) if i == 0 else x for i, x in enumerate(records)])]:
        try:
            pic.relocation_program(elf, image, bad, report['text_size'])
        except ValueError as exc:
            controls.append({'name': name, 'status': 'REJECTED', 'reason': str(exc)})
        else:
            controls.append({'name': name, 'status': 'NOT_REJECTED'})
    args.output.mkdir(parents=True, exist_ok=False)
    sources = [W / 'tests/test_r192_iop_pic.cpp', W / 'runtime/src/iop/r192_aot.cpp',
               W / 'runtime/src/iop/r192_pic.cpp', args.generated / 'iop_pic.cpp', args.generated / 'iop_pic_descriptor.cpp']
    pin_paths = sources + [W / 'runtime/src/iop/r192_aot.hpp', W / 'runtime/src/iop/r192_pic.hpp',
                          W / 'runtime/src/native/iop_native.h', W / 'runtime/src/native/iop_native_support.h',
                          W / 'scripts/iop_irx_imports.py', Path(pic.__file__), Path(fixed.__file__),
                          args.runtime_root / 'src/runtime/src/iop/cpu.cpp', args.runtime_root / 'src/runtime/src/iop/kernel.cpp',
                          args.runtime_root / 'src/runtime/src/iop/iop.hpp', args.generated / 'original.IRX']
    pins = [{'path': str(p.resolve()), 'sha256': sha(p)} for p in pin_paths]
    env = {k: v for k, v in os.environ.items() if not k.startswith(('PS2_', 'MKA_', 'SDL_'))}
    env['PATH'] = str(args.runtime_root / 'out') + os.pathsep + str(args.compiler.parent) + os.pathsep + env.get('PATH', '')

    def build(source, label):
        exe = args.output / (label + '.exe')
        actual = list(sources); actual[3] = source
        cmd = [str(args.compiler), '-std=c++20', '-O2', '-mavx2', '-static-libgcc', '-static-libstdc++',
               '-I' + str(args.runtime_root / 'src/runtime/include'), '-I' + str(args.runtime_root / 'src/runtime/src'),
               '-I' + str(W / 'runtime/src'), *(str(p) for p in actual), str(archive),
               '-Wl,--wrap=_ZN3ps23iop6write8Ejh', '-Wl,--wrap=_ZN3ps23iop7write16Ejt', '-Wl,--wrap=_ZN3ps23iop7write32Ejj',
               '-lbcrypt', '-lwinmm', str(args.runtime_root / 'src/third_party/sdl2/lib/libSDL2.dll.a'),
               str(args.runtime_root / 'src/third_party/wgpu-native/lib/libwgpu_native.a'),
               '-lws2_32', '-luserenv', '-lntdll', '-lsynchronization', '-lpthread', '-lm', '-lkernel32',
               '-luser32', '-lgdi32', '-lwinspool', '-lshell32', '-lole32', '-loleaut32', '-luuid', '-lcomdlg32', '-ladvapi32',
               '-o', str(exe)]
        process = subprocess.run(cmd, capture_output=True, text=True)
        (args.output / (label + '-build.log')).write_text(process.stdout + process.stderr, encoding='utf-8', newline='\n')
        require(process.returncode == 0, 'PIC_test_build_failed:' + label)
        return exe, {'argv': cmd, 'exit_code': process.returncode, 'binary_sha256': sha(exe)}

    def run(exe, base, label, alias=0):
        result = args.output / (label + '.json')
        cmd = [str(exe), str(args.generated / 'original.IRX'), hex(base), str(result)]
        if alias:
            cmd.append(hex(alias))
        began = time.monotonic()
        p = subprocess.run(cmd, capture_output=True, text=True, env=env, timeout=120)
        (args.output / (label + '.log')).write_text(p.stdout + p.stderr, encoding='utf-8', newline='\n')
        return {'argv': cmd, 'exit_code': p.returncode, 'seconds': time.monotonic() - began,
                'measurement': json.loads(result.read_text()) if result.exists() else None}

    exe, build_receipt = build(args.generated / 'iop_pic.cpp', 'actual-PIC')
    bases = [0x20000, 0x26300, 0x27000, 0x9FC00, 0x1A7000, 0x7D7000]
    cases = [run(exe, base, 'base-' + format(base, 'x')) for base in bases]
    cases += [run(exe, 0x27000, 'alias-' + format(alias, 'x'), alias) for alias in (0x80000000, 0xA0000000)]
    body = (args.generated / 'iop_pic.cpp').read_text()
    carry = 'moduleBase + 0x14f0u + 0x8000u'
    require(carry in body, 'HI16_carry_plant_witness_missing')
    mutant = args.output / 'wrong-HI16-carry.cpp'
    mutant.write_text(body.replace(carry, 'moduleBase + 0x14f0u + 0u'), encoding='utf-8', newline='\n')
    bad_exe, bad_build = build(mutant, 'wrong-HI16-carry')
    bad = run(bad_exe, 0x27000, 'wrong-HI16-carry')
    controls.append({'name': 'wrong_HI16_carry', 'status': 'REJECTED' if bad['exit_code'] == 1 and bad['measurement'] and bad['measurement']['unequal'] > 0 else 'NOT_REJECTED',
                     'build': bad_build, 'run': bad})
    at = body.index('L372:\n'); end = body.index('\nL373:', at)
    block = body[at:end]
    require('const uint32_t v = 0xffff0000u;' in block, 'ordinary_integer_plant_witness_missing')
    mutant = args.output / 'invented-pointer.cpp'
    mutant.write_text(body[:at] + block.replace('const uint32_t v = 0xffff0000u;', 'const uint32_t v = moduleBase + 0xffff0000u;') + body[end:], encoding='utf-8', newline='\n')
    bad_exe, bad_build = build(mutant, 'invented-pointer')
    bad = run(bad_exe, 0x27000, 'invented-pointer')
    controls.append({'name': 'ordinary_integer_rebased_as_pointer', 'status': 'REJECTED' if bad['exit_code'] == 1 and bad['measurement'] and bad['measurement']['unequal'] > 0 else 'NOT_REJECTED',
                     'build': bad_build, 'run': bad})
    changed = [p['path'] for p in pins if sha(Path(p['path'])) != p['sha256']]
    require(sha(archive) == args.expected_runtime_sha.lower(), 'reference_runtime_changed')
    passed = not changed and all(c['exit_code'] == 0 and c['measurement'] and c['measurement']['status'] == 'PASS' for c in cases) and all(c['status'] == 'REJECTED' for c in controls)
    receipt = {'status': 'PIC_COMPONENT_PASS' if passed else 'PIC_COMPONENT_REFUSED', 'classification': 'capability only',
               'native_credit': 0, 'source': {'path': str(Path(__file__).resolve()), 'sha256': sha(Path(__file__))},
               'source_pins': pins, 'reference_runtime_sha256': args.expected_runtime_sha.lower(),
               'compiler': {'path': str(args.compiler), 'sha256': sha(args.compiler)}, 'build': build_receipt,
               'bases': bases, 'cases': cases, 'controls': controls, 'input_changes': changed,
               'whole_export5_comparisons': sum((c['measurement'] or {}).get('whole_export5_comparisons', 0) for c in cases),
               'limits': ['Only SDRDRV export5 whole calls and entry-to-import prefixes verified; no whole-module/IOP closure.',
                          'Frozen reference has immediate loads; queued R3000 semantic gap remains explicit, not deployed.',
                          'Actual loader creates but never executes module threads; no card mount/game loop.']}
    target = args.output / 'RESULT.json'
    target.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'result': str(target), 'sha256': sha(target), 'status': receipt['status']}))
    return 0 if passed else 1


if __name__ == '__main__':
    raise SystemExit(main())
