"""Freeze original0403/base-frame authority and native pre-edit source identities."""
from pathlib import Path
import gzip,hashlib,json
R=Path(__file__).resolve().parent;ROOT=R.parents[2];PARENT=ROOT.parents[2]
identity=json.loads((ROOT/"research/runs/native-xbox-mode-scene-20261122/MODE_ANCHORS.json").read_text())
image=(PARENT/"private-inputs/xbox-mka-20261121/default.xbe").read_bytes()
assert hashlib.sha256(image).hexdigest()==identity["xbe_sha256"]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def mapped(at,n):
 s=[s for s in identity["sections"] if s["virtual_address"]<=at and at+n<=s["virtual_address"]+s["file_size"]]
 assert len(s)==1;s=s[0];offset=s["file_offset"]+at-s["virtual_address"]
 return image[offset:offset+n],dict(vma=hex(at),length=n,file_offset=offset,section=s["name"])
selected=[
 ("../native-xbox-scene-frame-20261122/source-module-callbacks-09","002bef60"),
 ("../native-xbox-scene-frame-20261122/source-module-callbacks-09","002befc0"),
 ("source-0403-02","002bf170"),("source-0403-03","002bf090"),
 ("source-0403-02","002bf230"),("source-0403-02","002bf2c0"),
 ("source-0403-03","002bf7d0"),("source-0403-02","002bf840"),
 ("source-0403-04","002bf350"),("source-0403-05","002bf000"),
 ("source-0403-05","002bf8e0"),("source-0403-04","002c2b90"),("source-0403-04","002c2bf0")]
functions={}
gap_expected={0x2bf817:("8da424000000008bff","LEA ESP,[ESP+0]; MOV EDI,EDI; skipped by unconditional JMP; no register/flag change"),
 0x2bf37a:("8d9b00000000","LEA EBX,[EBX+0]; skipped by unconditional JMP; no register/flag change")}
for folder,entry in selected:
 directory=(R/folder).resolve();join=Path(str(directory)+".join.json")
 row=json.loads(join.read_text())["functions"][entry];path=directory/(entry+".asm")
 assert sha(path)==row["listing_sha256"]
 bounds={line.split("\t")[0]:line.split("\t")[1] for line in (directory/"functions.tsv").read_text().splitlines()}
 cursor=int(entry,16);gaps=[]
 for span in row["original_spans"]:
  at=int(span["vma"],16)
  if cursor!=at:
   raw,mapping=mapped(cursor,at-cursor);expected,semantics=gap_expected[cursor];assert raw.hex()==expected
   gaps.append(dict(**mapping,bytes_hex=raw.hex(),decoded=semantics))
  raw,mapping=mapped(at,span["length"]);assert raw.hex()==span["bytes_hex"] and mapping["file_offset"]==span["file_offset"]
  cursor=at+span["length"]
 assert cursor==int(bounds[entry],16)+1
 functions[entry]=dict(listing=str(path.relative_to(ROOT)),listing_sha256=sha(path),
  join_sha256=sha(join),inclusive_range=[entry,bounds[entry]],instructions=row["instructions"],decoded_unlisted_gaps=gaps)
data={}
for at,n in [(0x4d7b54,4),(0x5a6768,32)]:
 raw,mapping=mapped(at,n);data[hex(at)]=dict(**mapping,bytes_hex=raw.hex())
assert data["0x4d7b54"]["bytes_hex"]=="0000803f"
assert bytes.fromhex(data["0x5a6768"]["bytes_hex"])[16:24]==b"\0"*8
before={}
for n in ["runtime/include/mka/retail_renderer_core.hpp","cmake/native_game.cmake"]:
 raw=(ROOT/n).read_bytes();assert b"\r" not in raw;before[n]=hashlib.sha256(raw).hexdigest()
 with (R/(Path(n).name+".before-0403.gz")).open("xb") as f:f.write(gzip.compress(raw,mtime=0))
receipt=dict(status="SOURCE_DERIVED_0403_RESOURCE_AND_BASE_FRAME_LIFECYCLE",xbe_sha256=identity["xbe_sha256"],
 functions=functions,data=data,native_before=before,
 selected_scope="real0403 owner; base-frame/initial-empty-registry allocation; hierarchy attach/detach/root destroy/leaf destroy; dirty root queue",
 original_errors="allocation NULL leaves module/queue unpublished; source FrameCreate ignores plugin-constructor return",
 native_limits="host-owned typed objects and native handles; no176-byte layout/50-entry block/offset/capacity model",
 unadopted=["nonempty frame plugin callbacks","clone/alias rewrites","single destroy with children","attached hierarchy destroy",
  "matrix transform writers","generic/game synchronization","attached objects","GPU/renderer acceptance"],
 owner_action="none",verification_scope="source-derived before runtime edit",acceptance="OPEN")
with (R/"SOURCE_0403.json").open("x",newline="\n") as f:json.dump(receipt,f,indent=2);f.write("\n")
print("PASS0403 original spans, decoded skipped words, identity/empty-registry data and native before snapshots",flush=True)
