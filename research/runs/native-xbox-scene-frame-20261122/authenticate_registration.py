"""Authenticate saved scene-registration evidence without repeating unchanged clients."""
from pathlib import Path
import gzip,hashlib,json,re,subprocess,sys
R=Path(__file__).resolve().parent;ROOT=R.parents[2];PARENT=ROOT.parents[2]
PRIOR=ROOT/"research/runs/native-xbox-mode-scene-20261122"
CODE=["cmake/native_game.cmake","runtime/include/mka/retail_renderer_core.hpp",
 "runtime/src/native/retail_renderer_core.cpp","runtime/src/native/retail_renderer_raster_registration.cpp",
 "tests/test_retail_renderer_raster_registration.cpp"]
CLIENTS=[("scene-registration-02","test_native_retail_renderer_raster_registration"),
 ("scene-core-02","test_native_retail_renderer_core"),("scene-allocator-01","test_native_retail_renderer_allocator"),
 ("scene-matrix-01","test_native_retail_renderer_matrix")]
TEST=CODE[-1]
def sha(p):
 with p.open("rb") as f:return hashlib.file_digest(f,"sha256").hexdigest()
def read(p):return json.loads(p.read_text())
def save(name,v):
 with (R/name).open("x",newline="\n") as f:json.dump(v,f,indent=2);f.write("\n")
def authenticate():
 prior=read(PRIOR/"SERVICE_COMPONENT_AUTHENTICATION.json")
 sample=read(R/"scene-registration-02/RESULT.json");pins=read(R/"scene-registration-02/SOURCE_FILES.json")
 schema={"prior_status":type(prior["status"]).__name__,"prior_remaining_symbols":type(prior["remaining_undefined_symbols"]).__name__,
  "prior_remaining_count":len(prior["remaining_undefined_symbols"]),"current_result":{k:type(v).__name__ for k,v in sample.items()},
  "source_pin_map":type(pins).__name__,"source_pin_values":sorted({type(v).__name__ for v in pins.values()}),
  "client_targets":[t for _,t in CLIENTS],"client_count":len(CLIENTS)}
 print("SCHEMA",json.dumps(schema,sort_keys=True),flush=True)
 assert prior["status"]=="AUTHENTICATED_MODE_ZERO_SERVICE_COMPONENT"
 assert isinstance(prior["remaining_undefined_symbols"],list) and len(prior["remaining_undefined_symbols"])==57
 assert isinstance(pins,dict) and all(isinstance(k,str) and isinstance(v,str) for k,v in pins.items())
 assert all(sha(ROOT/n)==v for n,v in pins.items())
 baseline=read(PRIOR/"service-mode-zero-05/SOURCE_FILES.json")
 assert sorted(n for n in set(pins)|set(baseline) if pins.get(n)!=baseline.get(n))==sorted(CODE)
 assert len(pins)==903
 receipts={};logs={}
 def run(name,expected,allow_old_test=False):
  path=R/name;v=read(path/"RESULT.json");p=read(path/"SOURCE_FILES.json")
  raw=gzip.decompress((path/"output.log.gz").read_bytes())
  assert type(v["exit"]) is int and type(v["status"]) is str and isinstance(v["argv"],list)
  assert v["source_commit"]=="2c2d9dcdfb46583e7ce68f0342007804e1b0bdda"
  assert v["source_manifest_sha256"]==sha(path/"SOURCE_FILES.json")
  difference=[n for n in set(p)|set(pins) if p.get(n)!=pins.get(n)]
  assert difference==([] if not allow_old_test else [TEST]),(name,difference)
  if difference:
   assert p[TEST]==sha(R/"test_raster_registration.before-error-expectation.cpp")
  assert v["runner_sha256"]==sha(R/"run_scene_check.py")
  assert v["compiler_sha256"]==sample["compiler_sha256"]
  assert v["cache_sha256"]==sha(ROOT/"b/CMakeCache.txt")
  assert v["log_sha256"]==hashlib.sha256(raw).hexdigest()
  assert v["visible_window"] is False and v["audio_endpoint"] is False and v["game_run"] is False
  assert (v["status"],v["exit"])==("PASS",0) if expected else v["status"]=="NONZERO" and v["exit"]!=0
  if expected or name=="scene-negative-01":assert v["executable_sha256"]==sha(Path(v["argv"][0]))
  receipts[name]={"result_sha256":sha(path/"RESULT.json"),"source_manifest_sha256":sha(path/"SOURCE_FILES.json"),
   "allowed_unlinked_test_difference":difference,"log_sha256":v["log_sha256"],"compressed_log_sha256":sha(path/"output.log.gz"),
   "argv":v["argv"],"executable_sha256":v["executable_sha256"],"seconds":v["seconds"],"status":v["status"],"exit":v["exit"]}
  logs[name]=raw.decode(errors="replace")
  return v
 for name,target in CLIENTS:
  v=run(name,True,name in {"scene-allocator-01","scene-matrix-01"})
  assert v["phase"]=="client" and Path(v["argv"][0]).stem==target
  assert "PASS" in logs[name] and "FAIL" not in logs[name]
 build=run("scene-compile-03",True,True)["argv"]
 assert build[build.index("--target")+1:build.index("-j2")]==[t for _,t in CLIENTS]
 build=run("scene-compile-04",True)["argv"]
 assert build[build.index("--target")+1:build.index("-j2")]==[CLIENTS[0][1]]
 run("scene-registration-01",False,True);run("scene-core-01",False,True)
 assert receipts["scene-core-01"]["executable_sha256"]==receipts["scene-core-02"]["executable_sha256"]
 assert "actual duplicate registry error side effect" in logs["scene-registration-01"]
 before=(R/"test_raster_registration.before-error-expectation.cpp").read_bytes()
 assert before.replace(
  b'error->plugin==1&&error->code==0x80000017u,"actual duplicate registry error side effect"',
  b'error->plugin==0&&error->code==0x80000017u,"actual duplicate high-bit error normalizes plugin to zero"'
 )==(ROOT/TEST).read_bytes()
 run("scene-negative-01",False);run("scene-symbols-01",True)
 removed=[n for n in prior["remaining_undefined_symbols"] if
  "renderer_required::register_object_extensions_1b72b0(" in n or
  "renderer_required::discard_object_extensions_after_core_init(" in n]
 missing=re.findall(r"undefined symbol: ([^\r\n]+)",logs["scene-negative-01"])
 assert len(removed)==2 and len(missing)==len(set(missing))==55
 assert sorted(missing)==sorted(n for n in prior["remaining_undefined_symbols"] if n not in removed)
 assert not (ROOT/"b/test_native_startup_missing_bindings.exe").exists()
 symbols=logs["scene-symbols-01"]
 for name in removed+["mka::retail::NativeRetailRendererRasterExtensionRegistration::observe(",
  "mka::retail::NativeRetailRendererCore::raster_extension("]:
  assert any(re.search(r"\bT\b",line) and name in line for line in symbols.splitlines()),name
 assert re.search(r"\bT SDL_InitSubSystem\b",symbols) and "__imp_SDL_InitSubSystem" not in symbols
 assert not re.search(r"(?<![A-Za-z0-9_])(?:ps2|gs|spu2|sif|Cpu|recomp)::|\b(?:g_ram|g_spr)\b|SDL_OpenAudio",symbols)
 identity=read(PRIOR/"MODE_ANCHORS.json")
 image=(PARENT/"private-inputs/xbox-mka-20261121/default.xbe").read_bytes()
 assert hashlib.sha256(image).hexdigest()==identity["xbe_sha256"]
 listings={}
 for path in sorted(R.glob("source-*.join.json")):
  joined=read(path);folder=R/path.name.removesuffix(".join.json")
  assert joined["xbe_sha256"]==identity["xbe_sha256"]
  assert (folder/"PROGRAM.txt").read_text().splitlines()==["default.xbe",identity["xbe_sha256"],"x86:LE:32:default"]
  for entry,row in joined["functions"].items():
   listing=folder/(entry+".asm");assert sha(listing)==row["listing_sha256"]
   lines=[r.split("\t",2) for r in listing.read_text().splitlines()]
   assert len(lines)==row["instructions"]==len(row["original_spans"])
   for line,span in zip(lines,row["original_spans"]):
    at=int(line[0],16);raw=bytes.fromhex(line[1]);end=at+len(raw)
    section=[s for s in identity["sections"] if s["virtual_address"]<=at and end<=s["virtual_address"]+s["file_size"]]
    assert len(section)==1
    offset=section[0]["file_offset"]+at-section[0]["virtual_address"]
    assert offset==span["file_offset"] and span["bytes_hex"]==line[1]
    assert image[offset:offset+len(raw)]==raw
   listings[listing.relative_to(R).as_posix()]={"sha256":sha(listing),"instructions":len(lines)}
 source=read(R/"REGISTRATION_SOURCE.json")
 assert source["xbe_sha256"]==identity["xbe_sha256"] and source["selected_behavior"]["constructor"] is None
 assert source["selected_behavior"]["destructor"] is None and source["selected_behavior"]["copy"] is None
 for row in source["functions"].values():
  assert row["listing_sha256"]==sha(R/row["listing"])
  assert row["join_sha256"]==sha(R/(row["listing"].split("/")[0]+".join.json"))
  assert row["unlisted_byte_gaps"]==[]
 for n,digest in source["native_before"].items():
  assert hashlib.sha256(gzip.decompress((R/(Path(n).name+".before-registration.gz")).read_bytes())).hexdigest()==digest==baseline[n]
 assert source["existing_registry_evidence_sha256"]==sha(ROOT/source["existing_registry_evidence"])
 cuts=int(re.search(r"allocation cuts=(\d+)",logs["scene-registration-02"]).group(1));assert 0<cuts<16
 config={}
 for line in (ROOT/"b/CMakeCache.txt").read_text().splitlines():
  if line and not line.startswith(("#","//")) and "=" in line:
   k,v=line.split("=",1)
   if k.startswith(("CMAKE_BUILD_TYPE:","CMAKE_CXX_COMPILER:","CMAKE_CXX_FLAGS","MKA_","PS2_")):config[k]=v
 assert all(b"\r" not in (ROOT/n).read_bytes() for n in CODE)
 subprocess.run(["git","-c","core.autocrlf=false","diff","--check","--",*CODE],cwd=ROOT,check=True,timeout=15)
 return {"status":"AUTHENTICATED_NATIVE_RASTER_REGISTRATION_COMPONENT","schema_preflight":schema,
  "measured_source_base":sample["source_commit"],"source_files":len(pins),
  "source_manifest_sha256":sha(R/"scene-registration-02/SOURCE_FILES.json"),"changed_source_paths":CODE,
  "client_targets":[t for _,t in CLIENTS],"client_target_count":len(CLIENTS),"allocation_cuts":cuts,
  "receipts":receipts,"resolved_bindings":removed,"remaining_undefined_symbols":missing,
  "configuration":config,"compiler_sha256":sample["compiler_sha256"],"cache_sha256":sample["cache_sha256"],
  "sdl2_sha256":sha(ROOT/"b/SDL2.dll"),"source_listings":listings,
  "registration_source_sha256":sha(R/"REGISTRATION_SOURCE.json"),
  "verification_scope":"tested component; precommit build authenticated; retained unchanged allocator/matrix executions",
  "raster_gpu_creation":"OPEN","renderer_factories":"OPEN","engine_open_start":"OPEN","oracle_comparison":"OPEN",
  "native_save_reload":"OPEN","numerical_hd":"OPEN","game_run":False,"visible_window":False,"audio_endpoint":False,"acceptance":"OPEN"}
if __name__=="__main__":
 try:result=authenticate()
 except Exception as error:
  if len(sys.argv)==2:save(sys.argv[1]+".failure.json",{"status":"AUTHENTICATION_FAILED","error":repr(error)})
  raise
 if len(sys.argv)==2:save(sys.argv[1],result)
 print(result["status"],"clients",result["client_target_count"],"source files",result["source_files"],
       "exact remaining",len(result["remaining_undefined_symbols"]),flush=True)
