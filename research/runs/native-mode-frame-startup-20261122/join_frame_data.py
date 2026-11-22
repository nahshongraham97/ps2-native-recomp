"""Prove the initializer's source backing extents are loader-zero memory, not input file bytes."""
from pathlib import Path
import hashlib,json,struct
RECORD=Path(__file__).resolve().parent;ROOT=RECORD.parents[2];PARENT=ROOT.parents[2]
image=(PARENT/"private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/SLUS_214.10").read_bytes()
identity=json.loads((RECORD/"source-01.original-heads.json").read_text())
assert hashlib.sha256(image).hexdigest()==identity["elf_sha256"]
segments=identity["pt_load"]
def word(at):
    matches=[s for s in segments if s["p_vaddr"]<=at and at+4<=s["p_vaddr"]+s["p_filesz"]];assert len(matches)==1
    s=matches[0];return struct.unpack_from("<I",image,s["p_offset"]+at-s["p_vaddr"])[0]
def signed_imm(w):
    value=w&65535;return value-65536 if value&32768 else value
capacity=((word(0x2e8e5c)&65535)<<16)|(word(0x2e8e64)&65535)
first=(((word(0x2e8e70)&65535)<<16)+signed_imm(word(0x2e8e74))+15)&~15
second=(((word(0x2e8e9c)&65535)<<16)+signed_imm(word(0x2e8ea4))+15)&~15
assert capacity==0x249f0 and first==0x6d0950 and second==0x73e730
ranges=[]
for base,count in [(first,3),(second,2)]:
    length=capacity*count
    matches=[s for s in segments if s["p_vaddr"]+s["p_filesz"]<=base and base+length<=s["p_vaddr"]+s["p_memsz"]]
    assert len(matches)==1
    s=matches[0];ranges.append(dict(source_base=hex(base),source_length=length,initial_windows=count,
        segment=s["index"],file_backed=False,loader_zero_initialized=True))
with (RECORD/"FRAME_DATA.json").open("x",newline="\n") as f:
    json.dump(dict(elf_sha256=identity["elf_sha256"],source_block_byte_hint=capacity,ranges=ranges,
        native_mapping="Actual existing Permanent heap; source sizes/counts are warm hints, never ceilings.",
        source_addresses_in_runtime=False),f,indent=2);f.write("\n")
print("Verified source loader-zero backing, source warm hints and signed immediates",flush=True)
