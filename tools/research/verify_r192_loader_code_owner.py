"""Join exact original IRX/PIC/schema to actual loader generations and entries.

Original filename/buffer loading and byte-restore are compared with/without
the bounded connector. This does not boot modules, reconstruct SDK history,
prove arbitrary retirement, or execute a saved-state/game run.
"""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess

import generate_r192_iop_aot as fixed
import build_r192_iop_v8_candidate as builder
import verify_r192_static_metadata_guard as schemas

W, require, sha = fixed.W, fixed.require, fixed.file_sha
FOCUS = ['94915a3d9673e8a370e144ae493552d5e1f3956bb9ae5bd91e2da5f36c83ce7d',
         '85a465ab251bde0c3d33151b3a67ec2780d57cc40ebb7df9d5ecb7bad3396b16']


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('reference-proof', 'candidate-build', 'domain', 'guard-proof', 'output'):
        p.add_argument('--' + key, type=Path, required=True)
        if key != 'output': p.add_argument('--expected-' + key + '-sha', required=True)
    p.add_argument('--state', type=Path, action='append', required=True)
    args = p.parse_args()
    for key in ('reference_proof', 'candidate_build', 'domain', 'guard_proof', 'output'):
        setattr(args, key, getattr(args, key).resolve())
    args.state = [s.resolve() for s in args.state]
    require(args.output.is_relative_to(W / 'build/oracle-telemetry') and not args.output.exists(), 'fresh_owned_output_required')
    require(all(s.is_relative_to(W / 'build/oracle-telemetry') and s.suffix == '.state' for s in args.state), 'owned_state_copies_only')
    docs = {}
    for key in ('reference_proof', 'candidate_build', 'domain', 'guard_proof'):
        path = getattr(args, key)
        require(sha(path) == getattr(args, 'expected_' + key + '_sha'), 'input_receipt_changed:' + key)
        docs[key] = json.loads(path.read_text())
    reference, build, domain, guard = [docs[k] for k in ('reference_proof', 'candidate_build', 'domain', 'guard_proof')]
    require(reference['status'] == 'WHOLE_START_REFERENCE_COMPATIBILITY_PASS' and
            build['status'] == 'DISPOSABLE_V8_IOP_AOT_BUILD_PASS' and guard['status'] == 'EXACT_STATIC_METADATA_GUARD_PASS',
            'completed_original_source_proofs_required')
    source = W / 'tests/test_r192_loader_code_owner.cpp'; header = W / 'runtime/src/iop/r192_loader_code_owner.hpp'
    fixture = W / 'tests/test_r192_iop_v8_start.cpp'; kernel = args.candidate_build.parent / 'src/r192-v8-kernel.cpp'
    pins = reference['source_pins'] + build['source_pins'] + domain['source_pins'] + guard['source_pins']
    for c in build['compile_receipts']:
        cmd = c['argv']; pins += [{'path': cmd[cmd.index('-c') + 1], 'sha256': c['source_sha256']},
                                  {'path': cmd[cmd.index('-o') + 1], 'sha256': c['object_sha256']}]
    pins += [{'path': str(f), 'sha256': sha(f)} for f in [Path(__file__).resolve(), source, header, fixture, kernel,
        args.reference_proof, args.candidate_build, args.domain, args.guard_proof, Path(schemas.__file__),
        W / 'scripts/iop_irx_imports.py'] + args.state]
    require(all(sha(Path(pin['path'])) == pin['sha256'] for pin in pins), 'source_or_object_pin_changed')
    inventory_path = Path(domain['frozen_inventory']['path'])
    require(sha(inventory_path) == domain['frozen_inventory']['sha256'], 'original_inventory_changed')
    inventory = json.loads(inventory_path.read_text()); rows = {r['original_sha256']: r for r in inventory['records']}
    irx = fixed.load_tool('owner_source_IRX', W / 'scripts/iop_irx_imports.py')
    descriptions = [schemas.schemas(row, irx) for _, row in sorted(rows.items())]
    require(len(descriptions) == 70 and not any(d['unresolved_record_candidates'] for d in descriptions), 'all_original_schemas_required')
    pins += [{'path': d['original_path'], 'sha256': d['original_sha256']} for d in descriptions]
    args.output.mkdir(parents=True, exist_ok=False)
    archive = args.output / 'source-archive'; archive.mkdir()
    for f in [Path(__file__).resolve(), source, header]: (archive / f.name).write_bytes(f.read_bytes())
    (args.output / 'INPUT_PINS.json').write_text(json.dumps(pins, indent=2) + '\n', encoding='utf-8', newline='\n')
    (args.output / 'ORIGINAL_SCHEMAS.json').write_text(json.dumps(descriptions, indent=2) + '\n', encoding='utf-8', newline='\n')
    lines = ['static const std::vector<lo::Source> original_sources = {']
    for desc in descriptions:
        row = rows[desc['original_sha256']]; data = Path(desc['original_path']).read_bytes()
        phoff = int.from_bytes(data[28:32], 'little'); phsize = int.from_bytes(data[42:44], 'little'); phnum = int.from_bytes(data[44:46], 'little')
        headers = [phoff + i * phsize for i in range(phnum)]
        mods = [int.from_bytes(data[x + 4:x + 8], 'little') for x in headers if int.from_bytes(data[x:x + 4], 'little') == 0x70000080]
        require(len(mods) == 1, 'one_original_IOPMOD_required')
        at = mods[0]; entry = int.from_bytes(data[at + 4:at + 8], 'little'); gp = int.from_bytes(data[at + 8:at + 12], 'little')
        require(entry == row['entry_offset'], 'original_entry_offset_changed')
        require(at + 26 < len(data), 'original_buffer_name_bounds_unproved')
        # Exact original frozen loader: bounded56 bytes, preserving case and
        # the literal empty MSL name. A missing historical JSON label is not
        # a source name and must never become a fallback/null registry entry.
        buffer_name = data[at + 26:min(len(data), at + 82)].split(b'\0', 1)[0].decode('ascii')
        require(buffer_name == (row['header_name'] or ''), 'original_buffer_name_inventory_disagrees')
        digest = desc['original_sha256']; filename = row['loader_name']
        lines.append('{' + ','.join(json.dumps(s) for s in (digest, filename, buffer_name)) +
                     ',aot::find_pic_template(' + json.dumps(filename) + ',' + json.dumps(digest) + '),' + str(entry) + ',' + str(gp) + ',{')
        for record in desc['records']:
            lines.append('{static_cast<ps2::iop::metadata_guard::Kind>(' + str(record['kind']) + '),' + str(record['image_offset']) +
                         ',{' + ','.join(f'0x{x:08X}u' for x in record['canonical_words']) + '},{' +
                         ','.join(map(str, record['relocation_offsets'])) + '}},')
        lines.append('}},')
    catalogue = args.output / 'original-sources.inc'; catalogue.write_text('\n'.join(lines + ['};']) + '\n', encoding='utf-8', newline='\n')
    prefix = args.output / 'original-prefix.inc'
    prefix.write_text(fixture.read_text().split('int main(', 1)[0], encoding='utf-8', newline='\n')
    registry = Path(domain['registry']['path']).with_suffix('.obj'); require(sha(registry) == domain['registry']['object_sha256'], 'registry_object_changed')
    command = []
    for item in reference['build_command']:
        if item.startswith('-DR192_REFERENCE_KERNEL_SOURCE='): item = '-DR192_REFERENCE_KERNEL_SOURCE="' + kernel.as_posix() + '"'
        elif item == str(fixture): item = str(source)
        elif Path(item).name == 'iop_pic_registry.obj': item = str(registry)
        command.append(item)
    command[1:1] = ['-DR192_OWNER_FIXTURE_PREFIX="' + prefix.as_posix() + '"', '-DR192_OWNER_SOURCES="' + catalogue.as_posix() + '"']
    compiler = Path(command[0]); require(sha(compiler) == reference['compiler_sha256'], 'compiler_changed')
    runtime = next(Path(x) for x in command if x.endswith('libps2runtime.a')).parent.parent
    env = {k: v for k, v in os.environ.items() if not k.startswith(('PS2_', 'MKA_', 'SDL_'))}
    env['PATH'] = str(runtime / 'out') + os.pathsep + str(compiler.parent) + os.pathsep + env.get('PATH', '')
    exe = args.output / 'actual-loader-owner.exe'; command[-1] = str(exe)
    require(command[-2] == '-o' and '-lbcrypt' in command, 'frozen_build_layout_changed')
    require(shutil.disk_usage(W).free > (12 << 30) + (256 << 20), 'reserved_disk_space_required')
    rsp = args.output / 'build.rsp'; rsp.write_text('\n'.join(builder.quote_response(x) for x in command[1:]) + '\n', encoding='utf-8', newline='\n')
    built = subprocess.run([str(compiler), '@' + str(rsp)], capture_output=True, text=True, timeout=180)
    (args.output / 'build.log').write_text(built.stdout + built.stderr, encoding='utf-8', newline='\n')
    require(built.returncode == 0, 'owner_connector_compile_failed')
    def run(label, mode, digest, route, scenario, state, chosen_exe=None):
        prefix = args.output / label
        cmd = [str(chosen_exe or exe), mode, rows[digest]['original_copy'], route, scenario, str(prefix), str(state)]
        result = subprocess.run(cmd, capture_output=True, text=True, env=env, timeout=120)
        (args.output / (label + '.log')).write_text(result.stdout + result.stderr, encoding='utf-8', newline='\n')
        measurement = json.loads(prefix.with_suffix('.json').read_text()) if prefix.with_suffix('.json').exists() else None
        return {'argv': cmd, 'exit_code': result.returncode, 'measurement': measurement}
    cases = []
    plans = [(d, route, 'fresh', args.state[0]) for d in FOCUS for route in ('filename', 'buffer')]
    plans += [(FOCUS[1], 'buffer', 'restore', s) for s in args.state]
    plans += [(FOCUS[1], 'buffer', 'partial_restore', args.state[0])]
    for i, (digest, route, scenario, state) in enumerate(plans):
        runs = {mode: run(f'case-{i:02}-{mode}', mode, digest, route, scenario, state) for mode in ('reference', 'connector')}
        comparisons = []
        for suffix in ('cpu', 'ram', 'iop-state'):
            a, b = [args.output / f'case-{i:02}-{mode}.{suffix}' for mode in ('reference', 'connector')]
            comparisons.append({'artifact': suffix, 'reference_sha256': sha(a) if a.exists() else None,
                'connector_sha256': sha(b) if b.exists() else None, 'equal': a.exists() and b.exists() and a.read_bytes() == b.read_bytes()})
        passed = all(r['exit_code'] == 0 and r['measurement'] and r['measurement']['status'] == 'PASS' for r in runs.values()) and all(c['equal'] for c in comparisons)
        cases.append({'source_digest': digest, 'route': route, 'scenario': scenario, 'state': str(state), 'runs': runs,
                      'comparisons': comparisons, 'status': 'PASS' if passed else 'REFUSED'})
    controls = []
    for scenario in ('wrong_image', 'wrong_facts', 'post_mutation_failure', 'forged_entry', 'wrong_body', 'changed_code', 'retire',
                     'overlap_admission', 'overlap_revalidation', 'reused_owner'):
        run_result = run('control-' + scenario, 'connector', FOCUS[1], 'buffer', scenario, args.state[0])
        m = run_result['measurement'] or {}
        refused = run_result['exit_code'] == 0 and m.get('status') == 'PASS' and m.get('negative_checks') == (2 if scenario == 'reused_owner' else 1)
        controls.append({'name': scenario, 'status': 'REJECTED' if refused else 'NOT_REJECTED', 'run': run_result})
    text = header.read_text(); mutations = []
    for name, before, after, scenario, expected in [
        ('retain_old_generation', 'ready(); invalidate();', 'ready();', 'restore', 'planted_operation_not_refused'),
        ('ignore_failed_restore', 'if (!actual_byte_load()) { poisoned_ = true; return false; }',
         'if (!actual_byte_load()) return false;', 'partial_restore', 'failed_progressive_restore_did_not_poison'),
        ('bypass_raw_hash', 'const auto digest = image_sha256(bytes);',
         'const auto digest = std::string("' + FOCUS[1] + '");', 'wrong_image', 'planted_operation_not_refused'),
        ('ignore_other_module_overlap', 'void verify_exclusive(const Facts &facts) const {',
         'void verify_exclusive(const Facts &facts) const { return;', 'overlap_revalidation', 'planted_operation_not_refused'),
    ]:
        require(text.count(before) == 1, 'semantic_mutation_anchor_changed:' + name)
        overlay = args.output / ('mutant-' + name); (overlay / 'iop').mkdir(parents=True)
        mutant_header = overlay / 'iop/r192_loader_code_owner.hpp'
        mutant_header.write_text(text.replace(before, after), encoding='utf-8', newline='\n')
        bad = overlay / 'mutant.exe'; bad_command = command.copy(); bad_command[1:1] = ['-I' + str(overlay)]; bad_command[-1] = str(bad)
        rsp = overlay / 'build.rsp'; rsp.write_text('\n'.join(builder.quote_response(x) for x in bad_command[1:]) + '\n', encoding='utf-8', newline='\n')
        require(shutil.disk_usage(W).free > (12 << 30) + (128 << 20), 'reserved_disk_space_required')
        result = subprocess.run([str(compiler), '@' + str(rsp)], capture_output=True, text=True, timeout=180)
        (overlay / 'build.log').write_text(result.stdout + result.stderr, encoding='utf-8', newline='\n')
        require(result.returncode == 0, 'semantic_mutant_compile_failed:' + name)
        outcome = run('mutant-' + name, 'connector', FOCUS[1], 'buffer', scenario, args.state[0], bad)
        measured = outcome['measurement'] or {}
        rejected = outcome['exit_code'] == 1 and measured.get('status') == 'REFUSED' and measured.get('failure') == expected
        mutations.append({'name': name, 'status': 'REJECTED' if rejected else 'NOT_REJECTED', 'run': outcome,
            'header_sha256': sha(mutant_header), 'executable_sha256': sha(bad), 'build_argv': bad_command})
    changed = [pin['path'] for pin in pins if sha(Path(pin['path'])) != pin['sha256']]
    passed = not changed and all(c['status'] == 'PASS' for c in cases) and all(c['status'] == 'REJECTED' for c in controls + mutations)
    report = {'status': 'ACTUAL_LOADER_CODE_OWNER_GENERATION_PASS' if passed else 'OWNER_CONNECTOR_REFUSED',
        'classification': 'bounded actual-loader/restore code-owner capability only', 'native_credit': 0,
        'source_pins': pins, 'build_command': command, 'executable_sha256': sha(exe),
        'catalogue': {'all_original_source_images': len(descriptions), 'schema_count': sum(len(d['records']) for d in descriptions),
                      'generated_include_sha256': sha(catalogue)},
        'cases': cases, 'controls': controls, 'compiled_semantic_mutations': mutations, 'input_changes': changed,
        'limits': ['Only LIBSD/SDRDRV fresh filename/buffer identity is executed in this component; catalogue/schema coverage is not all-module behavior.',
            'No module entry, guest instruction, game, mounted media/card or normal N mutation.',
            'Unstarted fixture init threads are explicitly cancelled through existing native APIs before serializer comparison; no on_exit success is fabricated.',
            'Restored metadata admits zero code owners; overlaps remain candidate lists without raw SHA or SDK-root/history inference.',
            'Partial restore deliberately removes the last end-tag byte; actual progressive parser mutations are retained and session poisoned.',
            'All context mutations must route through the connector; no claim of guarding external unobserved reset/load/free paths.',
            'Export identities prove original slot, compiled PIC body and current code/metadata owner generation; they do not prove typed callable ABI or run the export.',
            'Generic retirement, SDK registry transitions/IRQ integration and runtime adoption remain refused/unproved.']}
    target = args.output / 'RESULT.json'; target.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'result': str(target), 'sha256': sha(target), 'status': report['status']}))
    return 0 if passed else 1


if __name__ == '__main__': raise SystemExit(main())
