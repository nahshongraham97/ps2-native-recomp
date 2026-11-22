"""Pin bounded original Xbox data without reopening analysis or modifying the image."""
from pathlib import Path
import hashlib,json,sys
R=Path(__file__).resolve().parent;ROOT=R.parents[2];PARENT=ROOT.parents[2]
identity=json.loads((ROOT/"research/runs/native-xbox-mode-scene-20261122/MODE_ANCHORS.json").read_text())
image=(PARENT/"private-inputs/xbox-mka-20261121/default.xbe").read_bytes()
assert hashlib.sha256(image).hexdigest()==identity["xbe_sha256"]
name,*queries=sys.argv[1:];rows=[]
for query in queries:
 address,length=query.split(":");at=int(address,16);n=int(length);assert 0<n<=4096
 sections=[s for s in identity["sections"] if s["virtual_address"]<=at and at+n<=s["virtual_address"]+s["file_size"]]
 assert len(sections)==1;s=sections[0];o=s["file_offset"]+at-s["virtual_address"];raw=image[o:o+n]
 rows.append(dict(vma=hex(at),length=n,section=s["name"],file_offset=o,bytes_hex=raw.hex()))
 print(hex(at),raw.hex(),flush=True)
with (R/(name+".data.json")).open("x",newline="\n") as f:
 json.dump(dict(xbe_sha256=identity["xbe_sha256"],data=rows),f,indent=2);f.write("\n")
