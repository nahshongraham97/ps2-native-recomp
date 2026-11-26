"""Read-only original producer/copy/cache lifecycle evidence; never admits a runtime Plan.

Every reported original word is joined to the fixed retail ELF and listing.
Generated C++ excerpts are separately pinned orientation witnesses, not proof
that an arbitrary generated expression implements the original instruction.
The bounded direct-edge inventory does not close indirect or overlay callers.
"""
from pathlib import Path
from collections import defaultdict
import argparse
import copy
import csv
import hashlib
import json
import re
import struct
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
import analyze_r192_builder_domain as original_source

W = Path(__file__).resolve().parents[2]
ROOTS = {
    0x210660, 0x210780, 0x210920, 0x211c50, 0x211f60, 0x212150,
    0x2122d0, 0x212370, 0x212a40, 0x2e1eb0, 0x2e2528, 0x2e29b0,
    0x2e60e0, 0x2e7a30, 0x2e7ac0, 0x2e7ae0, 0x2e7d30, 0x2e7f90,
    0x2e8020, 0x2e8080, 0x2e8110, 0x2e8190, 0x2e83d0,
    0x2e8730, 0x2e8ac0, 0x2ed3e0, 0x2ed4b0, 0x2ee2e0,
    0x2fa9d0, 0x2fb400, 0x2fe090,
}
# Selected operands are fixed independently of the listing: mutations to the
# listing and ELF together cannot redefine these particular chain edges.
ANCHORS = {
    0x2ed458: 0x0c0bb52c, 0x2ed460: 0x0040b02d,
    0x2eda1c: 0xae820394, 0x2faeac: 0x0c0bb4f8,
    0x2faec0: 0x2402fffc, 0x2faec4: 0x00621024,
    0x2faf18: 0x0c084714, 0x2faf44: 0x0c076e2a,
    0x2faf7c: 0x8c8402bc, 0x2faf88: 0xac43004c,
    0x2fb070: 0x0c0ba044, 0x2e8158: 0x0c076e2a,
    0x2e815c: 0x240603a0, 0x2e2528: 0xae43004c,
    0x2108ac: 0x0c04b670,
    0x2e82c0: 0xae43027c, 0x2121fc: 0xaeb3003c,
    0x2e612c: 0x8c8c004c, 0x2e6178: 0x0180f809,
    0x2e617c: 0x01825821, 0x2e6184: 0xae620000,
    0x2ed46c: 0x0c04b698, 0x2ed47c: 0x0c04b698,
}


def need(ok, reason):
    if not ok:
        raise ValueError(reason)


def pin(path):
    return original_source.pin(path)


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def check_anchors(words):
    for pc, expected in ANCHORS.items():
        need(words.get(pc) == expected, f'original_chain_operand_changed:{pc:08X}')


def direct_target(pc, word):
    if word >> 26 in (2, 3):
        return ((pc+4) & 0xf0000000) | ((word & 0x3ffffff) << 2)
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('elf', 'listing', 'generated-directory', 'runtime-source', 'output'):
        parser.add_argument('--'+name, type=Path, required=True)
    a = parser.parse_args()
    output = a.output.resolve()
    need(output.is_relative_to((W/'build/oracle-telemetry').resolve()) and not output.exists(),
         'fresh_owned_telemetry_output_required')
    need(pin(a.elf)['sha256'] == original_source.ELF_SHA, 'original_ELF_identity_changed')
    need(pin(a.listing)['sha256'] == original_source.LISTING_SHA, 'original_listing_identity_changed')
    elf = original_source.Original(a.elf.read_bytes())
    rows = {}
    bodies = defaultdict(dict)
    with a.listing.open(encoding='utf8', newline='') as stream:
        for r in csv.DictReader(stream, delimiter='\t'):
            pc = int(r['address'], 16)
            word = int.from_bytes(bytes.fromhex(r['bytes']), 'little')
            rows[pc] = dict(pc=f'{pc:08X}', word=f'{word:08X}',
                            mnemonic=r['mnemonic'], function=r['function_entry'].upper(),
                            flow=r['flow_type'], listing_delay_slot=r['in_delay_slot'])
            if r['function_entry']:
                bodies[int(r['function_entry'], 16)][pc] = word
    # 2E2528 is an interior word of the separately typed 896-byte path.
    roots = (ROOTS-{0x2e2528}) | {int(rows[0x2e2528]['function'], 16)}
    # Ghidra did not assign these original words to functions. Recover only
    # explicit bounded listing ranges, preserving that absence of ownership.
    bounded_unowned = {0x210780: 0x21091c, 0x210920: 0x210928}
    for entry, end in bounded_unowned.items():
        need(all(pc in rows and not rows[pc]['function'] for pc in range(entry, end, 4)),
             'unowned_cleanup_listing_range_changed')
        bodies[entry] = {pc: int(rows[pc]['word'], 16) for pc in range(entry, end, 4)}
    words = {pc: elf.word(pc) for pc in ANCHORS}
    check_anchors(words)
    for pc, word in words.items():
        need(int(rows[pc]['word'], 16) == word, 'chain_listing_word_mismatch')
    # The constructor installs this table; its +12 slot is a jump veneer,
    # not a guessed destructor name. Follow the original jump explicitly.
    table = struct.unpack('<4I', elf.bytes(0x59fd90, 16))
    need(table == (0x12d870, 0x12d860, 0x12d830, 0x210920), 'installed_table_changed')
    need(direct_target(0x210920, elf.word(0x210920)) == 0x210780,
         'installed_cleanup_veneer_target_changed')
    sources = {}
    for path in sorted(a.generated_directory.glob('unit_*.cpp')):
        text = path.read_bytes().decode('utf8')
        if any(f'L_{entry:08X}:' in text for entry in roots):
            sources[path] = text
    records = []
    for entry in sorted(roots):
        body = bodies.get(entry)
        need(body, f'original_function_body_missing:{entry:08X}')
        joins = []
        for pc, word in sorted(body.items()):
            need(elf.word(pc) == word, f'original_body_word_mismatch:{pc:08X}')
            joins.append(rows[pc])
        # Retain gaps, rather than treating the range as a proven complete CFG.
        gaps = [f'{pc:08X}' for pc in range(min(body), max(body)+4, 4) if pc not in body]
        generated = []
        for path, text in sources.items():
            start = text.find(f'L_{entry:08X}:')
            if start < 0:
                continue
            end = next((m.start() for m in re.finditer(r'^L_([0-9A-F]{8}):', text[start:], re.M)
                        if int(m.group(1), 16) > max(body)), len(text)-start)+start
            excerpt = text[start:end]
            generated.append(dict(path=str(path.resolve()), start_byte=len(text[:start].encode()),
                                  end_byte=len(text[:end].encode()), sha256=sha(excerpt.encode()),
                                  cpp=excerpt))
        need(generated, f'generated_entry_missing:{entry:08X}')
        edges = []
        for pc, word in sorted(body.items()):
            target = direct_target(pc, word)
            indirect = word >> 26 == 0 and (word & 63) in (8, 9) and ((word >> 21) & 31) != 31
            if target is not None or indirect:
                edges.append(dict(**rows[pc], target=f'{target:08X}' if target is not None else None,
                                  delay_word=f'{elf.word(pc+4):08X}',
                                  kind='DIRECT_CALL' if word >> 26 == 3 else
                                  'DIRECT_JUMP_NO_TAIL_ABI_INFERRED' if target is not None else 'UNRESOLVED_INDIRECT'))
        callers = []
        for pc, r in rows.items():
            word = int(r['word'], 16)
            if direct_target(pc, word) == entry:
                need(elf.word(pc) == word, 'direct_caller_word_mismatch')
                callers.append(dict(**r, delay_word=f'{elf.word(pc+4):08X}'))
        records.append(dict(entry=f'{entry:08X}', span=[f'{min(body):08X}', f'{max(body)+4:08X}'],
                            extent_kind='BOUNDED_UNOWNED_LISTING_RANGE' if entry in bounded_unowned else 'GHIDRA_DECLARED_BODY',
                            declared_word_count=len(body), range_gaps=gaps,
                            span_sha256=sha(elf.bytes(min(body), max(body)+4-min(body))),
                            original_words=joins, generated_excerpts=generated,
                            direct_ELF_callers=callers, outgoing_edges=edges))
    system = a.runtime_source/'runtime/src/system.cpp'
    text = system.read_text(encoding='utf8')
    required = ['s.pod(g_cpu);', 's.bytes(g_ram, kRamSize);', 'machine_state(r);', 'g_resumed = true;']
    need(all(s in text for s in required), 'actual_v8_restore_binding_changed')
    need(re.search(r'kStateVersion\s*=\s*8\s*;', text), 'actual_v8_source_required')
    controls = []
    for label, pc, replacement in [
        ('wrong_copy_extent_896', 0x2e815c, 0x24060380),
        ('wrong_descriptor_pointer_field', 0x2faf88, 0xac430048),
        ('wrong_final_pool_field', 0x2faf7c, 0x8c8402b8),
        ('wrong_rounding_mask', 0x2faec0, 0x2402fff8),
        ('wrong_builder_callee', 0x2faeac, 0x0c0bb52c),
        ('896_record_store_masquerades_as_928_pointer_patch', 0x2faf88, words[0x2e2528]),
        ('wrong_codepointer_object_register', 0x2e612c, 0x8e6c004c),
        ('wrong_constantpool_delay_argument', 0x2e617c, 0x01825825),
    ]:
        changed = copy.copy(words)
        changed[pc] = replacement
        try:
            check_anchors(changed)
        except ValueError as error:
            controls.append(dict(name=label, status='REJECTED', reason=str(error)))
        else:
            raise ValueError('planted_chain_mutation_accepted:'+label)
    source_pins = [pin(p) for p in [Path(__file__), Path(original_source.__file__), a.elf,
                                   a.listing, system, *sources]]
    output.mkdir(parents=True)
    source_snapshot = output/'SOURCE.py'
    source_snapshot.write_bytes(Path(__file__).read_bytes())
    functions = output/'FUNCTIONS.json'
    functions.write_text(json.dumps(records, indent=2)+'\n', encoding='utf8', newline='\n')
    offset_candidates = []
    for pc, row in sorted(rows.items()):
        word = int(row['word'], 16)
        if not (0x210000 <= pc < 0x213000 or 0x2e0000 <= pc < 0x300000):
            continue
        if word >> 26 not in (0x23, 0x2b):
            continue
        offset = word & 65535
        if offset in (60, 72, 76, 524, 528, 636, 640, 700, 916):
            need(elf.word(pc) == word, 'offset_candidate_original_join_failed')
            offset_candidates.append(dict(**row, base_register=(word >> 21) & 31,
                                          field_offset=offset,
                                          binding='UNRESOLVED_BASE_TYPE_NOT_A_PROVED_OWNER_ALIAS'))
    result = dict(schema='r192_original_producer_lifecycle_v1', status='PASS_BOUNDED_SOURCE_AUDIT',
                  runtime_plan_lease_admission='OPEN', source_pins=source_pins,
                  anchors=[dict(pc=f'{pc:08X}', word=f'{word:08X}', listing=rows[pc]) for pc, word in words.items()],
                  installed_table=dict(va='0059FD90', words=[f'{v:08X}' for v in table],
                                       slot12_veneer='00210920', cleanup_body='00210780'),
                  checked_functions=len(records), checked_original_words=sum(r['declared_word_count'] for r in records),
                  functions=pin(functions), mutation_controls=controls,
                  tool_snapshot=pin(source_snapshot),
                  field_offset_candidates=offset_candidates,
                  chain=CHAIN, lease_requirements=LEASE_REQUIREMENTS,
                  limits=['Static source/expression audit, not behavioral differential or full CFG proof.',
                          'Direct callers are original main-ELF listing only; unresolved indirect and overlay writers remain open.',
                          'Generated owner counts are not the original sentinel attribute list.',
                          'No runtime changes, code execution, compilation, cache ingestion or ownership/credit promotion.'],
                  command=[sys.executable, str(Path(__file__).resolve()), *sys.argv[1:]])
    (output/'RESULT.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf8', newline='\n')
    need(all(pin(Path(p['path'])) == p for p in source_pins), 'input_changed_during_audit')
    print(json.dumps(dict(status=result['status'], runtime_plan_lease_admission='OPEN',
                          functions=len(records), words=result['checked_original_words'],
                          controls=len(controls), receipt=pin(output/'RESULT.json'))))


CHAIN = [
    dict(stage='produce', pcs=['002ED458','002ED460','002EDA1C'],
         fact='2ED3E0 calls 2ED4B0; returned word count captured in S6. Core records data-block word offset at descriptor+916. Wrapper scratch allocations are temporary, not a Plan lifetime.'),
    dict(stage='batch_temporary_code', pcs=['002FAEAC','002FAEB4','002FAEC0','002FAEC4'],
         fact='2FA9D0 builds selected temporary descriptors into global temporary code arena. Nonzero returned counts rounded to four words and accumulated; not every descriptor emits code.'),
    dict(stage='allocate_and_copy_final_code', pcs=['002FAF18','002FAF44','002FAF7C'],
         fact='2FAF18 calls 211C50 with out-cache pointer at SP+1500; final code destination is cache+700. 2FAF44 copies aggregate code bytes, after null guards.'),
    dict(stage='allocation_parentage', pcs=['002121FC','00212228','002E82C0'],
         fact='212150 stores allocated variable-layout backing block at cache+60, calls2E8190 on cache+64; owner+636 is therefore cache+700. Cleanup frees cache+60: pool and descriptor pointer table are views into this backing allocation, not independent host ownership.'),
    dict(stage='patch_temporary_descriptor', pcs=['002FAF64','002FAF88'],
         fact='Only nonzero length slots patch temporary descriptor+76 to final code pool plus accumulated padded lengths; descriptor stride928 is distinct from other896-byte records.'),
    dict(stage='copy_descriptors', pcs=['002FB070','002E8144','002E8158','002E815C'],
         fact='2E8110 requires owner+524 equal supplied count; destination fetched by 2E7F90 through owner+528 pointer table. Each memcpy copies928 bytes; mismatch silently skips copies.'),
    dict(stage='post_copy_bind', pcs=['002FB150','002E1EF4','002E1EFC','002E7D30'],
         fact='2E1EB0 iterates actual owner descriptor count; 2E7D30 writes descriptor+72 owner and translates effect/filter tag offsets using owner resources. Copy is not the last descriptor mutation.'),
    dict(stage='indirect_use', pcs=['002E612C','002E6164','002E6178','002E617C','002E6184','002E6DA0'],
         fact='Actual2E60E0 loads code pointer from +76(A0), not S3. At call JALR T4 delay computes T3=T4+4*descriptor+916; continuation stores V0 todescriptor+0 then restores caller fields. Lease must bind this actual descriptor/pool association and live consumer, not RA alone.'),
    dict(stage='publish_cache', pcs=['002FB2EC','002FB31C','002FB36C','002FB38C'],
         fact='Conditional cache publication uses pointer+identity pairs; subsequent896-byte resets are a separate record path. Exact insertion guards remain visible in original function evidence.'),
    dict(stage='cleanup_and_reuse', pcs=['00210920','00210780','00210898','002108AC','00212370'],
         fact='Installed table slot+12 jumps to210780; cleanup clears identity+4, drains owner+640 auxiliary list through2E8020, frees cache+60 via12D9C0 and performs further identity-checked indirect cleanup. Cache reuse consumers compare pointer identity words, not VA alone.'),
    dict(stage='restore', pcs=[],
         fact='Actual v8 machine_state restores full guest RAM and Cpu. Host Plan leases cannot survive this merely because guest pointer values reappear; global invalidation/re-authentication required.'),
]

LEASE_REQUIREMENTS = [
    dict(status='OPEN', requirement='Producer transaction identity and reentry/preempt ordering for global scratch arena and descriptor array; capture allocator metadata before wrapper scratch frees, and original sentinel list before the parent stack list expires.'),
    dict(status='OPEN', requirement='Preserve source success/failure/partial-emission/count mismatch branches. Attach token only after final code copy, pointer patch, descriptor copy and post-copy binding all succeed.'),
    dict(status='OPEN', requirement='Final pool and descriptor allocation bounds, aliases and ambient fourth-word snapshot time; owner attribute regeneration is not proof of original builder input list.'),
    dict(status='OPEN', requirement='Attach generation to cache pointer+identity, owner allocation, final pool range, descriptor slot and source Plan; invalidate on every mutation/free/reuse path including indirect callbacks.'),
    dict(status='OPEN', requirement='Close indirect cleanup callback targets and other raw memcpy/memmove/owner writer aliases across modules; bounded direct ELF inventory alone is incomplete.'),
    dict(status='OPEN', requirement='Restore/reset invalidates host leases. Saved caches need source-authenticated producer rehydration and exact expected-word comparison, never instruction-stream interpretation.'),
]


if __name__ == '__main__':
    main()
