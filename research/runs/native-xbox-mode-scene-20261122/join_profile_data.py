"""Pin original four-profile births and the source profile-binding projection."""
from pathlib import Path
import hashlib,json,struct
RECORD=Path(__file__).resolve().parent;ROOT=RECORD.parents[2];PARENT=ROOT.parents[2]
image=(PARENT/"private-inputs/xbox-mka-20261121/default.xbe").read_bytes()
identity=json.loads((RECORD/"MODE_ANCHORS.json").read_text())
assert hashlib.sha256(image).hexdigest()==identity["xbe_sha256"]
def mapped(at,n):
 matches=[s for s in identity["sections"] if s["virtual_address"]<=at and at+n<=s["virtual_address"]+s["file_size"]]
 assert len(matches)==1
 s=matches[0];offset=s["file_offset"]+at-s["virtual_address"]
 return image[offset:offset+n],dict(vma=hex(at),file_offset=offset,length=n,section=s["name"])
status,mapping=mapped(0x56162c,16);assert struct.unpack("<4I",status)==(8,8,8,8)
nodes=[]
for child in [0x731460,0x72de60,0x72a860,0x727260]:
 start=child-0x20;n=0x3600
 matches=[s for s in identity["sections"] if s["virtual_address"]+s["file_size"]<=start and start+n<=s["virtual_address"]+s["virtual_size"]]
 assert len(matches)==1
 nodes.append(dict(child=hex(child),metadata_begin=hex(start),total_source_bytes=n,
     source_child_bytes=0x35e0,section=matches[0]["name"],file_backed=False,loader_zero_initialized=True))
raw,projection=mapped(0xe5ad8,0xe5bb6-0xe5ad8)
pins={name:hashlib.sha256((RECORD/name).read_bytes()).hexdigest() for name in [
 "source-objects-03/001f98d0.asm","source-kart-profile-10/000e5a90.asm",
 "source-kart-profile-10.join.json","source-kart-06/000e68e0.asm"]}
with (RECORD/"PROFILE_OWNERSHIP_DATA.json").open("x",newline="\n") as f:
 json.dump(dict(xbe_sha256=identity["xbe_sha256"],nodes=nodes,
   player_status_mapping=mapping,player_status_bytes_hex=status.hex(),
   projection_mapping=projection,projection_bytes_hex=raw.hex(),evidence_sha256=pins,
   native_contract="Same Session nodes; mode-scoped access, live child/status references and call-time status==1 index order.",
   not_implemented=["whole E5A90","input ordinal prefix","display-query suffix","startup child+240 payload","loaded profile import","gameplay"],
   ps2_codec_sizes=dict(child=0x3c30,image=0x1e1e0,children_per_image=8),
   numerical_gate="OPEN",hd_gate="OPEN"),f,indent=2);f.write("\n")
print("Verified four loader-zero owners, four original status8 words and actual E5AD8..E5BB5 projection bytes",flush=True)
