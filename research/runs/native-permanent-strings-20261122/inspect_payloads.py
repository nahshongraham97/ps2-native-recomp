"""Derive bounded expected string-array values directly from original MKO ranges."""
from pathlib import Path
import hashlib
import json
import struct
RECORD=Path(__file__).resolve().parent
ROOT=RECORD.parents[2];PARENT=ROOT.parents[2]
descriptors=json.loads((RECORD/"DESCRIPTORS.json").read_text())
original=(PARENT/"private-inputs/native-ps2-loose-20261121/art/gameart.ssf").read_bytes()
assert hashlib.sha256(original).hexdigest()==descriptors["original_gameart_sha256"]
results=[]
for descriptor in descriptors["selected"]:
    b=original[descriptor["offset"]:descriptor["offset"]+descriptor["length"]]
    assert hashlib.sha256(b).hexdigest()==descriptor["payload_sha256"]
    header=struct.unpack_from("<8I",b)
    assert header[0]==0 and header[6]==1 and header[1]==0
    cursor=32+header[0]*4
    # No function records in these actual five images.
    cursor+=header[6]*4
    record=struct.unpack_from("<5I",b,cursor)
    refs=struct.unpack_from("<"+"I"*record[4],b,cursor+20)
    cursor+=20+record[4]*4
    names=b[cursor:cursor+header[2]];cursor+=header[2]
    strings=b[cursor:cursor+header[3]];cursor+=header[3]
    types=b[cursor:cursor+header[4]];cursor+=header[4]
    cells=struct.unpack_from("<"+"I"*(header[7]//4),b,cursor);cursor+=header[7]
    assert record[0]>0 and record[1]>0
    symbol=names[record[0]-1:].split(b"\0",1)[0]
    type_at=record[1]-1
    assert types[type_at+4]==1 and types[type_at+5]==4
    values=[]
    for i in range(record[3]):
        reference=cells[record[2]+i]
        assert 0<reference<=len(refs)
        at=refs[reference-1];end=strings.index(0,at)
        value=strings[at:end]
        values.append(dict(index=i,string_relative_offset=at,length=len(value),sha256=hashlib.sha256(value).hexdigest()))
    results.append(dict(name=descriptor["name"]["text"],module_offset=descriptor["offset"],module_length=len(b),
        payload_sha256=descriptor["payload_sha256"],header_words=list(header),variable_record=list(record),
        symbol=symbol.decode(),type_bytes_hex=types.hex(),reserved_first_cell=cells[0],
        decoded_extent=cursor,padding_bytes=len(b)-cursor,padding_sha256=hashlib.sha256(b[cursor:]).hexdigest(),
        first_examples=[strings[refs[cells[record[2]+i]-1]:].split(b"\0",1)[0].decode("latin1") for i in range(3)],
        values=values))
assert {r["symbol"] for r in results}=={"string_list"}
assert all(len(r["values"])==78 for r in results)
elf=(PARENT/"private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/SLUS_214.10").read_bytes()
segments=json.loads((RECORD/"source-01.original-heads.json").read_text())["pt_load"]
needle=b"string_list\0";offset=0;references=[]
while True:
    offset=elf.find(needle,offset)
    if offset<0:break
    matches=[s for s in segments if s["p_offset"]<=offset and offset+len(needle)<=s["p_offset"]+s["p_filesz"]]
    for s in matches:references.append(dict(file_offset=offset,vma=hex(s["p_vaddr"]+offset-s["p_offset"])))
    offset+=1
with (RECORD/"ORIGINAL_STRING_EXPECTATIONS.json").open("x",newline="\n") as f:
    json.dump(dict(modules=results,main_elf_symbol_literals=references,
        authority="Original data and verified2170E0/216D80 layout; not derived from new C++"),f,indent=2);f.write("\n")
print("Actual modules:",[(r["name"],r["symbol"],len(r["values"]),r["header_words"],r["type_bytes_hex"]) for r in results],flush=True)
print("Main string_list literals:",references,flush=True)
