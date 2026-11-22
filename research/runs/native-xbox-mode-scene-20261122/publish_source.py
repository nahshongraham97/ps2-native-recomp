"""Authenticate the bounded Xbox source findings; publish no runtime changes."""
from pathlib import Path
import gzip,hashlib,json,subprocess
RECORD=Path(__file__).resolve().parent;ROOT=RECORD.parents[2]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def git(*args):return subprocess.check_output(["git","-c","core.autocrlf=false",*args],cwd=ROOT,text=True).strip()
assert git("branch","--show-current")=="main" and not git("diff","--cached","--name-only")
assert not git("diff","--name-only","--","runtime","tests","cmake","CMakeLists.txt")
identity=json.loads((RECORD/"MODE_ANCHORS.json").read_text())
source=(ROOT.parents[2]/"private-inputs/xbox-mka-20261121/default.xbe").read_bytes()
assert hashlib.sha256(source).hexdigest()==identity["xbe_sha256"]
outputs={}
for joinpath in sorted(RECORD.glob("source-*.join.json")):
    name=joinpath.name.removesuffix(".join.json")
    joined=json.loads(joinpath.read_text())
    command=json.loads((RECORD/(name+".command.json")).read_text())
    assert command["exit"]==0
    program=(RECORD/name/"PROGRAM.txt").read_text().splitlines()
    assert program==["default.xbe",identity["xbe_sha256"],"x86:LE:32:default"]
    coverage={}
    for entry,f in joined["functions"].items():
        path=RECORD/name/(entry+".asm");assert sha(path)==f["listing_sha256"]
        rows=[r.split("\t",2) for r in path.read_text().splitlines()]
        assert len(rows)==f["instructions"]==len(f["original_spans"])
        gaps=[];end=None
        for (text,hexbytes,_),span in zip(rows,f["original_spans"]):
            at=int(text,16);raw=bytes.fromhex(hexbytes)
            assert span["vma"]==hex(at) and span["length"]==len(raw) and span["bytes_hex"]==hexbytes
            sections=[s for s in identity["sections"] if s["virtual_address"]<=at and at+len(raw)<=s["virtual_address"]+s["file_size"]]
            assert len(sections)==1
            s=sections[0];offset=s["file_offset"]+at-s["virtual_address"]
            assert span["file_offset"]==offset and span["section"]==s["name"]
            assert source[offset:offset+len(raw)]==raw
            if end is not None and at!=end:gaps.append([hex(end),hex(at)])
            end=at+len(raw)
        coverage[entry]=dict(instructions=len(rows),instruction_range_gaps=gaps)
    outputs[name]=dict(join_sha256=sha(joinpath),command_sha256=sha(RECORD/(name+".command.json")),functions=coverage)
critical=[("source-mode-01","0023c8a0"),("source-mode-01","0023d060"),
          ("source-objects-03","001f98d0"),("source-child-05","001f9bf0"),
          ("source-child-05","00105510")]
assert all(not outputs[name]["functions"][entry]["instruction_range_gaps"] for name,entry in critical)
value=dict(status="AUTHENTICATED_XBOX_SOURCE_FINDINGS_NOT_IMPLEMENTED",
    base_fork_commit=git("rev-parse","HEAD"),xbe_sha256=identity["xbe_sha256"],
    elf_comparison_sha256=identity["elf_sha256"],outputs=outputs,
    critical_contiguous_listing_spans=[dict(output=name,function=entry) for name,entry in critical],
    adopted_donor_runtime_functions=[],remaining="Motor Kombat profile-alias payload/native-field contract",
    source_priority="XBOX_FIRST_ALL_REMAINING_IMPLEMENTATION",game_run=False,acceptance="OPEN")
with (RECORD/"SOURCE_AUTHENTICATION.json").open("x",newline="\n") as f:
    json.dump(value,f,indent=2);f.write("\n")
record=RECORD.relative_to(ROOT).as_posix()
paths=git("ls-files","--others","--exclude-standard","--",record).splitlines()
assert paths and all(n.startswith(record+"/") for n in paths)
for n in paths:
    if Path(n).suffix in {".py",".java",".md",".json",".tsv",".asm"}:
        assert b"\r" not in (ROOT/n).read_bytes(),n
git("add","--",*paths)
assert sorted(git("diff","--cached","--name-only").splitlines())==sorted(paths)
git("diff","--cached","--check")
for n in paths:assert subprocess.check_output(["git","show",":"+n],cwd=ROOT)==(ROOT/n).read_bytes(),n
print(git("commit","-m","Trace Xbox-first startup and mode-object donor with original section joins"),flush=True)
print(git("push","origin","main"),flush=True)
print("Published source findings",git("rev-parse","HEAD"),"; no new donor runtime adoption claimed",flush=True)
