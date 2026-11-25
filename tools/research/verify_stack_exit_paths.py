"""Independent, offline original-word audit of local stack exit geometry.

This is an SP-only abstract CFG check, not an EE interpreter or native proof.
Calls have explicit, unproved SP-preserving return summaries. No call, tail,
unknown edge, or recovered frame extent can grant unconditional admission.
"""
from __future__ import annotations
from collections import Counter, deque
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys

ELF_SHA = 'a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f'


def sha(data): return hashlib.sha256(data).hexdigest()
def hx(value): return f'{value:08x}'
def imm(word): return (word & 65535) - (65536 if word & 32768 else 0)


def decode(pc, word):
    """Local control edges only; no concrete register or instruction execution."""
    op, rs, rt, fn = word >> 26, word >> 21 & 31, word >> 16 & 31, word & 63
    if op in (2, 3):
        return {'kind': 'call' if op == 3 else 'jump',
                'target': ((pc + 4) & 0xf0000000) | ((word & 0x3ffffff) << 2)}
    if op == 0 and fn in (8, 9):
        return {'kind': 'return' if fn == 8 and rs == 31 else 'indirect_jump' if fn == 8 else 'indirect_call',
                'register': rs, 'link_register': word >> 11 & 31}
    if op == 0 and fn in {0x0c, 0x0d, 0x30, 0x31, 0x32, 0x33, 0x34, 0x36}:
        return {'kind': 'unresolved', 'reason': 'explicit_trap_or_conditional_trap'}
    likely = False
    fixed = None
    if op in {4, 5, 6, 7, 0x14, 0x15, 0x16, 0x17}:
        likely = op >= 0x14
        basic = op - 0x10 if likely else op
        if basic in (4, 5) and rs == rt: fixed = basic == 4
        if basic in (6, 7) and rs == 0: fixed = basic == 6
    elif op == 1:
        if rt not in {0, 1, 2, 3, 16, 17, 18, 19}:
            return {'kind': 'unresolved', 'reason': 'REGIMM_trap_or_reserved_control'}
        likely = rt in {2, 3, 18, 19}
        if rs == 0: fixed = bool(rt & 1)
        if rt >= 16:
            return {'kind': 'unresolved', 'reason': 'REGIMM_link_control_not_summarized'}
    elif op in (0x10, 0x11, 0x12) and rs == 8:
        if rt > 3: return {'kind': 'unresolved', 'reason': 'COP_branch_condition_not_supported'}
        likely = bool(rt & 2)
    elif op == 0x10 and rs == 16:
        return {'kind': 'unresolved', 'reason': 'privileged_COP0_control'}
    else:
        # Explicit instruction-family whitelist; unknown words cannot silently
        # become fallthrough if a future cohort contains another control kind.
        known = {0, 8, 9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x10, 0x11, 0x12,
                 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1e, 0x1f,
                 *range(0x20, 0x30), 0x30, 0x31, 0x32, 0x34, 0x35, 0x36,
                 0x37, 0x38, 0x39, 0x3a, 0x3c, 0x3d, 0x3e, 0x3f}
        return None if op in known else {'kind': 'unresolved', 'reason': 'unknown_instruction_family'}
    return {'kind': 'branch', 'target': pc + 4 + (imm(word) << 2),
            'likely': likely, 'fixed_condition': fixed}


def sp_transfer(word, offset):
    """Only constant SP deltas are proved. Other SP definitions are unknown."""
    op, rs, rt, rd, fn = word >> 26, word >> 21 & 31, word >> 16 & 31, word >> 11 & 31, word & 63
    if op in (9, 0x19) and rt == 29:
        return offset + imm(word) if rs == 29 and offset is not None else None
    if op in {8, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, 0x18,
              0x1a, 0x1b, 0x1e, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
              0x30, 0x34, 0x37, 0x38, 0x3c} and rt == 29:
        return None
    if op in (0x10, 0x11, 0x12) and rs in (0, 1, 2) and rt == 29:
        return None
    if op == 0x1c and rd == 29: return None
    # GPR-producing SPECIAL instructions. Stores to F29 and loads into F29
    # are deliberately absent: FPR numbers are not GPR destinations.
    if op == 0 and rd == 29:
        if fn in (0x21, 0x2d, 0x25) and ((rs == 29 and rt == 0) or (rs == 0 and rt == 29)):
            return offset
        if fn not in {8, 0x0c, 0x0d, 0x0f, 0x11, 0x13, 0x18, 0x19, 0x1a, 0x1b,
                      0x30, 0x31, 0x32, 0x33, 0x34, 0x36}:
            return None
    return offset


def audit_words(words, entry, *, bounded_extent=False):
    """Explore every represented local branch, including unexecuted alternatives."""
    pending = deque([(entry, 0)])
    seen = set()
    edges, returns, unknown, calls, adjustments = set(), set(), set(), set(), set()
    visited = set()
    while pending:
        pc, offset = pending.popleft()
        if (pc, offset) in seen: continue
        if len(seen) >= 20000 or offset is not None and abs(offset) > 65536:
            unknown.add((pc, None, 'abstract_state_or_SP_bound_exceeded', offset)); continue
        seen.add((pc, offset))
        if pc not in words:
            unknown.add((pc, None, 'entry_or_fallthrough_outside_authenticated_body', offset)); continue
        visited.add(pc)
        w = words[pc]
        transfer = decode(pc, w)
        after = sp_transfer(w, offset)
        if after != offset: adjustments.add((pc, offset, after))
        if after is None:
            unknown.add((pc, None, 'dynamic_or_unknown_SP_definition', after))

        def enqueue(target, value, policy):
            edges.add((pc, target, policy, value))
            if target not in words:
                unknown.add((pc, target, 'CFG_gap_or_tail_outside_authenticated_body', value))
            else: pending.append((target, value))

        if transfer is None:
            enqueue(pc + 4, after, 'no_delay'); continue
        kind = transfer['kind']
        if kind == 'unresolved':
            unknown.add((pc, None, transfer['reason'], after)); continue
        if kind == 'branch' and transfer['likely'] and transfer['fixed_condition'] is False:
            enqueue(pc + 8, after, 'not_taken_delay_annulled'); continue
        delay = pc + 4
        if delay not in words:
            unknown.add((pc, delay, 'missing_delay_instruction', after))
            if kind == 'branch' and transfer['likely'] and transfer['fixed_condition'] is None:
                enqueue(pc + 8, after, 'not_taken_delay_annulled')
            continue
        if decode(delay, words[delay]) is not None:
            unknown.add((pc, delay, 'control_in_delay_slot', after))
            if kind == 'branch' and transfer['likely'] and transfer['fixed_condition'] is None:
                enqueue(pc + 8, after, 'not_taken_delay_annulled')
            continue
        delayed = sp_transfer(words[delay], after)
        visited.add(delay)
        if delayed != after: adjustments.add((delay, after, delayed))
        if delayed is None: unknown.add((delay, None, 'dynamic_or_unknown_delay_SP_definition', delayed))
        if kind == 'return':
            returns.add((pc, delay, offset, delayed))
        elif kind == 'indirect_jump':
            unknown.add((pc, None, 'unresolved_indirect_exit', delayed))
        elif kind in ('call', 'indirect_call'):
            calls.add((pc, kind, transfer.get('target'), transfer.get('register'), delayed))
            enqueue(pc + 8, delayed, 'SP_preserving_callee_return_summary_UNPROVED')
        elif kind == 'jump':
            enqueue(transfer['target'], delayed, 'delay_executed')
        else:
            fixed = transfer['fixed_condition']
            if fixed is not False: enqueue(transfer['target'], delayed, 'taken_delay_executed')
            if fixed is not True:
                enqueue(pc + 8, after if transfer['likely'] else delayed,
                        'not_taken_delay_annulled' if transfer['likely'] else 'not_taken_delay_executed')
    order = lambda rows: sorted(rows, key=lambda x: repr(x))
    normal = [{'pc': hx(p), 'word': hx(words[p]), 'delay_pc': hx(d), 'delay_word': hx(words[d]),
               'SP_before_JR': a, 'SP_after_delay': b, 'status': 'BALANCED' if b == 0 else 'UNPROVED_UNBALANCED_SP'}
              for p, d, a, b in order(returns)]
    blockers = sorted({reason for _, _, reason, _ in unknown})
    if not normal: blockers.append('no_reachable_normal_return')
    if any(x['SP_after_delay'] != 0 for x in normal): blockers.append('normal_return_SP_not_zero')
    local = 'PROVED_LOCAL_EXIT_GEOMETRY' if not blockers else 'UNPROVED'
    unconditional = list(blockers)
    if calls: unconditional.append('callee_SP_preservation_not_proved')
    if bounded_extent: unconditional.append('bounded_frame_extent_not_catalogue_function')
    return {'local_exit_status': local,
            'unconditional_status': 'UNPROVED' if unconditional else 'PROVED_SP_GEOMETRY_ONLY',
            'unconditional_blockers': sorted(set(unconditional)), 'local_blockers': sorted(set(blockers)),
            'normal_returns': normal,
            'unresolved_edges': [{'source_pc': hx(p), 'source_word': hx(words[p]) if p in words else None,
                                  'target_pc': hx(t) if t is not None else None, 'reason': reason, 'SP_offset': s}
                                 for p, t, reason, s in order(unknown)],
            'call_boundaries': [{'pc': hx(p), 'word': hx(words[p]), 'kind': k,
                                 'target': hx(t) if t is not None else None, 'target_register': r,
                                 'SP_after_delay': s, 'return_summary': 'SP_PRESERVING_UNPROVED'}
                                for p, k, t, r, s in order(calls)],
            'edges': [{'source_pc': hx(p), 'source_word': hx(words[p]), 'target_pc': hx(t),
                       'delay_pc': hx(p + 4) if d != 'no_delay' else None,
                       'delay_word': hx(words[p + 4]) if d != 'no_delay' and p + 4 in words else None,
                       'delay_policy': d, 'SP_offset': s}
                      for p, t, d, s in order(edges)],
            'SP_transfers': [{'pc': hx(p), 'before': a, 'after': b} for p, a, b in order(adjustments)],
            'reachable_original_pcs': [hx(p) for p in sorted(visited)],
            'native_admission': 'REFUSED; this verifier cannot authorize native emission', 'native_credit': 0}


def local_gate(rows):
    """Ordinary CLI success means all28 local graphs passed, not just ran."""
    passed = (len(rows) == 28 and len({x['source_entry'] for x in rows}) == 28
              and all(x['local_exit_status'] == 'PROVED_LOCAL_EXIT_GEOMETRY'
                      and not x['unresolved_edges'] for x in rows))
    return ('COMPLETED_LOCAL_CFG_AUDIT_WITH_UNCONDITIONAL_LIMITS', 0) if passed else ('UNPROVED_LOCAL_EXIT_GEOMETRY', 2)


def self_test():
    base = {0x1000: 0x27bdfff0, 0x1004: 0x10800004, 0x1008: 0,
            0x100c: 0x03e00008, 0x1010: 0x27bd0010,
            0x1018: 0x03e00008, 0x101c: 0x27bd0010}
    good = audit_words(base, 0x1000)
    assert good['local_exit_status'] == 'PROVED_LOCAL_EXIT_GEOMETRY' and len(good['normal_returns']) == 2
    controls = []
    for label, at, value, expected in (
        ('alternate_exit_without_SP_restore', 0x101c, 0, 'normal_return_SP_not_zero'),
        ('tail_or_unresolved_edge', 0x1018, 0x08000800, 'CFG_gap_or_tail_outside_authenticated_body'),
        ('wrong_JR_delay_SP_restore', 0x1010, 0x27bd0008, 'normal_return_SP_not_zero')):
        planted = dict(base); planted[at] = value
        result = audit_words(planted, 0x1000)
        assert result['local_exit_status'] == 'UNPROVED' and expected in result['local_blockers'], label
        controls.append({'mutation': label, 'status': 'REJECTED', 'required_reason': expected})
    # BEQL's not-taken path must annul -16 in its delay, whereas its taken
    # path executes it. Two returns balance only with the correct policy.
    likely = {0x2000: 0x50800005, 0x2004: 0x27bdfff0,
              0x2008: 0x03e00008, 0x200c: 0,
              0x2018: 0x03e00008, 0x201c: 0x27bd0010}
    result = audit_words(likely, 0x2000)
    assert result['local_exit_status'] == 'PROVED_LOCAL_EXIT_GEOMETRY'
    assert {x['SP_before_JR'] for x in result['normal_returns']} == {-16, 0}
    assert any(e['delay_policy'] == 'not_taken_delay_annulled' for e in result['edges'])
    fixed_annul = {0x3000: 0x54000005, 0x3004: 0x8fbd0000,
                   0x3008: 0x03e00008, 0x300c: 0}
    result = audit_words(fixed_annul, 0x3000)
    assert result['local_exit_status'] == 'PROVED_LOCAL_EXIT_GEOMETRY'
    assert '00003004' not in result['reachable_original_pcs']
    assert decode(0x4000, 0x45020005)['likely']  # BC1FL annuls on fallthrough.
    # SWC1/LWC1 F29 are not SP writes; an actual LW SP is unknown.
    assert sp_transfer(0xe7bd0024, -16) == -16
    assert sp_transfer(0xc7bd0024, -16) == -16
    assert sp_transfer(0x8fbd0000, -16) is None
    rows = [dict(good, source_entry=hx(n)) for n in range(28)]
    assert local_gate(rows)[1] == 0
    rows[-1] = dict(audit_words({0x5000: 0x27bdfff0}, 0x5000), source_entry=hx(27))
    assert local_gate(rows) == ('UNPROVED_LOCAL_EXIT_GEOMETRY', 2)
    return {'status': 'PASS', 'negative_controls': controls,
            'positive_controls': ['two_alternate_balanced_returns', 'branch_likely_annulment',
                                  'fixed_false_likely_annuls_unknown_SP_write', 'COP1_likely_decode', 'FPR29_is_not_SP'],
            'ordinary_result_gate': 'Unsafe nonplant local CFG returns UNPROVED_LOCAL_EXIT_GEOMETRY/exit2'}


def authenticate(analysis, cohort, image):
    if sha(image) != ELF_SHA or analysis['original_elf_sha256'] != ELF_SHA:
        raise ValueError('original ELF identity changed')
    if sha(cohort) != analysis['cohort_sha256']: raise ValueError('original39 cohort identity changed')
    samples = json.loads(cohort)['samples']
    targets = {x['address'].lower() for x in samples if x['reason'] == 'unbound_value:register_29'}
    if len(samples) != 39 or len(targets) != 28 or targets != {t['source_entry'] for t in analysis['targets']}:
        raise ValueError('exact original39/stack-first28 cohort required')
    ph = struct.unpack_from('<I', image, 28)[0]
    stride, count = struct.unpack_from('<HH', image, 42)
    segments = [struct.unpack_from('<8I', image, ph + i * stride) for i in range(count)]
    def read(start, length):
        matches = [image[off + start - va: off + start - va + length]
                   for kind, off, va, _, size, *_ in segments
                   if kind == 1 and va <= start and start + length <= va + size]
        if len(matches) != 1 or len(matches[0]) != length: raise ValueError('original span not unique/file-backed')
        return matches[0]
    for target in analysis['targets']:
        words = {}
        for span in target['body_spans']:
            start, end = int(span['start'], 16), int(span['end'], 16)
            if start & 3 or (end - start + 1) % 4: raise ValueError('original body alignment')
            raw = read(start, end - start + 1)
            if sha(raw) != span['sha256']: raise ValueError('original body hash mismatch')
            for n in range(0, len(raw), 4):
                at = start + n
                if at in words: raise ValueError('overlapping original body spans')
                words[at] = int.from_bytes(raw[n:n+4], 'little')
        claimed = {int(x['pc'], 16): int(x['word'], 16) for x in target['original_words']}
        if words != claimed or len(claimed) != len(target['original_words']):
            raise ValueError('serialized words differ from authenticated original body')
        yield target, words


def ordinary_cli_regression(args, analysis):
    """Run the ordinary CLI against a truncated but byte-authenticated map."""
    paths = [args.analysis, args.cohort, args.original_elf, Path(__file__)]
    before = {str(p): sha(p.read_bytes()) for p in paths}
    authenticated = list(authenticate(analysis, args.cohort.read_bytes(), args.original_elf.read_bytes()))
    target, words = next((t, w) for t, w in authenticated if t['source_entry'] == '00100320')
    selected = [0x100320, 0x100324, 0x100328]
    raw = b''.join(words[p].to_bytes(4, 'little') for p in selected)
    fixture = json.loads(json.dumps(analysis))
    row = next(t for t in fixture['targets'] if t['source_entry'] == target['source_entry'])
    row['body_spans'] = [{'start': hx(selected[0]), 'end': hx(selected[-1] + 3), 'sha256': sha(raw)}]
    row['original_words'] = [{'pc': hx(p), 'word': hx(words[p])} for p in selected]
    row['extent_provenance'] = 'TEST_FIXTURE_truncated_authenticated_original_map'
    fixture['test_fixture'] = 'Original instructions unchanged; truncated body map omits all exits. Not an analyzer-produced plan.'
    output = args.output.resolve(); output.mkdir()
    fixture_path = output / 'unsafe-original-frame-map.json'
    fixture_path.write_text(json.dumps(fixture, indent=2) + '\n', encoding='utf-8', newline='\n')
    command = [sys.executable, '-B', str(Path(__file__).resolve()), '--analysis', str(fixture_path),
               '--cohort', str(args.cohort.resolve()), '--original-elf', str(args.original_elf.resolve()),
               '--output', str(output / 'ordinary-cli-audit')]
    run = subprocess.run(command, capture_output=True, timeout=30)
    log = run.stdout + run.stderr
    (output / 'ordinary-cli.log').write_bytes(log)
    child_path = output / 'ordinary-cli-audit/RESULT.json'
    child = json.loads(child_path.read_bytes())
    assert run.returncode == 2 and child['status'] == 'UNPROVED_LOCAL_EXIT_GEOMETRY'
    assert child['planted_mutation'] is None
    assert child['summary']['local_status_counts'] == {'UNPROVED': 1, 'PROVED_LOCAL_EXIT_GEOMETRY': 27}
    assert before == {str(p): sha(p.read_bytes()) for p in paths}
    result = {'schema': 'ordinary_stack_exit_cli_gate_regression_v1', 'status': 'PASS',
              'scope': 'Ordinary CLI, without planted flags, refuses an unsafe original-byte-authenticated frame map.',
              'fixture': str(fixture_path), 'fixture_sha256': sha(fixture_path.read_bytes()),
              'input_sha256': before, 'argv': command, 'exit': run.returncode,
              'child_status': child['status'], 'child_receipt_sha256': sha(child_path.read_bytes()),
              'log_sha256': sha(log), 'native_credit': 0}
    path = output / 'RESULT.json'
    path.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'receipt': str(path), 'sha256': sha(path.read_bytes()), 'status': 'PASS', 'ordinary_cli_exit': run.returncode}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--analysis', type=Path)
    parser.add_argument('--cohort', type=Path)
    parser.add_argument('--original-elf', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--self-test', action='store_true')
    parser.add_argument('--test-ordinary-cli-gate', action='store_true')
    plants = parser.add_mutually_exclusive_group()
    plants.add_argument('--planted-alternate-exit', action='store_true')
    plants.add_argument('--planted-tail-edge', action='store_true')
    plants.add_argument('--planted-delay-sp', action='store_true')
    args = parser.parse_args()
    tests = self_test()
    if args.self_test and not args.analysis:
        print(json.dumps(tests)); return
    if any(x is None for x in (args.analysis, args.cohort, args.original_elf, args.output)):
        parser.error('--analysis/--cohort/--original-elf/--output required for original audit')
    fork = Path(__file__).resolve().parents[2]
    output = args.output.resolve()
    if output.exists() or not output.is_relative_to(fork / 'build/oracle-telemetry'):
        raise ValueError('output requires a new ignored telemetry directory')
    raw = args.analysis.read_bytes(); analysis = json.loads(raw)
    if analysis['schema'] != 'generated_stack_frame_analysis_v1': raise ValueError('unsupported analysis schema')
    analyzer = fork / 'tools/research/stack_frame_analysis.py'
    if sha(analyzer.read_bytes()) != analysis['analyzer_sha256']: raise ValueError('analyzer source pin changed')
    if args.test_ordinary_cli_gate:
        if args.planted_alternate_exit or args.planted_tail_edge or args.planted_delay_sp:
            parser.error('ordinary CLI gate regression cannot use planted controls')
        ordinary_cli_regression(args, analysis); return
    rows = []; mutation = None
    for target, words in authenticate(analysis, args.cohort.read_bytes(), args.original_elf.read_bytes()):
        if target['source_entry'] == '00100320':
            if args.planted_alternate_exit:
                mutation = {'kind': 'alternate_exit_without_SP_restore', 'pc': '00100334',
                            'original_word': '0c048b0c', 'mutant_word': '03e00008',
                            'required_reason': 'normal_return_SP_not_zero'}
            elif args.planted_tail_edge:
                mutation = {'kind': 'tail_or_unresolved_edge', 'pc': '00100334',
                            'original_word': '0c048b0c', 'mutant_word': '08200000',
                            'required_reason': 'CFG_gap_or_tail_outside_authenticated_body'}
            elif args.planted_delay_sp:
                mutation = {'kind': 'wrong_JR_delay_SP_restore', 'pc': '00100368',
                            'original_word': '27bd0010', 'mutant_word': '27bd0008',
                            'required_reason': 'normal_return_SP_not_zero'}
            if mutation:
                at = int(mutation['pc'], 16)
                if words[at] != int(mutation['original_word'], 16): raise ValueError('mutation original word pin changed')
                words = dict(words); words[at] = int(mutation['mutant_word'], 16)
                mutation.update(source_entry='00100320', scope='Private abstract word-map mutation after original input authentication; original files untouched')
        row = audit_words(words, int(target['source_entry'], 16),
                          bounded_extent=target.get('extent_provenance') == 'bounded_frame_CFG_not_catalogue_function')
        row.update(source_entry=target['source_entry'], body_spans=target['body_spans'],
                   extent_provenance=target.get('extent_provenance', 'catalogue_body_spans'),
                   original_analysis_native_admission=target['native_admission'])
        rows.append(row)
    rejected = False
    if mutation:
        row = next(x for x in rows if x['source_entry'] == mutation['source_entry'])
        rejected = row['local_exit_status'] == 'UNPROVED' and mutation['required_reason'] in row['local_blockers']
        mutation['status'] = 'REJECTED' if rejected else 'FAIL_NOT_REJECTED'
    status, exit_code = local_gate(rows)
    if mutation:
        status, exit_code = ('REJECTED_PLANTED_EXIT_PATH', 1) if rejected else ('FAIL_PLANTED_NOT_REJECTED', 2)
    result = {'schema': 'independent_original_stack_exit_paths_v1',
              'status': status,
              'scope': 'Every reachable represented local normal-control CFG exit; SP offsets only. Calls are conditional SP-preserving return summaries, never authenticated callee behavior. Exceptions, whole-function equivalence, ownership and native admission are not proved.',
              'SP_domain': 'Symbolic byte displacement of the SP address, with ADDIU address arithmetic interpreted modulo32. Zero proves address geometry only. Exact low64 identity after ADDIU requires an initially canonical sign-extended32-bit SP; this static audit does not establish that input precondition or full128 CPU identity. Bounds, lifetimes and termination are not proved.',
              'snapshot_check_limit': '3416 executed snapshot-seeded entry/exit checks cover only their executed boundary paths, not every static exit path.',
              'original_elf_sha256': ELF_SHA, 'analysis_sha256': sha(raw),
              'cohort_sha256': analysis['cohort_sha256'], 'core_analyzer_sha256': analysis['analyzer_sha256'],
              'verifier_sha256': sha(Path(__file__).read_bytes()), 'self_tests': tests, 'planted_mutation': mutation,
              'summary': {'targets': len(rows), 'local_status_counts': dict(Counter(x['local_exit_status'] for x in rows)),
                          'unconditional_status_counts': dict(Counter(x['unconditional_status'] for x in rows)),
                          'normal_return_observations': sum(len(x['normal_returns']) for x in rows),
                          'unresolved_edges': sum(len(x['unresolved_edges']) for x in rows),
                          'native_credit': 0}, 'targets': rows}
    output.mkdir()
    path = output / 'RESULT.json'
    path.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'receipt': str(path), 'sha256': sha(path.read_bytes()), 'status': status,
                      'exit_code': exit_code, 'summary': result['summary']}))
    if exit_code: raise SystemExit(exit_code)


if __name__ == '__main__': main()
