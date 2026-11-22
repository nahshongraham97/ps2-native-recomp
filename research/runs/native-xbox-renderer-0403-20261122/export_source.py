"""Bounded original data/ref/function export; exact source joins and owned deadline."""
from pathlib import Path
import gzip,hashlib,json,subprocess,sys,threading,time
R=Path(__file__).resolve().parent;ROOT=R.parents[2];PARENT=ROOT.parents[2]
PRIOR=ROOT/"research/runs/native-xbox-scene-frame-20261122"
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def save(p,v):
 with p.open("x",newline="\n") as f:json.dump(v,f,indent=2);f.write("\n")
identity=json.loads((ROOT/"research/runs/native-xbox-mode-scene-20261122/MODE_ANCHORS.json").read_text())
image=(PARENT/"private-inputs/xbox-mka-20261121/default.xbe").read_bytes()
assert hashlib.sha256(image).hexdigest()==identity["xbe_sha256"]
reference=json.loads((PRIOR/"SCENE_SESSION.json").read_text());session=Path(reference["session_path"])
assert sha(session)==reference["session_sha256"]
manifest=json.loads(session.read_text());project=Path(manifest["projects"]["xbox"]["project"])
assert not list(project.parent.glob(project.name+".lock*")),"private project busy; never remove its lock"
def mapped(at,n):
 sections=[s for s in identity["sections"] if s["virtual_address"]<=at and at+n<=s["virtual_address"]+s["file_size"]]
 assert len(sections)==1,(hex(at),n)
 s=sections[0];offset=s["file_offset"]+at-s["virtual_address"]
 return image[offset:offset+n],dict(vma=hex(at),length=n,file_offset=offset,section=s["name"])
name=sys.argv[1];queries=sys.argv[2:];assert queries and not (R/name).exists();heads={}
for query in queries:
 mode,value=query.split("=");parts=value.split(":");at=int(parts[0],16);n=int(parts[1]) if len(parts)>1 else 96
 assert mode in {"fn","ref","data"} and 0<n<=4096
 sections=[s for s in identity["sections"] if s["virtual_address"]<=at and at+n<=s["virtual_address"]+s["virtual_size"]]
 assert len(sections)==1;section=sections[0]
 if at>=section["virtual_address"]+section["file_size"]:
  heads[query]=dict(mapping=dict(vma=hex(at),length=n,section=section["name"]),file_backed=False,loader_zero_initialized=True)
 else:
  raw,mapping=mapped(at,n);heads[query]=dict(mapping=mapping,file_backed=True,bytes_hex=raw.hex())
save(R/(name+".original.json"),dict(xbe_sha256=identity["xbe_sha256"],queries=heads))
argv=manifest["projects"]["xbox"]["headless_argv"];argv[argv.index("-scriptPath")+1]=str(R)
argv+=["-postScript","ExportXbox0403.java",str(R/name),*queries]
print("START0403 source",name,"deadline180s",queries,flush=True)
start=time.monotonic();output=bytearray()
p=subprocess.Popen(argv,cwd=PARENT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
def stream():
 for line in iter(p.stdout.readline,b""):
  output.extend(line);text=line.decode(errors="replace").rstrip()
  if any(key in text for key in ["REPORT:","ERROR","WARN","Discarding changes"]):print(text,flush=True)
t=threading.Thread(target=stream,daemon=True);t.start();timed_out=False
try:exit=p.wait(timeout=180)
except subprocess.TimeoutExpired:
 timed_out=True;subprocess.run(["taskkill","/PID",str(p.pid),"/T","/F"],capture_output=True,timeout=10);exit=p.wait(timeout=10)
t.join(timeout=5)
with (R/(name+".log.gz")).open("xb") as f:f.write(gzip.compress(bytes(output),mtime=0))
save(R/(name+".command.json"),dict(argv=argv,exit=exit,timed_out=timed_out,seconds=time.monotonic()-start,
 deadline_seconds=180,session_sha256=sha(session),script_sha256=sha(R/"ExportXbox0403.java"),
 runner_sha256=sha(Path(__file__)),log_sha256=hashlib.sha256(output).hexdigest()))
assert not timed_out and exit==0
assert (R/name/"functions.tsv").is_file(),"script did not complete; inspect saved raw log"
joined={}
for path in (R/name).glob("*.asm"):
 spans=[];rows=[r.split("\t",2) for r in path.read_text().splitlines()]
 for address,text,_ in rows:
  raw=bytes.fromhex(text);actual,mapping=mapped(int(address,16),len(raw));assert raw==actual
  spans.append(dict(**mapping,bytes_hex=text))
 joined[path.stem]=dict(instructions=len(rows),listing_sha256=sha(path),original_spans=spans)
 c=path.with_suffix(".c")
 if c.exists():
  with c.with_suffix(".c.gz").open("xb") as f:f.write(gzip.compress(c.read_bytes(),mtime=0))
save(R/(name+".join.json"),dict(xbe_sha256=identity["xbe_sha256"],functions=joined,
 scope="actual original bytes/listings only; full coverage/semantics reviewed separately"))
print("PASS0403 export",round(time.monotonic()-start,2),"seconds",flush=True)
print((R/name/"functions.tsv").read_text(),flush=True)
