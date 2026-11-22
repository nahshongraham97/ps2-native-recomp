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
expected=[0x23d0b1,0x23d0ce,0x23d0f4,0x23d10e]
print("ACTUAL targets",[hex(v) for v in targets],"expected hypothesis",[hex(v) for v in expected],flush=True)
if targets!=expected:
 with (R/"MODE_SERVICE_DISPATCH_FAILURE_01.json").open("x",newline="\n") as f:
  json.dump({"status":"DISPATCH_HYPOTHESIS_DISPROVED","xbe_sha256":identity["xbe_sha256"],
   "vma":hex(at),"file_offset":offset,"bytes_hex":raw.hex(),"actual_targets":[hex(v) for v in targets],
   "disproved_targets":[hex(v) for v in expected],"production_change":"NONE; awaiting semantic resolution"},f,indent=2);f.write("\n")
assert targets==expected
result={"xbe_sha256":identity["xbe_sha256"],"mapping":{"vma":hex(at),"file_offset":offset,"length":n,"section":s["name"]},
 "bytes_hex":raw.hex(),"policy_targets":[hex(v) for v in targets],
 "policy_zero_target":"0x23d0b1","listing":"source-mode-01/0023d060.asm",
 "listing_sha256":hashlib.sha256((R/"source-mode-01/0023d060.asm").read_bytes()).hexdigest(),
 "scope":"actual indirect policy-zero target; policies1..3 not adopted"}
with (R/"MODE_SERVICE_DISPATCH.json").open("x",newline="\n") as f:json.dump(result,f,indent=2);f.write("\n")
print("SOURCE policy0 ->",result["policy_zero_target"],"original bytes",result["bytes_hex"],flush=True)
