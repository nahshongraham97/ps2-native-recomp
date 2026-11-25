"""L6 read-only call-boundary inventory and discharge proposals, never discharge.

Site groups partition 101. Direct-target counts and overlapping CFG traits have
different denominators. Names and module prefixes are not classification proof.
"""
from __future__ import annotations
from collections import Counter, defaultdict
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import re
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from verify_stack_exit_paths import audit_words, decode, ELF_SHA, hx

PINS = {'functions.tsv': '3ee745347f4f8076171fec3b6974dd01311bd165a30b5b359926f1a79edaeaa9',
        'function-bodies.tsv': '4f920c6660dba7ba5fd2ccf42150d7d464f2e1153110751ee81efb08ff9677c3',
        'instructions.tsv': '4611757f817e6c4ace87bb2d1203bca9c5d8523c33e0ae3e086890ab4b5ce148'}
PROPOSALS = {
    'INDIRECT_TARGET_UNRESOLVED': ['Authenticate the target set from original pointer stores/descriptor and actual caller flow.',
        'Prove each target ABI, SP/RA preservation and argument/result widths; no guessed native bridge.'],
    'DIRECT_EXTENT_UNAVAILABLE': ['Recover and independently authenticate the original function boundary/CFG before any contract proposal.'],
    'DIRECT_OUT_OF_BODY_EXIT': ['Resolve each out-of-body edge and prove tail intent, restored SP and retained return link; otherwise remain unresolved.',
        'Authenticate the destination contract before composing any return/SP guarantee.'],
    'DIRECT_UNPROVED_LOCAL_CFG': ['Close unknown control, SP definitions and every normal exit from authenticated original instructions.'],
    'DIRECT_CONTEXT_RESTORE_CONTRACT_UNPROVED': ['Recover original saved-context layout, provenance/lifetime, restored-register widths and destination as REFERENCE evidence only.',
        'Propose a native continuation/control-transfer replacement only after actual caller, producer and consumer proof; the ordinary preserve-SP/return-PC+8 summary is inapplicable without compatibility proof.',
        'The accepted route must contain no guest SP/RA/PC/register-bank object or fixed source context layout. Existing NativeMainReentry is only a candidate after compatibility proof and remains unchanged.'],
    'DIRECT_CLOSED_LEAF': ['Independently prove input RA identity and SP preservation across every original path.',
        'Join actual caller argument/result widths and side effects, then provide independent behavioral evidence; leaf identity alone does not discharge.'],
    'DIRECT_CLOSED_NONLEAF': ['Recursively resolve every nested direct/indirect callee contract and compose SP/RA guarantees.',
        'Prove actual caller input/result widths, side effects and independent behavior before admission.']}


def sha(raw): return hashlib.sha256(raw).hexdigest()
def rows(raw): return csv.DictReader(io.StringIO(raw.decode('utf-8-sig')), delimiter='\t')


def reader(image):
    if sha(image) != ELF_SHA: raise ValueError('original ELF pin changed')
    ph = struct.unpack_from('<I', image, 28)[0]
    stride, count = struct.unpack_from('<HH', image, 42)
    segments = [struct.unpack_from('<8I', image, ph + i * stride) for i in range(count)]
    def read(pc, size):
        candidates = [image[off + pc - va:off + pc - va + size]
                      for kind, off, va, _, length, *_ in segments
                      if kind == 1 and va <= pc and pc + size <= va + length]
        if len(candidates) != 1 or len(candidates[0]) != size: raise ValueError('original bytes not uniquely file-backed')
        return candidates[0]
    return read


def classify(cfg, has_extent):
    if not has_extent: return 'DIRECT_EXTENT_UNAVAILABLE'
    if any(e['target_pc'] is not None and e['reason'] == 'CFG_gap_or_tail_outside_authenticated_body'
           for e in cfg['unresolved_edges']): return 'DIRECT_OUT_OF_BODY_EXIT'
    if cfg['local_exit_status'] != 'PROVED_LOCAL_EXIT_GEOMETRY': return 'DIRECT_UNPROVED_LOCAL_CFG'
    return 'DIRECT_CLOSED_NONLEAF' if cfg['call_boundaries'] else 'DIRECT_CLOSED_LEAF'


def self_test():
    leaf = audit_words({0x1000: 0x03e00008, 0x1004: 0}, 0x1000)
    assert classify(leaf, True) == 'DIRECT_CLOSED_LEAF'
    # A leaf-like original J is only a potential out-of-body jump, not a
    # discharged tail call or a known native return convention.
    jump = audit_words({0x1000: 0x08000800, 0x1004: 0}, 0x1000)
    assert classify(jump, True) == 'DIRECT_OUT_OF_BODY_EXIT'
    call = audit_words({0x1000: 0x0c000800, 0x1004: 0, 0x1008: 0x03e00008, 0x100c: 0}, 0x1000)
    assert classify(call, True) == 'DIRECT_CLOSED_NONLEAF'
    assert classify({}, False) == 'DIRECT_EXTENT_UNAVAILABLE'
    return {'status': 'PASS', 'controls': ['leaf_with_JR', 'external_J_not_known_tail_convention',
                                         'nonleaf_keeps_unproved_callee_contract', 'missing_extent_is_unknown']}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--self-test', action='store_true')
    for name in ('call-paths', 'original-elf', 'export-dir', 'generated-dir', 'aot-census', 'output'):
        parser.add_argument('--' + name, type=Path)
    args = parser.parse_args(); tests = self_test()
    if args.self_test and not args.call_paths: print(json.dumps(tests)); return
    if any(getattr(args, x.replace('-', '_')) is None for x in
           ('call-paths', 'original-elf', 'export-dir', 'generated-dir', 'aot-census', 'output')):
        parser.error('all input/output paths are required')
    fork = Path(__file__).resolve().parents[2]; output = args.output.resolve()
    if output.exists() or not output.is_relative_to(fork / 'build/oracle-telemetry'):
        raise ValueError('output must be a new ignored telemetry directory')
    raw_paths = args.call_paths.read_bytes(); paths = json.loads(raw_paths)
    verifier = Path(__file__).with_name('verify_stack_exit_paths.py')
    if sha(verifier.read_bytes()) != paths['verifier_sha256']: raise ValueError('exit verifier source pin mismatch')
    if paths['status'] != 'COMPLETED_LOCAL_CFG_AUDIT_WITH_UNCONDITIONAL_LIMITS' or paths['planted_mutation']:
        raise ValueError('requires actual nonplant positive local exit audit')
    original = reader(args.original_elf.read_bytes())
    calls = [dict(c, caller_entry=t['source_entry']) for t in paths['targets'] for c in t['call_boundaries']]
    direct = {int(c['target'], 16) for c in calls if c['target'] is not None}
    if (len(calls) != 101 or len({c['pc'] for c in calls}) != 101 or len(direct) != 52
            or Counter(c['kind'] for c in calls) != {'call': 89, 'indirect_call': 12}):
        raise ValueError('the exact101-site/52-direct-target denominator changed')
    export_raw = {name: (args.export_dir / name).read_bytes() for name in PINS}
    if {name: sha(raw) for name, raw in export_raw.items()} != PINS: raise ValueError('Ghidra export pin changed')
    catalogue = {int(r['entry'], 16): r for r in rows(export_raw['functions.tsv'])}
    spans = defaultdict(list)
    for r in rows(export_raw['function-bodies.tsv']):
        at = int(r['entry'], 16)
        if at in direct: spans[at].append((int(r['start'], 16), int(r['end'], 16)))
    bodies = {}; needed = {int(c['pc'], 16) + d for c in calls for d in (0, 4)}
    for c in calls:
        if c['target'] == '001ddf98': needed.update(int(c['pc'], 16) + d for d in (-8, -4, 8))
    for entry in sorted(direct):
        words = {}; pins = []
        for start, end in spans[entry]:
            if start & 3 or (end - start + 1) % 4: raise ValueError('unaligned callee extent')
            raw = original(start, end - start + 1)
            pins.append({'start': hx(start), 'end': hx(end), 'sha256': sha(raw)})
            for at in range(start, end + 1, 4):
                if at in words: raise ValueError('overlapping callee body intervals')
                words[at] = int.from_bytes(original(at, 4), 'little')
        bodies[entry] = (words, pins); needed.update(words)
    listing = {}
    for row in rows(export_raw['instructions.tsv']):
        pc = int(row['address'], 16)
        if pc not in needed: continue
        if pc in listing: raise ValueError('duplicate listing PC')
        if bytes.fromhex(row['bytes']) != original(pc, 4): raise ValueError('original/listing word mismatch')
        listing[pc] = {'mnemonic': row['mnemonic'], 'function_entry': row['function_entry'],
                       'flow_type': row['flow_type'], 'in_delay_slot': row['in_delay_slot']}
    if needed - listing.keys(): raise ValueError('required original instruction absent from pinned listing')

    # Every used generated label is joined to the pre-existing census's whole
    # source-unit SHA; this neither invents retail replacements nor infers types.
    census_raw = args.aot_census.read_bytes(); census = json.loads(census_raw)
    source_map = census['aot']['all_measured_source_sha256']; labels = {}; unit_pins = {}
    for file in sorted(args.generated_dir.glob('unit_*.cpp')):
        raw = file.read_bytes(); text = raw.decode(); digest = sha(raw)
        if source_map.get(file.name) != digest: raise ValueError('AOT source differs from measured census: ' + file.name)
        matches = list(re.finditer(r'(?m)^L_([0-9A-F]{8}):', text))
        for n, match in enumerate(matches):
            pc = int(match[1], 16)
            if pc not in needed: continue
            if pc in labels: raise ValueError('duplicate generated label')
            end = matches[n + 1].start() if n + 1 < len(matches) else len(text)
            fragment = text[match.start():end]
            labels[pc] = {'unit': file.name, 'byte_offset': len(text[:match.start()].encode()),
                          'source_sha256': digest, 'fragment_sha256': sha(fragment.encode()), 'fragment': fragment}
            unit_pins[file.name] = digest
    if needed - labels.keys(): raise ValueError('required original PC absent from measured generated AOT')
    callees = []
    for entry in sorted(direct):
        words, pins = bodies[entry]; has_extent = bool(words) and entry in words and entry in catalogue
        cfg = audit_words(words, entry) if has_extent else {}
        group = classify(cfg, has_extent)
        loads = [{'pc': hx(pc), 'word': hx(w), 'register': w >> 16 & 31,
                  'base_register': w >> 21 & 31, 'offset': (w & 65535) - (65536 if w & 32768 else 0),
                  'width_bits': {0x23: 32, 0x37: 64, 0x1e: 128}[w >> 26]}
                 for pc, w in sorted(words.items()) if w >> 26 in {0x23, 0x37, 0x1e}
                 and w >> 16 & 31 in {29, 31} and w >> 21 & 31 != 29]
        context_restore = (any(x['register'] == 29 for x in loads)
                           and any(x['register'] == 31 and x['base_register'] == s['base_register']
                                   for x in loads for s in loads if s['register'] == 29)
                           and bool(cfg.get('normal_returns')))
        if context_restore: group = 'DIRECT_CONTEXT_RESTORE_CONTRACT_UNPROVED'
        external = cfg.get('unresolved_edges', [])
        traits = {'has_reachable_JR_RA': bool(cfg.get('normal_returns')),
                  'no_reachable_link_calls_within_declared_extent': not cfg.get('call_boundaries') if has_extent else None,
                  'has_nested_direct_calls': any(c['kind'] == 'call' for c in cfg.get('call_boundaries', [])),
                  'has_nested_indirect_calls': any(c['kind'] == 'indirect_call' for c in cfg.get('call_boundaries', [])),
                  'has_potential_out_of_body_jump': any(e['target_pc'] is not None for e in external),
                  'has_unresolved_indirect_exit': any(e['reason'] == 'unresolved_indirect_exit' for e in external),
                  'has_nonSP_based_SP_and_RA_context_restore': context_restore,
                  'local_CFG_closed_and_SP_balanced': cfg.get('local_exit_status') == 'PROVED_LOCAL_EXIT_GEOMETRY'}
        context_evidence = None
        if context_restore:
            context_evidence = {'full_original_context_spans': pins, 'SP_and_RA_loads': loads,
                'machine_observation': 'Original SP and RA loaded from the same non-SP-based structure before JR RA; exact destination/context provenance remains unproved.',
                'ordinary_callee_return_summary': 'INAPPLICABLE_AS_UNCONDITIONAL_CONTRACT; entry SP and JAL PC+8 are not the source of these restored values',
                'original_word_listing_generated_joins': [
                    {'pc': hx(pc), 'word': hx(w), 'listing': listing[pc], 'generated': labels[pc]}
                    for pc, w in sorted(words.items())]}
        callees.append({'entry': hx(entry), 'catalogue': catalogue.get(entry), 'body_spans': pins,
                        'body_instruction_count': len(words), 'listing_original_word_joins': len(words),
                        'generated_original_PC_joins': len(words),
                        'listing_owner_mismatches': [hx(p) for p in words if listing[p]['function_entry'] != hx(entry)],
                        'exclusive_group': group, 'overlapping_traits': traits, 'local_CFG': cfg,
                        'machine_return_convention': 'JR r31 observed at listed reachable normal exits' if traits['has_reachable_JR_RA'] else 'UNKNOWN',
                        'argument_and_result_types': 'UNKNOWN; no typed ABI inferred from labels or return form',
                        'SP_preservation_contract': 'UNPROVED; local static geometry is not a discharged interprocedural contract',
                        'tail_intent': 'UNPROVED; any out-of-body jump remains a potential tail only',
                        'nonlocal_context_evidence': context_evidence,
                        'call_site_PCs': [c['pc'] for c in calls if c['target'] == hx(entry)],
                        'proposed_discharge': PROPOSALS[group], 'discharged': False})
    by_entry = {c['entry']: c for c in callees}; sites = []
    for call in calls:
        pc = int(call['pc'], 16); word = int.from_bytes(original(pc, 4), 'little')
        transfer = decode(pc, word)
        if hx(word) != call['word'] or transfer['kind'] != call['kind']: raise ValueError('call identity mismatch')
        if call['target'] != (hx(transfer['target']) if 'target' in transfer else None): raise ValueError('call target mismatch')
        group = by_entry[call['target']]['exclusive_group'] if call['target'] else 'INDIRECT_TARGET_UNRESOLVED'
        ordinary_summary = ('INAPPLICABLE_AS_UNCONDITIONAL_CONTEXT_RESTORE_CONTRACT'
                            if group == 'DIRECT_CONTEXT_RESTORE_CONTRACT_UNPROVED' else 'UNPROVED_CONDITIONAL_SUMMARY')
        sites.append({**call, 'exclusive_group': group,
                      'historical_hypothetical_return_summary': call['return_summary'],
                      'ordinary_SP_preserving_PC_plus8_contract': ordinary_summary,
                      'original_delay_pc': hx(pc + 4), 'original_delay_word': hx(int.from_bytes(original(pc + 4, 4), 'little')),
                      'listing_call': listing[pc], 'listing_delay': listing[pc + 4],
                      'machine_call_signature': {'instruction': 'JAL' if call['kind'] == 'call' else 'JALR',
                          'link_register': 31 if call['kind'] == 'call' else word >> 11 & 31,
                          'target_register_before_delay': None if call['kind'] == 'call' else word >> 21 & 31,
                          'architectural_return_PC': hx(pc + 8), 'argument_types': 'UNKNOWN', 'result_types': 'UNKNOWN'},
                      'generated_call': labels[pc], 'generated_delay_label': labels[pc + 4],
                      'context_restore_caller_neighborhood': [
                          {'pc': hx(at), 'word': hx(int.from_bytes(original(at, 4), 'little')),
                           'listing': listing[at], 'generated': labels[at]}
                          for at in range(pc - 8, pc + 12, 4)]
                          if group == 'DIRECT_CONTEXT_RESTORE_CONTRACT_UNPROVED' else None,
                      'proposed_discharge': PROPOSALS[group], 'SP_preservation_contract': 'UNPROVED', 'discharged': False})
    site_groups = dict(Counter(s['exclusive_group'] for s in sites))
    if sum(site_groups.values()) != 101: raise AssertionError('exclusive site partition does not sum to 101')
    callers = [{'entry': entry, 'sites': [s['pc'] for s in sites if s['caller_entry'] == entry],
                'exclusive_site_groups': dict(Counter(s['exclusive_group'] for s in sites if s['caller_entry'] == entry)),
                'proposed_discharge_groups': sorted({s['exclusive_group'] for s in sites if s['caller_entry'] == entry}),
                'SP_contracts_discharged': 0} for entry in sorted({s['caller_entry'] for s in sites})]
    report = {'schema': 'stack_call_boundary_inventory_proposals_v1', 'status': 'INVENTORIED_NO_DISCHARGE',
              'scope': 'Read-only original-word/listing/measured-AOT classification and proposals. A closed local CFG, leaf or JR form does not prove typed ABI, return-link identity, interprocedural SP preservation, ownership or native admission.',
              'input_pins': {'call_paths_sha256': sha(raw_paths), 'original_elf_sha256': ELF_SHA,
                  'export_sha256': PINS, 'aot_census_sha256': sha(census_raw), 'used_generated_units_sha256': unit_pins,
                  'exit_verifier_sha256': sha(verifier.read_bytes()), 'tool_sha256': sha(Path(__file__).read_bytes())},
              'self_tests': tests,
              'contradictions_with_earlier_documents': ['The 101 earlier SP-preserving return summaries were conditional local-CFG assumptions, not 101 authenticated ordinary callee contracts. Source-evidenced context restoration at 001ddf98 is incompatible with treating that boundary as unconditionally preserve-SP/return-PC+8.'],
              'summary': {'call_site_denominator': 101, 'unique_call_site_PCs': 101, 'direct_sites': 89,
                  'unique_direct_targets': 52, 'unresolved_indirect_sites': 12,
                  'exclusive_site_groups': site_groups,
                  'exclusive_direct_target_groups': dict(Counter(c['exclusive_group'] for c in callees)),
                  'overlapping_direct_target_traits': dict(Counter(k for c in callees for k, v in c['overlapping_traits'].items() if v is True)),
                  'SP_contracts_discharged': 0, 'native_credit': 0},
              'callers': callers, 'sites': sites, 'direct_callees': callees}
    output.mkdir(); path = output / 'RESULT.json'
    path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'receipt': str(path), 'sha256': sha(path.read_bytes()), 'summary': report['summary']}))


if __name__ == '__main__': main()
