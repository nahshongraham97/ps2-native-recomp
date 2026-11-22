"""Find exact GP-relative source instructions for the2460B0 published aliases."""
from pathlib import Path
import hashlib
import json
import struct
RECORD=Path(__file__).resolve().parent;ROOT=RECORD.parents[2];PARENT=ROOT.parents[2]
e=(PARENT/"private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/SLUS_214.10").read_bytes()
assert hashlib.sha256(e).hexdigest()=="a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f"
shoff=struct.unpack_from("<I",e,32)[0];entsize,count=struct.unpack_from("<HH",e,46)
targets={0xa0cc,0xa0d0,0xa0f0,0xa0f4,0xb030,0xb034}
hits=[]
for i in range(count):
    fields=struct.unpack_from("<10I",e,shoff+i*entsize)
    _,kind,flags,vma,offset,size,*_=fields
    if not flags&4:continue
    for at in range(0,size-size%4,4):
        word=struct.unpack_from("<I",e,offset+at)[0]
        if ((word>>21)&31)==28 and (word&0xffff) in targets:
            hits.append(dict(pc=f"{vma+at:08x}",word_le32=f"{word:08x}",signed_immediate=(word&0xffff)-65536))
with (RECORD/"ALIAS_USERS.json").open("x",newline="\n") as f:json.dump(hits,f,indent=2);f.write("\n")
print(hits,flush=True)
