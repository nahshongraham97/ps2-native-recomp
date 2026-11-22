"""Freeze source-derived registration inputs and exact pre-edit native bytes."""
from pathlib import Path
import gzip,hashlib,json
R=Path(__file__).resolve().parent;ROOT=R.parents[2]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
functions={}
for folder,entry in [("source-module-callbacks-09","002c84d0"),
                     ("source-raster-registration-10","002b9cf0"),
                     ("source-raster-consumers-11","002b9e50"),
                     ("source-raster-consumers-11","002b9c70")]:
 joined=json.loads((R/(folder+".join.json")).read_text())
 row=joined["functions"][entry];path=R/folder/(entry+".asm")
 assert row["listing_sha256"]==sha(path)
 cursor=int(entry,16)
 for span in row["original_spans"]:
  assert int(span["vma"],16)==cursor
  cursor+=span["length"]
 functions[entry]={"listing":folder+"/"+entry+".asm","listing_sha256":sha(path),
  "join_sha256":sha(R/(folder+".join.json")),"inclusive_range":[entry,hex(cursor-1)],
  "instructions":row["instructions"],"unlisted_byte_gaps":[]}
before={}
for name in ["runtime/include/mka/retail_renderer_core.hpp","runtime/src/native/retail_renderer_core.cpp","cmake/native_game.cmake"]:
 data=(ROOT/name).read_bytes();assert b"\r" not in data;before[name]=hashlib.sha256(data).hexdigest()
 with (R/(Path(name).name+".before-registration.gz")).open("xb") as f:f.write(gzip.compress(data,mtime=0))
receipt={"status":"SOURCE_DERIVED_SELECTED_XBOX_REGISTRATION","xbe_sha256":joined["xbe_sha256"],
 "functions":functions,"native_before":before,
 "existing_registry_evidence":"research/runs/parallel-renderer-engine-init-20261122/XBOX_WORD_JOIN.json",
 "existing_registry_evidence_sha256":sha(ROOT/"research/runs/parallel-renderer-engine-init-20261122/XBOX_WORD_JOIN.json"),
 "selected_behavior":{"object_family":"raster","id":0x40c,"source_payload_bytes":24,
  "constructor":None,"destructor":None,"copy":None,"success":"signed32 registration result >= 0",
  "consumer_order":"2B9E50 device creation before registry construction; 2B9C70 registry destruction before device destruction"},
 "native_policy":"real typed registration identity owned by existing Core/heap; no console offset/payload or GPU-success stub",
 "version_differences":"PS2 92/216-byte040C and0110 stream/device layouts are not adopted; native GPU/object consumers remain required",
 "verification_scope":"source-derived before native edit","game_run":False,"acceptance":"OPEN"}
with (R/"REGISTRATION_SOURCE.json").open("x",newline="\n") as f:json.dump(receipt,f,indent=2);f.write("\n")
print("PASS original registration/consumer coverage and exact pre-edit native snapshots",flush=True)
