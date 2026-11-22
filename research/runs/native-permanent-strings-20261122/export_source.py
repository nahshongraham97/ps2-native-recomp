"""Pin original instruction heads, then export complete bodies in a private EE session."""
from pathlib import Path
import gzip
import hashlib
import json
import struct
import subprocess
import sys
import time
RECORD=Path(__file__).resolve().parent
ROOT=RECORD.parents[2];PARENT=ROOT.parents[2]
SESSION=PARENT/"local-research/ghidra-agent-sessions/permanent-strings-20261122-7079e607dac14c5c9b3b55d13f2778bc/SESSION.json"
ELF=PARENT/"private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/SLUS_214.10"
raw=ELF.read_bytes();expected="a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f"
assert hashlib.sha256(raw).hexdigest()==expected
phoff=struct.unpack_from("<I",raw,28)[0];size,count=struct.unpack_from("<HH",raw,42)
segments=[]
for n in range(count):
    kind,offset,vma,_,filesz,memsz,flags,align=struct.unpack_from("<8I",raw,phoff+n*size)
    if kind==1:segments.append(dict(index=n,p_offset=offset,p_vaddr=vma,p_filesz=filesz,p_memsz=memsz))
def mapped(at,length):
    found=[s for s in segments if s["p_vaddr"]<=at and at+length<=s["p_vaddr"]+s["p_filesz"]]
    assert len(found)==1
    s=found[0];offset=s["p_offset"]+at-s["p_vaddr"]
    return raw[offset:offset+length],dict(vma=hex(at),file_offset=offset,length=length,segment=s["index"])
name=sys.argv[1];addresses=sys.argv[2:]
heads={}
for text in addresses:
    b,mapping=mapped(int(text,16),64);heads[text]=dict(mapping=mapping,bytes_hex=b.hex())
with (RECORD/(name+".original-heads.json")).open("x",newline="\n") as f:
    json.dump(dict(elf_sha256=expected,pt_load=segments,heads=heads),f,indent=2);f.write("\n")
argv=json.loads(SESSION.read_text())["projects"]["ps2"]["headless_argv"]
argv[argv.index("-scriptPath")+1]=str(RECORD.parent/"native-selected-resource-owner-20261122")
argv+=["-postScript","ExportSelectedOwner.java",str(RECORD/name),*addresses]
start=time.monotonic()
p=subprocess.run(argv,cwd=PARENT,capture_output=True,timeout=300,creationflags=subprocess.CREATE_NO_WINDOW)
log=p.stdout+p.stderr
with (RECORD/(name+".log.gz")).open("xb") as f:f.write(gzip.compress(log,mtime=0))
with (RECORD/(name+".command.json")).open("x",newline="\n") as f:
    json.dump(dict(argv=argv,exit=p.returncode,seconds=time.monotonic()-start,session=str(SESSION),
        session_sha256=hashlib.sha256(SESSION.read_bytes()).hexdigest(),log_sha256=hashlib.sha256(log).hexdigest()),f,indent=2);f.write("\n")
assert p.returncode==0
joined={}
for text in addresses:
    path=RECORD/name/(text+".asm")
    assert path.is_file(),log.decode(errors="replace")[-2000:]
    rows=[line.split("\t",2) for line in path.read_text().splitlines()]
    at=int(rows[0][0],16)
    assert [int(r[0],16) for r in rows]==list(range(at,at+len(rows)*4,4))
    b,mapping=mapped(at,len(rows)*4);assert b==bytes.fromhex("".join(r[1] for r in rows))
    joined[text]=dict(mapping=mapping,original_sha256=hashlib.sha256(b).hexdigest(),
                     listing_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),words=len(rows))
    c=path.with_suffix(".c")
    if c.exists():
        with c.with_suffix(".c.gz").open("xb") as f:f.write(gzip.compress(c.read_bytes(),mtime=0))
with (RECORD/(name+".join.json")).open("x",newline="\n") as f:
    json.dump(dict(elf_sha256=expected,functions=joined,scope="Byte/disassembly identity; behavior and production implementation separate"),f,indent=2);f.write("\n")
print("PASS private EE full-span original-word joins:",joined,flush=True)
