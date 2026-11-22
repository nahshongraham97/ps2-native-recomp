"""Read-only authentication of saved service checks; optionally save a new receipt."""
from pathlib import Path
import gzip,hashlib,json,re,subprocess,sys
R=Path(__file__).resolve().parent;ROOT=R.parents[2];PARENT=ROOT.parents[2]
CODE=["cmake/native_game.cmake","runtime/include/mka/retail_sound_bank_catalog.hpp",
 "runtime/include/mka/retail_sound_catalog_data.hpp","runtime/src/native/retail_sound_bank_catalog.cpp",
 "runtime/src/native/retail_sound_bank_main.cpp","tests/test_retail_mode_service.cpp"]
RUNS=["service-compile-05","service-mode-zero-05","service-reset-01","service-metadata-01",
 "service-negative-01","service-symbols-01"]
TARGETS=["test_native_retail_mode_service","test_native_retail_reset_sound","test_native_retail_sound_metadata"]
RESOLVED="mka::retail::main_binding::initialize_retail_mode_service(mka::retail::NativeRetailMainState&, mka::retail::StartupFailure&)"
def sha(p):
 with p.open("rb") as f:return hashlib.file_digest(f,"sha256").hexdigest()
def read(p):return json.loads(p.read_text())
def git(*args):return subprocess.check_output(["git","-c","core.autocrlf=false",*args],cwd=ROOT,timeout=20).decode().strip()
def save(name,v):
 with (R/name).open("x",newline="\n") as f:json.dump(v,f,indent=2);f.write("\n")
def authenticate():
 prior_path=R/"validation-mode-clean-02/VERIFICATION.json";prior=read(prior_path)
 predecessor=prior["remaining_undefined_symbols"]
 assert prior["status"]=="PASS_NATIVE_MODE_STARTUP_AND_RESOLVED_FIELDS"
 assert isinstance(predecessor,list) and all(isinstance(x,str) for x in predecessor)
 assert len(predecessor)==len(set(predecessor))==58 and predecessor.count(RESOLVED)==1
 sample=read(R/"service-mode-zero-05/RESULT.json");pins=read(R/"service-mode-zero-05/SOURCE_FILES.json")
 schema={"predecessor_status":type(prior["status"]).__name__,
  "predecessor_remaining_undefined_symbols":type(predecessor).__name__,
  "predecessor_list_length":len(predecessor),"runner":{k:type(v).__name__ for k,v in sample.items()},
  "pin_map_type":type(pins).__name__,"pin_map_value_types":sorted({type(v).__name__ for v in pins.values()}),
  "expected_client_targets":TARGETS,"expected_client_target_count":len(TARGETS)}
 print("SCHEMA",json.dumps(schema,sort_keys=True),flush=True)
 assert isinstance(pins,dict) and all(isinstance(k,str) and isinstance(v,str) and re.fullmatch("[0-9a-f]{64}",v) for k,v in pins.items())
 assert all(sha(ROOT/n)==v for n,v in pins.items())
 old_pins=read(R/"validation-mode-clean-02/SOURCE_FILES.json")
 delta=sorted(n for n in set(pins)|set(old_pins) if pins.get(n)!=old_pins.get(n))
 assert delta==sorted(CODE),(delta,CODE)
 logs={};receipts={}
 for name in RUNS:
  folder=R/name;v=read(folder/"RESULT.json");raw=gzip.decompress((folder/"output.log.gz").read_bytes())
  assert isinstance(v["exit"],int) and not isinstance(v["exit"],bool)
  assert isinstance(v["status"],str) and isinstance(v["argv"],list)
  assert v["source_commit"]=="97bf25eeb7a541eefc48b9c618ae91c55d558e2c"
  assert v["source_manifest_sha256"]==sha(folder/"SOURCE_FILES.json") and read(folder/"SOURCE_FILES.json")==pins
  assert v["log_sha256"]==hashlib.sha256(raw).hexdigest()
  assert v["runner_sha256"]==sha(R/"run_service_check.py")
  assert v["executable_sha256"]==sha(Path(v["argv"][0]))
  assert v["cache_sha256"]==sha(ROOT/"b/CMakeCache.txt")
  assert v["compiler_sha256"]==sample["compiler_sha256"]
  assert v["game_run"] is False and v["visible_window"] is False and v["audio_endpoint"] is False
  if name=="service-negative-01":assert v["exit"]!=0 and v["status"]=="NONZERO"
  else:assert v["exit"]==0 and v["status"]=="PASS"
  receipts[name]={"result_sha256":sha(folder/"RESULT.json"),"log_sha256":v["log_sha256"],
   "compressed_log_sha256":sha(folder/"output.log.gz"),"seconds":v["seconds"],"argv":v["argv"],
   "executable_sha256":v["executable_sha256"]}
  logs[name]=raw.decode(errors="replace")
 assert sample["compiler_sha256"]==sha(PARENT.parent/"dependencies/compiler/llvm-mingw-20261006-ucrt-x86_64/bin/clang++.exe")
 build=read(R/"service-compile-05/RESULT.json")["argv"]
 assert build[build.index("--target")+1:build.index("-j2")]==TARGETS
 actual_clients=[Path(receipts[n]["argv"][0]).stem for n in RUNS[1:4]]
 assert actual_clients==TARGETS and len(actual_clients)==3
 for name in RUNS[1:4]:assert "PASS" in logs[name] and "FAIL" not in logs[name]
 missing=re.findall(r"undefined symbol: ([^\r\n]+)",logs["service-negative-01"])
 assert len(missing)==len(set(missing))==57
 assert sorted(missing)==sorted(n for n in predecessor if n!=RESOLVED)
 assert not (ROOT/"b/test_native_startup_missing_bindings.exe").exists()
 symbols=logs["service-symbols-01"]
 for name in [RESOLVED,"mka::retail::NativeRetailSoundBankCatalog::select_mode_zero("]:
  assert any(re.search(r"\bT\b",line) and name in line for line in symbols.splitlines()),name
 forbidden=r"(?<![A-Za-z0-9_])(?:ps2|gs|spu2|sif|Cpu|recomp)::|\b(?:g_ram|g_spr)\b|SDL_OpenAudio"
 assert not re.search(forbidden,symbols)
 assert re.search(r"\bT SDL_InitSubSystem\b",symbols) and "__imp_SDL_InitSubSystem" not in symbols
 identity=read(R/"MODE_ANCHORS.json")
 xbe=(PARENT/"private-inputs/xbox-mka-20261121/default.xbe").read_bytes()
 assert hashlib.sha256(xbe).hexdigest()==identity["xbe_sha256"]
 source={}
 for name in ["source-mode-01"]+[f"source-mode-zero-{i}" for i in range(17,24)]:
  joined=read(R/(name+".join.json"));assert joined["xbe_sha256"]==identity["xbe_sha256"]
  assert (R/name/"PROGRAM.txt").read_text().splitlines()[:3]==["default.xbe",identity["xbe_sha256"],"x86:LE:32:default"]
  bounds={r.split("\t")[0]:r.split("\t")[1:3] for r in (R/name/"functions.tsv").read_text().splitlines()}
  for entry,fn in joined["functions"].items():
   path=R/name/(entry+".asm");assert sha(path)==fn["listing_sha256"]
   rows=[r.split("\t",2) for r in path.read_text().splitlines()]
   assert len(rows)==fn["instructions"]==int(bounds[entry][1])==len(fn["original_spans"])
   cursor=int(entry,16);gaps=[]
   for row,span in zip(rows,fn["original_spans"]):
    at=int(row[0],16);raw=bytes.fromhex(row[1])
    if at!=cursor:gaps.append([hex(cursor),hex(at)])
    cursor=at+len(raw)
    section=[s for s in identity["sections"] if s["virtual_address"]<=at and cursor<=s["virtual_address"]+s["file_size"]]
    assert len(section)==1
    offset=section[0]["file_offset"]+at-section[0]["virtual_address"]
    assert span["file_offset"]==offset and span["vma"]==hex(at) and span["bytes_hex"]==row[1]
    assert xbe[offset:offset+len(raw)]==raw
   assert cursor==int(bounds[entry][0],16)+1
   source[name+"/"+entry]={"range":[entry,bounds[entry][0]],"instructions":len(rows),
    "listing_sha256":sha(path),"unlisted_byte_gaps":gaps}
 for key in ["source-mode-01/0023c8a0","source-mode-01/0023d060",
             "source-mode-zero-17/0023d4d0","source-mode-zero-17/0023d830",
             "source-mode-zero-18/0023d230","source-mode-zero-18/0023d540",
             "source-mode-zero-19/0023c700","source-mode-zero-19/0023d8d0",
             "source-mode-zero-20/0023baf0","source-mode-zero-21/0023bb20",
             "source-mode-zero-21/0023bb70","source-mode-zero-22/0023bbc0"]:
  assert not source[key]["unlisted_byte_gaps"],key
 data=read(R/"MODE_ZERO_DATA.json");assert data["xbe_sha256"]==identity["xbe_sha256"]
 assert all(x["rows"][0]["list_data"]["entries"]==[[0,0,1],[8,4,1]] for x in data["descriptors"])
 elf=PARENT/"private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/SLUS_214.10"
 assert sha(elf)==data["elf_sha256"]
 assets={}
 for file,expected in [("MSLASSET.MS2","74b7affaf2ec29052b39f531788ff12148f9870e79dde953e701c63e6e948d5d"),
                       ("MSLASSET.MS4","bd97bd4ae76b41a18852555bebb7909e80c1287aba950ec1d85adacc10f46781")]:
  path=elf.parent/"sndsps2"/file;assets[file]={"sha256":sha(path),"bytes":path.stat().st_size}
  assert assets[file]["sha256"]==expected
 config={}
 for line in (ROOT/"b/CMakeCache.txt").read_text().splitlines():
  if line and not line.startswith(("#","//")) and "=" in line:
   key,value=line.split("=",1)
   if key.startswith(("CMAKE_BUILD_TYPE:","CMAKE_CXX_COMPILER:","CMAKE_CXX_FLAGS","MKA_","PS2_")):config[key]=value
 changed_text=[ROOT/n for n in CODE]
 assert all(b"\r" not in p.read_bytes() for p in changed_text)
 git("diff","--check","--",*CODE)
 return {"status":"AUTHENTICATED_MODE_ZERO_SERVICE_COMPONENT","schema_preflight":schema,
  "measured_source_base":sample["source_commit"],"source_files":len(pins),"source_manifest_sha256":sha(R/"service-mode-zero-05/SOURCE_FILES.json"),
  "changed_source_paths":delta,"client_targets":actual_clients,"client_target_count":len(actual_clients),
  "receipts":receipts,"configuration":config,"compiler_sha256":sample["compiler_sha256"],
  "cache_sha256":sample["cache_sha256"],"sdl2_sha256":sha(ROOT/"b/SDL2.dll"),
  "predecessor_receipt_sha256":sha(prior_path),"resolved_bindings":[RESOLVED],"remaining_undefined_symbols":missing,
  "source_listings":source,"mode_data_sha256":sha(R/"MODE_ZERO_DATA.json"),"assets":assets,
  "expected_metadata_sha256":sha(ROOT/"research/runs/parallel-sound-metadata-resolver-20261122/expected-metadata.tsv"),
  "symbol_audit":"NO_GUEST_RUNTIME_SYMBOLS; SDL_STARTUP_ABORT_GUARD","verification_scope":"tested component; dirty precommit build",
  "pending_play_runtime_branch":"OPEN_NOT_EXERCISED","oracle_comparison":"OPEN","native_save_reload":"OPEN",
  "visible_window":False,"audio_endpoint":False,"game_run":False,"acceptance":"OPEN"}
if __name__=="__main__":
 try:result=authenticate()
 except Exception as error:
  if len(sys.argv)==2:save(sys.argv[1]+".failure.json",{"status":"AUTHENTICATION_FAILURE","error":repr(error)})
  raise
 if len(sys.argv)==2:save(sys.argv[1],result)
 print(result["status"],"clients",result["client_target_count"],"exact remaining",len(result["remaining_undefined_symbols"]),flush=True)
