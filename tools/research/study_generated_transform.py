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
    return {"schema": "generated_transform_full_tables_v1", "table_generator_sha256": sha(Path(__file__)), "primary33": primary, "additional6": extra,
            "existing_success2": successes, "native_module23": module_rows, "denominators": denominators,
            "technique_observed_fractions": observed, "existing_success_receipt": measurement,
            "relative_receiver_extension": {key:extension[key] for key in ("status","constructed_comparisons","saved_states","admitted","inapplicable","benchmark","benchmark_scope","credit_added","production_callers")}}


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
    lines += ["", "These are observed-feature subsets, not independent success rates for techniques the compiler never reached. The sole relative-receiver extension passed 18 constructed CPU/RAM comparisons but all 61 original GP-global receivers were null, so its saved-state result remains 0 admitted/61 inapplicable and zero credit. Wrong-value, wrong-field, disguised memory accessor, forged PASS/zero-admission and changed-output controls reject as recorded in the existing verification record.", "<!-- END GENERATED 5S FULL TABLES -->", ""]
    return "\n".join(lines)


def publish_existing_tables(parent, output, report, tables, markdown):
    path=parent/"research/runs/phase4-aot-completion-20261124/VERIFICATION.json"
    record=json.loads(path.read_bytes());section=record["instruction_phase5S"]
    if "prior_37_entry_study" not in section: section["prior_37_entry_study"] = section["study"]
    section["study"] = report
    section["study_receipt"] = {"path": (output/"RESULT.json").relative_to(parent/"external/forks/ps2-native-recomp").as_posix(), "sha256":sha(output/"RESULT.json")}
    section["full_human_readable_tables"] = tables
    section["measured_coverage"].update(attempted_entries=39, primary_cohort_entries=33, additional_entries=6,
        failure_categories=dict(__import__('collections').Counter(s["failure_category"] for s in report["samples"])),
        sample_accepted_fraction={"numerator":0,"denominator":33}, additional_accepted_fraction={"numerator":0,"denominator":6},population_projection=None)
    section["required_sample_result"] = [{key:s.get(key) for key in ("address","entry_authenticated","generated_unit","entry_case","status","failure_category","reason","first_unlowered_pc","differential")} for s in report["samples"] if int(s["address"],16) in MANDATORY]
    path.write_text(json.dumps(record,indent=2,ensure_ascii=True)+"\n",encoding="utf-8",newline="\n")
    status=parent/"PORT_STATUS.md";text=status.read_text(encoding="utf-8")
    begin="<!-- BEGIN GENERATED 5S FULL TABLES -->";end="<!-- END GENERATED 5S FULL TABLES -->"
    if begin in text:
        old=text[text.index(begin):text.index(end)+len(end)];text=text.replace(old,markdown.strip(),1)
    else: text=text.rstrip()+"\n\n"+markdown
    text=text.replace("frontend attempted 37 selected entries", "frontend attempted all33 original selections plus6 additional entries (39 unique)")
    status.write_text(text,encoding="utf-8",newline="\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--parent-root", type=Path, required=True)
    parser.add_argument("--census", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--compiler", type=Path)
    parser.add_argument("--publish-existing-tables", action="store_true", help="Update only the existing parent instruction_phase5S record and PORT_STATUS table block")
    parser.add_argument("--render-existing-study", type=Path, help="Render already measured39-entry evidence with current pinned positive receipts; never rerun or relabel compiler attempts")
    args = parser.parse_args()
    parent = args.parent_root.resolve()
    fork = parent / "external/forks/ps2-native-recomp"
    output = args.output.resolve()
    if output.exists() or not output.is_relative_to((fork / "build/oracle-telemetry").resolve()):
        raise ValueError("Study output must be a new owned telemetry directory")
    output.mkdir()
    if args.render_existing_study:
        report = json.loads(args.render_existing_study.read_bytes())
        if ({int(s["address"],16) for s in report["samples"]} != set(ORIGINAL_PRIMARY) | set(SUPPLEMENTAL) | CORRECTED_PROJECTIONS
            or len(report["samples"]) != 39 or report["inputs"]["transformer_sha256"] != sha(Path(transform.__file__))):
            raise ValueError("Existing study cohort or measured compiler changed; rerun39 attempts")
        # Preserve the actual measurement bytes and original producer hash.
        (output/"RESULT.json").write_bytes(args.render_existing_study.read_bytes())
        tables = full_tables(report, fork)
        markdown = markdown_tables(tables)
        (output/"full-tables.json").write_text(json.dumps(tables,indent=2)+"\n",encoding="utf-8",newline="\n")
        (output/"full-tables.md").write_text(markdown,encoding="utf-8",newline="\n")
        if args.publish_existing_tables:
            publish_existing_tables(parent, output, report, tables, markdown)
        print("RENDERED_EXISTING_MEASURED_STUDY", output, "samples39; no compiler attempts rerun")
        return
    census = json.loads(args.census.read_bytes())
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
            begin = time.perf_counter()
            lowering = transform.Lowering(definition, descriptor, contract)
            reason = None
            try:
                ir = lowering.lower()
            except transform.Refusal as error:
                reason = str(error)
                ir = None
            elapsed = time.perf_counter() - begin
            result = dict(sample, source_unit_sha256=expected, entry_authenticated=authenticated,
                          source_span_count=sum(int(row["entry"], 16) == address for row in ranges),
                          generated_entry_fragment=segment[:3000], ast_parse_seconds=ast_seconds,
                          ast_parse_shared_by=len(addresses), lowering_seconds=elapsed,
                          first_unlowered_pc=f"{lowering.pc:08x}", lowered_effects=len(lowering.effects),
                          source_budget_debits_processed=lowering.debit)
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
    (output / "RESULT.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    tables = full_tables(report, fork)
    markdown = markdown_tables(tables)
    (output / "full-tables.json").write_text(json.dumps(tables, indent=2) + "\n", encoding="utf-8", newline="\n")
    (output / "full-tables.md").write_text(markdown, encoding="utf-8", newline="\n")
    if args.publish_existing_tables:
        publish_existing_tables(parent, output, report, tables, markdown)
    print("STUDY", output, "samples", len(results))


if __name__ == "__main__":
    main()
