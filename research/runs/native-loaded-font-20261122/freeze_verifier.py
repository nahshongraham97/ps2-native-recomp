"""Preserve the exact Owner02 verifier before the final connected ABI check plan."""
from pathlib import Path
import gzip
import hashlib
import json
RECORD=Path(__file__).resolve().parent
raw=(RECORD/"verify_owner.py").read_bytes()
receipt=json.loads((RECORD/"validation-owner-02/VERIFICATION.json").read_text())
assert hashlib.sha256(raw).hexdigest()==receipt["verification_tool_sha256"]
with (RECORD/"verify_owner.before-final-plan.py.gz").open("xb") as f:
    f.write(gzip.compress(raw,mtime=0))
