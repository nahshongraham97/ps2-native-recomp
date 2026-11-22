"""Publish only the verified native arcade reset integration and pinned evidence."""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('phase',choices=['source','evidence'])
args=parser.parse_args()
record=Path(__file__).resolve().parent
fork=record.parents[2]
def git(*argv):return subprocess.check_output(['git','-c','core.longpaths=true',*argv],cwd=fork,text=True).strip()
assert git('branch','--show-current')=='main'
runtime=set(json.loads((record/'BEFORE.json').read_text())['pins'])
candidate=json.loads((record/'CANDIDATE_EVIDENCE.json').read_text())['files']
review=json.loads((record/'DELEGATE_REVIEW.json').read_text())['files']
owned=[p for p in record.glob('*') if p.is_file()]
for name in ['validation-01','validation-02','clean-validation-01']:
    if (record/name).is_dir():owned.extend(p for p in (record/name).glob('*') if p.is_file())
owned={p.relative_to(fork).as_posix() for p in owned if p.suffix in ('.json','.py','.md','.gz')}
approved=runtime|set(candidate)|set(review)|owned
assert set(git('diff','--cached','--name-only').splitlines()).issubset(approved)
assert set(git('diff','--name-only').splitlines()).issubset(approved)
for group in [candidate,review]:
    for name,digest in group.items():assert hashlib.sha256((fork/name).read_bytes()).hexdigest()==digest,name
for name in runtime|owned:
    if not name.endswith('.gz'):assert b'\r' not in (fork/name).read_bytes(),name
subprocess.run(['git','diff','--check','--',*sorted(approved)],cwd=fork,check=True)
subprocess.run(['git','-c','core.autocrlf=false','add','--',*sorted(approved)],cwd=fork,check=True)
staged=set(git('diff','--cached','--name-only').splitlines());assert staged and staged.issubset(approved)
subprocess.run(['git','diff','--cached','--check','--',*sorted(runtime|owned)],cwd=fork,check=True)
# Frozen imported CRLF is preserved as original evidence, with source LF checked above.
subprocess.run(['git','-c','core.whitespace=cr-at-eol','diff','--cached','--check'],cwd=fork,check=True)
for group in [candidate,review]:
    for name,digest in group.items():
        raw=subprocess.check_output(['git','show',':'+name],cwd=fork)
        assert hashlib.sha256(raw).hexdigest()==digest,('staged evidence bytes',name)
message=('Bind complete arcade ladder reset to persistent native Session' if args.phase=='source'
         else 'Record clean native arcade reset and connected caller checks')
subprocess.run(['git','commit','-m',message],cwd=fork,check=True)
head=git('rev-parse','HEAD')
subprocess.run(['git','push','origin','main'],cwd=fork,check=True)
assert head==git('rev-parse','origin/main')==git('ls-remote','origin','refs/heads/main').split()[0]
output=record/('PUBLISHED-'+args.phase+'.json');assert not output.exists()
output.write_text(json.dumps(dict(commit=head,phase=args.phase,staged_paths=sorted(staged),remote_main=head),indent=2)+'\n',newline='\n')
print(json.dumps(dict(commit=head,files=len(staged),phase=args.phase)))
