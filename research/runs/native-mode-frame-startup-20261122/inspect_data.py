"""Pin the mode selector row and its actual signed-immediate-derived table."""
from pathlib import Path
import hashlib
import json
import struct
RECORD=Path(__file__).resolve().parent;ROOT=RECORD.parents[2];PARENT=ROOT.parents[2]
e=(PARENT/"private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/SLUS_214.10").read_bytes()
identity=json.loads((RECORD/"source-01.original-heads.json").read_text())
assert hashlib.sha256(e).hexdigest()==identity["elf_sha256"]
segments=identity["pt_load"]
def mapped(at,n):
    found=[s for s in segments if s["p_vaddr"]<=at and at+n<=s["p_vaddr"]+s["p_filesz"]];assert len(found)==1
    s=found[0];offset=s["p_offset"]+at-s["p_vaddr"]
    return e[offset:offset+n],dict(vma=hex(at),file_offset=offset,length=n,segment=s["index"])
def u32(at):return struct.unpack("<I",mapped(at,4)[0])[0]
high=u32(0x12a514);low=u32(0x12a51c);imm=low&0xffff;signed=imm-65536 if imm&0x8000 else imm
table=((high&0xffff)<<16)+signed
raw,mapping=mapped(table,9*36)
rows=[list(struct.unpack_from("<9i",raw,i*36)) for i in range(9)]
with (RECORD/"MODE_DATA.json").open("x",newline="\n") as f:
    json.dump(dict(elf_sha256=identity["elf_sha256"],lui_le32=f"{high:08x}",addiu_le32=f"{low:08x}",
        signed_immediate=signed,table_mapping=mapping,table_bytes_hex=raw.hex(),rows_signed=rows),f,indent=2);f.write("\n")
print("Source selector table",hex(table),rows,flush=True)
