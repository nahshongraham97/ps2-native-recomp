"""Bounded streamed0406 private exports; join every listed instruction to its own original image."""
from pathlib import Path
import gzip,hashlib,json,struct,subprocess,sys,threading,time
R=Path(__file__).resolve().parent;ROOT=R.parents[2];PARENT=ROOT.parents[2]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def save(p,v):
 with p.open("x",newline="\n") as f:json.dump(v,f,indent=2);f.write("\n")
platform,name,*queries=sys.argv[1:];assert platform in {"xbox","ps2"} and queries
assert name.startswith("source-") and not (R/name).exists()
ref=json.loads((R/"SESSION_REFERENCE.json").read_text());session=Path(ref["session_path"])
assert sha(session)==ref["session_sha256"];manifest=json.loads(session.read_text())
project=Path(manifest["projects"][platform]["project"])
assert not list(project.parent.glob(project.name+".lock*")),"private project busy; never remove a lock"
identity=json.loads((ROOT/"research/runs/native-xbox-mode-scene-20261122/MODE_ANCHORS.json").read_text())
image_path=PARENT/("private-inputs/xbox-mka-20261121/default.xbe" if platform=="xbox" else "research/inputs/SLUS_214.10")
image=image_path.read_bytes();digest=hashlib.sha256(image).hexdigest()
assert digest==identity["xbe_sha256" if platform=="xbox" else "elf_sha256"]
if platform=="xbox":sections=identity["sections"]
else:
 assert image[:6]==b"\x7fELF\x01\x01"
 ph=struct.unpack_from("<I",image,28)[0];ps,pn=struct.unpack_from("<HH",image,42)
 loads=[struct.unpack_from("<8I",image,ph+i*ps) for i in range(pn)]
 sections=[dict(name="PT_LOAD_"+str(i),virtual_address=s[2],virtual_size=s[5],file_offset=s[1],file_size=s[4],
  original_program_header=list(s)) for i,s in enumerate(loads) if s[0]==1]
def mapped(at,n,allow_zero=False):
 ss=[s for s in sections if s["virtual_address"]<=at and at+n<=s["virtual_address"]+s["virtual_size"]]
 assert len(ss)==1,(hex(at),n);s=ss[0];m=dict(vma=hex(at),length=n,section=s["name"])
 if at>=s["virtual_address"]+s["file_size"]:
  assert allow_zero;return None,dict(**m,file_backed=False,loader_zero_initialized=True)
 assert at+n<=s["virtual_address"]+s["file_size"],"mixed file-backed/BSS query"
 o=s["file_offset"]+at-s["virtual_address"];return image[o:o+n],dict(**m,file_offset=o,file_backed=True)
heads={}
for query in queries:
 mode,value=query.split("=");parts=value.split(":");at=int(parts[0],16);n=int(parts[1]) if len(parts)>1 else 32
 assert mode in {"fn","ref","data"} and 0<n<=4096
 raw,m=mapped(at,n,True);heads[query]=dict(mapping=m,bytes_hex=raw.hex() if raw is not None else None)
save(R/(name+".original.json"),dict(platform=platform,image_sha256=digest,sections=sections,queries=heads))
argv=list(manifest["projects"][platform]["headless_argv"]);argv[argv.index("-scriptPath")+1]=str(R)
argv+=["-postScript","Export0406.java",str(R/name),platform,*queries]
print("START0406",platform,name,"deadline180s",queries,flush=True);start=time.monotonic();output=bytearray()
p=subprocess.Popen(argv,cwd=PARENT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
def stream():
 for line in iter(p.stdout.readline,b""):
  output.extend(line);text=line.decode(errors="replace").rstrip()
  if any(k in text for k in ["REPORT:","ERROR","WARN","Discarding changes"]):print(text,flush=True)
t=threading.Thread(target=stream,daemon=True);t.start();timed_out=False
try:exit=p.wait(timeout=180)
except subprocess.TimeoutExpired:
 timed_out=True;subprocess.run(["taskkill","/PID",str(p.pid),"/T","/F"],capture_output=True,timeout=10);exit=p.wait(timeout=10)
t.join(timeout=5)
with (R/(name+".log.gz")).open("xb") as f:f.write(gzip.compress(bytes(output),mtime=0))
save(R/(name+".command.json"),dict(argv=argv,exit=exit,timed_out=timed_out,seconds=time.monotonic()-start,
 deadline_seconds=180,session_sha256=sha(session),script_sha256=sha(R/"Export0406.java"),
 runner_sha256=sha(Path(__file__)),log_sha256=hashlib.sha256(output).hexdigest(),game_run=False))
assert not timed_out and exit==0 and (R/name/"functions.tsv").is_file()
joined={}
for path in (R/name).glob("*.asm"):
 spans=[];gaps=[];rows=[r.split("\t",2) for r in path.read_text().splitlines()];cursor=int(path.stem,16)
 for address,text,_ in rows:
  at=int(address,16);raw=bytes.fromhex(text);actual,m=mapped(at,len(raw));assert raw==actual
  if at!=cursor:
   assert at>cursor;gap,gm=mapped(cursor,at-cursor);gaps.append(dict(**gm,bytes_hex=gap.hex(),semantics="OPEN"))
  spans.append(dict(**m,bytes_hex=text));cursor=at+len(raw)
 joined[path.stem]=dict(instructions=len(rows),listing_sha256=sha(path),original_spans=spans,gaps=gaps)
 c=path.with_suffix(".c")
 if c.exists():
  with c.with_suffix(".c.gz").open("xb") as f:f.write(gzip.compress(c.read_bytes(),mtime=0))
save(R/(name+".join.json"),dict(platform=platform,image_sha256=digest,functions=joined,
 scope="original listed-byte identity only; full-span coverage/ABI/semantics reviewed separately"))
print("PASS0406",platform,"export",round(time.monotonic()-start,2),"seconds",flush=True)
print((R/name/"functions.tsv").read_text(),flush=True)
