"""Independent adversarial checks for reference-only stack-frame analysis.

These checks prove rejection scope, not native unit equivalence or ownership.
"""
from __future__ import annotations
import argparse
import copy
import hashlib
import json
from pathlib import Path
import struct
import stack_frame_analysis as frames

ELF_SHA = "a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f"

def fail(condition, message):
    if not condition:
        raise AssertionError(message)

def rejection(original, plan, expected):
    try:
        frames.validate_frame_plan(original, plan)
    except frames.FrameRefusal as error:
        fail(str(error) == expected, "unexpected rejection: " + str(error))
        return str(error)
    raise AssertionError("forged frame proof was admitted")

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", type=Path, required=True)
    parser.add_argument("--analysis", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    image = args.elf.read_bytes()
    fail(hashlib.sha256(image).hexdigest() == ELF_SHA, "original ELF pin changed")
    original = frames.elf_reader(image)
    report = json.loads(args.analysis.read_bytes())
    fail(report["original_elf_sha256"] == ELF_SHA, "analysis ELF pin changed")
    fail(report["analyzer_sha256"] == hashlib.sha256(Path(frames.__file__).read_bytes()).hexdigest(),
         "analysis must come from current analyzer")
    targets = report["targets"]
    fail(len(targets) == 28 and len({p["source_entry"] for p in targets}) == 28,
         "exact28 frame cohort required")
    for plan in targets:
        frames.validate_frame_plan(original, plan)
    actual = next(p for p in targets if p["source_entry"] == "001a0160")
    # Original ADDIU A0,SP,32 @1A01CC, followed by the original call @1A01E4.
    # The address of the64-bit local at allocated-SP+32 is passed to that call;
    # its adjacent32-bit local at SP+40 is stored by SWC1 in the call delay.
    witnesses = {0x1A01CC: 0x27A40020, 0x1A01DC: 0xFFA30020,
                 0x1A01E8: 0xE7A00028}
    for pc, word in witnesses.items():
        fail(int.from_bytes(original(pc,4), "little") == word, "escape witness changed")
    call = int.from_bytes(original(0x1A01E4,4), "little")
    fail(call >> 26 == 3, "original escaping address is not followed by JAL")
    fail(any(e["pc"] == "001a01e4" and e.get("register") == 4
             for e in actual["escapes"]), "actual escaped A0 was not detected")
    local = next(s for s in actual["slots"]
                 if s["offset_from_allocated_sp"] == 32 and s["width_bits"] == 64)
    fail(not local.get("transport_elision_proved",False),
         "real escaping local was accepted as elidable")
    planted = copy.deepcopy(actual)
    slot = next(s for s in planted["slots"]
                if s["offset_from_allocated_sp"] == 32 and s["width_bits"] == 64)
    slot.update(classification="typed_local_proved", transport_elision_proved=True,
                elision_status="PROVED_ELIDABLE")
    escape_rejection = rejection(original, planted, "frame_plan_not_original_derived:slots")
    removed = copy.deepcopy(actual)
    removed["escapes"] = []
    removed_escape_rejection = rejection(original, removed, "frame_plan_not_original_derived:escapes")
    wrong_size = copy.deepcopy(actual)
    wrong_size["frame_bytes"] += 16
    size_rejection = rejection(original, wrong_size, "frame_plan_not_original_derived:frame_bytes")
    # Synthetic source words test the transfer proof directly, independently of
    # serialized proof-integrity checks. No instruction execution occurs here.
    def fixture(words):
        data = b"".join(struct.pack("<I",w) for w in words)
        def read(address, count):
            fail(0x100000 <= address and address+count <= 0x100000+len(data),
                 "synthetic reader extent")
            return data[address-0x100000:address-0x100000+count]
        return frames.recover_stack_frame(read,0x100000,
            [{"start":"00100000","end":f"{0x100000+len(data)-1:08x}"}])
    # Balanced transport; a load of a different offset cannot restore saved RA.
    balanced = fixture([0x27BDFFF0,0xFFBF0000,0xDFBF0000,0x03E00008,0x27BD0010])
    fail(balanced["returns"][0]["SP_balanced"], "balanced SP was not recognized")
    fail(balanced["returns"][0]["RA_input_identity_locally_restored"],
         "untouched saved RA failed identity")
    clobbered = fixture([0x27BDFFF0,0xFFBF0000,0xFFA00000,0xDFBF0000,
                         0x03E00008,0x27BD0010])
    fail("frame_return_RA_clobber_unproved" in clobbered["blockers"],
         "clobbered RA source was accepted")
    escaped = fixture([0x27BDFFF0,0xFFBF0000,0x27A40008,0x0C040100,
                       0x00000000,0xDFBF0000,0x03E00008,0x27BD0010])
    fail(any(e.get("register") == 4 for e in escaped["escapes"]),
         "stack address passed to a callee was not detected")
    fail(escaped["native_admission"]["status"] == "REFUSED",
         "escaping synthetic frame admitted native emission")
    unbalanced = fixture([0x27BDFFF0,0xFFBF0000,0xDFBF0000,0x03E00008,0x27BD0020])
    fail("frame_unbalanced_return_SP" in unbalanced["blockers"],
         "incorrect return SP was accepted")
    late_ra = fixture([0x27BDFFF0,0xFFBF0000,0x0000F82D,0x03E00008,0xDFBF0000])
    fail("frame_JR_target_before_delay_unproved" in late_ra["blockers"],
         "loading RA in the delay instruction repaired an already captured wrong JR target")
    narrow = fixture([0x27BDFFF0,0xFFB00000,0xDFB00000,0x03E00008,0x27BD0010])
    fail(all(not s.get("transport_elision_proved",False)
             for s in narrow["slots"] if s["source_register"] == 16),
         "64-bit restore was treated as full128-bit callee preservation")
    fpr = fixture([0x27BDFFF0,0xE7A00008,0x03E00008,0x27BD0010])
    fail(fpr["slots"][0]["register_bank"] == "FPR"
         and fpr["native_admission"]["status"] == "REFUSED",
         "SWC1 F0 was treated as GPR0 or an authenticated typed local")
    result = {
        "schema":"stack_frame_adversarial_controls_v1","status":"PASS",
        "original_elf_sha256":ELF_SHA,
        "analysis_sha256":hashlib.sha256(args.analysis.read_bytes()).hexdigest(),
        "analyzer_sha256":report["analyzer_sha256"],
        "driver_sha256":hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "rederived_original_frames":len(targets),
        "actual_escaping_frame":"001a0160",
        "witness_words":{f"{p:08x}":f"{w:08x}" for p,w in witnesses.items()},
        "controls":[
            {"plant":"actual escaping local mislabeled as typed/elidable","rejection":escape_rejection},
            {"plant":"actual escape facts removed","rejection":removed_escape_rejection},
            {"plant":"wrong frame size","rejection":size_rejection},
            {"plant":"synthetic saved RA clobbered","rejection":"frame_return_RA_clobber_unproved"},
            {"plant":"synthetic stack pointer passed to callee","rejection":"native admission REFUSED"},
            {"plant":"synthetic wrong exit SP delta","rejection":"frame_unbalanced_return_SP"},
            {"plant":"RA repaired only in JR delay","rejection":"frame_JR_target_before_delay_unproved"},
            {"plant":"64-bit restore claimed full128-bit preservation","rejection":"transport_elision_proved remains false"},
            {"plant":"SWC1 F0 treated as GPR0/typed local","rejection":"FPR bank and native admission REFUSED"}
        ],
        "scope":"Static analysis and proof-integrity controls. Actual compiled AOT SP/RA mutation checks are separate. No function equivalence or native credit."
    }
    root = Path(__file__).resolve().parents[2]/"build/oracle-telemetry"
    output = args.output.resolve()
    fail(output.is_relative_to(root.resolve()) and not output.exists(),
         "control output must be a new ignored telemetry file")
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8",newline="\n")
    print("PASS_STACK_STATIC_CONTROLS frames28 controls9 real_escape001A0160 native_credit0")

if __name__ == "__main__":
    main()
