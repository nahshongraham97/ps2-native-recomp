"""Authenticate clean results, prepare scoped handoff, validate staging, commit and push."""
from pathlib import Path
import difflib
import gzip
import hashlib
import json
import struct
import subprocess
RECORD=Path(__file__).resolve().parent
ROOT=RECORD.parents[2];PARENT=ROOT.parents[2]
BRANCH="selected-resource-owner-20261122"
def git(*args):
    return subprocess.check_output(["git","-c","core.autocrlf=false",*args],cwd=ROOT,text=True).strip()
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def save(path,value):
    with path.open("x",newline="\n") as f:json.dump(value,f,indent=2);f.write("\n")
assert git("branch","--show-current")==BRANCH
assert not git("diff","--cached","--name-only")
assert not git("diff","--name-only","--","runtime","tests","cmake","CMakeLists.txt")
out=RECORD/"validation-owner-clean-03"
receipt=json.loads((out/"VERIFICATION.json").read_text())
assert receipt["status"]=="PASS_CONNECTED_NATIVE_LOADED_SYSTEM_ART_COMPONENTS"
assert receipt["clean_tracked_source"] is True and receipt["tracked_status"]==""
assert receipt["with_prerequisites"] is True and receipt["source_commit"]==git("rev-parse","HEAD")
expected_names=[
 "test_native_retail_loaded_system_art","test_native_retail_system_art_cpu",
 "test_native_retail_game_session","test_native_retail_initial_reset_arcade",
 "test_native_retail_profile_permission","test_native_retail_profile_entry_decision",
 "test_native_retail_saved_profile_records","test_native_file_read_job","test_native_file",
 "test_native_retail_package","test_native_gl_texture"]
assert isinstance(receipt["clients"],list)
assert [c["name"] for c in receipt["clients"]]==expected_names and len(receipt["clients"])==11
assert isinstance(receipt["remaining_undefined_symbols"],list) and len(receipt["remaining_undefined_symbols"])==62
assert len(receipt["resolved_bindings"])==3 and len(receipt["new_required_descendants"])==2
pins=json.loads((out/"SOURCE_FILES.json").read_text())
assert pins=={n:sha(ROOT/n) for n in pins}
committed={}
newline_pairs={}
for name,raw_sha in pins.items():
    raw=subprocess.check_output(["git","show","HEAD:"+name],cwd=ROOT)
    committed[name]=hashlib.sha256(raw).hexdigest()
    current=(ROOT/name).read_bytes()
    assert raw.replace(b"\r\n",b"\n")==current.replace(b"\r\n",b"\n"),name
    if raw!=current:newline_pairs[name]=dict(committed_sha256=committed[name],worktree_sha256=raw_sha)
for client in receipt["clients"]:
    name=client["name"];observed=json.loads((out/(name+".json")).read_text())
    assert observed["exit"]==0
    assert observed["executable_sha256"]==client["sha256"]==sha(ROOT/"b"/(name+".exe"))
    assert observed["source_manifest_sha256"]==receipt["source_manifest_sha256"]
    raw=gzip.decompress((out/(name+".log.gz")).read_bytes())
    assert hashlib.sha256(raw).hexdigest()==observed["log_sha256"]
assert not (ROOT/"b/test_native_startup_missing_bindings.exe").exists()
assets=PARENT/"private-inputs/native-ps2-loose-20261121/art/gameart.ssf"
assert sha(assets)=="9900120ff8782b16e0d73ee885c8f4df6c82545b915dd10105e6694a03658a39"
readbacks={p.name:sha(p) for p in sorted((out/"readbacks").glob("*.rgba"))}
prior_readbacks={p.name:sha(p) for p in sorted((RECORD/"validation-prerequisites-01/readbacks").glob("*.rgba"))}
assert len(readbacks)==105 and readbacks==prior_readbacks
save(RECORD/"CLEAN_AUTHENTICATION.json",dict(
 status="AUTHENTICATED_CLEAN_CONNECTED_COMPONENTS",runtime_source_commit=receipt["source_commit"],
 verification_sha256=sha(out/"VERIFICATION.json"),committed_source_sha256=committed,
 retained_newline_pairs=newline_pairs,clients=receipt["clients"],
 original_gameart_sha256=sha(assets),readback_sha256=readbacks,
 readback_comparison="105 exact unchanged original image readbacks across prerequisite and final runs",
 primary_worktree_source_changed=False,audio_source_changed=False,
 physical_platform_factory="COMPILED_NOT_EXECUTED",oracle_comparison="OPEN",game_acceptance="OPEN"))

# Record the separate caller-argument preflight without reopening a shared
# project or treating the master's split18DA30 tail as an independent function.
caller=RECORD/"source-reset-input-01/0018d970.asm"
assert caller.is_file()
rows=[line.split("\t",2) for line in caller.read_text().splitlines()]
by_address={int(r[0],16):r for r in rows}
setup=[r for r in rows if 0x18da10<=int(r[0],16)<0x18da30]
assert by_address[0x18da2c][1]=="2d200000" and by_address[0x18da2c][2].endswith("move a0,zero")
prior_source=json.loads((RECORD.parent/"native-selected-resource-owner-20261122/SOURCE_RECEIPT.json").read_text())
elf_path=Path(prior_source["elf"]["path"]);elf=elf_path.read_bytes()
assert hashlib.sha256(elf).hexdigest()==prior_source["elf"]["sha256"]
begin=int(rows[0][0],16);length=len(rows)*4
segments=prior_source["elf"]["pt_load_segments"]
matches=[s for s in segments if s["p_vaddr"]<=begin and begin+length<=s["p_vaddr"]+s["p_filesz"]]
assert len(matches)==1
segment=matches[0];offset=segment["p_offset"]+begin-segment["p_vaddr"]
original=elf[offset:offset+length]
assert original==bytes.fromhex("".join(r[1] for r in rows))
save(RECORD/"RESET_CALLER_SOURCE_JOIN.json",dict(
 elf_sha256=prior_source["elf"]["sha256"],vma=hex(begin),length=length,file_offset=offset,
 source_words=[f"{w[0]:08x}" for w in struct.iter_unpack("<I",original)],
 listing_sha256=sha(caller),argument_zero="18DA2C DADDU a0,zero,zero; only stores before16FC70 JAL and its store delay slot",
 scope="Original18D970 caller evidence, not a new reset implementation"))
for path in caller.parent.glob("*.c"):
    with path.with_suffix(".c.gz").open("xb") as f:f.write(gzip.compress(path.read_bytes(),mtime=0))

summary=("Published isolated native sysart/font source"+receipt["source_commit"][:7]+": all11 affected clients "
 "pass freshly on clean committed source, including prerequisite regressions, actual worker reads, "
 "all105 native image uploads/readbacks and production16FC70 cache/selected-font/layout/geometry/error/lifetime checks. "
 "Original-byte/private-EE evidence and failed Owner01 expectation are preserved. Platform image context acquisition "
 "is compiled, not game-executed. Section startup still requires actual permanent_strings_eng.mko "
 "215660/215730 descendants; general replacement/cache repopulation and full gameplay remain OPEN. "
 "Exact62 required full-entry names are diagnostic only. Audio and primary dirty work are preserved. "
 "Record: fork research/runs/native-loaded-font-20261122/. Owner: no action; agent next: integrate this branch/shared "
 "patch, implement permanent-string resource ownership, then continue connected startup.")
patch=[]
document_inputs={}
for name in ["PORT_STATUS.md","ROADMAP.md"]:
    raw=(PARENT/name).read_bytes();old=raw.decode().replace("\r\n","\n")
    lines=old.splitlines(True);assert lines[2].startswith("Current integration:")
    after="".join(lines[:2])+summary+"\n"+"".join(lines[3:])
    after=after.replace("Both production profile connections remain OPEN.",
      "At that rejected snapshot both production profile connections remained OPEN; published3f01556 supersedes that binding status.",1)
    document_inputs[name]=dict(base_raw_sha256=hashlib.sha256(raw).hexdigest(),proposed_sha256=hashlib.sha256(after.encode()).hexdigest())
    patch.extend(difflib.unified_diff(old.splitlines(True),after.splitlines(True),fromfile="a/"+name,tofile="b/"+name))
brief_name="research/runs/parallel-agent-preparation-20261122/READY_WORK.md"
raw=(PARENT/brief_name).read_bytes();old=raw.decode().replace("\r\n","\n")
lines=old.splitlines(True)
after="".join(lines[:2])+(
 "Current selected-resource branch checkpoint: saved-profile bindings already exist on main3f01556; "
 "the isolated selected-resource-owner-20261122 delivery now implements production16FC70 and real sysart ownership. "
 "Its clean source"+receipt["source_commit"][:7]+" passes11 affected clients. Primary integration uses "
 "fork research/runs/native-loaded-font-20261122/HANDOFF.md; actual permanent-string descendants and full game "
 "acceptance remain OPEN. The older53e1a59 binding check and assignments below are historical.\n\n")+"".join(lines[2:])
after=after.replace("Current assignment update: rejected","Historical assignment update: rejected",1)
after=after.replace("both production profile connections remain OPEN.",
                    "both production profile connections remained OPEN at that snapshot, superseded by3f01556.",1)
document_inputs[brief_name]=dict(base_raw_sha256=hashlib.sha256(raw).hexdigest(),proposed_sha256=hashlib.sha256(after.encode()).hexdigest())
patch.extend(difflib.unified_diff(old.splitlines(True),after.splitlines(True),fromfile="a/"+brief_name,tofile="b/"+brief_name))
with (RECORD/"parent-documents.final.patch.gz").open("xb") as f:f.write(gzip.compress("".join(patch).encode(),mtime=0))
base=(ROOT.parent/"ps2-native-recomp/cmake/native_game.cmake").read_bytes()
target=(ROOT/"cmake/native_game.cmake").read_bytes()
incremental="".join(difflib.unified_diff(base.decode().replace("\r\n","\n").splitlines(True),
 target.decode().splitlines(True),fromfile="a/cmake/native_game.cmake",tofile="b/cmake/native_game.cmake"))
with (RECORD/"shared-cmake-against-primary.patch.gz").open("xb") as f:f.write(gzip.compress(incremental.encode(),mtime=0))
save(RECORD/"INTEGRATION_PATCH_INPUTS.json",dict(parent_documents=document_inputs,
 primary_cmake_sha256=hashlib.sha256(base).hexdigest(),target_cmake_sha256=hashlib.sha256(target).hexdigest(),
 Main_header_changed=False,base_main="3f0155622a148ac490565337d65cb4aaf0cbce45"))
with (RECORD/"HANDOFF.md").open("x",newline="\n") as f:
    f.write("# Clean native sysart/font component handoff\n\n"+summary+"\n\n"+
      "## Integration\n\n"+
      "- Branch: `selected-resource-owner-20261122`; verified runtime source `"+receipt["source_commit"]+"`.\n"+
      "- `shared-main-cmake.patch.gz` is the full shared CMake diff against fork main3f01556; Main.hpp has no edits.\n"+
      "- `shared-cmake-against-primary.patch.gz` is the incremental CMake diff against the preserved primary dirty batch. "+
      "Authenticate its exact base with `INTEGRATION_PATCH_INPUTS.json`; reconcile rather than overwriting other work.\n"+
      "- `parent-documents.final.patch.gz` updates only the affected status/roadmap/brief. The primary writer applies it; "+
      "this branch does not silently modify shared parent documents.\n"+
      "- New production owners are `retail_loaded_system_art.hpp`, `retail_loaded_system_art.cpp` and "+
      "`retail_loaded_system_art_main.cpp`. Session/platform/CPU changes are in the same tested branch.\n\n"+
      "## Verification and limits\n\n"+
      "- `validation-owner-clean-03/VERIFICATION.json`:11 fresh affected client executions on clean committed source; "+
      "all source blobs, binaries/logs and105 readbacks authenticate in `CLEAN_AUTHENTICATION.json`.\n"+
      "- Source authority and the named single-subrecord sysart scope are in `SOURCE.md`; failed results and original bytes remain intact.\n"+
      "- Actual native platform acquisition is compiled only; tests use explicit never-visible WGL hosts. No game window, "+
      "presentation or audio endpoint is opened. Same-scenario playable-oracle comparison and gameplay acceptance remain OPEN.\n"+
      "- Required permanent-string resource parsing/ownership, generic section replacement and stale-cache repopulation "+
      "remain open, not hidden by texture success. The62-name negative link is not a remaining-work inventory.\n\n"+
      "Owner: no action. Coding agent next: reconcile this delivery, implement actual215660/215730 permanent-string "+
      "ownership, then continue connected startup. No background run remains after publication.\n")
for name in ["WORK.md","SOURCE.md"]:
    path=RECORD/name;text=path.read_bytes().decode().replace("\r\n","\n")
    with (RECORD/(name+".before-final.gz")).open("xb") as f:f.write(gzip.compress(path.read_bytes(),mtime=0))
    if name=="SOURCE.md":
        text=text.replace("Clean committed owner verification remains pending.",
            "Clean committed source"+receipt["source_commit"][:7]+" now passes all11 affected clients in validation-owner-clean-03.")
        text+=("\n`RESET_CALLER_SOURCE_JOIN.json` separately joins the actual60-word18D970 caller "
               "and proves a0=0 at18DA2C before the16FC70 call and its store delay slot.\n")
    else:
        text=text.replace("Clean committed owner\nverification is the remaining checkpoint before branch publication.",
            "Clean committed source"+receipt["source_commit"][:7]+" now passes all11 affected clients in validation-owner-clean-03.")
        text+=("\nFinal clean source/binary/log/input/readback authentication passes. The final shared/parent patches "
               "and remaining blockers are in HANDOFF.md. No background run remains.\n"
               "Owner: no action. Coding agent next: reconcile the delivery, then implement actual permanent-string ownership.\n")
    path.write_text(text,encoding="utf-8",newline="\n")

record_name=RECORD.relative_to(ROOT).as_posix()
paths=sorted(set(git("ls-files","--others","--exclude-standard","--",record_name).splitlines()+
                 git("diff","--name-only","--",record_name).splitlines()))
assert paths and all(n.startswith(record_name+"/") for n in paths)
git("add","--",*paths)
assert sorted(git("diff","--cached","--name-only").splitlines())==paths
git("diff","--cached","--check")
for name in paths:
    raw=subprocess.check_output(["git","show",":"+name],cwd=ROOT)
    assert raw==(ROOT/name).read_bytes(),name
print(git("commit","-m","Record clean native sysart and font-zero connected verification"),flush=True)
print(git("push","-u","origin","HEAD:refs/heads/"+BRANCH),flush=True)
print("Published",git("rev-parse","HEAD"),"runtime source",receipt["source_commit"],flush=True)
