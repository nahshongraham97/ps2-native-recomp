"""Add only original IOPMOD header-name literals to a pinned compiled registry.

No body is regenerated or recompiled. Existing source/object/compiler/header
identities must still match. Empty names are literal names, never defaults.
Private original IRX payloads stay in the existing ignored inventory.
"""
from __future__ import annotations
import argparse
import copy
import json
from pathlib import Path
import struct
import subprocess
import sys

import generate_r192_iop_aot as fixed
import build_r192_iop_v8_candidate as builder

W, require, sha = fixed.W, fixed.require, fixed.file_sha


def original_header(data):
    require(len(data) >= 52 and data[:7] == b'\x7fELF\x01\x01\x01', 'original_ELF32_LE_required')
    phoff = struct.unpack_from('<I', data, 28)[0]
    size, count = struct.unpack_from('<HH', data, 42)
    require(size >= 32 and phoff + size * count <= len(data), 'original_program_headers_out_of_bounds')
    offsets = [struct.unpack_from('<I', data, phoff + i * size + 4)[0] for i in range(count)
               if struct.unpack_from('<I', data, phoff + i * size)[0] == 0x70000080]
    require(len(offsets) == 1 and offsets[0] != 0, 'one_original_IOPMOD_required')
    at = offsets[0] + 26
    require(at < len(data), 'original_IOPMOD_name_out_of_bounds')
    bounded = data[at:at + min(len(data) - at, 56)]
    end = bounded.find(b'\0')
    require(end >= 0, 'original_IOPMOD_name_termination_unproved')
    name = bounded[:end].decode('ascii')
    require(all(32 <= c < 127 for c in bounded[:end]), 'original_IOPMOD_name_nonliteral')
    return {'name': name, 'file_offset': at, 'nul_terminated_bytes': bounded[:end + 1].hex(),
            'loader_rule': 'literal path==buffer; PT_IOPMOD+26; bounded56; case preserved'}


def registry_source(rows):
    lines = ['#include "iop/r192_pic.hpp"', '#include <cstring>', 'namespace ps2::iop::r192_aot {']
    cases, entries = [], []
    for row in rows:
        digest = row['original_sha256']; symbol = 'pic_template_' + digest[:16]
        lines.append(f'const PicTemplate &{symbol}();')
        for name in row['loader_names']:
            cases.append(f'    if (std::strcmp(name, {json.dumps(name)}) == 0 && std::strcmp(original_sha256, "{digest}") == 0) return &{symbol}();')
            entries.append(f'    {{{json.dumps(name)}, "{digest}", &{symbol}}},')
    return '\n'.join(lines + ['const PicTemplate *find_pic_template(const char *name, const char *original_sha256) {',
        '    if (!name || !original_sha256) return nullptr;', *cases, '    return nullptr;', '}',
        'namespace { const PicRegistryEntry entries[] = {', *entries, '}; }',
        'std::span<const PicRegistryEntry> pic_registry() { return entries; }', '}']) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('domain', 'runtime-root', 'compiler', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--expected-domain-sha', required=True)
    args = parser.parse_args()
    for name in ('domain', 'runtime_root', 'compiler', 'output'):
        setattr(args, name, getattr(args, name).resolve())
    require(args.output.resolve().is_relative_to(W / 'build') and not args.output.exists(), 'fresh_owned_output_required')
    require(sha(args.domain) == args.expected_domain_sha.lower(), 'compiled_domain_pin_mismatch')
    old = json.loads(args.domain.read_text())
    require(old['semantics'] == 'frozen_R192_v8_immediate_load_reference_compatibility' and
            old['summary']['compiled_PIC_bodies'] == 70 and len(old['records']) == 70, 'complete_frozen_v8_domain_required')
    require(sha(args.compiler) == old['compiler']['sha256'], 'compiler_identity_changed')
    kernel = args.runtime_root / 'src/runtime/src/iop/kernel.cpp'
    require(sha(kernel) == builder.KERNEL_SHA, 'original_loader_source_changed')
    pins = [dict(p) for p in old['source_pins']]
    for p in pins:
        require(sha(Path(p['path'])) == p['sha256'], 'compiled_domain_dependency_changed:' + p['path'])
    inventory_path = Path(old['frozen_inventory']['path'])
    require(sha(inventory_path) == old['frozen_inventory']['sha256'], 'original_inventory_changed')
    inventory = json.loads(inventory_path.read_text())
    originals = {}
    for row in inventory['records']:
        digest = row['original_sha256']; source = Path(row['original_copy'])
        require(sha(source) == digest, 'original_payload_changed')
        header = original_header(source.read_bytes())
        originals.setdefault(digest, (source, header))
        require(originals[digest][1] == header, 'one_digest_multiple_headers')
    require(len(originals) == 70, 'original_domain_incomplete')
    rows = copy.deepcopy(old['records'])
    require(rows == sorted(rows, key=lambda x: x['original_sha256']), 'original_registry_order_changed')
    # Authenticate the old name/getter matrix as well as its receipt.
    require(Path(old['registry']['path']).read_bytes() == registry_source(rows).encode(), 'old_registry_literal_matrix_changed')
    reused = []
    for row in rows:
        digest = row['original_sha256']; source, header = originals[digest]
        row['filename_loader_names'] = list(row['loader_names'])
        row['loader_names'] = sorted(set(row['loader_names'] + [header['name']]))
        row['original_header_name'] = dict(header, original_path=str(source), original_sha256=digest)
        require(len(row['object_compilations']) == 2, 'both_compiled_translation_units_required')
        for item in row['object_compilations']:
            require(item['exit_code'] == 0 and sha(Path(item['source'])) == item['source_sha256'] and
                    sha(Path(item['object'])) == item['object_sha256'], 'reused_source_or_object_changed')
            reused += [dict(item)]
    pins += [{'path': str(p.resolve()), 'sha256': sha(p)} for p in [Path(__file__), Path(builder.__file__), kernel,
             args.domain, inventory_path, args.compiler]]
    args.output.mkdir(parents=True, exist_ok=False)
    registry = args.output / 'iop_pic_registry.cpp'
    registry.write_text(registry_source(rows), encoding='utf-8', newline='\n')
    command = [str(args.compiler), '-std=c++20', '-O1', '-I' + str(args.runtime_root / 'src/runtime/src'),
               '-I' + str(W / 'runtime/src'), '-c', str(registry), '-o', str(registry.with_suffix('.obj'))]
    result = subprocess.run(command, capture_output=True, text=True)
    (args.output / 'registry-compile.log').write_text(result.stdout + result.stderr, encoding='utf-8', newline='\n')
    require(result.returncode == 0, 'expanded_registry_compile_failed')
    require(all(sha(Path(p['path'])) == p['sha256'] for p in pins), 'dependency_changed_during_expansion')
    require(all(sha(Path(p['source'])) == p['source_sha256'] and sha(Path(p['object'])) == p['object_sha256'] for p in reused),
            'object_changed_during_expansion')
    report = copy.deepcopy(old)
    report.update(source_pins=pins, records=rows, registry={'path': str(registry), 'sha256': sha(registry),
        'compile_command': command, 'compile_exit_code': result.returncode, 'object_sha256': sha(registry.with_suffix('.obj'))})
    report['summary']['reused_objects'] = len(reused)
    report['loader_name_extension'] = {'producer': str(Path(__file__).resolve()), 'producer_sha256': sha(Path(__file__)),
        'original_domain': {'path': str(args.domain), 'sha256': sha(args.domain)}, 'original_loader': {'path': str(kernel), 'sha256': sha(kernel)},
        'literal_pairs': sum(len(r['loader_names']) for r in rows), 'new_header_pairs': 70,
        'scope': 'Exact original header names plus old filename names only; empty is a literal, never default; no body/compiler/dispatcher change.',
        'limits': ['Original payload SHA is an offline registry key. Runtime selection proves relocated executable/import identity, not a unique original file SHA.',
                   'Equivalent executable variants may be indistinguishable after loading; aliases and all original inputs remain preserved.']}
    target = args.output / 'RESULT.json'
    target.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'result': str(target), 'sha256': sha(target), 'literal_pairs': report['loader_name_extension']['literal_pairs'], 'reused_objects': len(reused)}))


if __name__ == '__main__': main()
