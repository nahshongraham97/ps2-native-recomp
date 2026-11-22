"""Reuse the measured bounded/streamed checker, with new exclusive scene evidence."""
from pathlib import Path
import hashlib
R=Path(__file__).resolve().parent;ROOT=R.parents[2]
base=ROOT/"research/runs/native-xbox-mode-scene-20261122/run_service_check.py"
source=base.read_text()
anchor='"runner_sha256":sha(Path(__file__))'
assert source.count(anchor)==1
source=source.replace(anchor,anchor+',"base_runner_sha256":"'+hashlib.sha256(base.read_bytes()).hexdigest()+'"')
exec(compile(source,str(Path(__file__)),"exec"),{"__file__":str(Path(__file__)),"__name__":"__main__"})
