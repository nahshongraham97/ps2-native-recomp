"""N6 bounded original-machine return proof, separate from native typed ABI.

Offline abstract proof only. Optional execution calls actual compiled AOT; it
does not execute this abstract checker as a game/runtime compatibility route.
"""
from __future__ import annotations
import argparse
import copy
import csv
import gzip
import hashlib
import io
import json
import os
from pathlib import Path
import re
import subprocess
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from classify_stack_call_boundaries import reader, PINS
from verify_stack_exit_paths import decode as independent_control

RANK_SHA = '9bc6f3b85d70300f412da04d7662e87c28d9389eb2712e792ace48a499ba4ba3'
L6_SHA = '93760a5dcaf4e955e20814e6c173cc7ac7cd4e4312142ef5d9f192f88baa5b1a'
PARENT = 0x125370
HELPERS = {0x1250b8, 0x125148, 0x124fe0}
CONTEXT = 0x1ddf98
RAM_SIZE = 32 * 1024 * 1024


def sha(raw): return hashlib.sha256(raw).hexdigest()
def hx(n): return f'{n:08x}'
def simm(w): return (w & 65535) - (65536 if w & 32768 else 0)
def require(ok, reason):
    if not ok: raise ValueError(reason)


def flow(pc, w):
    """Independent normal-control decode; no general instruction execution."""
    op, rs, rt, fn = w >> 26, w >> 21 & 31, w >> 16 & 31, w & 63
    if op == 3: return ('call', (((pc + 4) & 0xf0000000) | ((w & 0x3ffffff) << 2),), False)
    if op == 2: return ('jump', (((pc + 4) & 0xf0000000) | ((w & 0x3ffffff) << 2),), False)
    if op == 0 and fn == 8: return ('return' if rs == 31 else 'indirect_exit', (), False)
    if op == 0 and fn == 9: return ('indirect_call', (), False)
    if op in {4, 5, 6, 7, 20, 21, 22, 23} or op == 1:
        require(op != 1 or rt in {0, 1, 2, 3}, 'unproved_REGIMM_link_or_trap')
        likely = op in {20, 21, 22, 23} or op == 1 and rt in {2, 3}
        target = pc + 4 + 4 * simm(w); targets = (target, pc + 8)
        if op in {4, 20} and rs == rt: targets = (target,)
        if op in {5, 21} and rs == rt: targets = (pc + 8,)
        return ('branch', targets, likely)
    return None


def writes(w):
    op, rt, rd, fn = w >> 26, w >> 16 & 31, w >> 11 & 31, w & 63
    if op == 0:
        if fn == 8: return set()
        require(fn in {0, 2, 3, 4, 6, 7, 10, 11, 16, 18, 20, 22, 23,
                       33, 35, 36, 37, 38, 39, 42, 43, 45, 47, 56, 58, 59, 60, 62, 63},
                'unsupported_GPR_write_semantics')
        return {rd} - {0}
    if op in {9, 10, 11, 12, 13, 14, 15, 25, 35, 39, 55, 30}: return {rt} - {0}
    if op == 0x31: return set()  # LWC1 writes an FPR, never the same-numbered GPR.
    if op == 3: return {31}
    require(op in {1, 2, 4, 5, 6, 7, 20, 21, 22, 23, 43, 63, 31}, 'unsupported_instruction_family')
    return set()


def prove_helper(entry, words):
    queue = [entry]; visited = set(); returns = []; edges = []; stores = []; changed = set()
    while queue:
        pc = queue.pop()
        if pc in visited: continue
        require(pc in words, 'helper_CFG_gap_or_external_exit'); visited.add(pc)
        transfer = flow(pc, words[pc]); check = independent_control(pc, words[pc])
        require(bool(transfer) == bool(check), 'independent_control_decode_disagreement')
        if transfer:
            require(transfer[0] == check['kind'], 'independent_control_kind_disagreement')
            if transfer[0] == 'branch':
                require(set(transfer[1]) <= {check['target'], pc + 8} and transfer[2] == check['likely'],
                        'independent_branch_delay_or_target_disagreement')
        changed |= writes(words[pc])
        if transfer:
            kind, targets, likely = transfer
            require(kind not in {'call', 'indirect_call', 'indirect_exit'}, 'helper_has_unproved_call_or_exit')
            require(pc + 4 in words and flow(pc + 4, words[pc + 4]) is None, 'unsafe_helper_delay_bundle')
            visited.add(pc + 4); changed |= writes(words[pc + 4])
            if kind == 'return':
                returns.append({'pc': hx(pc), 'word': hx(words[pc]), 'delay_pc': hx(pc + 4), 'delay_word': hx(words[pc + 4])})
            else:
                for target in targets:
                    require(target in words, 'helper_CFG_gap_or_external_exit')
                    edges.append({'pc': hx(pc), 'target': hx(target),
                                  'delay': 'ANNULLED' if likely and target == pc + 8 else 'EXECUTED'})
                    queue.append(target)
        else: queue.append(pc + 4)
    require(bool(returns), 'helper_has_no_normal_return')
    require(not changed & {16, 29, 31}, 'helper_modifies_S0_SP_or_RA_including_context_restore')
    for pc in sorted(visited):
        w = words[pc]; op = w >> 26
        if op not in {43, 63, 31}: continue
        base = w >> 21 & 31
        require(base not in changed, 'helper_store_base_is_not_preserved')
        stores.append({'pc': hx(pc), 'word': hx(w), 'base_register': base,
                       'offset': simm(w), 'width_bytes': {43: 4, 63: 8, 31: 16}[op]})
    return {'entry': hx(entry), 'words_sha256': sha(b''.join(words[p].to_bytes(4, 'little') for p in sorted(words))),
            'changed_GPRs': sorted(changed), 'preserved_all128_SP_RA_S0': True,
            'normal_returns': returns, 'edges': edges, 'memory_stores': stores,
            'proof': 'Every reachable original instruction has a decoded write set excluding SP/RA/S0; every exit is JR entry-RA with a complete delay. Store bases are unchanged argument registers.',
            'termination': 'Not proved; contract is for normal completion, not liveness', 'native_typed_ABI': 'UNPROVED'}


def frame_preconditions(sp, ra=0x100360):
    return (0 <= sp <= 0xffffffff and sp % 16 == 0 and 128 <= sp <= RAM_SIZE
            and 0 <= ra <= 0xffffffff and ra % 4 == 0)


def prove_parent(words, helper_words, contracts):
    regs = {r: ('input', r) for r in range(32)}; regs[0] = ('constant', 0); regs[29] = ('SP', 0)
    memory = {}; calls = []; all_stores = []; pc = PARENT
    def store(at, width, token, source):
        require(-128 <= at and at + width <= 0, 'parent_store_outside_bounded_frame')
        for key in list(memory):
            if max(key[0], at) < min(key[0] + key[1], at + width): del memory[key]
        memory[(at, width)] = token; all_stores.append({'pc': hx(source), 'offset_from_frame': at + 128, 'width': width})
    def execute(at):
        w = words[at]; op, rs, rt, rd, fn = w >> 26, w >> 21 & 31, w >> 16 & 31, w >> 11 & 31, w & 63
        if w == 0: return
        if op == 9:
            regs[rt] = ('SP', regs[rs][1] + simm(w)) if regs[rs][0] == 'SP' else ('unknown',)
        elif op == 0 and fn == 45 and (rs == 0 or rt == 0): regs[rd] = regs[rt if rs == 0 else rs]
        elif op in {63, 55}:
            require(regs[rs][0] == 'SP', 'unproved_parent_memory_base')
            address = regs[rs][1] + simm(w)
            if op == 63: store(address, 8, regs[rt], at)
            else:
                require((address, 8) in memory, 'parent_restore_has_no_retained_original_value')
                regs[rt] = memory[(address, 8)]
        else: raise ValueError('parent_statement_outside_bounded_proof')
    while True:
        require(pc in words, 'parent_body_gap')
        w = words[pc]; transfer = flow(pc, w)
        if transfer and transfer[0] == 'call':
            target = transfer[1][0]
            require(target in HELPERS and target in contracts, 'unproved_callee_or_context_masquerading_normal')
            actual = prove_helper(target, helper_words[target])
            require(contracts[target] == actual and actual['entry'] == hx(target), 'callee_contract_not_rederived_from_original')
            regs[31] = ('link', pc + 8); execute(pc + 4)
            writes_here = []
            for footprint in actual['memory_stores']:
                base = regs[footprint['base_register']]
                require(base[0] == 'SP', 'callee_store_argument_not_proved_frame_relative')
                address = base[1] + footprint['offset']; end = address + footprint['width_bytes']
                require(-128 <= address and end <= -40, 'callee_store_overlaps_saved_RA_S0_or_input_region')
                store(address, footprint['width_bytes'], ('helper_output',), int(footprint['pc'], 16))
                writes_here.append({'offset_from_frame': address + 128, 'width': footprint['width_bytes']})
            # Only freshly decoded clobbers are forgotten; SP/RA/S0 are proved
            # preserved by all original paths, not assumed from a library name.
            for reg in actual['changed_GPRs']: regs[reg] = ('unknown',)
            require(regs[31] == ('link', pc + 8), 'callee_did_not_preserve_actual_link')
            calls.append({'pc': hx(pc), 'word': hx(w), 'delay_pc': hx(pc + 4), 'delay_word': hx(words[pc + 4]),
                          'target': hx(target), 'continuation': hx(pc + 8), 'SP_offset': regs[29][1],
                          'helper_contract_sha256': sha(json.dumps(actual, sort_keys=True).encode()), 'writes': writes_here})
            pc += 8; continue
        if transfer:
            require(transfer[0] == 'return', 'unproved_parent_alternate_control')
            captured = regs[31]; execute(pc + 4)
            require(captured == ('input', 31), 'return_target_is_not_original_entry_RA')
            require(regs[31] == ('input', 31), 'delay_clobbered_restored_RA')
            require(regs[29] == ('SP', 0), 'return_SP_not_restored_including_delay')
            require(regs[16] == ('input', 16), 'S0_low64_not_restored')
            return {'entry': hx(PARENT), 'status': 'PROVED_BOUNDED_MACHINE_NORMAL_RETURN',
                    'SP_and_RA': 'Original entry values restored including upper-lane preservation; JR target captured before original delay',
                    'normal_return': {'pc': hx(pc), 'word': hx(w), 'delay_pc': hx(pc + 4), 'delay_word': hx(words[pc + 4])},
                    'internal_calls': calls, 'stores': all_stores,
                    'preconditions': ['Canonical sign-extended32-bit entry SP, 16-byte aligned',
                        'Nonwrapping writable original128-byte frame:128<=SP<=33554432',
                        'Readable original data and stack-local inputs; normal execution excludes memory faults',
                        'Aligned original return link; normal completion only, no termination guarantee'],
                    'native_typed_ABI': 'UNPROVED', 'full_function_E2': 'NOT_CLAIMED'}
        execute(pc); pc += 4


def mutation_tests(bodies, contracts):
    results = []
    cases = [('wrong_RA_restore', PARENT, 0x1253bc, 0xdfbf0070),
             ('wrong_SP_restore', PARENT, 0x1253c4, 0x27bd0070),
             ('wrong_JR_delay', PARENT, 0x1253c4, 0),
             ('helper_store_overlaps_RA', 0x1250b8, 0x1250d0, 0xacad0078),
             ('wrong_helper_RA_return', 0x1250b8, 0x12510c, 0x00400008),
             ('context_masquerading_normal', PARENT, 0x125388, 0x0c0777e6)]
    for label, entry, pc, replacement in cases:
        mutated = copy.deepcopy(bodies); original = mutated[entry][pc]; mutated[entry][pc] = replacement
        try:
            cs = {a: prove_helper(a, mutated[a]) for a in HELPERS}
            prove_parent(mutated[PARENT], mutated, cs)
        except ValueError as error:
            results.append({'mutation': label, 'pc': hx(pc), 'original_word': hx(original),
                            'mutant_word': hx(replacement), 'status': 'REJECTED', 'reason': str(error)})
        else: raise AssertionError('unsafe original-word mutation admitted: ' + label)
    forged = copy.deepcopy(contracts); forged[0x1250b8] = forged[0x125148]
    try: prove_parent(bodies[PARENT], bodies, forged)
    except ValueError as error: results.append({'mutation': 'wrong_callee_contract', 'status': 'REJECTED', 'reason': str(error)})
    else: raise AssertionError('forged callee contract admitted')
    try: prove_helper(CONTEXT, bodies[CONTEXT])
    except ValueError as error:
        require(str(error) == 'helper_modifies_S0_SP_or_RA_including_context_restore', 'context_control_did_not_reach_register_restore_proof')
        results.append({'mutation': 'actual_context_restorer_as_ordinary_leaf', 'status': 'REJECTED', 'reason': str(error)})
    else: raise AssertionError('context restorer admitted as ordinary return')
    invalid = [0, 127, 129, RAM_SIZE + 16, 0x100000000]
    require(all(not frame_preconditions(x) for x in invalid), 'invalid_frame_preconditions_admitted')
    results.append({'mutation': 'invalid_nonwrapping_frame_preconditions', 'values': invalid, 'status': 'REJECTED'})
    return results


DRIVER = r'''// TEST ONLY: execute actual unchanged compiled original AOT.
#define main phase4_main_not_used_by_machine_return_check
#include "test_phase4_aot_equivalence.cpp"
#undef main
#include "units.hpp"
namespace {
const char* plant = "";
unsigned cases = 0;
void machine_check(bool ok, const char* reason) {
    if (!ok) { std::fprintf(stderr,"FAIL_MACHINE_RETURN_AOT case=%u reason=%s\n",cases,reason);std::exit(1); }
}
void execute(const char* kind) {
    const auto before=ps2::g_cpu;
    const auto sp=before.r[29].w[0];
    machine_check(before.r[29].d[0]==sp && sp>=128 && sp<=ps2::kRamSize && !(sp&15u),"invalid nonwrapping canonical frame precondition");
    machine_check(!(before.r[31].w[0]&3u),"unaligned original return link");
    const std::vector<std::uint8_t> ram(ps2::g_ram,ps2::g_ram+ps2::kRamSize);
    ps2::g_cpu.pc=0x00125370u;ps2::g_cpu.budget=1000000;
    gen::unit_0009(ps2::g_cpu,49u,0u); // All four actual calls remain in original unit0009.
    if (cases==0) {
        if (std::strcmp(plant,"--planted-ra-result")==0)ps2::g_cpu.r[31].d[0]^=4u;
        if (std::strcmp(plant,"--planted-sp-result")==0)ps2::g_cpu.r[29].d[0]^=16u;
        if (std::strcmp(plant,"--planted-continuation-result")==0)ps2::g_cpu.pc^=4u;
    }
    machine_check(std::memcmp(&before.r[29],&ps2::g_cpu.r[29],sizeof(ps2::Reg128))==0,"SP differs from retained original entry SP");
    machine_check(std::memcmp(&before.r[31],&ps2::g_cpu.r[31],sizeof(ps2::Reg128))==0,"RA differs from retained original entry RA");
    machine_check(std::memcmp(&before.r[16],&ps2::g_cpu.r[16],sizeof(ps2::Reg128))==0,"S0 differs from retained original entry S0");
    machine_check(ps2::g_cpu.pc==before.r[31].w[0],"continuation differs from retained original entry RA target");
    const auto base=sp-128;
    const std::array<std::pair<unsigned,unsigned>,4> allowed={{{base,base+24},{base+32,base+56},{base+64,base+88},{base+96,sp}}};
    unsigned cursor=0;
    for (const auto [start,end]:allowed) {
        machine_check(std::memcmp(ram.data()+cursor,ps2::g_ram+cursor,start-cursor)==0,"RAM write escaped proved frame footprint");
        cursor=end;
    }
    machine_check(std::memcmp(ram.data()+cursor,ps2::g_ram+cursor,ps2::kRamSize-cursor)==0,"RAM write escaped proved frame footprint");
    machine_check(std::memcmp(ps2::g_ram+base+120,&before.r[31].d[0],8)==0,"saved original RA bytes were overwritten");
    std::printf("MACHINE_RETURN_AOT case=%u kind=%s SP=%08X RA=%08X frame_bytes=128 status=PASS whole_function_E2=NOT_CLAIMED\n",cases,kind,sp,before.r[31].w[0]);
    ++cases;
}
}
int main(int argc,char** argv) {
    if(argc<2||argc>3)return 2;
    if(argc==3)plant=argv[2];
    std::vector<std::filesystem::path> files;
    for(const auto& f:std::filesystem::directory_iterator(argv[1]))
        if(f.is_regular_file()&&f.path().extension()==".state")files.push_back(f.path());
    std::sort(files.begin(),files.end());machine_check(files.size()==61,"exact61 captured inputs required");
    for(const auto& path:files){OracleVuLayout layout;std::string reason;
        machine_check(load_oracle_ee_vu(read_binary(path),layout,reason),"original snapshot ABI admission");execute("captured_snapshot_seed");}
    OracleVuLayout layout;std::string reason;
    machine_check(load_oracle_ee_vu(read_binary(files[0]),layout,reason),"constructed seed snapshot admission");
    const auto seed=ps2::g_cpu;const std::vector<std::uint8_t> ram(ps2::g_ram,ps2::g_ram+ps2::kRamSize);
    const std::array<std::uint64_t,9> patterns={0ull,1ull,0x8000000000000000ull,0xffffffffffffffffull,
        0x3ff0000000000000ull,0x7ff0000000000000ull,0xfff0000000000000ull,0x7ff8000000000001ull,0x0010000000000000ull};
    for(auto a:patterns)for(auto b:patterns){ps2::g_cpu=seed;std::memcpy(ps2::g_ram,ram.data(),ram.size());
        ps2::g_cpu.r[4].d[0]=a;ps2::g_cpu.r[5].d[0]=b;execute("constructed_raw_bit_inputs");}
    machine_check(cases==142,"61 captured plus81 constructed cases required");
    std::printf("PASS_MACHINE_RETURN_AOT captured_states=61 constructed=81 total=142 whole_function_E2=NOT_CLAIMED native_ABI=UNPROVED\n");
}
'''


def runtime_check(args, output, fork):
    require(all(x is not None for x in (args.compiler, args.aot_library, args.states, args.input_manifest)),
            'compiled_crosscheck_requires_compiler_library_states_manifest')
    manifest = json.loads(args.input_manifest.read_bytes())
    state_pins = {name.rsplit('/', 1)[-1]: pin for name, pin in manifest.items()
                  if name.startswith('games/mkarmageddon/disc/states/') and name.endswith('.state')}
    require(len(state_pins) == 61, 'exact61_state_manifest_required')
    def verify_states():
        for name, pin in state_pins.items():
            path = args.states / name
            require(path.stat().st_size == pin['size'] and sha(path.read_bytes()) == pin['sha256'], 'original_state_identity_changed')
    verify_states()
    source = output / 'actual_original_AOT_machine_return.cpp'; source.write_text(DRIVER.replace('return2;', 'return 2;'), encoding='utf-8', newline='\n')
    binary = output / 'actual_original_AOT_machine_return.exe'; temporary = output / 'compiler-temp'; temporary.mkdir()
    environment = dict(os.environ, TEMP=str(temporary), TMP=str(temporary))
    backing = ['build/generated-aot/vu1/vu1_table.cpp', 'runtime/src/dispatch.cpp', 'runtime/src/observer_hook.cpp',
               'runtime/src/native_hook.cpp', 'runtime/src/cop.cpp', 'runtime/src/vu_micro.cpp']
    files = [args.aot_library, args.compiler, fork / 'tests/test_phase4_aot_equivalence.cpp', fork / 'runtime/include/ps2/generated.hpp'] + [fork / p for p in backing]
    pins = {str(p): sha(p.read_bytes()) for p in files}
    command = [str(args.compiler.resolve()), '-std=c++20', '-O1', '-static', '-pthread',
               '-I' + str(fork / 'tests'), '-I' + str(fork / 'runtime/include'), '-I' + str(fork / 'runtime/src'),
               '-I' + str(args.generated_dir.resolve()), str(source), *[str(fork / p) for p in backing],
               str(args.aot_library.resolve()), '-o', str(binary)]
    built = subprocess.run(command, cwd=fork, env=environment, capture_output=True, timeout=180)
    (output / 'compile.log').write_bytes(built.stdout + built.stderr)
    require(built.returncode == 0, 'compiled_original_reference_build_failed')
    checks = []
    for name, option, code, expected in [('positive', None, 0, 'PASS_MACHINE_RETURN_AOT captured_states=61 constructed=81 total=142'),
        ('wrong_RA_result', '--planted-ra-result', 1, 'RA differs from retained original entry RA'),
        ('wrong_SP_result', '--planted-sp-result', 1, 'SP differs from retained original entry SP'),
        ('wrong_continuation_result', '--planted-continuation-result', 1, 'continuation differs from retained original entry RA target')]:
        argv = [str(binary), str(args.states.resolve())] + ([option] if option else [])
        ran = subprocess.run(argv, cwd=fork, env=environment, capture_output=True, timeout=180)
        log = ran.stdout + ran.stderr; (output / (name + '.log.gz')).write_bytes(gzip.compress(log, mtime=0))
        require(ran.returncode == code and expected in log.decode(errors='replace'), 'compiled_original_reference_' + name + '_failed')
        checks.append({'name': name, 'argv': argv, 'exit': ran.returncode, 'expected_exit': code,
                       'status': 'PASS', 'log_sha256': sha(log)})
    verify_states(); require(pins == {str(p): sha(p.read_bytes()) for p in files}, 'compiled_reference_inputs_changed')
    return {'status': 'PASS_MACHINE_RETURN_AND_FOOTPRINT_ONLY', 'captured_snapshot_seeds': 61,
        'constructed_raw_bit_pairs': 81, 'checks': checks, 'driver_sha256': sha(source.read_bytes()),
        'binary_sha256': sha(binary.read_bytes()), 'compile_argv': command, 'build_input_pins': pins,
        'original_states_authenticated_before_after': True,
        'limits': 'Actual unchanged AOT unit0009 executed. Only SP/RA/S0/continuation and RAM footprint checked; no result semantics, full-function E2, historical invocation or native ABI claim.'}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('ranking-report', 'l6-report', 'original-elf', 'export-dir', 'generated-dir', 'output'):
        p.add_argument('--' + name, type=Path, required=True)
    p.add_argument('--run-compiled-original', action='store_true')
    for name in ('compiler', 'aot-library', 'states', 'input-manifest'): p.add_argument('--' + name, type=Path)
    args = p.parse_args(); fork = Path(__file__).resolve().parents[2]; output = args.output.resolve()
    require(not output.exists() and output.is_relative_to(fork / 'build/oracle-telemetry'), 'new_ignored_output_required')
    rank_raw = args.ranking_report.read_bytes(); l6_raw = args.l6_report.read_bytes()
    require(sha(rank_raw) == RANK_SHA and sha(l6_raw) == L6_SHA, 'approved_inventory_pin_changed')
    ranking = json.loads(rank_raw); inventory = json.loads(l6_raw); original = reader(args.original_elf.read_bytes())
    selected = [s for s in inventory['sites'] if s['target'] == hx(PARENT)]
    require(len(selected) == 1 and selected[0]['pc'] == '00100358', 'selected101_site_binding_changed')
    raw = {name: (args.export_dir / name).read_bytes() for name in PINS}
    require({name: sha(data) for name, data in raw.items()} == PINS, 'original_listing_pin_changed')
    wanted = HELPERS | {PARENT, CONTEXT}; spans = {a: [] for a in wanted}; bodies = {a: {} for a in wanted}
    for row in csv.DictReader(io.StringIO(raw['function-bodies.tsv'].decode()), delimiter='\t'):
        entry = int(row['entry'], 16)
        if entry not in wanted: continue
        start, end = int(row['start'], 16), int(row['end'], 16); data = original(start, end - start + 1)
        spans[entry].append({'start': hx(start), 'end': hx(end), 'sha256': sha(data)})
        for pc in range(start, end + 1, 4): bodies[entry][pc] = int.from_bytes(original(pc, 4), 'little')
    required = {pc for words in bodies.values() for pc in words} | {0x100358, 0x10035c}
    listing = {}
    for row in csv.DictReader(io.StringIO(raw['instructions.tsv'].decode()), delimiter='\t'):
        pc = int(row['address'], 16)
        if pc in required:
            require(bytes.fromhex(row['bytes']) == original(pc, 4), 'listing_original_word_mismatch'); listing[pc] = row
    require(required <= listing.keys(), 'missing_pinned_listing_instruction')
    units = {}; fragments = {}; source_pins = inventory['input_pins']['used_generated_units_sha256']
    for name in ('unit_0000.cpp', 'unit_0009.cpp', 'unit_0037.cpp'):
        data = (args.generated_dir / name).read_bytes(); units[name] = sha(data)
        # Unit0037 is already pinned in L6 context provenance; all other units
        # must also be present in that measured source manifest.
        require(source_pins[name] == sha(data), 'measured_AOT_unit_changed')
        text = data.decode()
        for pc in required:
            marker = f'L_{pc:08X}:'
            if marker in text:
                fragments[pc] = {'unit': name, 'source_sha256': sha(data),
                    'fragment': marker + text.split(marker, 1)[1].split('\nL_', 1)[0]}
    require(required <= fragments.keys(), 'missing_measured_AOT_PC')
    contracts = {a: prove_helper(a, bodies[a]) for a in HELPERS}
    proof = prove_parent(bodies[PARENT], bodies, contracts); controls = mutation_tests(bodies, contracts)
    evidence = [{'entry': hx(entry), 'body_spans': spans[entry],
                 'original_listing_AOT_joins': [{'pc': hx(pc), 'word': hx(word), 'listing': listing[pc],
                     'generated': fragments[pc]} for pc, word in sorted(bodies[entry].items())]}
                for entry in sorted(wanted)]
    report = {'schema': 'bounded_machine_call_return_contract_discharge_v1',
        'status': 'PROVED_ONE_SELECTED_MACHINE_SUBOBLIGATION_NATIVE_ABI_UNPROVED',
        'input_pins': {'ranking_sha256': RANK_SHA, 'L6_sha256': L6_SHA, 'exports': PINS,
            'original_ELF_sha256': sha(args.original_elf.read_bytes()), 'measured_AOT_units': units,
            'tool_sha256': sha(Path(__file__).read_bytes())},
        'selected_site': selected[0], 'machine_proof': proof, 'helper_contracts': list(contracts.values()),
        'discharged_subobligations': [{'selected_site': '00100358', 'target': '00125370',
            'layer': 'Bounded original-machine normal-completion SP/RA/continuation only',
            'status': 'PROVED_UNDER_EXPLICIT_FRAME_PRECONDITIONS', 'native_typed_ABI': 'UNPROVED',
            'source_inventory_and_existing_consumers_changed': False}],
        'remaining_selected_sites': [{'pc': s['pc'], 'target': s['target'], 'kind': s['kind'],
            'machine_status': 'ORDINARY_CONTRACT_INAPPLICABLE_CONTEXT_RESTORE' if s['target'] == hx(CONTEXT) else 'OPEN_UNPROVED'}
            for s in inventory['sites'] if s['pc'] != '00100358'],
        'mutation_tests': controls, 'original_evidence': evidence,
        'counts': {'selected_site_denominator': 101, 'largest_class_site_denominator': 45,
            'selected_machine_return_subobligations_discharged': 1, 'selected_machine_subobligations_remaining': 100,
            'selected_direct_targets_proved': 1, 'supporting_helpers': 3, 'internal_call_boundaries_proved': 4,
            'native_typed_ABI_proved': 0, 'native_typed_ABI_remaining': 101, 'native_credit': 0},
        'context_restorer': '001ddf98 at selected JAL00122668 remains ordinary-contract INAPPLICABLE/UNPROVED',
        'existing_static_exit_consumer': 'Unchanged and still UNPROVED; this new proof is not imported into existing consumers.',
        'runtime_crosscheck': 'NOT_RUN_YET', 'ownership_changes': False, 'runtime_changes': False}
    output.mkdir(); path = output / 'RESULT.json'
    if args.run_compiled_original: report['runtime_crosscheck'] = runtime_check(args, output, fork)
    path.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'receipt': str(path), 'sha256': sha(path.read_bytes()), 'counts': report['counts']}))


if __name__ == '__main__': main()
