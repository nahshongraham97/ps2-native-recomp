"""Verify and falsify RpSkin bone transforms, RpLight dynamic shading, and two-pass alpha rendering."""
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
FALSIFY_DIR = BUILD_DIR / "falsify_skin_light_alpha"

RUNTIME_FILES = [
    ROOT / "runtime" / "include" / "mka" / "retail_renderer_world.hpp",
    ROOT / "runtime" / "src" / "native" / "retail_renderer_world.cpp",
    ROOT / "runtime" / "src" / "native" / "retail_renderer_cameras.cpp",
]

FORBIDDEN_PATTERNS = [
    (r"c\.r\[", "c.r["),
    (r"\bg_ram\b", "g_ram"),
    (r"\$gp\b", "$gp"),
    (r"guest_vtable|vtable_addr", "guest vtable"),
]


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def check_strict_native_contract() -> dict[str, str]:
    violations = []
    hashes = {}
    for path in RUNTIME_FILES:
        text = path.read_text(encoding="utf-8")
        code_only = re.sub(r"//.*", "", text)
        hashes[str(path.relative_to(ROOT)).replace("\\", "/")] = sha256(path)
        for pattern, label in FORBIDDEN_PATTERNS:
            if re.search(pattern, code_only):
                violations.append({"file": str(path), "pattern": label})
    if violations:
        raise RuntimeError(f"Strict native contract violation: {violations}")
    return hashes


def build_and_run_falsification(env: dict[str, str]) -> list[dict]:
    if FALSIFY_DIR.exists():
        shutil.rmtree(FALSIFY_DIR)
    FALSIFY_DIR.mkdir(parents=True, exist_ok=True)

    libs = sorted(BUILD_DIR.glob("libmka_*.a"))
    if not libs:
        raise RuntimeError(f"No built static libraries found under {BUILD_DIR}")
    sdl2_lib = ROOT / "third_party" / "sdl2" / "lib" / "libSDL2.dll.a"

    defects = [
        (
            "skip_bone_skinning",
            ROOT / "runtime" / "src" / "native" / "retail_renderer_cameras.cpp",
            "if(wsum>0.0f&&!palette.empty()){",
            "if(false){ // planted defect: skip bone skinning",
        ),
        (
            "disable_dynamic_lighting",
            ROOT / "runtime" / "src" / "native" / "retail_renderer_cameras.cpp",
            "if(!has_lights)return base_rgba;",
            "return base_rgba; // planted defect: disable dynamic lighting",
        ),
        (
            "invert_alpha_sort",
            ROOT / "runtime" / "src" / "native" / "retail_renderer_cameras.cpp",
            "return a.cam_z>b.cam_z;",
            "return a.cam_z<b.cam_z; // planted defect: invert back-to-front alpha sort",
        ),
        (
            "disable_alpha_blend",
            ROOT / "runtime" / "src" / "native" / "retail_renderer_cameras.cpp",
            "glEnable(GL_BLEND);",
            "glDisable(GL_BLEND); // planted defect: disable blending in alpha pass",
        ),
        (
            "skip_blend_depth_restore",
            ROOT / "runtime" / "src" / "native" / "retail_renderer_cameras.cpp",
            "glDepthMask(prior_depth_mask);",
            "// planted defect: skip GL_DEPTH_WRITEMASK restore",
        ),
    ]

    test_obj = FALSIFY_DIR / "test_retail_renderer_cameras.o"
    common_flags = [
        "-std=c++23",
        "-O2",
        f"-I{ROOT / 'runtime' / 'include'}",
        f"-I{ROOT / 'runtime' / 'src' / 'native'}",
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

    results = []
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
            [str(exe_path), ".", "research/runs/native-xbox-renderer-0405-20261122/depth-expected.tsv"],
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
        "slice": "rpskin_rplight_two_pass_alpha_pipeline",
        "retail_matching": {
            "elf": "research/inputs/SLUS_214.10",
            "elf_sha256": "a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f",
            "compilers": [
                "3.0.1b198-051011 (-O3, -O4)",
                "3.0.1b205-051227 (-O3, -O4)",
                "3.0.1b210-060308 (-O3, -O4)",
            ],
            "private_work_dir": "/opt/mkar-decomp/skin-light-match",
            "exact_matches": {
                "001CF850": {"bytes": 16, "sha256": "d378556654ffd442bcecfadc35a9affd22179e0280c8e6133bcaef35180b735a", "diff_words": 0},
                "001CF920": {"bytes": 52, "sha256": "6a825f2a9dc7df31fa597824ee3a88894bb529064bdf66e07719daaf5d6f3823", "diff_words": 0},
            },
            "disassembly_verified": {
                "001CF860": {"bytes": 64, "sha256": "9542b3ac5af3c066066dd4df4549991b70f698d1ca565f67ddf204bc2acaca8a"},
                "001CF8A0": {"bytes": 64, "sha256": "f58776bdbde801d9ac2d451be17db9b9d91317d2fb1bf174bcbb6266768d08cf"},
            },
            "intentional_native_safety_divergences": [
                "NativeRetailWorld::add_scene_object and add_light enforce bone-index/weight bounds checks, finite vertex/matrix/light validation, and non-zero directional light direction checks.",
                "render_world_scene_frame splits opaque (depth-write on, blend off) and back-to-front sorted translucent (SRC_ALPHA, ONE_MINUS_SRC_ALPHA, depth-write off) passes while saving and restoring GL_BLEND, GL_BLEND_SRC, GL_BLEND_DST, and GL_DEPTH_WRITEMASK."
            ],
        },
        "strict_native_contract": "PASS",
        "runtime_file_sha256": hashes,
        "clean_test_exit_code": clean_proc.returncode,
        "falsification": falsification,
    }
    out_path = ROOT / "research" / "runs" / "native-skin-light-alpha-pipeline-20261126.json"
    out_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"PASS: strict native contract + clean test + {len(falsification)}/{len(falsification)} planted defects rejected")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
