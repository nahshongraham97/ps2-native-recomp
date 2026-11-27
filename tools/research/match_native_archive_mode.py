"""Bounded private MW matching; original bytes and drafts are never published."""
from pathlib import Path
import argparse,hashlib,importlib.util,json,os,shutil,subprocess,time

def sha(data): return hashlib.sha256(data).hexdigest()

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--candidate',type=Path,required=True)
    ap.add_argument('--originals',type=Path,required=True)
    ap.add_argument('--elf',type=Path,required=True)
    ap.add_argument('--toolchain',type=Path,required=True)
    ap.add_argument('--output',type=Path,required=True)
    a=ap.parse_args()
    assert not a.output.exists(),'Preserve old attempts; use a fresh output'
    assert shutil.disk_usage('/mnt/c').free>=12*(1<<30),'12GiB reserve'
    a.output.mkdir(parents=True)
    if hasattr(os,'sched_setaffinity'):os.sched_setaffinity(0,{4,5,6,7})
    original=json.loads(a.originals.read_bytes());elf=a.elf.read_bytes()
    assert sha(elf)==original['elf_sha256']
    for row in original['functions']:
        body=bytes.fromhex(row['raw_hex'])
        assert body==elf[row['file_offset']:row['file_offset']+row['bytes']]
        assert sha(body)==row['sha256'] and len(body)==row['bytes']
    source=a.candidate.read_bytes()
    assert not any(x in source for x in (b'__asm',b'INCLUDE_ASM',b'g_ram',b'c.r['))
    local=a.output/a.candidate.name;local.write_bytes(source)
    checker_path=Path(__file__).with_name('compiler_matching_linux.py')
    spec=importlib.util.spec_from_file_location('strict_mips',checker_path)
    checker=importlib.util.module_from_spec(spec);spec.loader.exec_module(checker)
    cells=[]
    for build in ('3.0.1b198-051011','3.0.1b205-051227','3.0.1b210-060308'):
        compiler=a.toolchain/'mwcps2'/build/'mwccps2.exe'
        for opt in ('-O3','-O4'):
            name=build+'_'+opt[1:];obj=a.output/(name+'.o')
            argv=[str(a.toolchain/'bin/wibo'),str(compiler),'-c','-nostdinc','-stderr',opt,'-sdatathreshold','8',str(local),'-o',str(obj)]
            start=time.perf_counter()
            try:
                run=subprocess.run(argv,stdin=subprocess.DEVNULL,capture_output=True,timeout=60)
                result={'build':build,'flags':[opt,'-sdatathreshold','8'],'argv':argv,'compiler_sha256':sha(compiler.read_bytes()),'exit_code':run.returncode,'seconds':time.perf_counter()-start,'stdout':run.stdout.decode(errors='replace'),'stderr':run.stderr.decode(errors='replace'),'functions':[]}
                if run.returncode==0:
                    result['object_sha256']=sha(obj.read_bytes())
                    for row in original['functions']:
                        item={'address':row['address'],'original_sha256':row['sha256']}
                        try:
                            raw,relocs=checker.object_bytes(obj,'func_'+row['address'].upper())
                            linked=checker.relocate(raw,relocs,original['main_gp'],int(row['address'],16))
                            target=bytes.fromhex(row['raw_hex'])
                            item.update(status='BYTE_EXACT' if linked==target else 'MISMATCH',bytes=len(linked),sha256=sha(linked),relocations=relocs,word_diff=[{'offset':i,'original':target[i:i+4].hex(),'candidate':linked[i:i+4].hex()} for i in range(0,max(len(target),len(linked)),4) if target[i:i+4]!=linked[i:i+4]])
                        except ValueError as error:item.update(status='REFUSED',reason=str(error))
                        result['functions'].append(item)
            except subprocess.TimeoutExpired as error:
                result={'build':build,'flags':[opt],'argv':argv,'status':'COMPILER_TIMEOUT','seconds':time.perf_counter()-start,'reason':str(error)}
            cells.append(result)
            (a.output/'RESULT.json').write_text(json.dumps({'schema':'mkar.archive-mode.compiler-cells.v1','candidate_sha256':sha(source),'originals_sha256':sha(a.originals.read_bytes()),'checker_sha256':sha(checker_path.read_bytes()),'cells':cells,'native_credit_added':0,'ownership_admissions':0,'scope':'Complete untrimmed body and original-address relocations; a match does not prove native memory/ABI/callee behavior.'},indent=2)+'\n')
            print(build,opt,result.get('exit_code'),[(r['address'],r['status']) for r in result.get('functions',[])],flush=True)
    return 0

if __name__=='__main__':raise SystemExit(main())

