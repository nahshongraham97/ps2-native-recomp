"""Reuse the authenticated0406 exporter/private projects, writing exclusively to this work record."""
from pathlib import Path
import hashlib
R=Path(__file__).resolve().parent
BASE=R.parent/"native-xbox-renderer-0406-20261122"
source=BASE/"export_source.py";text=source.read_text()
replacements={
 'R=Path(__file__).resolve().parent;ROOT=R.parents[2];PARENT=ROOT.parents[2]':
  "R=Path("+repr(str(R))+");ROOT=R.parents[2];PARENT=ROOT.parents[2]",
 '(R/"SESSION_REFERENCE.json").read_text()':'(BASE/"SESSION_REFERENCE.json").read_text()',
 'argv[argv.index("-scriptPath")+1]=str(R)':'argv[argv.index("-scriptPath")+1]=str(BASE)',
 'sha(R/"Export0406.java")':'sha(BASE/"Export0406.java")',
}
for old,new in replacements.items():
 assert text.count(old)==1,(old,"reused exporter changed; inspect before proceeding")
 text=text.replace(old,new)
print("REUSE0406 exporter",hashlib.sha256(source.read_bytes()).hexdigest(),
 "Java",hashlib.sha256((BASE/"Export0406.java").read_bytes()).hexdigest(),flush=True)
exec(compile(text,str(source),"exec"),{"__name__":"__main__","__file__":__file__,"BASE":BASE})
