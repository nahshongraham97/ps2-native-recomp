"""Prepare private0406 projects with a bounded saved-shell command; never open a master."""
from pathlib import Path
import hashlib,json,subprocess,sys,time
R=Path(__file__).resolve().parent;ROOT=R.parents[2];PARENT=ROOT.parents[2]
helper=PARENT/"tools/research/prepare_ghidra_session.py"
argv=[sys.executable,"-u",str(helper),"--agent","renderer-0406","--platform","both"]
assert not (R/"SESSION_REFERENCE.json").exists()
print("START0406 private project copies; deadline90s; saved analysis/installations reused",flush=True)
start=time.monotonic()
p=subprocess.run(argv,cwd=PARENT,capture_output=True,text=True,timeout=90,check=True)
print(p.stdout.strip(),flush=True)
path=Path(p.stdout.strip());raw=path.read_bytes();v=json.loads(raw)
assert v["masters_modified"] is False and v["shared_connector_reused"] is False
receipt={"session_path":str(path),"session_sha256":hashlib.sha256(raw).hexdigest(),
 "helper_sha256":hashlib.sha256(helper.read_bytes()).hexdigest(),"argv":argv,
 "exit":p.returncode,"seconds":time.monotonic()-start,"deadline_seconds":90,
 "platforms":sorted(v["projects"]),"private_project_copies":True,"masters_modified":False,
 "game_run":False,"local_model_used":False}
with (R/"SESSION_REFERENCE.json").open("x",newline="\n") as f:json.dump(receipt,f,indent=2);f.write("\n")
print("PASS0406 private sessions",round(receipt["seconds"],2),"seconds",flush=True)
