"""Scoped staging validation -> commit -> push, stopping on any failed gate."""
from pathlib import Path
import hashlib,json,runpy,subprocess,sys
R=Path(__file__).resolve().parent;ROOT=R.parents[2];REL=R.relative_to(ROOT).as_posix()
BASE="97bf25eeb7a541eefc48b9c618ae91c55d558e2c"
V=runpy.run_path(str(R/"verify_service_results.py"));CODE=V["CODE"]
def sha(p):
 with p.open("rb") as f:return hashlib.file_digest(f,"sha256").hexdigest()
def git(*args):
 return subprocess.check_output(["git","-c","core.autocrlf=false",*args],cwd=ROOT,timeout=25).decode().strip()
def read(p):return json.loads(p.read_text())
def blobs(revisions):
 p=subprocess.run(["git","cat-file","--batch"],cwd=ROOT,input=("\n".join(revisions)+"\n").encode(),
  capture_output=True,check=True,timeout=30)
 raw=p.stdout;cursor=0;out=[]
 for rev in revisions:
  end=raw.index(b"\n",cursor);header=raw[cursor:end].split()
  assert len(header)==3 and header[1]==b"blob",(rev,header)
  size=int(header[2]);cursor=end+1;data=raw[cursor:cursor+size];cursor+=size
  assert raw[cursor:cursor+1]==b"\n";cursor+=1
  out.append(hashlib.sha256(data).hexdigest())
 assert cursor==len(raw)
 return out
def save(name,value):
 with (R/name).open("x",newline="\n") as f:json.dump(value,f,indent=2);f.write("\n")
def stage_commit_push(code,message):
 assert git("branch","--show-current")=="main"
 assert git("rev-parse","HEAD")==git("rev-parse","origin/main")
 assert not git("diff","--cached","--name-only"),"preexisting staged work must remain untouched"
 changed=git("diff","--name-only").splitlines()
 assert all(n in code or n.startswith(REL+"/") for n in changed),changed
 record=git("ls-files","--others","--exclude-standard","--",REL).splitlines()
 paths=sorted(set(changed+record+[n for n in code if n in git("ls-files","--others","--exclude-standard","--",n).splitlines()]))
 assert paths and all(n in code or n.startswith(REL+"/") for n in paths)
 for n in paths:
  p=ROOT/n
  assert p.is_file() and "/private-kart-modules/" not in n and "/negative-content/" not in n
  assert p.suffix.lower() not in {".exe",".dll",".ms2",".ms4",".mko",".ssf",".bin",".state",".ps2"}
  if p.suffix in {".py",".md",".json",".hpp",".cpp",".cmake"}:assert b"\r" not in p.read_bytes(),n
 git("diff","--check","--",*paths)
 before={n:sha(ROOT/n) for n in paths}
 print("START staging validation",len(paths),"approved paths",flush=True)
 git("add","--",*paths)
 staged=git("diff","--cached","--name-only").splitlines()
 assert sorted(staged)==paths
 git("diff","--cached","--check")
 assert blobs([":"+n for n in paths])==[before[n] for n in paths],"staged bytes differ from reviewed working bytes"
 pins=read(R/"service-mode-zero-05/SOURCE_FILES.json")
 assert blobs([":"+n for n in pins])==list(pins.values()),"staged build sources differ from measured901-file identity"
 assert all(sha(ROOT/n)==v for n,v in before.items())
 print("PASS staged path/byte/source/LF gates; START commit",flush=True)
 subprocess.run(["git","-c","core.autocrlf=false","commit","-m",message],cwd=ROOT,check=True,timeout=60)
 commit=git("rev-parse","HEAD")
 assert blobs([commit+":"+n for n in pins])==list(pins.values())
 print("PASS committed measured source identity; START push",commit,flush=True)
 subprocess.run(["git","push","origin","main"],cwd=ROOT,check=True,timeout=90)
 assert git("rev-parse","origin/main")==commit
 print("PUBLISHED",commit,flush=True)
 return commit
assert len(sys.argv)==2 and sys.argv[1] in {"publish","finish"}
if sys.argv[1]=="publish":
 assert git("rev-parse","HEAD")==BASE
 actual=V["authenticate"]();assert actual==read(R/"SERVICE_COMPONENT_AUTHENTICATION.json")
 dispatch=read(R/"MODE_SERVICE_DISPATCH.json")
 assert dispatch["policy_zero_target"]=="0x23d0b1" and dispatch["production_code_change"] is False
 assert dispatch["bytes_hex"]=="b1d02300f4d023000ed12300ced02300"
 commit=stage_commit_push(CODE+["AGENTS.md"],"Finish native retail mode-zero bank selection and retirement")
 pins=read(R/"service-mode-zero-05/SOURCE_FILES.json")
 save("SERVICE_COMMITTED_AUTHENTICATION.json",{
  "status":"AUTHENTICATED_COMMITTED_SERVICE_SOURCE","runtime_source_commit":commit,
  "runtime_tree":git("rev-parse",commit+"^{tree}"),"origin_main_at_publication":git("rev-parse","origin/main"),
  "source_files":len(pins),"all_measured_source_blobs_match_commit":True,
  "source_manifest_sha256":sha(R/"service-mode-zero-05/SOURCE_FILES.json"),
  "component_authentication_sha256":sha(R/"SERVICE_COMPONENT_AUTHENTICATION.json"),
  "dispatch_sha256":sha(R/"MODE_SERVICE_DISPATCH.json"),
  "implementation_record_sha256":sha(R/"MODE_SERVICE_IMPLEMENTATION.md"),
  "verification_scope":"precommit compiled/tested bytes authenticated exactly against clean committed sources; no redundant rerun",
  "fresh_checkout_build":False,"game_run":False,"oracle_comparison":"OPEN","acceptance":"OPEN"})
 print("Saved committed-source authentication; finish evidence publication next",flush=True)
else:
 committed=read(R/"SERVICE_COMMITTED_AUTHENTICATION.json")
 assert git("rev-parse","HEAD")==committed["runtime_source_commit"]
 assert committed["component_authentication_sha256"]==sha(R/"SERVICE_COMPONENT_AUTHENTICATION.json")
 assert not git("diff","--name-only","--","runtime","tests","cmake","CMakeLists.txt")
 stage_commit_push([],"Record committed native mode-service provenance and remaining gates")
