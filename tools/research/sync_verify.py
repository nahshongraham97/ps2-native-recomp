"""Automated parent-fork ledger, inventory, and live doc synchronization verification."""
import hashlib
from pathlib import Path
import re
import shutil
import subprocess
import sys

SYNC_FILES = [
    "research/ledger/functions.csv",
    "research/ledger/xbox_functions.csv",
    "research/ledger/ps2_xbox_map.csv",
    "research/ledger/ported_function_evidence.json",
    "research/ledger/aot_coverage_inventory.json",
    "research/ledger/function_classification_inventory.json",
    "research/ledger/native_module_classification.json",
    "progress.svg",
    "tools/research/generate_ledgers.py",
    "tools/research/aot_inventory.py",
    "tools/research/compute_cross_isa_evidence.py",
    "tools/research/measure_xbox_capabilities.py",
    "research/runs/xbox-donor-20261121/PUBLICATION.json",
    "research/runs/test_full_existence_validation.py",
]

SYNC_DOCS = [
    "README.md",
    "TASK_BOARD.md",
    "PORT_STATUS.md",
]

_COMMIT_PIN_RE = re.compile(
    r"(\*\*Verified Fork Commit\*\*:\s*\[`)[0-9a-f]{8}(`\]\(https://github\.com/[^/]+/[^/]+/commit/)[0-9a-f]{40}(\))"
)

def find_roots(start_path: Path = None):
    p = (start_path or Path(__file__)).resolve()
    for parent in [p, *p.parents]:
        if parent.name in ("ps2-native-recomp", "ps2-native-recomp"):
            fork_root = parent
            if len(fork_root.parents) >= 3 and (fork_root.parents[2] / "research" / "runs" / "ghidra-function-coverage-20261118").exists():
                parent_repo = fork_root.parents[2]
                return fork_root, parent_repo
            return fork_root, None
        if parent.name == "armageddon-reimplementation":
            parent_repo = parent
            for cand in ("external/forks/ps2-native-recomp", "external/forks/ps2-native-recomp"):
                fork_root = parent_repo / cand
                if fork_root.exists():
                    return fork_root, parent_repo
            return parent_repo / "external/forks/ps2-native-recomp", parent_repo
    raise RuntimeError(f"Could not resolve parent and fork roots from {p}")

def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

def _normalize_doc(text: str, fork_root: Path):
    shas = []
    def _rep(m):
        full_sha = m.group(0).split("/commit/")[1].split(")")[0]
        shas.append(full_sha)
        return f"{m.group(1)}<FORK_SHA8>{m.group(2)}<FORK_SHA40>{m.group(3)}"
    norm = _COMMIT_PIN_RE.sub(_rep, text)
    for s in shas:
        subprocess.check_call(["git", "cat-file", "-e", s], cwd=fork_root, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return norm

def verify_sync(fork_root: Path, parent_repo: Path) -> dict:
    results = {}
    mismatches = []
    for rel in SYNC_FILES:
        f_fork = fork_root / rel
        f_parent = parent_repo / rel
        if not f_fork.exists():
            mismatches.append(f"{rel}: MISSING in fork ({f_fork})")
            continue
        if not f_parent.exists():
            mismatches.append(f"{rel}: MISSING in parent ({f_parent})")
            continue
        h_fork = sha(f_fork)
        h_parent = sha(f_parent)
        results[rel] = {"fork_sha": h_fork, "parent_sha": h_parent, "match": (h_fork == h_parent)}
        if h_fork != h_parent:
            mismatches.append(f"{rel}: MISMATCH (fork={h_fork[:12]}..., parent={h_parent[:12]}...)")

    for rel in SYNC_DOCS:
        f_fork = fork_root / rel
        f_parent = parent_repo / rel
        if not f_fork.exists() or not f_parent.exists():
            mismatches.append(f"{rel}: MISSING in parent or fork")
            continue
        t_fork = f_fork.read_text(encoding="utf-8")
        t_parent = f_parent.read_text(encoding="utf-8")
        try:
            _normalize_doc(t_fork, fork_root)
            _normalize_doc(t_parent, fork_root)
        except subprocess.CalledProcessError as exc:
            mismatches.append(f"{rel}: INVALID Verified Fork Commit SHA ({exc})")
            continue
        if f_fork.read_bytes() != f_parent.read_bytes():
            mismatches.append(f"{rel}: MISMATCH between parent and fork (including commit pins and line endings)")
    return {"results": results, "mismatches": mismatches}

def sync_from_fork_to_parent(fork_root: Path, parent_repo: Path):
    for rel in SYNC_FILES + SYNC_DOCS:
        src = fork_root / rel
        dst = parent_repo / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
    print("Synchronized all ledger and doc files from fork to parent.")

def sync_from_parent_to_fork(parent_repo: Path, fork_root: Path):
    for rel in SYNC_FILES + SYNC_DOCS:
        src = parent_repo / rel
        dst = fork_root / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
    print("Synchronized all ledger and doc files from parent to fork.")

def assert_synced():
    fork_root, parent_repo = find_roots()
    if parent_repo is None or not parent_repo.exists():
        print("SKIP: Standalone fork environment detected (no parent repository found). Parent-fork ledger sync check skipped.")
        return
    check = verify_sync(fork_root, parent_repo)
    if check["mismatches"]:
        msg = "Parent and fork ledger/doc files are NOT synchronized:\n" + "\n".join(check["mismatches"])
        raise AssertionError(msg)
    print(f"PASS: sync_verify checked {len(SYNC_FILES)} ledger files and {len(SYNC_DOCS)} docs between parent and fork.")

if __name__ == "__main__":
    fork_root, parent_repo = find_roots()
    if "--sync-to-fork" in sys.argv or "--sync" in sys.argv:
        sync_from_parent_to_fork(parent_repo, fork_root)
    check = verify_sync(fork_root, parent_repo)
    if check["mismatches"]:
        print("FAIL: Synchronization mismatch detected:")
        for m in check["mismatches"]:
            print(" ", m)
        sys.exit(1)
    print(f"PASS: All {len(SYNC_FILES)} ledger artifacts and {len(SYNC_DOCS)} synced docs match between parent and fork.")
