"""Private EE original-head/full-span extraction for the current connected batch."""
from pathlib import Path
import gzip
import hashlib
import json
import struct
import subprocess
import sys
import time
RECORD=Path(__file__).resolve().parent;ROOT=RECORD.parents[2];PARENT=ROOT.parents[2]
SESSION=PARENT/"local-research/ghidra-agent-sessions/mode-frame-20261122-6632aeac771042998a16bf70875625ef/SESSION.json"
ELF=PARENT/"private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/SLUS_214.10"
image=ELF.read_bytes();expected="a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f"
assert hashlib.sha256(image).hexdigest()==expected
phoff=struct.unpack_from("<I",image,28)[0];size,count=struct.unpack_from("<HH",image,42);segments=[]
for i in range(count):
    kind,offset,vma,_,filesz,memsz,flags,align=struct.unpack_from("<8I",image,phoff+i*size)
    if kind==1:segments.append(dict(index=i,p_offset=offset,p_vaddr=vma,p_filesz=filesz,p_memsz=memsz))
def mapped(at,n):
    matches=[s for s in segments if s["p_vaddr"]<=at and at+n<=s["p_vaddr"]+s["p_filesz"]]
    assert len(matches)==1
    s=matches[0];offset=s["p_offset"]+at-s["p_vaddr"]
    return image[offset:offset+n],dict(vma=hex(at),file_offset=offset,length=n,segment=s["index"])
name=sys.argv[1];addresses=sys.argv[2:];heads={}
for text in addresses:
    b,m=mapped(int(text,16),64);heads[text]=dict(mapping=m,bytes_hex=b.hex())
with (RECORD/(name+".original-heads.json")).open("x",newline="\n") as f:
    json.dump(dict(elf_sha256=expected,pt_load=segments,functions=heads),f,indent=2);f.write("\n")
argv=json.loads(SESSION.read_text())["projects"]["ps2"]["headless_argv"]
argv[argv.index("-scriptPath")+1]=str(RECORD.parent/"native-selected-resource-owner-20261122")
argv+=["-postScript","ExportSelectedOwner.java",str(RECORD/name),*addresses]
started=time.monotonic()
p=subprocess.run(argv,cwd=PARENT,capture_output=True,timeout=300,creationflags=subprocess.CREATE_NO_WINDOW)
raw=p.stdout+p.stderr
with (RECORD/(name+".log.gz")).open("xb") as f:f.write(gzip.compress(raw,mtime=0))
with (RECORD/(name+".command.json")).open("x",newline="\n") as f:
    json.dump(dict(argv=argv,exit=p.returncode,seconds=time.monotonic()-started,session=str(SESSION),
        session_sha256=hashlib.sha256(SESSION.read_bytes()).hexdigest()),f,indent=2);f.write("\n")
assert p.returncode==0
joined={}
for text in addresses:
    path=RECORD/name/(text+".asm");assert path.is_file(),raw.decode(errors="replace")[-2000:]
    rows=[r.split("\t",2) for r in path.read_text().splitlines()]
    at=int(rows[0][0],16);assert [int(r[0],16) for r in rows]==list(range(at,at+len(rows)*4,4))
    b,m=mapped(at,len(rows)*4);assert b==bytes.fromhex("".join(r[1] for r in rows))
    joined[text]=dict(mapping=m,words=len(rows),original_sha256=hashlib.sha256(b).hexdigest(),listing_sha256=hashlib.sha256(path.read_bytes()).hexdigest())
    c=path.with_suffix(".c")
    if c.exists():
        with c.with_suffix(".c.gz").open("xb") as f:f.write(gzip.compress(c.read_bytes(),mtime=0))
with (RECORD/(name+".join.json")).open("x",newline="\n") as f:
    json.dump(dict(elf_sha256=expected,functions=joined,scope="Byte/listing identity; semantic findings separate"),f,indent=2);f.write("\n")
print("Joined source spans",joined,flush=True)
