"""Source-only receiver/callback proofs for the bounded EE producer cache.

This does not install a Plan registry or execute guest code. Resolved table
targets are conditional on the authenticated constructed receiver retaining its
table/identity. Unresolved aliases, indirect callers and restore are not erased.
"""
from pathlib import Path
from collections import defaultdict
import argparse
import csv
import hashlib
import json
import re
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import analyze_r192_builder_domain as auth

W = Path(__file__).resolve().parents[2]
ROOTS = {0x12ec50, 0x12efb0, 0x12f100, 0x130a50, 0x130ab0,
         0x130d00, 0x185ee0, 0x211c30, 0x211c50, 0x2fee20,
         0x2fa9d0, 0x2fe090, 0x31ea00}
ANCHORS = {
    0x211cd4: 0x3c02005a, 0x211cdc: 0x2442fd90, 0x211ce4: 0xae220000,
    0x211da4: 0x26240040, 0x211da8: 0x0260f809,
    0x211e2c: 0x8e220000, 0x211e30: 0x8c42000c,
    0x211e34: 0x0040f809, 0x211e38: 0x0220202d,
    0x210808: 0x8c820000, 0x21080c: 0x8c42000c, 0x210810: 0x0040f809,
    0x2108ec: 0x8c820000, 0x2108f0: 0x8c42000c, 0x2108f4: 0x0040f809,
    0x12ecb4: 0x3c02005a, 0x12ecb8: 0x2442fc70, 0x12ecbc: 0xae020000,
    0x130ab0: 0x24030001, 0x130ab4: 0xaf849e88, 0x130abc: 0xaf839e84,
}


def need(ok, why):
    if not ok:
        raise ValueError(why)


def target(pc, word):
    return ((pc+4) & 0xf0000000) | ((word & 0x3ffffff) << 2) if word >> 26 in (2, 3) else None


def check_words(words):
    for pc, expected in ANCHORS.items():
        need(words.get(pc) == expected, f'original_receiver_operand_changed:{pc:08X}')


def check_tables(tables):
    need(tables.get(0x59fd90) == (0x12d870, 0x12d860, 0x12d830, 0x210920),
         'cache_constructed_table_changed')
    need(tables.get(0x59fc70) == (0x12d880, 0x12d850, 0x12d830, 0x12efb0),
         'task_constructed_table_changed')


def local_constant(words, call, register):
    """Bounded backward definition proof through a single straight-line suffix.

    Only literal/copy definitions are admitted. A control edge, load or other
    definition blocks the proof; this is not instruction execution or an ABI
    assumption. The actual JAL delay slot is visited first.
    """
    pcs = [call+4, *range(call-4, call-100, -4)]

    def search(reg, indices):
        for i in indices:
            pc = pcs[i]
            need(pc in words, 'local_constant_listing_gap')
            word = words[pc]
            op, rs, rt, rd, fn = word >> 26, (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31, word & 63
            if pc != call+4 and (op in (1, 2, 3, 4, 5, 6, 7, 20, 21, 22, 23) or op == 0 and fn in (8, 9)):
                raise ValueError('local_constant_crosses_control')
            imm = word & 65535
            signed = imm-65536 if imm & 32768 else imm
            if op == 15 and rt == reg:
                return (imm << 16), [pc]
            if op in (9, 13) and rt == reg:
                base, refs = (0, []) if rs == 0 else search(rs, range(i+1, len(pcs)))
                value = (base+signed) & 0xffffffff if op == 9 else base | imm
                return value, [pc, *refs]
            if op == 0 and rd == reg and fn in (0x21, 0x25, 0x2d):
                if rs == rt == 0:
                    return 0, [pc]
                if rs == 0 or rt == 0:
                    value, refs = search(rt if rs == 0 else rs, range(i+1, len(pcs)))
                    return value, [pc, *refs]
                raise ValueError('nonliteral_register_definition')
            if op == 0 and rd == reg and fn not in (0x18, 0x19, 0x1a, 0x1b):
                raise ValueError('nonliteral_integer_definition')
            if op in (*range(8, 16), 0x18, 0x19, 0x1a, 0x1b, 0x1e, *range(0x20, 0x28), 0x37) and rt == reg:
                raise ValueError('nonliteral_load_or_immediate_definition')
            if op in (0x10, 0x11, 0x12) and rs in (0, 1, 2) and rt == reg:
                raise ValueError('nonliteral_coprocessor_definition')
        raise ValueError('literal_definition_not_found')

    value, refs = search(register, range(len(pcs)))
    return dict(value=f'{value:08X}', definition_pcs=[f'{p:08X}' for p in refs])


def check_builders(words):
    records = []
    for pc in (0x2faf18, 0x2fe284):
        need(target(pc, words[pc]) == 0x211c50, 'cache_allocator_call_changed')
        args = {f'r{r}_T{r-8}': local_constant(words, pc, r) for r in (8, 9, 10)}
        need(args['r8_T0']['value'] == '002FEE20', 'original_builder_callback_not_2FEE20')
        need(args['r9_T1']['value'] == args['r10_T2']['value'] == '00000000',
             'original_builder_task_branch_not_skipped')
        records.append(dict(call_pc=f'{pc:08X}', arguments=args,
                            callback_at_211DA8='002FEE20', task_construction='SKIPPED_REGISTER_10_ZERO',
                            unknown_general_callers_not_inferred=True))
    return records


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('elf', 'listing', 'generated-directory', 'lifecycle-proof', 'output'):
        p.add_argument('--'+name, type=Path, required=True)
    a = p.parse_args()
    out = a.output.resolve()
    need(out.is_relative_to((W/'build/oracle-telemetry').resolve()) and not out.exists(), 'fresh_owned_output_required')
    need(auth.pin(a.elf)['sha256'] == auth.ELF_SHA and auth.pin(a.listing)['sha256'] == auth.LISTING_SHA,
         'retail_input_changed')
    prior = a.lifecycle_proof/'RESULT.json'
    need(auth.pin(prior)['sha256'] == 'ad4e372b83a49e797a5796c6e91c399a1f43cb3c1a1fff3a90f252095bb173b5',
         'frozen_lifecycle_receipt_changed')
    elf = auth.Original(a.elf.read_bytes())
    rows, words = {}, {}
    bodies = defaultdict(dict)
    with a.listing.open(encoding='utf8', newline='') as stream:
        for row in csv.DictReader(stream, delimiter='\t'):
            pc = int(row['address'], 16)
            words[pc] = int.from_bytes(bytes.fromhex(row['bytes']), 'little')
            rows[pc] = row
            if row['function_entry']:
                bodies[int(row['function_entry'], 16)][pc] = words[pc]
    for pc in ANCHORS:
        need(elf.word(pc) == words[pc], 'receiver_listing_ELF_mismatch')
    check_words(words)
    tables = {pc: struct.unpack('<4I', elf.bytes(pc, 16)) for pc in (0x59fd90, 0x59fc70)}
    check_tables(tables)
    builders = check_builders(words)
    sources = {}
    for path in sorted(a.generated_directory.glob('unit_*.cpp')):
        text = path.read_bytes().decode('utf8')
        if any(f'L_{entry:08X}:' in text for entry in ROOTS):
            sources[path] = text
    records = []
    for entry in sorted(ROOTS):
        body = bodies[entry]
        need(body, f'original_body_missing:{entry:08X}')
        for pc, word in body.items():
            need(elf.word(pc) == word, f'original_body_word_join_failed:{pc:08X}')
        excerpts = []
        for path, text in sources.items():
            start = text.find(f'L_{entry:08X}:')
            if start < 0:
                continue
            end = next((m.start()+start for m in re.finditer(r'^L_([0-9A-F]{8}):', text[start:], re.M)
                        if int(m.group(1), 16) > max(body)), len(text))
            excerpt = text[start:end]
            excerpts.append(dict(path=str(path.resolve()), start_byte=len(text[:start].encode()),
                                 sha256=hashlib.sha256(excerpt.encode()).hexdigest(), cpp=excerpt))
        need(excerpts, 'generated_entry_missing')
        callers = []
        for pc, word in words.items():
            if target(pc, word) == entry:
                need(elf.word(pc) == word, 'caller_ELF_mismatch')
                callers.append(dict(**rows[pc], delay_word=f'{elf.word(pc+4):08X}'))
        records.append(dict(entry=f'{entry:08X}', words=[rows[pc] for pc in sorted(body)],
                            declared_span=[f'{min(body):08X}', f'{max(body)+4:08X}'],
                            span_sha256=hashlib.sha256(elf.bytes(min(body), max(body)+4-min(body))).hexdigest(),
                            range_gaps=[f'{pc:08X}' for pc in range(min(body), max(body)+4, 4) if pc not in body],
                            generated_excerpts=excerpts, direct_ELF_callers=callers))
    controls = []
    mutations = [('wrong_failure_slot', 0x211e30, 0x8c420008, check_words),
                 ('wrong_cleanup_receiver_load', 0x2108ec, 0x8e020000, check_words),
                 ('wrong_constructor_table', 0x211cdc, 0x2442fc70, check_words),
                 ('callback_wrong_A8', 0x2faf10, None, check_builders),
                 ('task_branch_not_zero', 0x2faf14, None, check_builders),
                 ('wrong_identity_override_destination', 0x130ab4, 0xaf849e8c, check_words)]
    # Mutate the actual latest definitions found by the independent suffix
    # proof, not an arbitrary earlier assignment that might be overwritten.
    for label, pc, replacement, validator in mutations:
        mutated = dict(words)
        if label == 'callback_wrong_A8':
            pc = int(builders[0]['arguments']['r8_T0']['definition_pcs'][0], 16)
            replacement = words[pc] ^ 4
        elif label == 'task_branch_not_zero':
            pc = int(builders[0]['arguments']['r10_T2']['definition_pcs'][0], 16)
            replacement = 0x240a0001
        mutated[pc] = replacement
        try:
            validator(mutated)
        except ValueError as error:
            controls.append(dict(name=label, status='REJECTED', reason=str(error)))
        else:
            raise ValueError('planted_receiver_control_accepted:'+label)
    for label, table in [('wrong_task_virtual_target', 0x59fc70), ('wrong_cache_virtual_target', 0x59fd90)]:
        changed = dict(tables)
        changed[table] = (*changed[table][:3], 0x12d830)
        try:
            check_tables(changed)
        except ValueError as error:
            controls.append(dict(name=label, status='REJECTED', reason=str(error)))
        else:
            raise ValueError('planted_table_target_accepted')
    pins = [auth.pin(path) for path in [Path(__file__), Path(auth.__file__), a.elf, a.listing, prior, *sources]]
    out.mkdir(parents=True)
    functions = out/'FUNCTIONS.json'
    functions.write_text(json.dumps(records, indent=2)+'\n', encoding='utf8', newline='\n')
    (out/'SOURCE.py').write_bytes(Path(__file__).read_bytes())
    result = dict(schema='r192_cache_lease_edges_v1', status='PASS_BOUNDED_RECEIVER_SOURCE_PROOF',
                  runtime_lease_admission='OPEN', pins=pins, builders=builders,
                  constructor_tables={f'{pc:08X}':[f'{word:08X}' for word in row] for pc,row in tables.items()},
                  anchors=[dict(**rows[pc], numeric_word=f'{words[pc]:08X}') for pc in ANCHORS],
                  edges=EDGES, transaction=TRANSACTION, limitations=LIMITS,
                  identity_override_original_caller=dict(call_pc='0031ECB8',
                      argument='Original call delay loads A0 from current record+172; caller then clears record+168 toFFFFFFFF.',
                      scope='Source-backed explicit one-shot identity override; not a proof that this particular caller restores a renderer cache.'),
                  checked_original_words=sum(len(r['words']) for r in records),
                  functions=auth.pin(functions), controls=controls,
                  command=[sys.executable, str(Path(__file__).resolve()), *sys.argv[1:]])
    (out/'RESULT.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf8', newline='\n')
    need(all(auth.pin(Path(pin['path'])) == pin for pin in pins), 'proof_input_changed_during_audit')
    print(json.dumps(dict(status=result['status'], functions=len(records),
                          words=result['checked_original_words'], controls=len(controls),
                          receipt=auth.pin(out/'RESULT.json'))))


EDGES = [
    dict(pc='00211DA8', status='RESOLVED_FOR_TWO_DIRECT_BUILDERS', target='002FEE20',
         receiver='new cache+64 in A0; callback copied from incoming r8/T0 into S3',
         limit='211C30 tail wrapper forwards other arguments; its arbitrary r8/T0 is not resolved here.'),
    dict(pc='00211E34', status='CONSTRUCTED_TABLE_CONDITIONAL_TARGET', target='00210920 -> 00210780',
         receiver='new cache S1, table0059FD90 slot+12',
         limit='Factory table installation proved; mutations through callbacks/aliases must still be excluded or authenticated at the boundary.'),
    dict(pc='002108F4', status='CONSTRUCTED_TASK_CONDITIONAL_TARGET', target='0012EFB0',
         receiver='cache+12 pointer, checked task+4 against cache+16; task created12EC50 and returned unchanged by12F100',
         limit='Both known builder callers set r10/T2=0, skipping task construction; general nonzero branch requires exact task table/identity and no other setter.'),
    dict(pc='00210810', status='UNRESOLVED_DEPENDENT_RECEIVER', target=None,
         receiver='cache+24 pointer checked against cache+28, then its own table slot+12',
         limit='Constructor zeros+24/+28 and flags. No global proof that aliases/setters cannot later attach a different receiver. Never substitute the cache table for this child table.'),
]

TRANSACTION = [
    dict(boundary='BEGIN', pcs=['002FAEAC','002ED460'],
         obligation='Capture immutable original sentinel attributes, descriptor order and allocator metadata while parent/wrapper storage lives; identify transaction independently of shared scratch addresses.'),
    dict(boundary='FRESH_BACKING_ALLOCATION', pcs=['002121D4','002121FC','00212228'],
         obligation='New host epoch for every allocation attempt/success; pool and descriptor slots are ranges inside cache+60 allocation. Original pointer+4 identity is not a unique host epoch.'),
    dict(boundary='UNPUBLISHED_BIND', pcs=['002FAF44','002FAF88','002FB070','002FB150'],
         obligation='Prove final pool copy, per-descriptor nonzero length patch, exact-count928 copy and post-copy owner binding; failure invalidates transaction before any Plan attachment.'),
    dict(boundary='COMMIT', pcs=['002FB2EC','002FB36C','002FB38C'],
         obligation='Commit only original-success path with authenticated allocation/descriptor/code range and current owner binding. Cache-list publication is conditional, not proof of copied descriptors by itself.'),
    dict(boundary='REVOKE', pcs=['00210780','002108AC','00130AB0'],
         obligation='Revoke before destructor/callback/free/explicit identity override, plus every state load/reset and unproved memory writer. Retained leases cannot rely solely on pointer/identity equality.'),
]

LIMITS = [
    'No runtime execution/adoption, code generation, compiler, interpreter, ownership or credit change.',
    'Source joins and local literal dataflow do not prove whole-function behavior or absence of indirect/overlay aliases.',
    '130A50 identity can be explicitly overridden by130AB0; its finite countdown also restarts. Same pointer+identity may recur without demonstrating the210780 cleanup route.',
    'Full RAM restore can resurrect old code/descriptor pointers and identity values without executing producer/copy/free paths. Host epoch must be invalidated independently.',
    'Known2FA9D0 final code-copy route always follows211C50/212150 allocation; 2FE090 also allocates but no direct emitter wrapper call. General raw copy aliases remain open.',
    'Zero fields at construction do not establish immutable fields for the complete lifetime;210810 remains unresolved.',
]


if __name__ == '__main__':
    main()
