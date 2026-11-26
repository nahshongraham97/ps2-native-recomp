"""Compile one relocatable body for every frozen disc/ROMDIR IRX payload.

Consumes an explicitly pinned original inventory. Private payload copies and
generated source stay in a fresh owned build directory. No runtime admission,
production binary modification, or native-port credit follows from compiling.
"""
from __future__ import annotations
import argparse
from collections import Counter, defaultdict
import json
from pathlib import Path
import subprocess
import sys
import time

import generate_r192_iop_aot as fixed
import generate_r192_iop_pic as pic

W = fixed.W
require = fixed.require
sha = fixed.file_sha


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('inventory', 'donor-generator', 'compiler', 'runtime-include', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--expected-inventory-sha', required=True)
    parser.add_argument('--frozen-r192-v8-cpu', type=Path)
    parser.add_argument('--reuse-domain', type=Path)
    parser.add_argument('--expected-reuse-sha')
    args = parser.parse_args()
    require(args.output.resolve().is_relative_to(W / 'build') and not args.output.exists(), 'fresh_owned_output_required')
    require(sha(args.inventory) == args.expected_inventory_sha.lower(), 'frozen_original_inventory_pin_mismatch')
    old = json.loads(args.inventory.read_text())
    require(old['inputs']['ISO_sha256'] == fixed.ISO_SHA, 'original_disc_identity_mismatch')
    require(sha(args.donor_generator) == fixed.DONOR_SHA, 'donor_pin_mismatch')
    require(sha(args.compiler) == old['inputs']['compiler_sha256'], 'compiler_identity_changed')
    require(old['summary']['total_source_aliases'] == 73 and old['summary']['unique_original_ELF_bytes'] == 70,
            'frozen_domain_denominator_changed')
    by_hash = defaultdict(list)
    for row in old['records']:
        require(sha(Path(row['original_copy'])) == row['original_sha256'], 'frozen_original_payload_changed')
        by_hash[row['original_sha256']].append(row)
    require(len(by_hash) == 70 and len({r['alias'] for r in old['records']}) == 73, 'incomplete_original_domain')
    reuse = {}
    reuse_sha = None
    if args.reuse_domain:
        require(args.expected_reuse_sha and sha(args.reuse_domain) == args.expected_reuse_sha.lower(), 'reused_domain_pin_mismatch')
        previous = json.loads(args.reuse_domain.read_text())
        require(previous['compiler']['sha256'] == sha(args.compiler), 'reused_compiler_changed')
        for pin in previous['source_pins']:
            path = Path(pin['path'])
            if path.name in ('iop_native.h', 'iop_native_support.h', 'r192_aot.hpp', 'r192_pic.hpp', 'iop.hpp'):
                require(sha(path) == pin['sha256'], 'reused_translation_unit_header_changed')
        for row in previous['records']:
            for compiled in row['object_compilations']:
                if compiled['exit_code'] != 0:
                    continue
                require(sha(Path(compiled['source'])) == compiled['source_sha256'] and
                        sha(Path(compiled['object'])) == compiled['object_sha256'], 'reused_CPP_or_object_changed')
                reuse[(row['original_sha256'], Path(compiled['source']).name)] = compiled
        reuse_sha = args.expected_reuse_sha.lower()
    args.output.mkdir(parents=True, exist_ok=False)
    extra_pins = []
    if args.frozen_r192_v8_cpu:
        import r192_iop_v8_semantics
        r192_iop_v8_semantics.validate_source(args.frozen_r192_v8_cpu)
        extra_pins = [Path(r192_iop_v8_semantics.__file__), args.frozen_r192_v8_cpu]
    pins = [{'path': str(p.resolve()), 'sha256': sha(p)} for p in
            [Path(__file__), Path(pic.__file__), Path(fixed.__file__), W / 'scripts/iop_irx_imports.py',
             W / 'runtime/src/native/iop_native.h', W / 'runtime/src/native/iop_native_support.h',
             W / 'runtime/src/iop/r192_aot.hpp', W / 'runtime/src/iop/r192_pic.hpp', args.donor_generator,
             args.runtime_include / 'iop/iop.hpp', args.inventory] + extra_pins]
    rows = []
    registry = ['#include "iop/r192_pic.hpp"', '#include <cstring>', 'namespace ps2::iop::r192_aot {']
    registry_cases = []
    registry_entries = []
    for slot, (digest, aliases) in enumerate(sorted(by_hash.items())):
        directory = args.output / f'payload-{slot:03}'
        source = Path(aliases[0]['original_copy'])
        symbol = 'pic_template_' + digest[:16]
        command = [sys.executable, str(Path(pic.__file__)), '--original-irx', str(source), '--expected-irx-sha', digest,
                   '--donor-generator', str(args.donor_generator), '--module-name', aliases[0]['loader_name'],
                   '--template-symbol', symbol, '--output', str(directory)]
        if args.frozen_r192_v8_cpu:
            command += ['--frozen-r192-v8-cpu', str(args.frozen_r192_v8_cpu)]
        began = time.monotonic()
        generation = subprocess.run(command, capture_output=True, text=True)
        # A failed generator can still leave useful private intermediates.
        directory.mkdir(parents=True, exist_ok=True)
        (directory / 'generation.log').write_text(generation.stdout + generation.stderr, encoding='utf-8', newline='\n')
        row = {'original_sha256': digest, 'aliases': sorted({x['alias'] for x in aliases}),
               'loader_names': sorted({x['loader_name'] for x in aliases}),
               'original_copy': str(source), 'original_origins': [x['origin'] for x in aliases],
               'old_placement_analysis': [{'alias': x['alias'], 'placement_authority': x['placement_authority'],
                                         'first_refusal': x['first_refusal']} for x in aliases],
               'generation_command': command, 'generation_exit_code': generation.returncode,
               'classification': 'capability only', 'runtime_admitted': False, 'native_credit': 0,
               'object_compilations': []}
        if generation.returncode != 0:
            row.update(status='PIC_GENERATION_REFUSED', first_refusal=(generation.stderr.strip().splitlines() or ['unknown_generator_failure'])[-1])
        else:
            measured = json.loads((directory / 'RESULT.json').read_text())
            row['generation_receipt_sha256'] = sha(directory / 'RESULT.json')
            row['text_size'] = measured['text_size']
            row['relocation_counts'] = dict(Counter(str(x['type']) for x in measured['relocations']))
            for filename in ('iop_pic.cpp', 'iop_pic_descriptor.cpp'):
                cpp = directory / filename
                obj = cpp.with_suffix('.obj')
                previous = reuse.get((digest, filename))
                if previous and sha(cpp) == previous['source_sha256']:
                    row['object_compilations'].append(dict(previous, source=str(cpp),
                        reused_original_source=previous['source'], reused_domain_sha256=reuse_sha))
                    continue
                cmd = [str(args.compiler), '-std=c++20', '-O1', '-I' + str(args.runtime_include),
                       '-I' + str(W / 'runtime/src'), '-c', str(cpp), '-o', str(obj)]
                result = subprocess.run(cmd, capture_output=True, text=True)
                (directory / (filename + '.compile.log')).write_text(result.stdout + result.stderr, encoding='utf-8', newline='\n')
                row['object_compilations'].append({'argv': cmd, 'exit_code': result.returncode,
                    'source': str(cpp), 'source_sha256': sha(cpp), 'object': str(obj),
                    'object_sha256': sha(obj) if obj.exists() else None})
            if all(x['exit_code'] == 0 for x in row['object_compilations']):
                row.update(status='PIC_COMPILED_CAPABILITY_ONLY', first_refusal=None)
                registry.append(f'const PicTemplate &{symbol}();')
                for name in row['loader_names']:
                    registry_cases.append(f'    if (std::strcmp(name, {json.dumps(name)}) == 0 && std::strcmp(original_sha256, "{digest}") == 0) return &{symbol}();')
                    registry_entries.append(f'    {{{json.dumps(name)}, "{digest}", &{symbol}}},')
            else:
                row.update(status='PIC_CPP_COMPILE_REFUSED', first_refusal='actual_Clang_object_compilation_failed')
        row['seconds'] = time.monotonic() - began
        rows.append(row)
        (args.output / 'PROGRESS.json').write_text(json.dumps({'finished_unique_payloads': len(rows), 'total': len(by_hash),
            'histogram': dict(Counter(x['status'] for x in rows))}, indent=2) + '\n', encoding='utf-8', newline='\n')
        print(json.dumps({'finished': len(rows), 'total': len(by_hash), 'status': row['status'], 'aliases': row['aliases']}), flush=True)
    registry += ['const PicTemplate *find_pic_template(const char *name, const char *original_sha256) {',
                 '    if (!name || !original_sha256) return nullptr;', *registry_cases, '    return nullptr;', '}',
                 'namespace { const PicRegistryEntry entries[] = {', *registry_entries, '}; }',
                 'std::span<const PicRegistryEntry> pic_registry() { return entries; }', '}']
    registry_path = args.output / 'iop_pic_registry.cpp'
    registry_path.write_text('\n'.join(registry) + '\n', encoding='utf-8', newline='\n')
    command = [str(args.compiler), '-std=c++20', '-O1', '-I' + str(args.runtime_include), '-I' + str(W / 'runtime/src'),
               '-c', str(registry_path), '-o', str(args.output / 'iop_pic_registry.obj')]
    compiled = subprocess.run(command, capture_output=True, text=True)
    (args.output / 'registry-compile.log').write_text(compiled.stdout + compiled.stderr, encoding='utf-8', newline='\n')
    changes = [x['path'] for x in pins if sha(Path(x['path'])) != x['sha256']]
    require(not changes, 'source_changed_during_PIC_domain_measurement')
    receipt = {'status': 'PIC_DOMAIN_MEASURED', 'classification': 'capability only', 'native_credit': 0,
               'semantics': 'frozen_R192_v8_immediate_load_reference_compatibility' if args.frozen_r192_v8_cpu else 'R3000_queued_loads',
               'source_pins': pins, 'compiler': {'path': str(args.compiler), 'sha256': sha(args.compiler)},
               'frozen_inventory': {'path': str(args.inventory), 'sha256': args.expected_inventory_sha.lower()},
               'summary': {'source_aliases': 73, 'unique_original_payloads': 70,
                           'generated_PIC_bodies': sum(r['generation_exit_code'] == 0 for r in rows),
                           'compiled_PIC_bodies': sum(r['status'] == 'PIC_COMPILED_CAPABILITY_ONLY' for r in rows),
                           'reused_objects': sum('reused_domain_sha256' in c for r in rows for c in r['object_compilations']),
                           'first_refusal_histogram': dict(Counter(r['first_refusal'] or 'none_at_object_compile_scope' for r in rows)),
                           'runtime_admissions': 0, 'native_credit': 0}, 'records': rows,
               'registry': {'path': str(registry_path), 'sha256': sha(registry_path), 'compile_command': command,
                            'compile_exit_code': compiled.returncode,
                            'object_sha256': sha(args.output / 'iop_pic_registry.obj') if compiled.returncode == 0 else None},
               'limits': ['Compiling every payload does not prove all execution paths, import/services, dynamic destinations or runtime entry admission.',
                          'Original fixed-domain CFG/branch frontiers remain per alias; none are silently cleared by PIC compilation.',
                          'Frozen R192 immediate-load semantics versus queued R3000 loads remain a separate unclosed obligation.',
                          'Unknown original image/name has no registry default; no runtime compiler/interpreter fallback is introduced.']}
    receipt['reused_measurement'] = {'path': str(args.reuse_domain), 'sha256': reuse_sha} if args.reuse_domain else None
    result = args.output / 'RESULT.json'
    result.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'result': str(result), 'sha256': sha(result), 'summary': receipt['summary']}))


if __name__ == '__main__':
    main()
