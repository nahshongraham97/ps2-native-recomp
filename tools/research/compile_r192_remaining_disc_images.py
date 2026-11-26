"""Offline-only NETR/NTGUI source and native-object study from the whole disc.

Does not register code, execute either program, enable online services, change
the native catalogue/credit denominators, or touch any original input.
"""
from pathlib import Path
import argparse
import csv
import json
import re
import shutil
import struct
import subprocess

import aot_inventory
from extract_r192_elf_vu import require, sha

ROOT = Path(__file__).resolve().parents[2]
ISO_SHA = '48e395b56fe6888d9013d36b4b04d2a1decf2f52259063936a8e9f53e824966d'
INVENTORY_SHA = 'bf0908b9dbeba133bb026e623ea3ef8d2d5fea0e0d11a2a533a692e5e3db2ea9'
GENERATION_SHA = '215e9586c08523e3082822e68736beb745e665fe8aded8e4b805a9feb70efeca'


def entry_map(folder, entry):
    table = (folder / 'table.cpp').read_text()
    group = re.search(r'const ps2::UnitInfo kUnits\[\] = \{(.*?)\};', table, re.S)
    require(group is not None, 'main_entry_table_missing')
    count, witnessed = 0, False
    for base, size, function, name in re.findall(r'\{0x([0-9a-fA-F]+)u,\s*(\d+)u,\s*&(\w+),\s*(\w+)\}', group[1]):
        text = (folder / (function + '.cpp')).read_text()
        array = re.search(r'const std::uint16_t ' + re.escape(name) + r'\[(\d+)\] = \{(.*?)\};', text, re.S)
        require(array is not None, 'compiled_entry_array_missing')
        values = [int(x.strip().removesuffix('u')) for x in array[2].split(',') if x.strip()]
        require(len(values) == int(array[1]) and len(values) * 4 == int(size), 'compiled_entry_array_extent')
        targets = {int(i): int(pc, 16) for i, pc in re.findall(r'case (\d+)u: goto L_([0-9a-fA-F]+);', text)}
        for offset, identifier in enumerate(values):
            if not identifier: continue
            pc = int(base, 16) + offset * 4
            require(targets.get(identifier) == pc, 'compiled_entry_target_changed')
            count += 1
            witnessed |= pc == entry
    return {'active_PC_entries': count, 'ELF_entry_mapped_to_actual_label': witnessed}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('iso', 'inventory', 'generator-stage', 'main-elf', 'compiler', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--compile', action='store_true')
    args = parser.parse_args()
    out = args.output.resolve()
    require(out.is_relative_to(ROOT / 'build') and not out.exists(), 'fresh_owned_output_required')
    require(sha(args.inventory) == INVENTORY_SHA and sha(args.generator_stage / 'RESULT.json') == GENERATION_SHA,
            'source_inventory_or_generator_receipt_changed')
    require(sha(args.iso) == ISO_SHA, 'original_disc_changed')
    inventory = json.loads(args.inventory.read_text())
    stage = json.loads((args.generator_stage / 'RESULT.json').read_text())
    generator = args.generator_stage / 'eerecomp.exe'
    require(sha(generator) == stage['generator_sha256'] and sha(args.compiler) == stage['compiler_sha256'], 'generator_or_compiler_changed')
    require(sha(args.main_elf) == 'a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f', 'retail_main_ELF_changed')
    source_root = ROOT / 'build/r192-compiled-konquest/src'
    ledger = ROOT / 'research/ledger/functions.csv'
    pins = {str(p.resolve()): sha(p) for p in [Path(__file__), args.iso, args.inventory, generator,
            args.generator_stage / 'RESULT.json', args.main_elf, args.compiler, ledger,
            Path(aot_inventory.__file__), *sorted((source_root / 'runtime/include/ps2').glob('*.hpp'))]}
    if args.compile:
        archiver = args.compiler.with_name('llvm-ar.exe')
        pins[str(archiver.resolve())] = sha(archiver)
    require(shutil.disk_usage(ROOT / 'build').free > (12 << 30) + (1 << 30), 'disk_reserve_exhausted')
    out.mkdir(parents=True)
    originals = out / 'originals'
    originals.mkdir()
    selected = {}
    with args.iso.open('rb') as disc:
        for name in ('GAMER.OVL', 'MOVIER.OVL', 'NETR.OVL', 'NTGUI.ELF'):
            row = next(r for r in inventory['code_inputs'] if r['path'] == name)
            disc.seek(row['ISO_offset'])
            data = disc.read(row['bytes'])
            require(len(data) == row['bytes'], 'disc_file_truncated:' + name)
            target = originals / name
            target.write_bytes(data)
            require(sha(target) == row['sha256'], 'disc_file_identity_changed:' + name)
            selected[name] = dict(row, copy=str(target), copy_sha256=sha(target))
    modules = out / 'modules'
    modules.mkdir()
    for name in ('GAMER.OVL', 'MOVIER.OVL', 'NETR.OVL'):
        shutil.copy2(originals / name, modules / name)
    rows = list(csv.DictReader(ledger.open(newline='', encoding='utf8')))
    # An offline source inventory includes the explicitly excluded online code;
    # this does not alter its native feature exclusion or any ledger row.
    callable_rows = [r for r in rows if r['catalogue_kind'] in ('callable', 'excluded')]
    seeds = []
    for row in callable_rows:
        if row['module'] == 'SLUS_214.10': seeds.append(row['address'])
        elif row['module'] in ('GAMER.ovl', 'MOVIER.ovl', 'NETR.ovl'):
            seeds.append(row['module'].removesuffix('.ovl') + ':' + row['address'])
    entries = out / 'entries.txt'
    entries.write_text('\n'.join(sorted(seeds)) + '\n', encoding='ascii', newline='\n')
    commands = []
    def run(name, command):
        process = subprocess.run([str(x) for x in command], capture_output=True)
        log = out / (name + '.log')
        log.write_bytes(process.stdout + process.stderr)
        commands.append({'name': name, 'argv': [str(x) for x in command], 'exit': process.returncode, 'log_sha256': sha(log)})
        return process.returncode
    netr = out / 'NETR-generated'
    netr_exit = run('generate-NETR', [generator, args.main_elf, '--entries', entries, '--modules', modules, '--out', netr])
    netr_measure, netr_measure_error = None, None
    if netr_exit == 0:
        try:
            netr_measure = aot_inventory.measure_entries(netr, callable_rows)
        except AssertionError as exc:
            netr_measure_error = str(exc)  # NTGUI remains an independent input.
    ntgui = out / 'NTGUI-generated'
    ntgui_exit = run('generate-NTGUI', [generator, originals / 'NTGUI.ELF', '--out', ntgui])
    data = (originals / 'NTGUI.ELF').read_bytes()
    require(data[:7] == b'\x7fELF\x01\x01\x01' and struct.unpack_from('<H', data, 18)[0] == 8, 'NTGUI_ELF32_LE_MIPS_required')
    entry = struct.unpack_from('<I', data, 24)[0]
    ntgui_map = entry_map(ntgui, entry) if ntgui_exit == 0 else None
    generated = {}
    for name, folder in [('NETR', netr), ('NTGUI', ntgui)]:
        generated[name] = [{'path': str(p), 'sha256': sha(p), 'bytes': p.stat().st_size} for p in sorted(folder.glob('*')) if p.is_file()]
    native, archives = [], []
    if args.compile:
        for name, sources in [('NETR', sorted(netr.glob('m_NETR_unit_*.cpp'))), ('NTGUI', sorted(ntgui.glob('unit_*.cpp')))]:
            folder = out / (name + '-objects')
            folder.mkdir()
            objects = []
            for source in sources:
                require(shutil.disk_usage(ROOT / 'build').free > (12 << 30) + (128 << 20), 'disk_reserve_exhausted')
                obj = folder / (source.stem + '.obj')
                command = [args.compiler, '-std=c++20', '-O2', '-mavx2', '-ffp-contract=off', '-DNDEBUG',
                           '-I' + str(source_root / 'runtime/include'), '-c', source, '-o', obj]
                code = run('compile-' + name + '-' + source.stem, command)
                native.append({'image': name, 'source': str(source), 'source_sha256': sha(source), 'object': str(obj),
                               'object_sha256': sha(obj) if obj.exists() else None, 'exit': code})
                if code == 0: objects.append(obj)
            if sources and len(objects) == len(sources):
                archive = out / ('lib' + name.lower() + '-offline.a')
                code = run('archive-' + name, [archiver, 'rcs', archive, *objects])
                members = subprocess.run([str(archiver), 't', str(archive)], capture_output=True, text=True) if code == 0 else None
                exact = members is not None and members.returncode == 0 and members.stdout.splitlines() == [p.name for p in objects]
                archives.append({'image': name, 'path': str(archive), 'sha256': sha(archive) if archive.exists() else None,
                                 'objects': len(objects), 'member_list_exact': exact})
    require(pins == {path: sha(Path(path)) for path in pins}, 'original_or_source_input_changed')
    netr_rows = netr_measure['modules'].get('NETR.ovl') if netr_measure else None
    source_pass = netr_exit == 0 and ntgui_exit == 0 and netr_rows is not None and not netr_rows['missing_catalogue_entries'] and ntgui_map['ELF_entry_mapped_to_actual_label']
    native_pass = bool(native) and all(row['exit'] == 0 for row in native) and len(archives) == 2 and all(a['member_list_exact'] for a in archives) if args.compile else None
    passed = source_pass and (native_pass if args.compile else True)
    result = {'status': 'OFFLINE_DISC_IMAGE_GENERATION_PASS' if passed else 'OFFLINE_DISC_IMAGE_HAS_REFUSALS', 'input_pins': pins, 'original_copies': selected,
              'commands': commands, 'NETR_coverage': netr_rows, 'NETR_prior_uncovered': 46,
              'NETR_all_catalogued_callables_mapped': not netr_rows['missing_catalogue_entries'] if netr_rows else False,
              'NETR_measurement_error': netr_measure_error,
              'NTGUI_ELF_entry': f'{entry:08X}', 'NTGUI_generation_exit': ntgui_exit, 'NTGUI_entry_map': ntgui_map,
              'generated_files': generated, 'native_compilation_requested': args.compile, 'native_compilation_all_pass': native_pass, 'native_objects': native, 'native_archives': archives,
              'scope': 'Source discovery/native objects only. NETR online feature/service exclusion remains unchanged; NTGUI is not launched. No registration/link-to-game, whole-image execution or universal code/native-credit claim.'}
    target = out / 'RESULT.json'
    target.write_text(json.dumps(result, indent=2) + '\n', newline='\n')
    print(json.dumps({'status': result['status'], 'result_sha256': sha(target),
                      'NETR_missing': netr_rows['missing_catalogue_entries'] if netr_rows else None,
                      'NTGUI_generation_exit': ntgui_exit, 'native_objects': len(native)}))
    return 0 if passed else 2


if __name__ == '__main__':
    raise SystemExit(main())
