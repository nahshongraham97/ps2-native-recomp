"""Falsify the offline VU body auditor using explicitly constructed copies.

Original generated files are authenticated and never modified. These fixtures
test admission checks, not execution or retail-function equivalence.
"""
from pathlib import Path
import argparse
import json
import re
import shutil
import subprocess
import sys

from extract_r192_elf_vu import require, sha

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--generation', type=Path, required=True)
    parser.add_argument('--expected-generation-sha', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    require(out.is_relative_to(ROOT / 'build') and not out.exists(), 'fresh_owned_output_required')
    require(sha(args.generation) == args.expected_generation_sha, 'generation_pin')
    generation = json.loads(args.generation.read_text())
    require(generation['generation_requested'] and generation['generation_all_pass'], 'complete_generation_required')
    row = generation['records'][0]
    original = Path(row['entries']).parent / 'generated'
    for pin in row['generated_files']:
        require(sha(original / Path(pin['path']).name) == pin['sha256'], 'original_generated_file_changed')
    auditor = Path(__file__).with_name('audit_r192_vu_entry_bodies.py')
    pins = {str(p): sha(p) for p in [Path(__file__), auditor, args.generation]}
    out.mkdir(parents=True)
    runs = []
    cases = [
        ('positive', None),
        ('conflicting-body', 'REFUSED_GLOBAL_BODY_ADMISSION'),
        ('unsupported-lower', 'REFUSED_GLOBAL_BODY_ADMISSION'),
        ('missing-table-entry', 'table_body_set_changed'),
        ('unrepinned-source-change', 'generated_file_changed'),
    ]
    for name, expected in cases:
        directory = out / name
        directory.mkdir()
        records = []
        for index in range(2):
            module = directory / f'module-{index}'
            folder = module / 'generated'
            shutil.copytree(original, folder)
            target = next(p for p in sorted(folder.glob('vu1_*.cpp')) if p.name != 'vu1_table.cpp')
            if index == 1 and name in ('conflicting-body', 'unrepinned-source-change'):
                text = target.read_text()
                text, count = re.subn(r'(^void vu1_[0-9A-F]{16}\(Vu0Regs &r, std::uint8_t \*mem\) \{)',
                                      r'\1\n r.vi[1] ^= 1u;', text, count=1, flags=re.M)
                require(count == 1, 'fixture_function_seam_changed')
                target.write_text(text, newline='\n')
            if index == 1 and name == 'unsupported-lower':
                text, count = re.subn(r'(^L_[0-9A-F]{4}:)', r'\1 vu::interpret(1, 0u);',
                                      target.read_text(), count=1, flags=re.M)
                require(count == 1, 'fixture_instruction_label_changed')
                target.write_text(text, newline='\n')
            if index == 1 and name == 'missing-table-entry':
                table = folder / 'vu1_table.cpp'
                text, count = re.subn(r'^.*\{0x([0-9A-F]{16})ull, vu1_\1\}.*\n', '',
                                      table.read_text(), count=1, flags=re.M)
                require(count == 1, 'fixture_table_seam_changed')
                table.write_text(text, newline='\n')
            files = [{'path': str(p), 'sha256': sha(p)} for p in sorted(folder.glob('*.cpp'))]
            if index == 1 and name == 'unrepinned-source-change':
                for pin in files:
                    if Path(pin['path']).name == target.name:
                        pin['sha256'] = sha(original / target.name)
            records.append({'generation_pass': True, 'entries': str(module / 'entries.tsv'),
                            'generated_files': files, 'counts': {'admitted': row['counts']['admitted']}})
        # Deliberately constructed receipt, never represented as original evidence.
        fixture = {'classification': 'CONSTRUCTED_ADMISSION_MUTATION_FIXTURE',
                   'generation_requested': True, 'generation_all_pass': True,
                   'records': records, 'totals': {'admitted': 2 * row['counts']['admitted']},
                   'unique_keys': row['counts']['admitted']}
        manifest = directory / 'CONSTRUCTED.json'
        manifest.write_text(json.dumps(fixture, indent=2) + '\n', newline='\n')
        command = [sys.executable, '-B', str(auditor), '--generation', str(manifest),
                   '--expected-generation-sha', sha(manifest), '--prior-generated', str(original),
                   '--output', str(directory / 'audit')]
        run = subprocess.run(command, capture_output=True, text=True)
        log = run.stdout + run.stderr
        (directory / 'audit.log').write_text(log, newline='\n')
        result_path = directory / 'audit/RESULT.json'
        result = json.loads(result_path.read_text()) if result_path.exists() else None
        if expected is None:
            passed = run.returncode == 0 and result['status'] == 'BODY_IDENTITY_AND_SUPPORTED_LOWER_AUDIT_PASS'
            passed = passed and result['prior_exact_common_bodies'] == row['counts']['admitted']
        elif expected == 'REFUSED_GLOBAL_BODY_ADMISSION':
            passed = run.returncode == 2 and result and result['status'] == expected
            if name == 'conflicting-body':
                passed = passed and len(result['same_key_different_bodies']) == 1 and len(result['changed_common_bodies']) == 1
            else:
                passed = passed and len(result['unsupported_lower_emissions']) == 1
        else:
            passed = run.returncode != 0 and expected in log
        runs.append({'case': name, 'argv': command, 'exit': run.returncode, 'expected': expected or 'PASS',
                     'passed': bool(passed), 'log_sha256': sha(directory / 'audit.log'),
                     'result_sha256': sha(result_path) if result else None})
        print(json.dumps(runs[-1]), flush=True)
    require(pins == {p: sha(Path(p)) for p in pins}, 'verification_input_changed')
    for pin in row['generated_files']:
        require(sha(original / Path(pin['path']).name) == pin['sha256'], 'original_changed_during_test')
    result = {'status': 'PASS_AUDITOR_FALSIFICATION' if all(r['passed'] for r in runs) else 'FAIL',
              'source_pins': pins, 'original_fixture_payload': row['original_file_offset'],
              'fixture_unique_keys': row['counts']['admitted'], 'runs': runs,
              'scope': 'Constructed mutations of authenticated owned copies; no VU execution or native credit.'}
    target = out / 'RESULT.json'
    target.write_text(json.dumps(result, indent=2) + '\n', newline='\n')
    print(json.dumps({'status': result['status'], 'result': str(target), 'sha256': sha(target)}))
    return 0 if result['status'].startswith('PASS') else 1


if __name__ == '__main__':
    raise SystemExit(main())
