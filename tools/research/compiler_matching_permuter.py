"""Bounded five-minute search on three closest fixed-sample draft misses.

Uses the pinned kit's permuter command, then checks each changed draft under
every already-tested build/flag cell. Never accepts or commits game code.
"""
from pathlib import Path
from fractions import Fraction
import argparse
import contextlib
import copy
import hashlib
import importlib.util
import io
import json
import os
import re
import shutil
import subprocess
import sys
import time

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def write(path,value):
    path.write_text(json.dumps(value,indent=2)+'\n')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kit',type=Path,required=True)
    parser.add_argument('--pack',type=Path,required=True)
    parser.add_argument('--work',type=Path,required=True)
    parser.add_argument('--jobs',type=int,default=4)
    parser.add_argument('--harvest-only',action='store_true')
    args=parser.parse_args()
    if not 1<=args.jobs<=8:raise ValueError('half CPU cap')
    if shutil.disk_usage('/mnt/c').free<12*(1<<30):raise ValueError('host reserve')
    source=Path(__file__).with_name('compiler_matching_linux.py')
    spec=importlib.util.spec_from_file_location('strict_matching',source)
    strict=importlib.util.module_from_spec(spec);spec.loader.exec_module(strict)
    if sha(args.pack)!=strict.PACK_SHA:raise ValueError('frozen originals')
    pack=json.loads(args.pack.read_bytes())
    base=json.loads((args.work/'MATRIX.json').read_bytes())
    if base['status']!='COMPLETE' or base['sample_sha256']!=strict.SAMPLE_SHA:
        raise ValueError('complete fixed matrix required')
    if (args.work/'PERMUTER.json').exists():raise ValueError('preserve previous search')
    originals={(r['module'],r['address']):r for r in pack['sample']}
    sys.path.insert(0,str(args.kit));import dk
    if args.harvest_only:
        destination=args.work.parent/'uniform-harvest-01'
        if destination.exists():raise ValueError('preserve prior harvest')
        destination.mkdir()
        for directory in ('include','tools','src','asm','config'):
            shutil.copytree(args.work/directory,destination/directory)
        shutil.copy(args.work/'dk.json',destination/'dk.json')
        (destination/'build').mkdir();(destination/'work').mkdir()
        best=max(base['results'],key=lambda cell:(cell['nontrivial_exact'],cell['exact'],
            sum(Fraction(row.get('kit_matched',0),max(1,row.get('kit_target',0),row.get('kit_candidate',0))) for row in cell['rows'])))
        project=dk.Project(destination)
        project.cfg.update(gcc=best['compiler'],g=best['g'],cflags=best['flags']);project.save()
        # No per-function optimization override in this uniform-flag experiment.
        dk.looks_unoptimised=lambda *unused:False
        os.environ['DK_WARM']='0'
        previous=Path.cwd();log=io.StringIO()
        try:
            os.chdir(destination)
            with contextlib.redirect_stdout(log),contextlib.redirect_stderr(log):
                dk.cmd_harvest(argparse.Namespace(n=100,max=1000,dry_run=True))
        finally:
            os.chdir(previous)
        scores=json.loads((destination/'work/harvest.json').read_bytes())
        result=dict(status='KIT_DRY_HARVEST_COMPLETE',compiler=best['compiler'],flags=best['flags'],g=best['g'],
            uniform_optimization=True,source_no_O0_override=True,
            kit_scores=scores,kit_100_percent=sum(value==100 for value in scores.values()),
            strict_primary_free_wins=best['exact'],strict_primary_nontrivial_free_wins=best['nontrivial_exact'],
            log=log.getvalue(),new_game_repo=False,accept_or_commit_called=False,native_credit=0,
            limit='Kit harvest skips its own tagged/refused drafts and uses normalized scores. Strict raw-byte counts and fixed51/40denominators come from the independent primary matrix.')
        write(destination/'RESULT.json',result)
        print(json.dumps({key:value for key,value in result.items()if key not in ('kit_scores','log')}))
        return
    ever_exact={(r['module'],r['address']) for cell in base['results'] for r in cell['rows'] if r['status']=='BYTE_EXACT'}
    options={}
    for index,cell in enumerate(base['results']):
        for row in cell['rows']:
            key=(row['module'],row['address'])
            if key in ever_exact or not 25<=row['instructions']<=120 or row['status']!='MISMATCH':
                continue
            score=(Fraction(row['kit_matched'],max(row['kit_target'],row['kit_candidate'])),
                   -len(row['raw_instruction_diff']),cell['compiler'],cell['flags'])
            if key not in options or score>options[key][0]:
                options[key]=(score,index,row)
    selected=sorted(options,key=lambda key:(-options[key][0][0],-options[key][0][1],key))[:3]
    project=dk.Project(args.work)
    os.environ['DK_WARM']='0'
    write(args.work/'PERMUTER_SELECTION.json',dict(rule='Three nontrivial functions never exact in any primary cell; best kit instruction-match fraction, then raw-word differences, deterministic module/address ties.',selection=[dict(key=key,cell=options[key][1],before=options[key][2])for key in selected]))
    results=[];changed={}
    for key in selected:
        _,index,before=options[key];cell=base['results'][index]
        function='func_'+key[1].upper()
        project.cfg.update(gcc=cell['compiler'],g=cell['g'],cflags=cell['flags'])
        project.save()
        invocation=argparse.Namespace(func=function,file=None,minutes=5,jobs=args.jobs)
        log=io.StringIO();start=time.monotonic();exit_code=0
        previous=Path.cwd()
        try:
            os.chdir(args.work)
            with contextlib.redirect_stdout(log),contextlib.redirect_stderr(log):
                dk.cmd_perm(invocation)
        except SystemExit as error:
            exit_code=error.code
        except Exception as error:
            exit_code=1;log.write(repr(error))
        finally:
            os.chdir(previous)
        path=args.work/'work'/f'{function}.perm.c'
        item=dict(key=key,compiler=cell['compiler'],flags=cell['flags'],g=cell['g'],
                  max_seconds=300,seconds=time.monotonic()-start,jobs=args.jobs,
                  exit_code=exit_code,log=log.getvalue(),before=before)
        if path.exists():
            data=path.read_bytes()
            if re.search(rb'INCLUDE_ASM|GLOBAL_ASM|\b(?:__asm__|asm)\s*\(',data):
                raise ValueError('permuter source contains assembly fallback')
            item.update(candidate_source_sha256=sha(path),candidate_source=str(path))
            changed[key]=path
        results.append(item)
        write(args.work/'PERMUTER.json',dict(status='IN_PROGRESS',results=results))
        print(json.dumps({k:v for k,v in item.items() if k not in('log','before')}),flush=True)
    final=copy.deepcopy(base)
    for cell_index,cell in enumerate(final['results']):
        project.cfg.update(gcc=cell['compiler'],g=cell['g'],cflags=cell['flags']);project.save()
        for index,row in enumerate(cell['rows']):
            key=(row['module'],row['address'])
            if key not in changed or cell['compiler']=='3.2-030210-beta2':
                continue
            candidate=changed[key];function='func_'+key[1].upper();original=originals[key]
            item=dict(module=key[0],address=key[1],instructions=row['instructions'],observation=row['observation'],
                      observed_in_matched_replay=row['observed_in_matched_replay'],source_variant='permuted',
                      draft_sha256=sha(candidate),primary_status=row['status'])
            try:
                score,error=dk.try_func(project,function,candidate,gcc=cell['compiler'],g=cell['g'],cflags=cell['flags'])
                if score is None:
                    item.update(status='COMPILER_ERROR',reason=error)
                else:
                    obj=args.work/'build/try'/function/f'{function}.o'
                    dest=args.work/'permuted_objects'/str(cell_index)/(function+'.o')
                    dest.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(obj,dest)
                    raw,relocs=strict.object_bytes(obj,function)
                    linked=strict.relocate(raw,relocs,pack['main_gp'] if key[0]=='SLUS_214.10'else None,int(key[1],16))
                    target=bytes.fromhex(original['raw_hex'])
                    item.update(status='BYTE_EXACT'if linked==target else'MISMATCH',
                        kit_matched=score[0],kit_target=score[1],kit_candidate=score[2],kit_soft=score[3],instruction_diff=score[4],
                        candidate_object=str(dest),candidate_object_sha256=sha(dest),
                        original_sha256=hashlib.sha256(target).hexdigest(),candidate_resolved_sha256=hashlib.sha256(linked).hexdigest(),
                        original_bytes=len(target),candidate_bytes=len(linked),relocations=relocs,
                        raw_instruction_diff=[dict(index=i//4,original=target[i:i+4].hex(),candidate=linked[i:i+4].hex())
                            for i in range(0,max(len(target),len(linked)),4)if target[i:i+4]!=linked[i:i+4]])
            except (Exception,SystemExit)as error:
                item.update(status='CHECK_REFUSED',reason=repr(error))
            cell['rows'][index]=item
        cell['exact']=sum(row['status']=='BYTE_EXACT'for row in cell['rows'])
        cell['nontrivial_exact']=sum(row['status']=='BYTE_EXACT' and 25<=row['instructions']<=120 for row in cell['rows'])
    final.update(stage='AFTER_BOUNDED_PERMUTER',permuter_driver_sha256=sha(Path(__file__)),
                 changed_functions=[list(key)for key in changed],primary_matrix_sha256=sha(args.work/'MATRIX.json'))
    write(args.work/'AFTER_PERMUTER_MATRIX.json',final)
    write(args.work/'PERMUTER.json',dict(status='COMPLETE',results=results,changed_functions=[list(key)for key in changed],
        source_sha256=sha(Path(__file__)),strict_checker_sha256=sha(source),whole_game_or_native_credit=0))

if __name__=='__main__':
    main()
