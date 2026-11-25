"""Reproduce the bounded ownership rows in clean checkouts with pinned private inputs."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def row_digest(rows):
    return hashlib.sha256(json.dumps(rows, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def input_destination(item, roots):
    if item["destination_zone"] not in roots:
        raise ValueError("Unknown private input destination zone")
    root = roots[item["destination_zone"]].resolve()
    path = (root / item["destination"]).resolve()
    if not path.is_relative_to(root) or path == root:
        raise ValueError("Private input destination escapes its checkout")
    return path


def verify_inputs(manifest, roots):
    seen = set()
    for item in manifest["inputs"]:
        source = Path(item["source"])
        target = input_destination(item, roots)
        if target in seen:
            raise ValueError("Duplicate private input destination")
        seen.add(target)
        if source.stat().st_size != item["bytes"] or digest(source) != item["sha256"]:
            raise ValueError("Private ownership input identity changed: " + item["destination"])
    listing = manifest["private_listing"]
    if not any(item["sha256"] == listing["sha256"] and item["destination"].endswith("/instructions.tsv")
               for item in manifest["inputs"]):
        raise ValueError("Pinned private Ghidra listing is missing")


def git(root, *arguments):
    return subprocess.check_output(["git", *arguments], cwd=root, text=True, encoding="utf-8").strip()


def command(arguments, cwd, environment):
    result = subprocess.run([str(value) for value in arguments], cwd=cwd, env=environment,
                            capture_output=True, timeout=300, creationflags=subprocess.CREATE_NO_WINDOW)
    if result.returncode:
        raise RuntimeError(result.stderr.decode(errors="replace")[-3000:])
    return result


def self_test(manifest, roots):
    import copy
    verify_inputs(manifest, roots)
    checks = []
    for label, modify in (
        ("changed private input digest", lambda x: x["inputs"][0].update(sha256="0" * 64)),
        ("escaped input destination", lambda x: x["inputs"][0].update(destination="../../outside")),
        ("missing Ghidra input", lambda x: x.update(inputs=x["inputs"][1:])),
    ):
        bad = copy.deepcopy(manifest)
        modify(bad)
        try:
            verify_inputs(bad, roots)
        except ValueError:
            checks.append(label)
        else:
            raise AssertionError("Private input verifier accepted " + label)
    return {"status": "PASS", "planted_rejections": checks}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--parent-root", type=Path, required=True)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument("--expected-census", type=Path, required=True)
    parser.add_argument("--sdk31-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    parent = args.parent_root.resolve()
    fork = parent / "external/forks/ps2-native-recomp"
    output = args.output.resolve()
    telemetry = (fork / "build/oracle-telemetry").resolve()
    if output.exists() or not output.is_relative_to(telemetry):
        raise ValueError("Reproduction requires a new path in owned telemetry")
    manifest = json.loads(args.manifest.read_bytes())
    source_commits = {label: git(root, "rev-parse", "HEAD") for label, root in (("parent", parent), ("fork", fork))}
    roots = {"workspace": output, "parent": output / "Armageddon-backup/armageddon-reimplementation"}
    roots["fork"] = roots["parent"] / "external/forks/ps2-native-recomp"
    controls = self_test(manifest, roots)
    environment = dict(os.environ, GIT_LFS_SKIP_SMUDGE="1", GIT_OPTIONAL_LOCKS="0", PYTHONDONTWRITEBYTECODE="1")
    output.mkdir(parents=True)
    for label, original in (("parent", parent), ("fork", fork)):
        destination = roots[label]
        destination.parent.mkdir(parents=True, exist_ok=True)
        command(["git", "clone", "--shared", "--no-hardlinks", "--no-checkout", original, destination], parent, environment)
        command(["git", "checkout", "--detach", source_commits[label]], destination, environment)
        if git(destination, "status", "--porcelain", "--untracked-files=no"):
            raise ValueError("Fresh ownership checkout has tracked modifications")
    inputs = []
    for item in manifest["inputs"]:
        destination = input_destination(item, roots)
        if destination.exists():
            if digest(destination) != item["sha256"]:
                raise ValueError("Private input would replace different checkout content")
        else:
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(item["source"], destination)
        if digest(destination) != item["sha256"]:
            raise ValueError("Copied private input differs")
        inputs.append({"zone": item["destination_zone"], "path": item["destination"], "sha256": item["sha256"]})
    result_file = roots["fork"] / "build/oracle-telemetry/clean-census.json"
    result_file.parent.mkdir(parents=True, exist_ok=True)
    result = command([sys.executable, "-B", roots["fork"] / "tools/research/measure_renderer_scope.py",
                      "--parent-root", roots["parent"], "--fork-root", roots["fork"],
                      "--sdk31-root", args.sdk31_root.resolve(), "--bounded", "--output", result_file],
                     roots["parent"], environment)
    expected = json.loads(args.expected_census.read_bytes())["bounded_ownership"]["rows"]
    actual = json.loads(result_file.read_bytes())["bounded_ownership"]["rows"]
    if expected != actual:
        raise ValueError("Clean checkout ownership rows differ")
    states = {label: git(roots[label], "status", "--porcelain", "--untracked-files=no") for label in ("parent", "fork")}
    if any(states.values()):
        raise ValueError("Ownership reproduction changed a tracked checkout file")
    receipt = {"schema": "mkar.clean-ownership-reproduction.v1", "status": "PASS",
               "source_commits": source_commits, "private_manifest_sha256": digest(args.manifest),
               "ownership_rows": len(actual), "expected_rows_sha256": row_digest(expected),
               "actual_rows_sha256": row_digest(actual), "private_inputs": inputs,
               "checkout_tracked_status": states, "controls": controls,
               "stdout_sha256": hashlib.sha256(result.stdout).hexdigest(),
               "sharing": "Independent clean Git checkouts reuse read-only local Git object storage. Private inputs are separately copied and rehashed.",
               "Not verified": ["This checks ownership regeneration. It does not run a game or validate runtime behavior."],
               "Contradictions with earlier documents": ["The private listing is now explicitly supplied and authenticated for the clean checkout."],
               "Falsification attempts": controls["planted_rejections"]}
    (output / "RESULT.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps({k: receipt[k] for k in ("status", "source_commits", "ownership_rows", "actual_rows_sha256")}))


if __name__ == "__main__":
    main()
