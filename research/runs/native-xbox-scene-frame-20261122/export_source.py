"""Bounded streamed exports from this session's private saved Xbox program."""
from pathlib import Path
import gzip,hashlib,json,subprocess,sys,threading,time
R=Path(__file__).resolve().parent;ROOT=R.parents[2];PARENT=ROOT.parents[2]
PRIOR=ROOT/"research/runs/native-xbox-mode-scene-20261122"
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def save(path,value):
 with path.open("x",newline="\n") as f:json.dump(value,f,indent=2);f.write("\n")
identity=json.loads((PRIOR/"MODE_ANCHORS.json").read_text())
image=(PARENT/"private-inputs/xbox-mka-20261121/default.xbe").read_bytes()
assert hashlib.sha256(image).hexdigest()==identity["xbe_sha256"]
session_ref=json.loads((R/"SCENE_SESSION.json").read_text());session=Path(session_ref["session_path"])
assert sha(session)==session_ref["session_sha256"]
def mapped(at,n):
 sections=[s for s in identity["sections"] if s["virtual_address"]<=at and at+n<=s["virtual_address"]+s["file_size"]]
 assert len(sections)==1,(hex(at),n)
 s=sections[0];offset=s["file_offset"]+at-s["virtual_address"]
 return image[offset:offset+n],{"vma":hex(at),"file_offset":offset,"length":n,"section":s["name"]}
name=sys.argv[1];queries=sys.argv[2:];assert queries and not (R/name).exists()
heads={}
for query in queries:
 at=int(query.split(":")[0],16);n=(int(query.split(":")[1]) if ":" in query else 64) or 64
 sections=[s for s in identity["sections"] if s["virtual_address"]<=at and at+n<=s["virtual_address"]+s["virtual_size"]]
 assert len(sections)==1,(hex(at),n)
 section=sections[0]
 if at>=section["virtual_address"]+section["file_size"]:
  heads[query]={"mapping":{"vma":hex(at),"length":n,"section":section["name"]},
   "file_backed":False,"loader_zero_initialized":True}
 else:
  raw,mapping=mapped(at,n);heads[query]={"mapping":mapping,"bytes_hex":raw.hex(),"file_backed":True}
save(R/(name+".original.json"),{"xbe_sha256":identity["xbe_sha256"],"queries":heads})
argv=json.loads(session.read_text())["projects"]["xbox"]["headless_argv"]
argv[argv.index("-scriptPath")+1]=str(PRIOR)
argv+=["-postScript","ExportXboxMode.java",str(R/name),*queries]
print("START Xbox source export",name,"deadline240s","queries",queries,flush=True)
start=time.monotonic();raw=bytearray()
p=subprocess.Popen(argv,cwd=PARENT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
def stream():
 for line in iter(p.stdout.readline,b""):
  raw.extend(line);print(line.decode(errors="replace").rstrip(),flush=True)
t=threading.Thread(target=stream,daemon=True);t.start();timed_out=False
try:exit=p.wait(timeout=240)
except subprocess.TimeoutExpired:
 timed_out=True;subprocess.run(["taskkill","/PID",str(p.pid),"/T","/F"],capture_output=True,timeout=10);exit=p.wait(timeout=10)
t.join(timeout=5)
with (R/(name+".log.gz")).open("xb") as f:f.write(gzip.compress(bytes(raw),mtime=0))
save(R/(name+".command.json"),{"argv":argv,"exit":exit,"timed_out":timed_out,"seconds":time.monotonic()-start,
 "deadline_seconds":240,"session_sha256":sha(session),"script_sha256":sha(PRIOR/"ExportXboxMode.java"),
 "runner_sha256":sha(Path(__file__)),"log_sha256":hashlib.sha256(raw).hexdigest()})
assert not timed_out and exit==0
joined={}
for path in (R/name).glob("*.asm"):
 rows=[r.split("\t",2) for r in path.read_text().splitlines()];spans=[]
 for address,text,_ in rows:
  data=bytes.fromhex(text);actual,mapping=mapped(int(address,16),len(data));assert actual==data,(path,address)
  spans.append({**mapping,"bytes_hex":text})
 joined[path.stem]={"instructions":len(rows),"listing_sha256":sha(path),"original_spans":spans}
 c=path.with_suffix(".c")
 if c.exists():
  with c.with_suffix(".c.gz").open("xb") as f:f.write(gzip.compress(c.read_bytes(),mtime=0))
save(R/(name+".join.json"),{"xbe_sha256":identity["xbe_sha256"],"functions":joined,
 "scope":"actual listing bytes; behavior/full-body coverage reviewed separately"})
print("PASS source export",round(time.monotonic()-start,2),"seconds",flush=True)
print((R/name/"functions.tsv").read_text(),flush=True)
