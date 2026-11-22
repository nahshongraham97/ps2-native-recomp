"""Copy the existing saved Xbox project; never open or alter a shared master."""
from pathlib import Path
import hashlib,json,runpy
R=Path(__file__).resolve().parent;ROOT=R.parents[2];PARENT=ROOT.parents[2]
print("START private Xbox scene/frame session; existing project/plugins only",flush=True)
helper=PARENT/"tools/research/prepare_ghidra_session.py"
manifest=runpy.run_path(str(helper))["prepare"]("scene-frame-xbox",["xbox"])
result={"session_path":str(manifest),"session_sha256":hashlib.sha256(manifest.read_bytes()).hexdigest(),
 "helper_sha256":hashlib.sha256(helper.read_bytes()).hexdigest(),"platform":"xbox",
 "masters_modified":False,"shared_connector_reused":False}
with (R/"SCENE_SESSION.json").open("x",newline="\n") as f:json.dump(result,f,indent=2);f.write("\n")
print("PASS private session",manifest,flush=True)
