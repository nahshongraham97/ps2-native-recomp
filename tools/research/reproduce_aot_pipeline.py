"""Reproducible build-time AOT C++ generation pipeline from pinned SLUS_214.10, overlays, and VU1 captures."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import argparse
import json
import hashlib
import re
import os
from aot_inventory import stage_vu_inputs, update_inventory


def resolve_roots():
    p = Path(__file__).resolve()
    for anc in [p, *p.parents]:
        if anc.name == "armageddon-reimplementation":
            return anc, anc / "external" / "forks" / "ps2-native-recomp"
        if anc.name == "ps2-native-recomp":
            parent = anc.parents[2] if len(anc.parents) >= 3 and (anc.parents[2] / "private-inputs").exists() else None
            return parent, anc
    raise RuntimeError(f"Unable to resolve repo roots from {p}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--parent-root", type=Path)
    parser.add_argument("--fork-root", type=Path)
    parser.add_argument("--vu-only", action="store_true", help="Retain unchanged EE output while correcting VU capture admission")
    args = parser.parse_args()
    parent, fork_root = resolve_roots()
    parent = args.parent_root.resolve() if args.parent_root else parent
    fork_root = args.fork_root.resolve() if args.fork_root else fork_root
    if parent is None:
        raise RuntimeError("Parent workspace with private-inputs/ required for pinned SLUS_214.10, overlays, and VU1 captures.")
    if not fork_root.is_relative_to(parent) or not (fork_root / "CMakeLists.txt").is_file():
        raise RuntimeError("Generation checkout must be an explicit repository inside the parent workspace")
    deps = parent.parent / "dependencies"
    cmake_exe = next(deps.rglob("cmake.exe"), None) or Path(shutil.which("cmake"))
    ninja_exe = next(deps.rglob("ninja.exe"), None) or Path(shutil.which("ninja"))
    clang = deps / "compiler" / "llvm-mingw-20261006-ucrt-x86_64" / "bin" / "clang.exe"
    clangpp = deps / "compiler" / "llvm-mingw-20261006-ucrt-x86_64" / "bin" / "clang++.exe"

    bdir = fork_root / "b"
    bdir.mkdir(exist_ok=True)
    scratch = (bdir / "check-temp").resolve()
    if not scratch.is_relative_to(fork_root.resolve()):
        raise RuntimeError("Compiler temporary path escaped the checkout")
    scratch.mkdir(exist_ok=True)
    env = dict(os.environ)
    env["TEMP"] = env["TMP"] = str(scratch)
    env["PATH"] = str(clangpp.parent) + os.pathsep + env["PATH"]
    process_flags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
    sdl2_dir = fork_root / "third_party" / "sdl2" / "lib" / "cmake" / "SDL2"
    subprocess.run([
        str(cmake_exe), "-S", str(fork_root), "-B", str(bdir), "-G", "Ninja",
        f"-DCMAKE_MAKE_PROGRAM={ninja_exe}",
        f"-DCMAKE_C_COMPILER={clang}",
        f"-DCMAKE_CXX_COMPILER={clangpp}",
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DSDL2_DIR={sdl2_dir}",
    ], check=True, env=env, creationflags=process_flags)
    subprocess.run([str(cmake_exe), "--build", str(bdir), "--target", "eerecomp", "vurecomp"], check=True, env=env, creationflags=process_flags)

    elf_dir = parent / "private-inputs" / "SLUS_214.10.Mortal_Kombat_Armageddon"
    elf = elf_dir / "SLUS_214.10"
    entries = fork_root / "recomp" / "mkarmageddon_all_entries.txt"
    ov_in = parent / "private-inputs" / "mkarmageddon-aot-inputs" / "overlays"
    vu_in = parent / "private-inputs" / "mkarmageddon-aot-inputs" / "vu1"
    out_ee = fork_root / "build" / "generated-aot" / "phase4-all"
    out_vu = fork_root / "build" / "generated-aot" / "vu1"
    inventory_path = parent / "research/ledger/aot_coverage_inventory.json"
    inventory = json.loads(inventory_path.read_text(encoding="utf-8"))
    if hashlib.sha256(elf.read_bytes()).hexdigest() != inventory["sources"]["pinned_elf"]["sha256"]:
        raise RuntimeError("Pinned ELF source hash mismatch")
    if hashlib.sha256(entries.read_bytes()).hexdigest() != inventory["sources"]["entries_seed_list"]["sha256"]:
        raise RuntimeError("Pinned exploration entry manifest changed")
    # Stage in-scope MWo3 overlays (GAMER.ovl, MOVIER.ovl) plus the 4 runtime RAM snapshots (kModules).
    # Excludes NETR.ovl per DECISION_ONLINE_OUT_OF_SCOPE.json.
    r1 = None
    (fork_root / "build").mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(dir=fork_root / "build") as tmp_ov:
        ov_stage = Path(tmp_ov)
        for name in ("GAMER.ovl", "MOVIER.ovl"):
            src = elf_dir / name
            if not src.exists():
                raise FileNotFoundError(f"Missing required in-scope overlay: {src}")
            if hashlib.sha256(src.read_bytes()).hexdigest() != inventory["modules"][name]["source_ovl_sha256"]:
                raise RuntimeError(f"Pinned overlay source hash mismatch: {name}")
            shutil.copy2(src, ov_stage / name)
        for src in ov_in.iterdir():
            if src.name == "NETR.ovl":
                continue
            if src.is_file():
                shutil.copy2(src, ov_stage / src.name)
        admitted = stage_vu_inputs(parent, fork_root, ov_stage / "admitted-vu")
        # All source pins/admission checks precede replacing generated output.
        for d in ((out_vu,) if args.vu_only else (out_ee, out_vu)):
            if d.resolve() == (fork_root / "build/generated-aot").resolve() or not d.resolve().is_relative_to((fork_root / "build/generated-aot").resolve()):
                raise RuntimeError(f"Generated output escaped its build directory: {d}")
            if d.exists():
                shutil.rmtree(d)
            d.mkdir(parents=True)
        if not args.vu_only:
            r1 = subprocess.run(
                [str(bdir / "eerecomp.exe"), str(elf), "--modules", str(ov_stage), "--entries", str(entries), "--out", str(out_ee)],
                capture_output=True, text=True, check=True, env=env, creationflags=process_flags,
            )
        r2 = subprocess.run([str(bdir / "vurecomp.exe"), str(ov_stage / "admitted-vu"), str(out_vu), "--require-complete"], capture_output=True, text=True, check=True, env=env, creationflags=process_flags)
    update_inventory(parent, out_ee)
    inventory = json.loads(inventory_path.read_text(encoding="utf-8"))
    table_path = out_vu / "vu1_table.cpp"
    table = table_path.read_text(encoding="utf-8")
    keys = re.findall(r"\{0x([0-9A-Fa-f]{16})ull,", table)
    declared = re.search(r"register_programs\(gen::kVuPrograms, (\d+)u\)", table)
    if not declared or len(keys) != len(set(keys)) or len(keys) != int(declared[1]):
        raise RuntimeError("Generated VU table/key count mismatch")
    vu = inventory["vu_microprograms"]
    vu["vu1_static_table_keys"] = vu["vu1_strict_corpus_keys"] = len(keys)
    vu["vu1_bin_captures"] = len(admitted["admitted"])
    vu["admitted_capture_measurement"] = admitted
    vu["candidate_generation_scope"] = "Hash-admitted recorded inputs only; protected mis-extraction excluded and preserved. Semantic/packet equivalence remains OPEN."
    inventory["sources"]["reproduced_vu1_table_cpp"]["sha256"] = hashlib.sha256(table_path.read_bytes()).hexdigest()
    inventory["sources"]["reproduced_vu1_table_cpp"]["keys"] = len(keys)
    inventory_path.write_text(json.dumps(inventory, indent=2) + "\n", encoding="utf-8", newline="\n")
    if r1 is not None:
        print(r1.stdout.strip())
    print(r2.stdout.strip())
    print(f"Admitted {len(admitted['admitted'])} pinned capture files; preserved and excluded {len(admitted['excluded'])} invalid source extraction(s).")


if __name__ == "__main__":
    main()
