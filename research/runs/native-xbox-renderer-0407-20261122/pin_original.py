"""Read-only bounded original data/reference candidates,never a semantic decoder."""
from pathlib import Path
import hashlib,json,struct,sys
R=Path(__file__).resolve().parent;ROOT=R.parents[2];PARENT=ROOT.parents[2]
identity=json.loads((ROOT/"research/runs/native-xbox-mode-scene-20261122/MODE_ANCHORS.json").read_text())
image=(PARENT/"private-inputs/xbox-mka-20261121/default.xbe").read_bytes()
assert hashlib.sha256(image).hexdigest()==identity["xbe_sha256"]
name,*queries=sys.argv[1:];rows=[]
def mapped(at,n):
 ss=[s for s in identity["sections"] if s["virtual_address"]<=at and at+n<=s["virtual_address"]+s["file_size"]]
 assert len(ss)==1;s=ss[0];o=s["file_offset"]+at-s["virtual_address"]
 return image[o:o+n],dict(vma=hex(at),length=n,file_offset=o,section=s["name"])
for query in queries:
 mode,value=query.split("=")
 if mode=="data":
  address,length=value.split(":");at=int(address,16);n=int(length);assert 0<n<=4096
  raw,m=mapped(at,n);row=dict(mode=mode,**m,bytes_hex=raw.hex());rows.append(row)
  print(hex(at),raw.hex(),flush=True)
 elif mode=="refs":
  needle=struct.pack("<I",int(value,16));text=next(s for s in identity["sections"] if s["name"]==".text")
  blob=image[text["file_offset"]:text["file_offset"]+text["file_size"]];cursor=0;candidates=[]
  while True:
   found=blob.find(needle,cursor)
   if found<0:break
   at=text["virtual_address"]+found;start=max(text["virtual_address"],at-8);stop=min(text["virtual_address"]+text["file_size"],at+12)
   raw,m=mapped(start,stop-start);candidates.append(dict(immediate_vma=hex(at),**m,bytes_hex=raw.hex()))
   cursor=found+1
  assert len(candidates)<=256
  rows.append(dict(mode=mode,reference=value,candidates=candidates,scope="raw immediate candidates,not proved instructions/callers"))
  for c in candidates:print(value,c["immediate_vma"],c["bytes_hex"],flush=True)
 else:raise ValueError(mode)
with (R/(name+".original-data.json")).open("x",newline="\n") as f:
 json.dump(dict(xbe_sha256=identity["xbe_sha256"],queries=rows),f,indent=2);f.write("\n")
