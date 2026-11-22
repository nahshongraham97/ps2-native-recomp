"""Authenticate final0406 source/results and publish only the approved authored batch."""
from pathlib import Path
from fractions import Fraction
import gzip,hashlib,json,re,struct,subprocess,sys
R=Path(__file__).resolve().parent;ROOT=R.parents[2];PARENT=ROOT.parents[2];REL=R.relative_to(ROOT).as_posix()
BASE="77e58be423c3cf62071a3268c45dfb2218d605a4"
CODE=["cmake/native_game.cmake","runtime/include/mka/retail_renderer_core.hpp",
 "runtime/include/mka/retail_renderer_images.hpp","runtime/src/native/retail_renderer_images.cpp","tests/test_retail_renderer_images.cpp"]
CLIENTS=[("images-04","test_native_retail_renderer_images"),("cameras-02","test_native_retail_renderer_cameras"),
 ("streams-02","test_native_retail_renderer_streams"),("frames-02","test_native_retail_renderer_frames"),
 ("core-02","test_native_retail_renderer_core"),("matrix-02","test_native_retail_renderer_matrix"),
 ("registration-02","test_native_retail_renderer_raster_registration"),("allocator-02","test_native_retail_renderer_allocator")]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
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
 prior=ROOT/"research/runs/native-xbox-renderer-0405-20261122"
 old=read(prior/"COMPONENT_AUTHENTICATION.json");pins=read(R/"images-04/SOURCE_FILES.json")
 source=read(R/"SOURCE_IMPLEMENTATION.json");sample=read(R/"images-04/RESULT.json")
 schema={"prior_status":type(old["status"]).__name__,"prior_symbols":type(old["remaining_undefined_symbols"]).__name__,
  "pin_map":type(pins).__name__,"pin_values":sorted({type(v).__name__ for v in pins.values()}),
  "result":{k:type(v).__name__ for k,v in sample.items()},"expected_targets":[t for _,t in CLIENTS],
  "expected_target_count":len(CLIENTS),"negative_plan":{"negative-01":"NONZERO; exact49 minus ONLY0406 constructor/destructor; no executable"}}
 print("SCHEMA0406",json.dumps(schema,sort_keys=True),flush=True)
 assert old["status"]=="AUTHENTICATED_NATIVE0405_CAMERA_BASE_RESOURCE_COMPONENT"
 assert isinstance(old["remaining_undefined_symbols"],list) and len(old["remaining_undefined_symbols"])==49
 assert isinstance(pins,dict) and len(pins)==915 and all(sha(ROOT/n)==v for n,v in pins.items())
 baseline=read(prior/"cameras-02/SOURCE_FILES.json")
 assert sorted(n for n in set(pins)|set(baseline) if pins.get(n)!=baseline.get(n))==sorted(CODE)
 receipts={};logs={}
 for name in [n for n,_ in CLIENTS]+["compile-05","negative-01","symbols-01"]:
  folder=R/name;v=read(folder/"RESULT.json");raw=gzip.decompress((folder/"output.log.gz").read_bytes())
  assert v["source_commit"]==BASE and type(v["exit"]) is int
  assert v["source_manifest_sha256"]==sha(folder/"SOURCE_FILES.json")==sample["source_manifest_sha256"]
  assert read(folder/"SOURCE_FILES.json")==pins and v["runner_sha256"]==sha(R/"run_check.py")
  assert v["cache_sha256"]==sha(ROOT/"b/CMakeCache.txt") and v["compiler_sha256"]==sample["compiler_sha256"]
  assert v["log_sha256"]==hashlib.sha256(raw).hexdigest() and v["executable_sha256"]==sha(Path(v["argv"][0]))
  assert all(v[k] is False for k in ["game_run","visible_window","audio_endpoint"])
  if name=="negative-01":assert v["status"]=="NONZERO" and v["exit"]!=0
  else:assert (v["status"],v["exit"])==("PASS",0)
  receipts[name]={"result_sha256":sha(folder/"RESULT.json"),"source_manifest_sha256":v["source_manifest_sha256"],
   "log_sha256":v["log_sha256"],"compressed_log_sha256":sha(folder/"output.log.gz"),"argv":v["argv"],
   "executable_sha256":v["executable_sha256"],"seconds":v["seconds"]}
  logs[name]=raw.decode(errors="replace")
 for name,target in CLIENTS:
  assert Path(receipts[name]["argv"][0]).stem==target and "PASS" in logs[name] and "FAIL" not in logs[name]
 argv=receipts["compile-05"]["argv"];assert argv[argv.index("--target")+1:argv.index("-j2")]==[t for _,t in CLIENTS]
 removed=[n for n in old["remaining_undefined_symbols"] if "renderer_required::construct_core_plugin_0406(" in n or "renderer_required::destroy_core_plugin_0406(" in n]
 missing=re.findall(r"undefined symbol: ([^\r\n]+)",logs["negative-01"])
 assert len(removed)==2 and len(missing)==len(set(missing))==47
 assert sorted(missing)==sorted(n for n in old["remaining_undefined_symbols"] if n not in removed)
 assert not (ROOT/"b/test_native_startup_missing_bindings.exe").exists()
 for symbol in removed+["NativeRetailRendererImages::create_image(","NativeRetailRendererImages::allocate_pixels(",
  "NativeRetailRendererImages::destroy_image(","NativeRetailRendererImages::read_image("]:
  assert any(re.search(r"\bT\b",line) and symbol in line for line in logs["symbols-01"].splitlines()),symbol
 assert re.search(r"\bT SDL_InitSubSystem\b",logs["symbols-01"]) and "__imp_SDL_InitSubSystem" not in logs["symbols-01"]
 assert not re.search(r"(?<![A-Za-z0-9_])(?:ps2|gs|spu2|sif|Cpu|recomp)::|\b(?:g_ram|g_spr)\b|SDL_OpenAudio",logs["symbols-01"])
 assert source["status"]=="SOURCE_DERIVED_0406_IMPLEMENTATION_SELECTION"
 for path,digest in source["listings"].items():assert sha(ROOT/path)==digest
 for n,digest in source["native_before"].items():
  assert hashlib.sha256(gzip.decompress((R/(Path(n).name+".before-images.gz")).read_bytes())).hexdigest()==digest==baseline[n]
 for name,key in [("device-descriptor.data.json","device_descriptor_sha256"),("device-copy-switch.data.json","device_copy_switch_sha256"),
  ("gamma-one-expected.json","gamma_expected_sha256")]:assert sha(R/name)==source[key]
 gold=read(R/"gamma-one-expected.json")
 assert gold["tables"]==[list(range(256)),list(range(256))] and len(gold["intervals"])==255
 for i,row in enumerate(gold["intervals"],1):assert row["index"]==row["expected"]==i and i<=Fraction(row["lower"])<=Fraction(row["upper"])<i+1
 identity=read(ROOT/"research/runs/native-xbox-mode-scene-20261122/MODE_ANCHORS.json")
 image=(PARENT/"private-inputs/xbox-mka-20261121/default.xbe").read_bytes()
 assert hashlib.sha256(image).hexdigest()==source["input_sha256"]["xbox"]==identity["xbe_sha256"]
 def original(at,n):
  ss=[s for s in identity["sections"] if s["virtual_address"]<=at and at+n<=s["virtual_address"]+s["file_size"]]
  assert len(ss)==1;s=ss[0];offset=s["file_offset"]+at-s["virtual_address"];return image[offset:offset+n],offset
 exports={}
 for join in sorted(R.glob("source-init-*.join.json")):
  name=join.name.removesuffix(".join.json");v=read(join);command=read(R/(name+".command.json"))
  assert v["image_sha256"]==identity["xbe_sha256"] and (command["exit"],command["timed_out"])==(0,False)
  assert command["runner_sha256"]==sha(R/"export_source.py")
  assert command["script_sha256"]==sha(R.parent/"native-xbox-renderer-0406-20261122/Export0406.java")
  assert command["log_sha256"]==hashlib.sha256(gzip.decompress((R/(name+".log.gz")).read_bytes())).hexdigest()
  index={line.split("\t")[0]:line.split("\t") for line in (R/name/"functions.tsv").read_text().splitlines()}
  for entry,row in v["functions"].items():
   assert sha(R/name/(entry+".asm"))==row["listing_sha256"] and row["gaps"]==[]
   cursor=int(entry,16)
   for span in row["original_spans"]:
    at=int(span["vma"],16);raw,o=original(at,span["length"])
    assert at==cursor and raw.hex()==span["bytes_hex"] and o==span["file_offset"];cursor+=len(raw)
   assert cursor==int(index[entry][1],16)+1
  exports[name]={"join_sha256":sha(join),"command_sha256":sha(R/(name+".command.json")),
   "original_sha256":sha(R/(name+".original.json")),"seconds":command["seconds"]}
 assert sorted(exports)==["source-init-"+str(i).zfill(2) for i in range(1,8)]
 for name in ["device-descriptor.data.json","device-copy-switch.data.json"]:
  for row in read(R/name)["data"]:
   raw,o=original(int(row["vma"],16),row["length"]);assert raw.hex()==row["bytes_hex"] and o==row["file_offset"]
 table=bytes.fromhex(read(R/"device-copy-switch.data.json")["data"][0]["bytes_hex"])
 assert struct.unpack("<23I",table)[4]==0x2c74ca
 # Preserve every failed/superseded result and its actually measured test/source bytes.
 snapshots={"test_images.before-matrix-header.cpp":("compile-01",CODE[-1]),
  "retail_renderer_images.before-definition-snapshot.cpp":("images-01",CODE[-2]),
  "test_images.before-failure-extensions.cpp":("images-01",CODE[-1]),
  "test_images.before-normalized-error.cpp":("images-02",CODE[-1]),
  "retail_renderer_images.before-absolute-predicate.cpp":("images-03",CODE[-2]),
  "test_images.before-absolute-predicate.cpp":("images-03",CODE[-1])}
 for path,(run,n) in snapshots.items():assert sha(R/path)==read(R/run/"SOURCE_FILES.json")[n]
 assert read(R/"compile-01/RESULT.json")["status"]=="NONZERO" and read(R/"images-02/RESULT.json")["status"]=="NONZERO"
 cuts=re.search(r"allocation cuts module=(\d+) image=(\d+) pixel=(\d+) path=(\d+) format=(\d+)",logs["images-04"]);assert cuts
 assert tuple(map(int,cuts.groups()))==(9,4,3,2,4)
 assert all(b"\r" not in (ROOT/n).read_bytes() for n in CODE);git("diff","--check","--",*CODE)
 return {"status":"AUTHENTICATED_NATIVE0406_SHARED_CPU_IMAGE_COMPONENT","schema_preflight":schema,
  "measured_source_base":BASE,"source_files":len(pins),"source_manifest_sha256":sha(R/"images-04/SOURCE_FILES.json"),
  "changed_source_paths":CODE,"client_targets":[t for _,t in CLIENTS],"client_target_count":len(CLIENTS),"receipts":receipts,
  "resolved_bindings":removed,"remaining_undefined_symbols":missing,"allocation_cuts":{"module":9,"image":4,"pixel":3,"path":2,"format":4},
  "host_image_growth":64,"independent_gamma_entries":512,"gamma_domain":"exact1.0 only;non1 named unverified",
  "source_selection_sha256":sha(R/"SOURCE_IMPLEMENTATION.json"),"gamma_expected_sha256":sha(R/"gamma-one-expected.json"),
  "gamma_generator_sha256":sha(R/"freeze_source.py"),"source_exports":exports,
  "implementation_record_sha256":sha(R/"IMPLEMENTATION.md"),
  "compiler_sha256":sample["compiler_sha256"],"cache_sha256":sample["cache_sha256"],"sdl2_sha256":sha(ROOT/"b/SDL2.dll"),
  "preserved_snapshot_sha256":{n:sha(R/n) for n in snapshots},
  "verification_scope":"final precommit component executions on current identical bytes;no fresh-checkout rebuild",
  "historical_binaries":"superseded image executables not retained/not authenticated as current",
  "non1_codecs_plugins_write_clone_resampling_gpu_oracle_game":"OPEN","game_run":False,"visible_window":False,"audio_endpoint":False,"acceptance":"OPEN"}
def publish(code,message):
 assert git("branch","--show-current")=="main" and git("rev-parse","HEAD")==git("rev-parse","origin/main")
 assert not git("diff","--cached","--name-only")
 changed=git("diff","--name-only").splitlines();assert all(n in code or n.startswith(REL+"/") for n in changed)
 paths=sorted(set(changed+git("ls-files","--others","--exclude-standard","--",REL,*code).splitlines()));assert paths
 for n in paths:
  p=ROOT/n;assert p.is_file() and (n in code or n.startswith(REL+"/")) and "/fixtures/" not in n
  assert p.suffix.lower() not in {".exe",".dll",".xbe",".elf",".bin",".ps2",".state",".ms2",".ms4"}
  if p.suffix in {".py",".md",".json",".hpp",".cpp",".cmake",".tsv",".asm"}:assert b"\r" not in p.read_bytes(),n
 expected=[sha(ROOT/n) for n in paths];git("diff","--check","--",*paths)
 print("START0406 staging",len(paths),"approved paths",flush=True);git("add","--",*paths)
 assert sorted(git("diff","--cached","--name-only").splitlines())==paths;git("diff","--cached","--check")
 assert blobs([":"+n for n in paths])==expected
 pins=read(R/"images-04/SOURCE_FILES.json");assert blobs([":"+n for n in pins])==list(pins.values())
 subprocess.run(["git","-c","core.autocrlf=false","commit","--quiet","-m",message],cwd=ROOT,check=True,timeout=60)
 commit=git("rev-parse","HEAD");assert blobs([commit+":"+n for n in pins])==list(pins.values()) and blobs([commit+":"+n for n in paths])==expected
 print("START0406 push",commit,flush=True);subprocess.run(["git","push","origin","main"],cwd=ROOT,check=True,timeout=90)
 assert git("rev-parse","origin/main")==commit;print("PUBLISHED",commit,flush=True);return commit
assert len(sys.argv)==2
if sys.argv[1]=="publish":
 assert git("rev-parse","HEAD")==BASE;value=authenticate();save("COMPONENT_AUTHENTICATION.json",value)
 print("AUTHENTICATED0406",value["client_target_count"],"clients;exact remaining",len(value["remaining_undefined_symbols"]),flush=True)
 commit=publish(CODE,"Implement shared native0406 CPU image service with verified gamma1 and callbacks")
 save("COMMITTED_AUTHENTICATION.json",{"status":"AUTHENTICATED_COMMITTED_NATIVE0406_SHARED_CPU_IMAGES",
  "runtime_source_commit":commit,"runtime_tree":git("rev-parse",commit+"^{tree}"),"source_files":value["source_files"],
  "all_measured_source_blobs_match_commit":True,"source_manifest_sha256":sha(R/"images-04/SOURCE_FILES.json"),
  "component_authentication_sha256":sha(R/"COMPONENT_AUTHENTICATION.json"),"implementation_record_sha256":sha(R/"IMPLEMENTATION.md"),
  "verification_scope":value["verification_scope"],"fresh_checkout_build":False,"game_run":False,"acceptance":"OPEN"})
elif sys.argv[1]=="finish":
 a=read(R/"COMMITTED_AUTHENTICATION.json");assert git("rev-parse","HEAD")==a["runtime_source_commit"]
 assert a["component_authentication_sha256"]==sha(R/"COMPONENT_AUTHENTICATION.json") and a["implementation_record_sha256"]==sha(R/"IMPLEMENTATION.md")
 assert not git("diff","--name-only","--","runtime","tests","cmake","CMakeLists.txt")
 publish([],"Record committed0406 image-service verification and open renderer gates")
else:raise ValueError("publish/finish required")
