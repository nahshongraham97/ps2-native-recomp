"""Authenticate0405 source/results and commit/push exact approved bytes, fail closed."""
from pathlib import Path
import gzip,hashlib,json,re,subprocess,sys
R=Path(__file__).resolve().parent;ROOT=R.parents[2];PARENT=ROOT.parents[2];REL=R.relative_to(ROOT).as_posix()
BASE="0218826aa8c7cd2d80f5787edfaf491dd5352dbd"
CODE=["cmake/native_game.cmake","runtime/include/mka/retail_renderer_core.hpp",
 "runtime/include/mka/retail_renderer_cameras.hpp","runtime/src/native/retail_renderer_cameras.cpp",
 "tests/test_retail_renderer_cameras.cpp"]
CPP=CODE[-2]
CLIENTS=[("cameras-02","test_native_retail_renderer_cameras"),("streams-02","test_native_retail_renderer_streams"),
 ("frames-02","test_native_retail_renderer_frames"),("core-02","test_native_retail_renderer_core"),
 ("matrix-02","test_native_retail_renderer_matrix"),("registration-02","test_native_retail_renderer_raster_registration"),
 ("allocator-02","test_native_retail_renderer_allocator")]
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
  at=end+1;n=int(h[2]);out.append(hashlib.sha256(raw[at:at+n]).hexdigest());at+=n;assert raw[at:at+1]==b"\n";at+=1
 assert at==len(raw);return out
def authenticate():
 prior_dir=ROOT/"research/runs/native-xbox-renderer-0404-20261122"
 prior=read(prior_dir/"COMPONENT_AUTHENTICATION.json");source=read(R/"SOURCE_0405.json")
 sample=read(R/"cameras-02/RESULT.json");pins=read(R/"cameras-02/SOURCE_FILES.json")
 schema={"prior_status":type(prior["status"]).__name__,"prior_symbols":type(prior["remaining_undefined_symbols"]).__name__,
  "prior_symbol_count":len(prior["remaining_undefined_symbols"]),"result":{k:type(v).__name__ for k,v in sample.items()},
  "pin_map":type(pins).__name__,"pin_values":sorted({type(v).__name__ for v in pins.values()}),
  "source_functions":type(source["functions"]).__name__,"depth_cases":type(source["depth_cases"]).__name__,
  "expected_targets":[t for _,t in CLIENTS],"expected_count":len(CLIENTS)}
 print("SCHEMA0405",json.dumps(schema,sort_keys=True),flush=True)
 assert prior["status"]=="AUTHENTICATED_NATIVE0404_READ_STREAM_RESOURCE_COMPONENT"
 assert isinstance(prior["remaining_undefined_symbols"],list) and len(prior["remaining_undefined_symbols"])==51
 assert isinstance(pins,dict) and len(pins)==912 and all(sha(ROOT/n)==v for n,v in pins.items())
 baseline=read(prior_dir/"streams-02/SOURCE_FILES.json")
 assert sorted(n for n in set(pins)|set(baseline) if pins.get(n)!=baseline.get(n))==sorted(CODE)
 receipts={};logs={}
 def run(name):
  folder=R/name;v=read(folder/"RESULT.json");raw=gzip.decompress((folder/"output.log.gz").read_bytes())
  assert type(v["exit"]) is int and v["source_commit"]==BASE
  assert read(folder/"SOURCE_FILES.json")==pins and v["source_manifest_sha256"]==sha(folder/"SOURCE_FILES.json")
  assert v["runner_sha256"]==sha(R/"run_check.py") and v["cache_sha256"]==sha(ROOT/"b/CMakeCache.txt")
  assert v["compiler_sha256"]==sample["compiler_sha256"] and v["process_temp"]==sample["process_temp"]
  assert v["log_sha256"]==hashlib.sha256(raw).hexdigest() and v["executable_sha256"]==sha(Path(v["argv"][0]))
  assert all(v[k] is False for k in ["game_run","visible_window","audio_endpoint"])
  if name=="negative-01":assert v["status"]=="NONZERO" and v["exit"]!=0
  else:assert (v["status"],v["exit"])==("PASS",0)
  receipts[name]={"result_sha256":sha(folder/"RESULT.json"),"source_manifest_sha256":sha(folder/"SOURCE_FILES.json"),
   "log_sha256":v["log_sha256"],"compressed_log_sha256":sha(folder/"output.log.gz"),
   "argv":v["argv"],"executable_sha256":v["executable_sha256"],"seconds":v["seconds"]}
  logs[name]=raw.decode(errors="replace");return v
 for name,target in CLIENTS:
  v=run(name);assert Path(v["argv"][0]).stem==target and v["phase"]=="client"
  assert "PASS" in logs[name] and "FAIL" not in logs[name]
 build=run("compile-02")["argv"]
 assert build[build.index("--target")+1:build.index("-j2")]==[t for _,t in CLIENTS]
 run("negative-01");run("symbols-01")
 removed=[n for n in prior["remaining_undefined_symbols"] if
  "renderer_required::construct_core_plugin_0405(" in n or "renderer_required::destroy_core_plugin_0405(" in n]
 missing=re.findall(r"undefined symbol: ([^\r\n]+)",logs["negative-01"])
 assert len(removed)==2 and len(missing)==len(set(missing))==49
 assert sorted(missing)==sorted(n for n in prior["remaining_undefined_symbols"] if n not in removed)
 assert not (ROOT/"b/test_native_startup_missing_bindings.exe").exists()
 symbols=logs["symbols-01"]
 for name in removed+["NativeRetailRendererCameraModule::create_base_camera(",
  "NativeRetailRendererCameraModule::set_view_window(","NativeRetailRendererCameraModule::destroy_base_camera("]:
  assert any(re.search(r"\bT\b",line) and name in line for line in symbols.splitlines()),name
 assert re.search(r"\bT SDL_InitSubSystem\b",symbols) and "__imp_SDL_InitSubSystem" not in symbols
 assert not re.search(r"(?<![A-Za-z0-9_])(?:ps2|gs|spu2|sif|Cpu|recomp)::|\b(?:g_ram|g_spr)\b|SDL_OpenAudio",symbols)
 identity=read(ROOT/"research/runs/native-xbox-mode-scene-20261122/MODE_ANCHORS.json")
 image=(PARENT/"private-inputs/xbox-mka-20261121/default.xbe").read_bytes()
 assert hashlib.sha256(image).hexdigest()==source["xbe_sha256"]==identity["xbe_sha256"]
 def original(at,n):
  ss=[s for s in identity["sections"] if s["virtual_address"]<=at and at+n<=s["virtual_address"]+s["file_size"]]
  assert len(ss)==1;s=ss[0];o=s["file_offset"]+at-s["virtual_address"];return image[o:o+n],o
 for entry,row in source["functions"].items():
  path=ROOT/row["listing"];join=Path(str(path.parent)+".join.json")
  assert sha(path)==row["listing_sha256"] and sha(join)==row["join_sha256"] and row["unlisted_gaps"]==[]
  assert (path.parent/"PROGRAM.txt").read_text().splitlines()==["default.xbe",identity["xbe_sha256"],"x86:LE:32:default"]
  cursor=int(row["inclusive_range"][0],16);spans=read(join)["functions"][entry]["original_spans"]
  assert len(spans)==row["instructions"]
  for span in spans:
   at=int(span["vma"],16);assert at==cursor;raw,o=original(at,span["length"])
   assert raw.hex()==span["bytes_hex"] and o==span["file_offset"];cursor+=len(raw)
  assert cursor==int(row["inclusive_range"][1],16)+1
 assert source["status"]=="SOURCE_DERIVED_0405_CAMERA_RESOURCE_BASE_SCOPE" and len(source["functions"])==6
 for row in source["data"].values():
  raw,o=original(int(row["vma"],16),row["length"]);assert raw.hex()==row["bytes_hex"] and o==row["file_offset"]
 for n,digest in source["native_before"].items():
  assert hashlib.sha256(gzip.decompress((R/(Path(n).name+".before-0405.gz")).read_bytes())).hexdigest()==digest==baseline[n]
 corpus=[list(map(int,line.split())) for line in (R/"depth-expected.tsv").read_text().splitlines()[1:]]
 assert len(corpus)==7 and corpus==[list(row.values()) for row in source["depth_cases"]]
 before=(R/"retail_renderer_cameras.before-window-order.cpp").read_bytes()
 old=b" std::memcpy(&r->state.view_window,&input,sizeof(input));\n const auto one=value(0x3f800000u);\n r->state.inverse_window={div32(one,input.x),div32(one,input.y)};return true;\n"
 new=b" const auto one=value(0x3f800000u);\n std::memcpy(&r->state.view_window.x,&input.x,sizeof(input.x));\n const auto inverse_x=div32(one,input.x);\n std::memcpy(&r->state.view_window.y,&input.y,sizeof(input.y));\n const auto inverse_y=div32(one,input.y);\n r->state.inverse_window={inverse_x,inverse_y};return true;\n"
 assert before.count(old)==1 and before.replace(old,new)==(ROOT/CPP).read_bytes()
 historical=read(R/"cameras-01/SOURCE_FILES.json");assert historical[CPP]==hashlib.sha256(before).hexdigest()
 assert sorted(n for n in pins if pins[n]!=historical[n])==[CPP]
 cuts=re.search(r"allocation cuts module=(\d+) camera=(\d+)",logs["cameras-02"]);assert cuts
 module_cuts,camera_cuts=map(int,cuts.groups());assert 0<module_cuts<16 and 0<camera_cuts<16
 config={}
 for line in (ROOT/"b/CMakeCache.txt").read_text().splitlines():
  if line and not line.startswith(("#","//")) and "=" in line:
   k,v=line.split("=",1)
   if k.startswith(("CMAKE_BUILD_TYPE:","CMAKE_CXX_COMPILER:","CMAKE_CXX_FLAGS","MKA_","PS2_")):config[k]=v
 assert all(b"\r" not in (ROOT/n).read_bytes() for n in CODE);git("diff","--check","--",*CODE)
 return {"status":"AUTHENTICATED_NATIVE0405_CAMERA_BASE_RESOURCE_COMPONENT","schema_preflight":schema,
  "measured_source_base":BASE,"source_files":len(pins),"source_manifest_sha256":sha(R/"cameras-02/SOURCE_FILES.json"),
  "changed_source_paths":CODE,"client_targets":[t for _,t in CLIENTS],"client_target_count":len(CLIENTS),
  "module_allocation_cuts":module_cuts,"camera_allocation_cuts":camera_cuts,"host_camera_growth":64,"independent_depth_cases":7,
  "receipts":receipts,"resolved_bindings":removed,"remaining_undefined_symbols":missing,"configuration":config,
  "compiler_sha256":sample["compiler_sha256"],"cache_sha256":sample["cache_sha256"],"sdl2_sha256":sha(ROOT/"b/SDL2.dll"),
  "source_receipt_sha256":sha(R/"SOURCE_0405.json"),"depth_corpus_sha256":sha(R/"depth-expected.tsv"),
  "depth_generator_sha256":sha(R/"freeze_source.py"),
  "historical01":"superseded before-window-order source/logs retained; old cameras01 binary not retained/current-authenticated",
  "verification_scope":"canonical02 compiled/tested component on precommit bytes; base/initial-empty-registry/unattached scope",
  "production_depth_provider":"OPEN_EXPLICIT_INPUT_REQUIRED","camera_plugins_frame_raster_sync":"OPEN","device_begin_end":"OPEN",
  "numerical_hd_oracle_game":"OPEN","game_run":False,"visible_window":False,"audio_endpoint":False,"acceptance":"OPEN"}
def publish(code,message):
 assert git("branch","--show-current")=="main" and git("rev-parse","HEAD")==git("rev-parse","origin/main")
 assert not git("diff","--cached","--name-only")
 changed=git("diff","--name-only").splitlines();assert all(n in code or n.startswith(REL+"/") for n in changed)
 paths=sorted(set(changed+git("ls-files","--others","--exclude-standard","--",REL,*code).splitlines()));assert paths
 for n in paths:
  p=ROOT/n;assert p.is_file() and (n in code or n.startswith(REL+"/")) and "/fixtures/" not in n
  assert p.suffix.lower() not in {".exe",".dll",".ms2",".ms4",".mko",".ssf",".bin",".state",".ps2"}
  if p.suffix in {".py",".md",".json",".hpp",".cpp",".cmake",".tsv"}:assert b"\r" not in p.read_bytes(),n
 expected=[sha(ROOT/n) for n in paths];git("diff","--check","--",*paths)
 print("START0405 staging",len(paths),"approved paths",flush=True);git("add","--",*paths)
 assert sorted(git("diff","--cached","--name-only").splitlines())==paths
 git("diff","--cached","--check");assert blobs([":"+n for n in paths])==expected
 pins=read(R/"cameras-02/SOURCE_FILES.json");assert blobs([":"+n for n in pins])==list(pins.values())
 print("PASS staged paths/bytes/LF/no-assets and912 source identities; START commit",flush=True)
 subprocess.run(["git","-c","core.autocrlf=false","commit","--quiet","-m",message],cwd=ROOT,check=True,timeout=60)
 commit=git("rev-parse","HEAD");assert blobs([commit+":"+n for n in pins])==list(pins.values())
 assert blobs([commit+":"+n for n in paths])==expected
 print("START0405 push",commit,flush=True);subprocess.run(["git","push","origin","main"],cwd=ROOT,check=True,timeout=90)
 assert git("rev-parse","origin/main")==commit;print("PUBLISHED",commit,flush=True);return commit
assert len(sys.argv)==2
if sys.argv[1]=="publish":
 assert git("rev-parse","HEAD")==BASE
 value=authenticate();save("COMPONENT_AUTHENTICATION.json",value)
 print("AUTHENTICATED0405",value["client_target_count"],"clients; exact remaining",len(value["remaining_undefined_symbols"]),flush=True)
 commit=publish(CODE,"Implement verified native0405 camera resource and base defaults")
 save("COMMITTED_AUTHENTICATION.json",{"status":"AUTHENTICATED_COMMITTED_NATIVE0405_CAMERA_RESOURCE",
  "runtime_source_commit":commit,"runtime_tree":git("rev-parse",commit+"^{tree}"),"source_files":value["source_files"],
  "all_measured_source_blobs_match_commit":True,"source_manifest_sha256":sha(R/"cameras-02/SOURCE_FILES.json"),
  "component_authentication_sha256":sha(R/"COMPONENT_AUTHENTICATION.json"),"implementation_record_sha256":sha(R/"IMPLEMENTATION.md"),
  "verification_scope":"canonical02 precommit component executions authenticated against identical committed source; not a fresh-checkout rebuild",
  "base_camera_scope":True,"full_camera_renderer_game":"OPEN","fresh_checkout_build":False,"game_run":False,"acceptance":"OPEN"})
elif sys.argv[1]=="finish":
 a=read(R/"COMMITTED_AUTHENTICATION.json");assert git("rev-parse","HEAD")==a["runtime_source_commit"]
 assert a["component_authentication_sha256"]==sha(R/"COMPONENT_AUTHENTICATION.json")
 assert not git("diff","--name-only","--","runtime","tests","cmake","CMakeLists.txt")
 publish([],"Record committed0405 camera-resource verification and open renderer gates")
else:raise ValueError("publish/finish required")
