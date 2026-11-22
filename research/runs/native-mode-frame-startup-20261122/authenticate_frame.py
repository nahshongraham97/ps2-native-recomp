"""Read-only authentication of the verifier's actual rows, binaries and source pins."""
from pathlib import Path
import gzip,hashlib,json,re,subprocess,sys
RECORD=Path(__file__).resolve().parent;ROOT=RECORD.parents[2];PARENT=ROOT.parents[2]
CLIENTS=["test_native_retail_frame_work_storage","test_native_retail_permanent_strings",
         "test_native_retail_loaded_system_art","test_native_retail_game_session"]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def git(*args):return subprocess.check_output(["git","-c","core.autocrlf=false",*args],cwd=ROOT,text=True).strip()
def authenticate(folder,clean=False):
    out=RECORD/folder
    def read(name):return json.loads((out/(name+".json")).read_text())
    result=read("VERIFICATION")
    assert isinstance(result["status"],str) and result["status"]=="PASS_NATIVE_FRAME_WORK_STORAGE_COMPONENTS"
    assert isinstance(result["clients"],list) and [v["name"] for v in result["clients"]]==CLIENTS
    assert len(result["clients"])==4
    for field in ["resolved_bindings","remaining_undefined_symbols"]:
        assert isinstance(result[field],list) and all(isinstance(v,str) for v in result[field])
    assert len(result["resolved_bindings"])==1 and len(result["remaining_undefined_symbols"])==59
    assert result["source_commit"]==git("rev-parse","HEAD")
    assert result["verifier_sha256"]==sha(RECORD/"verify_frame.py")
    pins=read("SOURCE_FILES");manifest_sha=sha(out/"SOURCE_FILES.json")
    assert result["source_manifest_sha256"]==manifest_sha and isinstance(pins,dict)
    assert pins=={n:sha(ROOT/n) for n in pins}
    if clean:
        assert result["clean_tracked_source"] is True and result["tracked_status"]==""
        assert not git("status","--porcelain","-uno")
    def row(name):
        value=read(name);raw=gzip.decompress((out/(name+".log.gz")).read_bytes())
        assert isinstance(value["exit"],int) and value["source_commit"]==result["source_commit"]
        assert value["source_manifest_sha256"]==manifest_sha
        assert hashlib.sha256(raw).hexdigest()==value["log_sha256"]
        assert value["executable_sha256"]==sha(Path(value["argv"][0]))
        return value,raw.decode(errors="replace")
    assert row("build")[0]["exit"]==0
    for client in result["clients"]:
        value,text=row(client["name"])
        assert value["exit"]==0 and value["executable_sha256"]==client["sha256"]
        assert "PASS" in text
    value,negative=row("full-entry-negative");assert value["exit"]!=0
    missing=re.findall(r"undefined symbol: ([^\r\n]+)",negative)
    prior=json.loads((ROOT/"research/runs/native-permanent-strings-20261122/validation-clean-03/VERIFICATION.json").read_text())["remaining_undefined_symbols"]
    resolved=[n for n in prior if "main_binding::initialize_retail_frame_work_buffers(" in n]
    assert len(prior)==60 and len(resolved)==1
    assert result["resolved_bindings"]==resolved and result["remaining_undefined_symbols"]==missing
    assert len(missing)==len(set(missing))==59 and sorted(missing)==sorted(set(prior)-set(resolved))
    value,symbols=row("frame-symbols");assert value["exit"]==0
    assert not re.search(r"(?<![A-Za-z0-9_])(?:ps2|gs|spu2|sif|Cpu|recomp)::|\b(?:g_ram|g_spr)\b|SDL_OpenAudio",symbols)
    assert re.search(r"\bT SDL_InitSubSystem\b",symbols) and "__imp_SDL_InitSubSystem" not in symbols
    assert not (ROOT/"b/test_native_startup_missing_bindings.exe").exists()
    assert result["cache_sha256"]==sha(ROOT/"b/CMakeCache.txt")
    promotion=json.loads((ROOT/"research/runs/native-permanent-strings-20261122/PARENT_BEFORE_MAIN_PROMOTION.json").read_text())
    old=Path(promotion["fork_root"])
    assert promotion["dirty_pins"]=={n:sha(old/n) for n in promotion["dirty_pins"]}
    old_git=subprocess.check_output(["git","rev-parse","--absolute-git-dir"],cwd=old,text=True).strip()
    assert promotion["index_sha256"]==sha(Path(old_git)/"index")
    if clean:
        # Earlier source blobs include CRLF; authenticate exact bytes, never repin
        # or normalize their measurements to force agreement with Git.
        for n,digest in pins.items():
            raw=subprocess.check_output(["git","show",result["source_commit"]+":"+n],cwd=ROOT)
            assert hashlib.sha256(raw).hexdigest()==digest,n
    return dict(status="AUTHENTICATED_NATIVE_FRAME_COMPONENT_ROWS",clean_committed_source=clean,
        runtime_source_commit=result["source_commit"],verification_sha256=sha(out/"VERIFICATION.json"),
        source_manifest_sha256=manifest_sha,source_files=len(pins),clients=result["clients"],
        resolved_bindings=resolved,remaining_undefined_symbols=missing,
        preserved_dirty_draft_and_index_unchanged=True,audio_source_unchanged=True,
        source_blob_comparison="EXACT_BYTES" if clean else "NOT_YET_COMMITTED",
        visible_window=False,audio_endpoint=False,game_run=False,oracle_comparison="OPEN",acceptance="OPEN")
if __name__=="__main__":
    value=authenticate(sys.argv[1],"--clean" in sys.argv[2:])
    print(json.dumps({k:v for k,v in value.items() if k!="remaining_undefined_symbols"},indent=2),flush=True)
