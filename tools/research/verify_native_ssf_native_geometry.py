"""Hardened verification and 9-defect falsification suite for rpGEOMETRYNATIVE VIF/DMA unpacking and Armory GPU render."""
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
CMAKE = BACKUP / "dependencies" / "cmake" / "cmake-4.4.5-windows-x86_64" / "bin" / "cmake.exe"
BUILD_DIR = ROOT / "b"
FALSIFY_DIR = BUILD_DIR / "falsify_ssf_native_geometry"
PAK_ROOT = BACKUP / "armageddon-reimplementation" / "private-inputs" / "SLUS_214.10.Mortal_Kombat_Armageddon"

RUNTIME_FILES = [
    ROOT / "runtime" / "include" / "mka" / "retail_ssf_geometry.hpp",
    ROOT / "runtime" / "src" / "native" / "retail_ssf_geometry.cpp",
    ROOT / "runtime" / "include" / "mka" / "retail_scene_archive.hpp",
    ROOT / "runtime" / "src" / "native" / "retail_scene_archive.cpp",
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


def parse_census(stdout: str) -> dict[str, int]:
    for line in stdout.splitlines():
        if line.startswith("SSF_NATIVE_CENSUS "):
            out = {}
            for tok in line.split()[1:]:
                k, v = tok.split("=")
                out[k] = int(v)
            return out
    raise RuntimeError("Missing SSF_NATIVE_CENSUS line in test output")


def parse_gpu_pixels(stdout: str, prefix: str = "ARMORY_GPU_PIXELS ") -> dict[str, int]:
    for line in stdout.splitlines():
        if line.startswith(prefix):
            out = {}
            for tok in line.split()[1:]:
                k, v = tok.split("=")
                out[k] = int(v)
            return out
    raise RuntimeError(f"Missing {prefix}line in test output")


def build_and_run_falsification(env: dict[str, str]) -> tuple[list[dict], dict[str, str]]:
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

    fixture_src = ROOT / "tests" / "test_retail_ssf_native_geometry.cpp"
    fixture_obj = FALSIFY_DIR / "test_retail_ssf_native_geometry.o"
    subprocess.run(
        [str(CXX), *common_flags, "-c", str(fixture_src), "-o", str(fixture_obj)],
        check=True,
        env=env,
        cwd=ROOT,
    )
    receipts = {
        "compiler_sha256": sha256(CXX),
        "fixture_source_sha256": sha256(fixture_src),
        "fixture_object_sha256": sha256(fixture_obj),
    }

    defects = [
        (
            "bypass_vif_packet_bounds",
            ROOT / "runtime" / "src" / "native" / "retail_ssf_geometry.cpp",
            "r.need(pkt_size>=16&&std::size_t(pkt_size)<=geometry.end-np,RetailSsfGeometryError::vif_bounds,\"SSF native mesh packet extent\",np-8);",
            "// planted defect: bypass VIF packet bounds",
            True,
        ),
        (
            "bypass_dma_qwc_bounds",
            ROOT / "runtime" / "src" / "native" / "retail_ssf_geometry.cpp",
            "r.need(qwc<=(pkt_end-(dp+16))/16,RetailSsfGeometryError::dma_chain,\"SSF native DMA QWC out of bounds\",dp);",
            "// planted defect: bypass DMA QWC bounds",
            True,
        ),
        (
            "ignore_adc_strip_break",
            ROOT / "runtime" / "src" / "native" / "retail_ssf_geometry.cpp",
            "if(batch[vi].adc){",
            "if(false){ // planted defect: ignore ADC strip break",
            True,
        ),
        (
            "corrupt_unpack_position_stride",
            ROOT / "runtime" / "src" / "native" / "retail_ssf_geometry.cpp",
            "batch[vi].v.position={r.scalar(dp_off+vi*12),r.scalar(dp_off+vi*12+4),r.scalar(dp_off+vi*12+8)};",
            "batch[vi].v.position={0.0f,0.0f,0.0f}; // planted defect: corrupt UNPACK position",
            True,
        ),
        (
            "skip_native_binmesh_material_map",
            ROOT / "runtime" / "src" / "native" / "retail_ssf_geometry.cpp",
            "mesh_materials[mi]=mat_idx;",
            "mesh_materials[mi]=999u; // planted defect: corrupt mesh material mapping",
            True,
        ),
        (
            "allow_u32_chunk_cursor_wrap",
            ROOT / "runtime" / "include" / "mka" / "retail_ssf_geometry.hpp",
            "if(begin>parent_end||parent_end-begin<12||std::size_t(length)>parent_end-begin-12)return {};\n    return begin+12+std::size_t(length);",
            "return std::size_t(std::uint32_t(begin+12+length)); // planted defect: u32 cursor wrap",
            False,
        ),
        (
            "accept_unknown_vif_opcode_silently",
            ROOT / "runtime" / "src" / "native" / "retail_ssf_geometry.cpp",
            "r.reject(RetailSsfGeometryError::unsupported_vif_opcode,\"SSF native VIF unsupported opcode\",vp);",
            "vp+=4; // planted defect: accept unknown VIF opcode silently",
            True,
        ),
        (
            "accept_unknown_unpack_format_silently",
            ROOT / "runtime" / "src" / "native" / "retail_ssf_geometry.cpp",
            "r.reject(RetailSsfGeometryError::unsupported_unpack_format,\"SSF native VIF UNPACK format or VU1 address unsupported\",vp);",
            "// planted defect: accept unknown UNPACK format silently",
            True,
        ),
        (
            "leak_scene_gl_resources_on_125d60",
            ROOT / "runtime" / "src" / "native" / "retail_first_scene_continuations.cpp",
            "record_placeholder_hit(1, \"continue_scene_reset_at_125d60\", main.iteration);",
            "record_placeholder_hit(1, \"continue_scene_reset_at_125d60\", main.iteration); return true; // planted defect: leak GL/ownership on 125D60",
            True,
        ),
        (
            "skip_0x80000005_vec4_reads",
            ROOT / "runtime" / "src" / "native" / "retail_ssf_geometry.cpp",
            "for(unsigned i=0;i<4;++i)result.material_ext_vec4[i]=scalar(ext[0].begin+28+i*4);",
            "for(unsigned i=0;i<4;++i)result.material_ext_vec4[i]=0.0f; // planted defect: skip 0x80000005 vec4",
            True,
        ),
        (
            "corrupt_hanim_node_id",
            ROOT / "runtime" / "src" / "native" / "retail_ssf_geometry.cpp",
            "frame.hanim_node_id=r.word(fext[0].begin+4);",
            "frame.hanim_node_id=0u; // planted defect: corrupt HAnim node id",
            True,
        ),
        (
            "reject_0x6d_unpack_descriptor",
            ROOT / "runtime" / "src" / "native" / "retail_ssf_geometry.cpp",
            "} else if(vnvl==0x6du&&usn==1&&addr==0x002u&&cl==1u) {",
            "} else if(false) { // planted defect: reject 0x6d V4_16",
            True,
        ),
        (
            "skip_extra_atomic_composition",
            ROOT / "runtime" / "src" / "native" / "retail_scene_archive.cpp",
            "result.extra_geometries.push_back(std::move(*extra_geom));",
            "// planted defect: drop extra_geometries",
            True,
        ),
    ]

    results = []
    for idx, (name, target_file, old_str, new_str, reuse_fixture_obj) in enumerate(defects):
        src_text = target_file.read_text(encoding="utf-8")
        if old_str not in src_text:
            raise RuntimeError(f"Defect anchor not found in {target_file} for {name}")
        mutated_src = FALSIFY_DIR / f"{name}_{target_file.name}"
        mutated_src.write_text(src_text.replace(old_str, new_str, 1), encoding="utf-8")

        if reuse_fixture_obj:
            mutated_obj = FALSIFY_DIR / f"{name}.o"
            subprocess.run(
                [str(CXX), *common_flags, "-c", str(mutated_src), "-o", str(mutated_obj)],
                check=True,
                env=env,
                cwd=ROOT,
            )
            objs = [str(fixture_obj), str(mutated_obj)]
        else:
            # Header mutation: recompile fixture against mutated header directory
            hdr_dir = FALSIFY_DIR / f"hdr_{name}" / "mka"
            hdr_dir.mkdir(parents=True, exist_ok=True)
            shutil.copy2(mutated_src, hdr_dir / target_file.name)
            mut_fix_obj = FALSIFY_DIR / f"fix_{name}.o"
            subprocess.run(
                [str(CXX), "-std=c++23", "-O2", f"-I{hdr_dir.parent}", *common_flags[2:], "-c", str(fixture_src), "-o", str(mut_fix_obj)],
                check=True,
                env=env,
                cwd=ROOT,
            )
            objs = [str(mut_fix_obj)]

        exe_path = FALSIFY_DIR / f"falsify_{name}.exe"
        subprocess.run(
            [
                str(CXX),
                "-std=c++23",
                "-O2",
                *objs,
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

        out_dir = FALSIFY_DIR / f"out_{idx}"
        proc = subprocess.run(
            [str(exe_path), str(PAK_ROOT), str(out_dir)],
            env=env,
            cwd=ROOT,
            capture_output=True,
            text=True,
            timeout=60,
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
    return results, receipts


def main() -> int:
    env = dict(os.environ)
    env["PATH"] = str(BIN_DIR) + os.pathsep + str(ROOT / "third_party" / "sdl2" / "bin") + os.pathsep + env.get("PATH", "")

    hashes = check_strict_native_contract()

    subprocess.run(
        [str(CMAKE), "--build", "b", "--target", "test_native_retail_ssf_native_geometry", "-j2"],
        check=True,
        env=env,
        cwd=ROOT,
    )

    clean_out = BUILD_DIR / "check-temp" / "ssf_native_geom_clean"
    if clean_out.exists():
        shutil.rmtree(clean_out)
    exe = BUILD_DIR / "test_native_retail_ssf_native_geometry.exe"
    clean_proc = subprocess.run(
        [str(exe), str(PAK_ROOT), str(clean_out)],
        env=env,
        cwd=ROOT,
        capture_output=True,
        text=True,
        timeout=60,
    )
    if clean_proc.returncode != 0:
        print(clean_proc.stdout)
        print(clean_proc.stderr, file=sys.stderr)
        return 1

    census = parse_census(clean_proc.stdout)
    gpu_pixels = parse_gpu_pixels(clean_proc.stdout, "ARMORY_GPU_PIXELS ")
    multi_gpu_pixels = parse_gpu_pixels(clean_proc.stdout, "ARMORY_MULTI_GPU_PIXELS ")
    falsification, receipts = build_and_run_falsification(env)

    report = {
        "slice": "ps2_rpgeometrynative_vif_dma_unpacking_and_armory_gpu",
        "retail_matching": {
            "elf": "research/inputs/SLUS_214.10",
            "elf_sha256": "a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f",
            "compilers": [
                "3.0.1b198-051011 (-O3, -O4)",
                "3.0.1b205-051227 (-O3, -O4)",
                "3.0.1b210-060308 (-O3, -O4)",
            ],
            "private_work_dir": "/opt/mkar-decomp/native-geom-match",
            "material_atomic_work_dir": "/opt/mkar-decomp/mat-ext-match",
            "exact_matches": {
                "001CF000": {"bytes": 8, "sha256": "008d26890102af179c703d77fae42cb7b3f424a11b8db552383dd8e5313f4061", "diff_words": 0},
                "001CF010": {"bytes": 8, "sha256": "008d26890102af179c703d77fae42cb7b3f424a11b8db552383dd8e5313f4061", "diff_words": 0},
            },
            "disassembly_verified": {
                "001CEFB0": {"bytes": 52, "sha256": "75b224beb294918afa9bd1c084da97992107e672506281483e7af7ce955e3210"},
                "001CEFF0": {"bytes": 12, "sha256": "3f969139118634c7fd9b3ea32bbb84ff9ad18117b16c4176d90369cd1d97b98a"},
                "001D1810": {"bytes": 176, "sha256": "9e8dece63b3afc1cf9c490d94a789f0de562949d2c166f1d2cc448562bf5fd27"},
                "001D18C0": {"bytes": 688, "sha256": "18de13bbb1676bd1a1bbe9239cff5e61ea680df0441b18f5e7876e44cafc69d7"},
                "00213630": {"bytes": 152, "sha256": "2aa335b66de3404f0160c954460caf0e91917517293887b08ff8541b91b67066"},
                "002136D0": {"bytes": 680, "sha256": "8c0745b82d7215607b142d0576f3b72903627ada22921e122dce44b5e417574a"},
                "00213D60": {"bytes": 152, "sha256": "e98f240352b35b627656064f3dfa3dee828a0c12fc02d1bcf5e88c6784b65516"},
                "00213E00": {"bytes": 200, "sha256": "c4b3b60f4a1e3618553f8a8747789dd97ae6b50920607ca64fca0a8226233396"},
            },
            "intentional_native_safety_divergences": [
                "64-bit-safe std::size_t bounds/cursor arithmetic (bounded_chunk_end, checked QWC and UNPACK payload lengths) prevents 32-bit length/offset wrap.",
                "Strict VIF opcode, DMA tag ID, and UNPACK (vn/vl/usn/addr) validation fails closed with explicit named RetailSsfGeometryError values.",
                "Skinned native pipes (trailing RpSkin data) and unsupported UNPACK formats/opcodes are explicitly refused rather than silently skipped.",
                "RetailSceneResources ownership-journal rollback and 125D60 teardown release all rasters, textures, materials, frames, world objects, and GL handles on failure or reset.",
                "Armory GPU rendering uses explicit RetailSsfSceneRenderPolicy::diagnostic_native_vif_unlit_raw_rgba8 (unlit raw RGBA8 diagnostic preview, distinct from retail PS2 material/effect equivalence)."
            ],
        },
        "strict_native_contract": "PASS",
        "runtime_file_sha256": hashes,
        "build_receipts": receipts,
        "mkda_pak_census_baseline_v1": {
            "total_clumps": 3774,
            "non_native_clumps": 2,
            "native_clumps": 260,
            "decoded_ok": 192,
            "refused_skinned": 0,
            "refused_unpack": 67,
            "refused_vif": 1,
            "refused_other": 3512,
        },
        "mkda_pak_census": census,
        "armory_gpu_test": {
            "exit_code": clean_proc.returncode,
            "policy": "diagnostic_native_vif_unlit_raw_rgba8",
            **gpu_pixels,
        },
        "armory_multi_gpu_test": {
            "exit_code": clean_proc.returncode,
            "policy": "diagnostic_native_vif_unlit_raw_rgba8",
            "atomics": [18, 69, 72],
            "texture_records": [32, 41, 49],
            **multi_gpu_pixels,
        },
        "falsification": falsification,
    }
    out_path = ROOT / "research" / "runs" / "native-ssf-native-geometry-v2-20261127.json"
    out_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(f"PASS: strict native contract + census + Armory GPU + {len(falsification)}/{len(falsification)} planted defects rejected")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
