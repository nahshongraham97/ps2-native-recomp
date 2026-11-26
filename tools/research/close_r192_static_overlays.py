"""Regenerate the R192 static overlays with authenticated constructor roots.

Only a fresh owned stage is written. The installed R192 build is input only.
This uses the already reviewed main-branch MWo3 reader, retaining the v8
generator and CPU ABI. Static entry coverage is not whole-game acceptance.
"""
from __future__ import annotations
import argparse
import csv
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OLD_READER_SHA = 'e2403d747a92dad162fc0925db92721e96dfe49d1839fb070db76d8eb04361e3'
MAIN_READER_SHA = '0c1d46f829bed200b402e9e4e2032b4dfb21c2c836d3cf8ea5a2d821f99c53b4'
ELF_SHA = 'a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f'
MODULES = {'GAMER': ('70714c6f147128fb6462ac57bfe28e3e27a0dcf90730c4054e90c70908feba89', 0x7c4980),
           'MOVIER': ('3a34e3838042aa0bde0ce687a4e61e128b2eb965e60e65c34713e0a73891adb8', 0x84a100)}

def require(ok, reason):
    if not ok:
        raise ValueError(reason)

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def reader_function(text):
    start = text.index('std::optional<Snr2Module> read_mwo3_module(')
    end = text.index('\nLinks snr2_imports(', start)
    return text[start:end]

def run(argv, log):
    result = subprocess.run([str(x) for x in argv], capture_output=True)
    log.write_bytes(result.stdout + result.stderr)
    require(result.returncode == 0, 'command_failed:' + str(log))
    return {'argv': [str(x) for x in argv], 'exit_code': result.returncode, 'log_sha256': sha(log)}

def constructor_roots(raw):
    require(raw[:4] == b'MWo3', 'MWo3_magic')
    base, text_size = struct.unpack_from('<II', raw, 8)
    first, last = struct.unpack_from('<II', raw, 24)
    require(base + 0x40 <= first <= last <= base + len(raw) and (last-first) % 4 == 0, 'constructor_table_bounds')
    return base, text_size, [struct.unpack_from('<I', raw, p-base)[0] for p in range(first, last, 4)]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--stage', type=Path, required=True)
    args = parser.parse_args()
    build, stage = args.build.resolve(), args.stage.resolve()
    require(build.is_relative_to(ROOT/'build') and stage.is_relative_to(ROOT/'build'), 'owned_build_paths_required')
    require(not stage.exists() and not stage.is_relative_to(build), 'fresh_separate_stage_required')
    old = build/'src/recomp/snr2.cpp'
    reviewed = ROOT/'recomp/snr2.cpp'
    require(sha(old) == OLD_READER_SHA and sha(reviewed) == MAIN_READER_SHA, 'reader_source_pin')
    elf = build/'game/disc/SLUS_214.10'
    require(sha(elf) == ELF_SHA, 'ELF_pin')
    before_binary = sha(build/'out/ps2run.exe')
    source_pins = {str(p.relative_to(build)): sha(p) for p in (build/'src/recomp').iterdir() if p.is_file()}
    rows = list(csv.DictReader((ROOT/'research/ledger/functions.csv').open()))
    module_proof = {}
    for name, (digest, entry) in MODULES.items():
        path = build/'modules'/(name+'.OVL')
        require(sha(path) == digest, 'module_pin:'+name)
        raw = path.read_bytes()
        base, text_size, roots = constructor_roots(raw)
        require(entry in roots and entry + 8 <= base + len(raw), 'original_constructor_root:'+name)
        module_proof[name] = {'sha256': digest, 'raw_base': base, 'raw_size': len(raw),
                              'old_truncated_end': base+0x40+text_size, 'constructor_roots': roots,
                              'entry': entry, 'entry_file_offset': entry-base,
                              'entry_words': raw[entry-base:entry-base+16].hex()}
    stage.mkdir(parents=True)
    src = stage/'recomp'
    shutil.copytree(build/'src/recomp', src)
    old_text = old.read_text()
    replacement = reader_function(reviewed.read_text())
    patched = old_text.replace(reader_function(old_text), replacement)
    (src/'snr2.cpp').write_text(patched, encoding='utf-8', newline='\n')
    seed_lines = [line for line in (build/'entries_closure.txt').read_text().splitlines()
                  if not line.startswith(('GAMER:', 'MOVIER:'))]
    seed_lines += [r['module'].removesuffix('.ovl')+':'+r['address'] for r in rows
                   if r['module'] in ('GAMER.ovl','MOVIER.ovl') and r['catalogue_kind']=='callable']
    seeds = stage/'entries.txt'
    seeds.write_text('\n'.join(seed_lines)+'\n', encoding='ascii', newline='\n')
    compiler = json.loads((build/'CONFIGURE.json').read_text())['compiler']
    files = ['main.cpp','r5900.cpp','elf.cpp','analysis.cpp','codegen.cpp','eerecomp_gen.cpp','snr2.cpp']
    commands = [run([compiler,'-std=c++20','-O1','-static','-I'+str(build/'src/runtime/include'),
                     *[src/f for f in files],'-lshell32','-o',stage/'eerecomp.exe'], stage/'compile.log')]
    commands.append(run([stage/'eerecomp.exe',elf,'--entries',seeds,'--modules',build/'modules','--overlays',build/'overlays',
                         '--out',stage/'generated'], stage/'generate.log'))
    # Parse actual map IDs AND switch destinations; unit-range membership is insufficient.
    import aot_inventory
    measured = aot_inventory.measure_entries(stage/'generated', rows)
    missing = {k:v['missing_catalogue_entries'] for k,v in measured['modules'].items()}
    require(missing == {'SLUS_214.10':['005bf218'],'GAMER.ovl':[],'MOVIER.ovl':[]}, 'exact_static_catalogue_coverage')
    controls = []
    harness = stage/'reader_probe.cpp'
    harness.write_text('''#include "snr2.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>
int main(int argc,char**argv) {
 if(argc!=3)return 2;
 std::ifstream stream(argv[1],std::ios::binary);
 std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(stream),{}};
 auto m=recomp::read_mwo3_module(bytes,"test");if(!m)return 5;
 auto entry=(unsigned)std::stoul(argv[2],nullptr,16);
 if(std::find(m->seeds.begin(),m->seeds.end(),entry)==m->seeds.end())return 3;
 if(entry<m->base+m->code_begin||entry+8>m->base+m->code_end)return 4;
 return 0;
}
''', encoding='utf-8', newline='\n')
    variants = {'correct':patched,
                'truncated_extent':old_text.replace(reader_function(old_text),replacement.replace('m.code_end = m.size;', 'm.code_end = pad + 0x40u + u32(bytes, 0x0C);')),
                'dropped_constructor_root':old_text.replace(reader_function(old_text),replacement.replace('m.seeds.push_back(fn);','(void)fn;'))}
    require(all(v!=patched for k,v in variants.items() if k!='correct'), 'plant_not_applied')
    for variant, text in variants.items():
        reader = stage/(variant+'.cpp')
        reader.write_text(text,encoding='utf-8',newline='\n')
        exe = stage/(variant+'.exe')
        commands.append(run([compiler,'-std=c++20','-O0','-static','-I'+str(src),'-I'+str(build/'src/runtime/include'),harness,reader,src/'elf.cpp',
                             '-o',exe],stage/(variant+'-compile.log')))
        for name, proof in module_proof.items():
            require(proof['entry'] >= proof['old_truncated_end'], 'expected_truncation_witness:'+name)
            checked = subprocess.run([str(exe),str(build/'modules'/(name+'.OVL')),f"{proof['entry']:X}"],capture_output=True)
            expected = {'correct':0,'truncated_extent':4,'dropped_constructor_root':3}[variant]
            require(checked.returncode==expected, 'reader_control_failed:'+variant+':'+name)
            controls.append({'name':variant+':'+name,'exit_code':checked.returncode,
                             'status':'PASS' if variant=='correct' else 'REJECTED'})
    require(sha(old)==OLD_READER_SHA and sha(reviewed)==MAIN_READER_SHA and sha(build/'out/ps2run.exe')==before_binary,
            'installed_input_changed')
    require(source_pins=={str(p.relative_to(build)):sha(p) for p in (build/'src/recomp').iterdir() if p.is_file()}, 'v8_generator_input_changed')
    result = {'status':'STATIC_OVERLAY_ENTRY_GENERATION_PASS', 'scope':'Offline generation only; no gameplay or complete AOT closure claim.',
              'module_proof':module_proof,'old_reader_sha256':OLD_READER_SHA,'reviewed_reader_sha256':MAIN_READER_SHA,
              'patched_reader_sha256':sha(src/'snr2.cpp'),'seed_sha256':sha(seeds),
              'compiler_sha256':sha(Path(compiler)), 'generator_sha256':sha(stage/'eerecomp.exe'),
              'commands':commands,'measurement':measured,'controls':controls,
              'installed_binary_sha256_unchanged':before_binary,
              'source_pins':source_pins}
    output = stage/'RESULT.json'
    output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
    print(json.dumps({'result':str(output),'sha256':sha(output),'status':result['status'],
                      'modules':{k:{'covered':len(v['callable_catalogue_entries']),'missing':v['missing_catalogue_entries']}
                                 for k,v in measured['modules'].items()}}))

if __name__ == '__main__':
    main()
