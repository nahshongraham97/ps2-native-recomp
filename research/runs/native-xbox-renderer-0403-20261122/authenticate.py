"""Read-only authentication of0403 saved source/build/component evidence."""
from pathlib import Path
import gzip,hashlib,json,re,subprocess,sys
R=Path(__file__).resolve().parent;ROOT=R.parents[2];PARENT=ROOT.parents[2]
PRIOR=ROOT/"research/runs/native-xbox-scene-frame-20261122"
CODE=["cmake/native_game.cmake","runtime/include/mka/retail_renderer_core.hpp",
 "runtime/include/mka/retail_renderer_frames.hpp","runtime/src/native/retail_renderer_frames.cpp",
 "tests/test_retail_renderer_frames.cpp"]
TEST=CODE[-1];CPP=CODE[-2]
CLIENTS=[("frames-02","test_native_retail_renderer_frames"),("core-01","test_native_retail_renderer_core"),
 ("matrix-01","test_native_retail_renderer_matrix"),("registration-01","test_native_retail_renderer_raster_registration"),
 ("allocator-01","test_native_retail_renderer_allocator")]
def sha(p):
 with p.open("rb") as f:return hashlib.file_digest(f,"sha256").hexdigest()
def read(p):return json.loads(p.read_text())
def save(name,v):
 with (R/name).open("x",newline="\n") as f:json.dump(v,f,indent=2);f.write("\n")
def authenticate():
 prior=read(PRIOR/"REGISTRATION_COMPONENT_AUTHENTICATION.json")
 sample=read(R/"frames-02/RESULT.json");pins=read(R/"frames-02/SOURCE_FILES.json");source=read(R/"SOURCE_0403.json")
 schema={"prior_status":type(prior["status"]).__name__,"prior_symbols":type(prior["remaining_undefined_symbols"]).__name__,
  "prior_symbol_count":len(prior["remaining_undefined_symbols"]),"result":{k:type(v).__name__ for k,v in sample.items()},
  "source_pin_map":type(pins).__name__,"pin_value_types":sorted({type(v).__name__ for v in pins.values()}),
  "source_recipe_functions":type(source["functions"]).__name__,
  "source_recipe_gap_field":type(next(iter(source["functions"].values()))["decoded_unlisted_gaps"]).__name__,
  "expected_targets":[n for _,n in CLIENTS],"expected_target_count":len(CLIENTS)}
 print("SCHEMA",json.dumps(schema,sort_keys=True),flush=True)
 assert prior["status"]=="AUTHENTICATED_NATIVE_RASTER_REGISTRATION_COMPONENT"
 assert isinstance(prior["remaining_undefined_symbols"],list) and len(prior["remaining_undefined_symbols"])==55
 assert isinstance(pins,dict) and len(pins)==906 and all(isinstance(k,str) and isinstance(v,str) for k,v in pins.items())
 assert all(sha(ROOT/n)==v for n,v in pins.items())
 baseline=read(PRIOR/"scene-registration-02/SOURCE_FILES.json")
 assert sorted(n for n in set(pins)|set(baseline) if pins.get(n)!=baseline.get(n))==sorted(CODE)
 receipts={};logs={}
 old_test=sha(R/"test_retail_renderer_frames.before-middle-coverage.cpp")
 def run(name,expected,old=False,historic_binary=False):
  folder=R/name;v=read(folder/"RESULT.json");p=read(folder/"SOURCE_FILES.json")
  raw=gzip.decompress((folder/"output.log.gz").read_bytes())
  assert type(v["exit"]) is int and type(v["status"]) is str and isinstance(v["argv"],list)
  assert v["source_commit"]=="91aefc25a8d49111c07b63cf3aaf992ee93cf0df"
  assert v["source_manifest_sha256"]==sha(folder/"SOURCE_FILES.json")
  differences=sorted(n for n in set(p)|set(pins) if p.get(n)!=pins.get(n))
  expected_differences=sorted([TEST,CPP]) if name=="compile-01" else [TEST] if old else []
  assert differences==expected_differences,(name,differences)
  if TEST in differences:assert p[TEST]==old_test
  if CPP in differences:assert p[CPP]==sha(R/"retail_renderer_frames.before-capture-fix.cpp")
  assert v["runner_sha256"]==sha(R/"run_check.py")
  assert v["compiler_sha256"]==sample["compiler_sha256"] and v["cache_sha256"]==sha(ROOT/"b/CMakeCache.txt")
  assert v["log_sha256"]==hashlib.sha256(raw).hexdigest()
  assert v["visible_window"] is False and v["audio_endpoint"] is False and v["game_run"] is False
  if expected:assert (v["status"],v["exit"])==("PASS",0)
  else:assert v["status"]=="NONZERO" and v["exit"]!=0
  if not historic_binary:assert v["executable_sha256"]==sha(Path(v["argv"][0]))
  receipts[name]={"result_sha256":sha(folder/"RESULT.json"),"source_manifest_sha256":sha(folder/"SOURCE_FILES.json"),
   "allowed_source_differences":differences,"log_sha256":v["log_sha256"],
   "compressed_log_sha256":sha(folder/"output.log.gz"),"argv":v["argv"],"executable_sha256":v["executable_sha256"],
   "binary_authentication":"HISTORICAL_HASH_ONLY_OLD_FRAMES_BINARY_NOT_RETAINED" if historic_binary else "CURRENT_BYTES_MATCH",
   "status":v["status"],"exit":v["exit"],"seconds":v["seconds"]}
  logs[name]=raw.decode(errors="replace");return v
 for name,target in CLIENTS:
  v=run(name,True,name!="frames-02")
  assert v["phase"]=="client" and Path(v["argv"][0]).stem==target
  assert "PASS" in logs[name] and "FAIL" not in logs[name]
 build=run("compile-02",True,True)["argv"]
 assert build[build.index("--target")+1:build.index("-j2")]==[n for _,n in CLIENTS]
 build=run("compile-03",True)["argv"]
 assert build[build.index("--target")+1:build.index("-j2")]==[CLIENTS[0][1]]
 run("compile-01",False,True)
 assert "no matching member function for call to 'reset'" in logs["compile-01"]
 before=(R/"retail_renderer_frames.before-capture-fix.cpp").read_bytes()
 assert before.count(b"[allocator,storage=")==1 and before.count(b"allocator.reset();")==1
 corrected=before.replace(b"[allocator,storage=",b"[heap=std::shared_ptr<NativeRetailRendererAllocatorService>(allocator),storage=")
 corrected=corrected.replace(b"allocator.reset();",b"heap.reset();")
 assert corrected==(ROOT/CPP).read_bytes()
 run("frames-01",True,True,True)
 run("negative-01",False);run("symbols-01",True)
 removed=[n for n in prior["remaining_undefined_symbols"] if
  "renderer_required::construct_core_plugin_0403(" in n or "renderer_required::destroy_core_plugin_0403(" in n]
 missing=re.findall(r"undefined symbol: ([^\r\n]+)",logs["negative-01"])
 assert len(removed)==2 and len(missing)==len(set(missing))==53
 assert sorted(missing)==sorted(n for n in prior["remaining_undefined_symbols"] if n not in removed)
 assert not (ROOT/"b/test_native_startup_missing_bindings.exe").exists()
 symbols=logs["symbols-01"]
 for name in removed+["mka::retail::NativeRetailRendererFrames::create_base_frame(",
  "mka::retail::NativeRetailRendererFrames::add_child(","mka::retail::NativeRetailRendererFrames::destroy_root_hierarchy("]:
  assert any(re.search(r"\bT\b",line) and name in line for line in symbols.splitlines()),name
 assert re.search(r"\bT SDL_InitSubSystem\b",symbols) and "__imp_SDL_InitSubSystem" not in symbols
 assert not re.search(r"(?<![A-Za-z0-9_])(?:ps2|gs|spu2|sif|Cpu|recomp)::|\b(?:g_ram|g_spr)\b|SDL_OpenAudio",symbols)
 identity=read(ROOT/"research/runs/native-xbox-mode-scene-20261122/MODE_ANCHORS.json")
 image=(PARENT/"private-inputs/xbox-mka-20261121/default.xbe").read_bytes()
 assert source["status"]=="SOURCE_DERIVED_0403_RESOURCE_AND_BASE_FRAME_LIFECYCLE"
 assert hashlib.sha256(image).hexdigest()==source["xbe_sha256"]==identity["xbe_sha256"]
 def original(at,n):
  sections=[s for s in identity["sections"] if s["virtual_address"]<=at and at+n<=s["virtual_address"]+s["file_size"]]
  assert len(sections)==1;s=sections[0];offset=s["file_offset"]+at-s["virtual_address"]
  return image[offset:offset+n],offset
 adopted={}
 for entry,row in source["functions"].items():
  path=ROOT/row["listing"];join=Path(str(path.parent)+".join.json")
  assert row["listing_sha256"]==sha(path) and row["join_sha256"]==sha(join)
  program=(path.parent/"PROGRAM.txt").read_text().splitlines()
  assert program==["default.xbe",identity["xbe_sha256"],"x86:LE:32:default"]
  spans=read(join)["functions"][entry]["original_spans"];chunks=[]
  assert isinstance(row["decoded_unlisted_gaps"],list)
  for span in spans+row["decoded_unlisted_gaps"]:
   at=int(span["vma"],16);raw,offset=original(at,span["length"])
   assert raw.hex()==span["bytes_hex"] and offset==span["file_offset"]
   chunks.append((at,len(raw)))
  cursor=int(row["inclusive_range"][0],16)
  for at,n in sorted(chunks):assert cursor==at;cursor=at+n
  assert cursor==int(row["inclusive_range"][1],16)+1
  adopted[entry]={"listing_sha256":sha(path),"inclusive_range":row["inclusive_range"],
   "instructions":row["instructions"],"decoded_unlisted_gaps":row["decoded_unlisted_gaps"]}
 assert len(adopted)==13 and sum(len(v["decoded_unlisted_gaps"]) for v in adopted.values())==2
 for row in source["data"].values():
  raw,offset=original(int(row["vma"],16),row["length"])
  assert raw.hex()==row["bytes_hex"] and offset==row["file_offset"]
 for n,digest in source["native_before"].items():
  raw=gzip.decompress((R/(Path(n).name+".before-0403.gz")).read_bytes())
  assert hashlib.sha256(raw).hexdigest()==digest==baseline[n]
 cuts=re.search(r"allocation cuts module=(\d+) frame=(\d+)",logs["frames-02"]);assert cuts
 module_cuts,frame_cuts=map(int,cuts.groups());assert 0<module_cuts<16 and 0<frame_cuts<16
 config={}
 for line in (ROOT/"b/CMakeCache.txt").read_text().splitlines():
  if line and not line.startswith(("#","//")) and "=" in line:
   k,v=line.split("=",1)
   if k.startswith(("CMAKE_BUILD_TYPE:","CMAKE_CXX_COMPILER:","CMAKE_CXX_FLAGS","MKA_","PS2_")):config[k]=v
 assert all(b"\r" not in (ROOT/n).read_bytes() for n in CODE)
 subprocess.run(["git","-c","core.autocrlf=false","diff","--check","--",*CODE],cwd=ROOT,check=True,timeout=15)
 return {"status":"AUTHENTICATED_NATIVE0403_RESOURCE_BASE_FRAMES_COMPONENT","schema_preflight":schema,
  "measured_source_base":sample["source_commit"],"source_files":len(pins),"source_manifest_sha256":sha(R/"frames-02/SOURCE_FILES.json"),
  "changed_source_paths":CODE,"client_targets":[n for _,n in CLIENTS],"client_target_count":len(CLIENTS),
  "module_allocation_cuts":module_cuts,"frame_allocation_cuts":frame_cuts,"host_frame_growth":120,
  "receipts":receipts,"resolved_bindings":removed,"remaining_undefined_symbols":missing,
  "compiler_sha256":sample["compiler_sha256"],"cache_sha256":sample["cache_sha256"],"configuration":config,
  "sdl2_sha256":sha(ROOT/"b/SDL2.dll"),"source_receipt_sha256":sha(R/"SOURCE_0403.json"),"adopted_source":adopted,
  "verification_scope":"tested component; precommit source; four unchanged regression results differ only in unrelated expanded test",
  "base_empty_registry_only":True,"nonempty_frame_plugins":"OPEN","clone":"OPEN","game_synchronization":"OPEN",
  "renderer_gpu":"OPEN","native_save_reload":"OPEN","oracle_comparison":"OPEN","numerical_hd":"OPEN",
  "game_run":False,"visible_window":False,"audio_endpoint":False,"acceptance":"OPEN"}
if __name__=="__main__":
 try:result=authenticate()
 except Exception as error:
  if len(sys.argv)==2:save(sys.argv[1]+".failure.json",{"status":"AUTHENTICATION_FAILED","error":repr(error)})
  raise
 if len(sys.argv)==2:save(sys.argv[1],result)
 print(result["status"],"clients",result["client_target_count"],"sources",result["source_files"],
       "remaining",len(result["remaining_undefined_symbols"]),flush=True)
