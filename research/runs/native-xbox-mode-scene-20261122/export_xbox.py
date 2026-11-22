"""Export bounded existing Xbox functions and join every saved instruction to its XBE section."""
from pathlib import Path
import gzip,hashlib,json,struct,subprocess,sys,time
RECORD=Path(__file__).resolve().parent;ROOT=RECORD.parents[2];PARENT=ROOT.parents[2]
SESSION=PARENT/"local-research/ghidra-agent-sessions/mode-scene-xbox-1c3c47b6b8d84fc5874adda06c554d02/SESSION.json"
source=(PARENT/"private-inputs/xbox-mka-20261121/default.xbe").read_bytes()
identity=json.loads((RECORD/"MODE_ANCHORS.json").read_text())
assert hashlib.sha256(source).hexdigest()==identity["xbe_sha256"]
def mapped(at,n):
    matches=[s for s in identity["sections"] if s["virtual_address"]<=at and at+n<=s["virtual_address"]+s["file_size"]]
    assert len(matches)==1,(hex(at),n)
    s=matches[0];offset=s["file_offset"]+at-s["virtual_address"]
    return source[offset:offset+n],dict(vma=hex(at),file_offset=offset,length=n,section=s["name"])
name=sys.argv[1];queries=sys.argv[2:];heads={}
for query in queries:
    at=int(query.split(":")[0],16);n=int(query.split(":")[1]) if ":" in query else 64
    if n==0:n=64
    memory=[s for s in identity["sections"] if s["virtual_address"]<=at and at+n<=s["virtual_address"]+s["virtual_size"]]
    assert len(memory)==1,(hex(at),n)
    s=memory[0]
    if at>=s["virtual_address"]+s["file_size"]:
        heads[query]=dict(mapping=dict(vma=hex(at),length=n,section=s["name"]),
                          file_backed=False,loader_zero_initialized=True)
    else:
        raw,mapping=mapped(at,n);heads[query]=dict(mapping=mapping,file_backed=True,bytes_hex=raw.hex())
with (RECORD/(name+".original.json")).open("x",newline="\n") as f:
    json.dump(dict(xbe_sha256=identity["xbe_sha256"],queries=heads),f,indent=2);f.write("\n")
argv=json.loads(SESSION.read_text())["projects"]["xbox"]["headless_argv"]
argv[argv.index("-scriptPath")+1]=str(RECORD)
argv+=["-postScript","ExportXboxMode.java",str(RECORD/name),*queries]
start=time.monotonic();p=subprocess.run(argv,cwd=PARENT,capture_output=True,timeout=300,creationflags=subprocess.CREATE_NO_WINDOW)
raw=p.stdout+p.stderr
with (RECORD/(name+".log.gz")).open("xb") as f:f.write(gzip.compress(raw,mtime=0))
with (RECORD/(name+".command.json")).open("x",newline="\n") as f:
    json.dump(dict(argv=argv,exit=p.returncode,seconds=time.monotonic()-start,session_sha256=hashlib.sha256(SESSION.read_bytes()).hexdigest()),f,indent=2);f.write("\n")
assert p.returncode==0,raw.decode(errors="replace")[-2500:]
assert (RECORD/name/"functions.tsv").is_file(),raw.decode(errors="replace")[-2500:]
joined={}
for path in (RECORD/name).glob("*.asm"):
    rows=[r.split("\t",2) for r in path.read_text().splitlines()];spans=[]
    for address,text,_ in rows:
        data=bytes.fromhex(text);actual,mapping=mapped(int(address,16),len(data));assert actual==data,(path,address)
        spans.append(dict(**mapping,bytes_hex=text))
    joined[path.stem]=dict(instructions=len(rows),listing_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),original_spans=spans)
    c=path.with_suffix(".c")
    if c.exists():
        with c.with_suffix(".c.gz").open("xb") as f:f.write(gzip.compress(c.read_bytes(),mtime=0))
with (RECORD/(name+".join.json")).open("x",newline="\n") as f:
    json.dump(dict(xbe_sha256=identity["xbe_sha256"],functions=joined,
        scope="Actual Xbox listing bytes only; full-body coverage and PS2 equivalence must be reviewed"),f,indent=2);f.write("\n")
print((RECORD/name/"functions.tsv").read_text(),flush=True)
print((RECORD/name/"references.tsv").read_text()[:4000],flush=True)
