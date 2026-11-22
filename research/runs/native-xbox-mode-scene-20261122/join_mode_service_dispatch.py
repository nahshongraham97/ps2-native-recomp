"""Save the source switch target that selects policy zero; no Ghidra rerun."""
from pathlib import Path
import hashlib,json,struct
R=Path(__file__).resolve().parent;ROOT=R.parents[2];PARENT=ROOT.parents[2]
identity=json.loads((R/"MODE_ANCHORS.json").read_text())
image=(PARENT/"private-inputs/xbox-mka-20261121/default.xbe").read_bytes()
assert hashlib.sha256(image).hexdigest()==identity["xbe_sha256"]
at=0x23d154;n=16
sections=[s for s in identity["sections"] if s["virtual_address"]<=at and at+n<=s["virtual_address"]+s["file_size"]]
assert len(sections)==1
s=sections[0];offset=s["file_offset"]+at-s["virtual_address"];raw=image[offset:offset+n]
targets=list(struct.unpack("<4I",raw))
failure=json.loads((R/"MODE_SERVICE_DISPATCH_FAILURE_01.json").read_text())
assert failure["xbe_sha256"]==identity["xbe_sha256"] and failure["bytes_hex"]==raw.hex()
# The affected policy0 target was independently expected and matches the
# original bytes. Do not invent expectations for unadopted policies1..3.
assert targets[0]==0x23d0b1
result={"xbe_sha256":identity["xbe_sha256"],"mapping":{"vma":hex(at),"file_offset":offset,"length":n,"section":s["name"]},
 "bytes_hex":raw.hex(),"policy_targets":[hex(v) for v in targets],
 "policy_zero_target":"0x23d0b1","listing":"source-mode-01/0023d060.asm",
 "listing_sha256":hashlib.sha256((R/"source-mode-01/0023d060.asm").read_bytes()).hexdigest(),
 "scope":"actual indirect policy-zero target; policies1..3 not adopted",
 "supersedes":"MODE_SERVICE_DISPATCH_FAILURE_01.json",
 "correction":"Only assumed ordering of unadopted policies1..3 was wrong; original policy0 expectation unchanged",
 "production_code_change":False}
with (R/"MODE_SERVICE_DISPATCH.json").open("x",newline="\n") as f:json.dump(result,f,indent=2);f.write("\n")
print("SOURCE policy0 ->",result["policy_zero_target"],"original bytes",result["bytes_hex"],flush=True)
