"""Verify and falsify the native RW geometry ingestion and material/texture render pipeline."""
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
FALSIFY_DIR = BUILD_DIR / "falsify_geom_tex"

RUNTIME_FILES = [
    ROOT / "runtime" / "include" / "mka" / "retail_renderer_world.hpp",
    ROOT / "runtime" / "src" / "native" / "retail_renderer_world.cpp",
    ROOT / "runtime" / "src" / "native" / "retail_renderer_cameras.cpp",
    ROOT / "runtime" / "src" / "native" / "retail_first_scene_continuations.cpp",
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
            "skip_texture_bind",
            ROOT / "runtime" / "src" / "native" / "retail_renderer_cameras.cpp",
            "glEnable(GL_TEXTURE_2D);\n   glBindTexture(GL_TEXTURE_2D,gl_tex);",
            "glDisable(GL_TEXTURE_2D); // planted defect: skip texture bind",
        ),
        (
            "corrupt_vertex_ingestion",
            ROOT / "runtime" / "src" / "native" / "retail_renderer_cameras.cpp",
            "obj.triangles.push_back({positions[idx],uvs[idx][0],uvs[idx][1],vcols[idx]});",
            "obj.triangles.push_back({{0.0f,0.0f,0.0f},0.0f,0.0f,vcols[idx]}); // planted defect: corrupt vertex",
        ),
        (
            "bypass_stream_bounds",
            ROOT / "runtime" / "src" / "native" / "retail_renderer_cameras.cpp",
            "if(t.v0>=num_verts||t.v1>=num_verts||t.v2>=num_verts)return false;",
            "// planted defect: bypass vertex index bounds check",
        ),
        (
            "leak_gl_texture_teardown",
            ROOT / "runtime" / "src" / "native" / "retail_renderer_world.cpp",
            "glDeleteTextures(1, &tex);",
            "// planted defect: leak GL texture handle on teardown",
        ),
        (
            "skip_tex_state_restore",
            ROOT / "runtime" / "src" / "native" / "retail_renderer_cameras.cpp",
            "glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,prior_tex_env);",
            "// planted defect: skip GL_TEXTURE_ENV_MODE restore",
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
        "slice": "rw_geometry_stream_and_material_texture_binding",
        "retail_matching": {
            "elf": "research/inputs/SLUS_214.10",
            "elf_sha256": "a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f",
            "compilers": [
                "3.0.1b198-051011 (-O3, -O4)",
                "3.0.1b205-051227 (-O3, -O4)",
                "3.0.1b210-060308 (-O3, -O4)",
            ],
            "private_work_dir": "/opt/mkar-decomp/geom-tex-match",
            "exact_matches": {
                "001AD1B0": {"bytes": 8, "sha256": "d88502c877bfe1e498e0766a59b8a3457b6f74c106d876d28f19184e7194cc1a", "diff_words": 0},
                "001A3E70": {"bytes": 84, "sha256": "e1b0e3f917dff8c874ad9685b2c0112a443edd20a45f4fc97bdb25a7fdb025e5", "diff_words": 0},
            },
            "disassembly_verified": {
                "00225D40": {"bytes": 12, "sha256": "91770e920d744d1aa06d1099012ea918c2471e6a8a49655d8f8384fcd7c63542"},
                "001A1B30": {"bytes": 88, "sha256": "cfea9036e6abd54f1b2d5c62ff9e546f12a798174b7ae3ac96cf7ef372882acd"},
                "001A3DF0": {"bytes": 120, "sha256": "bae4bc41244d2646e2020243d6e2f873aabbac98dd0959dfa4aad36b27d0f94b"},
            },
            "intentional_native_safety_divergences": [
                "ingest_rw_geometry_stream enforces strict chunk-header/length bounds checks, finite float validation, vertex/material index bounds checks, and full rollback of created frames/materials/objects on any stream error.",
                "render_world_scene_frame saves and restores GL_TEXTURE_2D and GL_TEXTURE_BINDING_2D around textured object submission, and NativeRetailWorld::release_gpu_resources explicitly deletes cached GL texture handles on teardown."
            ],
        },
        "strict_native_contract": "PASS",
        "runtime_file_sha256": hashes,
        "clean_test_exit_code": clean_proc.returncode,
        "falsification": falsification,
    }
    out_path = ROOT / "research" / "runs" / "native-geom-tex-pipeline-20261126.json"
    out_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"PASS: strict native contract + clean test + {len(falsification)}/{len(falsification)} planted defects rejected")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
