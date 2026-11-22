from pathlib import Path
import gzip,hashlib,json
R=Path(__file__).resolve().parent;ROOT=R.parents[2]
test="tests/test_retail_mode_service.cpp";raw=(ROOT/test).read_bytes()
pins=json.loads((R/"service-mode-zero-02/SOURCE_FILES.json").read_text())
assert hashlib.sha256(raw).hexdigest()==pins[test]
with (R/"test_mode_service.before-diagnostics.cpp.gz").open("xb") as f:f.write(gzip.compress(raw,mtime=0))
print("Preserved failed client's exact measured test bytes; diagnostics only next",flush=True)
