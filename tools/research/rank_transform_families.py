"""M4/M5 evidence ranking only: no lowering, binding or contract discharge.

Conditional first-barrier sets, measured probe advancement, and verified whole
units are separate denominators. No instruction is skipped to model success.
"""
from __future__ import annotations
from collections import Counter
import argparse
import copy
import hashlib
import itertools
import json
from pathlib import Path
import re
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from classify_stack_call_boundaries import reader

L6_SHA = '93760a5dcaf4e955e20814e6c173cc7ac7cd4e4312142ef5d9f192f88baa5b1a'
FAMILY = {
 'GPR_value_assignment_or_livein_SSA': 'GPR_copies_and_scalar_SSA',
 'unbound_GPR_livein_in_existing_integer_expression': 'typed_input_and_owner_bindings',
 'typed_memory_load_expression_value_SSA': 'typed_input_and_owner_bindings',
 'typed_GP_load_expression_and_owner': 'typed_input_and_owner_bindings',
 'non_GP_receiver_binding_or_existing_contract_selection': 'typed_input_and_owner_bindings',
 'unbound_GP_owner_or_field_type': 'typed_input_and_owner_bindings',
 'unbound_absolute_source_data_symbol': 'typed_input_and_owner_bindings',
 'FPR_typed_memory_load': 'FPR_moves_loads_and_frame_bits',
 'FPR_stack_bits_and_native_frame_proof': 'FPR_moves_loads_and_frame_bits',
 'FPR_bit_transfer_from_GPR': 'FPR_moves_loads_and_frame_bits',
 'FPR_value_assignment_or_state_effect': 'FPR_moves_loads_and_frame_bits',
 'FPR_arithmetic_or_state_effect': 'FPR_arithmetic_semantics',
 'native_call_RA_delay_return_ABI': 'complete_call_and_control_bundles',
 'branch_condition_or_return_capture_contract': 'complete_call_and_control_bundles',
 'conditional_control_and_effect_contract': 'complete_call_and_control_bundles',
 'source_cursor_state_control_contract': 'complete_call_and_control_bundles',
 'missing_generated_entry_or_boundary': 'entry_context_recovery'}


def sha(raw): return hashlib.sha256(raw).hexdigest()


def scc_schedule(nodes, edges):
    """Tarjan SCCs, followed by dependency-first layers; no contract is assumed."""
    index = {}; low = {}; stack = []; active = set(); groups = []
    def visit(v):
        index[v] = low[v] = len(index); stack.append(v); active.add(v)
        for w in sorted(edges.get(v, set()) & nodes):
            if w not in index: visit(w); low[v] = min(low[v], low[w])
            elif w in active: low[v] = min(low[v], index[w])
        if low[v] == index[v]:
            component = []
            while True:
                w = stack.pop(); active.remove(w); component.append(w)
                if w == v: break
            groups.append(sorted(component))
    for v in sorted(nodes):
        if v not in index: visit(v)
    owner = {v: i for i, g in enumerate(groups) for v in g}; layers = {}
    def layer(i):
        if i not in layers:
            deps = {owner[w] for v in groups[i] for w in edges.get(v, set()) & nodes if owner[w] != i}
            layers[i] = 0 if not deps else 1 + max(layer(d) for d in deps)
        return layers[i]
    return [{'members': g, 'dependency_first_layer': layer(i),
             'cyclic': len(g) > 1 or any(v in edges.get(v, set()) for v in g)} for i, g in enumerate(groups)]


def m5_plan(inventory):
    sites, callees = inventory['sites'], inventory['direct_callees']
    if len(sites) != 101 or len({s['pc'] for s in sites}) != 101 or len(callees) != 52:
        raise ValueError('M5 requires all101 sites and52 direct targets')
    by = {c['entry']: c for c in callees}
    largest = {c['entry'] for c in callees if c['exclusive_group'] == 'DIRECT_CLOSED_NONLEAF'}
    largest_sites = [s for s in sites if s['exclusive_group'] == 'DIRECT_CLOSED_NONLEAF']
    if len(largest) != 32 or len(largest_sites) != 45: raise ValueError('largest exclusive class changed')
    graph = {c['entry']: {x['target'] for x in c['local_CFG'].get('call_boundaries', []) if x['target']}
             for c in callees}
    components = scc_schedule(largest, graph)
    schedule = []
    for component in components:
        members = set(component['members'])
        dependencies = sorted(set.union(*(graph[v] for v in members)) - members)
        indirect = [{'callee': v, **c} for v in sorted(members)
                    for c in by[v]['local_CFG']['call_boundaries'] if c['kind'] == 'indirect_call']
        schedule.append({**component,
            'selected_call_sites': sorted(s['pc'] for s in largest_sites if s['target'] in members),
            'direct_dependencies': [{'target': d,
                'inventory_class': by[d]['exclusive_group'] if d in by else 'OUTSIDE_52_TARGET_INVENTORY_UNPROVED',
                'SP_return_contract': 'UNPROVED'} for d in dependencies],
            'unresolved_indirect_dependencies': indirect,
            'work_order': ['Authenticate actual caller inputs, result uses and operand widths for each edge.',
                'Prove complete original call/delay bundle ordering, RA/link identity and SP effects.',
                'Classify suspension/nonlocal control transfer from actual producer/consumer evidence; do not assume ordinary return.',
                'Discharge external, indirect and context obligations before composing this SCC; recursion requires a joint invariant.',
                'Only then propose typed native calls/continuations and independent per-function E2 including errors and lifetime controls.'],
            'contracts_discharged': 0})
    shape = {'DIRECT_CLOSED_NONLEAF': 'normal_return_candidate', 'DIRECT_CLOSED_LEAF': 'leaf_candidate',
             'DIRECT_OUT_OF_BODY_EXIT': 'potential_out_of_body_jump_NOT_admitted_tail',
             'INDIRECT_TARGET_UNRESOLVED': 'indirect_target_unknown',
             'DIRECT_CONTEXT_RESTORE_CONTRACT_UNPROVED': 'context_restore_excluded_from_ordinary_return'}
    return {'site_denominator': 101, 'direct_site_count': 89, 'unique_direct_targets': 52,
            'indirect_site_count': 12,
            'exclusive_machine_shapes': {name: {'count': sum(shape[s['exclusive_group']] == name for s in sites),
                'site_PCs': sorted(s['pc'] for s in sites if shape[s['exclusive_group']] == name)} for name in shape.values()},
            'largest_exclusive_class': {'sites': 45, 'direct_targets': 32, 'target_addresses': sorted(largest)},
            'largest_class_graph_denominators': {
                'SCCs': len(schedule), 'cyclic_SCCs': sum(x['cyclic'] for x in schedule),
                'SCCs_per_dependency_layer': dict(Counter(x['dependency_first_layer'] for x in schedule)),
                'external_unique_direct_targets': len({d['target'] for x in schedule for d in x['direct_dependencies']
                    if d['inventory_class'] == 'OUTSIDE_52_TARGET_INVENTORY_UNPROVED'}),
                'nested_indirect_sites': len({c['pc'] for x in schedule for c in x['unresolved_indirect_dependencies']})},
            'largest_class_SCC_schedule': sorted(schedule, key=lambda x: (x['dependency_first_layer'], x['members'])),
            'complete_dependency_graph_claimed': False,
            'limits': 'The 52 direct targets are an inventory subset, not a complete graph. Layers/SCCs use bounded original local CFGs under unproved call-return summaries; topology provides no ABI/SP proof. External, indirect, potential-tail and context-return obligations remain explicit. Machine JR RA is not a native return ABI.',
            'selected_sites_all101': [{**{k: s[k] for k in ('pc', 'caller_entry', 'target', 'kind', 'exclusive_group',
                'machine_call_signature', 'ordinary_SP_preserving_PC_plus8_contract')},
                'pinned_L6_evidence_pointer': '/sites/' + str(i)} for i, s in enumerate(sites)],
            'direct_callees_all52': [{**{k: c[k] for k in ('entry', 'exclusive_group', 'overlapping_traits',
                'machine_return_convention', 'SP_preservation_contract')},
                'pinned_L6_evidence_pointer': '/direct_callees/' + str(i)} for i, c in enumerate(callees)],
            'contracts_discharged': 0}


def validate_probe(row, source, original):
    """Join the fatal statement and bundle, not the diagnostic string alone."""
    cause = row.get('first_failure_cause')
    if not cause: raise ValueError('missing first-failure context')
    category = cause['observed_semantic_category']
    if category not in FAMILY: raise ValueError('unrecognized family cause: ' + category)
    if category == 'missing_generated_entry_or_boundary':
        if row.get('first_unlowered_pc') is not None or row.get('entry_authenticated'):
            raise ValueError('forged missing-entry context')
        return {'address': row['address'], 'family': FAMILY[category], 'required_capabilities': ['entry_context'],
                'instruction_coverage_eligible': False,
                'category': category, 'source_context': 'No generated entry; no body or instruction advancement inferred'}
    if source is None or sha(source) != row['source_unit_sha256']: raise ValueError('source-unit identity mismatch')
    entry_case = rf'case {row["entry_case"]}u: goto L_{int(row["address"], 16):08X};'
    if entry_case.encode() not in source: raise ValueError('selected address/entry-case row swap')
    pc = int(row['first_unlowered_pc'], 16); bundle = cause.get('original_bundle_words')
    if not bundle or int(bundle[0]['pc'], 16) != pc: raise ValueError('row/PC bundle swap')
    for item in bundle:
        if int.from_bytes(original(int(item['pc'], 16), 4), 'little') != int(item['word'], 16):
            raise ValueError('original word mismatch')
    text = source.decode(); marker = f'L_{pc:08X}:'
    if marker not in text: raise ValueError('missing original PC generated label')
    fragment = text.split(marker, 1)[1].split('\nL_', 1)[0]
    if fragment != cause['generated_fragment_at_failure']: raise ValueError('failure source fragment mismatch')
    statement = cause.get('actual_statement'); ast = cause.get('actual_AST')
    if statement is None or ast is None: raise ValueError('missing fatal AST/source context')
    a, b = statement['start_byte'], statement['end_byte_exclusive']
    if source[a:b].decode() != statement['text']: raise ValueError('fatal AST byte range mismatch')
    # The statement must be inside this actual instruction bundle. Thus a row
    # cannot borrow another probe's valid AST text to forge its first family.
    marker_at = source.index(marker.encode()) + len(marker)
    if not marker_at <= a < b <= marker_at + len(fragment.encode()): raise ValueError('AST statement outside failure bundle')
    w = int(bundle[0]['word'], 16); op = w >> 26
    snippet = statement['text']; family = FAMILY[category]
    if family == 'GPR_copies_and_scalar_SSA' and not re.match(r'c\.r\[\d+\]\.(?:d|sd|w|sw)\[\d+\] =', snippet):
        raise ValueError('forged GPR family cause')
    if category == 'FPR_bit_transfer_from_GPR' and not (op == 0x11 and w >> 21 & 31 == 4):
        raise ValueError('forged FPR move cause')
    if category == 'FPR_typed_memory_load' and not (op == 0x31 and 'ld32' in snippet):
        raise ValueError('forged FPR load cause')
    if category == 'FPR_stack_bits_and_native_frame_proof' and not (op == 0x39 and w >> 21 & 31 == 29):
        raise ValueError('forged FPR frame cause')
    if category == 'FPR_arithmetic_or_state_effect' and not ('c.f[' in snippet and 'fp::ee::' in snippet):
        raise ValueError('forged FPR arithmetic cause')
    if category == 'native_call_RA_delay_return_ABI' and op != 3: raise ValueError('forged call family cause')
    caps = set()
    if family == 'GPR_copies_and_scalar_SSA':
        caps.add('GPR_scalar_SSA')
        rhs = snippet.split('=', 1)[1]
        if any(int(r) != 0 for r in re.findall(r'c\.r\[(\d+)\]', rhs)): caps.add('typed_GPR_input_or_dominating_definition')
    elif family == 'typed_input_and_owner_bindings':
        if category == 'unbound_GPR_livein_in_existing_integer_expression':
            if not row['reason'].startswith('unbound_value:register_'): raise ValueError('forged typed GPR cause')
            caps.add('typed_GPR_input_or_dominating_definition')
        else:
            caps.add('typed_memory_owner_and_field')
            if 'ld32' in snippet: caps.add('GPR_scalar_SSA')
            memory = cause.get('original_memory_use')
            if memory is None: raise ValueError('missing actual memory context')
            if memory['original_base_register'] not in (28, 29) and not memory.get('source_absolute_address'):
                caps.add('typed_GPR_input_or_dominating_definition')
            if any(int(r) not in (0, 28, 29) for r in re.findall(r'c\.r\[(\d+)\]', snippet.split(',', 1)[-1])):
                caps.add('typed_GPR_input_or_dominating_definition')
    elif family == 'FPR_moves_loads_and_frame_bits':
        caps.add('FPR_moves_loads_frame_bits')
        if category == 'FPR_typed_memory_load': caps.update(('typed_memory_owner_and_field', 'typed_GPR_input_or_dominating_definition'))
        if category == 'FPR_stack_bits_and_native_frame_proof': caps.add('proved_frame_alias_and_value_types')
        if category == 'FPR_bit_transfer_from_GPR': caps.add('typed_GPR_input_or_dominating_definition')
    elif family == 'FPR_arithmetic_semantics': caps.update(('FPR_arithmetic_semantics', 'typed_FPR_input_or_dominating_definition'))
    else: caps.update(('complete_call_control_bundle', 'typed_GPR_input_or_dominating_definition'))
    if not row.get('entry_authenticated'): caps.add('entry_context')
    return {'address': row['address'], 'first_unlowered_pc': row['first_unlowered_pc'],
            'instruction_coverage_eligible': True,
            'reason': row['reason'], 'category': category, 'family': family,
            'required_capabilities': sorted(caps), 'actual_statement': statement,
            'original_bundle_words': bundle, 'generated_unit': row['generated_unit'],
            'source_unit_sha256': row['source_unit_sha256'],
            'entry_authenticated': row['entry_authenticated'],
            'static_scope': 'Necessary family/dependency plan for this first barrier only; not a proof any capability clears it.'}


def m4_rank(report, source_root, original, before=None):
    samples = report['samples']; cohorts = report['cohorts']
    primary = set(cohorts['original_primary33'])
    extra = set(cohorts['supplemental4']) | set(cohorts['corrected_source_projection2'])
    if len(samples) != 39 or len(primary) != 33 or len(extra) != 6 or primary & extra or {r['address'] for r in samples} != primary | extra:
        raise ValueError('ranking requires exact original33 plus additional6')
    validated = []
    for row in samples:
        source = (source_root / row['generated_unit']).read_bytes() if row.get('generated_unit') else None
        validated.append(validate_probe(row, source, original))
    exemplar = next(r for r in samples if (r.get('first_failure_cause') or {}).get('observed_semantic_category') == 'GPR_value_assignment_or_livein_SSA')
    source = (source_root / exemplar['generated_unit']).read_bytes(); controls = []
    for name in ('row_swap', 'forged_family_cause', 'missing_context'):
        mutant = copy.deepcopy(exemplar)
        if name == 'row_swap': mutant['address'] = next(r['address'] for r in samples if r['address'] != exemplar['address'])
        elif name == 'forged_family_cause': mutant['first_failure_cause']['observed_semantic_category'] = 'FPR_bit_transfer_from_GPR'
        else: mutant['first_failure_cause']['actual_statement'] = None
        try: validate_probe(mutant, source, original)
        except ValueError as error: controls.append({'mutation': name, 'status': 'REJECTED', 'reason': str(error)})
        else: raise AssertionError('ranking validation admitted ' + name)
    families = [{'family': family, 'first_barrier_probe_count': len(found := [r for r in validated if r['family'] == family]),
                 'addresses': sorted(r['address'] for r in found),
                 'primary33': sum(r['address'] in primary for r in found), 'additional6': sum(r['address'] in extra for r in found),
                 'required_dependencies': sorted(set(c for r in found for c in r['required_capabilities'])),
                 'measured_barriers_cleared_by_this_proposal': 0, 'whole_units_unlocked': None}
                for family in sorted({r['family'] for r in validated})]
    families.sort(key=lambda r: (-r['first_barrier_probe_count'], r['family']))
    capabilities = sorted({c for r in validated for c in r['required_capabilities']})
    combinations = [(c,) for c in capabilities] + [('GPR_scalar_SSA', 'typed_GPR_input_or_dominating_definition'),
        ('GPR_scalar_SSA', 'typed_GPR_input_or_dominating_definition', 'typed_memory_owner_and_field'),
        ('FPR_moves_loads_frame_bits', 'typed_memory_owner_and_field', 'typed_GPR_input_or_dominating_definition', 'proved_frame_alias_and_value_types'),
        tuple(capabilities)]
    coverage = []
    for combo in combinations:
        found = sorted(r['address'] for r in validated if r['instruction_coverage_eligible'] and set(r['required_capabilities']) <= set(combo))
        coverage.append({'capabilities': list(combo), 'conditional_first_barrier_addresses': found,
                         'conditional_count_out_of39': len(found), 'experimentally_measured_clearance': None,
                         'whole_units_verified': 0,
                         'scope': 'Required-capability-set containment only. No lowerer run, skipped instruction or successful binding is simulated.'})
    measured = None
    if before:
        prior = {r['address']: r for r in before['samples']}
        if set(prior) != primary | extra: raise ValueError('before/current cohorts differ')
        for key in ('study_tool_sha256', 'transformer_sha256', 'original_elf_sha256'):
            if before['inputs'][key] != report['inputs'][key]: raise ValueError('measured comparison is not same-source: ' + key)
        changed = [r['address'] for r in samples if (r.get('first_unlowered_pc'), r['reason']) !=
                   (prior[r['address']].get('first_unlowered_pc'), prior[r['address']]['reason'])]
        advanced = [r['address'] for r in samples if r.get('first_unlowered_pc') != prior[r['address']].get('first_unlowered_pc')]
        bound = [r['address'] for r in samples if r.get('mtc1_bindings')]
        for row in samples:
            if row['generated_unit'] != prior[row['address']]['generated_unit'] or row.get('source_unit_sha256') != prior[row['address']].get('source_unit_sha256'):
                raise ValueError('before/current generated sample changed')
            for binding in row.get('mtc1_bindings', []):
                word = int(binding['original_word'], 16)
                if int.from_bytes(original(int(binding['source_pc'], 16), 4), 'little') != word or word >> 26 != 0x11 or word >> 21 & 31 != 4:
                    raise ValueError('MTC1 binding is not authenticated original MTC1')
        measured = {'changed_first_frontier_addresses': sorted(changed), 'MTC1_bound_addresses': sorted(bound),
                    'actual_PC_advanced_addresses': sorted(advanced),
                    'actual_bound_and_PC_advanced_addresses': sorted(set(advanced) & set(bound)),
                    'unexpected_nonbound_changes': sorted(set(changed) - set(bound)),
                    'whole_units_verified': report['new_verified_units'],
                    'scope': 'Observed real report comparison; changed diagnostics alone do not establish instruction binding or whole-function success.'}
    return {'denominators': {'primary': 33, 'additional': 6, 'union': 39,
                'existing_instruction_frontiers': sum(r['instruction_coverage_eligible'] for r in validated),
                'missing_generated_entry_not_in_instruction_coverage': sum(not r['instruction_coverage_eligible'] for r in validated),
                'whole_units_verified': report['new_verified_units']},
            'source_validation_mutations': controls,
            'probe_rows': validated, 'first_barrier_families': families, 'standalone_and_combination_coverage': coverage,
            'experimentally_measured_progression': measured,
            'recommendation': 'Next grouped family: GPR scalar/copy SSA together with proved typed GPR inputs or dominating definitions, then owner/field bindings. Syntax support alone is not input ABI proof. Rerun all39 and retain every real next failure; zero whole-unit unlock predicted.',
            'boundary_limits': 'Requested104000/102BB4 continuations and missing103FA0 remain explicit context obligations, never silently replaced.'}


def self_test():
    cyclic = scc_schedule({'a', 'b', 'c'}, {'a': {'b'}, 'b': {'a', 'c'}, 'c': set()})
    assert any(x['members'] == ['a', 'b'] and x['cyclic'] and x['dependency_first_layer'] == 1 for x in cyclic)
    return {'status': 'PASS', 'controls': ['SCC preserves recursion and dependency-first order']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('l6-report', 'original-elf', 'generated-dir', 'current-report', 'before-report', 'historical-LUI-report', 'output'):
        parser.add_argument('--' + key, type=Path)
    parser.add_argument('--self-test', action='store_true'); args = parser.parse_args(); tests = self_test()
    if args.self_test and not args.l6_report: print(json.dumps(tests)); return
    if any(x is None for x in (args.l6_report, args.original_elf, args.output)): parser.error('L6/original/output paths required')
    fork = Path(__file__).resolve().parents[2]; output = args.output.resolve()
    if output.exists() or not output.is_relative_to(fork / 'build/oracle-telemetry'): raise ValueError('new ignored output directory required')
    l6_raw = args.l6_report.read_bytes(); inventory = json.loads(l6_raw)
    if sha(l6_raw) != L6_SHA: raise ValueError('requires the approved exact L6-03 inventory')
    classifier = Path(__file__).with_name('classify_stack_call_boundaries.py')
    if sha(classifier.read_bytes()) != inventory['input_pins']['tool_sha256']: raise ValueError('L6 classifier source mismatch')
    original = reader(args.original_elf.read_bytes()); m5 = m5_plan(inventory)
    m4 = None; pins = {'L6_report_sha256': sha(l6_raw), 'classifier_sha256': sha(classifier.read_bytes()),
                      'tool_sha256': sha(Path(__file__).read_bytes())}
    if args.current_report:
        if args.generated_dir is None: parser.error('M4 needs --generated-dir')
        raw = args.current_report.read_bytes(); current = json.loads(raw)
        before = json.loads(args.before_report.read_bytes()) if args.before_report else None
        m4 = m4_rank(current, args.generated_dir, original, before)
        pins['current_report_sha256'] = sha(raw)
        if args.before_report: pins['before_report_sha256'] = sha(args.before_report.read_bytes())
        if args.historical_LUI_report: pins['historical_LUI_report_sha256'] = sha(args.historical_LUI_report.read_bytes())
    report = {'schema': 'transform_family_and_call_contract_plan_v1', 'status': 'PROPOSALS_ONLY_NO_DISCHARGE',
              'input_pins': pins, 'self_tests': tests, 'M5': m5, 'M4': m4,
              'runtime_changes': False, 'ownership_changes': False, 'native_credit_added': 0}
    output.mkdir(); path = output / 'RESULT.json'
    path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'receipt': str(path), 'sha256': sha(path.read_bytes()),
                      'M5_largest_class': m5['largest_exclusive_class'],
                      'M4_families': m4['first_barrier_families'] if m4 else None}))


if __name__ == '__main__': main()
