"""Verify a pinned clean native source checkout without a game or endpoint."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    fork = Path(__file__).resolve().parents[3]
    parent = fork.parents[2]
    clean = fork.parent / "ps2-native-recomp-clean-loose-20261121"
    output = args.output.resolve()
    assert output.is_relative_to(Path(__file__).resolve().parent)
    output.mkdir(exist_ok=False)

    def git(*arguments):
        return subprocess.check_output(["git", *arguments], cwd=clean, text=True).strip()

    assert git("rev-parse", "HEAD") == args.commit
    assert not git("status", "--porcelain", "--untracked-files=no")
    dependencies = parent.parent / "dependencies"
    compiler = dependencies / "compiler/llvm-mingw-20261006-ucrt-x86_64/bin"
    environment = dict(os.environ)
    environment["PATH"] = str(compiler) + os.pathsep + environment["PATH"]
    build = clean / "build-native-loose"
    cmake = dependencies / "cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe"

    def run(name, command, expected=0):
        command = list(map(str, command))
        started = time.monotonic()
        result = subprocess.run(command, cwd=clean, capture_output=True, text=True,
                                env=environment, creationflags=subprocess.CREATE_NO_WINDOW,
                                timeout=300)
        receipt = dict(argv=command, exit=result.returncode, stdout=result.stdout,
                       stderr=result.stderr, seconds=time.monotonic() - started,
                       build_commit=args.commit, game_run=False, frames=[], audio=[])
        executable = Path(command[0])
        if executable.suffix == ".exe":
            receipt["executable_sha256"] = hashlib.sha256(executable.read_bytes()).hexdigest()
        (output / (name + ".json")).write_text(json.dumps(receipt, indent=2) + "\n", newline="\n")
        print(name, result.returncode, flush=True)
        print(result.stdout[-1000:], result.stderr[-400:], flush=True)
        if expected is not None:
            assert result.returncode == expected, name
        return receipt

    targets = ["test_native_retail_reset_sound", "test_native_audio_shared_services",
               "test_native_content_sound_archive", "test_native_sound_file_errors",
               "test_native_retail_initial_reset", "test_native_audio_banks",
               "test_native_retail_sound_bank_jobs_retail"]
    run("build", [cmake, "--build", build, "--target", *targets, "-j2"])
    assets = parent / "private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon"
    ms2, ms4 = assets / "sndsps2/mslasset.ms2", assets / "sndsps2/mslasset.ms4"
    empty = output / "empty-content"
    empty.mkdir(exist_ok=False)
    plans = [(targets[0], [assets, output / "negative-content"]),
             (targets[1], [ms2, ms4]), (targets[2], [output / "adoption-fixtures"]),
             (targets[3], [output / "file-error-fixtures"]), (targets[4], [empty]),
             (targets[5], [ms2, ms4]), (targets[6], [ms2, ms4])]
    identities = []
    for name, extra in plans:
        result = run(name, [build / (name + ".exe"), *extra])
        identities.append(dict(name=name, sha256=result["executable_sha256"], receipt=name + ".json"))

    gate = run("full-entry-negative-link", [cmake, "--build", build, "--target",
               "test_native_startup_missing_bindings", "-j2"], None)
    missing = re.findall(r"undefined symbol: ([^\n]+)", gate["stdout"] + gate["stderr"])
    prior = json.loads((Path(__file__).parent / "caller-validation-01/full-entry-negative-link.json").read_text())
    assert gate["exit"] != 0
    assert sorted(missing) == sorted(prior["undefined_symbols"]) and len(missing) == 40
    assert not (build / "test_native_startup_missing_bindings.exe").exists()
    assert not git("status", "--porcelain", "--untracked-files=no")
    verification = dict(status="PASS_CLEAN_CONNECTED_RESET_BANK_COMPONENTS",
                        clean_source_commit=args.commit, build_type="Release",
                        cache_sha256=hashlib.sha256((build / "CMakeCache.txt").read_bytes()).hexdigest(),
                        clients=identities, remaining_full_entry_symbols=missing,
                        complete_game_executable_produced=False, game_run=False,
                        physical_endpoints_opened=False, frames=[], audio=[],
                        limits=["Connected reset test substitutes four unrelated reset effects and three context/game-program effects; production retains strong requirements",
                                "Native typed playback admission/live-reference cleanup and actual game GPU draws remain OPEN",
                                "This is not full-game acceptance"])
    (output / "verification.json").write_text(json.dumps(verification, indent=2) + "\n", newline="\n")
    print(verification["status"], flush=True)


if __name__ == "__main__":
    main()
