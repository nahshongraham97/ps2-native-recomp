"""Build a disposable v8-compatible static IOP candidate; never modify N.

Existing EE/VU libraries and host objects are pinned inputs. New IOP/kernel
objects precede the unchanged reference archive so its interpreter object is
not selected. All private generated payloads/packages remain under build/.
"""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

import generate_r192_iop_aot as fixed
import r192_iop_v8_semantics as semantics

W = fixed.W
require = fixed.require
sha = fixed.file_sha
KERNEL_SHA = '694c9e1fe073d383ffa8b7151468ccfca4271c5aec277f2f6f22929253f279bc'


def prepare_kernel(data, include):
    require(fixed.sha(data) == KERNEL_SHA, 'frozen_original_kernel_source_pin_mismatch')
    text = data.decode('utf-8').replace('\r\n', '\n')
    def replace(before, after):
        nonlocal text
        require(text.count(before) == 1, 'original_kernel_patch_anchor_ambiguous')
        text = text.replace(before, after)
    # The copy no longer resides beside its original quoted headers. Make IOP
    # provenance explicit so an EE kernel.hpp cannot win include-path lookup.
    for header in ('kernel.hpp', 'hw.hpp', 'io.hpp', 'sio2.hpp', 'spu2.hpp'):
        replace('#include "' + header + '"', '#include "iop/' + header + '"')
    replace('#include "../load_stats.hpp"', '#include "load_stats.hpp"')
    replace('std::vector<HleEntry> g_hle;', 'std::vector<HleEntry> g_hle;\nstd::set<std::uint32_t> g_r192_synthesized_imports;')
    replace('    g_hle.clear();', '    g_hle.clear();\n    g_r192_synthesized_imports.clear();')
    before = '''    return register_hle(library, index, [library, index](Cpu &c) {
        if (g_missing_reported.insert({library, index}).second)
            std::fprintf(stderr, "[iop] missing %s #%u called from 0x%08X\\n", library.c_str(), index, c.r[31]);
        c.r[2] = 0;
    });'''
    after = '''    const auto synthesized_id = register_hle(library, index, [library, index](Cpu &c) {
        (void)c;
        std::fprintf(stderr, "[iop-aot-v8] unknown_or_synthesized_HLE:%s#%u\\n", library.c_str(), index);
        std::fflush(stderr);
        throw std::runtime_error("IOP_AOT_REFUSED:unknown_or_synthesized_HLE:" + library + "#" + std::to_string(index));
    });
    g_r192_synthesized_imports.insert(synthesized_id);
    return synthesized_id;'''
    replace(before, after)
    replace('        kernel::g_hle[it->second].function = std::move(function);',
            '        kernel::g_r192_synthesized_imports.erase(it->second);\n        kernel::g_hle[it->second].function = std::move(function);')
    replace('    kernel::g_hle_ids[key] = id;', '    kernel::g_hle_ids[key] = id;\n    kernel::g_r192_synthesized_imports.erase(id);')
    return text + '\n#include ' + json.dumps(include.as_posix()) + '\n'


def quote_response(arg):
    # Clang GNU response parsing, with Windows paths normalized to slashes.
    return '"' + str(arg).replace('\\', '/').replace('"', '\\"') + '"'


def cpu_wrapper_source():
    return '''#include "iop/iop.hpp"
#include "iop/kernel.hpp"
#include <cstddef>
static_assert(sizeof(ps2::iop::Cpu) == 156);
static_assert(offsetof(ps2::iop::Cpu, pc) == 136 && offsetof(ps2::iop::Cpu, status) == 144);
namespace ps2::iop::kernel { void execute_aot_v8(Cpu &, std::int64_t &); }
namespace ps2::iop {
alignas(64) std::uint8_t g_ram[kRamSize + 16];
Cpu g_cpu;
std::uint32_t fetch(std::uint32_t pc) {
    const auto address = phys(pc);
    return address < kRamSize ? ram32(address) : kernel::fetch_special(pc);
}
void execute(Cpu &cpu, std::int64_t &budget) { kernel::execute_aot_v8(cpu, budget); }
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('runtime-root', 'pic-domain', 'compiler', 'harvest', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    parser.add_argument('--expected-runtime-sha', required=True)
    parser.add_argument('--expected-pic-domain-sha', required=True)
    parser.add_argument('--reuse-ee-package', type=Path,
                        help='Optional owned sealed package; create only fresh exclusive hardlinks, never overwrite payloads.')
    args = parser.parse_args()
    require(args.output.resolve().is_relative_to(W / 'build') and not args.output.exists(), 'fresh_owned_disposable_output_required')
    require(sha(args.pic_domain) == args.expected_pic_domain_sha.lower(), 'PIC_domain_receipt_pin_mismatch')
    domain = json.loads(args.pic_domain.read_text())
    require(domain['semantics'] == semantics.MODE and domain['summary']['compiled_PIC_bodies'] == 70 and
            domain['registry']['compile_exit_code'] == 0, 'all_original_domain_frozen_v8_bodies_required')
    n = args.runtime_root
    archive = n / 'out/libps2runtime.a'
    require(sha(archive) == args.expected_runtime_sha.lower(), 'reference_runtime_archive_pin_mismatch')
    semantics.validate_source(n / 'src/runtime/src/iop/cpu.cpp')
    kernel = n / 'src/runtime/src/iop/kernel.cpp'
    private_kernel = prepare_kernel(kernel.read_bytes(), W / 'runtime/src/iop/r192_v8_dispatch.inc')
    # This guard must fail before any output/source mutation on wrong lineage.
    try:
        prepare_kernel(kernel.read_bytes() + b'\n', W / 'runtime/src/iop/r192_v8_dispatch.inc')
    except ValueError as e:
        require(str(e) == 'frozen_original_kernel_source_pin_mismatch', 'wrong_source_control_reason')
    else:
        raise ValueError('wrong_original_source_control_not_rejected')
    objects = []
    for row in domain['records']:
        require(row['status'] == 'PIC_COMPILED_CAPABILITY_ONLY', 'incomplete_PIC_domain')
        for built in row['object_compilations']:
            path = Path(built['object'])
            require(sha(path) == built['object_sha256'], 'PIC_object_changed')
            require(sha(Path(built['source'])) == built['source_sha256'], 'PIC_generated_source_changed')
            objects.append(path)
    registry_obj = Path(domain['registry']['path']).with_suffix('.obj')
    require(sha(registry_obj) == domain['registry']['object_sha256'], 'PIC_registry_object_changed')
    objects.append(registry_obj)
    host_paths = [n / ('out/CMakeFiles/ps2run.dir/' + name + '.obj') for name in (
        'host/main.cpp', 'host/mka_unit_wrappers.cpp', 'host/r192_aot_modules/loader.cpp',
        'host/profile_win.cpp', 'host/window.cpp', 'host/keymap.cpp', 'host/stretch.cpp')]
    libraries = [n / 'out/libps2game.a', archive, n / 'src/third_party/sdl2/lib/libSDL2.dll.a',
                 n / 'src/third_party/wgpu-native/lib/libwgpu_native.a']
    pins = [{'path': str(p.resolve()), 'sha256': sha(p)} for p in host_paths + libraries + [Path(__file__),
        kernel, n / 'src/runtime/src/iop/cpu.cpp', W / 'runtime/src/iop/r192_v8_dispatch.inc',
        W / 'runtime/src/iop/r192_pic.hpp', W / 'runtime/src/iop/r192_pic.cpp',
        W / 'runtime/src/iop/r192_aot.hpp', W / 'runtime/src/iop/r192_aot.cpp', args.pic_domain]]
    args.output.mkdir(parents=True, exist_ok=False)
    src = args.output / 'src'; src.mkdir()
    out = args.output / 'out'; out.mkdir()
    patched = src / 'r192-v8-kernel.cpp'; patched.write_text(private_kernel, encoding='utf-8', newline='\n')
    cpu = src / 'r192-v8-cpu.cpp'
    cpu.write_text(cpu_wrapper_source(), encoding='utf-8', newline='\n')
    compile_receipts = []
    replacement_objects = []
    for index, source in enumerate((patched, cpu, W / 'runtime/src/iop/r192_pic.cpp', W / 'runtime/src/iop/r192_aot.cpp')):
        obj = args.output / ('replacement-' + str(index) + '.obj')
        command = [str(args.compiler), '-std=c++20', '-O2', '-mavx2', '-ffp-contract=off', '-DNDEBUG',
                   '-I' + str(n / 'src/runtime/include'), '-I' + str(n / 'src/runtime/src'),
                   '-I' + str(n / 'src/runtime/src/iop'), '-I' + str(W / 'runtime/src'), '-c', str(source), '-o', str(obj)]
        p = subprocess.run(command, capture_output=True, text=True)
        (args.output / ('compile-' + str(index) + '.log')).write_text(p.stdout + p.stderr, encoding='utf-8', newline='\n')
        compile_receipts.append({'argv': command, 'exit_code': p.returncode, 'source_sha256': sha(source),
                                 'object_sha256': sha(obj) if obj.exists() else None})
        require(p.returncode == 0, 'disposable_IOP_replacement_build_failed')
        replacement_objects.append(obj)
    exe = out / 'ps2run.exe'
    link_map = args.output / 'iop-reference-AOT-link.map'
    link = ['-mavx2', '-ffp-contract=off', '-O3', '-DNDEBUG', '-mwindows', '-static-libgcc', '-static-libstdc++',
            '-Wl,-Map,' + str(link_map), '-Wl,--wrap=_ZN3gen9unit_0018ERN3ps23CpuEjj',
            '-Wl,--wrap=_ZN3gen9unit_0020ERN3ps23CpuEjj', '-Wl,--wrap=_ZN3gen9unit_0025ERN3ps23CpuEjj',
            *(str(p) for p in host_paths + replacement_objects + objects), str(libraries[0]), str(libraries[1]),
            '-lbcrypt', '-lwinmm', str(libraries[2]), str(libraries[3]), '-lws2_32', '-luserenv', '-lntdll',
            '-lsynchronization', '-lpthread', '-lm', '-lkernel32', '-luser32', '-lgdi32', '-lwinspool', '-lshell32',
            '-lole32', '-loleaut32', '-luuid', '-lcomdlg32', '-ladvapi32', '-o', str(exe)]
    response = args.output / 'link.rsp'; response.write_text('\n'.join(quote_response(x) for x in link) + '\n', encoding='utf-8', newline='\n')
    command = [str(args.compiler), '@' + str(response)]
    p = subprocess.run(command, capture_output=True, text=True)
    (args.output / 'link.log').write_text(p.stdout + p.stderr, encoding='utf-8', newline='\n')
    require(p.returncode == 0, 'disposable_static_IOP_PE_link_failed')
    nm = args.compiler.with_name('llvm-nm.exe')
    symbols = subprocess.run([str(nm), '-C', '--defined-only', str(exe)], capture_output=True, text=True)
    (args.output / 'defined-symbols.log').write_text(symbols.stdout + symbols.stderr, encoding='utf-8', newline='\n')
    forbidden = [line for line in symbols.stdout.splitlines() if any(x in line for x in (
        'ps2::iop::(anonymous namespace)::bad(', 'ps2::iop::(anonymous namespace)::probe(',
        'ps2::iop::(anonymous namespace)::load_probes('))]
    require(symbols.returncode == 0 and not forbidden, 'original_IOP_interpreter_object_detected')
    require('ps2::iop::kernel::execute_aot_v8(' in symbols.stdout, 'AOT_dispatcher_not_linked')
    dlls = []
    for source in sorted((n / 'out').glob('*.dll')):
        target = out / source.name
        with target.open('xb') as stream: stream.write(source.read_bytes())
        require(sha(source) == sha(target), 'support_DLL_copy_failed')
        dlls.append({'source': str(source), 'sha256': sha(source)})
    package = [sys.executable, str(W / 'tools/research/package_r192_precompiled.py'), '--harvest', str(args.harvest),
               '--destination', str(out / 'aot-modules/ee'), '--compiler', str(args.compiler),
               '--abi-include', str(args.harvest / 'copies/game/ee-live/include'), '--output', str(args.output / 'EE-package-receipt')]
    reused_package = None
    if args.reuse_ee_package:
        source = args.reuse_ee_package.resolve()
        require(source.is_relative_to(W / 'build') and not source.is_relative_to(n.resolve()), 'owned_nonprotected_package_source_required')
        manifest = source / 'manifest.txt'
        require(sha(manifest) == '20e7a347164805453fe608d7226d3cb87f318dc0c3e285ea4ba8856fc4b4f86d', 'sealed_EE_manifest_changed')
        lines = manifest.read_text(encoding='ascii').splitlines()
        require(len(lines) == 3570 and lines[0] == 'R192_AOT_EE_V1 A2659FA9BD846661 1456 328 3569', 'sealed_package_domain_mismatch')
        destination = out / 'aot-modules/ee'; destination.mkdir(parents=True, exist_ok=False)
        for line in lines[1:]:
            match = re.fullmatch(r'([0-9a-f]{64}) ([01]) (\d+) ([A-Za-z0-9_.-]+\.dll)', line)
            require(match is not None, 'invalid_sealed_package_row')
            original, target = source / match[4], destination / match[4]
            require(sha(original) == match[1], 'sealed_EE_payload_changed')
            os.link(original, target) # Exclusive publication; never opens either file for writing.
            require(sha(original) == sha(target) == match[1], 'hardlinked_EE_payload_mismatch')
        with (destination / 'manifest.txt').open('xb') as stream: stream.write(manifest.read_bytes())
        reused_package = {'source': str(source), 'manifest_sha256': sha(manifest), 'DLLs': 3569,
                          'mechanism': 'fresh exclusive hardlinks; every source/target byte hash checked'}
    else:
        p = subprocess.run(package, capture_output=True, text=True)
        (args.output / 'package.log').write_text(p.stdout + p.stderr, encoding='utf-8', newline='\n')
        require(p.returncode == 0, 'disposable_EE_package_copy_failed')
    changed = [x['path'] for x in pins if sha(Path(x['path'])) != x['sha256']]
    require(not changed, 'original_input_changed_during_disposable_build')
    receipt = {'status': 'DISPOSABLE_V8_IOP_AOT_BUILD_PASS', 'classification': 'reference compatibility capability only',
               'native_credit': 0, 'candidate': str(exe), 'candidate_sha256': sha(exe), 'source_pins': pins,
               'compile_receipts': compile_receipts, 'link_command': command, 'link_arguments': link,
               'link_map_sha256': sha(link_map), 'original_interpreter_symbols': forbidden,
               'support_DLLs': dlls, 'EE_package_command': None if reused_package else package, 'input_changes': changed,
               'reused_package': reused_package,
               'limits': ['No gameplay/state-run pass is claimed by this build receipt.',
                          'Original v8 ABI/serialization retained; immediate-load reference behavior, not hardware correction.',
                          'Unknown import/code/entry refuses; no runtime instruction compiler or interpreter fallback.']}
    target = args.output / 'BUILD.json'; target.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'result': str(target), 'sha256': sha(target), 'candidate_sha256': sha(exe)}))


if __name__ == '__main__':
    main()
