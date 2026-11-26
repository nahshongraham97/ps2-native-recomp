"""Seal already compiled original-source VU DLLs into a fresh owned package.

No compilation, launcher change or original file write. Pilot packages are
explicit component fixtures; game integration requires the complete payload set.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import struct

from extract_r192_elf_vu import require, sha

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('modules', 'generation', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--expected-modules-sha', required=True)
    parser.add_argument('--expected-generation-sha', required=True)
    parser.add_argument('--pilot', action='store_true')
    args = parser.parse_args()
    out = args.output.resolve()
    require(out.is_relative_to(ROOT / 'build') and not out.exists(), 'fresh_owned_output_required')
    require(sha(args.modules) == args.expected_modules_sha and sha(args.generation) == args.expected_generation_sha, 'input_receipt_pin')
    modules = json.loads(args.modules.read_text())
    generation = json.loads(args.generation.read_text())
    require(modules['status'] == 'NATIVE_VU_MODULE_BUILD_COMPLETE', 'native_build_incomplete')
    require(generation['generation_requested'] and generation['generation_all_pass'], 'source_generation_incomplete')
    require(modules['input_pins']['generation_sha256'] == sha(args.generation), 'native_generation_identity_changed')
    require(modules['complete_original_payload_set'] or (args.pilot and modules['modules'] == 1), 'complete_original_payload_set_required')
    require(len(modules['module_receipts']) == modules['modules'], 'module_denominator')
    originals = {row['original_file_offset']: row for row in generation['records']}
    rows = []
    union = set()
    stamp = None
    for pin in modules['module_receipts']:
        receipt = Path(pin['path'])
        require(sha(receipt) == pin['sha256'], 'module_receipt_changed')
        row = json.loads(receipt.read_text())
        require(row['status'] == 'COMPILED_NATIVE_MODULE_METADATA_PASS' and row['input_pins'] == modules['input_pins'], 'native_module_identity_changed')
        source = originals[row['original_file_offset']]
        require(sha(Path(source['entries'])) == source['entries_sha256'], 'entry_list_changed')
        keys = sorted(int(line.split()[0], 16) for line in Path(source['entries']).read_text().splitlines())
        require(len(keys) == len(set(keys)) == row['programs'] == source['counts']['admitted'], 'module_key_denominator')
        union.update(keys)
        dll = Path(row['DLL'])
        require(sha(dll) == row['DLL_sha256'], 'compiled_DLL_changed')
        require(all(check['exit_code'] == check['expected_exit'] for check in row['metadata_checks']), 'native_module_checks_failed')
        require(stamp is None or stamp == row['source_ABI_stamp'], 'inconsistent_source_ABI_stamp')
        stamp = row['source_ABI_stamp']
        rows.append({'name': f"vu-{row['original_file_offset']:08x}.dll", 'source': str(dll),
                     'sha256': sha(dll), 'programs': len(keys),
                     'key_sha256': hashlib.sha256(b''.join(struct.pack('<Q', key) for key in keys)).hexdigest()})
    require(len({r['name'] for r in rows}) == len(rows), 'duplicate_payload_module')
    if not args.pilot:
        require(len(rows) == len(originals) and len(union) == generation['unique_keys'], 'original_payload_or_key_domain_incomplete')
    # Measured ABI must come from the final native metadata probe, not a guess.
    probe = Path(modules['module_receipts'][0]['path']).parent / 'probe-positive.log'
    match = re.fullmatch(r'PASS_NATIVE_MODULE programs=\d+ regs=(\d+) host=(\d+) null_controls=3 no_program_execution=1\s*', probe.read_text())
    require(match is not None, 'native_probe_ABI_receipt_changed')
    copy_bytes = sum(Path(r['source']).stat().st_size for r in rows) if args.pilot else 0
    require(shutil.disk_usage(ROOT / 'build').free > (12 << 30) + copy_bytes + (64 << 20), 'disk_reserve_exhausted')
    out.mkdir(parents=True)
    lines = [f"R192_AOT_VU_V1 {stamp.removeprefix('0x').upper()} {match[1]} {match[2]} {len(rows)} {len(union)}"]
    for row in sorted(rows, key=lambda r: r['name']):
        target = out / row['name']
        # Pilot fixtures are independent copies. Full packages retain the exact
        # native outputs through exclusive hardlinks, never opened for writing.
        # The falsification driver always makes its own independent copies.
        if args.pilot:
            with target.open('xb') as stream:
                stream.write(Path(row['source']).read_bytes())
        else:
            os.link(row['source'], target)
        require(sha(target) == row['sha256'], 'sealed_copy_changed')
        lines.append(f"{row['sha256']} {row['key_sha256']} {row['programs']} {row['name']}")
    manifest = out / 'manifest.txt'
    manifest.write_text('\n'.join(lines) + '\n', encoding='ascii', newline='\n')
    result = {'status': 'SEALED_VU_PILOT_COMPONENT_ONLY' if args.pilot else 'SEALED_ORIGINAL_SOURCE_VU_PACKAGE',
              'module_build': {'path': str(args.modules.resolve()), 'sha256': sha(args.modules)},
              'generation': {'path': str(args.generation.resolve()), 'sha256': sha(args.generation)},
              'producer_sha256': sha(Path(__file__)), 'manifest_sha256': sha(manifest),
              'source_ABI_stamp': stamp, 'unique_keys': len(union), 'modules': rows,
              'publication': 'independent_pilot_copies' if args.pilot else 'exclusive_hardlinks_never_written',
              'scope': 'Offline native code identity/ABI/package only; execution equivalence and whole-game closure remain separate.'}
    receipt = out / 'PACKAGE.json'
    receipt.write_text(json.dumps(result, indent=2) + '\n', newline='\n')
    print(json.dumps({'status': result['status'], 'manifest_sha256': sha(manifest),
                      'receipt_sha256': sha(receipt), 'unique_keys': len(union), 'modules': len(rows)}))


if __name__ == '__main__':
    main()
