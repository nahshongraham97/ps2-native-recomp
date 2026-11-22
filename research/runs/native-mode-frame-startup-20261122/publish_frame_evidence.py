"""Freeze clean verification, then publish only this record after doc maintenance."""
import json,subprocess,sys
from authenticate_frame import authenticate,git,sha,RECORD,ROOT
if "--freeze" in sys.argv[1:]:
    value=authenticate("validation-frame-clean-02",clean=True)
    with (RECORD/"CLEAN_AUTHENTICATION.json").open("x",newline="\n") as f:
        json.dump(value,f,indent=2);f.write("\n")
    print("Authenticated clean committed source, four clients, exact59 negative and preserved draft/index",flush=True)
else:
    value=json.loads((RECORD/"CLEAN_AUTHENTICATION.json").read_text())
    assert value["status"]=="AUTHENTICATED_NATIVE_FRAME_COMPONENT_ROWS"
    assert value["clean_committed_source"] is True
    assert value["runtime_source_commit"]==git("rev-parse","HEAD")
    assert value["verification_sha256"]==sha(RECORD/"validation-frame-clean-02/VERIFICATION.json")
    assert value["source_manifest_sha256"]==sha(RECORD/"validation-frame-clean-02/SOURCE_FILES.json")
    assert not git("diff","--name-only","--","runtime","tests","cmake","CMakeLists.txt")
    assert not git("diff","--cached","--name-only")
    assert git("branch","--show-current")=="main"
    record=RECORD.relative_to(ROOT).as_posix()
    paths=sorted(set(git("ls-files","--others","--exclude-standard","--",record).splitlines()+
                     git("diff","--name-only","--",record).splitlines()))
    assert paths and all(n.startswith(record+"/") for n in paths)
    git("add","--",*paths)
    assert sorted(git("diff","--cached","--name-only").splitlines())==paths
    git("diff","--cached","--check")
    for n in paths:assert subprocess.check_output(["git","show",":"+n],cwd=ROOT)==(ROOT/n).read_bytes(),n
    print(git("commit","-m","Record clean native frame storage checks and Xbox-first implementation priority"),flush=True)
    print(git("push","origin","main"),flush=True)
    print("Published",git("rev-parse","HEAD"),"; runtime source",value["runtime_source_commit"],flush=True)
