"""Link a complete sealed original-source VU package into a disposable build.

Reuse authenticated compiled IOP/host objects and the source-bounded static EE
archive. Original builds/launchers/cards remain unchanged. No VU live loader.
"""
from pathlib import Path
import argparse
import json
import os
import shutil
import subprocess

from extract_r192_elf_vu import require, sha

ROOT = Path(__file__).resolve().parents[2]
BRIDGE = r'''
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "vu_loader.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
void register_generated_vu(){try{
 wchar_t path[32768]{};const auto count=GetModuleFileNameW(nullptr,path,32768);
 if(!count||count>=32768)throw std::runtime_error("cannot locate VU package executable");
 mka::aot::initialize_precompiled_vu_modules(std::filesystem::path(path).parent_path()/"aot-modules/vu");
}catch(const std::exception& e){std::fprintf(stderr,"VU_PACKAGE_STARTUP_REFUSED: %s\n",e.what());std::exit(1);}}
'''


def quote(value):
    return '"' + str(value).replace('\\', '/').replace('"', '\\"') + '"'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('base-receipt', 'static-build', 'package', 'compiler', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    for name in ('base', 'static', 'package'):
        parser.add_argument('--expected-' + name + '-sha', required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    require(out.is_relative_to(ROOT / 'build') and not out.exists(), 'fresh_owned_output_required')
    require(sha(args.base_receipt) == args.expected_base_sha and
            sha(args.static_build / 'RESULT.json') == args.expected_static_sha and
            sha(args.package / 'PACKAGE.json') == args.expected_package_sha, 'build_receipt_pin')
    base = json.loads(args.base_receipt.read_text())
    static = json.loads((args.static_build / 'RESULT.json').read_text())
    package = json.loads((args.package / 'PACKAGE.json').read_text())
    require(base['status'] in ('DISPOSABLE_V8_IOP_AOT_BUILD_PASS', 'DISPOSABLE_COMPILED_REFERENCE_MERGE_BUILT') and
            not base['input_changes'], 'measured_IOP_reference_build_required')
    require(package['status'] == 'SEALED_ORIGINAL_SOURCE_VU_PACKAGE' and package['unique_keys'] == 65026,
            'complete_original_VU_package_required')
    require(static['binary_sha256'] == 'bfced6b44da3faa702e9bc88c0aae1e0a0b9510eb6b20f30ce7cd694fdd3cb31' and
            sha(args.static_build / 'out/ps2run.exe') == static['binary_sha256'], 'source_bounded_static_build_changed')
    normal = ROOT / 'build/r192-compiled-konquest'
    require(sha(normal / 'out/ps2run.exe') == '8823021a23e2a3fdd3b2dc4ca0c8eac9e7359beb3e5bce489b965587a0c2479b', 'normal_build_changed')
    for pin in base['source_pins']:
        require(sha(Path(pin['path'])) == pin['sha256'], 'IOP_source_or_object_changed:' + pin['path'])
    for path in (normal / 'src/runtime/include/ps2').glob('*.hpp'):
        require(sha(path) == sha(args.static_build / 'src/runtime/include/ps2' / path.name), 'static_ABI_header_changed:' + path.name)
    manifest = args.package / 'manifest.txt'
    require(sha(manifest) == package['manifest_sha256'], 'sealed_VU_manifest_changed')
    core = ROOT / 'tools/research/r192_aot_modules'
    inputs = [Path(__file__), core / 'vu_loader.cpp', core / 'vu_loader.hpp', args.base_receipt,
              args.static_build / 'RESULT.json', args.static_build / 'out/libps2game.a',
              normal / 'out/ps2run.exe', args.package / 'PACKAGE.json', manifest, args.compiler]
    # Pin every linked file, including newly generated replacement IOP objects.
    link = list(base['link_arguments'])
    for item in link:
        if not item.startswith('-') and Path(item).suffix in ('.obj', '.a'):
            path = Path(item).resolve()
            require(path.is_file(), 'link_input_missing:' + item)
            inputs.append(path)
    pins = {str(path.resolve()): sha(path) for path in dict.fromkeys(inputs)}
    require(shutil.disk_usage(ROOT / 'build').free > (12 << 30) + (2 << 30), 'disk_reserve_exhausted')
    out.mkdir(parents=True)
    target = out / 'out'
    target.mkdir()
    bridge = out / 'vu_startup.cpp'
    bridge.write_text(BRIDGE, newline='\n')
    commands = []
    for source in [core / 'vu_loader.cpp', bridge]:
        obj = out / (source.stem + '.obj')
        command = [str(args.compiler), '-std=c++20', '-O2', '-mavx2', '-ffp-contract=off', '-DNDEBUG',
                   '-I' + str(core), '-I' + str(normal / 'src/runtime/include'),
                   '-DR192_VU_MANIFEST_SHA256="' + package['manifest_sha256'] + '"',
                   '-DR192_VU_SOURCE_STAMP=' + package['source_ABI_stamp'] + 'ull',
                   '-c', str(source), '-o', str(obj)]
        run = subprocess.run(command, capture_output=True)
        (out / (source.stem + '-compile.log')).write_bytes(run.stdout + run.stderr)
        require(run.returncode == 0, 'VU_loader_compile_failed:' + source.name)
        commands.append({'argv': command, 'exit': run.returncode, 'source_sha256': sha(source), 'object_sha256': sha(obj)})
    replaced_game = 0
    for i, arg in enumerate(link):
        if arg.startswith('-Wl,-Map,'):
            link[i] = '-Wl,-Map,' + str(out / 'combined-AOT-link.map')
        elif Path(arg).name == 'libps2game.a':
            link[i] = str((args.static_build / 'out/libps2game.a').resolve())
            replaced_game += 1
    require(replaced_game == 1 and link[-2] == '-o', 'original_link_shape_changed')
    link[-1] = str(target / 'ps2run.exe')
    link[0:0] = [str(out / 'vu_loader.obj'), str(out / 'vu_startup.obj')]
    response = out / 'link.rsp'
    response.write_text('\n'.join(quote(x) for x in link) + '\n', newline='\n')
    command = [str(args.compiler), '@' + str(response)]
    run = subprocess.run(command, capture_output=True)
    (out / 'link.log').write_bytes(run.stdout + run.stderr)
    require(run.returncode == 0, 'combined_AOT_link_failed')
    for source in (normal / 'out').glob('*.dll'):
        shutil.copy2(source, target / source.name)
    # EE modules are immutable, existing offline DLLs; exclusive hardlinks are
    # verified before publication and never written by this tool.
    ee = target / 'aot-modules/ee'
    ee.mkdir(parents=True)
    old_ee = Path(base['candidate']).parent / 'aot-modules/ee'
    old_manifest = old_ee / 'manifest.txt'
    require(sha(old_manifest) == '20e7a347164805453fe608d7226d3cb87f318dc0c3e285ea4ba8856fc4b4f86d', 'EE_package_changed')
    for line in old_manifest.read_text().splitlines()[1:]:
        digest, rank, count, name = line.split()
        require(Path(name).name == name and sha(old_ee / name) == digest, 'EE_payload_changed')
        os.link(old_ee / name, ee / name)
        require(sha(ee / name) == digest, 'EE_link_changed')
    shutil.copy2(old_manifest, ee / 'manifest.txt')
    vu = target / 'aot-modules/vu'
    vu.mkdir()
    for row in package['modules']:
        source = args.package / row['name']
        require(sha(source) == row['sha256'], 'VU_payload_changed')
        os.link(source, vu / row['name'])
        require(sha(vu / row['name']) == row['sha256'], 'VU_copy_changed')
    shutil.copy2(manifest, vu / 'manifest.txt')
    nm = args.compiler.with_name('llvm-nm.exe')
    symbols = subprocess.run([str(nm), '-C', '--defined-only', str(target / 'ps2run.exe')], capture_output=True, text=True)
    (out / 'defined-symbols.log').write_text(symbols.stdout + symbols.stderr, newline='\n')
    require(symbols.returncode == 0 and 'initialize_precompiled_vu_modules(' in symbols.stdout and
            'ps2::iop::kernel::execute_aot_v8(' in symbols.stdout, 'compiled_VU_IOP_connections_missing')
    require(not any('gen::vu1_' in line for line in symbols.stdout.splitlines()), 'old_builtin_VU_table_pulled_from_archive')
    changes = [p for p, digest in pins.items() if sha(Path(p)) != digest]
    require(not changes, 'input_changed_during_connection')
    result = {'status': 'COMPLETE_SOURCE_VU_AND_STATIC_IOP_CONNECTION_BUILT', 'native_credit': 0,
              'candidate': str(target / 'ps2run.exe'), 'candidate_sha256': sha(target / 'ps2run.exe'),
              'input_pins': pins, 'compile_commands': commands, 'link_command': command,
              'link_arguments': link, 'VU_unique_keys': package['unique_keys'], 'VU_DLL_count': len(package['modules']),
              'input_changes': changes,
              'scope': 'Offline registration connection only. No game run, no normal launcher change, no universal-code or final native acceptance.'}
    receipt = out / 'BUILD.json'
    receipt.write_text(json.dumps(result, indent=2) + '\n', newline='\n')
    print(json.dumps({'status': result['status'], 'candidate_sha256': result['candidate_sha256'], 'receipt_sha256': sha(receipt)}))


if __name__ == '__main__':
    main()
