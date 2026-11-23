"""Bounded original MIPS word candidates for standard-table publication;not semantic proof."""
from pathlib import Path
import hashlib,json,struct
R=Path(__file__).resolve().parent;ROOT=R.parents[2];PARENT=ROOT.parents[2]
image=(PARENT/"research/inputs/SLUS_214.10").read_bytes();digest=hashlib.sha256(image).hexdigest()
assert digest=="a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f"
ph=struct.unpack_from("<I",image,28)[0];size,count=struct.unpack_from("<HH",image,42)
loads=[struct.unpack_from("<8I",image,ph+i*size) for i in range(count)];rows=[]
for segment in loads:
 if segment[0]!=1 or not segment[6]&1:continue
 for delta in range(0,segment[4]-3,4):
  offset=segment[1]+delta;word=struct.unpack_from("<I",image,offset)[0]
  if word>>26 in [9,13,35,43] and word&0xffff in [0xf2d8,0xf2e8,0xf2a0,0xf2a4]:
   start=max(segment[1],offset-20);stop=min(segment[1]+segment[4],offset+24)
   rows.append(dict(vma=hex(segment[2]+delta),word=hex(word),file_offset=offset,
    window_vma=hex(segment[2]+start-segment[1]),window_file_offset=start,
    words_le32=[hex(v) for v in struct.unpack("<"+"I"*((stop-start)//4),image[start:stop])],
    segment=list(segment),scope="instruction/address candidates only"))
assert len(rows)<=128
with (R/"PS2_STANDARD_CANDIDATES.json").open("x",newline="\n") as f:
 json.dump(dict(image_sha256=digest,candidates=rows),f,indent=2);f.write("\n")
for row in rows:print(row["vma"],row["word"],row["window_vma"]," ".join(row["words_le32"]),flush=True)
