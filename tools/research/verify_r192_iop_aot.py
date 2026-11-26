"""Build/run the bounded IOP AOT differential against a pinned frozen runtime.

States must be owned copies. The harness uses the real byte serializer, original
host RPC registrations and existing IOP memory services; it never boots/mounts a
card, enters the game loop, or modifies the candidate runtime/PE.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

W = Path(__file__).resolve().parents[2]


def require(ok, why):
    if not ok:
        raise ValueError(why)


def sha(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for data in iter(lambda: stream.read(4 << 20), b''):
            h.update(data)
    return h.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('runtime-root', 'generated', 'compiler', 'output'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--expected-runtime-sha', required=True)
    parser.add_argument('--state', type=Path, action='append', required=True)
    args = parser.parse_args()
    require(args.output.resolve().is_relative_to(W/'build') and not args.output.exists(), 'fresh_owned_output_required')
    require(args.generated.resolve().is_relative_to(W/'build'), 'owned_generated_IRX_source_required')
    require(all(path.resolve().is_relative_to(W/'build/oracle-telemetry') for path in args.state), 'owned_state_copies_required')
    archive = args.runtime_root/'out/libps2runtime.a'
    runtime_sha = sha(archive)
    require(runtime_sha == args.expected_runtime_sha.lower(), 'reference_runtime_pin_mismatch')
    registry = json.loads((args.generated/'capture-registry.json').read_text())
    require(len(registry) == 1, 'single_representative_module_required')
    source_name = registry[0]['source']
    require(Path(source_name).name == source_name, 'generated_source_path_escape')
    native_sources = [W/'tests/test_r192_iop_aot.cpp', W/'runtime/src/iop/r192_aot.cpp',
                      args.generated/source_name, args.generated/'r192_iop_descriptor.cpp']
    pins = [{'path': str(path.resolve()), 'sha256': sha(path)} for path in native_sources+[
        W/'runtime/src/iop/r192_aot.hpp', W/'runtime/src/native/iop_native.h',
        W/'runtime/src/native/iop_native_support.h', args.runtime_root/'src/runtime/src/iop/cpu.cpp',
        args.runtime_root/'src/runtime/src/iop/iop.hpp', args.runtime_root/'src/runtime/src/iop/kernel.cpp',
        args.runtime_root/'src/runtime/src/system.cpp', args.runtime_root/'src/runtime/include/ps2/state.hpp']]
    states = [{'path': str(path.resolve()), 'sha256': sha(path)} for path in args.state]
    args.output.mkdir(parents=True, exist_ok=False)
    exe = args.output/'test-r192-iop-aot.exe'
    command = [str(args.compiler), '-std=c++20', '-O2', '-mavx2', '-static-libgcc', '-static-libstdc++',
               '-I'+str(args.runtime_root/'src/runtime/include'), '-I'+str(args.runtime_root/'src/runtime/src'),
               '-I'+str(W/'runtime/src'), *(str(path) for path in native_sources), str(archive),
               '-Wl,--wrap=_ZN3ps23iop6write8Ejh', '-Wl,--wrap=_ZN3ps23iop7write16Ejt',
               '-Wl,--wrap=_ZN3ps23iop7write32Ejj', '-lbcrypt', '-lwinmm',
               str(args.runtime_root/'src/third_party/sdl2/lib/libSDL2.dll.a'),
               str(args.runtime_root/'src/third_party/wgpu-native/lib/libwgpu_native.a'),
               '-lws2_32', '-luserenv', '-lntdll', '-lsynchronization', '-lpthread', '-lm',
               '-lkernel32', '-luser32', '-lgdi32', '-lwinspool', '-lshell32', '-lole32', '-loleaut32',
               '-luuid', '-lcomdlg32', '-ladvapi32', '-o', str(exe)]
    build = subprocess.run(command, capture_output=True, text=True)
    (args.output/'build.log').write_text(build.stdout+build.stderr, encoding='utf-8', newline='\n')
    require(build.returncode == 0, 'bounded_IOP_test_build_failed')
    env = dict(os.environ)
    for name in list(env):
        if name.startswith(('PS2_', 'MKA_', 'SDL_')):
            del env[name]
    env['PATH'] = str(args.runtime_root/'out')+os.pathsep+str(args.compiler.parent)+os.pathsep+env.get('PATH', '')
    cases = []
    for slot, state in enumerate(args.state):
        target = args.output/f'case-{slot:02}.json'
        argv = [str(exe), str(state), str(target)]
        began = time.monotonic()
        result = subprocess.run(argv, env=env, capture_output=True, text=True, timeout=120)
        (args.output/f'case-{slot:02}.stdout.log').write_text(result.stdout, encoding='utf-8', newline='\n')
        (args.output/f'case-{slot:02}.stderr.log').write_text(result.stderr, encoding='utf-8', newline='\n')
        measured = json.loads(target.read_text()) if target.exists() else None
        cases.append({'argv': argv, 'exit_code': result.returncode, 'seconds': time.monotonic()-began,
                      'measurement': measured, 'status': 'PASS' if result.returncode == 0 and measured and measured['status'] == 'PASS' else 'REFUSED'})
    width = args.output/'planted-store-width.json'
    negative_command = [str(exe), str(args.state[0]), str(width), '--planted-store-width']
    result = subprocess.run(negative_command, env=env, capture_output=True, text=True, timeout=120)
    (args.output/'planted-store-width.log').write_text(result.stdout+result.stderr, encoding='utf-8', newline='\n')
    negative = json.loads(width.read_text()) if width.exists() else None
    width_refused = bool(result.returncode == 1 and negative and negative['status'] == 'PLANTED_WIDTH_REJECTED' and negative['planted_width_cases_detected'] > 0)
    changed = [pin['path'] for pin in pins+states if sha(Path(pin['path'])) != pin['sha256']]
    require(sha(archive) == runtime_sha, 'reference_runtime_changed')
    passed = all(row['status'] == 'PASS' for row in cases) and width_refused and not changed
    receipt = {'status': 'BOUNDED_COMPONENT_PASS' if passed else 'BOUNDED_COMPONENT_REFUSED',
               'source': {'path': str(Path(__file__).resolve()), 'sha256': sha(Path(__file__))},
               'reference_runtime': {'path': str(archive.resolve()), 'sha256': runtime_sha},
               'compiler': {'path': str(args.compiler.resolve()), 'sha256': sha(args.compiler)},
               'build_command': command, 'test_binary_sha256': sha(exe), 'source_pins': pins, 'state_pins': states,
               'cases': cases, 'whole_export5_comparisons': sum((row['measurement'] or {}).get('whole_export5_comparisons', 0) for row in cases if row['status'] == 'PASS'),
               'planted_store_width': {'argv': negative_command, 'exit_code': result.returncode,
                                       'status': 'REJECTED' if width_refused else 'NOT_REJECTED', 'measurement': negative},
               'input_changes': changed, 'native_credit': 0,
               'scope': 'Snapshot-seeded constructed complete export5 calls and original module-entry prefixes only. Imports unbound, immediate-load reference gap explicit, no whole-module/IOP/native completion.'}
    target = args.output/'RESULT.json'
    target.write_text(json.dumps(receipt, indent=2)+'\n', encoding='utf-8', newline='\n')
    print(json.dumps({'result': str(target.resolve()), 'sha256': sha(target), 'status': receipt['status'],
                      'states': len(cases), 'whole_export5_comparisons': receipt['whole_export5_comparisons'], 'width_plant': width_refused}))
    return 0 if passed else 1


if __name__ == '__main__':
    raise SystemExit(main())
