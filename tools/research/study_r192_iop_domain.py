"""Inventory and offline-generate every disc IRX and IOPRP ELF member.

All aliases and byte variants remain visible. An observed load placement is not
invented for unobserved modules: their relocation/generation uses an explicitly
labelled capability-only test base. No generated body is admitted to gameplay.
"""
from __future__ import annotations
import argparse
from collections import Counter
import json
from pathlib import Path
import re
import struct
import subprocess
import sys

from generate_r192_iop_aot import W, ISO_SHA, DONOR_SHA, require, sha, file_sha, load_tool, sections, relocate, romdir, code_cfg, u32


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('iso', 'donor-generator', 'output', 'placement-map', 'iop-ram', 'compiler'):
        parser.add_argument('--'+name, type=Path, required=True)
    parser.add_argument('--expected-placement-sha', required=True)
    parser.add_argument('--expected-ram-sha', required=True)
    parser.add_argument('--runtime-include', type=Path, required=True)
    parser.add_argument('--reuse-compiled-result', type=Path)
    parser.add_argument('--expected-reuse-sha')
    args = parser.parse_args()
    require(args.output.resolve().is_relative_to(W/'build') and not args.output.exists(), 'fresh_owned_output_required')
    require(file_sha(args.iso) == ISO_SHA, 'original_ISO_pin_mismatch')
    require(file_sha(args.donor_generator) == DONOR_SHA, 'offline_generator_pin_mismatch')
    require(file_sha(args.placement_map) == args.expected_placement_sha.lower(), 'placement_map_pin_mismatch')
    require(file_sha(args.iop_ram) == args.expected_ram_sha.lower(), 'IOP_RAM_pin_mismatch')
    ram = args.iop_ram.read_bytes()
    require(len(ram) == 8 << 20, 'wrong_IOP_RAM_size')
    placements = {}
    for line in args.placement_map.read_text().splitlines():
        fields = line.split()
        require(len(fields) == 6, 'invalid_original_runtime_module_map')
        placements.setdefault(fields[1], []).append({'base': int(fields[2], 16), 'size': int(fields[3], 16),
                                                   'entry': int(fields[4], 16), 'gp': int(fields[5], 16)})
    disc = load_tool('r192_domain_disc', W/'scripts/disc.py')
    irx = load_tool('r192_domain_irx', W/'scripts/iop_irx_imports.py')
    capture = load_tool('r192_domain_capture', W/'scripts/iop_capture_manifest.py')
    decoder = load_tool('r192_domain_decoder', args.donor_generator)
    decoder.OVERRIDES = {}
    args.output.mkdir(parents=True, exist_ok=False)
    originals = args.output/'original-images'; originals.mkdir()
    image = disc.Image(str(args.iso))
    domain, containers = [], []
    for name, layer, sector, size in image.files():
        if not name.upper().endswith(('.IRX', '.IMG')):
            continue
        image.file.seek(sector*2048)
        raw = image.file.read(size)
        require(len(raw) == size, 'truncated_disc_member')
        if name.upper().endswith('.IRX'):
            domain.append((name, raw, {'kind': 'disc_IRX', 'ISO_byte_offset': sector*2048, 'layer': layer.index}))
        else:
            members = romdir(raw)
            containers.append({'path': name, 'sha256': sha(raw), 'ISO_byte_offset': sector*2048, 'ROMDIR': members})
            for member in members:
                if member['ELF']:
                    at = member['image_offset']
                    blob = raw[at:at+member['size']]
                    domain.append((name+'!'+member['name'], blob,
                                   {'kind': 'IOPRP_ELF', 'container_sha256': sha(raw), 'image_offset': at,
                                    'ISO_byte_offset': sector*2048+at, 'layer': layer.index}))
    require(sum(origin['kind'] == 'disc_IRX' for _, _, origin in domain) == 41 and len(containers) == 2,
            'disc_IOP_domain_changed')
    records, generated_cache = [], {}
    reuse = None
    if args.reuse_compiled_result:
        require(args.expected_reuse_sha and file_sha(args.reuse_compiled_result) == args.expected_reuse_sha.lower(), 'reused_measurement_pin_mismatch')
        reuse = json.loads(args.reuse_compiled_result.read_text())
        expected_sources = {Path(x['path']).name: x['sha256'] for x in reuse['source_dependencies']}
        for path in (W/'scripts/iop_reference_codegen.py', W/'scripts/iop_irx_imports.py', args.donor_generator):
            require(file_sha(path) == expected_sources[path.name], 'reused_codegen_source_changed')
        require(file_sha(args.compiler) == reuse['inputs']['compiler_sha256'], 'reused_compiler_changed')
        for row in reuse['records']:
            if 'emitted' not in row:
                continue
            emitted = row['emitted']
            require(file_sha(Path(emitted['cpp'])) == emitted['cpp_sha256'], 'reused_emitted_source_changed')
            if emitted['object']:
                require(file_sha(Path(emitted['object'])) == emitted['object_sha256'], 'reused_object_changed')
            generated_cache[(row['original_sha256'], row['placement']['base'], row['loader_name'])] = dict(
                emitted, reused_measurement_sha256=args.expected_reuse_sha.lower())
    for number, (name, raw, origin) in enumerate(domain):
        digest = sha(raw)
        original = originals/(digest+'.elf')
        if not original.exists():
            with original.open('xb') as stream:
                require(stream.write(raw) == len(raw), 'short_original_copy')
        require(file_sha(original) == digest, 'original_copy_hash_mismatch')
        filename = name.split('!')[-1]
        loader_name = Path(filename).stem.upper()
        observed = placements.get(loader_name, [])
        # Compile each observed placement independently. A missing observation
        # uses a test placement only, with runtime admission explicitly false.
        targets = observed or [{'base': 0x20000, 'size': None, 'entry': None, 'gp': None}]
        for placement in targets:
            row = {'alias': name, 'origin': origin, 'original_sha256': digest, 'original_copy': str(original.resolve()),
                   'loader_name': loader_name, 'placement': placement,
                   'placement_authority': 'actual_runtime_parsed_snapshot_module_map' if observed else 'explicit_capability_test_base_not_observed',
                   'runtime_admitted': False, 'native_credit': 0}
            area = args.output/f'module-{len(records):03}'; area.mkdir()
            row['artifact_directory'] = str(area.resolve())
            stage = 'parse_IRX'
            try:
                unrelocated, load, header_name = irx._parse_elf_load_image(raw)
                row['header_name'] = header_name
                row['load'] = load
                require(load['virtual_address'] == 0, 'nonzero_IRX_link_VA_not_proved')
                executable = [x for x in sections(raw) if x['flags'] & 4 and x['size']]
                require(len(executable) == 1 and executable[0]['VA'] == 0, 'noncontiguous_IRX_text_not_proved')
                text_size = executable[0]['size']; row['text_size'] = text_size
                row['entry_offset'] = u32(raw, 24)
                relocs = irx.scan_relocations(raw, len(unrelocated))
                row['relocation_counts'] = {str(k): v for k, v in Counter(x['type'] for x in relocs).items()}
                stage = 'apply_original_relocations'
                linked, joins = relocate(raw, unrelocated, placement['base'], relocs)
                text = linked[:text_size]
                row['relocated_text_sha256'] = sha(text)
                row['relocated_text_fnv1a64'] = f'{capture.fnv1a64(text):016X}'
                row['relocation_word_joins'] = joins
                imports = irx.scan_import_tables(linked)
                exports = irx.scan_export_tables(unrelocated)
                row['imports'] = imports
                row['import_count'] = sum(len(x['imports']) for x in imports)
                row['exports'] = exports
                stage = 'original_code_CFG'
                row['CFG'] = code_cfg(text, placement['base'], row['entry_offset'], imports, exports, relocs, unrelocated, decoder)
                row['execution_obligations'] = {
                    'native_import_bindings_not_connected_in_this_stage': row['import_count'],
                    'dynamic_register_destinations': sum(x['kind'] == 'unresolved_register_destination' for x in row['CFG']['edges']),
                    'ordinary_JR_RA_returns': sum(x['kind'] == 'ordinary_JR_RA_return' for x in row['CFG']['edges']),
                    'observed_placement': bool(observed),
                    'R3000_queued_load_vs_frozen_R192_semantics': 'requires_caller_differential_before_deployment',
                }
                if observed and placement['base']+text_size <= len(ram):
                    current = ram[placement['base']:placement['base']+text_size]
                    stub_words = {entry['image_offset']+delta for table in imports for entry in table['imports'] for delta in (0, 4)}
                    differences = [at for at in range(0, text_size, 4) if text[at:at+4] != current[at:at+4]]
                    outside = [at for at in differences if at not in stub_words]
                    row['saved_RAM_code_join'] = {'all_non_import_words_match': not outside,
                                                  'different_word_count': len(differences), 'outside_import_offsets': outside}
                key = (digest, placement['base'], loader_name)
                if key not in generated_cache:
                    captures = area/'captures'; captures.mkdir()
                    ident = f'{loader_name}_B{placement["base"]:08X}_H{capture.fnv1a64(text):016X}'
                    (captures/(ident+'.bin')).write_bytes(text)
                    (captures/'module-identities.txt').write_text(f'{ident} {loader_name}\n', encoding='ascii', newline='\n')
                    (captures/'modules.txt').write_text(f'{ident} {placement["base"]:08X} {text_size:X} {len(linked):X} {capture.fnv1a64(text):016X}\n', encoding='ascii', newline='\n')
                    command = [sys.executable, str(W/'scripts/iop_reference_codegen.py'), str(captures), str(args.donor_generator), str(area/'generated')]
                    stage = 'generate_AOT_Cpp'
                    result = subprocess.run(command, capture_output=True, text=True, timeout=180)
                    (area/'generate.log').write_text(result.stdout+result.stderr, encoding='utf-8', newline='\n')
                    row['generator_command'] = command
                    require(result.returncode == 0, 'IOP_AOT_generator_failed')
                    registry = json.loads((area/'generated/capture-registry.json').read_text())
                    require(len(registry) == 1, 'invalid_single_module_registry')
                    cpp = area/'generated'/registry[0]['source']
                    stage = 'compile_emitted_Cpp'
                    obj = area/'module.obj'
                    command = [str(args.compiler), '-std=c++20', '-O1', '-I'+str(args.runtime_include),
                               '-I'+str(W/'runtime/src'), '-c', str(cpp), '-o', str(obj)]
                    built = subprocess.run(command, capture_output=True, text=True, timeout=180)
                    (area/'compile.log').write_text(built.stdout+built.stderr, encoding='utf-8', newline='\n')
                    generated_cache[key] = {'registry': registry[0], 'cpp': str(cpp.resolve()), 'cpp_sha256': file_sha(cpp),
                                            'compile_command': command, 'compile_exit_code': built.returncode,
                                            'object': str(obj.resolve()) if obj.exists() else None,
                                            'object_sha256': file_sha(obj) if obj.exists() else None}
                emitted = generated_cache[key]
                require(emitted['registry']['base'] == placement['base'] and emitted['registry']['size'] == text_size and
                        emitted['registry']['fnv1a64'] == row['relocated_text_fnv1a64'], 'reused_original_word_identity_mismatch')
                row['emitted'] = emitted
                stage = 'strict_module_admission'
                if emitted['registry']['interpreter_fallback_candidates']:
                    row['first_refusal'] = 'branch_in_delay_candidates_unproved'
                elif emitted['registry']['unmapped_stub_candidates']:
                    row['first_refusal'] = 'unmapped_import_stub_candidates'
                elif emitted['compile_exit_code'] != 0:
                    row['first_refusal'] = 'emitted_Cpp_compile_failed'
                elif row['CFG']['unresolved_or_exception_frontiers']:
                    row['first_refusal'] = 'rooted_CFG_frontier_unproved'
                else:
                    row['first_refusal'] = None
                row['status'] = 'COMPILED_CAPABILITY_ONLY' if row['first_refusal'] is None else 'REFUSED_CAPABILITY_WITH_ARTIFACTS'
            except (OSError, ValueError, struct.error, subprocess.TimeoutExpired) as exc:
                row['status'] = 'REFUSED'
                row['first_refusal'] = stage+':'+str(exc)
            records.append(row)
            print(json.dumps({'completed': len(records), 'alias': name, 'base': placement['base'],
                              'status': row['status'], 'first_refusal': row['first_refusal']}), flush=True)
    summary = {'disc_IRX_aliases': 41, 'IOPRP_images': len(containers),
               'IOPRP_ELF_aliases': sum(origin['kind'] == 'IOPRP_ELF' for _, _, origin in domain),
               'total_source_aliases': len(domain), 'unique_original_ELF_bytes': len({sha(raw) for _, raw, _ in domain}),
               'placement_rows': len(records), 'unique_generated_body_scopes': len(generated_cache),
               'compiled_objects': sum(x['compile_exit_code'] == 0 for x in generated_cache.values()),
               'reused_compiled_body_scopes': sum('reused_measurement_sha256' in x for x in generated_cache.values()),
               'first_refusal_histogram': dict(Counter(row['first_refusal'] or 'none_at_measured_compile_scope' for row in records)),
               'row_statuses': dict(Counter(row['status'] for row in records)), 'runtime_admissions': 0, 'native_credit': 0}
    receipt = {'status': 'COMPLETE_DOMAIN_MEASUREMENT_WITH_EXPLICIT_REFUSALS',
               'source': {'path': str(Path(__file__).resolve()), 'sha256': file_sha(Path(__file__))},
               'source_dependencies': [{'path': str(path.resolve()), 'sha256': file_sha(path)} for path in [
                   Path(__file__).with_name('generate_r192_iop_aot.py'), W/'scripts/iop_reference_codegen.py',
                   W/'scripts/iop_irx_imports.py', W/'scripts/disc.py', args.donor_generator]],
               'inputs': {'ISO': str(args.iso.resolve()), 'ISO_sha256': ISO_SHA,
                          'placement_map_sha256': args.expected_placement_sha.lower(), 'IOP_RAM_sha256': args.expected_ram_sha.lower(),
                          'compiler': str(args.compiler.resolve()), 'compiler_sha256': file_sha(args.compiler)},
               'summary': summary, 'containers': containers, 'records': records,
               'reused_measurement': {'path': str(args.reuse_compiled_result.resolve()), 'sha256': args.expected_reuse_sha.lower()} if reuse else None,
               'scope': 'Every disc/ROMFS ELF alias measured. Only observed placements have snapshot provenance; test placements never become runtime admissions. Actual C++ compilation is capability evidence only. Dynamic import/caller, native service and R3000-vs-R192 semantic obligations remain open.'}
    target = args.output/'RESULT.json'
    target.write_text(json.dumps(receipt, indent=2)+'\n', encoding='utf-8', newline='\n')
    print(json.dumps({'result': str(target.resolve()), 'sha256': file_sha(target), 'summary': summary}), flush=True)


if __name__ == '__main__':
    main()
