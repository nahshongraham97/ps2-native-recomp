"""Exercise every original IRX through actual frozen filename/buffer loaders.

Loading, registry selection and planted identity refusals only; no entrypoint,
game, card, service callback or interpreter is executed by the test driver.
"""
from __future__ import annotations
import argparse
from collections import Counter
import json
import os
from pathlib import Path
import subprocess

import expand_r192_iop_loader_names as names
import build_r192_iop_v8_candidate as builder

W, require, sha = names.W, names.require, names.sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('domain', 'runtime-root', 'compiler', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--expected-domain-sha', required=True)
    parser.add_argument('--expected-runtime-sha', required=True)
    args = parser.parse_args()
    for name in ('domain', 'runtime_root', 'compiler', 'output'):
        setattr(args, name, getattr(args, name).resolve())
    require(args.output.resolve().is_relative_to(W / 'build') and not args.output.exists(), 'fresh_owned_output_required')
    require(sha(args.domain) == args.expected_domain_sha.lower(), 'expanded_domain_pin_mismatch')
    domain = json.loads(args.domain.read_text())
    require(domain['loader_name_extension']['producer_sha256'] == sha(Path(names.__file__)) and
            domain['loader_name_extension']['literal_pairs'] == 140 and len(domain['records']) == 70,
            'complete_literal_name_extension_required')
    require(sha(args.compiler) == domain['compiler']['sha256'], 'compiler_identity_changed')
    archive = args.runtime_root / 'out/libps2runtime.a'
    require(sha(archive) == args.expected_runtime_sha.lower(), 'reference_archive_pin_mismatch')
    kernel = args.runtime_root / 'src/runtime/src/iop/kernel.cpp'
    require(sha(kernel) == builder.KERNEL_SHA, 'original_loader_source_changed')
    pins = list(domain['source_pins'])
    registry = Path(domain['registry']['path']); registry_obj = registry.with_suffix('.obj')
    require(sha(registry) == domain['registry']['sha256'] and sha(registry_obj) == domain['registry']['object_sha256'],
            'literal_registry_source_or_object_changed')
    require(registry.read_text() == names.registry_source(domain['records']), 'registry_alias_rows_changed')
    objects = []
    for row in domain['records']:
        original = Path(row['original_copy'])
        require(sha(original) == row['original_sha256'] and names.original_header(original.read_bytes()) ==
                {k: row['original_header_name'][k] for k in names.original_header(original.read_bytes())}, 'original_header_evidence_changed')
        for item in row['object_compilations']:
            require(sha(Path(item['source'])) == item['source_sha256'] and sha(Path(item['object'])) == item['object_sha256'],
                    'compiled_body_source_or_object_changed')
            objects.append(Path(item['object']))
            pins += [{'path': item['source'], 'sha256': item['source_sha256']}, {'path': item['object'], 'sha256': item['object_sha256']}]
        pins.append({'path': str(original), 'sha256': row['original_sha256']})
    source = W / 'tests/test_r192_iop_loader_names.cpp'
    pins += [{'path': str(p.resolve()), 'sha256': sha(p)} for p in [Path(__file__), Path(names.__file__), Path(builder.__file__),
        args.domain, args.compiler, kernel, archive, registry, registry_obj, source, W / 'runtime/src/iop/r192_v8_dispatch.inc',
        W / 'runtime/src/iop/r192_aot.cpp', W / 'runtime/src/iop/r192_pic.cpp']]
    require(all(sha(Path(p['path'])) == p['sha256'] for p in pins), 'input_pin_mismatch')
    args.output.mkdir(parents=True, exist_ok=False)
    private = args.output / 'private-kernel.cpp'
    private.write_text(builder.prepare_kernel(kernel.read_bytes(), W / 'runtime/src/iop/r192_v8_dispatch.inc'), encoding='utf-8', newline='\n')
    exe = args.output / 'test-loader-names.exe'
    command = [str(args.compiler), '-std=c++20', '-O2', '-mavx2', '-static-libgcc', '-static-libstdc++',
        '-DR192_REFERENCE_KERNEL_SOURCE="' + private.as_posix() + '"',
        '-I' + str(args.runtime_root / 'src/runtime/include'), '-I' + str(args.runtime_root / 'src/runtime/src'),
        '-I' + str(W / 'runtime/src'), str(source), str(W / 'runtime/src/iop/r192_aot.cpp'),
        str(W / 'runtime/src/iop/r192_pic.cpp'), *(str(p) for p in objects), str(registry_obj), str(archive),
        '-lbcrypt', '-lwinmm', str(args.runtime_root / 'src/third_party/sdl2/lib/libSDL2.dll.a'),
        str(args.runtime_root / 'src/third_party/wgpu-native/lib/libwgpu_native.a'), '-lws2_32', '-luserenv', '-lntdll',
        '-lsynchronization', '-lpthread', '-lm', '-lkernel32', '-luser32', '-lgdi32', '-lwinspool', '-lshell32',
        '-lole32', '-loleaut32', '-luuid', '-lcomdlg32', '-ladvapi32', '-o', str(exe)]
    rsp = args.output / 'build.rsp'
    rsp.write_text('\n'.join(builder.quote_response(x) for x in command[1:]) + '\n', encoding='utf-8', newline='\n')
    compiled = subprocess.run([command[0], '@' + str(rsp)], capture_output=True, text=True)
    (args.output / 'build.log').write_text(compiled.stdout + compiled.stderr, encoding='utf-8', newline='\n')
    require(compiled.returncode == 0, 'actual_loader_test_compile_failed')
    env = {k: v for k, v in os.environ.items() if not k.startswith(('PS2_', 'MKA_', 'SDL_'))}
    env['PATH'] = str(args.runtime_root / 'out') + os.pathsep + str(args.compiler.parent) + os.pathsep + env.get('PATH', '')
    cases = []
    for slot, row in enumerate(domain['records']):
        for mode in ('filename', 'buffer', 'unknown-name', 'unknown-header', 'changed-code'):
            label = f'{slot:03}-{mode}'; output = args.output / (label + '.json')
            argv = [str(exe), row['original_copy'], row['original_sha256'], row['filename_loader_names'][0], mode,
                    str(output), row['original_header_name']['name'], str(row['original_header_name']['file_offset'])]
            result = subprocess.run(argv, capture_output=True, text=True, env=env, timeout=60)
            (args.output / (label + '.log')).write_text(result.stdout + result.stderr, encoding='utf-8', newline='\n')
            measured = json.loads(output.read_text()) if output.exists() else None
            cases.append({'original_sha256': row['original_sha256'], 'aliases': row['aliases'], 'mode': mode,
                'argv': argv, 'exit_code': result.returncode, 'measurement': measured,
                'status': 'PASS' if result.returncode == 0 and measured and measured['status'] == 'PASS' else 'REFUSED'})
        (args.output / 'PROGRESS.json').write_text(json.dumps({'finished_payloads': slot + 1,
            'cases': len(cases), 'histogram': dict(Counter(c['status'] for c in cases))}, indent=2) + '\n', encoding='utf-8', newline='\n')
    changes = [p['path'] for p in pins if sha(Path(p['path'])) != p['sha256']]
    passed = not changes and len(cases) == 350 and all(c['status'] == 'PASS' for c in cases)
    report = {'status': 'ALL_ORIGINAL_LOADER_NAMES_PASS' if passed else 'LOADER_NAME_PROOF_REFUSED',
        'classification': 'original loader/registry identity capability only', 'native_credit': 0,
        'source_pins': pins, 'build_argv': command, 'test_executable_sha256': sha(exe), 'cases': cases,
        'summary': {'payloads': 70, 'literal_name_digest_pairs': 140, 'cases': len(cases),
            'status_histogram': dict(Counter(c['status'] for c in cases)),
            'outcome_histogram': dict(Counter((c['measurement'] or {}).get('outcome', 'no_result') for c in cases)),
            'equivalent_code_variant_cases': [c['original_sha256'] for c in cases if (c['measurement'] or {}).get('equivalent_code_variant')]},
        'input_changes': changes,
        'limits': ['No IRX entrypoint, interpreter, synthesized callback or game execution; this is loader identity proof only.',
                   'Existing native-module filename boundaries remain existing boundaries; buffer originals are selected independently.',
                   'Runtime executable/import identity cannot recover a unique original SHA for equivalent loaded-code variants.',
                   'No closure claim for unobserved executable families/import services or hardware load-delay semantics.']}
    target = args.output / 'RESULT.json'; target.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'result': str(target), 'sha256': sha(target), 'status': report['status'], 'summary': report['summary']}))
    return 0 if passed else 1


if __name__ == '__main__': raise SystemExit(main())
