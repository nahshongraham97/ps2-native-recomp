"""Read-only bounded original-word search for matching image allocation tags."""
from pathlib import Path
import hashlib,json,struct
R=Path(__file__).resolve().parent;ROOT=R.parents[2];PARENT=ROOT.parents[2]
path=PARENT/"research/inputs/SLUS_214.10";image=path.read_bytes()
digest=hashlib.sha256(image).hexdigest()
assert digest=="a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f"
ph=struct.unpack_from("<I",image,28)[0];ps,pn=struct.unpack_from("<HH",image,42)
loads=[struct.unpack_from("<8I",image,ph+i*ps) for i in range(pn)]
def mapped(at,n):
 ss=[s for s in loads if s[0]==1 and s[2]<=at and at+n<=s[2]+s[4]]
 assert len(ss)==1;s=ss[0];o=s[1]+at-s[2]
 return image[o:o+n],o
begin,end=0x1a7000,0x1a8504
raw,offset=mapped(begin,end-begin);words=struct.unpack("<"+"I"*(len(raw)//4),raw);candidates=[]
for i,w in enumerate(words):
 if w>>26==13 and w&0xffff==0x18:
  at=begin+i*4;start=max(begin,at-48);stop=min(end,at+32);b,o=mapped(start,stop-start)
  candidates.append(dict(vma=hex(at),ori_rt=(w>>16)&31,ori_rs=(w>>21)&31,
   window_vma=hex(start),window_file_offset=o,words_le32=[hex(x) for x in struct.unpack("<"+"I"*(len(b)//4),b)]))
value=dict(status="BOUNDED_ORIGINAL_WORD_CANDIDATES_NOT_SEMANTIC_PROOF",image_sha256=digest,
 begin=hex(begin),end_exclusive=hex(end),file_offset=offset,candidates=candidates)
with (R/"PS2_ALLOCATION_CANDIDATES.json").open("x",newline="\n") as f:json.dump(value,f,indent=2);f.write("\n")
print("PASS0406 bounded original-word candidates",flush=True)
for c in candidates:print(c["vma"],c["window_vma"]," ".join(c["words_le32"]),flush=True)
