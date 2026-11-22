"""Scope-check, commit and publish only the measured routing change/evidence."""
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
initial=set(git('diff','--cached','--name-only').splitlines())
runtime=json.loads((record/'BEFORE.json').read_text())['pins']
reviews=json.loads((record/'DELEGATE_REVIEWS.json').read_text())['files']
owned=[p for p in record.glob('*') if p.is_file()]
for name in ['validation-01','diagnostic-01','validation-02','validation-03','clean-validation-01']:
    if (record/name).is_dir():owned.extend(p for p in (record/name).glob('*') if p.is_file())
owned=[p.relative_to(fork).as_posix() for p in owned if p.suffix in ('.json','.md','.py','.gz')]
approved=set(runtime)|set(reviews)|set(owned)
assert initial.issubset(approved)
assert set(git('diff','--name-only').splitlines()).issubset(approved)
for name in approved:
    raw=(fork/name).read_bytes()
    if name in reviews:assert hashlib.sha256(raw).hexdigest()==reviews[name],name
    if not name.endswith(('.gz','.bin')):assert b'\r' not in raw,name
subprocess.run(['git','diff','--check','--',*sorted(approved)],cwd=fork,check=True)
word_files={name for name in reviews if name.endswith('.words.bin')}
assert all((fork/name).stat().st_size<=4096 for name in word_files)
subprocess.run(['git','add','--',*sorted(approved-word_files)],cwd=fork,check=True)
if word_files:subprocess.run(['git','add','--force','--',*sorted(word_files)],cwd=fork,check=True)
staged=set(git('diff','--cached','--name-only').splitlines())
assert staged and staged.issubset(approved)
subprocess.run(['git','diff','--cached','--check'],cwd=fork,check=True)
message=('Connect actual native direct and worker play admission' if args.phase=='source'
         else 'Record clean native play routing verification and source identity')
subprocess.run(['git','commit','-m',message],cwd=fork,check=True)
head=git('rev-parse','HEAD')
subprocess.run(['git','push','origin','main'],cwd=fork,check=True)
assert git('ls-remote','origin','refs/heads/main').split()[0]==head==git('rev-parse','origin/main')
output=record/('PUBLISHED-'+args.phase+'.json');assert not output.exists()
output.write_text(json.dumps(dict(commit=head,phase=args.phase,staged_paths=sorted(staged),remote_main=head),indent=2)+'\n',newline='\n')
print(json.dumps(dict(commit=head,files=len(staged),phase=args.phase)))
