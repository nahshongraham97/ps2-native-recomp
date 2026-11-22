"""Authenticate0404 evidence and publish scoped exact bytes; fail before commit/push on any mismatch."""
from pathlib import Path
import gzip,hashlib,json,re,subprocess,sys
R=Path(__file__).resolve().parent;ROOT=R.parents[2];PARENT=ROOT.parents[2];REL=R.relative_to(ROOT).as_posix()
BASE="e81087e937b159a6c325c93a5f10fe78aa16150b"
CODE=["cmake/native_game.cmake","runtime/include/mka/retail_renderer_core.hpp","runtime/src/native/retail_renderer_core.cpp",
 "runtime/include/mka/native_file.hpp","runtime/src/native/native_file.cpp","runtime/include/mka/content_volume.hpp",
 "runtime/src/native/content_volume.cpp","runtime/include/mka/retail_renderer_streams.hpp",
 "runtime/src/native/retail_renderer_streams.cpp","tests/test_retail_renderer_streams.cpp"]
TEST=CODE[-1]
CLIENTS=[("streams-02","test_native_retail_renderer_streams"),("frames-01","test_native_retail_renderer_frames"),
 ("core-01","test_native_retail_renderer_core"),("matrix-01","test_native_retail_renderer_matrix"),
 ("registration-01","test_native_retail_renderer_raster_registration"),("allocator-01","test_native_retail_renderer_allocator"),
 ("native-file-01","test_native_file"),("volume-01","test_native_content_volume_owner")]
def sha(p):
 with p.open("rb") as f:return hashlib.file_digest(f,"sha256").hexdigest()
def read(p):return json.loads(p.read_text())
def save(name,v):
 with (R/name).open("x",newline="\n") as f:json.dump(v,f,indent=2);f.write("\n")
def git(*a):return subprocess.check_output(["git","-c","core.autocrlf=false",*a],cwd=ROOT,timeout=25).decode().strip()
def blobs(revs):
 p=subprocess.run(["git","cat-file","--batch"],cwd=ROOT,input=("\n".join(revs)+"\n").encode(),capture_output=True,check=True,timeout=30)
 raw=p.stdout;at=0;out=[]
 for rev in revs:
  end=raw.index(b"\n",at);h=raw[at:end].split();assert len(h)==3 and h[1]==b"blob",(rev,h)
  at=end+1;n=int(h[2]);out.append(hashlib.sha256(raw[at:at+n]).hexdigest());at+=n
  assert raw[at:at+1]==b"\n";at+=1
 assert at==len(raw);return out
def authenticate():
 prior_dir=ROOT/"research/runs/native-xbox-renderer-0403-20261122"
 prior=read(prior_dir/"COMPONENT_AUTHENTICATION.json");source=read(R/"SOURCE_0404.json")
 sample=read(R/"streams-02/RESULT.json");pins=read(R/"streams-02/SOURCE_FILES.json")
 schema={"prior_status":type(prior["status"]).__name__,"prior_symbols":type(prior["remaining_undefined_symbols"]).__name__,
  "prior_symbol_count":len(prior["remaining_undefined_symbols"]),"result":{k:type(v).__name__ for k,v in sample.items()},
  "pin_map":type(pins).__name__,"pin_values":sorted({type(v).__name__ for v in pins.values()}),
  "source_functions":type(source["functions"]).__name__,"source_before_values":type(next(iter(source["native_before"].values()))).__name__,
  "expected_targets":[t for _,t in CLIENTS],"expected_count":len(CLIENTS)}
 print("SCHEMA0404",json.dumps(schema,sort_keys=True),flush=True)
 assert prior["status"]=="AUTHENTICATED_NATIVE0403_RESOURCE_BASE_FRAMES_COMPONENT"
 assert isinstance(prior["remaining_undefined_symbols"],list) and len(prior["remaining_undefined_symbols"])==53
 assert isinstance(pins,dict) and len(pins)==909 and all(sha(ROOT/n)==v for n,v in pins.items())
 baseline=read(prior_dir/"frames-02/SOURCE_FILES.json")
 assert sorted(n for n in set(pins)|set(baseline) if pins.get(n)!=baseline.get(n))==sorted(CODE)
 receipts={};logs={};old_test=sha(R/"test_retail_renderer_streams.before-unwind-case.cpp")
 def run(name,old=False,historical=False):
  folder=R/name;v=read(folder/"RESULT.json");p=read(folder/"SOURCE_FILES.json")
  raw=gzip.decompress((folder/"output.log.gz").read_bytes())
  assert type(v["exit"]) is int and v["source_commit"]==BASE
  assert v["source_manifest_sha256"]==sha(folder/"SOURCE_FILES.json")
  delta=sorted(n for n in set(p)|set(pins) if p.get(n)!=pins.get(n))
  assert delta==([TEST] if old else []),(name,delta)
  if old:assert p[TEST]==old_test
  assert v["runner_sha256"]==sha(R/"run_check.py") and v["cache_sha256"]==sha(ROOT/"b/CMakeCache.txt")
  assert v["compiler_sha256"]==sample["compiler_sha256"] and v["process_temp"]==sample["process_temp"]
  assert v["log_sha256"]==hashlib.sha256(raw).hexdigest()
  assert all(v[k] is False for k in ["game_run","visible_window","audio_endpoint"])
  if name=="negative-01":assert v["status"]=="NONZERO" and v["exit"]!=0
  else:assert (v["status"],v["exit"])==("PASS",0)
  if not historical:assert v["executable_sha256"]==sha(Path(v["argv"][0]))
  receipts[name]={"result_sha256":sha(folder/"RESULT.json"),"source_manifest_sha256":sha(folder/"SOURCE_FILES.json"),
   "allowed_source_difference":delta,"log_sha256":v["log_sha256"],"compressed_log_sha256":sha(folder/"output.log.gz"),
   "argv":v["argv"],"executable_sha256":v["executable_sha256"],"seconds":v["seconds"],
   "binary_authentication":"HISTORICAL_STREAMS01_HASH_ONLY_NOT_RETAINED" if historical else "CURRENT_BYTES_MATCH"}
  logs[name]=raw.decode(errors="replace");return v
 for name,target in CLIENTS:
  v=run(name,name!="streams-02");assert Path(v["argv"][0]).stem==target and v["phase"]=="client"
  assert "PASS" in logs[name] or "tests passed" in logs[name]
  assert "FAIL" not in logs[name]
 build=run("compile-01",True)["argv"];assert build[build.index("--target")+1:build.index("-j2")]==[t for _,t in CLIENTS]
 build=run("compile-02")["argv"];assert build[build.index("--target")+1:build.index("-j2")]==[CLIENTS[0][1]]
 run("streams-01",True,True);run("negative-01");run("symbols-01")
 assert "SKIP symlink creation: 1314" in logs["volume-01"]
 cuts=re.search(r"allocation cuts module=(\d+) stream=(\d+)",logs["streams-02"]);assert cuts
 module_cuts,stream_cuts=map(int,cuts.groups());assert 0<module_cuts<64 and 0<stream_cuts<64
 removed=[n for n in prior["remaining_undefined_symbols"] if
  "renderer_required::construct_core_plugin_0404(" in n or "renderer_required::destroy_core_plugin_0404(" in n]
 missing=re.findall(r"undefined symbol: ([^\r\n]+)",logs["negative-01"])
 assert len(removed)==2 and len(missing)==len(set(missing))==51
 assert sorted(missing)==sorted(n for n in prior["remaining_undefined_symbols"] if n not in removed)
 assert not (ROOT/"b/test_native_startup_missing_bindings.exe").exists()
 symbols=logs["symbols-01"]
 for name in removed+["NativeRetailRendererStreams::open_resource_read(","NativeRetailRendererStreams::read(",
  "NativeRetailRendererStreams::close(","NativeFile::close_checked(","NativeContentFile::close_checked("]:
  assert any(re.search(r"\bT\b",line) and name in line for line in symbols.splitlines()),name
 for name in ["SDL_InitSubSystem","__wrap_fclose","__wrap_fread"]:assert re.search(r"\bT "+name+r"\b",symbols),name
 assert "__imp_SDL_InitSubSystem" not in symbols
 assert not re.search(r"(?<![A-Za-z0-9_])(?:ps2|gs|spu2|sif|Cpu|recomp)::|\b(?:g_ram|g_spr)\b|SDL_OpenAudio",symbols)
 identity=read(ROOT/"research/runs/native-xbox-mode-scene-20261122/MODE_ANCHORS.json")
 image=(PARENT/"private-inputs/xbox-mka-20261121/default.xbe").read_bytes()
 assert hashlib.sha256(image).hexdigest()==source["xbe_sha256"]==identity["xbe_sha256"]
 def original(at,n):
  s=[s for s in identity["sections"] if s["virtual_address"]<=at and at+n<=s["virtual_address"]+s["file_size"]]
  assert len(s)==1;s=s[0];o=s["file_offset"]+at-s["virtual_address"];return image[o:o+n],o
 for entry,row in source["functions"].items():
  path=ROOT/row["listing"];join=Path(str(path.parent)+".join.json")
  assert sha(path)==row["listing_sha256"] and sha(join)==row["join_sha256"] and row["unlisted_gaps"]==[]
  assert (path.parent/"PROGRAM.txt").read_text().splitlines()==["default.xbe",identity["xbe_sha256"],"x86:LE:32:default"]
  cursor=int(row["inclusive_range"][0],16)
  spans=read(join)["functions"][entry]["original_spans"];assert len(spans)==row["instructions"]
  for span in spans:
   at=int(span["vma"],16);assert at==cursor;raw,o=original(at,span["length"])
   assert raw.hex()==span["bytes_hex"] and o==span["file_offset"];cursor+=len(raw)
  assert cursor==int(row["inclusive_range"][1],16)+1
 assert len(source["functions"])==12
 for row in source["data"].values():
  raw,o=original(int(row["vma"],16),row["length"]);assert raw.hex()==row["bytes_hex"] and o==row["file_offset"]
 for n,before in source["native_before"].items():
  raw=gzip.decompress((R/(Path(n).name+".before-0404.gz")).read_bytes())
  assert hashlib.sha256(raw).hexdigest()==before["sha256"]==baseline[n]
 for n,pattern in [
  ("runtime/src/native/native_file.cpp",r"^bool NativeFile::(?:close_checked|end_of_file)\([^\n]*\{\n.*?^}\n"),
  ("runtime/src/native/content_volume.cpp",r"^(?:bool NativeContentFile::close_checked|std::optional<bool> NativeContentFile::end_of_file)\([^\n]*\{\n.*?^}\n")]:
  before=gzip.decompress((R/(Path(n).name+".before-0404.gz")).read_bytes()).decode().replace("\r\n","\n")
  after=(ROOT/n).read_text();stripped,count=re.subn(pattern,"",after,flags=re.M|re.S);assert count==2 and stripped==before,n
 imports=read(R/"NATIVE_FILE_IMPORTS.json")
 assert isinstance(imports["symbols"],list) and {"fclose","fread"}.issubset(imports["symbols"])
 fixture=R/"streams-02/fixtures/data.bin";assert fixture.read_bytes()==b"ABCDEFGHIJ"
 config={}
 for line in (ROOT/"b/CMakeCache.txt").read_text().splitlines():
  if line and not line.startswith(("#","//")) and "=" in line:
   k,v=line.split("=",1)
   if k.startswith(("CMAKE_BUILD_TYPE:","CMAKE_CXX_COMPILER:","CMAKE_CXX_FLAGS","MKA_","PS2_")):config[k]=v
 assert all(b"\r" not in (ROOT/n).read_bytes() for n in CODE)
 git("diff","--check","--",*CODE)
 return {"status":"AUTHENTICATED_NATIVE0404_READ_STREAM_RESOURCE_COMPONENT","schema_preflight":schema,
  "measured_source_base":BASE,"source_files":len(pins),"source_manifest_sha256":sha(R/"streams-02/SOURCE_FILES.json"),
  "changed_source_paths":CODE,"client_targets":[t for _,t in CLIENTS],"client_target_count":len(CLIENTS),
  "module_allocation_cuts":module_cuts,"stream_open_allocation_cuts":stream_cuts,"host_stream_growth":64,
  "receipts":receipts,"resolved_bindings":removed,"remaining_undefined_symbols":missing,
  "compiler_sha256":sample["compiler_sha256"],"cache_sha256":sample["cache_sha256"],"configuration":config,
  "sdl2_sha256":sha(ROOT/"b/SDL2.dll"),"source_receipt_sha256":sha(R/"SOURCE_0404.json"),
  "fixture_bytes_sha256":sha(fixture),"legacy_file_methods_unchanged_normalized_lf":True,
  "preflight_import_object":"HISTORICAL_PREBUILD_IMPORT_SPELLING_ONLY",
  "verification_scope":"tested READ component; precommit build; seven unchanged regressions differ only in unrelated expanded new test",
  "writes_custom_caller_records":"OPEN","source_wrapping_skip":"OPEN_NAMED_REFUSAL","chunk_helpers":"OPEN",
  "volume_symlink_execution":"OPEN_PRIVILEGE1314_SKIP","renderer_gpu":"OPEN","oracle_comparison":"OPEN",
  "game_run":False,"visible_window":False,"audio_endpoint":False,"acceptance":"OPEN"}
def publish(code,message):
 assert git("branch","--show-current")=="main" and git("rev-parse","HEAD")==git("rev-parse","origin/main")
 assert not git("diff","--cached","--name-only")
 changed=git("diff","--name-only").splitlines();assert all(n in code or n.startswith(REL+"/") for n in changed)
 paths=sorted(set(changed+git("ls-files","--others","--exclude-standard","--",REL,*code).splitlines()));assert paths
 for n in paths:
  p=ROOT/n;assert p.is_file() and (n in code or n.startswith(REL+"/")) and "/fixtures/" not in n
  assert p.suffix.lower() not in {".exe",".dll",".ms2",".ms4",".mko",".ssf",".bin",".state",".ps2"}
  if p.suffix in {".py",".md",".json",".hpp",".cpp",".cmake"}:assert b"\r" not in p.read_bytes(),n
 expected=[sha(ROOT/n) for n in paths];git("diff","--check","--",*paths)
 print("START0404 staging",len(paths),"approved paths",flush=True);git("add","--",*paths)
 assert sorted(git("diff","--cached","--name-only").splitlines())==paths
 git("diff","--cached","--check");assert blobs([":"+n for n in paths])==expected
 pins=read(R/"streams-02/SOURCE_FILES.json");assert blobs([":"+n for n in pins])==list(pins.values())
 print("PASS staged paths/bytes/LF/no-assets and909 source identities; START commit",flush=True)
 subprocess.run(["git","-c","core.autocrlf=false","commit","--quiet","-m",message],cwd=ROOT,check=True,timeout=60)
 commit=git("rev-parse","HEAD");assert blobs([commit+":"+n for n in pins])==list(pins.values())
 assert blobs([commit+":"+n for n in paths])==expected
 print("START0404 push",commit,flush=True);subprocess.run(["git","push","origin","main"],cwd=ROOT,check=True,timeout=90)
 assert git("rev-parse","origin/main")==commit;print("PUBLISHED",commit,flush=True);return commit
assert len(sys.argv)==2
if sys.argv[1]=="publish":
 assert git("rev-parse","HEAD")==BASE
 value=authenticate();save("COMPONENT_AUTHENTICATION.json",value)
 print("AUTHENTICATED0404",value["client_target_count"],"clients; exact remaining",len(value["remaining_undefined_symbols"]),flush=True)
 commit=publish(CODE,"Implement native0404 read-stream owner and checked file cleanup")
 save("COMMITTED_AUTHENTICATION.json",{"status":"AUTHENTICATED_COMMITTED_NATIVE0404_READ_RESOURCE",
  "runtime_source_commit":commit,"runtime_tree":git("rev-parse",commit+"^{tree}"),"source_files":value["source_files"],
  "all_measured_source_blobs_match_commit":True,"source_manifest_sha256":sha(R/"streams-02/SOURCE_FILES.json"),
  "component_authentication_sha256":sha(R/"COMPONENT_AUTHENTICATION.json"),"implementation_record_sha256":sha(R/"IMPLEMENTATION.md"),
  "verification_scope":"recorded precommit component executions authenticated against identical committed sources; not a fresh-checkout rebuild",
  "retained_regressions":"seven unchanged binaries/production inputs; unrelated expanded new test differs",
  "read_only_scope":True,"full_stream_renderer_game":"OPEN","fresh_checkout_build":False,"game_run":False,"acceptance":"OPEN"})
elif sys.argv[1]=="finish":
 a=read(R/"COMMITTED_AUTHENTICATION.json");assert git("rev-parse","HEAD")==a["runtime_source_commit"]
 assert a["component_authentication_sha256"]==sha(R/"COMPONENT_AUTHENTICATION.json")
 assert not git("diff","--name-only","--","runtime","tests","cmake","CMakeLists.txt")
 publish([],"Record committed0404 read-stream verification and remaining gates")
else:raise ValueError("publish/finish required")
