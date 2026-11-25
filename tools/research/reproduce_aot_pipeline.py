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
import csv
from aot_inventory import stage_vu_inputs, update_inventory, verified_nonfunction_catalogue_addresses


def catalogue_seed_union(original: bytes, rows: list[dict]) -> tuple[bytes, dict]:
    """Retain exploration tokens and add every raw base-catalogue start."""
    tokens = set()
    base_entries = set()
    for line in original.decode("utf-8").splitlines():
        for token in line.split("#", 1)[0].split():
            if not re.fullmatch(r"(?:[^:\s]+:)?(?:0[xX])?[0-9A-Fa-f]+", token):
                raise RuntimeError(f"Invalid pinned exploration seed token: {token}")
            tokens.add(token)
            if ":" not in token:
                base_entries.add(int(token, 16))
    catalogue = {int(row["address"], 16) for row in rows if row["module"] == "SLUS_214.10"}
    if any(address < 0 or address > 0xFFFFFFFF or address & 3 for address in catalogue):
        raise RuntimeError("Raw base catalogue contains an invalid or unaligned start")
    additions = sorted(catalogue - base_entries)
    tokens.update(f"{address:08x}" for address in additions)

    def key(token):
        module, address = token.rsplit(":", 1) if ":" in token else ("", token)
        return module, int(address, 16), token

    derived = ("\n".join(sorted(tokens, key=key)) + "\n").encode("utf-8")
    return derived, {
        "original_seed_sha256": hashlib.sha256(original).hexdigest(),
        "derived_seed_sha256": hashlib.sha256(derived).hexdigest(),
        "raw_base_catalogue_starts": len(catalogue),
        "catalogue_additions": [f"{address:08x}" for address in additions],
        "derived_tokens": len(tokens),
        "scope": "All raw base catalogue starts are seeded, including separately verified nonfunction data; no denominator or classification changes",
    }


def verify_overlay_window_inputs(contents: dict[str, bytes], manifest: list[dict]) -> list[dict]:
    """Authenticate the four recorded dynamic images before replacing output."""
    names = [row["name"] for row in manifest]
    if len(names) != 4 or len(set(names)) != len(names):
        raise RuntimeError("Dynamic window input manifest must identify four distinct images")
    if set(contents) != {name + ".bin" for name in names}:
        raise RuntimeError("Unknown/missing dynamic window input files")
    verified = []
    for row in sorted(manifest, key=lambda item: item["name"]):
        name = row["name"]
        if not re.fullmatch(r"[0-9A-F]{8}_[0-9A-F]{8}", name):
            raise RuntimeError(f"Noncanonical dynamic window input name: {name}")
        base = int(row["base_addr"], 16)
        if base != int(name[:8], 16):
            raise RuntimeError(f"Dynamic window base/name mismatch: {name}")
        raw = contents[name + ".bin"]
        if len(raw) != row["span_bytes"] or hashlib.sha256(raw).hexdigest() != row["sha256"]:
            raise RuntimeError(f"Dynamic window source hash/extent mismatch: {name}")
        verified.append({"name": name, "base_addr": base, "span_bytes": len(raw), "sha256": row["sha256"]})
    return verified


def verify_generated_overlay_windows(measured: list[dict], expected: list[dict]) -> None:
    names = [row["name"] for row in measured]
    wanted = {row["name"]: row for row in expected}
    if len(names) != len(set(names)) or set(names) != set(wanted):
        raise RuntimeError(f"Generated dynamic window names differ from verified inputs: {sorted(names)}")
    for row in measured:
        source = wanted[row["name"]]
        if (row["base_addr"], row["span_bytes"]) != (source["base_addr"], source["span_bytes"]):
            raise RuntimeError(f"Generated dynamic window base/extent mismatch: {row['name']}")
        if row["unit_count"] <= 0 or row["callable_entries"] <= 0:
            raise RuntimeError(f"Generated dynamic window has no callable units: {row['name']}")


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
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--vu-only", action="store_true", help="Retain unchanged EE output while correcting VU capture admission")
    mode.add_argument("--ee-only", action="store_true", help="Retain unchanged VU output while correcting EE callable entries")
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
    generated_root = (fork_root / "build/generated-aot").resolve()
    if not generated_root.is_relative_to(fork_root.resolve()) or not (fork_root / "build").resolve().is_relative_to(fork_root.resolve()):
        raise RuntimeError("Generation build/output root escaped the checkout")
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
    if not (sdl2_dir / "SDL2Config.cmake").is_file():
        # A clean source clone has no ignored dependency bundle. Reuse the
        # user's existing SDK package as a read-only dependency, never a
        # previously built generator or generated retail source tree.
        sdl2_dir = parent / "external/forks/ps2-native-recomp/third_party/sdl2/lib/cmake/SDL2"
    if not (sdl2_dir / "SDL2Config.cmake").is_file():
        raise RuntimeError("Existing SDL2 dependency package is required for Windows CMake configuration")
    subprocess.run([
        str(cmake_exe), "-S", str(fork_root), "-B", str(bdir), "-G", "Ninja",
        f"-DCMAKE_MAKE_PROGRAM={ninja_exe}",
        f"-DCMAKE_C_COMPILER={clang}",
        f"-DCMAKE_CXX_COMPILER={clangpp}",
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DSDL2_DIR={sdl2_dir}",
    ], check=True, env=env, creationflags=process_flags)
    targets = ["vurecomp"] if args.vu_only else ["eerecomp"] if args.ee_only else ["eerecomp", "vurecomp"]
    subprocess.run([str(cmake_exe), "--build", str(bdir), "--target", *targets, "--parallel", "4"], check=True, env=env, creationflags=process_flags)

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
    catalogue_path = parent / "research/ledger/functions.csv"
    with catalogue_path.open(encoding="utf-8", newline="") as stream:
        rows = list(csv.DictReader(stream))
    derived, seed_receipt = catalogue_seed_union(entries.read_bytes(), rows)
    seed_receipt["catalogue_sha256"] = hashlib.sha256(catalogue_path.read_bytes()).hexdigest()
    # This helper authenticates the data-role proof; an unknown missing entry
    # can never be waived by a hand-written exclusion in this pipeline.
    verified_data = verified_nonfunction_catalogue_addresses(parent, rows)
    dynamic_contents = {path.name: path.read_bytes() for path in ov_in.glob("*.bin")}
    dynamic_inputs = verify_overlay_window_inputs(dynamic_contents, inventory["dynamic_overlay_windows_kModules"])
    # Stage in-scope MWo3 overlays (GAMER.ovl, MOVIER.ovl) plus the 4 runtime RAM snapshots (kModules).
    # Excludes NETR.ovl per DECISION_ONLINE_OUT_OF_SCOPE.json.
    r1 = None
    r2 = None
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
        for row in dynamic_inputs:
            name = row["name"] + ".bin"
            with (ov_stage / name).open("xb") as stream:
                stream.write(dynamic_contents[name])
        admitted = None if args.ee_only else stage_vu_inputs(parent, fork_root, ov_stage / "admitted-vu")
        # All source pins/admission checks precede replacing generated output.
        outputs = (out_vu,) if args.vu_only else (out_ee,) if args.ee_only else (out_ee, out_vu)
        if not args.vu_only:
            generated_seeds = (out_ee.parent / "catalogue_entries.txt").resolve()
            if not generated_seeds.is_relative_to((fork_root / "build/generated-aot").resolve()):
                raise RuntimeError("Generated seed file escaped its build directory")
            generated_seeds.parent.mkdir(parents=True, exist_ok=True)
            generated_seeds.write_bytes(derived)
        for d in outputs:
            if d.resolve() == (fork_root / "build/generated-aot").resolve() or not d.resolve().is_relative_to((fork_root / "build/generated-aot").resolve()):
                raise RuntimeError(f"Generated output escaped its build directory: {d}")
            if d.exists():
                shutil.rmtree(d)
            d.mkdir(parents=True)
        if not args.vu_only:
            r1 = subprocess.run(
                [str(bdir / "eerecomp.exe"), str(elf), "--modules", str(ov_stage), "--overlays", str(ov_stage), "--entries", str(generated_seeds), "--out", str(out_ee)],
                capture_output=True, text=True, check=True, env=env, creationflags=process_flags,
            )
        if not args.ee_only:
            r2 = subprocess.run([str(bdir / "vurecomp.exe"), str(ov_stage / "admitted-vu"), str(out_vu), "--require-complete"], capture_output=True, text=True, check=True, env=env, creationflags=process_flags)
    measured = update_inventory(parent, out_ee)
    verify_generated_overlay_windows(measured["dynamic_windows"], dynamic_inputs)
    inventory = json.loads(inventory_path.read_text(encoding="utf-8"))
    inventory["entry_measurement"]["dynamic_window_inputs"] = dynamic_inputs
    if not args.vu_only:
        inventory["sources"]["catalogue_entry_seeds"].update(seed_receipt)
    unexpected = {
        module: [address for address in result["missing_catalogue_entries"]
                 if module != "SLUS_214.10" or int(address, 16) not in verified_data]
        for module, result in measured["modules"].items()
    }
    unexpected = {module: addresses for module, addresses in unexpected.items() if addresses}
    # Persist the measurement/provenance even on failure; never claim that a
    # data exception supplies an executable entry or silently change counts.
    inventory_path.write_text(json.dumps(inventory, indent=2) + "\n", encoding="utf-8", newline="\n")
    if unexpected:
        raise RuntimeError("Missing required catalogue callables after generation: " + json.dumps(unexpected, sort_keys=True))
    if args.ee_only:
        print(r1.stdout.strip())
        print(f"Derived EE seeds: {seed_receipt['derived_seed_sha256']}; catalogue additions={len(seed_receipt['catalogue_additions'])}; VU output unchanged.")
        return
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
