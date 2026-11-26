"""Measure every original70 filename/buffer source-owner identity, not boot.

Keeps full byte-comparison artifacts, transparently compressing completed owned
files with SHA before/after so the12GiB reserve does not require deleting data.
"""
from __future__ import annotations
import argparse
from collections import Counter
import ctypes
from ctypes import wintypes
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import generate_r192_iop_aot as fixed
import build_r192_iop_v8_candidate as builder
import verify_r192_static_metadata_guard as schema_producer

W, require, sha = fixed.W, fixed.require, fixed.file_sha
RESERVE = 12 << 30


def slot_join(slot, proof):
    return bool(slot['status'] == 'VALIDATED_IDENTITY' and proof and proof['status'] == 'PROVED_ENTRY_MAP' and
                all(slot[key] == proof[key] for key in ('table_offset', 'ordinal', 'target_offset')))


def code_map(image, desc, body, table, ordinal, target, expected_word=None):
    load = desc['load']; start = load['file_offset'] + target
    evidence = {'table_offset': table, 'ordinal': ordinal, 'target_offset': target,
                'original_file_offset': start, 'scope': 'original slot/word and generated entry-map identity only'}
    try:
        require(not target & 3 and target + 4 <= desc['text_size'] and start + 4 <= len(image), 'target_not_original_file_backed_text')
        word = int.from_bytes(image[start:start + 4], 'little'); evidence['original_word'] = f'{word:08X}'
        if expected_word is not None: require(word == expected_word, 'original_target_word_mismatch')
        data_records = [r for r in desc['records'] if r['image_offset'] <= target < r['image_offset'] + r['extent']]
        require(not data_records, 'target_is_authenticated_static_metadata')
        label_pattern = rf'^([LD]{target // 4:x}):\s*$'
        matches = list(re.finditer(label_pattern, body, re.M)); require(len(matches) == 1, 'generated_entry_label_unproved')
        found = matches[0]; label = found.group(1)
        functions = list(re.finditer(r'^uint32_t chunk\d+\(', body[:found.start()], re.M)); require(functions, 'generated_chunk_unproved')
        chunk = body[functions[-1].start():found.start()]
        bases = re.findall(r'switch \(\(cpu\.pc & 0x1FFFFFFFu\) - moduleBase - (0x[0-9a-fA-F]+)u\)', chunk)
        cases = re.findall(r'case (0x[0-9a-fA-F]+)u: goto ' + re.escape(label) + r';', chunk)
        require(len(bases) == len(cases) == 1 and int(bases[0], 16) + int(cases[0], 16) == target, 'generated_case_target_unproved')
        end = re.search(r'^[LD][0-9a-f]+:', body[found.end():], re.M)
        stop = found.end() + end.start() if end else len(body)
        block = body[found.start():stop]
        require(re.search(r'moduleBase \+ 0x' + f'{target:x}' + r'u', block), 'generated_budget_PC_unproved')
        evidence.update({'status': 'PROVED_ENTRY_MAP', 'label': label, 'source_byte_range': [found.start(), stop],
                         'source_block_sha256': fixed.sha(block.encode())})
    except ValueError as error: evidence.update({'status': 'UNPROVED', 'reason': str(error)})
    return evidence


class Compression:
    def __init__(self, output):
        require(os.name == 'nt', 'Windows_NTFS_compression_required')
        self.output = output; self.api = ctypes.WinDLL('kernel32', use_last_error=True)
        self.api.GetCompressedFileSizeW.argtypes = [wintypes.LPCWSTR, ctypes.POINTER(wintypes.DWORD)]
        self.api.GetCompressedFileSizeW.restype = wintypes.DWORD
        self.api.GetFileAttributesW.argtypes = [wintypes.LPCWSTR]; self.api.GetFileAttributesW.restype = wintypes.DWORD
        self.api.GetVolumeInformationW.argtypes = [wintypes.LPCWSTR, wintypes.LPWSTR, wintypes.DWORD,
            ctypes.POINTER(wintypes.DWORD), ctypes.POINTER(wintypes.DWORD), ctypes.POINTER(wintypes.DWORD), wintypes.LPWSTR, wintypes.DWORD]
        self.api.GetVolumeInformationW.restype = wintypes.BOOL
        serial, length, flags = wintypes.DWORD(), wintypes.DWORD(), wintypes.DWORD(); fs = ctypes.create_unicode_buffer(32)
        require(self.api.GetVolumeInformationW(str(output.anchor), None, 0, ctypes.byref(serial), ctypes.byref(length),
                    ctypes.byref(flags), fs, len(fs)), 'volume_information_failed')
        require(fs.value == 'NTFS' and flags.value & 0x10, 'NTFS_file_compression_not_supported')
        self.volume = {'filesystem': fs.value, 'flags': flags.value, 'compression_supported': True}
        self.compact = Path(os.environ['SystemRoot']) / 'System32/compact.exe'
    def allocated(self, path):
        high = wintypes.DWORD(); ctypes.set_last_error(0)
        low = self.api.GetCompressedFileSizeW(str(path), ctypes.byref(high))
        require(low != 0xffffffff or ctypes.get_last_error() == 0, 'compressed_size_query_failed')
        return (high.value << 32) | low
    def compress(self, path):
        require(path.resolve().is_relative_to(self.output), 'compression_target_not_owned')
        before = sha(path); physical_before = self.allocated(path)
        cmd = [str(self.compact), '/C', '/F', '/Q', str(path)]
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
        after = sha(path); attributes = self.api.GetFileAttributesW(str(path))
        record = {'path': str(path), 'logical_bytes': path.stat().st_size, 'physical_before': physical_before,
            'physical_after': self.allocated(path), 'sha256_before': before, 'sha256_after': after,
            'argv': cmd, 'exit_code': result.returncode, 'log': result.stdout + result.stderr}
        require(result.returncode == 0 and before == after and attributes != 0xffffffff and attributes & 0x800,
                'transparent_compression_not_verified')
        return record


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('owner-proof', 'domain', 'output'):
        p.add_argument('--' + name, type=Path, required=True)
        if name != 'output': p.add_argument('--expected-' + name + '-sha', required=True)
    args = p.parse_args(); args.owner_proof = args.owner_proof.resolve(); args.domain = args.domain.resolve(); args.output = args.output.resolve()
    require(args.output.is_relative_to(W / 'build/oracle-telemetry') and not args.output.exists(), 'fresh_owned_output_required')
    require(sha(args.owner_proof) == args.expected_owner_proof_sha and sha(args.domain) == args.expected_domain_sha, 'input_receipt_changed')
    old = json.loads(args.owner_proof.read_text()); domain = json.loads(args.domain.read_text())
    require(old['status'] == 'ACTUAL_LOADER_CODE_OWNER_GENERATION_PASS' and old['catalogue']['all_original_source_images'] == 70,
            'frozen_completed_owner_proof_required')
    original_fixture = W / 'tests/test_r192_loader_code_owner.cpp'; fixture = W / 'tests/test_r192_loader_owner_corpus.cpp'
    public_prefix_source = W / 'tests/test_r192_iop_v8_start.cpp'
    pins = old['source_pins'] + [{'path': str(f), 'sha256': sha(f)} for f in [Path(__file__).resolve(), fixture, args.owner_proof, args.domain,
        public_prefix_source, Path(schema_producer.__file__), W / 'scripts/iop_irx_imports.py']]
    rows = {r['original_sha256']: r for r in domain['records']}
    irx = fixed.load_tool('corpus_original_IRX_schemas', W / 'scripts/iop_irx_imports.py')
    # Rebuild every schema from complete hash-pinned original bytes and real
    # relocation sections. Ignored historical JSON is not an input authority.
    descriptions = [schema_producer.schemas(row, irx) for _, row in sorted(rows.items())]
    require(len(rows) == len(descriptions) == 70 and {d['original_sha256'] for d in descriptions} == set(rows), 'all70_source_images_required')
    require(not any(d['unresolved_record_candidates'] for d in descriptions), 'original_static_schema_unproved')
    cpp_include = args.owner_proof.parent / 'original-sources.inc'
    require(sha(cpp_include) == old['catalogue']['generated_include_sha256'], 'source_catalogue_changed')
    bodies, witnesses, controls = {}, {}, []
    for desc in descriptions:
        digest = desc['original_sha256']; row = rows[digest]; image = Path(desc['original_path']).read_bytes()
        require(fixed.sha(image) == digest, 'original_payload_changed')
        built = row['object_compilations']; body_rec = next(x for x in built if Path(x['source']).name == 'iop_pic.cpp')
        descriptor_rec = next(x for x in built if Path(x['source']).name == 'iop_pic_descriptor.cpp')
        for rec in (body_rec, descriptor_rec):
            require(sha(Path(rec['source'])) == rec['source_sha256'] and sha(Path(rec['object'])) == rec['object_sha256'], 'original_compiled_source_or_object_changed')
            pins += [{'path': rec['source'], 'sha256': rec['source_sha256']}, {'path': rec['object'], 'sha256': rec['object_sha256']}]
        body_path = Path(body_rec['source']); body = body_path.read_text(); bodies[digest] = body
        require(body.encode() == body_path.read_bytes(), 'generated_source_byte_ranges_require_UTF8_LF')
        descriptor = Path(descriptor_rec['source']).read_text(); word_array = re.search(r'const uint32_t words\[\] = \{(.*?)\};', descriptor, re.S)
        require(word_array, 'original_descriptor_word_array_unproved')
        words = [int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]+)u', word_array.group(1))]
        load = desc['load']; original = image[load['file_offset']:load['file_offset'] + desc['text_size']]
        require(len(words) * 4 == desc['text_size'] and b''.join(x.to_bytes(4, 'little') for x in words) == original, 'full_original_descriptor_text_join_failed')
        entries = []
        for record in desc['records']:
            if record['kind'] != 0: continue
            for ordinal, site in enumerate(record['original_callable_pointer_sites']):
                entries.append(code_map(image, desc, body, record['image_offset'], ordinal, site['target_image_offset'], words[site['target_image_offset'] // 4]))
        witnesses[digest] = entries
    require(all(sha(Path(x['path'])) == x['sha256'] for x in pins), 'input_source_pin_changed')
    # Negative entry-map controls use actual source context, not a fabricated
    # extra function or a new catalogue member.
    control_desc = next(d for d in descriptions if d['original_sha256'].startswith('85a465'))
    control_body = bodies[control_desc['original_sha256']]; control_image = Path(control_desc['original_path']).read_bytes()
    good = next(x for x in witnesses[control_desc['original_sha256']] if x['status'] == 'PROVED_ENTRY_MAP')
    for name, target, body, word, expected in [
        ('unaligned_target', good['target_offset'] + 2, control_body, None, 'target_not_original_file_backed_text'),
        ('wrong_original_word', good['target_offset'], control_body, int(good['original_word'], 16) ^ 1, 'original_target_word_mismatch'),
        ('missing_generated_label', good['target_offset'], control_body.replace(good['label'] + ':\n', 'REMOVED_LABEL:\n'), None, 'generated_entry_label_unproved')]:
        outcome = code_map(control_image, control_desc, body, good['table_offset'], good['ordinal'], target, word)
        controls.append({'name': name, 'status': 'REJECTED' if outcome.get('reason') == expected else 'NOT_REJECTED', 'outcome': outcome})
    other = next(x for x in witnesses[control_desc['original_sha256']] if x['status'] == 'PROVED_ENTRY_MAP' and x['target_offset'] != good['target_offset'])
    forged = dict(good, target_offset=other['target_offset'])
    slot = dict(good, status='VALIDATED_IDENTITY')
    controls.append({'name': 'swapped_valid_target_witness', 'status': 'REJECTED' if not slot_join(slot, forged) else 'NOT_REJECTED',
                     'original_target': good['target_offset'], 'forged_target': forged['target_offset']})
    args.output.mkdir(parents=True, exist_ok=False); archive = args.output / 'source-archive'; archive.mkdir()
    for f in (Path(__file__).resolve(), fixture): (archive / f.name).write_bytes(f.read_bytes())
    (args.output / 'INPUT_PINS.json').write_text(json.dumps(pins, indent=2) + '\n', encoding='utf-8', newline='\n')
    (args.output / 'ORIGINAL_SCHEMAS.json').write_text(json.dumps(descriptions, indent=2) + '\n', encoding='utf-8', newline='\n')
    (args.output / 'ORIGINAL_EXPORT_CODE_MAP.json').write_text(json.dumps(witnesses, indent=2) + '\n', encoding='utf-8', newline='\n')
    compression = Compression(args.output)
    prefix = args.output / 'original-prefix.inc'
    prefix.write_text(public_prefix_source.read_text().split('int main(', 1)[0], encoding='utf-8', newline='\n')
    command = []
    for item in old['build_command']:
        if item == str(original_fixture): item = str(fixture)
        elif item.startswith('-DR192_OWNER_FIXTURE_PREFIX='): item = '-DR192_OWNER_FIXTURE_PREFIX="' + prefix.as_posix() + '"'
        command.append(item)
    require(str(fixture) in command and command[-2] == '-o', 'frozen_build_layout_changed')
    require('-DR192_OWNER_SOURCES="' + cpp_include.as_posix() + '"' in command, 'verified_source_catalogue_not_build_input')
    exe = args.output / 'whole-corpus-owner.exe'; command[-1] = str(exe)
    require(shutil.disk_usage(W).free > RESERVE + (256 << 20), 'reserved_disk_space_required')
    rsp = args.output / 'build.rsp'; rsp.write_text('\n'.join(builder.quote_response(x) for x in command[1:]) + '\n', encoding='utf-8', newline='\n')
    result = subprocess.run([command[0], '@' + str(rsp)], capture_output=True, text=True, timeout=180)
    (args.output / 'build.log').write_text(result.stdout + result.stderr, encoding='utf-8', newline='\n'); require(result.returncode == 0, 'corpus_fixture_compile_failed')
    compiler = Path(command[0]); runtime = next(Path(x) for x in command if x.endswith('libps2runtime.a')).parent.parent
    env = {k: v for k, v in os.environ.items() if not k.startswith(('PS2_', 'MKA_', 'SDL_'))}
    env['PATH'] = str(runtime / 'out') + os.pathsep + str(compiler.parent) + os.pathsep + env.get('PATH', '')
    cases, compressions = [], []
    for index, digest in enumerate(sorted(rows)):
        for route in ('filename', 'buffer'):
            require(shutil.disk_usage(W).free > RESERVE + (64 << 20), 'reserve_before_next_pair_required')
            case_dir = args.output / f'image-{index:02}-{route}'; case_dir.mkdir(); runs = {}
            for mode in ('reference', 'connector'):
                prefix = case_dir / mode; cmd = [str(exe), mode, rows[digest]['original_copy'], route, str(prefix)]
                result = subprocess.run(cmd, capture_output=True, text=True, env=env, timeout=120)
                (case_dir / (mode + '.log')).write_text(result.stdout + result.stderr, encoding='utf-8', newline='\n')
                path = prefix.with_suffix('.json')
                runs[mode] = {'argv': cmd, 'exit_code': result.returncode, 'measurement': json.loads(path.read_text()) if path.exists() else None}
            comparisons = []
            for suffix in ('cpu', 'ram', 'iop-state'):
                a, b = [case_dir / (mode + '.' + suffix) for mode in ('reference', 'connector')]
                equal = a.exists() and b.exists() and a.read_bytes() == b.read_bytes()
                comparisons.append({'artifact': suffix, 'equal': equal, 'reference_sha256': sha(a) if a.exists() else None, 'connector_sha256': sha(b) if b.exists() else None})
                for path in (a, b):
                    if path.exists(): compressions.append(compression.compress(path))
            measured = runs['connector']['measurement'] or {}; entry_map = {(e['table_offset'], e['ordinal']): e for e in witnesses[digest]}
            slot_results = []
            for slot in measured.get('slots', []):
                proof = entry_map.get((slot['table_offset'], slot['ordinal']))
                passed = slot_join(slot, proof)
                slot_results.append(dict(slot, code_map=proof, final_status='IDENTITY_AND_ENTRY_MAP_PROVED' if passed else 'REFUSED_OR_UNPROVED'))
            captured = all(r['exit_code'] == 0 and (r['measurement'] or {}).get('capture_status') == 'COMPLETE' for r in runs.values())
            slots_complete = len(slot_results) == len(witnesses[digest])
            case = {'original_sha256': digest, 'route': route, 'runs': runs, 'comparisons': comparisons, 'slots': slot_results,
                    'capture_and_byte_comparison': 'PASS' if captured and all(c['equal'] for c in comparisons) else 'REFUSED',
                    'source_owner_admitted': measured.get('source_owner_admitted', False), 'slot_population_complete': slots_complete,
                    'no_export_tables': not witnesses[digest], 'owner_refusal': measured.get('owner_refusal'),
                    'identity_status': 'PROVED' if measured.get('source_owner_admitted') and slots_complete and all(s['final_status'] == 'IDENTITY_AND_ENTRY_MAP_PROVED' for s in slot_results) else 'REFUSED_OR_UNPROVED'}
            cases.append(case)
            (case_dir / 'COMPARISON.json').write_text(json.dumps(case, indent=2) + '\n', encoding='utf-8', newline='\n')
            (args.output / 'PROGRESS.json').write_text(json.dumps({'completed_pairs': len(cases), 'required_pairs': 140,
                'physical_raw_bytes': sum(c['physical_after'] for c in compressions), 'reserve_free_bytes': shutil.disk_usage(W).free}, indent=2) + '\n', encoding='utf-8', newline='\n')
    changed = [x['path'] for x in pins if sha(Path(x['path'])) != x['sha256']]
    captured = len(cases) == 140 and all(c['capture_and_byte_comparison'] == 'PASS' and c['slot_population_complete'] for c in cases)
    admitted = captured and all(c['identity_status'] == 'PROVED' for c in cases)
    counts = Counter(s['reason'] or (s['code_map'] or {}).get('reason') or s['status'] for c in cases for s in c['slots'] if s['final_status'] != 'IDENTITY_AND_ENTRY_MAP_PROVED')
    owner_counts = Counter(c['owner_refusal'] for c in cases if not c['source_owner_admitted'])
    passed = admitted and not changed and all(c['status'] == 'REJECTED' for c in controls)
    report = {'status': 'ALL70_SOURCE_OWNER_IDENTITIES_PASS' if passed else 'FINITE_CORPUS_COMPLETE_WITH_REFUSALS',
        'classification': 'source-authenticated actual loader identity capability only', 'native_credit': 0,
        'source_pins': pins, 'build_command': command, 'executable_sha256': sha(exe), 'cases': cases,
        'entry_map_controls': controls, 'input_changes': changed, 'capture_complete': captured,
        'summary': {'source_images': 70, 'routes_per_image': 2, 'pairs': len(cases), 'byte_comparison_pass_pairs': sum(c['capture_and_byte_comparison'] == 'PASS' for c in cases),
            'source_owner_admitted_pairs': sum(c['source_owner_admitted'] for c in cases), 'no_export_images': sum(not v for v in witnesses.values()),
            'original_export_slots': sum(map(len, witnesses.values())), 'route_slot_observations': sum(len(c['slots']) for c in cases),
            'proved_route_slots': sum(s['final_status'] == 'IDENTITY_AND_ENTRY_MAP_PROVED' for c in cases for s in c['slots']),
            'owner_refusal_histogram': dict(owner_counts), 'slot_refusal_histogram': dict(counts)},
        'compression': {'volume': compression.volume, 'files': compressions, 'logical_raw_bytes': sum(c['logical_bytes'] for c in compressions),
            'physical_raw_bytes': sum(c['physical_after'] for c in compressions), 'final_free_bytes': shutil.disk_usage(W).free},
        'limits': ['All70 payloads and both literal loader paths; no module entry/guest instructions/game executed.',
            'Twelve images with no export tables are reported as such; no callable identity is invented for them.',
            'Entry-map proof joins original pointer/target word, descriptor full text, emitted case/label/budget PC and pinned compiled body; no callable ABI/semantics claim.',
            'Raw fullCPU/RAM/serializer outputs are retained and byte-compared; NTFS compression changes allocation only, verified by SHA before/after.',
            'Each original loader init thread is explicitly cancelled while unstarted before serializer capture; module boot/on_exit behavior unproved.',
            'Retirement, restored source owners, registry/IRQ runtime adoption and native credit remain outside this measurement.']}
    target = args.output / 'RESULT.json'; target.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'result': str(target), 'sha256': sha(target), 'status': report['status'], 'summary': report['summary']}))
    return 0 if passed else 1


if __name__ == '__main__': raise SystemExit(main())
