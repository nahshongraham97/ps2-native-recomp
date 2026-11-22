"""Reuse the bounded streaming checker; record all current native source identities in this batch."""
from pathlib import Path
import hashlib
base=Path(__file__).resolve().parent.parent/"native-xbox-renderer-0405-20261122/run_check.py"
print("REUSE bounded checker",hashlib.sha256(base.read_bytes()).hexdigest(),flush=True)
exec(compile(base.read_text().replace("0405","0406"),str(base),"exec"),{"__name__":"__main__","__file__":__file__})
