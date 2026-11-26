"""Generate original static-record schemas and prove exact guarded transitions.

All original bytes/schema payloads stay in a fresh ignored output. A schema is
not a broad text mask: each callable pointer needs its actual R_MIPS_32 site.
"""
from __future__ import annotations
import argparse
from collections import Counter
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

import generate_r192_iop_aot as fixed
import build_r192_iop_v8_candidate as builder

W, require, sha = fixed.W, fixed.require, fixed.file_sha
FOCUS = ['93d205474b13ab152c74a578948351daee2d4ad108745797721816e77ae15f3c',
         '94915a3d9673e8a370e144ae493552d5e1f3956bb9ae5bd91e2da5f36c83ce7d',
         '85a465ab251bde0c3d33151b3a67ec2780d57cc40ebb7df9d5ecb7bad3396b16']


def schemas(row, irx):
    path = Path(row['original_copy']); data = path.read_bytes()
    require(fixed.sha(data) == row['original_sha256'], 'original_IRX_changed')
    image, load, _ = irx._parse_elf_load_image(data)
    relocs = irx.scan_relocations(data, len(image)); at_reloc = {}
    for r in relocs: at_reloc.setdefault(r['offset'], []).append(r)
    u32 = lambda at: int.from_bytes(image[at:at + 4], 'little')
    records, unresolved = [], []
    for at in range(0, load['file_size'] - 20 + 1, 4):
        magic = u32(at)
        if magic not in (0x41C00000, 0x41E00000): continue
        try: name = bytes(image[at + 12:at + 20]).split(b'\0', 1)[0].decode('ascii')
        except UnicodeDecodeError: continue
        if not re.fullmatch(r'[A-Za-z0-9_]{1,8}', name): continue
        try:
            cursor = at + 20; pointers = []
            if magic == 0x41C00000:
                while cursor + 4 <= load['file_size']:
                    value = u32(cursor); reloc = at_reloc.get(cursor, [])
                    if value == 0 and not reloc: break
                    require(len(reloc) == 1 and reloc[0]['type'] == 2 and not value & 3 and value < row['text_size'], 'export_pointer_source_unproved')
                    pointers.append({'image_offset': cursor, 'file_offset': load['file_offset'] + cursor, 'target_image_offset': value, 'relocation': reloc[0]})
                    cursor += 4
                require(pointers and cursor + 4 <= load['file_size'] and u32(cursor) == 0 and cursor not in at_reloc, 'export_terminator_unproved')
                end = cursor + 4; kind = 0
            else:
                while cursor + 8 <= load['file_size'] and u32(cursor) == 0x03E00008 and u32(cursor + 4) & 0xFFFF0000 == 0x24000000:
                    cursor += 8
                require(cursor > at + 20 and cursor + 8 <= load['file_size'] and u32(cursor) == 0 and u32(cursor + 4) == 0, 'client_shape_or_terminator_unproved')
                end = cursor + 8; kind = 1
            in_record = [r for r in relocs if at <= r['offset'] < end]
            require(all(r['type'] == 2 and r['offset'] in {p['image_offset'] for p in pointers} for r in in_record), 'record_relocation_role_unproved')
            records.append({'kind': kind, 'kind_name': 'exports' if kind == 0 else 'client', 'library': name,
                'image_offset': at, 'file_offset': load['file_offset'] + at, 'extent': end - at,
                'canonical_words': [u32(x) for x in range(at, end, 4)],
                'relocation_offsets': [r['offset'] - at for r in in_record], 'original_callable_pointer_sites': pointers,
                'canonical_sha256': fixed.sha(image[at:end])})
        except ValueError as e: unresolved.append({'image_offset': at, 'library': name, 'reason': str(e)})
    return {'original_sha256': row['original_sha256'], 'original_path': str(path), 'load': load,
            'text_size': row['text_size'], 'records': records, 'unresolved_record_candidates': unresolved}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('reference-proof', 'candidate-build', 'domain', 'lifecycle-proof', 'output'):
        parser.add_argument('--' + key, type=Path, required=True)
        if key != 'output': parser.add_argument('--expected-' + key + '-sha', required=True)
    args = parser.parse_args()
    for key in ('reference_proof', 'candidate_build', 'domain', 'lifecycle_proof', 'output'):
        setattr(args, key, getattr(args, key).resolve())
    require(args.output.is_relative_to(W / 'build') and not args.output.exists(), 'fresh_owned_output_required')
    for key in ('reference_proof', 'candidate_build', 'domain', 'lifecycle_proof'):
        require(sha(getattr(args, key)) == getattr(args, 'expected_' + key + '_sha').lower(), 'receipt_pin_mismatch:' + key)
    reference = json.loads(args.reference_proof.read_text()); build = json.loads(args.candidate_build.read_text()); domain = json.loads(args.domain.read_text())
    lifecycle = json.loads(args.lifecycle_proof.read_text())
    require(reference['status'] == 'WHOLE_START_REFERENCE_COMPATIBILITY_PASS' and build['status'] == 'DISPOSABLE_V8_IOP_AOT_BUILD_PASS' and
            lifecycle['status'] == 'TYPED_REGISTRY_ORIGINAL_PROJECTION_PASS', 'completed_source_proofs_required')
    for p in lifecycle['source_pins']: require(sha(Path(p['path'])) == p['sha256'], 'lifecycle_proof_source_changed')
    source = W / 'tests/test_r192_static_metadata_guard.cpp'; header = W / 'runtime/src/iop/r192_static_metadata_guard.hpp'
    fixture = W / 'tests/test_r192_iop_v8_start.cpp'; kernel = args.candidate_build.parent / 'src/r192-v8-kernel.cpp'
    pins = reference['source_pins'] + build['source_pins'] + domain['source_pins'] + lifecycle['source_pins']
    for c in build['compile_receipts']:
        cmd = c['argv']; pins += [{'path': cmd[cmd.index('-c') + 1], 'sha256': c['source_sha256']}, {'path': cmd[cmd.index('-o') + 1], 'sha256': c['object_sha256']}]
    pins += [{'path': str(p), 'sha256': sha(p)} for p in [Path(__file__).resolve(), source, header, fixture, kernel, args.reference_proof,
             args.candidate_build, args.domain, args.lifecycle_proof, W / 'scripts/iop_irx_imports.py']]
    require(all(sha(Path(p['path'])) == p['sha256'] for p in pins), 'source_or_object_changed')
    irx = fixed.load_tool('authenticated_static_records', W / 'scripts/iop_irx_imports.py')
    inventory = json.loads(Path(domain['frozen_inventory']['path']).read_text())
    require(sha(Path(domain['frozen_inventory']['path'])) == domain['frozen_inventory']['sha256'], 'original_inventory_changed')
    rows = {r['original_sha256']: r for r in inventory['records']}
    descriptions = [schemas(row, irx) for _, row in sorted(rows.items())]
    require(len(descriptions) == 70, 'all_original_payloads_required')
    require(all(not d['unresolved_record_candidates'] for d in descriptions if d['original_sha256'] in FOCUS), 'focused_schema_unproved')
    pins += [{'path': d['original_path'], 'sha256': d['original_sha256']} for d in descriptions]
    args.output.mkdir(parents=True, exist_ok=False)
    archive = args.output / 'source-archive'; archive.mkdir()
    source_archives = []
    for original in (Path(__file__).resolve(), source, header):
        copy = archive / original.name; copy.write_bytes(original.read_bytes())
        source_archives.append({'source': str(original), 'copy': str(copy), 'sha256': sha(copy)})
    (args.output / 'INPUT_PINS.json').write_text(json.dumps({'source_pins': pins, 'source_archives': source_archives}, indent=2) + '\n', encoding='utf-8', newline='\n')
    schema_json = args.output / 'STATIC_SCHEMAS.json'; schema_json.write_text(json.dumps(descriptions, indent=2) + '\n', encoding='utf-8', newline='\n')
    schema_cpp = args.output / 'static-schemas.inc'
    lines = ['static const std::map<std::string, std::vector<CanonicalSpec>> original_schemas = {']
    for d in descriptions:
        lines.append('{' + json.dumps(d['original_sha256']) + ', {')
        for r in d['records']:
            lines.append('{' + str(r['kind']) + ',' + str(r['image_offset']) + ',{' + ','.join(f'0x{x:08X}u' for x in r['canonical_words']) + '},{' + ','.join(map(str, r['relocation_offsets'])) + '}},')
        lines.append('}},')
    schema_cpp.write_text('\n'.join(lines + ['};']) + '\n', encoding='utf-8', newline='\n')
    text = fixture.read_text(); prefix = text[:text.index('int main(')]
    anchor = 'extern "C" void __real__ZN3ps23iop7executeERNS0_3CpuERx(Cpu &, std::int64_t &);'
    call = '    __real__ZN3ps23iop7executeERNS0_3CpuERx(cpu, budget);'
    service = 'void service(std::uint32_t id, Cpu &cpu) {'
    ret = '    kernel::g_hle[id].function(cpu); // Actual, pre-existing registered native body.'
    aret = '    } else if (observe) checkpoint(cpu);'
    write = 'void record_write(std::uint32_t a, std::uint32_t width, std::uint32_t value) {'
    require(all(prefix.count(a) == 1 for a in (anchor, call, service, ret, aret, write)), 'independent_prefix_anchors_changed')
    prefix = prefix.replace(anchor, 'static bool r192_route_reference;\n' + anchor).replace(call,
        '    if (r192_route_reference) ps2::iop::reference_execute(cpu, budget);\n    else __real__ZN3ps23iop7executeERNS0_3CpuERx(cpu, budget);')
    prefix = prefix.replace(service, 'void record_metadata_return(std::uint32_t, Cpu &);\n' + service)
    prefix = prefix.replace(ret, ret + '\n    record_metadata_return(id, cpu);').replace(aret, aret + '\n    if (!before) record_metadata_return(id, cpu);')
    prefix = prefix.replace(write, 'void guarded_metadata_write(std::uint32_t, std::uint32_t, std::uint32_t);\n' + write + '\n    guarded_metadata_write(a, width, value);')
    prefix_path = args.output / 'independent-prefix.inc'; prefix_path.write_text(prefix, encoding='utf-8', newline='\n')
    registry = Path(domain['registry']['path']).with_suffix('.obj'); require(sha(registry) == domain['registry']['object_sha256'], 'registry_object_changed')
    command = []
    for item in reference['build_command']:
        if item.startswith('-DR192_REFERENCE_KERNEL_SOURCE='): item = '-DR192_REFERENCE_KERNEL_SOURCE="' + kernel.as_posix() + '"'
        elif item == str(fixture): item = str(source)
        elif Path(item).name == 'iop_pic_registry.obj': item = str(registry)
        command.append(item)
    command[1:1] = ['-DR192_METADATA_FIXTURE_PREFIX="' + prefix_path.as_posix() + '"', '-DR192_METADATA_SCHEMAS="' + schema_cpp.as_posix() + '"']
    compiler = Path(command[0]); require(sha(compiler) == reference['compiler_sha256'], 'compiler_changed')
    runtime = next(Path(x) for x in command if x.endswith('libps2runtime.a')).parent.parent
    env = {k: v for k, v in os.environ.items() if not k.startswith(('PS2_', 'MKA_', 'SDL_'))}
    env['PATH'] = str(runtime / 'out') + os.pathsep + str(compiler.parent) + os.pathsep + env.get('PATH', '')
    def compile_case(label, overlay=None):
        require(shutil.disk_usage(W).free > (12 << 30) + (128 << 20), 'required_12GiB_reserve')
        exe = args.output / (label + '.exe'); cmd = command[:-1] + [str(exe)]
        if overlay: cmd.insert(1, '-I' + str(overlay))
        rsp = args.output / (label + '.rsp'); rsp.write_text('\n'.join(builder.quote_response(x) for x in cmd[1:]) + '\n', encoding='utf-8', newline='\n')
        ran = subprocess.run([cmd[0], '@' + str(rsp)], capture_output=True, text=True)
        (args.output / (label + '-build.log')).write_text(ran.stdout + ran.stderr, encoding='utf-8', newline='\n')
        if ran.returncode:
            (args.output / (label + '-BUILD_FAILURE.json')).write_text(json.dumps({'status': 'BUILD_REFUSED', 'argv': cmd,
                'exit_code': ran.returncode, 'source_archives': source_archives}, indent=2) + '\n', encoding='utf-8', newline='\n')
        require(ran.returncode == 0, 'guard_test_compile_failed:' + label)
        return exe, {'argv': cmd, 'executable_sha256': sha(exe)}
    originals = [rows[digest]['original_copy'] for digest in FOCUS]
    def run(exe, label, mode='compiled', plant='none'):
        out = args.output / (label + '.json'); argv = [str(exe), *originals, mode, str(out), plant]
        ran = subprocess.run(argv, capture_output=True, text=True, env=env, timeout=120)
        (args.output / (label + '.log')).write_text(ran.stdout + ran.stderr, encoding='utf-8', newline='\n')
        return {'argv': argv, 'exit_code': ran.returncode, 'measurement': json.loads(out.read_text()) if out.exists() else None}
    exe, built = compile_case('guard')
    cases = [run(exe, mode, mode) for mode in ('reference', 'compiled')]
    plants = [run(exe, 'plant-' + p, plant=p) for p in ['write-immutable', 'foreign-pointer', 'wrong-slot', 'version', 'name', 'function-pointer', 'code-word', 'live-owner']]
    (args.output / 'PROGRESS.json').write_text(json.dumps({'cases': cases, 'plants': plants}, indent=2) + '\n', encoding='utf-8', newline='\n')
    controls = []
    mutations = [
        ('PC_as_data_allowed', 'if (r.kind != Kind::client || at < 20 || at >= 20 + 8 * r.count) return false;', 'if (false) return false;', 'none', 'metadata_admitted_as_code'),
        ('J_becomes_JAL', 'cpu.pc = cpu.npc; cpu.npc = target;', 'cpu.r[31] = cpu.pc + 8; cpu.pc = cpu.npc; cpu.npc = target;', 'none', 'SDK_stub_CPU_delay_mismatch'),
        ('delay_target_lost', '} else { cpu.pc = cpu.npc; cpu.npc += 4; }', '} else { cpu.pc += 4; cpu.npc += 4; }', 'none', 'SDK_stub_CPU_delay_mismatch'),
        ('stale_owner_allowed', 'h.index >= owners_.size() || !owners_[h.index].live || owners_[h.index].generation != h.generation', 'h.index >= owners_.size()', 'none', 'retired_owner_or_record_admitted'),
        ('code_identity_disabled', 'if (read_(o.module.base + static_cast<std::uint32_t>(i * 4), 4) != o.words[i])', 'if (false)', 'code-word', None),
        ('immutable_field_allowed', 'else poison("immutable_export_field");', 'else {}', 'write-immutable', 'forged_metadata_plan_admitted'),
        ('wrong_slot_allowed', 'if (value != expected) poison("original_export_slot_binding");', 'if (false) poison("original_export_slot_binding");', 'wrong-slot', 'forged_metadata_plan_admitted')]
    native = header.read_text()
    for label, before, after, plant, reason in mutations:
        require(native.count(before) == 1, 'guard_mutation_anchor_changed:' + label)
        folder = args.output / ('mutant-' + label); (folder / 'iop').mkdir(parents=True)
        mutated = folder / 'iop' / header.name; mutated.write_text(native.replace(before, after), encoding='utf-8', newline='\n')
        bad, compiled = compile_case(label, folder); outcome = run(bad, label, plant=plant); measured = outcome['measurement'] or {}
        detected = (outcome['exit_code'] == 1 and measured.get('failure') == reason) if reason else (measured.get('status') != 'PLANTED_REFUSAL')
        controls.append({'name': label, 'status': 'DETECTED' if detected else 'NOT_DETECTED', 'build': compiled, 'run': outcome, 'mutant_header_sha256': sha(mutated)})
    changes = [p['path'] for p in pins if sha(Path(p['path'])) != p['sha256']]
    passed = not changes and all(c['exit_code'] == 0 and c['measurement'] and c['measurement']['status'] == 'PASS' for c in cases) and all(p['measurement'] and p['measurement']['status'] == 'PLANTED_REFUSAL' for p in plants) and all(c['status'] == 'DETECTED' for c in controls)
    report = {'status': 'EXACT_STATIC_METADATA_GUARD_PASS' if passed else 'STATIC_METADATA_GUARD_REFUSED', 'classification': 'isolated original-loader metadata/code identity capability only', 'native_credit': 0,
        'source_pins': pins, 'source_archives': source_archives, 'build': built, 'schemas': {'path': str(schema_json), 'sha256': sha(schema_json), 'cpp_sha256': sha(schema_cpp),
            'payloads': len(descriptions), 'records': sum(len(d['records']) for d in descriptions), 'kind_histogram': dict(Counter(r['kind_name'] for d in descriptions for r in d['records'])),
            'unresolved_candidates': [dict(c, original_sha256=d['original_sha256']) for d in descriptions for c in d['unresolved_record_candidates']]},
        'cases': cases, 'plants': plants, 'compiled_guard_mutations': controls, 'input_changes': changes,
        'limits': ['No actual SDK module entry/start is claimed: pending init threads are explicitly cancelled with existing native Terminate/Delete APIs.',
            'Original6/7/8/9 execute against actual static LIBSD/SDRDRV records. Canonical client preparation is test-only; runtime must defer legacy linking by authenticated producer.',
            'Owner-generation rebind uses the same loaded storage; it is not an actual allocator/module unload-reload claim.',
            'Retirement probe closes only this finite metadata-method fixture, with no remaining threads and only return-stub services; generic RPC/IRQ/callback lifetime and restored state remain unbound.',
            'Code words and immutable record bytes compare exactly; only checked native-model field writes update expected words. PC-as-data still refuses.',
            'SDK stub J/ADDIU execution compares fullCPU on physical/KSEG aliases, zero/split budgets. It is not a generic instruction interpreter or native callable ABI.',
            'No frozen dispatcher/kernel modification, runtime adoption, N/card/launcher writes or native credit.']}
    target = args.output / 'RESULT.json'; target.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'result': str(target), 'sha256': sha(target), 'status': report['status'], 'schema_summary': report['schemas'],
        'cases': [c['measurement'] for c in cases], 'controls': [(c['name'], c['status']) for c in controls]}))
    return 0 if passed else 1


if __name__ == '__main__': raise SystemExit(main())
