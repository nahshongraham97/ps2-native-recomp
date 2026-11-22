"""Create a new immutable clean checkout and measure the published routing code."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time

record=Path(__file__).resolve().parent
fork=record.parents[2]
clean=fork.parent/'clean-route-22'
build=clean/'build-native'
def git(*argv):return subprocess.check_output(['git',*argv],cwd=fork,text=True).strip()
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
assert not git('status','--porcelain','-uno')
commit=git('rev-parse','HEAD')
assert commit==git('rev-parse','origin/main')
assert not clean.exists() and clean.parent.resolve()==fork.parent.resolve()
subprocess.run(['git','-c','core.autocrlf=false','-c','core.longpaths=true','worktree','add','--detach',str(clean),commit],cwd=fork,check=True)
original=fork/'third_party/sdl2'
pins={p.relative_to(original).as_posix():sha(p) for p in original.rglob('*') if p.is_file()}
target=clean/'third_party/sdl2';assert not target.exists()
shutil.copytree(original,target)
assert pins=={p.relative_to(target).as_posix():sha(p) for p in target.rglob('*') if p.is_file()}
(record/'CLEAN_SDL_DEPENDENCY.json').write_text(json.dumps(dict(source=str(original),target=str(target),files=pins,byte_identical=True),indent=2)+'\n',newline='\n')
prior=json.loads((record.parent/'native-retail-play-admission-20261122/CLEAN_CONFIGURE.json').read_text())
old=prior['clean_checkout']
argv=[arg.replace(old,str(clean)) for arg in prior['argv']]
environment=dict(os.environ)
compiler=Path(next(arg.split('=',1)[1] for arg in argv if arg.startswith('-DCMAKE_CXX_COMPILER=')))
environment['PATH']=str(compiler.parent)+os.pathsep+environment['PATH']
def run(name,command,timeout):
    started=time.monotonic();p=subprocess.run(command,cwd=fork,capture_output=True,text=True,env=environment,creationflags=subprocess.CREATE_NO_WINDOW,timeout=timeout)
    value=dict(argv=command,exit=p.returncode,stdout=p.stdout,stderr=p.stderr,seconds=time.monotonic()-started,
               source_commit=commit,clean_checkout=str(clean),game_run=False,physical_endpoints_opened=False,frames=[],audio=[])
    output=record/(name+'.json');assert not output.exists();output.write_text(json.dumps(value,indent=2)+'\n',newline='\n')
    print(name,p.returncode,p.stdout[-600:],p.stderr[-1000:],flush=True);assert p.returncode==0
run('CLEAN_CONFIGURE',argv,120)
run('CLEAN_CHECK',[sys.executable,str(record/'verify.py'),'--checkout',str(clean),'--build',str(build),'--output',str(record/'clean-validation-01')],300)
