"""Compare typed ordinal42 query with the original disc THREADMAN method.

The unchanged v8 execute() is the independent machine reference. Guest frame
transport is measured separately, not emitted by the typed service. No game,
card mount, original input modification, or native ownership credit occurs.
"""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import subprocess

import analyze_r192_thbase42 as analysis
import generate_r192_iop_aot as original

W = original.W
require = original.require
sha = original.file_sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('runtime-root', 'evidence', 'compiler', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--expected-evidence-sha', required=True)
    parser.add_argument('--expected-runtime-sha', required=True)
    parser.add_argument('--state', type=Path, action='append', default=[])
    args = parser.parse_args()
    require(args.output.resolve().is_relative_to(W / 'build') and not args.output.exists(), 'fresh_owned_output_required')
    require(sha(args.evidence) == args.expected_evidence_sha.lower(), 'original_SDK_role_evidence_pin_mismatch')
    proof = json.loads(args.evidence.read_text())
    require(proof['producer']['sha256'] == sha(Path(analysis.__file__)) and proof['SDK']['commit'] == analysis.SDK_COMMIT and
            proof['contract']['ordinal'] == 42 and proof['contract']['version'] == '0102', 'role_evidence_producer_or_signature_changed')
    for sdk in proof['SDK']['files']:
        require(sha(Path(sdk['copy'])) == sdk['sha256'] == analysis.SDK_FILES[sdk['path']], 'official_SDK_source_changed')
    reference_irx = Path(proof['original_definitions'][0]['original']['path'])
    require(sha(reference_irx) == proof['original_definitions'][0]['original']['sha256'], 'original_THREADMAN_changed')
    n = args.runtime_root; archive = n / 'out/libps2runtime.a'
    require(sha(archive) == args.expected_runtime_sha.lower(), 'reference_archive_pin_mismatch')
    source = W / 'tests/test_r192_thbase42.cpp'; header = W / 'runtime/src/iop/r192_thbase42.hpp'
    kernel = n / 'src/runtime/src/iop/kernel.cpp'
    pins = [{'path': str(p.resolve()), 'sha256': sha(p)} for p in [Path(__file__), Path(analysis.__file__), source, header,
            reference_irx, args.evidence, kernel, n / 'src/runtime/src/iop/cpu.cpp', n / 'src/runtime/src/iop/libs.cpp',
            n / 'src/runtime/src/iop/iop.hpp', n / 'src/runtime/src/iop/kernel.hpp'] + args.state]
    require(all(p.resolve().is_relative_to(W / 'build/oracle-telemetry') for p in args.state), 'owned_state_copies_required')
    args.output.mkdir(parents=True, exist_ok=False)
    env = {k: v for k, v in os.environ.items() if not k.startswith(('PS2_', 'MKA_', 'SDL_'))}
    env['PATH'] = str(n / 'out') + os.pathsep + str(args.compiler.parent) + os.pathsep + env.get('PATH', '')
    def build(label, overlay=None):
        exe = args.output / (label + '.exe')
        command = [str(args.compiler), '-std=c++20', '-O2', '-mavx2', '-static-libgcc', '-static-libstdc++',
            '-DR192_REFERENCE_KERNEL_SOURCE="' + kernel.as_posix() + '"',
            '-I' + str(n / 'src/runtime/include'), '-I' + str(n / 'src/runtime/src')]
        if overlay: command += ['-I' + str(overlay)]
        command += ['-I' + str(W / 'runtime/src'), str(source), str(archive),
            '-Wl,--wrap=_ZN3ps23iop6write8Ejh', '-Wl,--wrap=_ZN3ps23iop7write16Ejt', '-Wl,--wrap=_ZN3ps23iop7write32Ejj',
            '-lbcrypt', '-lwinmm', str(n / 'src/third_party/sdl2/lib/libSDL2.dll.a'), str(n / 'src/third_party/wgpu-native/lib/libwgpu_native.a'),
            '-lws2_32', '-luserenv', '-lntdll', '-lsynchronization', '-lpthread', '-lm', '-lkernel32', '-luser32', '-lgdi32',
            '-lwinspool', '-lshell32', '-lole32', '-loleaut32', '-luuid', '-lcomdlg32', '-ladvapi32', '-o', str(exe)]
        p = subprocess.run(command, capture_output=True, text=True)
        (args.output / (label + '-build.log')).write_text(p.stdout + p.stderr, encoding='utf-8', newline='\n')
        require(p.returncode == 0, 'thbase42_test_compile_failed:' + label)
        return exe, {'argv': command, 'binary_sha256': sha(exe)}
    def run(exe, label, state=None):
        target = args.output / (label + '.json')
        command = [str(exe), str(reference_irx), str(target)] + ([str(state)] if state else [])
        p = subprocess.run(command, capture_output=True, text=True, env=env, timeout=120)
        (args.output / (label + '.log')).write_text(p.stdout + p.stderr, encoding='utf-8', newline='\n')
        return {'argv': command, 'exit_code': p.returncode, 'measurement': json.loads(target.read_text()) if target.exists() else None}
    exe, build_receipt = build('actual-query')
    cases = [run(exe, 'constructed')] + [run(exe, 'state-' + str(i).zfill(2), state) for i, state in enumerate(args.state)]
    text = header.read_text()
    mutations = [
        ('unsigned16', 'static_cast<std::int16_t>(*current_priority)', 'static_cast<std::uint16_t>(*current_priority)'),
        ('IRQ_zero', 'if (interrupt_context) return -100;', 'if (interrupt_context) return 0;'),
        ('no_thread_zero', 'if (!current_priority) refuse("IOP_THBASE42_REFUSED:no_running_thread");', 'if (!current_priority) return 0;'),
        ('wrong_priority_field', 'priority = thread->priority;', 'priority = thread->init_priority;'),
    ]
    controls = []
    for name, before, after in mutations:
        require(text.count(before) == 1, 'semantic_mutation_anchor_missing:' + name)
        directory = args.output / ('mutant-' + name); (directory / 'iop').mkdir(parents=True)
        mutant = directory / 'iop/r192_thbase42.hpp'; mutant.write_text(text.replace(before, after), encoding='utf-8', newline='\n')
        bad, built = build(name, directory); outcome = run(bad, name)
        measured = outcome['measurement'] or {}
        rejected = outcome['exit_code'] == 1 and measured.get('status') == 'REFUSED' and measured.get('failures', 0) > 0
        controls.append({'name': name, 'status': 'REJECTED' if rejected else 'NOT_REJECTED',
                         'mutant_header_sha256': sha(mutant), 'build': built, 'run': outcome})
    changed = [p['path'] for p in pins if sha(Path(p['path'])) != p['sha256']]
    require(sha(archive) == args.expected_runtime_sha.lower(), 'reference_archive_changed')
    passed = not changed and all(c['exit_code'] == 0 and c['measurement'] and c['measurement']['status'] == 'PASS' for c in cases) and all(c['status'] == 'REJECTED' for c in controls)
    receipt = {'status': 'THBASE42_ORIGINAL_METHOD_PASS' if passed else 'THBASE42_ORIGINAL_METHOD_REFUSED',
        'classification': 'source-supported native-service capability only', 'native_credit': 0,
        'source_pins': pins, 'reference_runtime_sha256': args.expected_runtime_sha.lower(), 'compiler_sha256': sha(args.compiler),
        'build': build_receipt, 'cases': cases, 'controls': controls, 'input_changes': changed,
        'whole_original_method_comparisons': sum((c['measurement'] or {}).get('whole_original_method_comparisons', 0) for c in cases),
        'limits': ['Original reference stack spill transport is measured separately; this is not full native RAM equivalence/credit.',
                   'Snapshot current priorities seed constructed original-method calls; no historical source-method entry is claimed.',
                   'Official SDK source plus original IRX establish role; old N synthetic zero is not the expected behavior.']}
    path = args.output / 'RESULT.json'; path.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'result': str(path), 'sha256': sha(path), 'status': receipt['status']}))
    return 0 if passed else 1


if __name__ == '__main__': raise SystemExit(main())
