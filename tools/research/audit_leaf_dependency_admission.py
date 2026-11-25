"""P3-prime leaf dependencies of the original admitted17, without promotion.

Applies the existing source/body/caller E1 or installed-class E4 evidence gates.
Eligibility is a review result, never an ownership/native-credit mutation.
"""
from collections import Counter, defaultdict
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from classify_stack_call_boundaries import reader, PINS
from verify_stack_exit_paths import audit_words

ADMITTED17 = {0x19f0b0, 0x19f1e0, 0x1a2020, 0x1a2bc0, 0x1a64a0,
              0x4b7f60, 0x4b7f90, 0x4b8010, 0x4b8060, 0x4b81e0,
              0x4baf10, 0x4bb210, 0x4bb3e0, 0x4e2e10, 0x4e2ee0, 0x4e2fd0, 0x50ccc0}
SDK_REVISION = '1a8f7da4c58f6725cb73fbe71a8803fce9e0c752'
SPEC = {
 0x1a3c40: {'role': '_rwResHeapClose', 'source': 'resmem.c', 'end': 0x1a3c48,
   'body_sha256': '5ce5ad86d452c4d2422bd63e15223d5d6b3dfb77f224a88c1f476e9fb34e359d',
   'signature': '_rwResHeapClose(void *resHeap __RWUNUSED__)',
   'source_operations': ['RWRETURN(TRUE);'],
   'caller_words': {0x1a2068: 0x0c068f10, 0x1a206c: 0x8e24000c},
   'ABI': {'parameters': 'Unused heap pointer in A0 low32; no dereference in retail body',
           'return': 'RwBool is RwInt32/int; retail V0 low64=sign_extend32(1), upper64 preserved',
           'effects': 'No memory writes or external calls; all registers except V0 preserved, JR entry RA with return-value delay',
           'native_type_proposal': 'int32_t close_heap(opaque typed guest-address heap) noexcept; no bool-width guess'},
   'first_blockers': ['Actual AST typed-return emission and full preserved-state comparator; no missing pointer/receiver binding is needed for this unused argument.']},
 0x1a3be0: {'role': '_rwResHeapInit', 'source': 'resmem.c', 'end': 0x1a3c34,
   'body_sha256': '13999ca70b4a7418437bdddbf105b9f9e1c651329ee6de7f0aebdcf269e414ee',
   'signature': '_rwResHeapInit(void *resHeap, RwUInt32 size)',
   'source_operations': ['blockSize < 32', 'firstBlock->heap = heapInfo', 'firstBlock->next',
                         'firstBlock->prev', 'firstBlock->flags', 'firstBlock->size = blockSize',
                         'heapInfo->firstBlock = firstBlock', 'heapInfo->firstFreeBlock = firstBlock'],
   'caller_words': {0x1a20d4: 0x0240282d, 0x1a20d8: 0x0c068ef8, 0x1a20dc: 0x0040202d},
   'ABI': {'parameters': 'A0 heap pointer, A1 unsigned32 size; source declares void*, RwUInt32',
           'return': 'RwBool/int32 result0 or1 in V0; signed block-size comparison is original',
           'effects': 'Failure has zero stores; success has seven ordered32-bit stores, first in taken BEQL delay',
           'native_type_proposal': 'int32_t init_heap(typed guest-memory access, opaque guest-address heap, uint32_t size)'},
   'first_blockers': ['Whole actual AST lowering of alignment arithmetic and branch-likely bundle; exact write-order/value comparator; legal heap extent/nonwrapping admission.']},
 0x1a7510: {'role': 'RwFrameUpdateObjects', 'source': 'baframe.c', 'end': 0x1a7578,
   'body_sha256': '624ddc6f40f31d001aa72a3b386aa2ce2754f08918b32646261f254348ac5906',
   'signature': 'RwFrameUpdateObjects(RwFrame *frame)',
   'source_operations': ['rwObjectGetPrivateFlags(frame->root)', 'rwLinkListAddLLLink',
                         'RWSRCGLOBAL(dirtyFrameList)', 'rwObjectSetPrivateFlags(frame->root',
                         'rwObjectSetPrivateFlags(frame,', 'RWRETURN(frame)'],
   'caller_words': {0x1a64e4: 0x8c840004, 0x1a64f0: 0x0c069d44, 0x1a64f4: 0},
   'ABI': {'parameters': 'A0 RwFrame guest address, source explicitly declares RwFrame*',
           'return': 'Same input frame address in V0 low64; untouched upper64 preserved',
           'effects': 'Conditional intrusive dirty-list writes and byte flags, including final SB in JR delay',
           'native_type_proposal': 'GuestAddress<RwFrame> update_objects(typed guest-memory access, GuestAddress<RwFrame>)'},
   'first_blockers': ['Complete pointer/list provenance and alias-safe admission; exact conditional word/byte write trace; actual whole AST lowering.']}}
SDK_HASH = {'resmem.c': 'bacc01b37e6c796117f8112d8cc758233710c58052fbd32adf325889358cbc69',
            'baframe.c': '47ee0bdf20bbae224bfaa0b1808ee8fa9d805d12d1e09ac382cd5c38f37e7536'}


def sha(raw): return hashlib.sha256(raw).hexdigest()
def hx(n): return f'{n:08x}'
def require(ok, reason):
    if not ok: raise ValueError(reason)
def table(raw): return csv.DictReader(io.StringIO(raw.decode('utf-8-sig')), delimiter='\t')


def validate_e1(entry, original, sources, headers):
    spec = SPEC[entry]; body = original(entry, spec['end'] - entry)
    require(sha(body) == spec['body_sha256'], 'E1_original_complete_body_changed')
    data = sources[spec['source']]
    require(sha(data) == SDK_HASH[spec['source']], 'E1_pinned_SDK_source_changed')
    signature = spec['signature'].encode()
    require(data.count(signature) == 1, 'E1_exact_SDK_signature_missing')
    block = data.split(signature, 1)[1].split(b'\n}', 1)[0]
    require(all(s.encode() in block for s in spec['source_operations']), 'E1_SDK_semantic_operation_missing')
    require(b'typedef int RwInt32;' in headers['ostypes.h'] and b'typedef RwInt32 RwBool;' in headers['ostypes.h'],
            'E1_RwBool_integer_return_convention_missing')
    for pc, expected in spec['caller_words'].items():
        require(original(pc, 4) == expected.to_bytes(4, 'little'), 'E1_original_caller_or_delay_changed')
    return {'status': 'E1_ELIGIBLE_PENDING_REVIEW', 'gate': 'Existing SDK source + complete original body + authenticated original callers',
            'cross_version_scope': 'Reference SDK3.1 identifies the source API; retail3.6 behavior and ABI are independently pinned by complete original words and authentic callers. SDK debug/MEMORYLOG behavior is not assumed present in retail.',
            'reference_role': spec['role'], 'original_body_sha256': sha(body),
            'source_signature': spec['signature'], 'SDK_source_sha256': sha(data), 'ABI': spec['ABI'],
            'caller_semantic_words': [{'pc': hx(p), 'word': hx(w)} for p, w in spec['caller_words'].items()],
            'native_credit_added': 0, 'ownership_status_automatically_changed': False}


def corpus_audit(parent, corpus):
    manifest_path = parent / 'launchers/oracle_original_manifest.json'
    raw = manifest_path.read_bytes(); manifest = json.loads(raw); pins = {}
    for name, pin in manifest.items():
        if not name.startswith('games/mkarmageddon/disc/states/'): continue
        path = corpus / name.rsplit('/', 1)[-1]
        require(path.stat().st_size == pin['size'] and sha(path.read_bytes()) == pin['sha256'], 'original_corpus_identity_changed')
        pins[path.name] = pin
    states = sorted(n for n in pins if n.endswith('.state'))
    sidecars = sorted(n for n in pins if n.endswith('.inputs'))
    require(len(states) == 61 and len(sidecars) == 72 and len(pins) == 133, 'exact61_states72_sidecars_required')
    observed = {p.name for p in corpus.iterdir() if p.is_file() and p.suffix in {'.state', '.inputs'}}
    require(observed == set(pins), 'actual_corpus_inventory_differs_from_pinned61_and72')
    rows = []; kinds = Counter()
    for name in sidecars:
        lines = (corpus / name).read_text().splitlines()
        require(lines[0] == 'ps2-inputs 1', 'original_input_sidecar_version_changed')
        start = next(line.split() for line in lines if line.startswith('start state '))
        require(start[2] in states, 'sidecar_start_state_missing')
        counts = Counter(line.split()[0] for line in lines if line.strip()); kinds.update(counts)
        rows.append({'name': name, 'sha256': pins[name]['sha256'], 'format': lines[0],
                     'start_state': start[2], 'start_frame': int(start[4]),
                     'same_stem_state': name[:-7] in states, 'record_counts': dict(counts)})
    parser = parent / 'external/forks/ps2-native-recomp/runtime/src/quick_keys.inc'
    source = parser.read_bytes()
    require(b'ps2-inputs 1' in source and b'h %llu %08x' in source and b'hash_bytes(g_cpu.r' in source,
            'recording_parser_checkpoint_semantics_changed')
    return {'manifest_sha256': sha(raw), 'state_count': 61, 'sidecar_count': 72,
            'same_stem_sidecars': sum(r['same_stem_state'] for r in rows),
            'run_sidecars': sum(not r['same_stem_state'] for r in rows),
            'unique_start_states': len({r['start_state'] for r in rows}), 'all_start_states_present': True,
            'record_kinds': dict(kinds), 'sidecars': rows, 'original_file_pins': pins,
            'parser_source_sha256': sha(source),
            'scope': 'Input events plus chained rotating RAM/register checkpoint hashes, not per-function call/argument/write records. Snapshot seeding does not prove historical execution or legal per-function ABI applicability.',
            'game_executed': False, 'inputs_replayed': False}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--parent-root', type=Path, required=True)
    p.add_argument('--sdk-root', type=Path, required=True)
    p.add_argument('--corpus', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    args = p.parse_args(); parent = args.parent_root.resolve(); fork = Path(__file__).resolve().parents[2]
    output = args.output.resolve()
    require(not output.exists() and output.is_relative_to(fork / 'build/oracle-telemetry'), 'new_ignored_output_directory_required')
    ledger_path = fork / 'research/ledger/functions.csv'; ledger_raw = ledger_path.read_bytes()
    ledger = {int(r['address'], 16): r for r in csv.DictReader(io.StringIO(ledger_raw.decode())) if r['module'] == 'SLUS_214.10'}
    require(all(ledger[a]['ownership_status'].startswith('verified-') for a in ADMITTED17), 'original_admitted17_status_changed')
    export = parent / 'research/runs/ghidra-function-coverage-20261118/export-20261118-230132-1875-18160'
    exports = {name: (export / name).read_bytes() for name in PINS}
    require({name: sha(data) for name, data in exports.items()} == PINS, 'original_export_pin_changed')
    image = parent / 'private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/SLUS_214.10'
    original = reader(image.read_bytes())
    instructions = list(table(exports['instructions.tsv'])); by_pc = {int(r['address'], 16): r for r in instructions}
    spans = defaultdict(list)
    for row in table(exports['function-bodies.tsv']):
        spans[int(row['entry'], 16)].append((int(row['start'], 16), int(row['end'], 16)))
    direct = defaultdict(list); indirect = []
    all_direct = defaultdict(list)
    for row in instructions:
        pc = int(row['address'], 16); w = int.from_bytes(bytes.fromhex(row['bytes']), 'little')
        owner = int(row['function_entry'], 16) if row['function_entry'] else None
        if w >> 26 == 3:
            target = ((pc + 4) & 0xf0000000) | ((w & 0x3ffffff) << 2)
            all_direct[target].append((pc, owner))
            if owner in ADMITTED17:
                require(original(pc, 4) == w.to_bytes(4, 'little'), 'admitted_caller_word_mismatch')
                direct[target].append((pc, owner))
        elif w >> 26 == 0 and w & 63 == 9 and owner in ADMITTED17: indirect.append(pc)
    sources = {name: (args.sdk_root / 'rwsdk/src' / name).read_bytes().replace(b'\r\n', b'\n') for name in SDK_HASH}
    headers = {'resmem.h': (args.sdk_root / 'rwsdk/src/resmem.h').read_bytes(),
               'baframe.h': (args.sdk_root / 'rwsdk/src/baframe.h').read_bytes(),
               'ostypes.h': (args.sdk_root / 'rwsdk/os/sky/ostypes.h').read_bytes()}
    require(b'RwBool   _rwResHeapClose(void *resHeap);' in headers['resmem.h'], 'SDK_heap_close_declaration_changed')
    revision = subprocess.run(['git', '-C', str(args.sdk_root.resolve()), 'rev-parse', 'HEAD'], capture_output=True, text=True, check=True).stdout.strip()
    require(revision == SDK_REVISION, 'SDK_revision_changed')
    scope_path = fork / 'research/runs/renderer-scope.json'; scope_raw = scope_path.read_bytes(); scope = json.loads(scope_raw)
    members = scope['bounded_census']['installed_tables']['game_members']
    dependencies = []; candidate_evidence = {}
    def body(entry):
        words = {}; pins = []
        for start, end in spans[entry]:
            raw = original(start, end - start + 1); pins.append({'start': hx(start), 'end': hx(end), 'sha256': sha(raw)})
            for pc in range(start, end + 1, 4):
                require(pc in by_pc and bytes.fromhex(by_pc[pc]['bytes']) == original(pc, 4), 'full_body_listing_join_mismatch')
                words[pc] = int.from_bytes(original(pc, 4), 'little')
        return words, pins
    for entry, sites in sorted(direct.items()):
        words, pins = body(entry); cfg = audit_words(words, entry)
        link_calls = [hx(pc) for pc, w in words.items() if w >> 26 == 3 or w >> 26 == 0 and w & 63 == 9
                      or w >> 26 == 1 and w >> 16 & 31 in {16, 17, 18, 19}]
        strict_leaf = not link_calls and cfg['local_exit_status'] == 'PROVED_LOCAL_EXIT_GEOMETRY'
        e4 = [r for r in members if int(r['method'], 16) == entry and r['promotion_eligible']]
        evidence = validate_e1(entry, original, sources, headers) if entry in SPEC and strict_leaf else None
        if evidence:
            witnesses = []
            for pc, owner in all_direct[entry]:
                caller_words, caller_pins = body(owner)
                require(pc in caller_words and pc + 4 in caller_words, 'caller_complete_bundle_missing')
                witnesses.append({'call_pc': hx(pc), 'caller_entry': hx(owner), 'caller_complete_body_spans': caller_pins,
                    'call_word': hx(caller_words[pc]), 'delay_pc': hx(pc + 4), 'delay_word': hx(caller_words[pc + 4]),
                    'listing_call': by_pc[pc], 'listing_delay': by_pc[pc + 4]})
            evidence['all_original_direct_callers'] = witnesses
            evidence['original_complete_body_words'] = [{'pc': hx(pc), 'word': hx(w)} for pc, w in sorted(words.items())]
            candidate_evidence[hx(entry)] = evidence
        gate = ('NOT_A_STRICT_LEAF' if not strict_leaf else 'E1_ELIGIBLE_PENDING_REVIEW' if evidence
                else 'EXISTING_E4_MEMBER_REQUIRES_REVIEW' if e4 else 'CANNOT_ADMIT_WITH_CURRENT_E1_E4_EVIDENCE')
        dependencies.append({'entry': hx(entry), 'current_ownership': ledger.get(entry, {}).get('ownership_status', 'NOT_IN_LEDGER'),
            'original_body_spans': pins, 'link_call_sites': link_calls, 'strict_local_return_leaf': strict_leaf,
            'original_CFG_status': cfg['local_exit_status'], 'unproved_exits': cfg['unresolved_edges'],
            'admitted_caller_sites': [{'pc': hx(pc), 'entry': hx(owner)} for pc, owner in sites],
            'admission_review': gate, 'existing_E4_membership_count': len(e4),
            'reason': 'Complete SDK source/body/caller witness verified; existing rules may admit after review.' if evidence else
                'A verified caller does not prove this helper belongs to that game class or SDK; no complete E1 source-semantic witness or eligible E4 installed membership is recorded.' if strict_leaf else
                'Link calls, unresolved indirect exit or out-of-body tail candidate prevents strict leaf selection.',
            'first_blockers': SPEC[entry]['first_blockers'] if evidence else
                ['Recover exact source/ABI ownership witness and legal caller data; do not infer ownership from call adjacency.'],
            'ownership_status_automatically_changed': False, 'native_credit_added': 0})
    controls = []
    for label, pc, replacement in [('wrong_return_value', 0x1a3c44, 0x24020000),
        ('wrong_return_target', 0x1a3c40, 0x00400008), ('wrong_caller_target', 0x1a2068, 0x0c068f14),
        ('wrong_delay_heap_field', 0x1a206c, 0x8e240010)]:
        def planted(at, size):
            data = bytearray(original(at, size)); payload = replacement.to_bytes(4, 'little')
            begin, end = max(at, pc), min(at + size, pc + 4)
            if begin < end: data[begin-at:end-at] = payload[begin-pc:end-pc]
            return bytes(data)
        try: validate_e1(0x1a3c40, planted, sources, headers)
        except ValueError as error: controls.append({'mutation': label, 'status': 'REJECTED', 'reason': str(error)})
        else: raise AssertionError('forged E1 admission accepted: ' + label)
    bad_sources = dict(sources); bad_sources['resmem.c'] = sources['resmem.c'].replace(b'RwBool\n_rwResHeapClose', b'void\n_rwResHeapClose')
    try: validate_e1(0x1a3c40, original, bad_sources, headers)
    except ValueError as error: controls.append({'mutation': 'forged_SDK_return_type', 'status': 'REJECTED', 'reason': str(error)})
    else: raise AssertionError('forged SDK signature admitted')
    require(next(r for r in dependencies if r['entry'] == '004bd220')['admission_review'] == 'CANNOT_ADMIT_WITH_CURRENT_E1_E4_EVIDENCE',
            'game_caller_only_incorrectly_promotes_helper')
    controls.append({'mutation': 'game_caller_only_promotes_uninstalled_helper', 'status': 'REJECTED_BY_GATE'})
    corpus = corpus_audit(parent, args.corpus)
    report = {'schema': 'admitted17_leaf_dependency_admission_review_v1', 'status': 'REVIEW_ONLY_NO_PROMOTION',
        'input_pins': {'ledger_sha256': sha(ledger_raw), 'renderer_scope_sha256': sha(scope_raw), 'export_sha256': PINS,
            'SDK_revision': revision, 'SDK_sources_sha256': {n: sha(s) for n, s in sources.items()},
            'SDK_headers_sha256': {n: sha(s) for n, s in headers.items()}, 'tool_sha256': sha(Path(__file__).read_bytes())},
        'fixed_original_admitted17': [hx(a) for a in sorted(ADMITTED17)],
        'summary': {'admitted_function_cohort': 17, 'direct_sites': sum(map(len, direct.values())),
            'direct_dependencies': len(direct), 'indirect_sites': len(indirect),
            'no_link_call_dependencies': sum(not d['link_call_sites'] for d in dependencies),
            'strict_leaf_dependencies': sum(d['strict_local_return_leaf'] for d in dependencies),
            'admission_review_counts': dict(Counter(d['admission_review'] for d in dependencies)),
            'new_ownership_admissions': 0, 'new_native_credit': 0},
        'indirect_sites': [hx(a) for a in indirect], 'dependencies': dependencies,
        'eligible_E1_candidate_evidence': candidate_evidence, 'mutation_controls': controls, 'corpus': corpus,
        'limits': 'Eligibility does not change the current bounded ownership roster. Capability-only probes earn no ownership or credit. No Stage A code, typed ABI acceptance, state invocation or runtime replay was performed.'}
    output.mkdir(); path = output / 'RESULT.json'
    path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'receipt': str(path), 'sha256': sha(path.read_bytes()), 'summary': report['summary']}))


if __name__ == '__main__': main()
