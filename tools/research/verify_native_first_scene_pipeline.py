"""Verify and falsify live first-scene asset loading, PS2 pixel unswizzling, and render_world_scene_frame dispatch."""
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
FALSIFY_DIR = BUILD_DIR / "falsify_first_scene"

RUNTIME_FILES = [
    ROOT / "runtime" / "include" / "mka" / "retail_scene_owners.hpp",
    ROOT / "runtime" / "src" / "native" / "retail_first_scene_continuations.cpp",
    ROOT / "runtime" / "src" / "native" / "retail_initial_movie_path.cpp",
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

    common_flags = [
        "-std=c++23",
        "-O2",
        f"-I{ROOT / 'runtime' / 'include'}",
        f"-I{ROOT / 'runtime' / 'src' / 'native'}",
        f"-I{ROOT / 'tests'}",
        f"-I{ROOT / 'third_party' / 'sdl2' / 'include'}",
        *[f"-I{d}" for d in BUILD_DIR.glob("native-retail-startup-*") if d.is_dir()],
        *[f"-I{d}" for d in BUILD_DIR.glob("native-typed-retail-*") if d.is_dir()],
    ]

    first_scene_obj = FALSIFY_DIR / "test_retail_first_scene_pipeline.o"
    cam_test_obj = FALSIFY_DIR / "test_retail_renderer_cameras.o"
    for src, obj in [
        (ROOT / "tests" / "test_retail_first_scene_pipeline.cpp", first_scene_obj),
        (ROOT / "tests" / "test_retail_renderer_cameras.cpp", cam_test_obj),
    ]:
        subprocess.run(
            [str(CXX), *common_flags, "-c", str(src), "-o", str(obj)],
            check=True,
            env=env,
            cwd=ROOT,
        )

    defects = [
        (
            "skip_first_scene_stream_load",
            ROOT / "runtime" / "src" / "native" / "retail_first_scene_continuations.cpp",
            "if (!txd_key.empty() || !world_key.empty()) {",
            "if (false) { // planted defect: skip first-scene stream load",
            first_scene_obj,
            [],
        ),
        (
            "leak_world_gl_resources_on_teardown",
            ROOT / "runtime" / "src" / "native" / "retail_first_scene_continuations.cpp",
            "    main.scene.reset();",
            "    // planted defect: leak scene and GL resources on teardown",
            first_scene_obj,
            [],
        ),
        (
            "skip_render_world_scene_dispatch",
            ROOT / "runtime" / "src" / "native" / "retail_initial_movie_path.cpp",
            "if (main.scene && main.scene->scene_loaded && main.scene->world &&",
            "if (false && main.scene && main.scene->scene_loaded && main.scene->world &&",
            first_scene_obj,
            [],
        ),
        (
            "bypass_txd_swizzle_bounds_check",
            ROOT / "runtime" / "src" / "native" / "retail_renderer_cameras.cpp",
            "    if(nb.remaining()<swizzled_bytes){\n"
            "     rollback();failure.kind=RetailRendererTextureError::invalid_raster;failure.operation=\"TXD swizzled PSMCT32 payload truncated\";return false;\n"
            "    }\n"
            "    std::vector<std::uint8_t> raw_swizzled(swizzled_bytes);\n"
            "    if(!nb.read_bytes(raw_swizzled.data(),swizzled_bytes)){\n"
            "     rollback();failure.kind=RetailRendererTextureError::invalid_raster;failure.operation=\"TXD swizzled PSMCT32 read failed\";return false;\n"
            "    }",
            "    std::vector<std::uint8_t> raw_swizzled(swizzled_bytes); // planted defect: bypass swizzled TXD bounds check",
            cam_test_obj,
            [".", "research/runs/native-xbox-renderer-0405-20261122/depth-expected.tsv"],
        ),
        (
            "corrupt_ps2_pixel_unswizzle",
            ROOT / "runtime" / "src" / "native" / "retail_renderer_cameras.cpp",
            "return page*8192u+inner;",
            "return 0u; // planted defect: corrupt PS2 pixel unswizzle",
            cam_test_obj,
            [".", "research/runs/native-xbox-renderer-0405-20261122/depth-expected.tsv"],
        ),
    ]

    results = []
    for name, target_file, old_str, new_str, test_obj, run_args in defects:
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
            [str(exe_path), *run_args],
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

    exe_first = BUILD_DIR / "test_native_retail_first_scene_pipeline.exe"
    clean_first = subprocess.run(
        [str(exe_first)],
        env=env,
        cwd=ROOT,
        capture_output=True,
        text=True,
        timeout=30,
    )
    if clean_first.returncode != 0:
        print(clean_first.stdout)
        print(clean_first.stderr, file=sys.stderr)
        return 1

    exe_cam = BUILD_DIR / "test_native_retail_renderer_cameras.exe"
    clean_cam = subprocess.run(
        [str(exe_cam), ".", "research/runs/native-xbox-renderer-0405-20261122/depth-expected.tsv"],
        env=env,
        cwd=ROOT,
        capture_output=True,
        text=True,
        timeout=30,
    )
    if clean_cam.returncode != 0:
        print(clean_cam.stdout)
        print(clean_cam.stderr, file=sys.stderr)
        return 1

    falsification = build_and_run_falsification(env)

    report = {
        "slice": "first_scene_asset_load_ps2_unswizzle_and_render_dispatch",
        "retail_matching": {
            "elf": "research/inputs/SLUS_214.10",
            "elf_sha256": "a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f",
            "compilers": [
                "3.0.1b198-051011 (-O3, -O4)",
                "3.0.1b205-051227 (-O3, -O4)",
                "3.0.1b210-060308 (-O3, -O4)",
            ],
            "private_work_dir": "/opt/mkar-decomp/first-scene-match",
            "exact_matches": {
                "00225D40": {"bytes": 12, "sha256": "91770e920d744d1aa06d1099012ea918c2471e6a8a49655d8f8384fcd7c63542", "diff_words": 0},
                "001AD1B0": {"bytes": 8, "sha256": "d88502c877bfe1e498e0766a59b8a3457b6f74c106d876d28f19184e7194cc1a", "diff_words": 0},
                "00225D90": {"bytes": 8, "sha256": "5ce5ad86d452c4d2422bd63e15223d5d6b3dfb77f224a88c1f476e9fb34e359d", "diff_words": 0},
                "00225DA0": {"bytes": 12, "sha256": "7a6a4ea8b3e473358aa6971c0a898957cf9747b50f79f924a4f041c99bbae9b9", "diff_words": 0},
                "00181870": {"bytes": 40, "sha256": "dd9b0191eac3786cdcd968d3b333c5f422c5678c046e137917988976a60d0698", "diff_words": 0},
            },
            "disassembly_verified": {
                "00225D50": {"bytes": 52, "sha256": "5c9f8d8c74747066d08a485f13718c76fe5c37c68065eae16f8385c33e53331e"},
                "00126940": {"role": "render_retail_frame (RwCameraClear/BeginUpdate/EndUpdate/ShowRaster)"},
            },
            "intentional_native_safety_divergences": [
                "continue_first_scene_at_225d40 and ingest_rw_tex_dictionary_stream enforce fail-closed stream/TOC/pixel-swizzle bounds checks and clean rollback on malformed assets.",
                "In asset-less headless mode (no scene TXD/BSP/DFF in NativeContentVolume), continue_first_scene_at_225d40 leaves has_world=false and returns ready so the 60-frame headless gate completes cleanly.",
                "In asset-loaded headless mode without an active GL context, render_retail_frame skips GPU submission cleanly without GL errors.",
                "continue_scene_reset_at_125d60 explicitly detaches cameras, clears published world state, and releases NativeRetailWorld GPU resources to prevent GL texture leaks."
            ],
        },
        "strict_native_contract": "PASS",
        "runtime_file_sha256": hashes,
        "clean_test_exit_codes": {
            "test_native_retail_first_scene_pipeline": clean_first.returncode,
            "test_native_retail_renderer_cameras": clean_cam.returncode,
        },
        "falsification": falsification,
    }
    out_path = ROOT / "research" / "runs" / "native-first-scene-pipeline-20261126.json"
    out_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"PASS: strict native contract + clean tests + {len(falsification)}/{len(falsification)} planted defects rejected")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
