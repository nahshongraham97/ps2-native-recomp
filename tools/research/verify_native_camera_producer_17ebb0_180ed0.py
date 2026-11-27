"""Verify and falsify the native 17EBB0 / 180ED0 camera producer and sync slice."""
from __future__ import annotations

import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BACKUP = ROOT.parents[3]
BIN_DIR = BACKUP / "dependencies" / "compiler" / "llvm-mingw-20261006-ucrt-x86_64" / "bin"
CXX = BIN_DIR / "clang++.exe"
BUILD_DIR = ROOT / "b"
FALSIFY_DIR = BUILD_DIR / "falsify_17ebb0_180ed0"

RUNTIME_FILES = [
    ROOT / "runtime" / "include" / "mka" / "retail_renderer_frames.hpp",
    ROOT / "runtime" / "src" / "native" / "retail_renderer_frames.cpp",
    ROOT / "runtime" / "include" / "mka" / "retail_renderer_cameras.hpp",
    ROOT / "runtime" / "src" / "native" / "retail_renderer_cameras.cpp",
    ROOT / "runtime" / "include" / "mka" / "retail_camera_world.hpp",
    ROOT / "runtime" / "src" / "native" / "retail_camera_world.cpp",
]

FORBIDDEN_PATTERNS = [
    (r"c\.r\[", "c.r[...]"),
    (r"\bg_ram\b", "g_ram"),
    (r"\$gp\b", "$gp"),
    (r"guest_vtable|vtable_addr", "guest vtable"),
]


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def check_strict_native_contract() -> dict:
    violations = []
    hashes = {}
    for path in RUNTIME_FILES:
        text = path.read_text(encoding="utf-8")
        hashes[str(path.relative_to(ROOT)).replace("\\", "/")] = sha256(path)
        for pattern, label in FORBIDDEN_PATTERNS:
            if re.search(pattern, text):
                violations.append({"file": str(path), "pattern": label})
    if violations:
        raise RuntimeError(f"Strict native contract violation: {violations}")
    return hashes


def build_and_run_falsification(env: dict[str, str]) -> list[dict]:
    if FALSIFY_DIR.exists():
        shutil.rmtree(FALSIFY_DIR)
    FALSIFY_DIR.mkdir(parents=True, exist_ok=True)

    # Locate all built native static libraries in b/
    libs = sorted(BUILD_DIR.glob("libmka_*.a"))
    if not libs:
        raise RuntimeError(f"No built static libraries found under {BUILD_DIR}")
    sdl2_lib = ROOT / "third_party" / "sdl2" / "lib" / "libSDL2.dll.a"

    # Compile a modified copy of the 3 runtime TUs + test_retail_renderer_cameras.cpp into a falsification exe
    defects = [
        (
            "dirty_sync_skip",
            ROOT / "runtime" / "src" / "native" / "retail_renderer_frames.cpp",
            "sync_node(sync_node,root,false);",
            "// planted defect: skip sync_node",
        ),
        (
            "view_matrix_wrong_sign",
            ROOT / "runtime" / "src" / "native" / "retail_renderer_cameras.cpp",
            "-(tx*i02+ty*i12+tz*i22),",
            "+(tx*i02+ty*i12+tz*i22),",
        ),
        (
            "frustum_plane_flip",
            ROOT / "runtime" / "src" / "native" / "retail_renderer_cameras.cpp",
            "normalize_plane(r3[0]+r2[0],r3[1]+r2[1],r3[2]+r2[2],r3[3]+r2[3],planes[4])&&",
            "normalize_plane(-(r3[0]+r2[0]),-(r3[1]+r2[1]),-(r3[2]+r2[2]),-(r3[3]+r2[3]),planes[4])&&",
        ),
        (
            "callback_reverse_order",
            ROOT / "runtime" / "src" / "native" / "retail_renderer_cameras.cpp",
            "for(const auto& entry:callbacks_snapshot){",
            "for(auto it=callbacks_snapshot.rbegin();it!=callbacks_snapshot.rend();++it){const auto& entry=*it;",
        ),
        (
            "rollback_leak",
            ROOT / "runtime" / "src" / "native" / "retail_renderer_cameras.cpp",
            "cameras.destroy_base_camera(produced.camera,cf);",
            "// planted defect: leak camera on rollback",
        ),
    ]

    results = []
    # Precompile test_retail_renderer_cameras.cpp once into an object file
    test_obj = FALSIFY_DIR / "test_retail_renderer_cameras.o"
    common_flags = [
        "-std=c++23",
        "-O2",
        f"-I{ROOT / 'runtime' / 'include'}",
        f"-I{ROOT / 'tests'}",
        f"-I{ROOT / 'third_party' / 'sdl2' / 'include'}",
        *[f"-I{d}" for d in BUILD_DIR.glob("native-retail-startup-*") if d.is_dir()],
    ]
    subprocess.run(
        [str(CXX), *common_flags, "-c", str(ROOT / "tests" / "test_retail_renderer_cameras.cpp"), "-o", str(test_obj)],
        check=True,
        env=env,
        cwd=ROOT,
    )

    for name, target_file, old_str, new_str in defects:
        src_text = target_file.read_text(encoding="utf-8")
        if old_str not in src_text:
            raise RuntimeError(f"Defect anchor not found in {target_file} for {name}")
        mutated_src = FALSIFY_DIR / f"{name}_{target_file.name}"
        mutated_src.write_text(src_text.replace(old_str, new_str, 1), encoding="utf-8")

        mutated_obj = FALSIFY_DIR / f"{name}.o"
        subprocess.run(
            [str(CXX), *common_flags, "-c", str(mutated_src), "-o", str(mutated_obj)],
            check=True,
            env=env,
            cwd=ROOT,
        )

        exe_path = FALSIFY_DIR / f"falsify_{name}.exe"
        subprocess.run(
            [
                str(CXX),
                "-std=c++23",
                "-O2",
                str(test_obj),
                str(mutated_obj),
                "-Wl,--allow-multiple-definition",
                "-Wl,--start-group",
                *[str(lib) for lib in libs],
                str(sdl2_lib),
                "-Wl,--end-group",
                "-lpthread",
                "-lopengl32",
                "-lgdi32",
                "-luser32",
                "-lbcrypt",
                "-lshell32",
                "-lole32",
                "-loleaut32",
                "-luuid",
                "-lcomdlg32",
                "-ladvapi32",
                "-lwinspool",
                "-o",
                str(exe_path),
            ],
            check=True,
            env=env,
            cwd=ROOT,
        )

        proc = subprocess.run(
            [
                str(exe_path),
                ".",
                "research/runs/native-xbox-renderer-0405-20261122/depth-expected.tsv",
            ],
            env=env,
            cwd=ROOT,
            capture_output=True,
            text=True,
            timeout=30,
        )
        rejected = proc.returncode != 0
        if not rejected:
            raise RuntimeError(f"Planted defect '{name}' was NOT rejected (exit 0)!")
        results.append(
            {
                "defect": name,
                "target": str(target_file.relative_to(ROOT)).replace("\\", "/"),
                "exit_code": proc.returncode,
                "rejected": rejected,
            }
        )
    shutil.rmtree(FALSIFY_DIR, ignore_errors=True)
    return results


def main() -> int:
    env = dict(os.environ)
    env["PATH"] = str(BIN_DIR) + os.pathsep + str(ROOT / "third_party" / "sdl2" / "bin") + os.pathsep + env.get("PATH", "")

    hashes = check_strict_native_contract()

    # Run clean baseline test_native_retail_renderer_cameras.exe
    exe = BUILD_DIR / "test_native_retail_renderer_cameras.exe"
    clean_proc = subprocess.run(
        [str(exe), ".", "research/runs/native-xbox-renderer-0405-20261122/depth-expected.tsv"],
        env=env,
        cwd=ROOT,
        capture_output=True,
        text=True,
        timeout=30,
    )
    if clean_proc.returncode != 0:
        print(clean_proc.stdout)
        print(clean_proc.stderr, file=sys.stderr)
        return 1

    falsification = build_and_run_falsification(env)

    report = {
        "slice": "17EBB0_180ED0_native_camera_producer",
        "strict_native_contract": "PASS",
        "runtime_file_sha256": hashes,
        "clean_test_exit_code": clean_proc.returncode,
        "falsification": falsification,
    }
    out_path = ROOT / "research" / "runs" / "native-camera-producer-17ebb0-180ed0-20261126.json"
    out_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"PASS: strict native contract + clean test + {len(falsification)}/{len(falsification)} planted defects rejected")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
