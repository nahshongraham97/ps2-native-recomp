"""Read-only export of required status dependencies in the private MSL project."""
from pathlib import Path
import json,subprocess,sys,time,hashlib
record=Path(__file__).resolve().parent
prior=record.parent/'native-retail-sound-worker-20261121'
argv=json.loads((prior/'HEADLESS-07.json').read_text())['argv']
session_manifest=json.loads((record/'PROJECT_SESSIONS.json').read_text())['msl']
private=json.loads(Path(session_manifest).read_text())['projects']['msl']['headless_argv']
headless=argv.index('ghidra.app.util.headless.AnalyzeHeadless')
private_headless=private.index('ghidra.app.util.headless.AnalyzeHeadless')
argv[:headless+3]=private[:private_headless+3]
round_id=sys.argv[1]
assert not (record/('HEADLESS-'+round_id+'.json')).exists()
post=argv.index('-postScript');argv[post+2:argv.index('-log')]=[str(record/('source-'+round_id)),*sys.argv[2:]]
argv[argv.index('-log')+1]=str(record/('ghidra-'+round_id+'.log'))
argv[argv.index('-scriptlog')+1]=str(record/('scripts-'+round_id+'.log'))
argv.append('-readOnly')
start=time.monotonic();p=subprocess.run(argv,capture_output=True,text=True,creationflags=subprocess.CREATE_NO_WINDOW)
(record/('HEADLESS-'+round_id+'.json')).write_text(json.dumps(dict(argv=argv,exit=p.returncode,stdout=p.stdout,stderr=p.stderr,seconds=time.monotonic()-start,read_only=True),indent=2)+'\n',newline='\n')
print('read-only MSL pending-play export',p.returncode);assert p.returncode==0

assert 'SCRIPT ERROR' not in p.stdout+p.stderr
assert (record/('source-'+round_id)/'LISTING.json').is_file()
