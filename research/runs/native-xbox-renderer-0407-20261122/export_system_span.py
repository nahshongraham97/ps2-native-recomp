"""Reuse bounded original-word joining for the explicitly decoded PS2 switch span."""
from pathlib import Path
import hashlib
R=Path(__file__).resolve().parent;BASE=R.parent/"native-xbox-renderer-0406-20261122"
source=BASE/"export_source.py";text=source.read_text()
replacements={
 'R=Path(__file__).resolve().parent;ROOT=R.parents[2];PARENT=ROOT.parents[2]':
  "R=Path("+repr(str(R))+");ROOT=R.parents[2];PARENT=ROOT.parents[2]",
 '(R/"SESSION_REFERENCE.json").read_text()':'(BASE/"SESSION_REFERENCE.json").read_text()',
 '"Export0406.java"':'"Export0407SystemSpan.java"',
}
for old,new in replacements.items():
 expected=2 if old=='"Export0406.java"' else 1
 assert text.count(old)==expected,(old,"reused exporter changed")
 text=text.replace(old,new)
print("REUSE0407 explicit-span exporter",hashlib.sha256(source.read_bytes()).hexdigest(),flush=True)
exec(compile(text,str(source),"exec"),{"__name__":"__main__","__file__":__file__,"BASE":BASE})
