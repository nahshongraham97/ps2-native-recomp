"""Inspect actual native CRT imports before choosing the test-only close fault hook."""
from pathlib import Path
import hashlib,json,subprocess
R=Path(__file__).resolve().parent;ROOT=R.parents[2];PARENT=ROOT.parents[2]
tool=PARENT.parent/"dependencies/compiler/llvm-mingw-20261006-ucrt-x86_64/bin/llvm-nm.exe"
obj=ROOT/"b/CMakeFiles/mka_native_content_volume.dir/runtime/src/native/native_file.cpp.obj"
argv=[str(tool),"--undefined-only",str(obj)]
print("START native-file import preflight; deadline30s",flush=True)
p=subprocess.run(argv,cwd=ROOT,capture_output=True,timeout=30,creationflags=subprocess.CREATE_NO_WINDOW)
symbols=[line.split()[-1] for line in p.stdout.decode().splitlines() if line.split()]
value={"argv":argv,"exit":p.returncode,"tool_sha256":hashlib.sha256(tool.read_bytes()).hexdigest(),
 "object_sha256":hashlib.sha256(obj.read_bytes()).hexdigest(),"symbols":symbols,"stdout":p.stdout.decode(),"stderr":p.stderr.decode()}
with (R/"NATIVE_FILE_IMPORTS.json").open("x",newline="\n") as f:json.dump(value,f,indent=2);f.write("\n")
assert p.returncode==0
print("ACTUAL close imports",[n for n in symbols if "fclose" in n],flush=True)
