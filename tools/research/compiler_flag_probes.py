"""Constructed CodeWarrior string/exception-section probes; no game credit."""
from pathlib import Path
import argparse
import hashlib
import itertools
import json
import os
import shutil
import subprocess
from elftools.elf.elffile import ELFFile

SOURCE = '''extern void mkar_sink(int);
struct mkar_probe {
    int n;
    mkar_probe() : n(7) {}
    ~mkar_probe() { mkar_sink(n); }
};
const char *mkar_literal_probe() { return "probe"; }
void mkar_exception_probe() { mkar_probe local; mkar_sink(3); }
'''

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if args.output.exists():raise ValueError('fresh owned output required')
    if shutil.disk_usage('/mnt/c').free<12*(1<<30):raise ValueError('host disk reserve')
    args.output.mkdir(parents=True)
    source=args.output/'probe.cpp';source.write_text(SOURCE)
    tc=Path(os.environ['DK_TOOLCHAIN']);results=[]
    for compiler in sorted((tc/'mwcps2').iterdir()):
        if not compiler.is_dir():continue
        binary=compiler/'mwccps2.exe'
        if not binary.exists():binary=compiler/'mwccmips.exe'
        for readonly,exceptions_off in itertools.product((False,True),repeat=2):
            output=args.output/f'{compiler.name}-str{int(readonly)}-exc{int(exceptions_off)}.o'
            command=[str(tc/'bin/wibo'),str(binary),'-c','-O4','-nostdinc','-stderr',
                     *(['-str','readonly']if readonly else []),
                     *(['-Cpp_exceptions','off']if exceptions_off else []),str(source),'-o',str(output)]
            run=subprocess.run(command,stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=60)
            record=dict(compiler=compiler.name,readonly=readonly,exceptions_off=exceptions_off,
                        argv=command,exit_code=run.returncode,
                        stdout=run.stdout.decode(errors='replace'),stderr=run.stderr.decode(errors='replace'))
            if run.returncode==0 and output.exists():
                with output.open('rb')as stream:
                    elf=ELFFile(stream);sections=[]
                    for section in elf.iter_sections():
                        data=section.data()if section['sh_type']!='SHT_NOBITS'else b''
                        sections.append(dict(name=section.name,size=section['sh_size'],flags=section['sh_flags'],
                                             contains_probe_literal=b'probe\0'in data))
                record.update(object_sha256=hashlib.sha256(output.read_bytes()).hexdigest(),sections=sections)
            results.append(record)
        print(json.dumps(dict(compiler=compiler.name,completed=4)),flush=True)
    result=dict(schema='mkar_constructed_compiler_flag_section_probes_v1',
                source=SOURCE,source_sha256=hashlib.sha256(SOURCE.encode()).hexdigest(),results=results,
                original_image_limit='Original merged unnamed RWX section has no separate .rodata/.exceptix names. Constructed flag effects do not establish the original flags.',
                credit_delta=0)
    (args.output/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n')

if __name__=='__main__':
    main()
