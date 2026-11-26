"""Audit original IOP imports and control-shape obligations without gameplay.

Enumerates actual source-pinned native registrations, not regex-guessed names.
Original export slots are candidates only: data identity does not prove their
registration/start/lifetime or a complete native service implementation.
"""
from __future__ import annotations
import argparse
from collections import Counter, defaultdict
import json
import os
from pathlib import Path
import re
import struct
import subprocess

import generate_r192_iop_aot as fixed
import build_r192_iop_v8_candidate as builder
import expand_r192_iop_loader_names as names

W, require, sha = fixed.W, fixed.require, fixed.file_sha

REGISTRY_PROBE = r'''
#include R192_REFERENCE_KERNEL_SOURCE
#include "iop/r192_thbase42.hpp"
#include "ps2/system.hpp"
#include <fstream>
int main(int argc, char **argv) {
    if (argc != 2 || !ps2::g_options.memcard_path.empty()) return 2;
    ps2::iop::kernel::boot();
    ps2::iop::r192_thbase42::install();
    std::ofstream out(argv[1]);
    for (const auto &[key, id] : ps2::iop::kernel::g_hle_ids)
        out << key.first << '\t' << key.second << '\t' << id << '\n';
    return out ? 0 : 1;
}
'''


def u32(data, at): return struct.unpack_from('<I', data, at)[0]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('domain', 'runtime-root', 'compiler', 'output'):
        parser.add_argument('--' + key, type=Path, required=True)
    parser.add_argument('--expected-domain-sha', required=True)
    parser.add_argument('--expected-runtime-sha', required=True)
    args = parser.parse_args()
    for key in ('domain', 'runtime_root', 'compiler', 'output'):
        setattr(args, key, getattr(args, key).resolve())
    require(args.output.is_relative_to(W / 'build') and not args.output.exists(), 'fresh_owned_output_required')
    require(sha(args.domain) == args.expected_domain_sha.lower(), 'expanded_domain_pin_mismatch')
    domain = json.loads(args.domain.read_text())
    require(domain['summary']['compiled_PIC_bodies'] == 70 and domain['loader_name_extension']['literal_pairs'] == 140,
            'complete_original_alias_domain_required')
    require(sha(args.compiler) == domain['compiler']['sha256'], 'compiler_identity_changed')
    archive = args.runtime_root / 'out/libps2runtime.a'; kernel = args.runtime_root / 'src/runtime/src/iop/kernel.cpp'
    require(sha(archive) == args.expected_runtime_sha.lower() and sha(kernel) == builder.KERNEL_SHA, 'reference_source_or_archive_changed')
    inventory_path = Path(domain['frozen_inventory']['path'])
    require(sha(inventory_path) == domain['frozen_inventory']['sha256'], 'original_inventory_changed')
    old = json.loads(inventory_path.read_text()); originals = {}
    for row in old['records']: originals.setdefault(row['original_sha256'], row)
    require(len(originals) == 70, 'original_payload_domain_changed')
    pins = list(domain['source_pins']) + [{'path': str(p.resolve()), 'sha256': sha(p)} for p in
        [Path(__file__), Path(builder.__file__), Path(names.__file__), args.domain, kernel, archive,
         W / 'runtime/src/iop/r192_thbase42.hpp', W / 'runtime/src/iop/r192_aot.cpp', W / 'runtime/src/iop/r192_pic.cpp',
         W / 'runtime/src/iop/r192_v8_dispatch.inc', W / 'scripts/iop_irx_imports.py']]
    service_sources = sorted((args.runtime_root / 'src/runtime/src/iop').glob('*.cpp'))
    pins += [{'path': str(p), 'sha256': sha(p)} for p in service_sources]
    objects = []
    for row in domain['records']:
        for item in row['object_compilations']:
            require(sha(Path(item['source'])) == item['source_sha256'] and sha(Path(item['object'])) == item['object_sha256'],
                    'compiled_object_identity_changed')
            objects.append(Path(item['object']))
            pins += [{'path': item['object'], 'sha256': item['object_sha256']}, {'path': item['source'], 'sha256': item['source_sha256']}]
    registry = Path(domain['registry']['path']); registry_obj = registry.with_suffix('.obj')
    require(sha(registry) == domain['registry']['sha256'] and sha(registry_obj) == domain['registry']['object_sha256'], 'registry_changed')
    pins += [{'path': str(p), 'sha256': sha(p)} for p in [registry, registry_obj]]
    require(all(sha(Path(p['path'])) == p['sha256'] for p in pins), 'initial_source_pin_changed')
    args.output.mkdir(parents=True, exist_ok=False)
    private = args.output / 'private-kernel.cpp'
    private.write_text(builder.prepare_kernel(kernel.read_bytes(), W / 'runtime/src/iop/r192_v8_dispatch.inc'), encoding='utf-8', newline='\n')
    probe = args.output / 'registry-probe.cpp'; probe.write_text(REGISTRY_PROBE, encoding='utf-8', newline='\n')
    exe = args.output / 'registry-probe.exe'; table = args.output / 'native-registrations.tsv'
    command = [str(args.compiler), '-std=c++20', '-O2', '-mavx2', '-static-libgcc', '-static-libstdc++',
        '-DR192_REFERENCE_KERNEL_SOURCE="' + private.as_posix() + '"',
        '-I' + str(args.runtime_root / 'src/runtime/include'), '-I' + str(args.runtime_root / 'src/runtime/src'),
        '-I' + str(W / 'runtime/src'), str(probe), str(W / 'runtime/src/iop/r192_aot.cpp'), str(W / 'runtime/src/iop/r192_pic.cpp'),
        *(str(p) for p in objects), str(registry_obj), str(archive), '-lbcrypt', '-lwinmm',
        str(args.runtime_root / 'src/third_party/sdl2/lib/libSDL2.dll.a'), str(args.runtime_root / 'src/third_party/wgpu-native/lib/libwgpu_native.a'),
        '-lws2_32', '-luserenv', '-lntdll', '-lsynchronization', '-lpthread', '-lm', '-lkernel32', '-luser32', '-lgdi32',
        '-lwinspool', '-lshell32', '-lole32', '-loleaut32', '-luuid', '-lcomdlg32', '-ladvapi32', '-o', str(exe)]
    rsp = args.output / 'build.rsp'; rsp.write_text('\n'.join(builder.quote_response(x) for x in command[1:]) + '\n', encoding='utf-8', newline='\n')
    built = subprocess.run([command[0], '@' + str(rsp)], capture_output=True, text=True)
    (args.output / 'build.log').write_text(built.stdout + built.stderr, encoding='utf-8', newline='\n')
    require(built.returncode == 0, 'native_registry_probe_compile_failed')
    env = {k: v for k, v in os.environ.items() if not k.startswith(('PS2_', 'MKA_', 'SDL_'))}
    env['PATH'] = str(args.runtime_root / 'out') + os.pathsep + str(args.compiler.parent) + os.pathsep + env.get('PATH', '')
    ran = subprocess.run([str(exe), str(table)], capture_output=True, text=True, env=env, timeout=60)
    (args.output / 'run.log').write_text(ran.stdout + ran.stderr, encoding='utf-8', newline='\n')
    require(ran.returncode == 0, 'native_registry_probe_failed')
    native = {(v[0], int(v[1])): int(v[2]) for line in table.read_text().splitlines() if (v := line.split('\t'))}
    require(('thbase', 42) in native and not any(k[0] == 'not-a-real-library' for k in native), 'source_supported_registry_controls_failed')
    irx = fixed.load_tool('closure_original_irx', W / 'scripts/iop_irx_imports.py')
    requests, providers, payloads = defaultdict(list), defaultdict(list), []
    for digest, row in sorted(originals.items()):
        path = Path(row['original_copy']); raw = path.read_bytes(); require(fixed.sha(raw) == digest, 'original_IRX_changed')
        pins.append({'path': str(path), 'sha256': digest})
        image, load, _ = irx._parse_elf_load_image(raw)
        imports = irx.scan_import_tables(image); relocs = irx.scan_relocations(raw, len(image))
        pointer_sites = {r['offset'] for r in relocs if r['type'] == 2}
        text_size = row['text_size']; headers = []
        for at in range(0, len(image) - 20 + 1, 4):
            if u32(image, at) not in (0x41C00000, 0x41E00000): continue
            name = bytes(image[at + 12:at + 20]).split(b'\0', 1)[0].decode('ascii', errors='ignore')
            if re.fullmatch(r'[A-Za-z0-9_]{1,8}', name): headers.append((at, u32(image, at), name))
        for tab in imports:
            for entry in tab['imports']:
                requests[(tab['library'], entry['ordinal'])].append({'payload_sha256': digest, 'alias': row['alias'],
                    'stub_image_offset': entry['image_offset'], 'stub_file_offset': load['file_offset'] + entry['image_offset'],
                    'original_words': ['03E00008', f'{0x24000000 | entry["ordinal"]:08X}'], 'requested_version': tab['version']})
        for index, (at, magic, library) in enumerate(headers):
            if magic != 0x41C00000: continue
            bound = headers[index + 1][0] if index + 1 < len(headers) else len(image)
            for site in sorted(x for x in pointer_sites if at + 20 <= x < bound):
                target = u32(image, site)
                if target & 3 or target >= text_size: continue
                providers[(library, (site - at - 20) // 4)].append({'payload_sha256': digest, 'alias': row['alias'],
                    'table_image_offset': at, 'slot_image_offset': site, 'slot_file_offset': load['file_offset'] + site,
                    'target_image_offset': target, 'target_word': f'{u32(image, target):08X}', 'relocation_type': 'R_MIPS_32',
                    'status': 'bounded_original_export_slot_candidate_not_registered_or_executed',
                    'extent_limit': 'next recognized table header or load image end; table semantic extent remains unproved'})
        shapes = defaultdict(list)
        for at in range(0, text_size, 4):
            word = u32(image, at); op = word >> 26; fn = word & 63; rs = (word >> 21) & 31
            kind = 'JALR_dynamic_target' if op == 0 and fn == 9 else 'JR_dynamic_target' if op == 0 and fn == 8 and rs != 31 else 'ordinary_JR_RA_return' if op == 0 and fn == 8 else 'SYSCALL' if op == 0 and fn == 12 else 'BREAK' if op == 0 and fn == 13 else 'COP0' if op == 16 else None
            if kind: shapes[kind].append({'image_offset': at, 'file_offset': load['file_offset'] + at, 'word': f'{word:08X}'})
        payloads.append({'original_sha256': digest, 'alias': row['alias'], 'original_path': str(path), 'text_size': text_size,
            'all_aligned_text_control_shape_candidates': dict(shapes), 'rooted_CFG_edges': row['CFG']['edges'],
            'rooted_CFG_frontiers': row['CFG']['unresolved_or_exception_frontiers'], 'old_first_refusal': row['first_refusal']})
    obligations = []
    for key, uses in sorted(requests.items()):
        obligations.append({'library': key[0], 'ordinal': key[1], 'original_import_sites': uses,
            'boot_native_registration': {'id': native[key], 'status': 'existing_source_registration_not_full_service_semantic_proof'} if key in native else None,
            'original_export_candidates': providers.get(key, []),
            'status': 'source_registered_native_boundary' if key in native else 'native_missing_original_provider_candidate' if key in providers else 'native_missing_no_bounded_original_provider'})
    service_source_markers = [{'path': str(p), 'sha256': sha(p), 'lines': [{'line': i + 1, 'text': line.strip()} for i, line in enumerate(p.read_text().splitlines())
        if ('register_hle(' in line or 'def(' in line or 'define(' in line or 'register_hle_library(' in line)]} for p in service_sources]
    changes = [p['path'] for p in pins if sha(Path(p['path'])) != p['sha256']]
    require(not changes, 'source_changed_during_closure_audit')
    report = {'status': 'SOURCE_BOUNDED_IOP_CLOSURE_AUDIT_COMPLETE', 'classification': 'closure obligations, not executable-family acceptance', 'native_credit': 0,
        'source_pins': pins, 'build_argv': command, 'registry_probe_cpp_sha256': sha(probe), 'registry_probe_exe_sha256': sha(exe),
        'actual_native_registration_table': {'path': str(table), 'sha256': sha(table), 'rows': len(native)}, 'obligations': obligations,
        'payloads': payloads, 'native_service_source_markers': service_source_markers,
        'summary': {'original_payloads': len(payloads), 'distinct_import_requirements': len(obligations), 'import_libraries': len({k[0] for k in requests}),
            'distinct_library_version_ordinal_requirements': len({(r['library'], use['requested_version'], r['ordinal']) for r in obligations for use in r['original_import_sites']}),
            'import_status_histogram': dict(Counter(r['status'] for r in obligations)),
            'missing_by_library': {lib: [r['ordinal'] for r in obligations if r['library'] == lib and not r['boot_native_registration']]
                for lib in sorted({r['library'] for r in obligations if not r['boot_native_registration']})},
            'aligned_text_shape_candidates': dict(sum((Counter({k: len(v) for k, v in p['all_aligned_text_control_shape_candidates'].items()}) for p in payloads), Counter())),
            'rooted_edge_kind_histogram': dict(Counter(e['kind'] for p in payloads for e in p['rooted_CFG_edges']))},
        'input_changes': changes,
        'limits': ['Boot registration presence is source availability, not verified service semantics or actual module-entry execution.',
            'Original export-slot candidates require actual registration, table extent, loaded version/lifetime and target proof before they can replace a missing native boundary.',
            'Aligned text includes possible embedded data and import stubs; shapes are candidate obligations, not invented reachable instruction claims.',
            'Rooted CFG is a bounded original prior analysis. Register-target closure, data-derived callbacks, exceptions, scheduler/context and code writes remain explicit.',
            'Unknown/synthesized import execution must still refuse by name; no callback is invoked here.']}
    target = args.output / 'RESULT.json'; target.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'result': str(target), 'sha256': sha(target), 'summary': report['summary']}))


if __name__ == '__main__': main()
