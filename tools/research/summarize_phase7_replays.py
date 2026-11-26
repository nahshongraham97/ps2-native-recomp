"""Join serial replay counters to the retained base-AOT source and leaf census.

Counts are observed wrapper entries, not proof of all function calls or absence
of execution. Structural exclusions are not measured compiler first failures.
"""
from pathlib import Path
import argparse
import collections
import csv
import hashlib
import json
import re
import shutil
import sys

ROOT = Path(__file__).resolve().parents[2]
POPULATION = '01239cb9ec1cd96ce5830bfca12e4f0f7beefa0e497a7e9184f4c5abcc7437cb'


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as source:
        for block in iter(lambda: source.read(4 << 20), b''):
            digest.update(block)
    return digest.hexdigest()


def need(value, message):
    if not value:
        raise ValueError(message)


def wrapper_labels(source):
    """Recover labels from each wrapper's symbol and explicit count calls.

    Labels are opaque: a known producer bug appended the mangling separator E
    to the hexadecimal unit number. Collisions never receive guessed counts.
    The caller authenticates this bounded source before parsing it.
    """
    need('R"' not in source, 'raw_cpp_literal_requires_lexer_extension')
    masked = re.sub(r"""//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'""",
                    lambda match: ''.join('\n' if char == '\n' else ' ' for char in match[0]),
                    source, flags=re.S)
    pattern = re.compile(r'void\s+__wrap__ZN3gen9unit_([0-9A-Fa-f]{4})ERN3ps23CpuEjj\([^)]*\)\s*\{')
    definitions = list(pattern.finditer(masked))
    labels = collections.defaultdict(set)
    uncounted = []
    actual_units = set()
    for index, definition in enumerate(definitions):
        unit = int(definition[1], 16)
        need(unit not in actual_units, 'duplicate_wrapper_symbol')
        actual_units.add(unit)
        depth, end = 1, definition.end()
        while depth:
            need(end < len(masked), 'truncated_wrapper_body')
            depth += (masked[end] == '{') - (masked[end] == '}')
            end += 1
        body = masked[definition.end():end-1]
        calls = {int(raw, 16) for raw in re.findall(r'__real__ZN3gen9unit_([0-9A-Fa-f]{4})ERN3ps23CpuEjj', body)}
        need(calls == {unit}, 'wrapper_real_symbol_mismatch')
        recorded = {int(raw, 16) for raw in re.findall(r'ps2::capture::record_unit_entry\(\s*(0x[0-9A-Fa-f]+)u\s*,\s*entry\s*\)', body)}
        need(len(recorded) <= 1, 'wrapper_has_conflicting_labels')
        if not recorded:
            uncounted.append(unit)
        else:
            labels[next(iter(recorded))].add(unit)
    return actual_units, labels, uncounted


def resolve_label(labels, label):
    candidates = labels.get(label, set())
    need(candidates, 'observed_counter_label_not_in_pinned_wrapper_source')
    return next(iter(candidates)) if len(candidates) == 1 else None


def label_controls():
    def wrapper(unit, recorded):
        count = '' if recorded is None else f'ps2::capture::record_unit_entry(0x{recorded:04X}u, entry);'
        return f'void __wrap__ZN3gen9unit_{unit:04X}ERN3ps23CpuEjj(ps2::Cpu &c, std::uint32_t entry, std::uint32_t depth) {{ {count} __real__ZN3gen9unit_{unit:04X}ERN3ps23CpuEjj(c, entry, depth); }}\n'
    units, labels, uncounted = wrapper_labels(wrapper(0, 0xe) + wrapper(1, 0x1e) + wrapper(0x1e, 0x1e) + wrapper(0x20, None))
    need(units == {0, 1, 0x1e, 0x20} and labels[0xe] == {0}, 'mangling_separator_label_control')
    need(labels[0x1e] == {1, 0x1e} and uncounted == [0x20], 'collision_and_uncounted_controls')
    need(resolve_label(labels, 0x1e) is None and resolve_label(labels, 0xe) == 0, 'ambiguous_allocation_was_not_withheld')
    try:
        wrapper_labels(wrapper(0, 0xe).replace('__real__ZN3gen9unit_0000', '__real__ZN3gen9unit_0001'))
    except ValueError:
        pass
    else:
        raise ValueError('forged_real_symbol_was_not_rejected')
    return dict(mangled_label_recovered=True, collided_label_withheld=True,
                uncounted_wrapper_reported=True, forged_symbol_rejected=True)


def main():
    controls = label_controls()
    if sys.argv[1:] == ['--self-test']:
        print(json.dumps(controls))
        return 0
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--replays', type=Path, required=True)
    parser.add_argument('--population', type=Path, required=True)
    parser.add_argument('--linked-source', type=Path, required=True)
    parser.add_argument('--wrapper-source', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--allow-in-progress', action='store_true')
    args = parser.parse_args()
    result_bytes = (args.replays / 'RESULT.json').read_bytes()
    result = json.loads(result_bytes)
    need(args.allow_in_progress or result['status'] == 'ALL_PLANNED_CASES_ATTEMPTED', 'matrix_not_finished')
    need(sha(args.population) == POPULATION, 'frozen_structural_population_changed')
    plan = json.loads((args.replays / 'PLAN.json').read_text())
    planned_names = [case['sidecar'] for case in plan['sidecars']]
    observed_names = [case['sidecar'] for case in result['cases']]
    need(len(planned_names) == len(set(planned_names)) == 72, 'planned_sidecar_roster')
    need(observed_names == planned_names[:len(observed_names)], 'case_roster_differs_from_plan')
    count_key = 'attempted_cases' if result['status'] == 'ALL_PLANNED_CASES_ATTEMPTED' else 'completed_cases'
    need(result[count_key] == len(observed_names), 'completed_case_count')
    if not args.allow_in_progress:
        need(len(observed_names) == result['planned_cases'] == 72, 'incomplete_attempt_matrix')
    for planned, measured in zip(plan['sidecars'], result['cases']):
        need(all(planned[key] == measured[key] for key in ('state', 'begin', 'end', 'requested_frames', 'sha256', 'state_sha256')),
             'case_identity_differs_from_plan')
    wrapper_pins = [value for key, value in plan['source_pins'].items() if Path(key).name == args.wrapper_source.name]
    need(wrapper_pins == [sha(args.wrapper_source)], 'wrapper_source_not_identical_to_measured_producer')
    units, labels, uncounted = wrapper_labels(args.wrapper_source.read_text())
    need(len(units) == 268 and len(labels) == 264 and uncounted == [0x20], 'measured_wrapper_shape_changed')
    collisions = {label: candidates for label, candidates in labels.items() if len(candidates) > 1}
    need(collisions == {0x1e: {1, 0x1e}, 0xae: {0xa, 0xae}, 0xee: {0xe, 0xee}}, 'measured_label_collisions_changed')
    output = args.output.resolve()
    need(output.is_relative_to(ROOT / 'build/oracle-telemetry') and not output.exists(), 'fresh_owned_output')
    need(shutil.disk_usage(ROOT).free >= 12 * (1 << 30), 'disk_reserve_below_12GiB')
    ledger_path = ROOT / 'research/ledger/functions.csv'
    ledger = {r['module'] + ':' + r['address']: r for r in csv.DictReader(ledger_path.open(encoding='utf8-sig'))}
    population = {r['key']: r for r in json.loads(args.population.read_text())
                  if r['key'] in ledger and ledger[r['key']]['catalogue_kind'] == 'callable'}
    need(len(population) == 15669, 'callable_denominator')
    eligible = {key for key, row in population.items() if row['in_scope'] and row['sample_eligible']}
    need(len(eligible) == 1952, 'structural_eligible_denominator')
    mapping = {}
    source_pins = {}
    for path in sorted(args.linked_source.glob('unit_*.cpp')):
        unit = int(path.stem.split('_')[-1], 16)
        raw = path.read_bytes()
        source_pins[path.name] = hashlib.sha256(raw).hexdigest()
        for entry, pc in re.findall(r'case (\d+)u: goto L_([0-9A-Fa-f]+);', raw.decode('utf8')):
            key = (unit, int(entry))
            need(key not in mapping, 'duplicate_base_unit_entry')
            mapping[key] = pc.lower()
    need(len(source_pins) == 268, 'linked_base268_source_required')
    need(units == {int(Path(name).stem.split('_')[-1], 16) for name in source_pins}, 'wrapped_units_differ_from_linked_base')
    base_eligible = {key for key in eligible if key.startswith('SLUS_214.10:')}
    linked_addresses = {'SLUS_214.10:' + address for address in mapping.values()}
    recoverable_units = {next(iter(candidates)) for candidates in labels.values() if len(candidates) == 1}
    recoverable_addresses = {'SLUS_214.10:' + address for (unit, _), address in mapping.items() if unit in recoverable_units}
    observed = collections.Counter()
    same_to_end = collections.Counter()
    by_case = []
    raw_unmapped = 0
    interior = 0
    ambiguous = collections.Counter()
    for index, case in enumerate(result['cases']):
        counts = collections.Counter()
        for raw in case['unit_entry_counts']:
            label, entry, count = int(raw['unit_hex'], 16), int(raw['entry']), int(raw['calls'])
            need(count > 0, 'nonpositive_observation')
            unit = resolve_label(labels, label)
            if unit is None:
                ambiguous[label] += count
                continue
            address = mapping.get((unit, entry))
            if address is None:
                raw_unmapped += count
                continue
            key = 'SLUS_214.10:' + address
            if key not in population:
                interior += count
                continue
            counts[key] += count
            observed[key] += count
            if case['parity'] == 'SAME_TO_END':
                same_to_end[key] += count
        by_case.append(dict(index=index, sidecar=case['sidecar'], parity=case['parity'],
            observed_end=case['observed_end_frame'], completed=case['completed_recording_length'],
            function_entries=dict(sorted(counts.items()))))
    intersection = sorted(eligible & observed.keys())
    known = [key for key in intersection if ledger[key]['ownership_status'].startswith('verified-')]
    hard = collections.Counter()
    unresolved = collections.Counter()
    gp = 0
    for row in population.values():
        for key, value in row['hard_exclusions'].items():
            if value:
                hard[key] += 1
        for key, value in row['unresolved'].items():
            if value:
                unresolved[key] += 1
        gp += bool(row['instruction_mix'].get('GP_relative_memory', 0))
    output.mkdir(parents=True)
    with (output / 'executed_base_entries.csv').open('w', encoding='utf8', newline='') as stream:
        writer = csv.writer(stream, lineterminator='\n')
        writer.writerow(['sidecar', 'module', 'address', 'wrapper_entries', 'parity', 'observed_end'])
        for case in by_case:
            for key, count in case['function_entries'].items():
                module, address = key.split(':')
                writer.writerow([case['sidecar'], module, address, count, case['parity'], case['observed_end']])
    summary = dict(schema='phase7_observed_entries_and_capability_intersection_v1',
        status='IN_PROGRESS' if args.allow_in_progress else 'MEASURED_COMPLETE_ATTEMPT_MATRIX',
        invocation=[sys.executable, *sys.argv],
        inputs=dict(replay_result_sha256=hashlib.sha256(result_bytes).hexdigest(),
            population_sha256=POPULATION, ledger_sha256=sha(ledger_path), linked_source_sha256=source_pins,
            wrapper_source_sha256=sha(args.wrapper_source)),
        replay=dict(attempted=len(result['cases']), planned=len(plan['sidecars']),
            unique_start_states=len({c['state'] for c in result['cases']}),
            full_length=sum(c['completed_recording_length'] for c in result['cases']),
            same_to_end=sum(c['parity'] == 'SAME_TO_END' for c in result['cases']),
            diverged_full_length=sum(c['parity'] == 'DIVERGED_FULL_LENGTH' for c in result['cases'])),
        base_wrappers=268, counted_base_wrappers=267, uncounted_base_wrappers=['0020'],
        label_map={f'{label:x}': [f'{unit:04x}' for unit in sorted(candidates)] for label, candidates in sorted(labels.items())},
        withheld_collided_label_observations={f'{label:x}': count for label, count in sorted(ambiguous.items())},
        label_controls=controls, observed_catalogue_entries=len(observed),
        raw_unmapped_entry_observations=raw_unmapped, interior_or_excluded_observations=interior,
        eligible_denominator=1952, observed_eligible=len(intersection),
        observed_eligible_same_to_end=len(eligible & same_to_end.keys()),
        structural_coverage=dict(global_eligible=1952, base_eligible=len(base_eligible),
            overlay_eligible=len(eligible-base_eligible), base_eligible_with_linked_case=len(base_eligible & linked_addresses),
            base_eligible_with_uniquely_recoverable_counter=len(base_eligible & recoverable_addresses)),
        observed_eligible_and_verified_ownership=len(known),
        eligible_rows=[dict(key=k, wrapper_entries=observed[k], ownership=ledger[k]['ownership_status'],
            body_bytes=population[k]['body_bytes'], instruction_mix=population[k]['instruction_mix']) for k in intersection],
        already_owned_eligible=known,
        static_shape_exclusions=dict(hard.most_common()),
        unresolved_structural_findings=dict(unresolved.most_common()),
        direct_GP_relative_functions=gp,
        csv=dict(path='executed_base_entries.csv', sha256=sha(output / 'executed_base_entries.csv')),
        selected_per_sidecar=[dict(sidecar=c['sidecar'], parity=c['parity'], observed_end=c['observed_end'],
            completed=c['completed'], hit_counts={k: c['function_entries'].get(k, 0) for k in (
                'SLUS_214.10:00162e70', 'SLUS_214.10:002e60e0', 'SLUS_214.10:00194370',
                *('SLUS_214.10:' + p for p in ('001a6410', '001a6510', '001a6540', '001a65a0',
                '001a66a0', '001a68f0', '001a6a10', '001a6a20', '001a6b30', '001a6be0',
                '001a6c30', '001a6d70', '001a6dd0', '001a6f40', '001a6fd0')))}) for c in by_case],
        credit_delta=0,
        limits=['Base wrapper observations only: unit0020 has no counter; overlay DLLs and internal same-unit paths may escape observation; zero counts do not prove unreachability.',
            'Autogenerated counter labels consumed a mangling separator E. This join derives opaque labels from authenticated wrapper symbols; every collided label is withheld without allocating its counts.',
            'Retained source of the linked base archive supplies the case map; selected captures separately authenticate original callers and code identity.',
            'The1952 population is structural capability only. Neither observations nor ownership alone provide whole-function or production admission.',
            'Structural hard-exclusion/GP counts overlap and are not compiler refusal histograms or predicted single-capability unlock counts.'])
    (output / 'RESULT.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf8', newline='\n')
    print(json.dumps({k: summary[k] for k in ('status', 'replay', 'observed_catalogue_entries', 'observed_eligible',
        'observed_eligible_and_verified_ownership', 'already_owned_eligible', 'static_shape_exclusions',
        'unresolved_structural_findings', 'direct_GP_relative_functions')}))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
