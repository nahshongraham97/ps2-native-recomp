"""Authenticate the original EE builder, finite callback tables and open domains.

This is an offline producer-domain audit. It neither interprets emitted code nor
turns cache group counts into a constructor universe. Original bytes, listing
coverage and generated-source expressions are kept distinct.
"""
from pathlib import Path
from collections import defaultdict
import argparse
import csv
import hashlib
import json
import re
import struct

W = Path(__file__).resolve().parents[2]
ELF_SHA = 'a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f'
LISTING_SHA = '4611757f817e6c4ace87bb2d1203bca9c5d8523c33e0ae3e086890ab4b5ce148'
EFFECTS = [(1, 0x2ee290), (25, 0x2ee160), (2, 0x2ee160),
           (3, 0x2ee160), (26, 0x2edfa0), (5, 0x2edfa0),
           (6, 0x2edfa0), (17, 0x2edc50), (22, 0x2ee2d0),
           (21, 0x2ee2d0), (24, 0x2ee2d0)]
ROOTS = {0x2ed4b0, 0x2ed3e0, 0x2ed300, *(v for _, v in EFFECTS),
         0x2fa9d0, 0x2e7960, 0x2e7c20, 0x2e7620, 0x2e7d30, 0x2eae10}
APPEND_CODE = {
    0x2ed808: ('PREF', '0xCC000050 | (allocated_GPR_index << 21)'),
    0x2ed828: ('PREF', '0xCC000050 | (allocated_GPR_index << 21)'),
    0x2ed9f4: ('NOP', '0'), 0x2ed9fc: ('NOP', '0'),
    0x2edf08: ('CVT.W.S', '0x46000024 | (allocated_FPR_index << 11) | (allocated_FPR_index << 6)'),
    0x2edf50: ('SWC1', '0xE4000000 | (base_GPR_index << 21) | (value_FPR_index << 16) | ((field_offset + 12) & 0xFFFF)'),
}


def need(ok, why):
    if not ok:
        raise ValueError(why)


def pin(path):
    data = path.read_bytes()
    return dict(path=str(path.resolve()), size=len(data), sha256=hashlib.sha256(data).hexdigest())


class Original:
    def __init__(self, raw):
        self.raw = raw
        ph = struct.unpack_from('<I', raw, 28)[0]
        step, count = struct.unpack_from('<HH', raw, 42)
        self.loads = [struct.unpack_from('<8I', raw, ph+i*step) for i in range(count)]

    def bytes(self, address, size):
        mappings = [self.raw[o+address-v:o+address-v+size]
                    for k, o, v, _, s, *_ in self.loads
                    if k == 1 and v <= address <= address+size <= v+s]
        need(len(mappings) == 1, 'ambiguous_or_nonfile_ELF_mapping')
        return mappings[0]

    def word(self, address):
        return struct.unpack('<I', self.bytes(address, 4))[0]


def table(original, address, expected):
    rows = []
    for i, pair in enumerate([*expected, (0xffffffff, 0)]):
        actual = struct.unpack('<II', original.bytes(address+i*8, 8))
        need(actual == pair, 'original_callback_table_changed')
        rows.append(dict(address=f'{address+i*8:08X}', id=actual[0], callback=f'{actual[1]:08X}'))
    return rows


def self_test(original):
    table(original, 0x5caf90, EFFECTS)
    for changed in [[(1, 0x2ee160), *EFFECTS[1:]], EFFECTS[:-1]]:
        try:
            table(original, 0x5caf90, changed)
        except ValueError:
            continue
        raise ValueError('planted_callback_map_accepted')
    # The signed immediate matters: 005D0000 + ADDIU AF90 = 005CAF90.
    need(0x5d0000 + struct.unpack('<h', bytes.fromhex('90af'))[0] == 0x5caf90,
         'signed_table_address_derivation_failed')
    return dict(status='PASS', rejected=['wrong_callback', 'truncated_callback_table'],
                signed_low_immediate=True)


def literal_argument(original, pc, register):
    """Strict local literal witness, including the executed JAL delay slot.

    This does not propagate across calls, branches or unknown definitions.
    Unknown is retained, rather than inferring the argument from a callee name.
    """
    for q in [pc+4, *range(pc-4, max(0, pc-64), -4)]:
        word = original.word(q); op = word >> 26
        rs, rt, rd, fn = (word >> 21)&31, (word >> 16)&31, (word >> 11)&31, word&63
        if q != pc+4 and (op in (1, 2, 3, 4, 5, 6, 7, 20, 21, 22, 23)
                           or (op == 0 and fn in (8, 9))):
            return None
        if op in (8, 9) and rt == register:
            if rs != 0:
                return None
            value = word&65535
            value -= 65536 if value&32768 else 0
            return dict(value=value, definition_pc=f'{q:08X}', word=f'{word:08X}')
        if op == 13 and rt == register:
            return dict(value=word&65535, definition_pc=f'{q:08X}', word=f'{word:08X}') if rs == 0 else None
        # Ordinary integer instructions that define this argument invalidate a
        # backward literal search, including memory loads and register copies.
        if op == 0 and rd == register and fn not in (0x18, 0x19, 0x1a, 0x1b):
            return dict(value=0, definition_pc=f'{q:08X}', word=f'{word:08X}') if rs == rt == 0 and fn in (0x21, 0x25, 0x2d) else None
        if op in (*range(8, 16), 0x18, 0x19, 0x1a, 0x1b, 0x1e, *range(0x20, 0x28), 0x37) and rt == register:
            return None
        if op in (0x10, 0x11, 0x12) and rs in (0, 1, 2) and rt == register:
            return None
        if op == 0x1c and rd == register:
            return None
        if op not in (0, *range(8, 16), 0x10, 0x11, 0x12, 0x18, 0x19, 0x1a, 0x1b, 0x1c,
                      0x1e, 0x1f, *range(0x20, 0x3a), 0x3c, 0x3d, 0x3e, 0x3f):
            return None
    return None


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--elf', type=Path, required=True)
    p.add_argument('--listing', type=Path, required=True)
    p.add_argument('--generated-directory', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    out = a.output.resolve()
    need(out.is_relative_to((W/'build/oracle-telemetry').resolve()) and not out.exists(),
         'fresh_owned_output_required')
    need(pin(a.elf)['sha256'] == ELF_SHA, 'retail_ELF_changed')
    need(pin(a.listing)['sha256'] == LISTING_SHA, 'original_listing_changed')
    original = Original(a.elf.read_bytes())
    bodies = defaultdict(dict)
    all_rows = {}
    descriptor_callers = []
    with a.listing.open(encoding='utf8', newline='') as stream:
        for row in csv.DictReader(stream, delimiter='\t'):
            pc = int(row['address'], 16)
            word = int.from_bytes(bytes.fromhex(row['bytes']), 'little')
            op = word >> 26
            if op in (2, 3, 4, 5, 6, 7, 20, 21, 22, 23):
                immediate=word&65535;immediate-=65536 if immediate&32768 else 0
                target = (((pc+4)&0xf0000000)|((word&0x3ffffff)<<2)) if op in (2,3) else pc+4+immediate*4
                if target in (0x2e7960, 0x2e7c20, 0x2ed3e0, 0x2ed4b0):
                    need(original.word(pc) == word, 'caller_listing_ELF_word_mismatch')
                    descriptor_callers.append(dict(pc=f'{pc:08X}', word=f'{word:08X}',
                                                   function=row['function_entry'], target=f'{target:08X}',
                                                   edge_kind='DIRECT_CALL' if op == 3 else 'DIRECT_JUMP_OR_BRANCH_NO_RETURN_ABI_INFERRED',
                                                   delay_word=f'{original.word(pc+4):08X}',
                                                   effect_or_filter_id=literal_argument(original, pc, 5)
                                                   if target in (0x2e7960, 0x2e7c20) else None))
            if not (0x2e0000 <= pc < 0x300000):
                continue
            need(original.word(pc) == word, 'listing_ELF_word_mismatch')
            all_rows[pc] = dict(pc=f'{pc:08X}', word=f'{word:08X}', mnemonic=row['mnemonic'],
                                flow_type=row['flow_type'], in_delay_slot=row['in_delay_slot'])
            if row['function_entry']:
                bodies[int(row['function_entry'], 16)][pc] = word
    # This two-instruction callback was not assigned a function by Ghidra.
    need(original.word(0x2ee2d0) == 0x03e00008 and original.word(0x2ee2d4) == 0x24020001,
         'no_op_callback_return_changed')
    bodies[0x2ee2d0] = {pc: original.word(pc) for pc in (0x2ee2d0, 0x2ee2d4)}
    generated = {}
    source_pins = []
    for path in sorted(a.generated_directory.glob('unit_*.cpp')):
        text = path.read_bytes().decode('utf8')
        if re.search(r'L_002(?:E|F)', text):
            generated[path.name] = text
            source_pins.append(pin(path))
    todo = list(ROOTS); visited = set(); records = []
    while todo:
        entry = todo.pop()
        if entry in visited:
            continue
        visited.add(entry)
        body = bodies.get(entry)
        if not body:
            records.append(dict(entry=f'{entry:08X}', status='ORIGINAL_FUNCTION_EXTENT_UNRESOLVED'))
            continue
        lo, hi = min(body), max(body)+4
        calls = []; indirect = []; append_sites = []; tail_edges = []
        for pc, word in sorted(body.items()):
            if word >> 26 == 3:
                target = ((pc+4)&0xf0000000)|((word&0x3ffffff)<<2)
                calls.append(dict(pc=f'{pc:08X}', word=f'{word:08X}', target=f'{target:08X}',
                                  delay_word=f'{original.word(pc+4):08X}'))
                if 0x2ec000 <= target < 0x2f0600:
                    todo.append(target)
                if target == 0x2effc0:
                    context = [all_rows[q] for q in range(max(lo, pc-32), pc+8, 4) if q in all_rows]
                    form = APPEND_CODE.get(pc)
                    append_sites.append(dict(pc=f'{pc:08X}', original_context=context,
                                             classification='CONSTRUCTED_INSTRUCTION_WORD' if form else 'CONSTANT_BLOCK_VALUE_SOURCE_REVIEW',
                                             opcode=form[0] if form else None, expression=form[1] if form else None,
                                             limit='Register/offset domains and selected code/data block lifetime still require producer proof.'))
            elif word >> 26 == 2:
                target = ((pc+4)&0xf0000000)|((word&0x3ffffff)<<2)
                if target not in body:
                    tail_edges.append(dict(pc=f'{pc:08X}', word=f'{word:08X}', target=f'{target:08X}',
                                           delay_word=f'{original.word(pc+4):08X}', tail_return_ABI='UNPROVED'))
                    if 0x2ec000 <= target < 0x2f0600:
                        todo.append(target)
            elif word >> 26 == 0 and (word&63) == 9:
                indirect.append(dict(pc=f'{pc:08X}', word=f'{word:08X}',
                                     target_register=(word>>21)&31, delay_word=f'{original.word(pc+4):08X}'))
        source = []
        for name, text in generated.items():
            start = text.find(f'L_{entry:08X}:')
            if start < 0:
                continue
            finish = text.find(f'L_{hi:08X}:', start)
            if finish < 0:
                labels = [(int(m[1], 16), m.start()) for m in re.finditer(r'^L_([0-9A-F]{8}):', text, re.M)]
                finish = next((offset for pc, offset in labels if pc >= hi and offset > start), len(text))
            excerpt = text[start:finish]
            source.append(dict(unit=name, start_byte=len(text[:start].encode()),
                               excerpt_sha256=hashlib.sha256(excerpt.encode()).hexdigest(),
                               generated_cpp=excerpt))
        raw = original.bytes(lo, hi-lo)
        records.append(dict(entry=f'{entry:08X}', status='AUTHENTICATED_ORIGINAL_SPAN',
                            span=[f'{lo:08X}', f'{hi:08X}'], span_sha256=hashlib.sha256(raw).hexdigest(),
                            listing_words=len(body), listing_unowned_words=[f'{q:08X}' for q in range(lo, hi, 4) if q not in body],
                            direct_calls=calls, indirect_calls=indirect, out_of_body_jumps=tail_edges, raw_append_sites=append_sites,
                            source_excerpts=source))
    out.mkdir(parents=True)
    detail = out/'producer-functions.json'
    detail.write_text(json.dumps(sorted(records, key=lambda r:r['entry']), indent=2)+'\n', encoding='utf8', newline='\n')
    result = dict(status='OPEN_PRODUCER_DOMAIN_CLOSURE', source_pins=[pin(Path(__file__)), pin(a.elf), pin(a.listing), *source_pins],
                  self_test=self_test(original), effect_table=table(original, 0x5caf90, EFFECTS),
                  filter_table=table(original, 0x5caff0, [(2, 0x2ed300)]),
                  effect_id_count=11, effect_callback_count=5, filter_callback_count=1,
                  reachable_function_records=len(records), direct_call_sites=sum(len(r.get('direct_calls', [])) for r in records),
                  raw_append_sites=sum(len(r.get('raw_append_sites', [])) for r in records),
                  original_indirect_sites=[dict(function=r['entry'], **c) for r in records for c in r.get('indirect_calls', [])],
                  record=pin(detail),
                  original_descriptor_callers=descriptor_callers,
                  non_emitted_effect_switch=dict(address='00630C90', ids=list(range(23)),
                    targets=[f'{x:08X}' for x in struct.unpack('<23I', original.bytes(0x630c90, 92))],
                    limit='Independent semantic orientation only: IDs24/25/26 are outside this switch; ID22 has work here but emits no code. Whole-route equivalence is NOT inferred.'),
                  producer_bounds=dict(effect_append_entry='002E7960', effect_signed_upper_guard=13,
                                       effect_negative_count_rejected_by_original=False,
                                       filter_append_entry='002E7C20', filter_capacity_guard='NONE in this function',
                                       descriptor_record_bytes=928, parent_batch_count_upper_exclusive=11,
                                       parent_producer='002FA9D0', builder_call='002FAEAC',
                                       parent_output_capacity_words=2000,
                                       limits='These are source-local guards, not a proved invariant for all callers or loaded descriptors.'),
                  boundary_edges=[dict(function=r['entry'], **c) for r in records for c in
                                  r.get('direct_calls', [])+r.get('out_of_body_jumps', [])
                                  if not (0x2ec000 <= int(c['target'], 16) < 0x2f0600)],
                  descriptor_geometry=dict(effect_count_offset=192, effect_records_offset=208, effect_stride=48,
                                           filter_count_offset=80, filter_records_offset=84, filter_stride=24,
                                           attribute_record_stride=12, attribute_sentinel=0xffffffff),
                  unresolved=['All descriptor producers/asset constraints and register-allocation limits must be recovered.',
                              'Raw append receives constants as well as constructed instructions; a raw opcode universe cannot be inferred from every appended word.',
                              'Finite callback table identity alone does not close descriptor sequences, offset arithmetic, code/data block layout or branches.',
                              'No runtime opcode-handler VM, no cache-template promotion, no production integration, no ownership/native-credit change.'])
    target=out/'RESULT.json'; target.write_text(json.dumps(result, indent=2)+'\n', encoding='utf8', newline='\n')
    print(json.dumps(dict(status=result['status'], result=str(target), sha256=pin(target)['sha256'])))


if __name__ == '__main__':
    main()
