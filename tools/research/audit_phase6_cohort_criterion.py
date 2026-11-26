"""Separate the original saved-state criterion from constructed capability.

Read-only private inputs; never manufacture a call by rewriting saved PC or
arguments. A missing authenticated call is NOT_ESTABLISHED, not a mismatch.
"""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
SEED = 'Phase6:N4:holdout:20261126'
PINS = {
    'SAMPLE.json': 'd5d2e9bc6cf3cd97df0111de20c1a966bdcd05d500e88775da42e361a621aa7e',
    'POPULATION.json': '01239cb9ec1cd96ce5830bfca12e4f0f7beefa0e497a7e9184f4c5abcc7437cb',
}
CONSTRUCTED = '8718e5f3f94b2f94201db7ea4d61013bb98d25ed58e5ac1be57d54028023d4e3'


def need(value, why):
    if not value:
        raise ValueError(why)


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as source:
        for block in iter(lambda: source.read(4 << 20), b''):
            digest.update(block)
    return digest.hexdigest()


def cohort(directory):
    for name, digest in PINS.items():
        need(sha(directory / name) == digest, 'frozen_cohort_pin:' + name)
    original = json.loads((directory / 'SAMPLE.json').read_text())
    population = json.loads((directory / 'POPULATION.json').read_text())
    keys = {row['key'] for row in original}
    need(len(original) == len(keys) == 30, 'original_denominator')
    ranked = sorted(
        (r for r in population if r.get('in_scope') and r.get('sample_eligible') and r['key'] not in keys),
        key=lambda r: (hashlib.sha256(SEED.encode() + b'\0' + r['key'].encode()).hexdigest(), r['key']))
    rows = [dict(r, cohort='original30') for r in original]
    rows += [dict(r, cohort='heldout30') for r in ranked[:30]]
    need(len(rows) == len({r['key'] for r in rows}) == 60, 'independent_60_denominator')
    return rows


def original_admission(constructed_pass, actual_calls, actual_equal):
    # Merely matching a PC cannot prove a function's ABI or complete effects.
    return constructed_pass and actual_calls > 0 and actual_equal == actual_calls


def self_test():
    need(not original_admission(True, 0, 0), 'zero_historical_cases_must_not_pass')
    need(not original_admission(False, 61, 61), 'constructed_refusal_must_not_pass')
    need(not original_admission(True, 61, 60), 'historical_mismatch_must_not_pass')
    need(original_admission(True, 61, 61), 'complete_comparator_positive_control')
    return ['reject_zero_historical_cases', 'reject_constructed_refusal', 'reject_historical_mismatch', 'positive_boolean_gate_only']


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('parent-root', 'cohort-dir', 'constructed-result', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    parent = args.parent_root.resolve()
    output = args.output.resolve()
    need(output.is_relative_to(ROOT / 'build/oracle-telemetry') and not output.exists(), 'fresh_owned_output')
    need(shutil.disk_usage(ROOT).free >= 12 * 1024**3, '12GiB_reserve')
    rows = cohort(args.cohort_dir)
    need(sha(args.constructed_result) == CONSTRUCTED, 'constructed_receipt_pin')
    prior = json.loads(args.constructed_result.read_text())
    attempts = {r['key']: r for r in prior['attempts']}
    need(set(attempts) == {r['key'] for r in rows}, 'same_cohort_required')
    manifest = parent / 'launchers/oracle_original_manifest.json'
    inventory = json.loads(manifest.read_text())
    state_root = parent.parent.parent / 'milestone_work_verified_playable_20261110/games/mkarmageddon/disc/states'
    reader = ROOT / 'tests/test_phase4_aot_equivalence.cpp'
    reader_text = reader.read_text()
    need('cpu = 20, ram = cpu + 1456' in reader_text and 'offsetof(ps2::Cpu, pc) == 548' in reader_text
         and 'only source-proved native version8 admitted' in reader_text, 'v8_serialized_Cpu_ABI_reader_witness')
    prior_reader = {digest for name, digest in prior['source_pins'].items()
                    if name.replace('\\', '/').endswith('/tests/test_phase4_aot_equivalence.cpp')}
    need(prior_reader == {sha(reader)}, 'same_measured_Cpu_ABI_reader_required')
    pins = {str(q.resolve()): sha(q) for q in (Path(__file__), manifest, reader, args.constructed_result,
            args.cohort_dir / 'SAMPLE.json', args.cohort_dir / 'POPULATION.json')}
    states = []
    for name, expected in sorted(inventory.items()):
        if not (name.startswith('games/mkarmageddon/disc/states/') and name.endswith('.state')):
            continue
        path = state_root / Path(name).name
        need(path.stat().st_size == expected['size'] and sha(path) == expected['sha256'], 'protected_state_identity:' + name)
        with path.open('rb') as source:
            prefix = source.read(20 + 1456)
            source.seek(-8, 2)
            end = source.read(8)
        need(len(prefix) == 1476 and prefix[:8] == b'PS2STATE' and struct.unpack_from('<I', prefix, 8)[0] == 8
             and prefix[12:20] == b'ee\0\0\0\0\0\0' and end == b'end\0\0\0\0\0', 'v8_ee_prefix_identity')
        pc = struct.unpack_from('<I', prefix, 20 + 548)[0]
        states.append(dict(name=path.name, sha256=expected['sha256'], size=expected['size'], raw_pc=f'{pc:08x}',
                           unwind_cleared_pc=f'{pc & ~1:08x}'))
    need(len(states) == 61, 'original61_denominator')
    report_rows = []
    for row in rows:
        old = attempts[row['key']]
        raw = [s['name'] for s in states if int(s['raw_pc'], 16) == int(row['address'], 16)]
        cleared = [s['name'] for s in states if int(s['unwind_cleared_pc'], 16) == int(row['address'], 16)]
        # This audit does not execute a saved machine or authenticate a call
        # merely from its address. A positive match needs a separate harness.
        need(not raw and not cleared, 'actual_state_candidate_requires_authenticated_call_harness:' + row['key'])
        passed = old['status'] == 'PASS_CONSTRUCTED_CAPABILITY'
        report_rows.append(dict(key=row['key'], cohort=row['cohort'], original_criterion='NOT_ESTABLISHED',
            reason='no_authenticated_saved_state_call' if passed else 'native_transform_refused_before_state_equivalence',
            states_inspected=61, raw_entry_matches=0, unwind_cleared_entry_matches=0,
            historical_function_comparisons=0, historical_mismatches=0, constructed_only=old['status'],
            constructed_comparisons=old.get('constructed_comparisons', 0), declined_inputs=old.get('constructed_declined', 0),
            first_refusal=None if passed else old.get('reason'), original_criterion_admitted=original_admission(passed, 0, 0)))
    aggregates = {}
    for group in ('original30', 'heldout30'):
        selected = [r for r in report_rows if r['cohort'] == group]
        aggregates[group] = dict(denominator=len(selected), original_criterion_established=0,
            original_criterion_established_rate=0.0, original_criterion_not_established=len(selected),
            historical_function_comparisons=0, historical_mismatches=0,
            constructed_only_pass=sum(r['constructed_only'] == 'PASS_CONSTRUCTED_CAPABILITY' for r in selected),
            constructed_only_comparisons=sum(r['constructed_comparisons'] for r in selected),
            declined_inputs=sum(r['declined_inputs'] for r in selected))
    need(all(sha(q) == digest for q, digest in pins.items()), 'audit_inputs_changed')
    controls = self_test()
    output.mkdir(parents=True)
    result = dict(status='AUDITED_ORIGINAL_CRITERION_NOT_ESTABLISHED_CONSTRUCTED_SEPARATE',
        invocation=[sys.executable, *sys.argv], source_pins=pins, states=states, rows=report_rows, cohorts=aggregates,
        gate_controls=controls, credit_delta=0,
        original_comparison='The earlier0/30 was refusal before differential execution. New constructed-only figures use a changed harness/domain and are not a comparable whole-function/game-state pass-rate improvement.',
        continuation='The prior constructed-only N5 gate claim is superseded. No original-criterion general-stage continuation or production admission is established. The owner separately authorized the existing44 obligations,six refusals,and camera work.',
        limits=['CPU-prefix/whole-file identity inspection only; no full machine restore, new game run or historical function execution.',
                'Missing actual inputs are not behavioral failures. No PC/argument rewriting or constructed case is labelled a historical invocation.',
                'Four boolean admission controls test reporting only, not function equivalence.'])
    target = output / 'RESULT.json'
    target.write_text(json.dumps(result, indent=2) + '\n', encoding='utf8', newline='\n')
    print(json.dumps(dict(status=result['status'], cohorts=aggregates, result=str(target), sha256=sha(target))))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
