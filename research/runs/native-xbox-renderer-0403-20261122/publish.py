"""Validate exact staged0403 bytes, commit and push in one fail-closed process."""
from pathlib import Path
import hashlib,json,runpy,subprocess,sys
R=Path(__file__).resolve().parent;ROOT=R.parents[2];REL=R.relative_to(ROOT).as_posix()
V=runpy.run_path(str(R/"authenticate.py"));CODE=V["CODE"]
BASE="91aefc25a8d49111c07b63cf3aaf992ee93cf0df"
def sha(p):
 with p.open("rb") as f:return hashlib.file_digest(f,"sha256").hexdigest()
def read(p):return json.loads(p.read_text())
def git(*a):return subprocess.check_output(["git","-c","core.autocrlf=false",*a],cwd=ROOT,timeout=25).decode().strip()
def blobs(revs):
 p=subprocess.run(["git","cat-file","--batch"],cwd=ROOT,input=("\n".join(revs)+"\n").encode(),
  capture_output=True,check=True,timeout=30)
 raw=p.stdout;cursor=0;out=[]
 for rev in revs:
  end=raw.index(b"\n",cursor);head=raw[cursor:end].split();assert len(head)==3 and head[1]==b"blob",(rev,head)
  cursor=end+1;size=int(head[2]);out.append(hashlib.sha256(raw[cursor:cursor+size]).hexdigest());cursor+=size
  assert raw[cursor:cursor+1]==b"\n";cursor+=1
 assert cursor==len(raw)
 return out
def save(name,value):
 with (R/name).open("x",newline="\n") as f:json.dump(value,f,indent=2);f.write("\n")
def publish(code,message):
 assert git("branch","--show-current")=="main" and git("rev-parse","HEAD")==git("rev-parse","origin/main")
 assert not git("diff","--cached","--name-only")
 changed=git("diff","--name-only").splitlines();assert all(n in code or n.startswith(REL+"/") for n in changed)
 extra=git("ls-files","--others","--exclude-standard","--",REL,*code).splitlines()
 paths=sorted(set(changed+extra));assert paths
 for n in paths:
  p=ROOT/n;assert p.is_file() and (n in code or n.startswith(REL+"/"))
  assert p.suffix.lower() not in {".exe",".dll",".ms2",".ms4",".mko",".ssf",".bin",".state",".ps2"}
  if p.suffix in {".py",".md",".json",".hpp",".cpp",".cmake",".java"}:assert b"\r" not in p.read_bytes(),n
 expected=[sha(ROOT/n) for n in paths];git("diff","--check","--",*paths)
 print("START0403 staging",len(paths),"approved authored paths",flush=True)
 git("add","--",*paths)
 assert sorted(git("diff","--cached","--name-only").splitlines())==paths
 git("diff","--cached","--check");assert blobs([":"+n for n in paths])==expected
 pins=read(R/"frames-02/SOURCE_FILES.json")
 assert blobs([":"+n for n in pins])==list(pins.values())
 assert all(sha(ROOT/n)==v for n,v in zip(paths,expected))
 print("PASS staged paths/bytes/LF/no-assets and906 source identities; START commit",flush=True)
 subprocess.run(["git","-c","core.autocrlf=false","commit","--quiet","-m",message],cwd=ROOT,check=True,timeout=60)
 commit=git("rev-parse","HEAD")
 assert blobs([commit+":"+n for n in pins])==list(pins.values())
 assert blobs([commit+":"+n for n in paths])==expected
 print("START0403 push",commit,flush=True)
 subprocess.run(["git","push","origin","main"],cwd=ROOT,check=True,timeout=90)
 assert git("rev-parse","origin/main")==commit
 print("PUBLISHED",commit,flush=True);return commit
assert len(sys.argv)==2 and sys.argv[1] in {"publish","finish"}
if sys.argv[1]=="publish":
 assert git("rev-parse","HEAD")==BASE
 actual=V["authenticate"]();assert actual==read(R/"COMPONENT_AUTHENTICATION.json")
 commit=publish(CODE,"Implement native0403 frame resource and verified base hierarchy lifecycle")
 pins=read(R/"frames-02/SOURCE_FILES.json")
 save("COMMITTED_AUTHENTICATION.json",{
  "status":"AUTHENTICATED_COMMITTED_NATIVE0403_BASE_RESOURCE","runtime_source_commit":commit,
  "runtime_tree":git("rev-parse",commit+"^{tree}"),"origin_main_at_publication":git("rev-parse","origin/main"),
  "source_files":len(pins),"all_measured_source_blobs_match_commit":True,
  "source_manifest_sha256":sha(R/"frames-02/SOURCE_FILES.json"),
  "component_authentication_sha256":sha(R/"COMPONENT_AUTHENTICATION.json"),
  "implementation_record_sha256":sha(R/"IMPLEMENTATION.md"),
  "verification_scope":"recorded precommit component executions authenticated against identical committed source; not a fresh-checkout rebuild",
  "retained_regressions":"four unchanged binaries/production inputs; only unrelated expanded test source differs",
  "base_empty_registry_only":True,"frame_plugins_clone_sync":"OPEN","renderer_gpu":"OPEN",
  "fresh_checkout_build":False,"game_run":False,"oracle_comparison":"OPEN","acceptance":"OPEN"})
 print("Saved exact committed-source receipt; evidence publication next",flush=True)
else:
 a=read(R/"COMMITTED_AUTHENTICATION.json")
 assert git("rev-parse","HEAD")==a["runtime_source_commit"]
 assert a["component_authentication_sha256"]==sha(R/"COMPONENT_AUTHENTICATION.json")
 assert not git("diff","--name-only","--","runtime","tests","cmake","CMakeLists.txt")
 publish([],"Record committed0403 base-resource verification and remaining game gates")
