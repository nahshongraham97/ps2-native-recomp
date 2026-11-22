"""Join only bounded kart displacement candidates to the original XBE."""
from pathlib import Path
import gzip,hashlib,json,subprocess,time
RECORD=Path(__file__).resolve().parent;ROOT=RECORD.parents[2];PARENT=ROOT.parents[2]
session=PARENT/"local-research/ghidra-agent-sessions/mode-scene-xbox-1c3c47b6b8d84fc5874adda06c554d02/SESSION.json"
argv=json.loads(session.read_text())["projects"]["xbox"]["headless_argv"]
argv[argv.index("-scriptPath")+1]=str(RECORD)
argv+=["-postScript","FindXboxKartOffsets.java",str(RECORD/"KART_OFFSET_USERS.tsv")]
start=time.monotonic();p=subprocess.run(argv,cwd=PARENT,capture_output=True,timeout=120,creationflags=subprocess.CREATE_NO_WINDOW)
raw=p.stdout+p.stderr
with (RECORD/"kart-offsets.log.gz").open("xb") as f:f.write(gzip.compress(raw,mtime=0))
with (RECORD/"KART_OFFSET_COMMAND.json").open("x",newline="\n") as f:
 json.dump(dict(argv=argv,exit=p.returncode,seconds=time.monotonic()-start,
   script_sha256=hashlib.sha256((RECORD/"FindXboxKartOffsets.java").read_bytes()).hexdigest()),f,indent=2);f.write("\n")
assert p.returncode==0
path=RECORD/"KART_OFFSET_USERS.tsv";assert path.is_file(),raw.decode(errors="replace")[-2000:]
image=(PARENT/"private-inputs/xbox-mka-20261121/default.xbe").read_bytes()
identity=json.loads((RECORD/"MODE_ANCHORS.json").read_text())
assert hashlib.sha256(image).hexdigest()==identity["xbe_sha256"]
for line in path.read_text().splitlines():
 address,hexbytes,*_=line.split("\t");at=int(address,16);data=bytes.fromhex(hexbytes)
 matches=[s for s in identity["sections"] if s["virtual_address"]<=at and at+len(data)<=s["virtual_address"]+s["file_size"]]
 assert len(matches)==1;s=matches[0];offset=s["file_offset"]+at-s["virtual_address"]
 assert image[offset:offset+len(data)]==data
print(path.read_text(),flush=True)
