"""Authenticate/publish0407 source recovery only;all native runtime source must remain unchanged."""
from pathlib import Path
import gzip,hashlib,json,struct,subprocess
R=Path(__file__).resolve().parent;ROOT=R.parents[2];PARENT=ROOT.parents[2];REL=R.relative_to(ROOT).as_posix()
BASE="dedae248d0761a105483e932466edfd53d53d0a0"
RUNTIME="2f9ea3c1ddceb1b144f4ea812129cc49e7881d08"
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
assert git("branch","--show-current")=="main" and git("rev-parse","HEAD")==git("rev-parse","origin/main")==BASE
assert not git("diff","--cached","--name-only") and not git("diff","--name-only")
prior=ROOT/"research/runs/native-xbox-renderer-0406-implementation-20261122"
c=read(prior/"COMPONENT_AUTHENTICATION.json");pins=read(prior/"images-04/SOURCE_FILES.json")
sample=read(R/"source-xbox-01.command.json")
schema={"prior_status":type(c["status"]).__name__,"prior_names":type(c["remaining_undefined_symbols"]).__name__,
 "pin_map":type(pins).__name__,"pin_values":sorted({type(v).__name__ for v in pins.values()}),
 "export_result":{k:type(v).__name__ for k,v in sample.items()},
 "scope":"source-derived recovery;no component build/test or new negative-link execution"}
print("SCHEMA0407",json.dumps(schema,sort_keys=True),flush=True)
assert c["status"]=="AUTHENTICATED_NATIVE0406_SHARED_CPU_IMAGE_COMPONENT"
assert isinstance(c["remaining_undefined_symbols"],list) and len(c["remaining_undefined_symbols"])==47
assert sum("core_plugin_0407(" in v for v in c["remaining_undefined_symbols"])==2
assert isinstance(pins,dict) and len(pins)==915 and all(sha(ROOT/n)==v for n,v in pins.items())
assert blobs([BASE+":"+n for n in pins])==blobs([RUNTIME+":"+n for n in pins])==list(pins.values())
old=R.parent/"native-xbox-renderer-0406-20261122"
ref=read(old/"SESSION_REFERENCE.json");session=Path(ref["session_path"]);assert sha(session)==ref["session_sha256"]
manifest=read(session)
for v in manifest["projects"].values():
 project=Path(v["project"]);assert not list(project.parent.glob(project.name+".lock*"))
images={"xbox":(PARENT/"private-inputs/xbox-mka-20261121/default.xbe").read_bytes(),
 "ps2":(PARENT/"research/inputs/SLUS_214.10").read_bytes()}
exports={};functions={}
scalar={"002b9f70","002b9f10","002b9e50","002b9c70","002b9ae0","002b9b60",
 "002b9cb0","002b9d50","002b9df0","002c9810","002d03b0","002c0e90","002d04e0","002d04c0","002b9d90"}
for join in sorted(R.glob("source-*.join.json")):
 name=join.name.removesuffix(".join.json");v=read(join);o=read(R/(name+".original.json"));command=read(R/(name+".command.json"))
 platform=v["platform"];image=images[platform]
 assert (command["exit"],command["timed_out"],command["game_run"])==(0,False,False)
 runner=R/("export_system_span.py" if name=="source-ps2-08" else "export_source.py")
 script=R/"Export0407SystemSpan.java" if name=="source-ps2-08" else old/"Export0406.java"
 assert command["runner_sha256"]==sha(runner) and command["script_sha256"]==sha(script)
 assert command["session_sha256"]==sha(session)
 assert command["log_sha256"]==hashlib.sha256(gzip.decompress((R/(name+".log.gz")).read_bytes())).hexdigest()
 assert o["image_sha256"]==v["image_sha256"]==hashlib.sha256(image).hexdigest()==manifest["projects"][platform]["executable_sha256"]
 def mapped(at,n):
  ss=[s for s in o["sections"] if s["virtual_address"]<=at and at+n<=s["virtual_address"]+s["file_size"]]
  assert len(ss)==1;s=ss[0];offset=s["file_offset"]+at-s["virtual_address"];return image[offset:offset+n],offset
 for row in o["queries"].values():
  m=row["mapping"];at=int(m["vma"],16)
  if m["file_backed"]:
   raw,offset=mapped(at,m["length"]);assert raw.hex()==row["bytes_hex"] and offset==m["file_offset"]
  else:
   assert row["bytes_hex"] is None and m["loader_zero_initialized"] is True
   assert len([s for s in o["sections"] if s["virtual_address"]+s["file_size"]<=at and at+m["length"]<=s["virtual_address"]+s["virtual_size"]])==1
 assert (R/name/"PROGRAM.txt").read_text().splitlines()==[manifest["projects"][platform]["program"],o["image_sha256"],manifest["projects"][platform]["language"]]
 index={line.split("\t")[0]:line.split("\t") for line in (R/name/"functions.tsv").read_text().splitlines()}
 for entry,row in v["functions"].items():
  path=R/name/(entry+".asm");assert sha(path)==row["listing_sha256"]
  lines=[line.split("\t",2) for line in path.read_text().splitlines()]
  assert len(lines)==row["instructions"]==len(row["original_spans"])==int(index[entry][2])
  cursor=int(entry,16);gaps=[]
  for line,span in zip(lines,row["original_spans"]):
   at=int(span["vma"],16);raw,offset=mapped(at,span["length"])
   assert at==int(line[0],16) and raw.hex()==span["bytes_hex"]==line[1] and offset==span["file_offset"]
   if at!=cursor:
    assert at>cursor;missing,off=mapped(cursor,at-cursor)
    role="OPEN_NOT_FULL_SEMANTICS"
    if platform=="xbox":
     assert entry=="002c8f70" and cursor==0x2c902d and missing.hex()=="8d4900"
     role="LEA ECX,[ECX];alignment skipped byJMP2C902B->2C9030"
    gaps.append(dict(vma=hex(cursor),file_offset=off,bytes_hex=missing.hex(),role=role))
   cursor=at+len(raw)
  assert cursor==int(index[entry][1],16)+1
  scope="SOURCE_DERIVED_SCALAR_CONTRACT" if platform=="xbox" and entry in scalar else "BOUNDARY_OR_DEPENDENCY_ONLY_HELPERS_OPEN"
  functions[name+"/"+entry]={"platform":platform,"entry":entry,"end_inclusive":index[entry][1],
   "instructions":row["instructions"],"listing":path.relative_to(ROOT).as_posix(),"listing_sha256":sha(path),
   "join_sha256":sha(join),"gaps":gaps,"scope":scope,"compiled":False,"tested_component":False}
 exports[name]={"platform":platform,"command_sha256":sha(R/(name+".command.json")),"original_sha256":sha(R/(name+".original.json")),
  "join_sha256":sha(join),"seconds":command["seconds"],"exit":0}
expected=sorted(["source-xbox-"+str(i).zfill(2) for i in range(1,12)]+["source-ps2-"+str(i).zfill(2) for i in range(1,11)])
assert sorted(exports)==expected and len(exports)==21
assert functions["source-ps2-06/001b9cf0"]["instructions"]==21
assert functions["source-ps2-08/001b9cf0"]["instructions"]==262 and not functions["source-ps2-08/001b9cf0"]["gaps"]
for name in ["module-reference-candidates","raster-dispatch-data","format-tables"]:
 data=read(R/(name+".original-data.json"));assert data["xbe_sha256"]==manifest["projects"]["xbox"]["executable_sha256"]
 for q in data["queries"]:
  items=q["candidates"] if q["mode"]=="refs" else [q]
  for row in items:
   assert images["xbox"][row["file_offset"]:row["file_offset"]+row["length"]].hex()==row["bytes_hex"]
ps_table=bytes.fromhex(read(R/"source-ps2-09.original.json")["queries"]["data=005a4050:224"]["bytes_hex"])
words=struct.unpack("<56I",ps_table);pairs=list(zip(words[::2],words[1::2]));standards=dict(pairs)
assert len(pairs)==28 and all(standards[k]==v for k,v in {4:0x1b34c0,5:0x1b53d0,7:0x1be5d0,15:0x1b6050,16:0x1b6bf0}.items())
switch=bytes.fromhex(read(R/"source-ps2-07.original.json")["queries"]["data=005fd160:92"]["bytes_hex"])
assert struct.unpack("<23I",switch)[11]==0x1ba0c0
native_paths=["runtime/include/mka/native_gl_texture.hpp","runtime/src/native/movie_gl_device.cpp",
 "runtime/include/mka/retail_loaded_system_art.hpp","runtime/include/mka/retail_renderer_images.hpp"]
assert all(n in pins for n in native_paths)
colors=[]
for rgba,word,opaque in [((0x12,0x34,0x56,0x78),0x78123456,0xff123456),((0,0,0,0),0,0xff000000),
 ((255,0,128,255),0xffff0080,0xffff0080),((1,2,3,128),0x80010203,0xff010203)]:
 r,g,b,a=rgba;assert (a<<24|r<<16|g<<8|b)==word and (0xff000000|r<<16|g<<8|b)==opaque
 colors.append({"rgba":list(rgba),"format0500_word":hex(word),"format0600_word":hex(opaque),"scope":"SOURCE_DERIVED_EXPECTATION_NOT_NATIVE_TEST"})
value={"status":"AUTHENTICATED_SOURCE_DERIVED_0407_RASTER_RECOVERY_ONLY","schema_preflight":schema,
 "fork_base":BASE,"unchanged_runtime_source":RUNTIME,"unchanged_source_files":len(pins),"all_runtime_source_blobs_unchanged":True,
 "source_manifest_sha256":sha(prior/"images-04/SOURCE_FILES.json"),"image_sha256":{k:hashlib.sha256(v).hexdigest() for k,v in images.items()},
 "exports":exports,"functions":functions,"ps2_standard_pairs":pairs,"source_derived_color_expectations":colors,
 "recovery_record_sha256":sha(R/"RECOVERY.md"),"reused_native_services":{n:pins[n] for n in native_paths},
 "route_decision":"COMMON_TYPED_OWNERSHIP_AND_XBOX_SCALAR_FORMAT_BRIDGE_REUSE_EXISTING_NATIVE_GPU_NOT_EITHER_CONSOLE_BACKEND",
 "advantage_scope":"avoids adoptingEE packed backing descriptors;Xbox headers/aligned memory also discarded;no measured whole0407 speedup",
 "sparse_coverage":"oldPS2 system21 instructions preserved;new262-word span closes switch coverage;PS2 backing-create omitted words/helpers OPEN",
 "retained_prior_negative_link":{"status":"RETAINED_UNCHANGED_SOURCE_NOT_RERUN","authentication_sha256":sha(prior/"COMPONENT_AUTHENTICATION.json"),"names":c["remaining_undefined_symbols"]},
 "open":["0407 native implementation/connected tests","full platform format/helper/asset behavior","native targets/depth/views/locks/readback/mips",
  "parent/view retirement and unknown GPU deletion outcomes","same-scenario oracle and game acceptance"],
 "native_implementation_changed":False,"component_build_or_test_run":False,"game_run":False,"visible_window":False,"audio_endpoint":False,
 "local_model_used":False,"owner_action":"none","agent_next":"implement0407 real ownership/context and connected verified native backing path","acceptance":"OPEN"}
save("RECOVERY_AUTHENTICATION.json",value)
paths=sorted(git("ls-files","--others","--exclude-standard","--",REL).splitlines());assert paths
for n in paths:
 p=ROOT/n;assert n.startswith(REL+"/") and p.suffix.lower() not in {".exe",".dll",".xbe",".elf",".bin",".ps2",".state",".ms2",".ms4"}
 if p.suffix.lower() in {".md",".json",".py",".java",".asm",".tsv",".txt"}:assert b"\r" not in p.read_bytes(),n
expected=[sha(ROOT/n) for n in paths];git("diff","--check","--",*paths)
print("PASS0407 source-only recovery;915 native files unchanged;START staging",len(paths),"approved paths",flush=True)
git("add","--",*paths)
assert sorted(git("diff","--cached","--name-only").splitlines())==paths;git("diff","--cached","--check")
assert blobs([":"+n for n in paths])==expected and blobs([":"+n for n in pins])==list(pins.values())
subprocess.run(["git","-c","core.autocrlf=false","commit","--quiet","-m","Recover0407 raster contracts and evidence-based native donor route"],cwd=ROOT,check=True,timeout=60)
commit=git("rev-parse","HEAD")
assert blobs([commit+":"+n for n in paths])==expected and blobs([commit+":"+n for n in pins])==list(pins.values())
print("START0407 recovery push",commit,flush=True);subprocess.run(["git","push","origin","main"],cwd=ROOT,check=True,timeout=90)
assert git("rev-parse","origin/main")==commit and not git("status","--porcelain")
print("PUBLISHED0407 source recovery",commit,"runtime unchanged",RUNTIME,flush=True)
