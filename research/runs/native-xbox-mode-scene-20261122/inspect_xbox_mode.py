"""Use the pinned Xbox image's OWN sections to find bounded mode-table anchors."""
from pathlib import Path
import hashlib,json,struct
RECORD=Path(__file__).resolve().parent;ROOT=RECORD.parents[2];PARENT=ROOT.parents[2]
def sha(raw):return hashlib.sha256(raw).hexdigest()
inventory_path=PARENT/"research/runs/xbox-donor-20261121/original-sound-inventory.json"
inventory=json.loads(inventory_path.read_text())
xbe=(PARENT/"private-inputs/xbox-mka-20261121/default.xbe").read_bytes()
assert sha(xbe)==inventory["source_sha256"]=="78f99fca447fddc5c5116c09d27e6f1c04af8572dace94f559966588c6f5e393"
assert xbe[:4]==b"XBEH"
base=struct.unpack_from("<I",xbe,0x104)[0]
count,table=struct.unpack_from("<II",xbe,0x11c)
sections=[]
for i in range(count):
    flags,at,size,offset,filesz,nameptr=struct.unpack_from("<6I",xbe,table-base+i*56)
    name=xbe[nameptr-base:xbe.index(b"\0",nameptr-base)].decode()
    sections.append(dict(name=name,flags=flags,virtual_address=at,virtual_size=size,file_offset=offset,file_size=filesz))
assert sections==inventory["sections"]
elf=(PARENT/"private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/SLUS_214.10").read_bytes()
prior=ROOT/"research/runs/native-mode-frame-startup-20261122"
identity=json.loads((prior/"source-01.original-heads.json").read_text())
assert sha(elf)==identity["elf_sha256"]
mode=json.loads((prior/"MODE_DATA.json").read_text())
def ps2_string(at):
    matches=[s for s in identity["pt_load"] if s["p_vaddr"]<=at<s["p_vaddr"]+s["p_filesz"]]
    assert len(matches)==1
    s=matches[0];offset=s["p_offset"]+at-s["p_vaddr"];end=elf.index(b"\0",offset)
    assert end-offset<256
    return elf[offset:end]
def occurrences(raw,needle):
    pos=0;result=[]
    while True:
        pos=raw.find(needle,pos)
        if pos<0:return result
        result.append(pos);pos+=1
def xbe_map(offset,n):
    matches=[s for s in sections if s["file_offset"]<=offset and offset+n<=s["file_offset"]+s["file_size"]]
    assert len(matches)==1,(offset,n)
    s=matches[0];return s["virtual_address"]+offset-s["file_offset"],s
anchors=[]
for index,row in enumerate(mode["rows_signed"]):
    name=ps2_string(row[2]);found=[]
    for offset in occurrences(xbe,name+b"\0"):
        at,s=xbe_map(offset,len(name)+1);refs=[]
        for ref in occurrences(xbe,struct.pack("<I",at)):
            refat,rs=xbe_map(ref,4)
            start=max(rs["file_offset"],ref-32);end=min(rs["file_offset"]+rs["file_size"],ref+64)
            refs.append(dict(address=hex(refat),section=rs["name"],executable=bool(rs["flags"]&4),
                file_offset=ref,context_file_offset=start,context_bytes_hex=xbe[start:end].hex()))
        found.append(dict(address=hex(at),file_offset=offset,section=s["name"],references=refs))
    anchors.append(dict(source_mode=index,name=name.decode("ascii"),ps2_name_address=hex(row[2]),xbox=found))
with (RECORD/"MODE_ANCHORS.json").open("x",newline="\n") as f:
    json.dump(dict(xbe_sha256=sha(xbe),elf_sha256=sha(elf),sections=sections,
        prior_inventory_sha256=sha(inventory_path.read_bytes()),anchors=anchors,
        scope="Original data/byte references only; no function equivalence assumed"),f,indent=2);f.write("\n")
for row in anchors:
    print(row["source_mode"],row["name"],[(v["address"],[(r["address"],r["section"]) for r in v["references"]]) for v in row["xbox"]],flush=True)
