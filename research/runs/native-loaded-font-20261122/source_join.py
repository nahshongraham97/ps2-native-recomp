"""Join newly inspected private EE consumers to the pinned ELF's real PT_LOAD."""
from pathlib import Path
import hashlib
import json
import struct
RECORD = Path(__file__).resolve().parent
ROOT = RECORD.parents[2]
PARENT = ROOT.parents[2]
data = (PARENT / "private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/SLUS_214.10").read_bytes()
expected = "a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f"
assert hashlib.sha256(data).hexdigest()==expected
phoff=struct.unpack_from("<I",data,28)[0]
size,count=struct.unpack_from("<HH",data,42)
segments=[]
for i in range(count):
    kind,offset,vma,_,filesz,memsz,flags,align=struct.unpack_from("<8I",data,phoff+i*size)
    if kind==1: segments.append(dict(index=i,p_offset=offset,p_vaddr=vma,p_filesz=filesz,p_memsz=memsz))
def mapped(at,length):
    hits=[s for s in segments if s["p_vaddr"]<=at and at+length<=s["p_vaddr"]+s["p_filesz"]]
    assert len(hits)==1
    s=hits[0]; offset=s["p_offset"]+at-s["p_vaddr"]
    return data[offset:offset+length],dict(vma=hex(at),file_offset=offset,length=length,segment=s["index"],file_backed=True)
spans={}
for folder in ["source-consumers-01","source-filter-01"]:
    for path in sorted((RECORD/folder).glob("*.asm")):
        rows=[line.split("\t",2) for line in path.read_text().splitlines()]
        addresses=[int(r[0],16) for r in rows]
        assert addresses==list(range(addresses[0],addresses[-1]+4,4))
        raw,mapping=mapped(addresses[0],len(rows)*4)
        assert raw==bytes.fromhex("".join(r[1] for r in rows))
        spans[str(path.relative_to(RECORD))]=dict(mapping=mapping,words=len(rows),
            original_sha256=hashlib.sha256(raw).hexdigest(),listing_sha256=hashlib.sha256(path.read_bytes()).hexdigest())
high,_=mapped(0x19a65c,4)
low,_=mapped(0x19a660,4)
high_word=struct.unpack("<I",high)[0];low_word=struct.unpack("<I",low)[0]
immediate=low_word&0xffff
signed_immediate=immediate-0x10000 if immediate&0x8000 else immediate
key_vma=((high_word&0xffff)<<16)+signed_immediate
assert key_vma==0x5fce60
raw,mapping=mapped(key_vma,64)
key=raw[:raw.index(0)+1]
with (RECORD/"CONSUMER_SOURCE_JOIN-02.json").open("x",newline="\n") as f:
    json.dump(dict(elf_sha256=expected,pt_load=segments,spans=spans,
        resource_key=dict(mapping=mapping,bytes_hex=key.hex(),text=key[:-1].decode(),
            lui_le32=f"{high_word:08x}",addiu_le32=f"{low_word:08x}",signed_immediate=signed_immediate),
        supersedes=dict(record="CONSUMER_SOURCE_JOIN.json",
            reason="Its nine function joins remain valid, but its hard-coded60CE60 data citation ignored ADDIU signed immediate and is rejected."),
        source_policy=dict(filter1="1CFCA0 clears bits5..8 and ORs0",
            filter2="1CFCDC branch-likely taken delay ORs60; MMAG/MMIN=1",
            address3="1D3584..35C8 sets U/V clamp bits;1D38F0..3910 emits TEX1=14 and CLAMP=08",
            source_word3301="original16FC70 verified in preceding SOURCE_RECEIPT",
            no_runtime_register_model=True)),f,indent=2)
    f.write("\n")
print("Joined",len(spans),"actual EE spans; managed source key",repr(key))
