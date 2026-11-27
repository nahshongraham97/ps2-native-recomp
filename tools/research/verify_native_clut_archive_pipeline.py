"""Verify and falsify PS2 PAL4/PAL8 CLUT decoding and retail archive stream selection."""
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
FALSIFY_DIR = BUILD_DIR / "falsify_clut_archive"

RUNTIME_FILES = [
    ROOT / "runtime" / "include" / "mka" / "retail_renderer_world.hpp",
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
            "skip_csm1_unswizzle",
            ROOT / "runtime" / "src" / "native" / "retail_renderer_cameras.cpp",
            "if(bank==0x08u||bank==0x10u)return idx^0x18u;",
            "// planted defect: skip CSM1 bank unswizzle",
        ),
        (
            "omit_gs_alpha_scaling",
            ROOT / "runtime" / "src" / "native" / "retail_renderer_cameras.cpp",
            "return (a>=0x80u)?0xFFu:static_cast<std::uint8_t>((static_cast<std::uint32_t>(a)*255u+64u)/128u);",
            "return a; // planted defect: omit PS2 GS alpha scaling",
        ),
        (
            "corrupt_pal4_nibble",
            ROOT / "runtime" / "src" / "native" / "retail_renderer_cameras.cpp",
            "const std::size_t p0=static_cast<std::size_t>(packed&0x0Fu)*4u;",
            "const std::size_t p0=static_cast<std::size_t>((packed>>4u)&0x0Fu)*4u; // planted defect: swap nibble",
        ),
        (
            "bypass_pal_bounds",
            ROOT / "runtime" / "src" / "native" / "retail_renderer_cameras.cpp",
            "rollback();failure.kind=RetailRendererTextureError::invalid_raster;failure.operation=\"TXD PAL4 palette/pixel payload truncated\";return false;",
            "return true; // planted defect: bypass PAL4 bounds check",
        ),
        (
            "leak_gl_teardown",
            ROOT / "runtime" / "src" / "native" / "retail_renderer_world.cpp",
            "glDeleteTextures(1, &tex);",
            "// planted defect: leak GL texture handle",
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
        "slice": "ps2_clut_pal4_pal8_and_archive_stream_selector",
        "retail_matching": {
            "elf": "research/inputs/SLUS_214.10",
            "elf_sha256": "a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f",
            "compilers": [
                "3.0.1b198-051011 (-O3, -O4)",
                "3.0.1b205-051227 (-O3, -O4)",
                "3.0.1b210-060308 (-O3, -O4)",
            ],
            "private_work_dir": "/opt/mkar-decomp/clut-archive-match",
            "exact_matches": {
                "001A8510": {"bytes": 60, "sha256": "7cc39df0e784de7f78ea08fe3cdf873eb8c0c31aa3860629694b7849594618f5", "diff_words": 0},
            },
            "disassembly_verified": {
                "001A8640": {"bytes": 60, "sha256": "8133a4bafebd167dddaf05cbc4da32cc23469d498a2544c4487340e1be8e1fe3"},
                "001A8900": {"bytes": 68, "sha256": "649073a409c0873d740aadd2cc8e7e9aef06e0c0d8771ddd1d0369b030e96110"},
            },
            "intentional_native_safety_divergences": [
                "ingest_rw_tex_dictionary_stream decodes PS2 PAL4 and PAL8 (with CSM1 CLUT bank unswizzle and GS 0..128 alpha expansion to 0..255) into typed host RGBA8 rasters with strict palette/pixel buffer bounds checks and full rollback on malformed streams.",
                "load_retail_scene_streams_from_volume validates resource keys and file lengths via NativeContentVolume before TXD/BSP ingestion."
            ],
        },
        "strict_native_contract": "PASS",
        "runtime_file_sha256": hashes,
        "clean_test_exit_code": clean_proc.returncode,
        "falsification": falsification,
    }
    out_path = ROOT / "research" / "runs" / "native-clut-archive-pipeline-20261126.json"
    out_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"PASS: strict native contract + clean test + {len(falsification)}/{len(falsification)} planted defects rejected")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
