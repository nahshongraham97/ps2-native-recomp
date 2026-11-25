"""Capability-only Stage A experiment for a source-proved constant-return leaf.

Consumes the real generated C++ AST, not an ELF-to-C++ implementation template.
The existing general transformer and its admission/credit rules are unchanged.
An unused typed source reference remains an explicit intermediate ABI parameter.
"""
from __future__ import annotations

import argparse
import copy
import csv
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

import transform_generated_units as transform
from classify_stack_call_boundaries import PINS, reader


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def ownership_gate(row, body_hash):
    transform.require(row['module']=='SLUS_214.10' and row['address']=='001a3c40'
                      and row['ownership_status']=='verified-renderware'
                      and 'E1' in row['ownership_evidence_kinds'].split('|')
                      and 'sha256='+body_hash in row['ownership_evidence_ref'],
                      'stage_A_required_ownership_admission_missing')


class ConstantReturnABI(transform.Lowering):
    """Separate ABI experiment: a proved return needs no fabricated owner store."""
    def statement(self, node):
        if node.get('kind') != 'ReturnStmt':
            return super().statement(node)
        transform.require(not transform.children(node) and self.return_guard
                          and self.cursor_return, 'stage_A_unproved_return_bundle')
        transform.require(set(self.regs) == {2}, 'stage_A_unproved_liveout_set')
        transform.require(len(self.values) == 1 and len(self.effects) == 1
                          and self.effects[0]['kind'] == 'define',
                          'stage_A_nonconstant_or_memory_effect')
        self.effects.append({'kind': 'return_value', 'value': self.regs[2]})
        self.returned = True


def lower(definition, descriptor, contract):
    ir = ConstantReturnABI(definition, descriptor, contract).lower()
    transform.require(ir['host_budget_debit'] == 2, 'stage_A_wrong_frame_or_accounting')
    value = ir['values'][0]['constant_value']
    word = value & 0xffffffff
    signed = word if word < 0x80000000 else word - 0x100000000
    transform.require(value == signed & 0xffffffffffffffff,
                      'stage_A_return_width_not_sign_extended_32')
    return ir, signed


def ast_controls(definition, descriptor, contract):
    def target(tree, predicate):
        return next(n for n in transform.walk(tree) if predicate(n))
    def delay(tree):
        body = next(n for n in transform.children(tree) if n.get('kind') == 'CompoundStmt')
        nodes = transform.children(body)
        position = next(i for i,n in enumerate(nodes) if n.get('kind') == 'LabelStmt'
                        and n.get('name') == 'L_001A3C40')
        return target(nodes[position+1], lambda n: n.get('kind') == 'CallExpr'
                      and transform.declaration(transform.children(n)[0]).get('name') == 'set32')
    plants = []
    for name in ['invented_input', 'wrong_return_register', 'dropped_delay', 'wrong_frame']:
        tree = copy.deepcopy(definition)
        call = delay(tree)
        if name == 'invented_input':
            # A read of an incoming register cannot be substituted for proved zero.
            source = copy.deepcopy(transform.children(call)[1])
            # The Reg128& operand is not a scalar expression; it must refuse too.
            call['inner'][2] = source
        elif name == 'wrong_return_register':
            index = next(n for n in transform.walk(call['inner'][1])
                         if n.get('kind') == 'IntegerLiteral')
            index['value'] = '3'
        elif name == 'dropped_delay':
            call.clear(); call.update(kind='NullStmt')
        else:
            label = target(tree, lambda n: n.get('kind') == 'LabelStmt'
                           and n.get('name') == 'L_001A3C40')
            debit = target(label, lambda n: n.get('kind') == 'CompoundAssignOperator')
            number = target(debit, lambda n: n.get('kind') == 'IntegerLiteral')
            number['value'] = '3'
        try:
            lower(tree, descriptor, contract)
        except transform.Refusal as error:
            plants.append({'plant': name, 'rejected': True, 'reason': str(error)})
        else:
            raise RuntimeError('Stage A AST plant escaped: ' + name)
    return plants


def run(args, cwd, env, output, name, expected=0, text=None, timeout=240):
    result = subprocess.run(list(map(str, args)), cwd=cwd, env=env,
                            capture_output=True, timeout=timeout,
                            creationflags=subprocess.CREATE_NO_WINDOW)
    data = result.stdout + result.stderr
    path = output / (name + '.log')
    path.write_bytes(data)
    passed = result.returncode == expected and (text is None or text in data.decode(errors='replace'))
    row = {'name': name, 'argv': list(map(str, args)), 'exit_code': result.returncode,
           'expected_exit_code': expected, 'status': 'PASS' if passed else 'FAIL',
           'log': str(path), 'sha256': sha(path)}
    if not passed:
        (output / 'FAILED_CHECK.json').write_text(json.dumps(row,indent=2)+'\n',encoding='utf8')
        print(data.decode(errors='replace')[-3000:])
        raise RuntimeError('Stage A check failed: ' + name)
    return row, data.decode(errors='replace')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--parent-root', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--require-admitted-ownership', action='store_true')
    a = ap.parse_args()
    F = Path(__file__).resolve().parents[2]; P = a.parent_root.resolve(); O = a.output.resolve()
    assert O.is_relative_to(F / 'build/oracle-telemetry') and not O.exists()
    O.mkdir(); (O / 'temp').mkdir()
    ledger_path = F/'research/ledger/functions.csv'
    with ledger_path.open(encoding='utf-8') as handle:
        owner = next(r for r in csv.DictReader(handle)
                     if r['module']=='SLUS_214.10' and r['address']=='001a3c40')
    if a.require_admitted_ownership:
        ownership_gate(owner, '5ce5ad86d452c4d2422bd63e15223d5d6b3dfb77f224a88c1f476e9fb34e359d')
    generated = F / 'build/generated-aot/phase4-all'
    unit = generated / 'unit_0028.cpp'
    image_path = P / 'research/inputs/SLUS_214.10'
    original = reader(image_path.read_bytes())
    body = original(0x1a3c40, 8)
    assert body.hex() == '0800e00301000224'
    assert hashlib.sha256(body).hexdigest() == '5ce5ad86d452c4d2422bd63e15223d5d6b3dfb77f224a88c1f476e9fb34e359d'
    export = P / 'research/runs/ghidra-function-coverage-20261118/export-20261118-230132-1875-18160'
    for name, expected in PINS.items():
        assert sha(export/name) == expected
    with (export/'function-bodies.tsv').open(encoding='utf-8-sig') as handle:
        spans = [r for r in csv.DictReader(handle, delimiter='\t') if r['entry']=='001a3c40']
    assert spans == [{'entry':'001a3c40','start':'001a3c40','end':'001a3c47'}]
    with (export/'instructions.tsv').open(encoding='utf-8-sig') as handle:
        instructions = [r for r in csv.DictReader(handle,delimiter='\t')
                        if r['function_entry']=='001a3c40']
    assert len(instructions)==2
    for row in instructions:
        assert bytes.fromhex(row['bytes']) == original(int(row['address'],16),4)
    sdk = P.parent/'forks/rwsdk-v3.1-ps2'
    sdk_pins = {
        'rwsdk/src/resmem.c':'bacc01b37e6c796117f8112d8cc758233710c58052fbd32adf325889358cbc69',
        'rwsdk/src/resmem.h':'90f4fca3c0284a49c97da3b261e3dbf092196c2cdc956f93dadbe49265e9a45d',
        'rwsdk/os/sky/ostypes.h':'a83ffefb17e206ba34cb9f44b9d1e8827f581337895d2f9014063c26224d90a8'}
    for name, expected in sdk_pins.items():
        assert sha(sdk/name)==expected
    assert re.search(r'typedef\s+int\s+RwInt32\s*;', (sdk/'rwsdk/os/sky/ostypes.h').read_text())
    assert re.search(r'typedef\s+RwInt32\s+RwBool\s*;', (sdk/'rwsdk/os/sky/ostypes.h').read_text())
    census_path = F / 'build/oracle-telemetry/amended-ownership-arena-correction-01.json'
    census = json.loads(census_path.read_bytes())
    # The published AOT map is an identity gate, not ownership admission.
    assert sha(unit) == census['aot']['all_measured_source_sha256']['unit_0028.cpp']
    cache = (F / 'b/CMakeCache.txt').read_text()
    compiler = Path(next(x.split('=', 1)[1] for x in cache.splitlines()
                         if x.startswith('CMAKE_CXX_COMPILER:')))
    inputs = [unit, image_path, census_path, ledger_path, Path(__file__), F / 'tools/research/transform_generated_units.py',
              F / 'recomp/overlay_initializer_contract.json', generated/'units.hpp',
              F / 'tools/research/classify_stack_call_boundaries.py',
              F / 'tests/test_phase6_stage_a_leaf.cpp',
              F / 'tests/test_phase4_aot_equivalence.cpp', F / 'tests/test_phase5_generated_transform.cpp',
              F / 'runtime/include/ps2/cpu.hpp', F / 'runtime/include/ps2/memory.hpp',
              F / 'runtime/include/ps2/generated.hpp', compiler,
              *[export/name for name in PINS], *[sdk/name for name in sdk_pins]]
    before = {str(p): sha(p) for p in inputs}
    contract = json.loads((F / 'recomp/overlay_initializer_contract.json').read_bytes())
    descriptor = {'source_entry': '001a3c40', 'entry_case': 294, 'unit_name': 'unit_0028'}
    assert 'case 294u: goto L_001A3C40;' in unit.read_text()
    definition = transform.function_definition(transform.clang_ast(compiler, unit,
        [generated, F / 'runtime/include'], 'unit_0028', F), 'unit_0028')
    ir, value = lower(definition, descriptor, contract)
    assert value == 1  # Independent original-word and SDK result; never emitted from this assertion.
    controls = ast_controls(definition, descriptor, contract)
    if a.require_admitted_ownership:
        for key, control_value in [('ownership_status','unresolved'),
                           ('ownership_evidence_kinds','E5'),('ownership_evidence_ref','caller only')]:
            altered = dict(owner); altered[key] = control_value
            try:
                ownership_gate(altered, hashlib.sha256(body).hexdigest())
            except transform.Refusal as error:
                controls.append({'plant':'ownership_'+key,'rejected':True,'reason':str(error)})
            else:
                raise RuntimeError('Ownership gate plant escaped: '+key)
    header = ('#pragma once\n#include <cstdint>\nnamespace stage_a {\n'
              'struct ResourceHeapReference { std::uint32_t address; };\n'
              'std::int32_t close_resource_heap(ResourceHeapReference unused) noexcept;\n}\n')
    source = ('#include "stage_a_leaf.hpp"\nnamespace stage_a {\n'
              'std::int32_t close_resource_heap(ResourceHeapReference) noexcept {\n'
              f'    return std::int32_t{{{value}}};\n}}\n}}\n')
    (O / 'stage_a_leaf.hpp').write_text(header, encoding='utf8', newline='\n')
    (O / 'stage_a_leaf.cpp').write_text(source, encoding='utf8', newline='\n')
    # Reference only. Preserve all original operations; wrappers observe complete
    # write payloads. This is never linked into a product or protected oracle.
    trace = ['#pragma once', '#include "ps2/generated.hpp"', '#include "units.hpp"',
             'namespace leaf_reference {',
             'void unit_0028(ps2::Cpu&,std::uint32_t,std::uint32_t);']
    for bits in (8, 16, 32, 64):
        trace += [f'std::uint{bits}_t observed_ld{bits}(std::uint32_t);',
                  f'void observed_st{bits}(std::uint32_t,std::uint{bits}_t);']
    trace += ['void observed_ld128(std::uint32_t,ps2::Reg128&);',
              'void observed_st128(std::uint32_t,const ps2::Reg128&);', '}']
    (O / 'trace_reference.hpp').write_text('\n'.join(trace)+'\n', encoding='utf8', newline='\n')
    text = unit.read_text()
    assert text.count('namespace gen {') == 1
    copied = text.replace('namespace gen {', 'namespace leaf_reference {\nusing namespace gen;', 1)
    macros = '\n'.join(f'#define {kind}{bits} leaf_reference::observed_{kind}{bits}'
                       for kind in ['ld', 'st'] for bits in [8, 16, 32, 64, 128])
    copied = '#include "trace_reference.hpp"\n' + macros + '\n' + copied
    (O / 'observed_original.cpp').write_text(copied, encoding='utf8', newline='\n')
    archive = O / 'original_aot.a'
    shutil.copyfile(F / 'b/libps2_phase4_aot.a', archive)
    assert sha(archive) == sha(F / 'b/libps2_phase4_aot.a')
    states = P.parent.parent / 'milestone_work_verified_playable_20261110/games/mkarmageddon/disc/states'
    manifest_path = P / 'launchers/oracle_original_manifest.json'
    manifest = json.loads(manifest_path.read_bytes())
    state_pins = {name.rsplit('/', 1)[-1]: pin for name, pin in manifest.items()
                  if name.startswith('games/mkarmageddon/disc/states/') and name.endswith('.state')}
    def verify_states():
        assert len(state_pins) == 61
        for name, pin in state_pins.items():
            p = states / name
            assert p.stat().st_size == pin['size'] and sha(p) == pin['sha256']
    verify_states()
    backing = [F / p for p in ['build/generated-aot/vu1/vu1_table.cpp',
        'runtime/src/dispatch.cpp', 'runtime/src/observer_hook.cpp', 'runtime/src/native_hook.cpp',
        'runtime/src/cop.cpp', 'runtime/src/vu_micro.cpp']]
    inputs += backing + [F / 'b/libmka_transformed_overlay_initializers.a', archive, manifest_path,
        O/'stage_a_leaf.hpp', O/'stage_a_leaf.cpp', O/'observed_original.cpp', O/'trace_reference.hpp']
    before.update({str(p): sha(p) for p in inputs})
    env = dict(os.environ, TEMP=str(O / 'temp'), TMP=str(O / 'temp'),
               PATH=str(compiler.parent)+os.pathsep+os.environ['PATH'])
    checks = []
    native_object = O / 'stage_a_leaf.o'
    q, _ = run([compiler, '-std=c++20', '-O2', '-Wall', '-Wextra', '-Werror', '-c',
        O/'stage_a_leaf.cpp', '-o', native_object], F, env, O, 'compile-native')
    checks.append(q)
    q, symbols = run([compiler.parent/'llvm-nm.exe', '-C', archive], F, env, O, 'original-symbols')
    assert len(re.findall(r'\bT\s+gen::unit_0028\(', symbols))==1
    checks.append(q)
    q, symbols = run([compiler.parent/'llvm-nm.exe', native_object], F, env, O, 'native-symbols')
    assert not re.search(r'\bU\s+\S', symbols)
    checks.append(q)
    binary = O / 'test_stage_a_leaf.exe'
    args = [compiler, '-std=c++20', '-O1', '-static', '-pthread',
            '-I'+str(O), '-I'+str(F/'tests'), '-I'+str(F/'runtime/include'),
            '-I'+str(F/'runtime/src'), '-I'+str(generated),
            '-I'+str(F/'b/native-overlay-initializers'),
            F/'tests/test_phase6_stage_a_leaf.cpp', O/'observed_original.cpp', native_object,
            *backing, archive, F/'b/libmka_transformed_overlay_initializers.a', '-o', binary]
    q, _ = run(args, F, env, O, 'compile-differential'); checks.append(q)
    q, log = run([binary, states], F, env, O, 'equivalence', text='PASS_STAGE_A_LEAF')
    checks.append(q)
    summary = next(s for s in log.splitlines() if s.startswith('PASS_STAGE_A_LEAF'))
    counts = {k: int(v) for k,v in re.findall(r'(\w+)=(\d+)', summary)}
    for plant in ['width', 'stale-ssa', 'delay', 'invented-input', 'return-contract',
                  'frame-size', 'transient-write']:
        q, _ = run([binary, states, '--planted-'+plant], F, env, O, 'plant-'+plant,
                    expected=1, text='FAIL_STAGE_A_LEAF'); checks.append(q)
    verify_states()
    assert before == {str(p): sha(p) for p in inputs}, 'Stage A input changed during measurement'
    result = {'schema': 'stage_A_leaf_capability_v1', 'status': 'PASS_CAPABILITY_ONLY',
        'classification': 'capability only', 'function': 'SLUS_214.10:001a3c40',
        'body_sha256': hashlib.sha256(body).hexdigest(), 'source_pins': before,
        'generated_source_sha256': sha(O/'stage_a_leaf.cpp'), 'native_object_sha256': sha(native_object),
        'binary_sha256': sha(binary), 'typed_IR': ir, 'counts': counts, 'checks': checks,
        'AST_controls': controls, 'ownership': {'required_for_this_run': a.require_admitted_ownership,
            'ledger_row': owner, 'scope':'Source ownership admission is separate from implementation credit. This tool never changes either.'},
        'native_ABI': {'input': 'unused opaque source ResourceHeapReference, uint32 address',
            'return': 'RwBool / int32 proved by original delay slot and SDK; sign-extended low64 projection',
            'preserved': 'Every other GPR128 lane, special/FPR/VU/COP state, SP, RA and all memory',
            'frame_bytes': 0, 'dependent_callee_return_contracts': 0},
        'SDK_scope': 'Pinned SDK3.1 corroborates role and RwBool typedef; retail behavior/width/absence of MEMORYLOG effects come from the complete original eight-byte body.',
        'switch': 'Explicit test boundary selects actual original AOT or Stage A; production direct-call routing is unchanged.',
        'sidecars': 'Controller/input sidecars do not contain a heap-close invocation ABI. Not misrepresented as 72 function calls.',
        'trace_scope': 'Concrete whole function has no writes/calls by complete original words and AST. Empty trace is compared dynamically; nonempty tuple mutation fixtures test only the comparator. No generic external-effect journal or historical invocation is claimed.',
        'credit_delta': 0, 'transformed_total_delta': 0,
        'Not verified': ['Production routing, Stage B native owner/lifetime, whole game, historical invocation; general transform scaling.'],
        'Contradictions with earlier documents': ['The old lowerer demands an initializer store; it refuses a genuine no-store leaf. Stage A is a separate ABI experiment.'],
        'Falsification attempts': controls + [q for q in checks if q['expected_exit_code'] == 1]}
    (O/'RESULT.json').write_text(json.dumps(result, indent=2)+'\n', encoding='utf8', newline='\n')
    print('PASS', O/'RESULT.json', sha(O/'RESULT.json'), counts)


if __name__ == '__main__':
    main()
