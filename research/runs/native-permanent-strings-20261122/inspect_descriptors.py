"""Read source-proved descriptor tables and pin the corresponding loose-file ranges."""
from pathlib import Path
import hashlib
import json
import struct
RECORD=Path(__file__).resolve().parent
ROOT=RECORD.parents[2];PARENT=ROOT.parents[2]
elf=(PARENT/"private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/SLUS_214.10").read_bytes()
identity=json.loads((RECORD/"source-01.original-heads.json").read_text())
assert hashlib.sha256(elf).hexdigest()==identity["elf_sha256"]
segments=identity["pt_load"]
def mapped(at,n):
    matches=[s for s in segments if s["p_vaddr"]<=at and at+n<=s["p_vaddr"]+s["p_filesz"]]
    assert len(matches)==1,(hex(at),n)
    s=matches[0];offset=s["p_offset"]+at-s["p_vaddr"]
    return elf[offset:offset+n],dict(vma=hex(at),file_offset=offset,length=n,segment=s["index"])
def u32(at):return struct.unpack("<I",mapped(at,4)[0])[0]
def string(at):
    b,m=mapped(at,256);b=b[:b.index(0)+1]
    return dict(text=b[:-1].decode("ascii"),bytes_hex=b.hex(),mapping={**m,"length":len(b)})
root=u32(0x5b472c)
rows=[]
for i in range(1024):
    raw,mapping=mapped(root+12+i*12,12)
    descriptor,offset,length=struct.unpack("<3I",raw)
    if not descriptor:break
    d,dm=mapped(descriptor,16)
    words=struct.unpack("<4I",d)
    rows.append(dict(index=i,row_mapping=mapping,row_bytes=raw.hex(),
        descriptor_mapping=dm,descriptor_words=[hex(w) for w in words],
        name=string(words[0]),offset=offset,length=length))
else:raise AssertionError("Unterminated source table")
selected=[r for r in rows if r["name"]["text"].startswith("permanent_strings_")]
assert len(selected)==5,[(r["name"]["text"],r["offset"],r["length"]) for r in rows]
asset=PARENT/"private-inputs/native-ps2-loose-20261121/art/gameart.ssf"
data=asset.read_bytes()
assert hashlib.sha256(data).hexdigest()=="9900120ff8782b16e0d73ee885c8f4df6c82545b915dd10105e6694a03658a39"
for r in selected:
    at,n=r["offset"],r["length"]
    assert at+n<=len(data)
    b=data[at:at+n]
    r["payload_sha256"]=hashlib.sha256(b).hexdigest()
    r["first_64_bytes_hex"]=b[:64].hex()
with (RECORD/"DESCRIPTORS.json").open("x",newline="\n") as f:
    json.dump(dict(elf_sha256=identity["elf_sha256"],root_vma=hex(root),
        root_bytes=mapped(root,12)[0].hex(),selected=selected,
        original_gameart_sha256=hashlib.sha256(data).hexdigest(),all_source_names=[r["name"]["text"] for r in rows]),
        f,indent=2);f.write("\n")
print(json.dumps(selected,indent=2),flush=True)
