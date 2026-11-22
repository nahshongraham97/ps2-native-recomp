"""Preserve and normalize only this batch's edited CMake file to explicit LF."""
from pathlib import Path
import gzip
import hashlib
import json
ROOT = Path(__file__).resolve().parents[3]
RECORD = Path(__file__).resolve().parent
path = ROOT / "cmake/native_game.cmake"
raw = path.read_bytes()
with (RECORD / "native_game.before-LF.gz").open("xb") as f:
    f.write(gzip.compress(raw, mtime=0))
normalized = raw.replace(b"\r\n", b"\n")
assert b"\r" not in normalized
path.write_bytes(normalized)
with (RECORD / "CMAKE_LF_CORRECTION.json").open("x", newline="\n") as f:
    json.dump(dict(failed_check="checkpoint.py rejected CR in edited cmake/native_game.cmake before staging/build",
        before_sha256=hashlib.sha256(raw).hexdigest(), after_sha256=hashlib.sha256(normalized).hexdigest(),
        original_bytes_preserved="native_game.before-LF.gz", cxx_build_attempted=False), f, indent=2)
    f.write("\n")
