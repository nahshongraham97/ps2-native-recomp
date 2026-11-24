"""Reproducible build-time AOT C++ generation pipeline from pinned SLUS_214.10 and VU1 captures."""
from pathlib import Path
import shutil
import subprocess
import sys


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
    parent, fork_root = resolve_roots()
    if parent is None:
        raise RuntimeError("Parent workspace with private-inputs/ required for pinned SLUS_214.10 and VU1 captures.")
    deps = parent.parent / "dependencies"
    cmake_exe = next(deps.rglob("cmake.exe"), None) or Path(shutil.which("cmake"))
    ninja_exe = next(deps.rglob("ninja.exe"), None) or Path(shutil.which("ninja"))
    clang = deps / "compiler" / "llvm-mingw-20261006-ucrt-x86_64" / "bin" / "clang.exe"
    clangpp = deps / "compiler" / "llvm-mingw-20261006-ucrt-x86_64" / "bin" / "clang++.exe"

    bdir = fork_root / "b"
    sdl2_dir = fork_root / "third_party" / "sdl2" / "lib" / "cmake" / "SDL2"
    subprocess.run([
        str(cmake_exe), "-S", str(fork_root), "-B", str(bdir), "-G", "Ninja",
        f"-DCMAKE_MAKE_PROGRAM={ninja_exe}",
        f"-DCMAKE_C_COMPILER={clang}",
        f"-DCMAKE_CXX_COMPILER={clangpp}",
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DSDL2_DIR={sdl2_dir}",
    ], check=True)
    subprocess.run([str(cmake_exe), "--build", str(bdir), "--target", "eerecomp", "vurecomp"], check=True)

    elf = parent / "private-inputs" / "SLUS_214.10.Mortal_Kombat_Armageddon" / "SLUS_214.10"
    entries = fork_root / "recomp" / "mkarmageddon_entries.txt"
    vu_in = parent / "private-inputs" / "mkarmageddon-aot-inputs" / "vu1"
    out_ee = fork_root / "build" / "generated-aot" / "ee"
    out_vu = fork_root / "build" / "generated-aot" / "vu1"
    for d in (out_ee, out_vu):
        if d.exists():
            shutil.rmtree(d)
        d.mkdir(parents=True)

    r1 = subprocess.run([str(bdir / "eerecomp.exe"), str(elf), "--entries", str(entries), "--out", str(out_ee)], capture_output=True, text=True, check=True)
    r2 = subprocess.run([str(bdir / "vurecomp.exe"), str(vu_in), str(out_vu), "--require-complete"], capture_output=True, text=True, check=True)
    print(r1.stdout.strip())
    print(r2.stdout.strip())


if __name__ == "__main__":
    main()
