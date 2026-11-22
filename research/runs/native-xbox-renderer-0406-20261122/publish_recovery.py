"""Authenticate source-only0406 recovery and publish only its approved evidence."""
from pathlib import Path
import gzip,hashlib,json,struct,subprocess
R=Path(__file__).resolve().parent;ROOT=R.parents[2];PARENT=ROOT.parents[2];REL=R.relative_to(ROOT).as_posix()
BASE="f3fb51973e271c6187d94709915ee7fe976eb200"
RUNTIME="316d5495bb58552dac00394097278fee5b2e8ca3"
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return json.loads(p.read_text())
def save(p,v):
 with p.open("x",newline="\n") as f:json.dump(v,f,indent=2);f.write("\n")
def git(*args):return subprocess.check_output(["git","-c","core.autocrlf=false",*args],cwd=ROOT,timeout=25).decode().strip()
def blobs(revs):
 p=subprocess.run(["git","cat-file","--batch"],cwd=ROOT,input=("\n".join(revs)+"\n").encode(),
  capture_output=True,check=True,timeout=30)
 raw=p.stdout;at=0;out=[]
 for rev in revs:
  end=raw.index(b"\n",at);header=raw[at:end].split();assert len(header)==3 and header[1]==b"blob",(rev,header)
  at=end+1;n=int(header[2]);out.append(hashlib.sha256(raw[at:at+n]).hexdigest())
  at+=n;assert raw[at:at+1]==b"\n";at+=1
 assert at==len(raw);return out
assert git("rev-parse","HEAD")==git("rev-parse","origin/main")==BASE
assert git("branch","--show-current")=="main" and not git("diff","--cached","--name-only")
assert not git("diff","--name-only"),"source recovery must not include a tracked runtime edit"
prior=ROOT/"research/runs/native-xbox-renderer-0405-20261122"
c=read(prior/"COMPONENT_AUTHENTICATION.json");pins=read(prior/"cameras-02/SOURCE_FILES.json")
schema={"prior_status":type(c["status"]).__name__,"prior_symbols":type(c["remaining_undefined_symbols"]).__name__,
 "prior_symbol_items":sorted({type(v).__name__ for v in c["remaining_undefined_symbols"]}),
 "pin_map":type(pins).__name__,"pin_values":sorted({type(v).__name__ for v in pins.values()}),
 "export_result":{k:type(v).__name__ for k,v in read(R/"source-xbox-01.command.json").items()}}
print("SCHEMA0406 recovery",json.dumps(schema,sort_keys=True),flush=True)
assert c["status"]=="AUTHENTICATED_NATIVE0405_CAMERA_BASE_RESOURCE_COMPONENT"
assert isinstance(c["remaining_undefined_symbols"],list) and len(c["remaining_undefined_symbols"])==49
assert sum("core_plugin_0406(" in v for v in c["remaining_undefined_symbols"])==2
assert isinstance(pins,dict) and len(pins)==912 and all(sha(ROOT/n)==v for n,v in pins.items())
assert blobs([BASE+":"+n for n in pins])==blobs([RUNTIME+":"+n for n in pins])==list(pins.values())
ref=read(R/"SESSION_REFERENCE.json");session=Path(ref["session_path"]);assert sha(session)==ref["session_sha256"]
manifest=read(session);assert manifest["masters_modified"] is False and manifest["shared_connector_reused"] is False
for v in manifest["projects"].values():
 master=Path(v["master"]);project=Path(v["project"])
 assert not list(project.parent.glob(project.name+".lock*")) and not list(master.parent.glob(master.name+".lock*"))
 assert all(sha(master.parent/n)==digest for n,digest in v["saved_files_sha256"].items())
images={"xbox":(PARENT/"private-inputs/xbox-mka-20261121/default.xbe").read_bytes(),
 "ps2":(PARENT/"research/inputs/SLUS_214.10").read_bytes()}
selected={"xbox":{"002bce10","002bbc00","002bbe30","002bbe90","002bcfc0","002bc1d0","002bbce0",
 "002bc590","002bc410","002bc2b0","002bbdd0","002bcc40","002bcd60","002bfeb0"},
 "ps2":{"001a7de0","001a8000","001a8190","001a8240","001a82b0","001a8380"}}
gaps_expected={("xbox","002bbdd0",0x2bbdea):("8d9b00000000","LEA EBX,[EBX]; skipped local alignment after JMP2BBDE8"),
 **{("ps2","0011ea20",at):("00000000","SLL zero,zero,0 NOP; after unconditional branch and its already-listed delay slot")
  for at in [0x11ebc4,0x11ec44,0x11ef94]}}
exports={};functions={};seen_gaps=set()
for path in sorted(R.glob("source-*.join.json")):
 name=path.name.removesuffix(".join.json");join=read(path);original=read(R/(name+".original.json"))
 command=read(R/(name+".command.json"));platform=join["platform"];image=images[platform]
 assert (command["exit"],command["timed_out"],command["game_run"])==(0,False,False)
 assert command["runner_sha256"]==sha(R/"export_source.py") and command["script_sha256"]==sha(R/"Export0406.java")
 assert command["session_sha256"]==sha(session)
 assert command["log_sha256"]==hashlib.sha256(gzip.decompress((R/(name+".log.gz")).read_bytes())).hexdigest()
 assert original["image_sha256"]==join["image_sha256"]==hashlib.sha256(image).hexdigest()
 def mapped(at,n):
  candidates=[s for s in original["sections"] if s["virtual_address"]<=at and at+n<=s["virtual_address"]+s["file_size"]]
  assert len(candidates)==1;s=candidates[0];o=s["file_offset"]+at-s["virtual_address"]
  return image[o:o+n],o
 for row in original["queries"].values():
  m=row["mapping"];at=int(m["vma"],16)
  if m["file_backed"]:
   raw,o=mapped(at,m["length"]);assert raw.hex()==row["bytes_hex"] and o==m["file_offset"]
  else:
   assert row["bytes_hex"] is None and m["loader_zero_initialized"] is True
   ss=[s for s in original["sections"] if s["virtual_address"]+s["file_size"]<=at and
    at+m["length"]<=s["virtual_address"]+s["virtual_size"]];assert len(ss)==1
 index={r.split("\t")[0]:r.split("\t") for r in (R/name/"functions.tsv").read_text().splitlines()}
 program=(R/name/"PROGRAM.txt").read_text().splitlines()
 assert program==[manifest["projects"][platform]["program"],original["image_sha256"],manifest["projects"][platform]["language"]]
 for entry,row in join["functions"].items():
  listing=R/name/(entry+".asm");assert sha(listing)==row["listing_sha256"]
  lines=[line.split("\t",2) for line in listing.read_text().splitlines()]
  assert len(lines)==row["instructions"]==int(index[entry][2])
  cursor=int(entry,16)
  for line,span in zip(lines,row["original_spans"]):
   at=int(span["vma"],16);raw,o=mapped(at,span["length"])
   assert at==int(line[0],16) and raw.hex()==span["bytes_hex"]==line[1] and o==span["file_offset"]
   if at!=cursor:
    assert at>cursor;key=(platform,entry,cursor);expected,meaning=gaps_expected[key]
    missing,offset=mapped(cursor,at-cursor);assert missing.hex()==expected
    if platform=="ps2":
     branch,_=mapped(cursor-8,4);w=struct.unpack("<I",branch)[0]
     assert w>>26==4 and ((w>>21)&31)==0 and ((w>>16)&31)==0
    seen_gaps.add(key)
   cursor=at+len(raw)
  assert cursor==int(index[entry][1],16)+1
  key=platform+":"+entry
  assert key not in functions
  functions[key]={"listing":listing.relative_to(ROOT).as_posix(),"listing_sha256":sha(listing),
   "entry":entry,"end_inclusive":index[entry][1],"instructions":row["instructions"],
   "mapping_receipt":path.relative_to(ROOT).as_posix(),"mapping_receipt_sha256":sha(path),
   "scope":"SOURCE_DERIVED_SELECTED_CONTRACT_WITH_RECORDED_LIMITS" if entry in selected[platform] else "CALLER_OR_DEPENDENCY_ORIENTATION_ONLY",
   "compiled":False,"tested_component":False}
 exports[name]={"platform":platform,"command_sha256":sha(R/(name+".command.json")),
  "original_sha256":sha(R/(name+".original.json")),"join_sha256":sha(path),
  "seconds":command["seconds"],"exit":0}
assert len(exports)==12 and seen_gaps==set(gaps_expected)
assert all(platform+":"+entry in functions for platform,entries in selected.items() for entry in entries)
base=PARENT.parent/"dependencies/ghidra/ghidra_12.1.4_PUBLIC/Ghidra/Extensions/ghidra-emotionengine-reloaded"
extension={n:sha(base/n) for n in ["extension.properties","data/languages/emotionengine.ldefs","data/languages/r5900.sla"]}
expected_sizes=[]
for depth,stride,total in [(4,4,72),(8,4,1032),(16,8,16),(24,12,24),(32,12,24)]:
 derived_stride=(((depth+7)//8)*3+3)&~3
 palette=(1<<depth)*4 if depth in [4,8] else 0
 assert derived_stride==stride and derived_stride*2+palette==total
 expected_sizes.append({"width":3,"height":2,"depth":depth,"stride":stride,"pixel_bytes":stride*2,
  "palette_bytes":palette,"total":total,"scope":"SOURCE_DERIVED_EXPECTATION_NOT_A_NATIVE_TEST"})
value={"status":"AUTHENTICATED_SOURCE_DERIVED_0406_RECOVERY_ONLY","schema_preflight":schema,
 "fork_base":BASE,"unchanged_runtime_source":RUNTIME,"unchanged_source_files":len(pins),
 "source_manifest_sha256":sha(prior/"cameras-02/SOURCE_FILES.json"),"all_runtime_source_blobs_unchanged":True,
 "exports":exports,"functions":functions,"image_sha256":{k:hashlib.sha256(v).hexdigest() for k,v in images.items()},
 "decoded_listing_gaps":[{"platform":k[0],"function":k[1],"vma":hex(k[2]),"bytes":v[0],"role":v[1]}
  for k,v in gaps_expected.items()],"ee_extension_available_identity":extension,
 "ee_extension_version":"12.1.4","ee_language_definition_version":"1.4.0",
 "recovery_record_sha256":sha(R/"RECOVERY.md"),"source_derived_extent_expectations":expected_sizes,
 "route_decision":"NO_SUBSTANTIAL_XBOX_SPECIFIC_0406_NATIVE_DEPENDENCY_REDUCTION_ESTABLISHED",
 "recommended_destination":"ONE_SHARED_TYPED_NATIVE_CPU_IMAGE_SERVICE_REUSING_EXISTING_OWNERS",
 "retained_prior_negative_link":{"status":"RETAINED_UNCHANGED_SOURCE_NOT_RERUN",
  "authentication_sha256":sha(prior/"COMPONENT_AUTHENTICATION.json"),"names":c["remaining_undefined_symbols"]},
 "open":["gamma-table quantization and835350 producer/domain","concrete format/extension callbacks and lifetime",
  "native0406 implementation/connected tests","same-scenario oracle comparison","full game acceptance"],
 "native_implementation_changed":False,"component_build_or_test_run":False,"game_run":False,
 "visible_window":False,"audio_endpoint":False,"local_model_used":False,"acceptance":"OPEN"}
save(R/"RECOVERY_AUTHENTICATION.json",value)
paths=sorted(git("ls-files","--others","--exclude-standard","--",REL).splitlines());assert paths
assert all(n.startswith(REL+"/") and "/fixtures/" not in n for n in paths)
for n in paths:
 p=ROOT/n;assert p.suffix.lower() not in {".exe",".dll",".xbe",".elf",".bin",".ps2",".state"}
 if p.suffix.lower() in {".md",".json",".py",".java",".asm",".tsv",".txt"}:assert b"\r" not in p.read_bytes(),n
expected=[sha(ROOT/n) for n in paths];git("diff","--check","--",*paths)
print("PASS0406 source-only recovery;",len(exports),"exports;912 runtime files unchanged; START staging",len(paths),"paths",flush=True)
git("add","--",*paths)
assert sorted(git("diff","--cached","--name-only").splitlines())==paths
git("diff","--cached","--check");assert blobs([":"+n for n in paths])==expected
assert blobs([":"+n for n in pins])==list(pins.values())
subprocess.run(["git","-c","core.autocrlf=false","commit","--quiet","-m",
 "Recover0406 CPU image contracts and compare Xbox/PS2 adaptation"],cwd=ROOT,check=True,timeout=60)
commit=git("rev-parse","HEAD")
assert blobs([commit+":"+n for n in paths])==expected and blobs([commit+":"+n for n in pins])==list(pins.values())
print("START0406 recovery push",commit,flush=True)
subprocess.run(["git","push","origin","main"],cwd=ROOT,check=True,timeout=90)
assert git("rev-parse","origin/main")==commit and not git("status","--porcelain")
print("PUBLISHED0406 source recovery",commit,"runtime unchanged",RUNTIME,flush=True)
