"""Pin0404 original routines/tables and native service bytes before implementation."""
from pathlib import Path
import gzip,hashlib,json,struct
R=Path(__file__).resolve().parent;ROOT=R.parents[2];PARENT=ROOT.parents[2]
identity=json.loads((ROOT/"research/runs/native-xbox-mode-scene-20261122/MODE_ANCHORS.json").read_text())
image=(PARENT/"private-inputs/xbox-mka-20261121/default.xbe").read_bytes()
assert hashlib.sha256(image).hexdigest()==identity["xbe_sha256"]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def mapped(at,n):
 sections=[s for s in identity["sections"] if s["virtual_address"]<=at and at+n<=s["virtual_address"]+s["file_size"]]
 assert len(sections)==1;s=sections[0];offset=s["file_offset"]+at-s["virtual_address"]
 return image[offset:offset+n],dict(vma=hex(at),length=n,file_offset=offset,section=s["name"])
selected=[("../native-xbox-scene-frame-20261122/source-module-callbacks-09","002b8de0"),
 ("../native-xbox-scene-frame-20261122/source-module-callbacks-09","002b8e30"),
 ("source-0404-02","002b95a0"),("source-0404-03","002b8fb0"),
 ("source-0404-04","002b8e80"),("source-0404-04","002b8f10"),("source-0404-04","00241140"),
 ("source-0404-05","002b90b0"),("source-0404-06","002b93c0"),("source-0404-02","002b94d0"),
 ("source-0404-06","002c0dd0"),("source-0404-07","0039b16e")]
functions={}
for folder,entry in selected:
 directory=(R/folder).resolve();join=Path(str(directory)+".join.json");row=json.loads(join.read_text())["functions"][entry]
 path=directory/(entry+".asm");assert sha(path)==row["listing_sha256"]
 bounds={r.split("\t")[0]:r.split("\t")[1] for r in (directory/"functions.tsv").read_text().splitlines()}
 cursor=int(entry,16)
 for span in row["original_spans"]:
  at=int(span["vma"],16);assert cursor==at
  raw,mapping=mapped(at,span["length"]);assert raw.hex()==span["bytes_hex"] and mapping["file_offset"]==span["file_offset"]
  cursor+=span["length"]
 assert cursor==int(bounds[entry],16)+1
 functions[entry]=dict(listing=path.relative_to(ROOT).as_posix(),listing_sha256=sha(path),join_sha256=sha(join),
  inclusive_range=[entry,bounds[entry]],instructions=row["instructions"],unlisted_gaps=[])
data={}
for at,n in [(0x5a66f0,8),(0x4e1458,3),(0x50b05c,7),(0x2b9094,16),(0x2b91f0,16),(0x2b94bc,16),(0x2b9584,16)]:
 raw,mapping=mapped(at,n);data[hex(at)]=dict(**mapping,bytes_hex=raw.hex())
tables={"0x2b9094":[0x2b8fec,0x2b9016,0x2b9027,0x2b903c],
 "0x2b91f0":[0x2b90ce,0x2b90ce,0x2b914a,0x2b91aa],
 "0x2b94bc":[0x2b93eb,0x2b93eb,0x2b943e,0x2b947e],
 "0x2b9584":[0x2b952f,0x2b94e8,0x2b94fe,0x2b951f]}
for address,expected in tables.items():assert list(struct.unpack("<4I",bytes.fromhex(data[address]["bytes_hex"])))==expected
assert data["0x4e1458"]["bytes_hex"]=="726200"
before={}
names=["runtime/include/mka/retail_renderer_core.hpp","runtime/src/native/retail_renderer_core.cpp","cmake/native_game.cmake",
 "runtime/include/mka/native_file.hpp","runtime/src/native/native_file.cpp",
 "runtime/include/mka/content_volume.hpp","runtime/src/native/content_volume.cpp"]
for n in names:
 raw=(ROOT/n).read_bytes();before[n]={"sha256":hashlib.sha256(raw).hexdigest(),"crlf_pairs":raw.count(b"\r\n")}
 with (R/(Path(n).name+".before-0404.gz")).open("xb") as f:f.write(gzip.compress(raw,mtime=0))
receipt=dict(status="SOURCE_DERIVED_0404_OWNED_STREAM_RESOURCE_READ_SCOPE",xbe_sha256=identity["xbe_sha256"],
 functions=functions,data=data,tables={k:[hex(v) for v in a] for k,a in tables.items()},native_before=before,
 selected_scope="real0404 native owner; immutable-memory and borrowed/owned native-file READ streams; read/forward skip/close",
 close_dependency="source filename close compares fclose result to zero then frees owned record even on failure",
 native_cleanup="checked retirement failure must remain visible through Core cleanup; no retry/replay or success stub",
 numerical_policy="host-wide lengths/cursors, no36-byte/16-element/4GiB model; invalid source32-wrap skip remains named unadopted",
 unadopted=["write/append","custom callbacks","caller-allocated source stream records","source wraparound skip","chunk/version helpers","full renderer/game"],
 owner_action="none",verification_scope="source-derived before runtime edits",acceptance="OPEN")
with (R/"SOURCE_0404.json").open("x",newline="\n") as f:json.dump(receipt,f,indent=2);f.write("\n")
print("PASS0404 original12 full spans, actual dispatch/mode data and native before snapshots",flush=True)
