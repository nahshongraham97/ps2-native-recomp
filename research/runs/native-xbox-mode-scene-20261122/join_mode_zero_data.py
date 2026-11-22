"""Pin actual zero/one descriptors in each original image, not guessed column layouts."""
from pathlib import Path
import hashlib,json,struct
RECORD=Path(__file__).resolve().parent;ROOT=RECORD.parents[2];PARENT=ROOT.parents[2]
xbe=(PARENT/"private-inputs/xbox-mka-20261121/default.xbe").read_bytes()
elf=(PARENT/"private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/SLUS_214.10").read_bytes()
xid=json.loads((RECORD/"MODE_ANCHORS.json").read_text())
eid=json.loads((ROOT/"research/runs/native-mode-frame-startup-20261122/source-01.original-heads.json").read_text())
assert hashlib.sha256(xbe).hexdigest()==xid["xbe_sha256"]
assert hashlib.sha256(elf).hexdigest()==eid["elf_sha256"]
def mapped(image,segments,at,n,xbox):
 v,z,o=("virtual_address","file_size","file_offset") if xbox else ("p_vaddr","p_filesz","p_offset")
 matches=[s for s in segments if s[v]<=at and at+n<=s[v]+s[z]];assert len(matches)==1
 s=matches[0];offset=s[o]+at-s[v]
 return image[offset:offset+n],dict(vma=hex(at),file_offset=offset,length=n,section=s.get("name",s.get("index")))
out=[]
for platform,image,segments,base,xbox in [
 ("xbox",xbe,xid["sections"],0x56c938,True),("ps2",elf,eid["pt_load"],0x59f988,False)]:
 rows=[]
 for index in range(2):
  raw,m=mapped(image,segments,base+index*36,36,xbox);words=list(struct.unpack("<9I",raw))
  name,nm=mapped(image,segments,words[8],96,xbox);name=name.split(b"\0",1)[0].decode()
  row=dict(index=index,mapping=m,bytes_hex=raw.hex(),words=words,name=name,name_mapping=nm,
    list_pointer=hex(words[2]),list_count=words[3],policy=words[6],retained_descriptor=words[7])
  if words[3]:
   data,dm=mapped(image,segments,words[2],words[3]*12,xbox)
   row["list_data"]=dict(mapping=dm,bytes_hex=data.hex(),entries=[list(v) for v in struct.iter_unpack("<3I",data)])
  rows.append(row)
  print(platform,index,name,"count",words[3],"policy",words[6],"words",[hex(v) for v in words],flush=True)
 out.append(dict(platform=platform,rows=rows))
assert all(v["rows"][0]["list_count"]==2 and v["rows"][0]["policy"]==0 for v in out)
assert all(v["rows"][1]["list_count"]==4 and v["rows"][1]["policy"]==0 for v in out)
with (RECORD/"MODE_ZERO_DATA.json").open("x",newline="\n") as f:
 json.dump(dict(xbe_sha256=xid["xbe_sha256"],elf_sha256=eid["elf_sha256"],descriptors=out,
   scope="Original mode0 TWO-entry request list/policy0 and mode1 regression descriptors; live retirement semantics require callee proof"),f,indent=2);f.write("\n")
for v in out:
 print(v["platform"],"mode0 entries",v["rows"][0]["list_data"]["entries"],flush=True)
