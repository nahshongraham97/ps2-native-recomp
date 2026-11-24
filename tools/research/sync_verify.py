"""Automated parent-fork ledger and generator synchronization verification."""
import hashlib
from pathlib import Path
import shutil
import sys

SYNC_FILES = [
    "research/ledger/functions.csv",
    "research/ledger/xbox_functions.csv",
    "research/ledger/ps2_xbox_map.csv",
    "research/ledger/ported_function_evidence.json",
    "progress.svg",
    "tools/research/generate_ledgers.py",
]

SYNC_DOCS = [
    "README.md",
    "TASK_BOARD.md",
]

def find_roots(start_path: Path = None):
    p = (start_path or Path(__file__)).resolve()
    # Check if we are inside fork retail-main-22
    for parent in [p, *p.parents]:
        if parent.name == "retail-main-22":
            fork_root = parent
            # Parent repository is at armageddon-reimplementation
            parent_repo = fork_root.parents[2]
            return fork_root, parent_repo
        if parent.name == "armageddon-reimplementation":
            parent_repo = parent
            fork_root = parent_repo / "external/forks/retail-main-22"
            return fork_root, parent_repo
    raise RuntimeError(f"Could not resolve parent and fork roots from {p}")

def sha(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()

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
        results[rel] = {
            "fork_sha": h_fork,
            "parent_sha": h_parent,
            "match": (h_fork == h_parent)
        }
        if h_fork != h_parent:
            mismatches.append(f"{rel}: MISMATCH (fork={h_fork[:12]}..., parent={h_parent[:12]}...)")
    return {"results": results, "mismatches": mismatches}

def sync_from_fork_to_parent(fork_root: Path, parent_repo: Path):
    for rel in SYNC_FILES:
        src = fork_root / rel
        dst = parent_repo / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
    print("Synchronized all ledger files from fork (authoritative) to parent.")

def sync_from_parent_to_fork(parent_repo: Path, fork_root: Path):
    for rel in SYNC_FILES:
        src = parent_repo / rel
        dst = fork_root / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
    print("Synchronized all ledger files from parent to fork.")

def assert_synced():
    fork_root, parent_repo = find_roots()
    check = verify_sync(fork_root, parent_repo)
    if check["mismatches"]:
        msg = "Parent and fork ledger files are NOT synchronized:\n" + "\n".join(check["mismatches"])
        raise AssertionError(msg)
    print(f"PASS: sync_verify checked {len(SYNC_FILES)} ledger files between parent and fork (100% byte-for-byte match).")

if __name__ == "__main__":
    fork_root, parent_repo = find_roots()
    if "--sync-to-fork" in sys.argv:
        sync_from_parent_to_fork(parent_repo, fork_root)
    elif "--sync" in sys.argv:
        sync_from_parent_to_fork(parent_repo, fork_root)
    check = verify_sync(fork_root, parent_repo)
    if check["mismatches"]:
        print("FAIL: Ledger synchronization mismatch detected:")
        for m in check["mismatches"]:
            print("  ", m)
        sys.exit(1)
    else:
        print(f"PASS: All {len(SYNC_FILES)} ledger artifacts are identical between parent and fork.")
        sys.exit(0)
