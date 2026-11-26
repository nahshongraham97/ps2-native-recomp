"""Exercise the actual sealed VU loader headlessly; no VU program executes."""
from pathlib import Path
import argparse
import json
import shutil
import subprocess

from extract_r192_elf_vu import require, sha

ROOT = Path(__file__).resolve().parents[2]
HARNESS = r'''
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "vu_loader.hpp"
#include "ps2/vu_ops.hpp"
#include <cstdio>
#include <filesystem>
#include <stdexcept>
static unsigned registrations{},programs{};
namespace ps2::vu {
void interpret(int,unsigned){throw std::runtime_error("unexpected program execution");}
void kick(int,unsigned){throw std::runtime_error("unexpected kick");}
unsigned top(){throw std::runtime_error("unexpected TOP");}
unsigned itop(){throw std::runtime_error("unexpected ITOP");}
void register_programs(const Program* table,unsigned count){
 if(!table||!count)throw std::runtime_error("empty registration");
 for(unsigned i=1;i<count;++i)if(table[i-1].key>=table[i].key)throw std::runtime_error("unordered registration");
 ++registrations;programs=count;
}
}
int wmain(int argc,wchar_t**argv){
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
 if(argc!=3)return 2;
 try{
  mka::aot::initialize_precompiled_vu_modules(argv[1]);
  if(registrations!=1)throw std::runtime_error("registration count changed");
  for(auto& row:std::filesystem::directory_iterator(argv[1]))if(row.path().extension()==L".dll"){
   auto file=CreateFileW(row.path().c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,0,nullptr);
   if(file!=INVALID_HANDLE_VALUE){CloseHandle(file);throw std::runtime_error("resident DLL is writable");}
   if(GetLastError()!=ERROR_SHARING_VIOLATION)throw std::runtime_error("unexpected file lock result");
  }
  if(std::wstring(argv[2])==L"double")mka::aot::initialize_precompiled_vu_modules(argv[1]);
  std::printf("PASS_SEALED_VU registrations=%u programs=%u locked=1 no_program_execution=1\n",registrations,programs);return 0;
 }catch(const std::exception& e){std::fprintf(stderr,"LOADER_REFUSED registrations=%u programs=%u reason=%s\n",registrations,programs,e.what());return 1;}
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('package', 'source-root', 'compiler', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--expected-package-sha', required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    require(out.is_relative_to(ROOT / 'build') and not out.exists(), 'fresh_owned_output_required')
    receipt = args.package / 'PACKAGE.json'
    require(sha(receipt) == args.expected_package_sha, 'package_receipt_pin')
    package = json.loads(receipt.read_text())
    require(sha(args.package / 'manifest.txt') == package['manifest_sha256'], 'package_manifest_changed')
    module = package['modules'][0]
    require(sha(args.package / module['name']) == module['sha256'], 'package_module_changed')
    core = ROOT / 'tools/research/r192_aot_modules'
    paths = [Path(__file__), receipt, args.package / 'manifest.txt', args.package / module['name'],
             args.compiler, core / 'vu_loader.cpp', core / 'vu_loader.hpp']
    pins = {str(path): sha(path) for path in paths}
    out.mkdir(parents=True)
    runs = []
    # Re-sealed constructed manifests test deeper validators separately from the
    # outer manifest hash. No constructed manifest is a retail build receipt.
    cases = [('positive', None, 0), ('manifest-seal', 'manifest seal mismatch', 0),
             ('DLL-seal', 'DLL seal mismatch', 0), ('count', 'DLL entry count mismatch', 0),
             ('keys', 'DLL key-set digest mismatch', 0), ('name', 'invalid manifest row', 0),
             ('second-DLL', 'DLL seal mismatch', 0), ('double', 'initializer called twice', 1)]
    for name, refusal, registered in cases:
        directory = out / name
        package_dir = directory / 'package'
        package_dir.mkdir(parents=True)
        source = args.package / module['name']
        shutil.copy2(source, package_dir / 'a.dll')
        first = (args.package / 'manifest.txt').read_text().splitlines()[0].split()
        first[-2] = '1'
        first[-1] = str(module['programs'])
        fields = [module['sha256'], module['key_sha256'], str(module['programs']), 'a.dll']
        if name == 'DLL-seal': fields[0] = '0' * 64
        if name == 'count': fields[2] = str(module['programs'] + 1)
        if name == 'keys': fields[1] = '0' * 64
        if name == 'name': fields[3] = '../a.dll'
        lines = [' '.join(first), ' '.join(fields)]
        if name == 'second-DLL':
            shutil.copy2(source, package_dir / 'b.dll')
            first[-2] = '2'
            lines[0] = ' '.join(first)
            lines.append(' '.join(['0' * 64, module['key_sha256'], str(module['programs']), 'b.dll']))
        manifest = package_dir / 'manifest.txt'
        manifest.write_text('\n'.join(lines) + '\n', encoding='ascii', newline='\n')
        seal = sha(manifest) if name != 'manifest-seal' else '0' * 64
        cpp = directory / 'test.cpp'
        cpp.write_text(HARNESS, newline='\n')
        exe = directory / 'test.exe'
        command = [str(args.compiler), '-std=c++20', '-O2', '-mavx2', '-ffp-contract=off',
                   '-static-libgcc', '-static-libstdc++', '-municode',
                   '-I' + str(core), '-I' + str(args.source_root / 'runtime/include'),
                   '-DR192_VU_MANIFEST_SHA256="' + seal + '"',
                   '-DR192_VU_SOURCE_STAMP=' + package['source_ABI_stamp'] + 'ull',
                   str(cpp), str(core / 'vu_loader.cpp'), '-lbcrypt', '-o', str(exe)]
        compiled = subprocess.run(command, capture_output=True)
        (directory / 'compile.log').write_bytes(compiled.stdout + compiled.stderr)
        require(compiled.returncode == 0, 'loader_control_compile_failed:' + name)
        argv = [str(exe), str(package_dir), 'double' if name == 'double' else 'single']
        run = subprocess.run(argv, capture_output=True, text=True)
        log = run.stdout + run.stderr
        (directory / 'test.log').write_text(log, newline='\n')
        passed = run.returncode == (1 if refusal else 0)
        passed = passed and (refusal in log and f'registrations={registered}' in log if refusal else
                             f'PASS_SEALED_VU registrations=1 programs={module["programs"]} locked=1 no_program_execution=1' in log)
        runs.append({'case': name, 'compile_argv': command, 'argv': argv, 'exit': run.returncode,
                     'passed': bool(passed), 'log': log, 'exe_sha256': sha(exe), 'manifest_sha256': sha(manifest)})
        print(json.dumps({'case': name, 'passed': bool(passed), 'exit': run.returncode}), flush=True)
    require(pins == {p: sha(Path(p)) for p in pins}, 'input_changed_during_test')
    result = {'status': 'PASS_SEALED_VU_LOADER_CONTROLS' if all(r['passed'] for r in runs) else 'FAIL',
              'input_pins': pins, 'runs': runs,
              'scope': 'Actual authored loader with original precompiled DLL; constructed manifests, no game or VU execution, no credit.'}
    target = out / 'RESULT.json'
    target.write_text(json.dumps(result, indent=2) + '\n', newline='\n')
    print(json.dumps({'status': result['status'], 'result_sha256': sha(target)}))
    return 0 if result['status'].startswith('PASS') else 1


if __name__ == '__main__':
    raise SystemExit(main())
