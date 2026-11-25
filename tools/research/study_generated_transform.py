"""Measure generated-unit transformation attempts for the mandatory scale-out sample."""
from __future__ import annotations

import argparse
import csv
import gc
import hashlib
import json
from pathlib import Path
import re
import struct
import time

import transform_generated_units as transform

MANDATORY = {
    0x104000: "requested leaf matrix/float sample",
    0x102BB4: "requested struct-mutator sample",
    0x100320: "branching caller",
    0x18DA30: "$gp global cleaner",
    0x1225D0: "virtual method transition",
    0x19F0B0: "chunk-header seed",
    0x19F1E0: "chunk-finder seed",
    0x1A2BC0: "plugin-registry seed",
    0x1A64A0: "camera view-window seed",
    0x197BC0: "health deduction",
}
SUPPLEMENTAL = {
    0x102B88: "authenticated enclosing function for the requested 00102BB4 continuation",
    0x18D970: "authenticated enclosing function for the requested 0018DA30 continuation",
    0x103FA0: "candidate enclosing math CFG; entry and signature remain unproved",
    0x14DE80: "authenticated signed-index invalidation leaf; relative receiver projection requires separate E2",
}
BOUNDARIES = {
    "Core Renderer & Allocator": 0x1A3190,
    "Display & Movie GL Presentation": 0x1A63D0,
    "VFS, Files & Saves": 0x2E06E0,
    "Audio / Sound / Codecs": 0x12C440,
    "Input & Haptics": 0x185CB0,
    "Session (Host Container + Game Bindings)": 0x1588C0,
    "Tasks (Task Domain & Registry)": 0x130AC0,
    "Main Reentry & Frame Loop": 0x12E4E0,
    "Profiles (Aggregate, Records, Preferences, Menu)": 0x171B10,
    "Modes (Mode State & Dispatch)": 0x185EC0,
    "First-Scene Allocator & Continuations": 0x220900,
}

# Original primary cohort, recorded before correcting module correspondence.
# These addresses are never replaced by easier or newly bound projections.
ORIGINAL_COHORT_RECEIPT_SHA256 = "a3d280c1903219facd1cf036d883e5895f0eff3e6ca4b3bf7d99d6a374f27914"
ORIGINAL_PRIMARY = {
    0x00100320: [],
    0x00102BB4: [],
    0x00104000: [],
    0x001225D0: [],
    0x0012C440: ['Audio / Sound / Codecs'],
    0x0012E4E0: ['Main Reentry & Frame Loop'],
    0x00130AC0: ['Tasks (Task Domain & Registry)'],
    0x001588C0: ['Session (Host Container + Game Bindings)'],
    0x00171B10: ['Profiles (Aggregate, Records, Preferences, Menu)'],
    0x00185CB0: ['Input & Haptics'],
    0x0018DA30: [],
    0x00197BC0: [],
    0x0019F0B0: [],
    0x0019F1E0: [],
    0x001A2BC0: [],
    0x001A0160: ['0402 Matrix'],
    0x001A1B30: ['040b Geometry'],
    0x001A2270: ['0404 Streams'],
    0x001A3AF0: ['0401 Transforms'],
    0x001A3DF0: ['0412 World'],
    0x001A3190: ['Core Renderer & Allocator'],
    0x001A64A0: [],
    0x001A6F40: ['0403 Frames'],
    0x001A7DE0: ['0406 Images'],
    0x001A63D0: ['Display & Movie GL Presentation'],
    0x001A61D0: ['0405 Cameras'],
    0x001A89C0: ['0407 Rasters & Registration'],
    0x001A9E80: ['0408 Textures'],
    0x001C4A80: ['0409 Texture Dictionaries'],
    0x001C49B0: ['040a Materials'],
    0x00220900: ['First-Scene Allocator & Continuations'],
    0x002E06E0: ['VFS, Files & Saves'],
    0x00334700: ['Modes (Mode State & Dispatch)'],
}
CORRECTED_PROJECTIONS = {0x1CF300, 0x185EC0}

# Source-declared projections, not semantic equivalence claims. Numeric 040x
# labels are deliberately absent: they do not bind native code to retail logic.
COUNTERPARTS = {
    "0412 World": (0x1CF300, "runtime/src/native/retail_camera_world.cpp", "NativeRetailCameraWorldLink::attach_after_frame_update_from_1cf300("),
    "Tasks (Task Domain & Registry)": (0x130AC0, "runtime/src/native/retail_task_registry.cpp", "NativeRetailTaskRegistry::drain_task_observers_from_130ac0("),
    "Main Reentry & Frame Loop": (0x12E4E0, "runtime/src/native/retail_main.cpp", "!pump_retail_middleware(m, f)"),
    "Profiles (Aggregate, Records, Preferences, Menu)": (0x171B10, "runtime/src/native/retail_saved_profile_records.cpp", "NativeRetailSavedProfileRecords::initialize_from_171b10("),
    "Modes (Mode State & Dispatch)": (0x185EC0, "runtime/src/native/retail_main_mode.cpp", "// This is the original185EC0 effect, not implicit readiness in a reader."),
    "First-Scene Allocator & Continuations": (0x220900, "runtime/src/native/retail_first_scene_allocator.cpp", "continue_first_scene_at_220900("),
}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate_sample_coverage(samples, modules, reviews):
    if not set(ORIGINAL_PRIMARY) <= samples.keys() or not set(MANDATORY) <= samples.keys():
        raise ValueError("Mandatory transform sample is missing")
    expected = {module["name"] for module in modules}
    if expected != {item["name"] for item in reviews} or len(reviews) != len(expected):
        raise ValueError("Native module correspondence review differs")
    for item in reviews:
        if item["status"] == "SOURCE_DECLARED_RETAIL_PROJECTION":
            address = int(item["original_entry"], 16)
            if address not in samples or item["name"] not in [m["name"] for m in samples[address]["native_modules"]]:
                raise ValueError("Source-bound native counterpart was not attempted")
        elif item["status"] != "NO_PROVEN_ONE_TO_ONE_RETAIL_LOGIC_COUNTERPART":
            raise ValueError("Unknown native correspondence disposition")


def verify_selected_unit(path, expected):
    if sha(path) != expected:
        raise ValueError("Selected generated-unit identity changed")


def study_controls(samples, modules, reviews, unit, expected):
    import copy
    validate_sample_coverage(samples, modules, reviews)
    verify_selected_unit(unit, expected)
    rejected = []
    missing_sample = copy.deepcopy(samples)
    missing_sample.pop(0x18DA30)
    for label, operation in (
        ("missing original primary probe", lambda: validate_sample_coverage({k:v for k,v in samples.items() if k != 0x1A3DF0}, modules, reviews)),
        ("missing requested GP cleaner", lambda: validate_sample_coverage(missing_sample, modules, reviews)),
        ("missing native module review", lambda: validate_sample_coverage(samples, modules, reviews[1:])),
        ("changed generated-unit digest", lambda: verify_selected_unit(unit, "0" * 64)),
    ):
        try:
            operation()
        except ValueError:
            rejected.append(label)
        else:
            raise AssertionError("Study admitted " + label)
    assert classify_failure("unbound_memory:non_GP_base", "", True) == "unbound typed pointer/field ABI"
    assert classify_failure("unbound_indirect_target:cursor_assignment", "", True) == "lowerer cursor transport/parser limitation"
    return {"status": "PASS", "planted_rejections": rejected,
            "taxonomy_controls": ["non_GP_base is not GP evidence", "cursor_assignment is not virtual dispatch evidence"]}


def classify_failure(reason, text, authenticated_entry):
    if not authenticated_entry:
        return "function boundary and live-in state"
    if "cursor_assignment" in reason:
        return "lowerer cursor transport/parser limitation"
    if "non_GP_base" in reason or "relative_receiver" in reason or "relative_field" in reason:
        return "unbound typed pointer/field ABI"
    if "GP_field" in reason or "unknown_GP" in reason:
        return "unbound specific GP field contract"
    if "unbound_indirect_target" in reason:
        return "unbound generated indirect target; original dispatch recorded separately"
    if "unsupported_conditional" in reason or "GotoStmt" in reason:
        return "branching control flow and delay slots"
    if "ld" in text or "st" in text or "memory" in reason or "register" in reason:
        return "pointers and ABI live-ins"
    return "unsupported generated operation"


def lui_outcome_summary(results, expected, before):
    """Keep diagnostic changes distinct from bound instructions that advanced."""
    changed={r['address'] for r in results if (r['reason'],r['first_unlowered_pc'])!=
             (before[r['address']]['reason'],before[r['address']]['first_unlowered_pc'])}
    bound={r['address'] for r in results if r.get('lui_bindings')}
    advanced={r['address'] for r in results if r['address'] in bound and
              r['first_unlowered_pc']!=before[r['address']]['first_unlowered_pc']}
    unexpected=changed-expected
    unexpected_bindings=bound-expected
    failed=expected-bound
    not_advanced=expected-advanced
    return {'status':'PASS_BOUNDED_INSTRUCTION_BINDING' if not (unexpected or unexpected_bindings or failed or not_advanced) else 'INCOMPLETE_OR_FAILED_LUI_BINDING',
            'actual_bound_probe_count':len(bound),'actual_advanced_probe_count':len(advanced),
            'bound_probe_addresses':sorted(bound),'advanced_probe_addresses':sorted(advanced),
            'changed_first_frontier_addresses':sorted(changed),
            'binding_failure_addresses':sorted(failed),'not_advanced_addresses':sorted(not_advanced),
            'unexpected_non_LUI_changes':sorted(unexpected),'unexpected_non_LUI_bindings':sorted(unexpected_bindings)}


def lui_outcome_controls():
    import copy
    expected={f'{i:08x}' for i in range(15)}
    before={a:{'reason':'old','first_unlowered_pc':'00100000'} for a in expected|{'ffffffff'}}
    rows=[{'address':a,'reason':'next','first_unlowered_pc':'00100004','lui_bindings':[{'proved':True}]} for a in sorted(expected)]
    rows.append({'address':'ffffffff',**before['ffffffff']})
    complete=lui_outcome_summary(rows,expected,before)
    if complete['status']!='PASS_BOUNDED_INSTRUCTION_BINDING' or complete['actual_advanced_probe_count']!=15:
        raise AssertionError('Complete LUI metadata control failed')
    failed=copy.deepcopy(rows);failed[0].update(reason='binding_refused',first_unlowered_pc='00100000',lui_bindings=[])
    retained=lui_outcome_summary(failed,expected,before)
    if retained['status']!='INCOMPLETE_OR_FAILED_LUI_BINDING' or retained['actual_bound_probe_count']!=14 or retained['actual_advanced_probe_count']!=14 or retained['binding_failure_addresses']!=['00000000'] or retained['not_advanced_addresses']!=['00000000']:
        raise AssertionError('Changed diagnostic falsely counted as instruction advance')
    unexpected=copy.deepcopy(rows);unexpected[-1]['reason']='unexpected'
    rejected=lui_outcome_summary(unexpected,expected,before)
    if rejected['status']!='INCOMPLETE_OR_FAILED_LUI_BINDING' or rejected['unexpected_non_LUI_changes']!=['ffffffff']:
        raise AssertionError('Unexpected non-LUI change was not retained')
    return {'status':'PASS','scope':'Synthetic outcome accounting only; no additional original probes or native credit.',
            'complete15':complete,'one_failed_binding_retained':retained,'unexpected_non_LUI_change_retained':rejected}


def mtc1_outcome_summary(results,expected,before):
    rows=[dict(row,lui_bindings=row.get('mtc1_bindings',[])) for row in results]
    return {key.replace('non_LUI','non_MTC1'):value.replace('_LUI_','_MTC1_') if isinstance(value,str) else value
            for key,value in lui_outcome_summary(rows,expected,before).items()}


def mtc1_outcome_controls():
    before={'001a64a0':{'reason':'old','first_unlowered_pc':'001a64ac'},'ffffffff':{'reason':'old','first_unlowered_pc':None}}
    rows=[{'address':address,**row,'mtc1_bindings':[]} for address,row in before.items()]
    rows[0].update(reason='binding_refused')
    failed=mtc1_outcome_summary(rows,{'001a64a0'},before)
    if failed['actual_bound_probe_count'] or failed['actual_advanced_probe_count'] or failed['binding_failure_addresses']!=['001a64a0']:
        raise AssertionError('MTC1 failure at same PC counted as advancement')
    rows[0].update(mtc1_bindings=[{'proved':True}],first_unlowered_pc='001a64b0')
    passed=mtc1_outcome_summary(rows,{'001a64a0'},before)
    if passed['status']!='PASS_BOUNDED_INSTRUCTION_BINDING' or passed['actual_advanced_probe_count']!=1:
        raise AssertionError('MTC1 advancement accounting failed')
    rows[1]['reason']='unexpected'
    unexpected=mtc1_outcome_summary(rows,{'001a64a0'},before)
    if unexpected['unexpected_non_MTC1_changes']!=['ffffffff'] or unexpected['status']!='INCOMPLETE_OR_FAILED_MTC1_BINDING':
        raise AssertionError('MTC1 unexpected change not retained')
    return {'status':'PASS','failed_binding_retained':failed,'one_bound_and_advanced':passed,'unexpected_change_retained':unexpected,
            'scope':'Synthetic outcome controls only; no extra original probes or native credit.'}


def scalar_outcome_summary(results,expected,before):
    changed={r['address'] for r in results if (r['reason'],r['first_unlowered_pc'])!=(before[r['address']]['reason'],before[r['address']]['first_unlowered_pc'])}
    bound={r['address'] for r in results if r.get('scalar_bindings')}
    advanced={r['address'] for r in results if r['address'] in bound and r['first_unlowered_pc']!=before[r['address']]['first_unlowered_pc']}
    joined={r['address'] for r in results if r.get('scalar_candidates')}
    unknown={r['address'] for r in results if r['reason'].startswith('scalar_unbound_GPR_value_ABI:')}
    unexpected=changed-expected
    invalid={r['address'] for r in results if r['address'] in changed and r['address'] not in advanced and r['address'] not in unknown}
    return {'status':'PASS_BOUNDED_REPLAY_WITH_UNBOUND_INPUTS' if not (unexpected or invalid) else 'INCOMPLETE_OR_FAILED_SCALAR_REPLAY',
            'actual_bound_probe_count':len(bound),'bound_probe_addresses':sorted(bound),
            'actual_advanced_probe_count':len(advanced),'advanced_probe_addresses':sorted(advanced),
            'changed_diagnostic_addresses':sorted(changed),'original_AST_joined_probe_addresses':sorted(joined),
            'input_unbound_probe_addresses':sorted(unknown),'unexpected_non_scalar_changes':sorted(unexpected),
            'invalid_non_advance_changes':sorted(invalid),'new_complete_E2_units':0,'new_credit':0}


def scalar_outcome_controls():
    before={a:{'reason':'old','first_unlowered_pc':'00100000'} for a in ('00000000','00000001')}
    rows=[{'address':'00000000','reason':'scalar_unbound_GPR_value_ABI:register_4','first_unlowered_pc':'00100000','scalar_candidates':[{}]},
          {'address':'00000001','reason':'old','first_unlowered_pc':'00100000'}]
    unknown=scalar_outcome_summary(rows,{'00000000'},before)
    if unknown['actual_advanced_probe_count'] or unknown['actual_bound_probe_count'] or unknown['status']!='PASS_BOUNDED_REPLAY_WITH_UNBOUND_INPUTS':raise AssertionError('Unknown input falsely counted as advance')
    rows[0].update(first_unlowered_pc='00100004',scalar_bindings=[{}])
    advance=scalar_outcome_summary(rows,{'00000000'},before)
    if advance['actual_advanced_probe_count']!=1:raise AssertionError('Actual scalar advance missing')
    rows[1]['reason']='unexpected'
    rejected=scalar_outcome_summary(rows,{'00000000'},before)
    if rejected['status']!='INCOMPLETE_OR_FAILED_SCALAR_REPLAY':raise AssertionError('Non-scalar change ignored')
    return {'status':'PASS','unknown_input_retained':unknown,'bound_PC_advance':advance,'unexpected_change_retained':rejected}


class LUIFailureTrace(transform.Lowering):
    """Record actual propagated statement failures, without changing lowering."""
    def __init__(self,*args):self.statement_failures=[];super().__init__(*args)
    def statement(self,node):
        try:return super().statement(node)
        except transform.Refusal as error:
            self.statement_failures.append((error,node,self.pc));raise


def actual_failure_annotation(lowering,error,source,original,reason):
    trace=[(n,pc) for e,n,pc in lowering.statement_failures if e is error]
    if not trace:return {'observed_semantic_category':'no_statement_refusal','actual_AST':None}
    node,pc=trace[0];word=int.from_bytes(original(pc,4),'little')
    def compact(n):
        result={k:n[k] for k in ('kind','opcode','castKind','name','value') if k in n}
        if 'type' in n:result['type']={k:v for k,v in n['type'].items() if k!='typeAliasDeclId'}
        if 'referencedDecl' in n:result['referencedDecl']={k:n['referencedDecl'][k] for k in ('kind','name') if k in n['referencedDecl']}
        result['children']=[compact(c) for c in transform.children(n)];return result
    kind=node.get('kind');category='unclassified_statement_requires_review'
    calls=[]
    for n in transform.walk(node):
        if n.get('kind')=='CallExpr':
            try:calls.append(transform.declaration(transform.children(n)[0]).get('name'))
            except transform.Refusal:calls.append('unbound_call_target')
    if reason=='unbound_stack_FPR_or_mixed_source_type':category='FPR_stack_bits_and_native_frame_proof'
    elif reason.startswith('unbound_stack_local'):category='uncertain_native_stack_local_or_callee_effects'
    elif reason=='unbound_memory:GP_field_or_width':category='unbound_GP_owner_or_field_type'
    elif reason=='unbound_memory:non_GP_base':category='non_GP_receiver_binding_or_existing_contract_selection'
    elif reason.startswith('scalar_unbound_GPR_value_ABI:'):category='source_joined_scalar_operation_with_unbound_GPR_native_input'
    elif reason.startswith('unbound_value:register_'):category='unbound_GPR_livein_in_existing_integer_expression'
    elif kind=='DeclStmt':category='branch_condition_or_return_capture_contract'
    elif kind=='BinaryOperator' and node.get('opcode')=='=':
        lhs,rhs=transform.children(node);lhs=transform.unwrap(lhs)
        if lhs.get('kind')=='MemberExpr' and lhs.get('name')=='pc':category='source_cursor_state_control_contract'
        elif lhs.get('kind')=='ArraySubscriptExpr':
            base=transform.unwrap(transform.children(lhs)[0]);bank=base.get('name')
            if bank=='f':
                if 'ld32' in calls:category='FPR_typed_memory_load'
                elif calls:category='FPR_arithmetic_or_state_effect'
                elif word>>26==0x11 and word>>21&31==4:category='FPR_bit_transfer_from_GPR'
                else:category='FPR_value_assignment_or_state_effect'
            elif bank in ('d','sd','w','sw'):
                category='native_call_RA_delay_return_ABI' if word>>26==3 else 'GPR_value_assignment_or_livein_SSA'
    elif kind=='CallExpr' and 'ld32' in calls:category='typed_memory_load_expression_value_SSA'
    elif kind=='IfStmt':category='conditional_control_and_effect_contract'
    def location(item):
        while 'expansionLoc' in item:item=item['expansionLoc']
        return item
    begin=location(node.get('range',{}).get('begin',{}));end=location(node.get('range',{}).get('end',{}));snippet=None
    if 'offset' in begin and 'offset' in end:
        raw=source.encode('utf-8');a,b=begin['offset'],end['offset']+end.get('tokLen',1)
        snippet={'start_byte':a,'end_byte_exclusive':b,'text':raw[a:b].decode('utf-8')}
    from stack_frame_analysis import control
    pcs=[pc]+([pc+4] if control(pc,word) else [])
    memory=None
    memory_words=[(p,int.from_bytes(original(p,4),'little')) for p in pcs
                  if int.from_bytes(original(p,4),'little')>>26 in {0x20,0x21,0x23,0x24,0x25,0x27,0x28,0x29,0x2b,0x31,0x39,0x37,0x3f,0x1e,0x1f}]
    if memory_words:
        at,opword=memory_words[-1];base=opword>>21&31;offset=opword&65535;offset-=65536 if offset&32768 else 0
        memory={'operation_pc':f'{at:08x}','original_base_register':base,'signed_offset':offset,'native_owner':'UNBOUND'}
        if base==28:
            memory['source_GP_address']=f'{(int(lowering.contract["source_gp"],16)+offset)&0xffffffff:08x}'
            if category=='typed_memory_load_expression_value_SSA':category='typed_GP_load_expression_and_owner'
        elif base in lowering.regs:
            value=lowering.values[lowering.regs[base]]['constant_value']
            memory['source_absolute_address']=f'{(value+offset)&0xffffffff:08x}'
            memory['evidence']='Original memory use of an actual constant SSA base; not an inferred native pointer.'
            if reason.startswith('unbound_memory') or category=='typed_memory_load_expression_value_SSA':category='unbound_absolute_source_data_symbol'
        elif lowering.frame and base in lowering.frame.offsets:
            memory['symbolic_frame_offset']=lowering.frame.offsets[base]+offset
    label='L_'+f'{pc:08X}'+':'
    fragment=source.split(label,1)[1].split('\nL_',1)[0] if label in source else None
    return {'observed_semantic_category':category,'actual_AST':compact(node),'actual_statement':snippet,'original_memory_use':memory,
            'generated_fragment_at_failure':fragment,'original_bundle_words':[{'pc':f'{p:08x}','word':f'{int.from_bytes(original(p,4),"little"):08x}'} for p in pcs],
            'scope':'Observed fatal AST/source shape only, kept separate from raw refusal taxonomy; not a native binding or next-barrier prediction.'}


def function_table_row(sample):
    """Summarize measured original features without treating refusal as absence."""
    context = sample.get("original_function_context", {})
    features = context.get("features")
    hard = {key: None for key in ("pointers", "GP_globals", "vtable_indirect", "register128")}
    if features is not None:
        hard.update(pointers={"non_GP_non_stack_memory_operations": len(features["pointer_memory"]),
                              "stack_operations": features["stack_memory_count"]},
                    GP_globals={"operations": len(features["gp_accesses"]),
                                "addresses": sorted({e["original_global_va"] for e in features["gp_accesses"]})},
                    vtable_indirect={"jalr_operations": len(features["indirect_calls"]), "typed_vtable_binding": "unproved"},
                    register128={"lq_sq_operations": len(features["wide_register_memory"]), "MMI_opcode_operations": features["mmi_count"]})
    hard["overlays"] = "Selected main ELF input, not an overlay; cross-overlay semantics unproved"
    hard["cycle_budget"] = {"generated_budget_debits_processed_before_refusal": sample.get("source_budget_debits_processed"),
                            "suspension_or_native_scheduling_contract": "unproved"}
    requirements = []
    if not sample.get("entry_authenticated"):
        requirements.append("Authenticate entry and caller live-ins before choosing a native signature")
    reason = sample["reason"]
    if "register_29" in reason:
        requirements.append("Lower stack/frame SSA and define scalar/128-bit callee-save live-ins/live-outs")
    if "cursor_assignment" in reason:
        requirements.append("Lower generated assignment/cursor AST; this refusal does not prove original virtual dispatch")
    if "CallExpr" in reason:
        requirements.append("Lower nested helper/load expression with original width and signedness")
    requirements.append("Prove typed field owners, extents, lifetimes and aliases for original memory bases; do not cast a guest address into a host pointer")
    if features and features["gp_accesses"]:
        requirements.append("Bind each listed GP address to a typed native owner and preserve exact load/store width")
    if features and features["indirect_calls"]:
        requirements.append("Prove target set, receiver adjustment, native callable ABI and return/live-out effects for original jalr sites")
    if sample["address"] == "0014de80":
        requirements = ["Separate IndexSelection{int32 index} field projection at +0xCA8 is proved; complete class layout/aliases are not",
                        "Original GP[-0x6130] receiver is null in all61 saved states; zero legal saved-state E2 admissions"]
    deeper = {"original_entry": context.get("entry"), "body_spans": context.get("body_spans", []),
              "instruction_count": features["instructions"] if features else None,
              "memory_instruction_kinds": features["load_store_kinds"] if features else {},
              "type_limit": "Instruction width/sign-extension evidence is not a recovered C++ owner/pointer type; unobserved deeper operations stay unknown"}
    semantics = {"lb":"signed8 load", "lbu":"zero-extended8 load", "lh":"signed16 load", "lhu":"zero-extended16 load",
                 "lw":"signed32-to64 load", "lwu":"zero-extended32 load", "sw":"low32 store", "sb":"low8 store", "sh":"low16 store",
                 "lq":"128-bit load", "sq":"128-bit store", "ld":"64-bit load", "sd":"64-bit store",
                 "lwc1":"raw32 FPU load", "swc1":"raw32 FPU store", "lwl":"partial32 left merge", "lwr":"partial32 right merge",
                 "ldl":"partial64 left merge", "ldr":"partial64 right merge", "swl":"partial32 left store", "swr":"partial32 right store"}
    deeper["instruction_type_handling"] = {op:semantics.get(op,"Original instruction operation; no C++ owner type inferred") for op in deeper["memory_instruction_kinds"]}
    return {"address": sample["address"], "cohort": sample["cohort"],
            "prior_native_module_associations": sample.get("prior_native_module_associations", []),
            "current_source_bound_modules": [m["name"] for m in sample["native_modules"]],
            "prior_association_scope": sample.get("prior_association_scope"),
            "six_hard_classes": hard, "first_compiler_refusal": {"category": sample["failure_category"], "reason": reason, "pc": sample["first_unlowered_pc"]},
            "deeper_original_findings": deeper, "pointer_ABI_requirements": requirements,
            "measured_cost": {"shared_AST_seconds": sample.get("ast_parse_seconds"), "AST_shared_by": sample.get("ast_parse_shared_by"),
                              "AST_share_unit": sample.get("generated_unit"), "lowering_seconds": sample.get("lowering_seconds"),
                              "native_runtime_ns": None,
                              "native_runtime_unmeasured_reason": "No saved-state-verified emitted unit in this attempted contract"},
            "result": "REFUSED; new transformed credit0; E2 not run for this attempted contract"}


def full_tables(report, fork):
    if report.get('frame_stage'):raise ValueError('legacy_before_frame_tables_refuse_frame_stage_report')
    rows = {s["address"]: function_table_row(s) for s in report["samples"]}
    primary = [rows[f"{a:08x}"] for a in ORIGINAL_PRIMARY]
    extra = [rows[f"{a:08x}"] for a in list(SUPPLEMENTAL) + sorted(CORRECTED_PROJECTIONS)]
    module_rows = []
    for module in report["native_module_correspondence_review"]:
        legacy = next((f"{a:08x}" for a,names in ORIGINAL_PRIMARY.items() if module["name"] in names), None)
        actual = module.get("original_entry")
        selected = rows.get(actual or legacy)
        module_rows.append({"name": module["name"], "classification": module["classification"],
                            "historical_probe": legacy, "source_bound_projection": actual,
                            "correspondence": module["status"], "correspondence_limit": module["limit"],
                            "source_sha256": module["source_sha256"], "source_witness": module.get("witness"),
                            "call_chain_witnesses": module.get("call_chain_witnesses", []),
                            "observation_scope": "Source-bound original projection" if actual else "Historical original probe only; these findings do not characterize the native module",
                            "six_hard_classes": selected["six_hard_classes"] if selected else None,
                            "first_compiler_refusal": selected["first_compiler_refusal"] if selected else None,
                            "deeper_original_findings": selected["deeper_original_findings"] if selected else None,
                            "pointer_ABI_requirements": selected["pointer_ABI_requirements"] if selected else ["No original correspondence proved"],
                            "measured_cost": selected["measured_cost"] if selected else None,
                            "result": "Platform service retained; generated counterpart not required" if module["classification"] == "platform-boundary" else
                                      "Bounded source projection attempted; new E2/native credit0" if actual else "Mixed Session game-side correspondence unresolved; 5S.1 remains BLOCKED"})
    inventory = json.loads((fork/"research/ledger/aot_coverage_inventory.json").read_bytes())
    measurement = inventory["memory_abi_transform"]["measurement"]
    receipt_path = fork/measurement["receipt_path"]
    if sha(receipt_path) != measurement["receipt_sha256"]:
        raise ValueError("Existing successful prototype receipt changed")
    receipt = json.loads(receipt_path.read_bytes())
    successes = [{**item, "cohort": "existing two prototypes; outside primary33 and additional6",
                  "six_hard_classes": {"pointers": "One native u32 selected_mode field; no pointers in emitted kernel", "GP_globals": "Original0064F1A0 explicitly bound to OverlaySelection.selected_mode",
                      "vtable_indirect": "No guest indirect access in emitted kernel; caller/return boundary pinned", "register128": "V1 low64 explicit live-out; upper64 and remaining CPU unchanged in reference adapter",
                      "overlays": item["module"]+" original initializer/source input pinned", "cycle_budget": "Reference-side exact -3; no native scheduling contract claimed"},
                  "first_compiler_refusal": None, "pointer_ABI_requirements": ["Bounded selected_mode owner and V1 low64 live-out only; no general receiver/class ABI"],
                  "measured_cost": {"benchmark": receipt["benchmark"], "scope": "Shared two-function isolated repeated-call benchmark; not a per-function runtime, frame or population estimate"},
                  "result": "VERIFIED prototype;61 saved states per function (122 total),8 constructed total;2 retained transformed credits,0 new"} for item in receipt["functions"]]
    record_path = fork.parents[2]/"research/runs/phase4-aot-completion-20261124/VERIFICATION.json"
    extension = json.loads(record_path.read_bytes())["instruction_phase5S"]["relative_receiver_extension"]
    index = rows["0014de80"]
    index["measured_cost"]["separate_constructed_receiver_benchmark"] = extension["benchmark"]
    index["measured_cost"]["separate_constructed_receiver_scope"] = extension["benchmark_scope"]
    index["result"] += "; separate typed signed-index projection18 constructed pass,0 admitted/61 inapplicable saved states; wrong-value and wrong-field rejected; zero credit"
    denominators = {"primary_selected_addresses": 33, "additional_selected_addresses": 6, "union_selected_addresses": 39,
                    "primary_saved_state_verified": 0, "additional_saved_state_verified": 0, "existing_verified_outside_sample": 2,
                    "population_catalogue_in_scope": 15670, "population_projection": None,
                    "note": "Selected addresses include continuations; primary0/33 and additional0/6 are separate from existing tier2/15670. No statistical or runtime extrapolation."}
    observed = {}
    for key in ("pointers", "GP_globals", "vtable_indirect", "register128", "overlays", "cycle_budget"):
        relevant = []
        for row in primary:
            h = row["six_hard_classes"][key]
            positive = (bool(h and h["non_GP_non_stack_memory_operations"]) if key == "pointers" else
                        bool(h and h["operations"]) if key == "GP_globals" else
                        bool(h and h["jalr_operations"]) if key == "vtable_indirect" else
                        bool(h and (h["lq_sq_operations"] or h["MMI_opcode_operations"])) if key == "register128" else
                        False if key == "overlays" else row["measured_cost"]["AST_share_unit"] is not None)
            if positive: relevant.append(row["address"])
        observed[key] = {"primary_observed_addresses": relevant, "verified_after_transform": 0, "observed_denominator": len(relevant),
                         "scope": "Observed original feature/generated budget membership; this is not proof the lowerer reached or solved the feature"}
    if (len(primary) != 33 or len(extra) != 6 or len(module_rows) != 23 or len(successes) != 2
        or len({r["address"] for r in primary+extra}) != 39
        or {r["address"] for r in primary} != {f"{a:08x}" for a in ORIGINAL_PRIMARY}
        or {r["address"] for r in primary} & {r["address"] for r in extra}):
        raise ValueError("Full table cohorts, module census or retained successes changed")
    aggregate = refusal_aggregate(report)
    plan = minimal_bindings_plan(report, aggregate)
    controls = aggregate_self_tests(report)
    return {"refusal_aggregate_controls": controls, "refusal_aggregate": aggregate, "minimal_bindings_plan": plan, "schema": "generated_transform_full_tables_v1", "table_generator_sha256": sha(Path(__file__)), "primary33": primary, "additional6": extra,
            "existing_success2": successes, "native_module23": module_rows, "denominators": denominators,
            "technique_observed_fractions": observed, "existing_success_receipt": measurement,
            "relative_receiver_extension": {key:extension[key] for key in ("status","constructed_comparisons","saved_states","admitted","inapplicable","benchmark","benchmark_scope","credit_added","production_callers")}}


def refusal_aggregate(report):
    if report.get('frame_stage'):raise ValueError('legacy_before_frame_aggregate_refuse_frame_stage_report')
    from collections import Counter
    rows=report['samples']
    primary=set(report['cohorts']['original_primary33'])
    extra=set(report['cohorts']['supplemental4'])|set(report['cohorts']['corrected_source_projection2'])
    if len(rows)!=39 or len(primary)!=33 or len(extra)!=6 or primary&extra or {r['address'] for r in rows}!=primary|extra:
        raise ValueError('Aggregate requires the exact original33 plus additional6')
    def addresses(predicate):return sorted(r['address'] for r in rows if predicate(r))
    selectors={
        'stack_SSA_and_liveins':lambda r:r['reason']=='unbound_value:register_29',
        'GP_typed_field_map':lambda r:r['reason']=='unbound_memory:GP_field_or_width',
        'vtable_target_binding':lambda r:r.get('proved_first_barrier_kind')=='native_vtable_target_binding',
        'register128_types':lambda r:r['reason'].startswith(('unbound_128','unsupported_128')),
    }
    ranking=[]
    for name,predicate in selectors.items():
        found=addresses(predicate)
        ranking.append(dict(fix=name,current_first_barrier_matches=len(found),primary_matches=len(primary&set(found)),additional_matches=len(extra&set(found)),addresses=found,measured_first_barriers_removed=0,whole_verified_units_unlocked=None,
            qualification='Conditional engineering target count, not an observed removal or a predicted full-unit unlock. No proposed fix was applied to these39 attempts.'))
    ranking.sort(key=lambda r:-r['current_first_barrier_matches'])
    for row in ranking:row['priority_rank_by_first_barrier_incidence']=1+sum(x['current_first_barrier_matches']>row['current_first_barrier_matches'] for x in ranking)
    def features(row):return row.get('original_function_context',{}).get('features')
    predicates={'non_GP_non_stack_memory':lambda f:bool(f['pointer_memory']), 'GP_operations':lambda f:bool(f['gp_accesses']), 'jalr_sites_not_proved_virtual':lambda f:bool(f['indirect_calls']), 'lq_sq_or_MMI_opcode':lambda f:bool(f['wide_register_memory'] or f['mmi_count'])}
    sets={k:set(addresses(lambda r,p=p:features(r) is not None and p(features(r)))) for k,p in predicates.items()}
    stack=set(next(row for row in ranking if row['fix']=='stack_SSA_and_liveins')['addresses'])
    burden={k:dict(selected_addresses=len(found),primary_selected_addresses=len(primary&found),additional_selected_addresses=len(extra&found),addresses=sorted(found),overlap_with_stack_first_barrier=len(found&stack),stack_overlap_addresses=sorted(found&stack)) for k,found in sets.items()}
    pairs={a+' AND '+b:dict(count=len(sets[a]&sets[b]),addresses=sorted(sets[a]&sets[b])) for i,a in enumerate(sets) for b in list(sets)[i+1:]}
    wide_details=[]
    for row in rows:
        f=features(row)
        if f is None: continue
        sp=[];nonstack=[]
        for operation in f['wide_register_memory']:
            word=int.from_bytes(bytes.fromhex(operation['word_le']),'little')
            (sp if (word>>21&31)==29 else nonstack).append(operation)
        if sp or nonstack or f['mmi_count']:
            wide_details.append(dict(address=row['address'],SP_relative_LQ_SQ=sp,nonstack_LQ_SQ=nonstack,MMI_opcode_operations=f['mmi_count']))
    wide_breakdown=dict(per_probe=wide_details,SP_relative_LQ_SQ_operation_memberships=sum(len(x['SP_relative_LQ_SQ']) for x in wide_details),SP_relative_LQ_SQ_probes=sum(bool(x['SP_relative_LQ_SQ']) for x in wide_details),nonstack_LQ_SQ_operation_memberships=sum(len(x['nonstack_LQ_SQ']) for x in wide_details),nonstack_LQ_SQ_probes=sum(bool(x['nonstack_LQ_SQ']) for x in wide_details),MMI_opcode_operations=sum(x['MMI_opcode_operations'] for x in wide_details),MMI_opcode_probes=sum(bool(x['MMI_opcode_operations']) for x in wide_details),meaning='Observed SP-relative transport, not proof of128-bit arithmetic or native type need. Eliminate only proved callee-save/spill effects after defuse,alias and native-call ABI analysis. Zero nonstack/MMI applies only to35 known-body selections; four unknown bodies and transitive callees remain unknown.')
    reasons=Counter(r['reason'] for r in rows)
    others={reason:dict(count=count,addresses=addresses(lambda r,reason=reason:r['reason']==reason)) for reason,count in reasons.items() if reason not in ('unbound_value:register_29','unbound_memory:GP_field_or_width')}
    unknown=addresses(lambda r:features(r) is None)
    return dict(schema='generated_transform_refusal_aggregate_v1',denominators=dict(primary_selected_addresses=33,additional_selected_addresses=6,union_selected_addresses=39,known_exported_body_selected_addresses=39-len(unknown),unique_known_exported_original_functions=len({r['original_function_context']['entry'] for r in rows if features(r) is not None}),existing_verified_prototypes_outside_sample=2,population_in_scope=15670),
        first_refusal_categories=dict(Counter(r['failure_category'] for r in rows)),exact_first_refusal_reasons=dict(reasons),requested_fix_ranking=ranking,other_first_barriers=others,
        original_feature_burden=burden,wide_operation_breakdown=wide_breakdown,overlapping_source_feature_pairs=pairs,unknown_original_body_addresses=unknown,
        measured_new_saved_state_verified_units=0,measured_repairs_applied_to_cohort=0,whole_unit_unlock_prediction=None,population_projection=None,
        counterfactual_prefix_walk=dict(status='NOT_RUN_UNSOUND_WITHOUT_NEW_SYMBOLIC_IR',reason='The current lowerer evaluates register expressions as constants. Supplying a guessed SP, GP value, receiver, vtable or skipping a rejected effect would fabricate semantics. No speculative prefix traversal, native emission or fake bindings were used.'),
        interpretation=['All39 general-contract attempts refused. The ranking counts which observed FIRST refusal a proposed fix would need to address; it does not measure successful barrier removal.',
            'All5 selected bodies containing jalr and all22 containing SP-relative lq/sq (128operations,0nonstack and0MMI) are already in the28 stack-first set. Of23 selected bodies with GP operations,20 are in that set. These are overlapping body features, not additional independent unlocks.',
            'Two continuations share their enclosing function bodies; source burden uses39 selected-address denominators and separately reports unique exported owners. Four selected addresses have no authenticated exported body and remain unknown.',
            'Vtable and128-bit fixes tie at0 current first refusals. This does not make them unnecessary; the parser/stack barrier prevents a measured later-barrier ranking.',
            'The separately scoped index projection has18 constructed comparisons and0/61 saved admissions; its first receiver binding is not one of the four requested fixes and earns no new transformed credit.'])

def minimal_bindings_plan(report,aggregate):
    return dict(schema='minimal_generated_transform_bindings_plan_v1',status='ENGINEERING_PLAN_NO_NEW_BINDINGS_OR_CREDIT',ordered_slices=[
        dict(order=1,name='Symbolic stack/frame SSA and typed scalar live-ins',target='00100320',first_barrier_opportunities=aggregate['requested_fix_ranking'][0]['current_first_barrier_matches'],
             original_prerequisites=['Original19-instruction body100320..10036B:16-byte stack frame,sd/ld RA atsp+0,return delay restores sp','a0 low32 is zero-extended by dsll32/dsrl32 before blez; do not infer the external native parameter type without callers','Direct original JAL00122C30 at100334 and10034C; JAL00125370 at100358 with delay-slotA1=V0 require actual callee identity,input/result widths and architectural clobber contracts','Original122C30 forms bit constants40F0000000000000 and41F0000000000000 and calls125920,125430 and125370; no float role or allocator meaning is established by those constants','Original125370 passes addresses of stack locals to1250B8,125148 and124FE0; escaped-frame aggregate extents,aliases,lifetimes and callee effects need explicit proof','BLEZ signedness/operand width must follow original R5900 instruction semantics; no decompiler-based integer or floating type guess'],
             minimum_bindings=['Symbolic frame slots with proved extents,alias exclusion and saved RA/control-flow handling; never guessed SP constants or flat RAM','Typed caller/callee live-ins, low/upper64 preservation, branch/delay semantics and source cycle-budget observables in test adapter only'],
             next_barrier_gate='After stack support rerun the unchanged selected entry and record the real next refusal. Do not bypass missing callee/bit-width/control-flow contracts or count a prefix as a transformed unit.',
             E2_acceptance='Only after the generated AST produces a register/RAM/guest-address-free kernel: fullCPU+fullRAM comparison against original AOT over every applicable pinned state, constructed signed/zero-extension/branch bounds,wrong-callee/width/value and canary mutations; record applicability. Zero legal states means no saved-state credit.'),
        dict(order=2,name='Parser/scalar-expression prerequisites and smallest typed GP field slice',target='00185EC0 plus requested0018DA30 continuation/enclosing0018D970',
             original_prerequisites=['185EC0 currently stops at cursor assignment at185EC4,not a GP map. Its original GP store at185EC8 targets0064C40C and writes low32 of-1; subsequent absolute-address/callee effects need their own contract','18DA30 first unbound GP stores are0064E348,0064E34C,0064E340,0064E344. Enclosing18D970 also touches0064E334 and0064E330; fieldownership/type is not established by zero stores','Original18DA3C JAL0016FC70,18DA44 JAL00163A50,and absolute store006602D0 at18DA50 remain obligations.18DA30 is not an independent entry'],
             minimum_bindings=['Lower actual cursor/register assignment and nested helper CallExpr without changing semantics','Bind each raw32 global field to a named native owner only after its producer/consumer/lifetime evidence establishes word vs reference semantics','For the complete18D970 body also bind original offset84 byte flags and other absolute state fields using their original widths,not a fabricated class'],
             next_barrier_gate='GP map directly targets one current first-refusal probe (1/39),which is a continuation. Its complete original function remains stack-first; typed mapping alone earns zero whole-function credit.',
             E2_acceptance='Full original-entry E2 with caller-derived live-ins,fullCPU/RAM and all globals/canaries; plant wrong GP field,wrong width,callee effect and delay-slot ordering. Continuation-only comparisons must not be promoted to a complete function.'),
        dict(order=3,name='SP-relative128-bit transport and native-call ABI preservation',target='First stack-recovered entries with original lq/sq or MMI;001225D0 is a concrete example',
             original_prerequisites=['22/39 selected addresses show128 SP-relativeLQ/SQ operations; all22 are stack-first. NonstackLQ/SQ and actualMMI opcode counts are0 in known sampled bodies; unknown bodies/callees stay unknown. This is transport evidence,not22arithmetic-type requirements or measured unlocks','001225D0 original saves s1 viaSQ@1225D8 ands0 viaSQ@1225DC,restores viaLQ@122674/122678; identify preservation vs actual128-bit arithmetic separately'],
             minimum_bindings=['Prove callee-save/spill purpose through original defuse,aliases and callee ABI. Eliminate proved compiler transport in native code instead of copying original register banks; project justified original frame effects only in E2testadapter','Preserve genuinely semantic live bits through typed value/frame objects; native output may not retain guest register banks or guest memory','Prove MMI operation widths,signedness,lane effects and carries for each reached operation; opcode membership alone is not128-bit semantics'],
             next_barrier_gate='There is no measured128-bit first refusal in39 attempts. Rerun after the earlier stack/parser fixes and record the first real width barrier; do not predict whole units from body-feature counts.',
             E2_acceptance='Full128-bit register deltas and RAM,upper/lower lane edge cases,carry/shift/sign extremes,wrong-lane/wrong-width/callee-save mutations; no credit until applicable saved-state comparison passes.'),
        dict(order=4,name='Receiver and indirect-target binding after earlier barriers',target='001225D0',
             original_prerequisites=['Original9GP operations include load0064DF54 at1225FC; conditional path loads[v0+0] at12260C then[v0+18] at122610 andJALRv0 at122614','The byte chain alone does not establish a constructor-installed table identity,complete type,concrete native slot or targetset; those remain unresolved unless joined to original constructor/table/factory evidence','Bind other observed fields0064DE88,0064C180,0064DEA4,0064DF4C and0064DE84; define null-state path and lifetime/alias constraints'],
             minimum_bindings=['Prove concrete installing constructor/table VA,original caller resource chain where ownership is claimed,slot offset18,target code addresses and any receiver adjustment','Bind native callable ABI and receiver field ownership from original evidence; no generic guest-address dispatch or fabricated class owner'],
             next_barrier_gate='0/39 current first refusals are proved vtable-target binding failures. All5 jalr-bearing selected bodies are stack-first. Concrete targets remain unknown until earlier barriers are passed and original chains verified.',
             E2_acceptance='Admit only states/constructed objects satisfying the proved table/receiver/slot contract; test null and alternate target admission,receiver adjustment,slot mutation and return/clobber effects against original AOT; no ownership/native credit from identification alone.')],
        universal_acceptance=['Freeze source/body/caller/field pins before each transform; no hand-written retail replacement or generic guest-memory shim','Generated native kernel must contain no source registers,RAM,GP base,guest addresses or guest-vtable access; native AST/object and renamed/disguised-accessor refusals remain mandatory','Separate actual first-barrier removals,successful generated-body emission,constructed comparisons,saved-state E2 and credited/native production adoption at every stage','Existing two prototype credits remain separate; any production replacement also needs the existing perfunction ownership gate'])

def aggregate_self_tests(report):
    import copy
    expected=refusal_aggregate(report)
    missing=copy.deepcopy(report)
    missing['samples']=[r for r in missing['samples'] if r['address']!='001a3df0']
    try:refusal_aggregate(missing)
    except ValueError:pass
    else:raise AssertionError('Original primary probe deletion admitted')
    planted=copy.deepcopy(report)
    for row in planted['samples']:
        features=row.get('original_function_context',{}).get('features')
        if features is not None:features['mmi_count']+=1
    changed=refusal_aggregate(planted)
    signature=lambda a:[(x['fix'],x['current_first_barrier_matches']) for x in a['requested_fix_ranking']]
    if signature(expected)!=signature(changed):raise AssertionError('Source-feature presence became a first-barrier unlock prediction')
    planted=copy.deepcopy(report)
    next(r for r in planted['samples'] if r['address']=='0012c440')['reason']='unbound_indirect_target:return_capture'
    changed=refusal_aggregate(planted)
    if next(r for r in changed['requested_fix_ranking'] if r['fix']=='vtable_target_binding')['current_first_barrier_matches']:
        raise AssertionError('Parser return-capture refusal became virtual-dispatch proof')
    if any(r['measured_first_barriers_removed'] or r['whole_verified_units_unlocked'] is not None for r in expected['requested_fix_ranking']):
        raise AssertionError('Unimplemented repair earned measured removals or whole-unit unlocks')
    return dict(status='PASS',scope='Planted metadata controls for aggregate inference only; no original bytes, native kernels or ledger admission modified',controls=['Dropped original primary probe refused','Added source MMI feature does not increase first-barrier opportunities','Indirect-target return/parser refusal is not a proved vtable failure','No proposed repair earns measured removals or whole-unit unlocks'])


def markdown_tables(tables):
    def cell(value): return str(value).replace("|", "&#124;").replace("\n", " ")
    def hard(h):
        if h is None: return "Unknown"
        def fmt(key, render): return render(h[key]) if h[key] is not None else "unknown body"
        return "; ".join(["P="+fmt("pointers",lambda v:f"{v['non_GP_non_stack_memory_operations']} non-GP/non-stack memory,{v['stack_operations']} stack"),
                          "G="+fmt("GP_globals",lambda v:str(v['operations'])+" GP ops"),
                          "V="+fmt("vtable_indirect",lambda v:str(v['jalr_operations'])+" jalr; typed vtable unproved"),
                          "Q="+fmt("register128",lambda v:f"{v['lq_sq_operations']} lq/sq,{v['MMI_opcode_operations']} MMI"),
                          "O=main ELF input", "B="+str(h['cycle_budget']['generated_budget_debits_processed_before_refusal'])+" debits processed; scheduling unproved"])
    def refusal(row):
        v=row["first_compiler_refusal"]
        return v["category"]+": "+v["reason"]+" @"+str(v["pc"]) if v else "None"
    def cost(row):
        v=row["measured_cost"]
        if not v or v.get("shared_AST_seconds") is None:return "Not attempted; no native runtime"
        result=f"AST {v['shared_AST_seconds']:.4f}s shared/{v['AST_shared_by']} in {v['AST_share_unit']}; lower {v['lowering_seconds']*1000:.3f}ms; native runtime unmeasured for refused contract"
        if "separate_constructed_receiver_benchmark" in v:
            b=v["separate_constructed_receiver_benchmark"]
            result+=f"; separate scoped kernel {b['calls_per_route']} calls/route: AOT {b['generated_ns']}ns,typed {b['typed_ns']}ns,constructed receiver only"
        return result
    def deeper(row):
        d=row["deeper_original_findings"]
        if not d or d["instruction_count"] is None:return "No authenticated exported body; deeper operations unknown"
        return f"Owner {d['original_entry']}; {d['instruction_count']} original instructions; "+", ".join(f"{k}:{v}({d['instruction_type_handling'][k]})" for k,v in sorted(d["memory_instruction_kinds"].items()))+"; C++ types/aliases unproved"
    lines=["<!-- BEGIN GENERATED 5S FULL TABLES -->", "### Full measured 5S function and module tables", "",
           "The primary cohort remains the original **33 selected addresses**. Four supplemental probes and two corrected source projections are additional: **39 unique addresses**, with no substitution. Primary **0/33** and additional **0/6** gained saved-state-verified transformed credit. The two pre-existing overlay successes are a separate cohort, retaining **2/15,670** catalogue credit. These selected samples support no population or game-runtime extrapolation.", "",
           "Six hard classes: **P** = original non-GP/non-stack memory bases (32-bit receiver/field ownership remains unproved); **G** = original GP operations; **V** = original `jalr` sites (typed vtable binding remains unproved); **Q** = original `lq`/`sq` and MMI opcode operations; **O** = selected overlay/main input; **B** = generated budget debits processed before refusal. Zero observed body operations does not establish absence in callees or unobserved execution. An unauthenticated body stays unknown. Compiler first refusal is separate from deeper findings. AST time is shared by all listed entries in a generated unit, so row times must not be summed as independent parses.", ""]
    for title,rows in [("Original 33 primary selections",tables["primary33"]),("Four supplemental probes and two corrected projections",tables["additional6"])]:
        lines += ["#### "+title,"", "| Address / cohort | Original module association → current binding | Six hard-class observations | First compiler refusal | Deeper original findings / type limits | Pointer/ABI work needed | Measured cost | Result |", "|---|---|---|---|---|---|---|---|"]
        for row in rows:
            association=", ".join(row["prior_native_module_associations"]) or "No old module claim"
            binding=", ".join(row["current_source_bound_modules"]) or "Unproved; old probe association is not native correspondence"
            values=[row["address"]+" / "+row["cohort"],association+" → "+binding,hard(row["six_hard_classes"]),refusal(row),deeper(row),"; ".join(row["pointer_ABI_requirements"]),cost(row),row["result"]]
            if row["address"]=="0014de80":values[-1]+="; separate receiver contract:18 constructed pass,0/61 saved admissions,0 credit"
            lines.append("| "+" | ".join(map(cell,values))+" |")
        lines.append("")
    lines += ["#### Existing two verified prototypes (outside both sample denominators)","", "| Function | Bound native ABI / six hard classes | Differential and result | Isolated measured cost |", "|---|---|---|---|"]
    for row in tables["existing_success2"]:
        b=row["measured_cost"]["benchmark"]
        values=[row["module"]+":"+row["address"]+" "+row["native_name"],"; ".join(k+"="+v for k,v in row["six_hard_classes"].items()),row["result"],f"Shared {b['calls_per_route']} calls/route: AOT {b['generated_ns']}ns; typed {b['typed_ns']}ns; not per-function/frame/population cost"]
        lines.append("| "+" | ".join(map(cell,values))+" |")
    lines += ["", "#### All 23 native module groups", "", "All five retail-logic groups have a bounded source-declared original projection attempted. The mixed Session game-side mapping remains unproved, so 5S.1 remains partially BLOCKED. The 17 platform-boundary groups retain native services; they do not require an invented generated retail counterpart. For an unbound group the observations below describe only its historical original probe, not the native implementation.","", "| Native group / category | Historical probe → source-bound original projection | Six hard-class observations (scope explicit) | First compiler refusal | Deeper original findings | Pointer/ABI requirements | Measured cost | Result |", "|---|---|---|---|---|---|---|---|"]
    for row in tables["native_module23"]:
        values=[row["name"]+" / "+row["classification"],str(row["historical_probe"])+" → "+str(row["source_bound_projection"]),row["observation_scope"]+": "+hard(row["six_hard_classes"]),refusal(row),deeper(row),"; ".join(row["pointer_ABI_requirements"]),cost(row),row["result"]]
        lines.append("| "+" | ".join(map(cell,values))+" |")
    lines += ["", "#### Technique fractions with explicit observed denominators", "", "| Hard class | Original-primary entries with observed evidence | Saved-state-verified transforms among those entries |", "|---|---|---|"]
    for key,value in tables["technique_observed_fractions"].items():
        lines.append(f"| {key} | {value['observed_denominator']}/33 | 0/{value['observed_denominator']}"+(" (no primary overlay input; undefined success fraction)" if value['observed_denominator']==0 else "")+" |")
    lines += ["", "These are observed-feature subsets, not independent success rates for techniques the compiler never reached. The sole relative-receiver extension passed 18 constructed CPU/RAM comparisons but all 61 original GP-global receivers were null, so its saved-state result remains 0 admitted/61 inapplicable and zero credit. Wrong-value, wrong-field, disguised memory accessor, forged PASS/zero-admission and changed-output controls reject as recorded in the existing verification record.", ""]
    aggregate = tables["refusal_aggregate"]
    lines += ["", "#### Aggregate first barriers and requested-fix ranking", "", "Every general-contract attempt refused: original33 plus6 additional,39 total. The counts below are current first-refusal opportunities only. No proposed fix was applied, no first-barrier removal was measured, and whole-unit unlock remains UNMEASURED. Original source-feature counts overlap and are not unlock predictions.", "", "| Requested fix | Current first-refusal matches /39 | Primary /33 | Additional /6 | Measured removals | Whole-unit unlock |", "|---|---|---|---|---|---|"]
    for row in aggregate["requested_fix_ranking"]:
        lines.append(f"| {row['fix']} | {row['current_first_barrier_matches']}/39 | {row['primary_matches']}/33 | {row['additional_matches']}/6 | 0 | UNMEASURED |")
    lines += ["", "Exact other first barriers: " + "; ".join(reason+"="+str(count) for reason,count in aggregate["exact_first_refusal_reasons"].items() if reason not in ("unbound_value:register_29", "unbound_memory:GP_field_or_width")) + ".", "", "| Original body feature (overlapping selected-address observations) | Present /39 | Also stack-first |", "|---|---|---|"]
    for key, row in aggregate["original_feature_burden"].items():
        lines.append(f"| {key} | {row['selected_addresses']}/39 | {row['overlap_with_stack_first_barrier']}/28 |")
    lines += ["", "All 5 jalr-bearing selections and all 22 SP-relative LQ/SQ-bearing selections are already stack-first; 20 of 23 GP-bearing selections overlap that set. The wide evidence is 128 SP-relative LQ/SQ operations, 0 nonstack LQ/SQ and 0 actual MMI opcodes in known sampled bodies. This is transport evidence,not 22 arithmetic-type requirements. Four addresses lack an authenticated exported body. Two continuation probes share enclosing function evidence, so35 known-body selections correspond to33 distinct exported functions. None of these counts proves a native vtable type, native width semantics, later compiler reachability or whole-unit success.", "", "An analysis-only counterfactual prefix walk was not run: the current lowerer evaluates register expressions as constants. Guessing SP/GP/receiver values or skipping a rejected effect would fabricate semantics. The actual next refusal must be measured after implementing a justified symbolic/typed contract.", "", "#### Minimal next bindings and acceptance plan", ""]
    for step in tables["minimal_bindings_plan"]["ordered_slices"]:
        lines += [str(step["order"])+". **"+step["name"]+"** — "+step["target"]+".", "", "   Original prerequisites: "+"; ".join(step["original_prerequisites"])+".", "", "   Minimum bindings: "+"; ".join(step["minimum_bindings"])+".", "", "   Next-barrier gate: "+step["next_barrier_gate"], "", "   E2 acceptance: "+step["E2_acceptance"], ""]
    lines += ["SP-relative128-bit transport is listed before target binding because it participates in frame/call-ABI analysis, not because 22 arithmetic types or future unit unlocks have been measured. Callee-save/spill elimination still needs defuse and alias proof. Both requested fixes tie at 0 observed first refusals.", "", "<!-- END GENERATED 5S FULL TABLES -->", ""]
    return "\n".join(lines)


def publish_existing_tables(parent, output, report, tables, markdown):
    if report.get('frame_stage'):raise ValueError('legacy_before_frame_publication_refuses_frame_stage_report')
    path=parent/"research/runs/phase4-aot-completion-20261124/VERIFICATION.json"
    record=json.loads(path.read_bytes());section=record["instruction_phase5S"]
    if "prior_37_entry_study" not in section: section["prior_37_entry_study"] = section["study"]
    section["study"] = report
    section["study_receipt"] = {"path": (output/"RESULT.json").relative_to(parent/"external/forks/ps2-native-recomp").as_posix(), "sha256":sha(output/"RESULT.json")}
    section["full_human_readable_tables"] = tables
    section["full_human_readable_markdown"] = markdown
    section["refusal_aggregate"] = tables["refusal_aggregate"]
    section["refusal_aggregate_controls"] = tables["refusal_aggregate_controls"]
    section["minimal_bindings_plan"] = tables["minimal_bindings_plan"]
    section["measured_coverage"].update(attempted_entries=39, primary_cohort_entries=33, additional_entries=6,
        failure_categories=dict(__import__('collections').Counter(s["failure_category"] for s in report["samples"])),
        sample_accepted_fraction={"numerator":0,"denominator":33}, additional_accepted_fraction={"numerator":0,"denominator":6},population_projection=None)
    section["required_sample_result"] = [{key:s.get(key) for key in ("address","entry_authenticated","generated_unit","entry_case","status","failure_category","reason","first_unlowered_pc","differential")} for s in report["samples"] if int(s["address"],16) in MANDATORY]
    path.write_text(json.dumps(record,indent=2,ensure_ascii=True)+"\n",encoding="utf-8",newline="\n")
    status=parent/"PORT_STATUS.md";text=status.read_text(encoding="utf-8")
    begin="<!-- BEGIN GENERATED 5S FULL TABLES -->";end="<!-- END GENERATED 5S FULL TABLES -->"
    if begin in text:
        old=text[text.index(begin):text.index(end)+len(end)]
        text=text.replace(old,"",1)
    summary = ">   - **5S measured boundary**: All 39 selected-address probes refused (the original 33 plus 6 additional). Stack/live-ins account for 28 first refusals and GP fields for 1; vtable and 128-bit bindings account for 0 each at the current first barrier. Later barriers and whole-unit unlock remain unmeasured; the 2 existing verified prototypes are separate. Full tables, aggregate and plan: parent repository work-item record `research/runs/phase4-aot-completion-20261124/VERIFICATION.json`, section `instruction_phase5S`."
    if "**5S measured boundary**" in text:
        text="\n".join(summary if "**5S measured boundary**" in line else line for line in text.splitlines())
    else:
        text=text.rstrip()+"\n\n"+summary
    status.write_text(text.rstrip()+"\n",encoding="utf-8",newline="\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--parent-root", type=Path, required=True)
    parser.add_argument("--census", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--compiler", type=Path)
    parser.add_argument("--stack-frames",type=Path,help="Original-derived28 frame plans for actual symbolic AST lowering; no guessed SP")
    parser.add_argument("--frame-before-study",type=Path,help="Pinned existing39 compiler-frontier baseline")
    parser.add_argument('--borrowed-reference-map',action='store_true',help='S6 experimental original-validated reference-clear IR; no native owner binding')
    parser.add_argument('--symbol-before-study',type=Path,help='Frozen S5-before-S6 actual39 report')
    parser.add_argument('--lui-constants',action='store_true',help='Opt-in original/AST joined signed32 LUI low64 SSA binding')
    parser.add_argument('--lui-before-study',type=Path,help='Published historical S6 actual39 comparison')
    parser.add_argument('--lui-current-before-study',type=Path,help='Fresh same-source S6 report with LUI opt-in disabled')
    parser.add_argument('--mtc1-bits',action='store_true',help='Opt-in original/AST joined GPR low32 to FPR raw32 bit-move SSA')
    parser.add_argument('--mtc1-before-study',type=Path,help='Published historical LUI-on actual39 comparison')
    parser.add_argument('--mtc1-current-before-study',type=Path,help='Fresh same-source LUI-on report with MTC1 disabled')
    parser.add_argument('--scalar-ssa',action='store_true',help='Opt-in original/AST joined nontrapping 32/64 integer SSA; no invented inputs')
    parser.add_argument('--scalar-before-study',type=Path,help='Published MTC1-on actual39 comparison')
    parser.add_argument('--scalar-current-before-study',type=Path,help='Fresh same-source MTC1-on/scalar-disabled actual39')
    parser.add_argument("--publish-existing-tables", action="store_true", help="Update only existing parent instruction_phase5S evidence/tables and short PORT_STATUS summary")
    parser.add_argument("--render-existing-study", type=Path, help="Render already measured39-entry evidence with current pinned positive receipts; never rerun or relabel compiler attempts")
    args = parser.parse_args()
    immutable_tools={Path(__file__).resolve():sha(Path(__file__)),Path(transform.__file__).resolve():sha(Path(transform.__file__))}
    if args.stack_frames:
        frame_source=Path(__file__).with_name('stack_frame_analysis.py').resolve();immutable_tools[frame_source]=sha(frame_source)
    if args.borrowed_reference_map:
        symbol_source=Path(__file__).with_name('retail_data_symbol_map.py').resolve();immutable_tools[symbol_source]=sha(symbol_source)
        if not args.stack_frames or not args.symbol_before_study:raise ValueError('S6 requires separate frozen S5 evidence and frame plans')
    if args.lui_constants and (not args.borrowed_reference_map or not args.lui_before_study or not args.lui_current_before_study):
        raise ValueError('LUI opt-in requires original and fresh LUI-disabled S6 comparison reports')
    if args.mtc1_bits and (not args.lui_constants or not args.mtc1_before_study or not args.mtc1_current_before_study):
        raise ValueError('MTC1 requires original and same-source LUI-on/MTC1-disabled reports')
    if args.scalar_ssa and (not args.mtc1_bits or not args.scalar_before_study or not args.scalar_current_before_study):
        raise ValueError('Scalar SSA requires original and same-source MTC1-on/scalar-disabled reports')
    parent = args.parent_root.resolve()
    fork = parent / "external/forks/ps2-native-recomp"
    output = args.output.resolve()
    if output.exists() or not output.is_relative_to((fork / "build/oracle-telemetry").resolve()):
        raise ValueError("Study output must be a new owned telemetry directory")
    output.mkdir()
    if args.render_existing_study:
        report = json.loads(args.render_existing_study.read_bytes())
        if report.get('frame_stage'):raise ValueError('legacy_before_frame_tables_refuse_frame_stage_report')
        if ({int(s["address"],16) for s in report["samples"]} != set(ORIGINAL_PRIMARY) | set(SUPPLEMENTAL) | CORRECTED_PROJECTIONS
            or len(report["samples"]) != 39 or report["inputs"]["transformer_sha256"] != sha(Path(transform.__file__))):
            raise ValueError("Existing study cohort or measured compiler changed; rerun39 attempts")
        # Preserve the actual measurement bytes and original producer hash.
        (output/"RESULT.json").write_bytes(args.render_existing_study.read_bytes())
        tables = full_tables(report, fork)
        markdown = markdown_tables(tables)
        (output/"full-tables.json").write_text(json.dumps(tables,indent=2)+"\n",encoding="utf-8",newline="\n")
        if args.publish_existing_tables:
            publish_existing_tables(parent, output, report, tables, markdown)
        print("RENDERED_EXISTING_MEASURED_STUDY", output, "samples39; no compiler attempts rerun")
        return
    census = json.loads(args.census.read_bytes())
    frame_plans={};frame_before={};symbol_before={};symbol_controls=None;lui_before={};lui_current={};mtc1_before={};mtc1_current={};scalar_before={};scalar_current={}
    if args.stack_frames:
        if not args.frame_before_study:raise ValueError('Frame study requires the exact original39 before receipt')
        frame_data=json.loads(args.stack_frames.read_bytes())
        frame_plans={row['source_entry']:row for row in frame_data['targets']}
        before_data=json.loads(args.frame_before_study.read_bytes());frame_before={row['address']:row for row in before_data['samples']}
        if len(frame_before)!=39 or len(frame_plans)!=28:raise ValueError('Frame study cohort changed')
    if args.borrowed_reference_map:
        symbol_baseline=json.loads(args.symbol_before_study.read_bytes())
        if symbol_baseline.get('frame_stage',{}).get('stage')!='S5_BEFORE_ANY_S6_GP_BINDING':raise ValueError('S6 baseline is not an actual S5-before-S6 report')
        symbol_before={row['address']:row for row in symbol_baseline['samples']}
        if len(symbol_before)!=39:raise ValueError('S6 changed original39 cohort')
    if args.lui_constants:
        for path,target in ((args.lui_before_study,lui_before),(args.lui_current_before_study,lui_current)):
            before=json.loads(path.read_bytes())
            if before.get('frame_stage',{}).get('stage')!='S6_EXPERIMENTAL_BORROWED_REFERENCE_CLEAR':raise ValueError('LUI comparison requires S6 with LUI disabled')
            target.update({row['address']:row for row in before['samples']})
        if len(lui_before)!=39 or lui_before.keys()!=lui_current.keys():raise ValueError('LUI comparison changed original39')
        if any((lui_before[key]['reason'],lui_before[key]['first_unlowered_pc'])!=(lui_current[key]['reason'],lui_current[key]['first_unlowered_pc']) for key in lui_before):
            raise ValueError('Fresh LUI-disabled source changed historical S6 frontiers')
    if args.mtc1_bits:
        for path,target in ((args.mtc1_before_study,mtc1_before),(args.mtc1_current_before_study,mtc1_current)):
            before=json.loads(path.read_bytes())
            if before.get('frame_stage',{}).get('stage')!='LUI_CONSTANT_BINDING_OPT_IN' or before.get('mtc1_stage'):
                raise ValueError('MTC1 comparison requires LUI-on with MTC1 disabled')
            if path==args.mtc1_current_before_study and (before['inputs']['transformer_sha256']!=sha(Path(transform.__file__)) or before['inputs']['study_tool_sha256']!=sha(Path(__file__))):
                raise ValueError('MTC1 disabled comparison must use the same frozen sources')
            target.update({row['address']:row for row in before['samples']})
        if len(mtc1_before)!=39 or mtc1_before.keys()!=mtc1_current.keys() or any((mtc1_before[key]['reason'],mtc1_before[key]['first_unlowered_pc'])!=(mtc1_current[key]['reason'],mtc1_current[key]['first_unlowered_pc']) for key in mtc1_before):
            raise ValueError('Fresh MTC1-disabled comparison changed published LUI39 frontiers')
    if args.scalar_ssa:
        for path,target in ((args.scalar_before_study,scalar_before),(args.scalar_current_before_study,scalar_current)):
            before=json.loads(path.read_bytes())
            if before.get('frame_stage',{}).get('stage')!='MTC1_RAW32_BIT_MOVE_OPT_IN' or before.get('scalar_stage'):raise ValueError('Scalar comparison requires MTC1-on/scalar-off')
            if path==args.scalar_current_before_study and (before['inputs']['transformer_sha256']!=sha(Path(transform.__file__)) or before['inputs']['study_tool_sha256']!=sha(Path(__file__))):raise ValueError('Scalar disabled comparison must use same frozen sources')
            target.update({row['address']:row for row in before['samples']})
        if len(scalar_before)!=39 or scalar_before.keys()!=scalar_current.keys() or any((scalar_before[k]['reason'],scalar_before[k]['first_unlowered_pc'])!=(scalar_current[k]['reason'],scalar_current[k]['first_unlowered_pc']) for k in scalar_before):raise ValueError('Fresh scalar-disabled comparison changed published MTC1 frontiers')
    contract_path = fork / "recomp/overlay_initializer_contract.json"
    contract = json.loads(contract_path.read_bytes())
    generated = fork / "build/generated-aot/phase4-all"
    compiler = args.compiler or parent.parent / "dependencies/compiler/llvm-mingw-20261006-ucrt-x86_64/bin/clang++.exe"
    modules_file = fork / "research/ledger/native_module_classification.json"
    modules = json.loads(modules_file.read_bytes())["modules"]
    registrations = {row["plugin_id"]: int(row["constructor"], 16) for row in census["registrations"]}
    reviews = []
    samples = {address: {"address": f"{address:08x}", "reasons": [reason], "native_modules": []}
               for address, reason in MANDATORY.items()}
    for address, associations in ORIGINAL_PRIMARY.items():
        sample = samples.setdefault(address, {"address": f"{address:08x}", "reasons": [], "native_modules": []})
        sample["cohort"] = "original33"
        sample["prior_native_module_associations"] = associations
        sample["prior_association_scope"] = "Historical selection only; not proof of original/native correspondence. Current source bindings are recorded separately."
        sample["reasons"].append("preserved original33 primary cohort")
    for module in modules:
        prefix = module["name"].split()[0].lower()
        binding = COUNTERPARTS.get(module["name"])
        # Retain the old exploratory callback probes, but never call their IDs
        # a native function correspondence. Corrected source bindings are extra
        # probes; they never replace the original33 cohort.
        address = binding[0] if binding else registrations.get(prefix, BOUNDARIES.get(module["name"]))
        if address is None:
            raise ValueError("Missing native-module sample selection: " + module["name"])
        sample = samples.setdefault(address, {"address": f"{address:08x}", "reasons": [], "native_modules": []})
        review = {**module, "source_sha256": {name: sha(fork / name) for name in module["sources"]}}
        if binding:
            _, source, witness = binding
            lines = (fork / source).read_text(encoding="utf-8").splitlines()
            matches = [i+1 for i, line in enumerate(lines) if witness in line]
            if len(matches) != 1 or source not in module["sources"]:
                raise ValueError("Native counterpart source witness changed")
            review.update(status="SOURCE_DECLARED_RETAIL_PROJECTION", original_entry=f"{address:08x}",
                          witness={"path": source, "line": matches[0], "text": lines[matches[0]-1], "sha256": sha(fork/source)},
                          limit="Source-declared bounded projection, not whole routine equivalence or generated/native E2.")
            if module["name"] == "Main Reentry & Frame Loop":
                # Main calls a separately implemented original task dispatcher;
                # 12E4E0 is not the entry of the complete native main loop.
                links = [("runtime/src/native/retail_main.cpp", "!dispatch_retail_task_lists(m, f)"),
                         ("runtime/include/mka/retail_main.hpp", "// 12E4E0"),
                         ("runtime/src/native/retail_task_dispatch_main.cpp", "dispatch_first_root_zero_flags_from_12e4e0(")]
                review["call_chain_witnesses"] = []
                for path, text in links:
                    content = (fork/path).read_text(encoding="utf-8").splitlines()
                    locations = [i+1 for i,line in enumerate(content) if text in line]
                    if len(locations) != 1:
                        raise ValueError("Main to original task-dispatch source chain changed")
                    review["call_chain_witnesses"].append({"path":path, "line":locations[0], "text":content[locations[0]-1], "sha256":sha(fork/path)})
                review["limit"] = "Main calls the separately source-bound 12E4E0 task dispatcher; it is not the entry or complete semantics of native Main."
            sample["native_modules"].append({"name": module["name"], "source_witness": review["witness"]})
            sample["reasons"].append("exact native source-declared original projection")
        else:
            review.update(status="NO_PROVEN_ONE_TO_ONE_RETAIL_LOGIC_COUNTERPART", original_entry=None,
                          limit="Native host/platform/domain service; names, numeric prefixes and nearby retail callers do not establish a one-to-one original routine.")
            sample["reasons"].append("exploratory original lifecycle callback; not bound to native module" if prefix in registrations else "exploratory domain probe; not bound to native service")
        reviews.append(review)
    for address, reason in SUPPLEMENTAL.items():
        sample = samples.setdefault(address, {"address": f"{address:08x}", "reasons": [], "native_modules": []})
        sample["reasons"].append(reason)
    for address, sample in samples.items():
        sample.setdefault("cohort", "supplemental4" if address in SUPPLEMENTAL else "corrected_source_projection2")
    if set(samples) != set(ORIGINAL_PRIMARY) | set(SUPPLEMENTAL) | CORRECTED_PROJECTIONS or len(samples) != 39:
        raise ValueError("Primary33 plus supplemental4 plus corrected2 cohort identity changed")
    validate_sample_coverage(samples, modules, reviews)
    export = parent / "research/runs/ghidra-function-coverage-20261118/export-20261118-230132-1875-18160"
    with (export / "function-bodies.tsv").open(encoding="utf-8", newline="") as stream:
        ranges = list(csv.DictReader(stream, delimiter="\t"))
    catalogue = {int(row["entry"], 16) for row in ranges}
    from bounded_ownership import code_ranges
    owner = code_ranges(export)
    original_path = parent / "research/inputs/SLUS_214.10"
    image = original_path.read_bytes()
    if sha(original_path) != "a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f":
        raise ValueError("Original ELF pin changed")
    phoff = struct.unpack_from("<I", image, 28)[0]
    stride, count = struct.unpack_from("<HH", image, 42)
    segments = [struct.unpack_from("<8I", image, phoff+i*stride) for i in range(count)]
    def original(va, size):
        matches = [image[off+va-base:off+va-base+size] for kind,off,base,_,length,_,_,_ in segments
                   if kind == 1 and base <= va and va+size <= base+length]
        if len(matches) != 1 or len(matches[0]) != size:
            raise ValueError("Original address is not uniquely file backed")
        return matches[0]
    lui_expected=set()
    if args.lui_constants:
        for address,row in lui_before.items():
            if row.get('first_unlowered_pc') is None:continue
            word=int.from_bytes(original(int(row['first_unlowered_pc'],16),4),'little')
            if word>>26==0x0f and word>>16&31 not in (0,28,29,31):lui_expected.add(address)
        if len(lui_expected)!=15:raise ValueError('Published original-opcode LUI target set is not the expected15')
    mtc1_expected=set()
    if args.mtc1_bits:
        for address,row in mtc1_before.items():
            if row.get('first_unlowered_pc') is None:continue
            word=int.from_bytes(original(int(row['first_unlowered_pc'],16),4),'little')
            if word>>26==0x11 and word>>21&31==4 and word&0x7ff==0:mtc1_expected.add(address)
        if mtc1_expected!={'001a64a0'}:raise ValueError('Published current MTC1 first-op target set changed')
    scalar_expected=set()
    if args.scalar_ssa:
        for address,row in scalar_before.items():
            if row.get('first_unlowered_pc') is None:continue
            shape=transform.scalar_word_shape(int.from_bytes(original(int(row['first_unlowered_pc'],16),4),'little'))
            if shape is not None and shape['destination'] not in (28,29,31):scalar_expected.add(address)
    selected_owners = {owner(address) for address in samples}
    original_features = {entry: {"instructions": 0, "gp_accesses": [], "indirect_calls": [], "wide_register_memory": [], "pointer_memory": [], "stack_memory_count": 0, "mmi_count": 0, "load_store_kinds": {}}
                         for entry in selected_owners if entry is not None}
    with (export / "instructions.tsv").open(encoding="utf-8", newline="") as stream:
        for row in csv.DictReader(stream, delimiter="\t"):
            entry = int(row["function_entry"], 16) if row["function_entry"] else None
            if entry not in original_features:
                continue
            feature = original_features[entry]
            feature["instructions"] += 1
            if original(int(row["address"], 16), len(bytes.fromhex(row["bytes"]))) != bytes.fromhex(row["bytes"]):
                raise ValueError("Original instruction/export mismatch")
            word = int.from_bytes(bytes.fromhex(row["bytes"]), "little")
            opcode, base = word >> 26, word >> 21 & 31
            evidence = {"pc": row["address"], "word_le": row["bytes"].lower(), "instruction": row["mnemonic"]}
            memory_opcodes = {0x1e,0x1f,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x31,0x35,0x37,0x39,0x3d,0x3f}
            if opcode in memory_opcodes:
                operation = row["mnemonic"].split()[0].lstrip("_")
                feature["load_store_kinds"][operation] = feature["load_store_kinds"].get(operation, 0) + 1
                if base == 29:
                    feature["stack_memory_count"] += 1
                elif base != 28:
                    feature["pointer_memory"].append(dict(evidence, base_register=base, offset=word & 0xffff))
            if opcode == 0x1c:
                feature["mmi_count"] += 1
            if base == 28 and opcode in memory_opcodes:
                offset = word & 0xffff
                if offset >= 0x8000:
                    offset -= 0x10000
                evidence["original_global_va"] = f"{(int(contract['source_gp'], 16)+offset)&0xffffffff:08x}"
                evidence["native_field_binding"] = next((field for field in contract["fields"]
                                                          if field["source_address"].lower() == evidence["original_global_va"]), None)
                feature["gp_accesses"].append(evidence)
            if opcode == 0 and word & 63 == 9:
                feature["indirect_calls"].append(evidence)
            if opcode in (0x1e, 0x1f):
                feature["wide_register_memory"].append(evidence)
    registered = {int(row[key], 16) for row in census["registrations"] for key in ("constructor", "destructor")}
    unit_groups = {}
    for unit in sorted(generated.glob("unit_*.cpp")):
        text = unit.read_text(encoding="utf-8")
        cases = {int(address, 16): int(case) for case, address in re.findall(r"case (\d+)u: goto L_([0-9a-fA-F]+);", text)}
        for address in samples.keys() & cases.keys():
            if "generated_unit" in samples[address]:
                raise ValueError("Ambiguous generated entry")
            samples[address].update(generated_unit=unit.name, entry_case=cases[address])
            unit_groups.setdefault(unit.name, []).append(address)
    selected = list(samples.values())
    (output / "sample.json").write_text(json.dumps(selected, indent=2) + "\n", encoding="utf-8", newline="\n")
    results = []
    first_unit = generated / next(iter(unit_groups))
    controls = study_controls(samples, modules, reviews, first_unit, census["aot"]["all_measured_source_sha256"][first_unit.name])
    started = time.perf_counter()
    for unit_name, addresses in unit_groups.items():
        unit = generated / unit_name
        expected = census["aot"]["all_measured_source_sha256"][unit_name]
        verify_selected_unit(unit, expected)
        text = unit.read_text(encoding="utf-8")
        begin = time.perf_counter()
        definitions = transform.clang_ast(compiler, unit, [generated, fork / "runtime/include"], unit.stem, fork)
        definition = transform.function_definition(definitions, unit.stem)
        ast_seconds = time.perf_counter() - begin
        for address in addresses:
            sample = samples[address]
            label = "L_" + sample["address"].upper() + ":"
            segment = text.split(label, 1)[1].split("\nL_", 1)[0]
            authenticated = address in catalogue or address in registered
            descriptor = {"source_entry": sample["address"], "entry_case": sample["entry_case"],
                          "unit_name": unit.stem, "module": "SLUS_214.10"}
            if sample['address'] in frame_plans:
                descriptor.update(_frame_plan=frame_plans[sample['address']],_frame_original=original)
            if args.borrowed_reference_map:descriptor['_data_symbol_image']=image
            if args.lui_constants:descriptor['_lui_original']=original
            if args.mtc1_bits:descriptor['_mtc1_original']=original
            if args.scalar_ssa:descriptor['_scalar_original']=original
            begin = time.perf_counter()
            lowering = (LUIFailureTrace if args.lui_constants else transform.Lowering)(definition, descriptor, contract)
            reason = None;fatal_error=None
            try:
                ir = lowering.lower()
            except transform.Refusal as error:
                reason = str(error);fatal_error=error
                ir = None
            elapsed = time.perf_counter() - begin
            result = dict(sample, source_unit_sha256=expected, entry_authenticated=authenticated,
                          source_span_count=sum(int(row["entry"], 16) == address for row in ranges),
                          generated_entry_fragment=segment[:3000], ast_parse_seconds=ast_seconds,
                          ast_parse_shared_by=len(addresses), lowering_seconds=elapsed,
                          first_unlowered_pc=f"{lowering.pc:08x}", lowered_effects=len(lowering.effects),
                          source_budget_debits_processed=lowering.debit)
            if args.stack_frames:
                old=frame_before[sample['address']]
                result['before_frame_frontier']={k:old.get(k) for k in ('reason','first_unlowered_pc','failure_category')}
                result['frame_lowering']=lowering.frame.report() if lowering.frame else None
            if args.borrowed_reference_map:
                result['before_symbol_frontier']={k:symbol_before[sample['address']].get(k) for k in ('reason','first_unlowered_pc','failure_category')}
                result['borrowed_reference_IR']=lowering.data_symbols.report()
                if address==0x18da30:symbol_controls=transform.borrowed_reference_self_tests(definition,descriptor,contract)
            if args.lui_constants:
                result['lui_bindings']=lowering.lui_bindings
                result['before_LUI_frontier']={key:lui_before[sample['address']].get(key) for key in ('reason','first_unlowered_pc','failure_category')}
            if args.mtc1_bits:
                result['mtc1_bindings']=lowering.mtc1_bindings
                result['before_MTC1_frontier']={key:mtc1_before[sample['address']].get(key) for key in ('reason','first_unlowered_pc','first_failure_category','first_failure_cause')}
            if args.scalar_ssa:
                result['scalar_bindings']=lowering.scalar_bindings;result['scalar_candidates']=lowering.scalar_candidates
                result['before_scalar_frontier']={key:scalar_before[sample['address']].get(key) for key in ('reason','first_unlowered_pc','first_failure_category','first_failure_cause')}
            containing = owner(address)
            result["original_function_context"] = {"entry": f"{containing:08x}" if containing is not None else None,
                                                   "features": original_features.get(containing)}
            spans = [row for row in ranges if containing is not None and int(row["entry"],16)==containing]
            result["original_function_context"]["body_spans"] = [
                {"start": row["start"], "end": row["end"], "sha256": hashlib.sha256(original(int(row["start"],16), int(row["end"],16)-int(row["start"],16)+1)).hexdigest()} for row in spans]
            result["original_function_context"]["evidence_scope"] = "Complete body instruction bytes joined to pinned ELF; feature presence is separate from the immediate lowerer refusal."
            if reason is None:
                result.update(status="LOWERED_REQUIRES_FUNCTION_ABI_AND_DIFFERENTIAL",
                              failure_category="unbound function ABI or owner layout",
                              reason="Existing scalar initializer IR is insufficient to authenticate this distinct function ABI.",
                              typed_ir=ir)
            else:
                result.update(status="REFUSED", failure_category=classify_failure(reason, segment, authenticated), reason=reason)
            if args.lui_constants:
                result['first_failure_category']=result['failure_category']
                result['first_failure_cause']=actual_failure_annotation(lowering,fatal_error,text,original,reason)
            result["differential"] = {"status": "NOT_RUN_NO_AUTHENTICATED_NATIVE_UNIT",
                                       "passing_new_unit": False}
            results.append(result)
            (output / (sample["address"] + ".json")).write_text(json.dumps(result, indent=2) + "\n",
                                                               encoding="utf-8", newline="\n")
            print(sample["address"], result["status"], result["reason"], flush=True)
        del definitions, definition
        gc.collect()
    missing = [sample for sample in samples.values() if "generated_unit" not in sample]
    for sample in missing:
        results.append(dict(sample, status="REFUSED", failure_category="generated entry absent",
                            first_unlowered_pc=None, generated_unit=None, entry_case=None,
                            entry_authenticated=int(sample["address"],16) in catalogue,
                            reason="The mandatory selected original address has no generated entry.",
                            differential={"status": "NOT_RUN_NO_AUTHENTICATED_NATIVE_UNIT", "passing_new_unit": False}))
        if args.lui_constants:
            results[-1]['first_failure_category']='generated entry absent'
            results[-1]['first_failure_cause']={'observed_semantic_category':'missing_generated_entry_or_boundary','actual_AST':None}
    report = {"schema": "mkar.generated-transform-scale-study.v1", "status": "COMPLETED_ATTEMPTS_WITH_RECORDED_LIMITS", "samples": results,
              "controls": controls,
              "cohorts": {"original_primary33": [f"{a:08x}" for a in ORIGINAL_PRIMARY], "supplemental4": [f"{a:08x}" for a in SUPPLEMENTAL], "corrected_source_projection2": [f"{a:08x}" for a in sorted(CORRECTED_PROJECTIONS)], "original_cohort_receipt_sha256": ORIGINAL_COHORT_RECEIPT_SHA256},
              "mandatory_addresses": [f"{address:08x}" for address in MANDATORY],
              "supplemental_addresses": [f"{address:08x}" for address in SUPPLEMENTAL],
              "native_module_count": len(modules), "sampled_unique_entries": len(samples),
              "native_module_correspondence_review": reviews,
              "native_module_coverage": {
                  "all_modules_reviewed": len(reviews),
                  "platform_boundary_modules": sum(m["classification"] == "platform-boundary" for m in reviews),
                  "retail_logic_modules": sum(m["classification"] == "retail-logic" for m in reviews),
                  "mixed_modules": sum(m["classification"] == "mixed" for m in reviews),
                  "source_bound_retail_logic_or_mixed_attempted": sum(m["classification"] in ("retail-logic", "mixed") and m["status"] == "SOURCE_DECLARED_RETAIL_PROJECTION" for m in reviews),
                  "unbound_retail_logic_or_mixed": [m["name"] for m in reviews if m["classification"] in ("retail-logic", "mixed") and m["status"] != "SOURCE_DECLARED_RETAIL_PROJECTION"],
                  "scope": "Platform boundaries do not require generated retail counterparts. Unbound mixed Session remains a specific 5S.1 coverage limit, not absent production native code."},
              "elapsed_seconds": time.perf_counter() - started,
              "inputs": {"census_sha256": sha(args.census), "contract_sha256": sha(contract_path),
                         "original_elf_sha256": sha(original_path), "function_bodies_sha256": sha(export/"function-bodies.tsv"),
                         "instructions_sha256": sha(export/"instructions.tsv"), "study_tool_sha256": sha(Path(__file__)),
                         "module_manifest_sha256": sha(modules_file), "transformer_sha256": sha(Path(transform.__file__)),
                         "compiler_sha256": sha(compiler)},
              "new_verified_units": 0, "native_credit_added": 0,
              "binding_scope": {"global_pointer_original_va": contract["source_gp"],
                                "bound_global_fields": contract["fields"],
                                "pointer_fields": "No general 32-bit game pointer to native object binding is implemented in this scalar initializer lowerer.",
                                "global_map_limit": "Existing GP access inventories identify original locations. The initializer contract binds only its measured field; it is not a full native global-type map.",
                                "vtable_limit": "Recovered constructor/table layouts do not by themselves bind every indirect call in the sample. A native callable/receiver binding must be proved before emission.",
                                "runtime_cost": "No attempted general-contract sample has a saved-state-verified native unit. The separately scoped receiver prototype has a constructed-only kernel benchmark, not a saved-state or game-runtime claim."},
              "Not verified": ["Every new general-contract sample lacks a passing saved-state native differential. Existing two prototypes retain their separate measured credit.",
                               "The overlay field map does not establish arbitrary game-struct pointer, global or vtable bindings."],
              "Contradictions with earlier documents": ["Generated page entries and labels can be continuation points. Original entry authentication is recorded separately.",
                  "Earlier module-prefix sampling did not prove native/original correspondence. The 0409 native Texture Dictionaries, 040a Materials and 040b Geometry labels differ from the original plugin identities; production module names/code are preserved.",
                  "A cursor_assignment parser refusal is not original virtual dispatch. A non_GP_base refusal is not GP access evidence."],
              "Falsification attempts": ["The existing transformer refuses unsupported selected operations before native emission. Separate positive and semantic failure controls are required for this study receipt."]}
    if any(sha(path)!=expected for path,expected in immutable_tools.items()):
        raise ValueError('Study source changed during measurement; preserve diagnostic output and rerun frozen sources')
    (output / "RESULT.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    if args.stack_frames:
        from collections import Counter
        report['frame_stage']={'schema':'actual_symbolic_stack_frame_study_v1','stage':'S5_BEFORE_ANY_S6_GP_BINDING',
            'frame_plan_sha256':sha(args.stack_frames),'before39_sha256':sha(args.frame_before_study),
            'frame_analyzer_sha256':sha(Path(__file__).with_name('stack_frame_analysis.py')),
            'target_count':28,'all_selected_addresses':39,
            'first_compiler_refusals_after':dict(Counter(r['reason'] for r in results)),
            'symbolic_SP_prefixes_lowered':sum(any(op['kind']=='symbolic_SP_adjustment' for op in (r.get('frame_lowering') or {}).get('normalized_operations',[])) for r in results),
            'reference_only_frame_effects':sum(len((r.get('frame_lowering') or {}).get('reference_only_effects',[])) for r in results),
            'whole_saved_state_verified_units':0,'new_credit':0,
            'scope':'Actual AST lowering past matched symbolic frame operations. Reference-only transport and independent frame/native blockers remain unaccepted; no partial prefix is a full native unit.'}
        def first_category(row):
            reason=row['reason']
            if reason=='unbound_memory:GP_field_or_width':return 'GP'
            if row.get('proved_first_barrier_kind')=='native_vtable_target_binding':return 'vtable'
            if reason in ('unbound_stack_local_128_type','unbound_native_128_operation'):return '128bit'
            return 'other'
        report['frame_stage']['four_first_barrier_categories']={key:{'count':sum(first_category(r)==key for r in results),
            'addresses':[r['address'] for r in results if first_category(r)==key],
            'scope':'Actual current first compiler refusal only; no source-feature or whole-unit unlock inference'} for key in ('GP','vtable','128bit','other')}
        report['frame_stage']['native_frame_blockers']=dict(Counter(b for r in results for b in (r.get('frame_lowering') or {}).get('native_emission_blockers',[])))
        if args.borrowed_reference_map:
            report['frame_stage']['stage']='S6_EXPERIMENTAL_BORROWED_REFERENCE_CLEAR'
            report['frame_stage']['S5_before_sha256']=sha(args.symbol_before_study)
            report['frame_stage']['retail_symbol_map_sha256']=sha(Path(__file__).with_name('retail_data_symbol_map.py'))
            report['frame_stage']['borrowed_reference_controls']=symbol_controls
            report['frame_stage']['experimental_clear_effects']=sum(len(r.get('borrowed_reference_IR',{}).get('effects',[])) for r in results)
            target=next(r for r in results if r['address']=='0018da30')
            if target['first_unlowered_pc']!='0018da34' or target['reason']!='unbound_memory:GP_field_or_width' or len(target['borrowed_reference_IR']['effects'])!=1:
                raise ValueError('S6 clear or next unbound cookie was not actually measured')
        if args.lui_constants and not args.scalar_ssa:
            report['frame_stage']['stage']='LUI_CONSTANT_BINDING_OPT_IN'
            joined=[row for row in results if row.get('lui_bindings')]
            report['lui_stage']={'schema':'actual_original_AST_LUI_stage_v1','published_S6_sha256':sha(args.lui_before_study),
                'same_source_LUI_disabled_sha256':sha(args.lui_current_before_study),'target_original_word':'3c023f80',
                'opt_in':True,'all_selected_addresses':39,'bound_probe_addresses':[row['address'] for row in joined],
                'original_LUI_definitions':sum(len(row.get('lui_bindings',[])) for row in results),
                'expected_probe_addresses':sorted(lui_expected),
                'outcomes15':[{key:row.get(key) for key in ('address','before_LUI_frontier','reason','first_unlowered_pc','first_failure_category','first_failure_cause','lui_bindings','frame_lowering')} for row in results if row['address'] in lui_expected],
                'new_complete_E2_units':0,'new_credit':0,'scope':'Original LUI instruction SSA only. Upper64 is preserved; pointer roles, owners, callees and later operations remain unbound.'}
            report['lui_stage'].update(lui_outcome_summary(results,lui_expected,lui_before))
            report['lui_stage']['outcome_retention_controls']=lui_outcome_controls()
            report['lui_stage']['raw_first_failure_categories']=dict(Counter(row['first_failure_category'] for row in results))
            report['lui_stage']['observed_first_semantic_categories']=dict(Counter(row['first_failure_cause']['observed_semantic_category'] for row in results))
        if args.mtc1_bits and not args.scalar_ssa:
            report['frame_stage']['stage']='MTC1_RAW32_BIT_MOVE_OPT_IN'
            report['mtc1_stage']={'schema':'actual_original_AST_MTC1_stage_v1','published_LUI_sha256':sha(args.mtc1_before_study),
                'same_source_MTC1_disabled_sha256':sha(args.mtc1_current_before_study),'target_original_word':'44820800',
                'opt_in':True,'all_selected_addresses':39,'expected_probe_addresses':sorted(mtc1_expected),
                'original_MTC1_definitions':sum(len(row.get('mtc1_bindings',[])) for row in results),
                'outcomes39':[{key:row.get(key) for key in ('address','before_MTC1_frontier','reason','first_unlowered_pc','first_failure_category','first_failure_cause','mtc1_bindings')} for row in results],
                'new_complete_E2_units':0,'new_credit':0,'scope':'Raw GPR-low32 to FPR-raw32 bit-move SSA only. No numeric conversion, memory/owner binding, FPR arithmetic, native adoption or whole-unit forecast.'}
            report['mtc1_stage'].update(mtc1_outcome_summary(results,mtc1_expected,mtc1_before))
            report['mtc1_stage']['outcome_retention_controls']=mtc1_outcome_controls()
            report['mtc1_stage']['raw_first_failure_categories']=dict(Counter(row['first_failure_category'] for row in results))
            report['mtc1_stage']['observed_first_semantic_categories']=dict(Counter(row['first_failure_cause']['observed_semantic_category'] for row in results))
        if args.scalar_ssa:
            report['frame_stage']['stage']='SCALAR_NONTRAPPING_SSA_OPT_IN'
            report['scalar_stage']={'schema':'actual_original_AST_scalar_stage_v1','published_MTC1_sha256':sha(args.scalar_before_study),
                'same_source_scalar_disabled_sha256':sha(args.scalar_current_before_study),'all_selected_addresses':39,
                'expected_current_first_scalar_addresses':sorted(scalar_expected),'operation_definitions':sum(len(r.get('scalar_bindings',[])) for r in results),
                'outcomes39':[{key:r.get(key) for key in ('address','before_scalar_frontier','reason','first_unlowered_pc','first_failure_category','first_failure_cause','scalar_bindings','scalar_candidates')} for r in results],
                'retail_input_bindings':0,'scope':'Original/AST scalar family only; existing proved SSA and architectural zero only. Source-joined copies with unknown native pointer/owner/live-in ABI remain refused; no whole-unit forecast.',
                'unsupported_families':['trapping ADD/SUB/DADD/DSUB','variable shifts','conditional moves/comparisons','HI/LO/MMI/SIMD','unbound native inputs','control/callee/return ABI','typed pointer/memory/owner operations'],
                'prior_stage_actual_comparisons':{name:{'path':str(path),'sha256':sha(path)} for name,path in (
                    ('same_source_S6_off',args.lui_current_before_study),('same_source_LUI_on_MTC1_off',args.mtc1_current_before_study),('same_source_MTC1_on_scalar_off',args.scalar_current_before_study))}}
            report['scalar_stage'].update(scalar_outcome_summary(results,scalar_expected,scalar_before))
            report['scalar_stage']['outcome_retention_controls']=scalar_outcome_controls()
            report['scalar_stage']['raw_first_failure_categories']=dict(Counter(r['first_failure_category'] for r in results))
            report['scalar_stage']['observed_first_semantic_categories']=dict(Counter(r['first_failure_cause']['observed_semantic_category'] for r in results))
        for result in results:
            exempt=args.borrowed_reference_map and result['address']=='0018da30'
            exempt=exempt or args.lui_constants and result['address'] in lui_expected
            exempt=exempt or args.scalar_ssa and result['address'] in scalar_expected
            if result['address'] not in frame_plans and not exempt and result['reason']!=frame_before[result['address']]['reason']:
                if not args.lui_constants:raise ValueError('Non-frame selected probe changed under opt-in frame lowering')
                stage=report['scalar_stage'] if args.scalar_ssa else report['lui_stage']
                stage['status']='INCOMPLETE_OR_FAILED_SCALAR_REPLAY' if args.scalar_ssa else 'INCOMPLETE_OR_FAILED_LUI_BINDING'
                stage.setdefault('unexpected_non_frame_changes',[]).append(result['address'])
        (output/'RESULT.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n')
        if args.lui_constants and not args.scalar_ssa and report['lui_stage']['status']!='PASS_BOUNDED_INSTRUCTION_BINDING':
            raise ValueError('Incomplete LUI outcome retained in '+str(output/'RESULT.json'))
        if args.mtc1_bits and not args.scalar_ssa and report['mtc1_stage']['status']!='PASS_BOUNDED_INSTRUCTION_BINDING':
            raise ValueError('Incomplete MTC1 outcome retained in '+str(output/'RESULT.json'))
        if args.scalar_ssa and report['scalar_stage']['status']!='PASS_BOUNDED_REPLAY_WITH_UNBOUND_INPUTS':
            raise ValueError('Incomplete scalar outcome retained in '+str(output/'RESULT.json'))
        print('STACK_FRAME_STUDY',output,json.dumps(report['frame_stage']))
        return
    tables = full_tables(report, fork)
    markdown = markdown_tables(tables)
    (output / "full-tables.json").write_text(json.dumps(tables, indent=2) + "\n", encoding="utf-8", newline="\n")
    if args.publish_existing_tables:
        publish_existing_tables(parent, output, report, tables, markdown)
    print("STUDY", output, "samples", len(results))


if __name__ == "__main__":
    main()
