"""Bounded headless diagnostic on an owned PE/state/card copy, never the oracle.

This measures one case and preserves misses. It cannot establish suite closure,
image parity, native implementation credit, or removal of emulated devices.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import time

WORKTREE = Path(__file__).resolve().parents[2]
PE_SHA = "0ad85a65845ba9d772b83bf82eb9a87d6d7b01531799bb2ce9cf8d9d4ab13b48"
ELF_SHA = "a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f"


def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as f:
        for block in iter(lambda: f.read(8 << 20), b""):
            h.update(block)
    return h.hexdigest()


def pin(path):
    before = path.stat()
    value = digest(path)
    after = path.stat()
    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
        raise ValueError("input changed while hashing: " + str(path))
    return dict(path=str(path), size=after.st_size, sha256=value, mtime_ns=after.st_mtime_ns)


def evaluate_case(rc, text, expected_start, additional, timed_out=False, miss_tokens=()):
    starts = re.findall(r"\[state\] loaded frame (\d+)", text)
    stops = re.findall(r"\[ee\] stopped after (\d+) frames", text)
    start = int(starts[-1]) if starts else None
    stop = int(stops[-1]) if stops else None
    misses = re.findall(r"UNTRANSLATED JUMP at 0x([0-9A-Fa-f]+).*?ra=0x([0-9A-Fa-f]+)", text)
    objections = []
    if len(starts) != 1 or len(stops) != 1:
        objections.append("ambiguous_frame_observations")
    if timed_out:
        objections.append("timeout")
    if start != expected_start:
        objections.append("saved_start_frame_not_bound")
    if rc != 0:
        objections.append("nonzero_exit")
    if "[ee] halted: frame limit reached" not in text:
        objections.append("expected_stop_missing")
    if stop is None or start is None or stop - start != additional:
        objections.append("requested_additional_frames_not_measured")
    if misses or miss_tokens or "[vu1-aot-miss]" in text or "missing code at" in text or "[ee] bad entry" in text:
        objections.append("untranslated_code")
    return dict(loaded_frame=start, stopped_frame=stop,
                measured_additional_frames=stop-start if start is not None and stop is not None else None,
                ee_misses_pc_ra=misses, miss_file_tokens=list(miss_tokens), diagnostic_objections=objections,
                bounded_frame_probe_passed=not objections,
                full_suite_or_native_acceptance=False)


def self_test():
    good = "[state] loaded frame 80000\n[ee] halted: frame limit reached\n[ee] stopped after 80016 frames\n"
    assert evaluate_case(0, good, 80000, 16)["bounded_frame_probe_passed"]
    plants = [
        ("absolute_limit_below_saved_frame", 0, good.replace("80016", "80001"), False),
        ("nonzero_exit_with_clean_stop", 4, good, False),
        ("wrong_saved_start", 0, good.replace("loaded frame 80000", "loaded frame 70000"), False),
        ("missing_code_with_clean_stop", 0, good+"[ee-aot-failfast] UNTRANSLATED JUMP at 0x00B17700 ra=0x002E6180\n", False),
        ("timeout_with_clean_text", 0, good, True),
        ("ambiguous_reload", 0, "[state] loaded frame 79900\n"+good, False),
    ]
    for name, rc, text, timeout in plants:
        assert not evaluate_case(rc, text, 80000, 16, timeout)["bounded_frame_probe_passed"], name
    assert not evaluate_case(0,good,80000,16,miss_tokens=["00B17700"])["bounded_frame_probe_passed"]
    return dict(status="PASS", positive_controls=1, rejected_plants=[p[0] for p in plants]+["miss_file_without_stderr"])


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--source-fork", type=Path)
    ap.add_argument("--state", type=Path)
    ap.add_argument("--card-copy", type=Path)
    ap.add_argument("--output", type=Path)
    ap.add_argument("--expected-start-frame", type=int)
    ap.add_argument("--additional-frames", type=int, default=16)
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args()
    tests = self_test()
    if args.self_test and not any((args.source_fork,args.state,args.card_copy,args.output)):
        print(json.dumps(tests,indent=2)); return 0
    if not all((args.source_fork,args.state,args.card_copy,args.output)) or args.expected_start_frame is None:
        ap.error("explicit source-fork, owned state/card, fresh output and expected start are required")
    if not 1 <= args.additional_frames <= 600 or args.expected_start_frame < 0:
        ap.error("invalid bounded frame request")
    source, state, card, output = [p.resolve() for p in (args.source_fork,args.state,args.card_copy,args.output)]
    owned = (WORKTREE/"build/oracle-telemetry").resolve()
    if any(not p.is_relative_to(owned) or p == owned for p in (state,card,output)):
        ap.error("state, card-copy and output must be in this worktree's owned telemetry")
    if output.exists() or output.is_relative_to(state.parent) or state.is_relative_to(output) or card.is_relative_to(output):
        ap.error("output must be fresh and separate from captured inputs")
    if state.suffix != ".state" or card != Path(str(state)+".ps2"):
        ap.error("use the completed quick-state card companion, never the persistent card")
    candidate=source/"build/r192-aot-closure"
    parent=source.parents[2]
    binary=candidate/"out/ps2run.exe"
    elf=parent/"research/inputs/SLUS_214.10"
    disc=parent/"external/forks/ps2-native-recomp-backup-20261110/games/mkarmageddon/disc.iso"
    originals=[binary,elf,disc,state,card]
    originals += [candidate/"out"/n for n in ("SDL2.dll","dxcompiler.dll","dxil.dll")]
    originals += [candidate/"src"/n for n in ("host/main.cpp","runtime/src/system.cpp","runtime/src/aot_only_stubs.cpp","runtime/src/dispatch.cpp")]
    pins=[pin(p) for p in originals]
    initial_pins={p["path"]:p for p in pins}
    if pins[0]["sha256"] != PE_SHA or pins[1]["sha256"] != ELF_SHA:
        raise ValueError("candidate PE or ELF identity changed; review before running")
    with state.open("rb") as f:
        if f.read(12) != b"PS2STATE\x08\x00\x00\x00":
            raise ValueError("only the pinned version8 capture route is eligible")
    output.mkdir(parents=True,exist_ok=False)
    run_dir=output/"run";run_dir.mkdir()
    for original in originals[:2]+originals[5:8]:
        shutil.copyfile(original,run_dir/original.name)
        if digest(run_dir/original.name)!=initial_pins[str(original)]["sha256"]:
            raise ValueError("copy differs from initial pin before launch: "+str(original))
    shutil.copyfile(card,run_dir/"disposable.ps2")
    if digest(run_dir/"disposable.ps2")!=initial_pins[str(card)]["sha256"]:
        raise ValueError("card companion changed before launch")
    for sub in ("misses","vu-record","quick"):
        (output/sub).mkdir()
    env={k:v for k,v in os.environ.items() if not k.startswith(("PS2_","MKA_"))}
    overrides=dict(PS2_EE_LIVE="0",PS2_VU_LIVE="0",PS2_RECORD="0",PS2_THREADS="0",
                   MKA_INTERCEPT_COMBAT="1",PS2_NATIVE_VERTEX="1",
                   PS2_MISS_DUMP_DIR=str(output/"misses"),PS2_VU_RECORD=str(output/"vu-record"))
    env.update(overrides)
    target=args.expected_start_frame+args.additional_frames
    argv=[str(run_dir/"ps2run.exe"),str(run_dir/elf.name),"--disc",str(disc),
          "--memcard",str(run_dir/"disposable.ps2"),"--load-state",str(state),
          "--frames",str(target),"--misses",str(output/"misses.txt"),
          "--quick-dir",str(output/"quick"),"--no-record"]
    t=time.monotonic();timeout=False
    try:
        p=subprocess.run(argv,cwd=run_dir,env=env,stdout=subprocess.PIPE,stderr=subprocess.PIPE,
                         timeout=60,creationflags=subprocess.CREATE_NO_WINDOW)
        rc=p.returncode; stdout=p.stdout;stderr=p.stderr
    except subprocess.TimeoutExpired as exc:
        timeout=True;rc=None;stdout=exc.stdout or b"";stderr=exc.stderr or b""
    (output/"stdout.log").write_bytes(stdout);(output/"stderr.log").write_bytes(stderr)
    miss_file=output/"misses.txt"
    miss_tokens=[s.strip() for s in miss_file.read_text(encoding="utf8").splitlines() if s.strip()] if miss_file.exists() else []
    result=evaluate_case(rc,stderr.decode("utf8",errors="replace"),args.expected_start_frame,args.additional_frames,timeout,miss_tokens)
    result.update(status="BOUNDED_DIAGNOSTIC" if result["bounded_frame_probe_passed"] else "OPEN_DIAGNOSTIC_FAILURE",
                  argv=argv,environment_overrides=overrides,exit_code=rc,seconds=time.monotonic()-t,
                  requested_start=args.expected_start_frame,absolute_frame_limit=target,additional_frames_requested=args.additional_frames,
                  input_pins=pins,tool_sha256=digest(Path(__file__)),self_test=tests,
                  scope="One headless copied-PE probe. No window/audio flags, no replay parity or suite coverage, no claim of zero emulated devices or native credit.")
    changed=[]
    for p in pins:
        if digest(Path(p["path"]))!=p["sha256"]:
            changed.append(p["path"])
    result["input_changes"]=changed
    if changed:
        result["status"]="INPUT_CHANGED_REJECTED"
    result["outputs"]=[pin(p) for p in sorted(output.rglob("*")) if p.is_file()]
    payload=(json.dumps(result,indent=2)+"\n").encode()
    (output/"RESULT.json").write_bytes(payload)
    print(json.dumps(dict(status=result["status"],result=str(output/"RESULT.json"),sha256=hashlib.sha256(payload).hexdigest(),frame_result=result["measured_additional_frames"],misses=result["ee_misses_pc_ra"])))
    return 0 if result["bounded_frame_probe_passed"] and not changed else 2


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError,ValueError) as error:
        print("PROBE_REFUSED: "+str(error),file=sys.stderr)
        raise SystemExit(2)
