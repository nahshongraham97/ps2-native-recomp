"""Preserve the initial receipt while packaging exact unified-diff bytes."""
from pathlib import Path
import gzip
import hashlib
import json

RECORD = Path(__file__).resolve().parent
ROOT = RECORD.parents[2]
def sha(raw):
    return hashlib.sha256(raw).hexdigest()
initial = (RECORD / "DELIVERY.json").read_bytes()
delivery = json.loads(initial)
before = RECORD / "DELIVERY.before-patch-packaging.json"
with before.open("xb") as f:
    f.write(initial)
old_ignore = b"source-01/*.c\n"
ignore_name = (RECORD / ".gitignore").relative_to(ROOT).as_posix()
assert sha(old_ignore) == delivery["record_files"][ignore_name]
with (RECORD / "gitignore.before-packaging.gz").open("xb") as f:
    f.write(gzip.compress(old_ignore, mtime=0))
patch = RECORD / "parent-documents.patch"
patch_name = patch.relative_to(ROOT).as_posix()
assert sha(patch.read_bytes()) == delivery["record_files"][patch_name]
with patch.with_suffix(".patch.gz").open("xb") as f:
    f.write(gzip.compress(patch.read_bytes(), mtime=0))
delivery["private_raw_parent_patch_sha256"] = delivery["record_files"].pop(patch_name)
delivery["packaging_supersedes"] = dict(
    receipt=before.name, sha256=sha(initial),
    reason="Exact unified-diff bytes compressed; context whitespace is preserved, not normalized.")
for path in [before, RECORD / "gitignore.before-packaging.gz", patch.with_suffix(".patch.gz"),
             RECORD / ".gitignore", RECORD / "WORK.md", Path(__file__).resolve()]:
    delivery["record_files"][path.relative_to(ROOT).as_posix()] = sha(path.read_bytes())
with (RECORD / "DELIVERY.json").open("w", newline="\n") as f:
    json.dump(delivery, f, indent=2)
    f.write("\n")
print("Sealed exact patch bytes with preserved initial receipt; no runtime change")
