"""Select this record's temporary exact-entry callback exporter, reusing the pinned runner."""
from pathlib import Path
R=Path(__file__).resolve().parent
original=(R/"export_source.py").read_text()
old='argv[argv.index("-scriptPath")+1]=str(PRIOR)'
new='argv[argv.index("-scriptPath")+1]=str(R)'
assert original.count(old)==1 and original.count('"ExportXboxMode.java"')==2
source=original.replace(old,new).replace('"ExportXboxMode.java"','"ExportXboxScene.java"')
source=source.replace('sha(PRIOR/"ExportXboxScene.java")','sha(R/"ExportXboxScene.java")')
source=source.replace('"runner_sha256":sha(Path(__file__))',
 '"runner_sha256":sha(Path(__file__)),"callback_wrapper_sha256":sha(R/"export_callbacks.py")')
exec(compile(source,str(R/"export_source.py"),"exec"),{"__file__":str(R/"export_source.py"),"__name__":"__main__"})
