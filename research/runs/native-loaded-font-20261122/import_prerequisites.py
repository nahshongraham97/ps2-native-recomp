"""Import only named immutable prerequisite source blobs; preserve prior bytes."""
from pathlib import Path
import gzip
import hashlib
import json
import subprocess

RECORD = Path(__file__).resolve().parent
ROOT = RECORD.parents[2]
def git(*args):
    return subprocess.check_output(["git", "-c", "core.autocrlf=false", *args], cwd=ROOT)
assert not git("status", "--porcelain", "-uno").strip()
groups = {
    "b6042e88dc072e369024e24ee353ec126cbfe737": [
        "runtime/include/mka/retail_profile_entry_decision.hpp",
        "runtime/include/mka/retail_saved_profile_child.hpp",
        "runtime/src/native/retail_profile_permission.cpp",
        "tests/test_retail_profile_entry_decision.cpp", "tests/test_retail_profile_permission.cpp"],
    "6382224a8a9ca7893cd35e6b9ac7427e6b39c687": [
        "runtime/include/mka/content_volume.hpp", "runtime/include/mka/native_file.hpp",
        "runtime/include/mka/native_file_read_job.hpp", "runtime/include/mka/retail_package.hpp",
        "runtime/src/native/native_file_read_job.cpp", "runtime/src/native/retail_package.cpp",
        "tests/test_native_file_read_job.cpp"],
    "69e0f3ce9c5e242fa8b539a7e217fc093fc01566": [
        "runtime/include/mka/movie_gl_device.hpp", "runtime/include/mka/native_gl_texture.hpp",
        "runtime/src/native/movie_gl_device.cpp", "tests/test_native_gl_texture.cpp"]
}
before = RECORD / "before-prerequisites"
before.mkdir(exist_ok=False)
receipt = {}
for commit, paths in groups.items():
    for name in paths:
        assert name not in receipt and "/audio" not in name and "/sound" not in name
        path = ROOT / name
        prior = path.read_bytes() if path.exists() else None
        raw = git("show", commit + ":" + name)
        assert b"\r" not in raw, name
        if prior is not None:
            (before / (name.replace("/", "__") + ".gz")).write_bytes(gzip.compress(prior, mtime=0))
        receipt[name] = dict(commit=commit, prior_sha256=hashlib.sha256(prior).hexdigest() if prior is not None else None,
                             imported_sha256=hashlib.sha256(raw).hexdigest())
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(raw)
with (RECORD / "PREREQUISITE_IMPORT.json").open("x", newline="\n") as f:
    json.dump(dict(base=git("rev-parse", "HEAD").decode().strip(), paths=receipt,
                   immutable_commit_blobs=True, primary_worktree_modified=False), f, indent=2)
    f.write("\n")
print("Imported", len(receipt), "explicit immutable source blobs; shared CMake still requires authored integration")
