"""Independently execute original LOADCORE6/7 against a typed native model.

Only test-owned data records are projected. This is not permission to mutate
original code/text tables or install the model into a game/kernel build.
"""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import subprocess

import generate_r192_iop_aot as fixed
import build_r192_iop_v8_candidate as builder

W, require, sha = fixed.W, fixed.require, fixed.file_sha
SDK_COMMIT = 'd4b9943bd4a58dbe69800bf4ec9d81640df5bc5b'
SDK = {
    'iop/system/loadcore/src/loadcore.c': '2eb29f11cce431de18f8d5f64c64e4eda22507bfca26487a57b53b63750e0d78',
    'iop/system/loadcore/src/exports.tab': '2be677ef32f1ece733861906b2c94b80d6da21d8789fdf12a0640012ec3ed63b',
    'iop/system/loadcore/include/xloadcore.h': 'd177e5b2ec3d6fb4ccac7e33ac32a7babc94350bfce95aa9d3ee8443b41d96e1',
    'iop/system/loadcore/include/loadcore.h': '50b145acd6da87563824378783d0607a5eb9d2606b3bc2b0a9760a1fdef7d983',
    'iop/kernel/include/irx.h': '2afc986bdf65970f599f635ed3c5c73353f8ed6a83f2ca186c285c927bdddff7',
    'iop/kernel/include/kerr.h': '8977f2f47b0e9fa10340c0d07163f01aaa5bd60d3866be9aca0929b058067f89',
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('reference-proof', 'candidate-build', 'domain', 'sdk-pins', 'output'):
        parser.add_argument('--' + key, type=Path, required=True)
        if key != 'output': parser.add_argument('--expected-' + key + '-sha', required=True)
    args = parser.parse_args()
    for key in ('reference_proof', 'candidate_build', 'domain', 'sdk_pins', 'output'):
        setattr(args, key, getattr(args, key).resolve())
    require(args.output.is_relative_to(W / 'build') and not args.output.exists(), 'fresh_owned_output_required')
    for key in ('reference_proof', 'candidate_build', 'domain', 'sdk_pins'):
        require(sha(getattr(args, key)) == getattr(args, 'expected_' + key + '_sha').lower(), 'input_receipt_pin_mismatch:' + key)
    reference = json.loads(args.reference_proof.read_text()); build = json.loads(args.candidate_build.read_text()); domain = json.loads(args.domain.read_text())
    sdk = json.loads(args.sdk_pins.read_text())
    require(sdk['commit'] == SDK_COMMIT and {p['path']: p['sha256'] for p in sdk['files']} == SDK, 'official_SDK_source_set_mismatch')
    for p in sdk['files']: require(sha(Path(p['copy'])) == p['sha256'], 'official_SDK_bytes_changed')
    require(reference['status'] == 'WHOLE_START_REFERENCE_COMPATIBILITY_PASS' and build['status'] == 'DISPOSABLE_V8_IOP_AOT_BUILD_PASS', 'completed_source_phase_required')
    source = W / 'tests/test_r192_library_registry_model.cpp'; header = W / 'runtime/src/iop/r192_library_registry_model.hpp'
    fixture = W / 'tests/test_r192_iop_v8_start.cpp'; kernel = args.candidate_build.parent / 'src/r192-v8-kernel.cpp'
    pins = reference['source_pins'] + build['source_pins'] + domain['source_pins']
    for c in build['compile_receipts']:
        cmd = c['argv']; pins += [{'path': cmd[cmd.index('-c') + 1], 'sha256': c['source_sha256']}, {'path': cmd[cmd.index('-o') + 1], 'sha256': c['object_sha256']}]
    pins += [{'path': str(p), 'sha256': sha(p)} for p in [Path(__file__).resolve(), source, header, fixture, kernel, args.sdk_pins, args.reference_proof, args.candidate_build, args.domain]]
    pins += [{'path': p['copy'], 'sha256': p['sha256']} for p in sdk['files']]
    originals = [r for r in domain['records'] if 'LOADCORE' in r['filename_loader_names']]
    require(len(originals) == 2, 'both_original_LOADCORE_variants_required')
    irx = fixed.load_tool('model_original_LOADCORE', W / 'scripts/iop_irx_imports.py')
    authority = []
    for row in originals:
        path = Path(row['original_copy']); data = path.read_bytes(); require(fixed.sha(data) == row['original_sha256'], 'original_LOADCORE_changed')
        image, load, _ = irx._parse_elf_load_image(data)
        require(int.from_bytes(image[0x1E40:0x1E44], 'little') == 0x41C00000 and image[0x1E4C:0x1E54] == b'loadcore' and
                int.from_bytes(image[0x1E6C:0x1E70], 'little') == 0x938 and int.from_bytes(image[0x1E70:0x1E74], 'little') == 0xB80,
                'original_export_6_7_slots_changed')
        require(load['file_size'] == 0x1F50 and load['memory_size'] > 0x1F74, 'original_LC_BSS_context_changed')
        functions = []
        for label, start, end in [('register6', 0x938, 0xB80), ('release7', 0xB80, 0xC40)]:
            functions.append({'name': label, 'image_range': [start, end], 'file_range': [load['file_offset'] + start, load['file_offset'] + end],
                'sha256': fixed.sha(image[start:end]), 'words': [{'image_offset': at, 'file_offset': load['file_offset'] + at,
                    'word': f'{int.from_bytes(image[at:at+4], "little"):08X}'} for at in range(start, end, 4)]})
        authority.append({'original': {'path': str(path), 'sha256': row['original_sha256']}, 'load': load, 'functions': functions,
                          'registry_root_image_offset': 0x1F50, 'pending_root_field_offset': 12, 'IRQ_callback_field_offset': 32})
        pins.append({'path': str(path), 'sha256': row['original_sha256']})
    require(authority[0]['functions'] == authority[1]['functions'], 'original_function_variants_differ')
    require(all(sha(Path(p['path'])) == p['sha256'] for p in pins), 'source_or_object_changed')
    args.output.mkdir(parents=True, exist_ok=False)
    text = fixture.read_text(); prefix = text[:text.index('int main(')]
    anchor = 'extern "C" void __real__ZN3ps23iop7executeERNS0_3CpuERx(Cpu &, std::int64_t &);'
    call = '    __real__ZN3ps23iop7executeERNS0_3CpuERx(cpu, budget);'
    require(prefix.count(anchor) == 1 and prefix.count(call) == 1, 'source_execute_adapter_anchors_changed')
    prefix = prefix.replace(anchor, 'static bool r192_route_reference;\n' + anchor).replace(call,
        '    if (r192_route_reference) ps2::iop::reference_execute(cpu, budget);\n    else __real__ZN3ps23iop7executeERNS0_3CpuERx(cpu, budget);')
    service = 'void service(std::uint32_t id, Cpu &cpu) {'
    ret = '    kernel::g_hle[id].function(cpu); // Actual, pre-existing registered native body.'
    aret = '    } else if (observe) checkpoint(cpu);'
    write = 'void record_write(std::uint32_t a, std::uint32_t width, std::uint32_t value) {'
    require(all(prefix.count(a) == 1 for a in (service, ret, aret, write)), 'independent_observer_anchors_changed')
    prefix = prefix.replace(service, 'void record_registry_original_return(std::uint32_t, Cpu &);\n' + service)
    prefix = prefix.replace(ret, ret + '\n    record_registry_original_return(id, cpu);')
    prefix = prefix.replace(aret, aret + '\n    if (!before) record_registry_original_return(id, cpu);')
    prefix = prefix.replace(write, 'void record_registry_metadata_write(std::uint32_t, std::uint32_t, std::uint32_t);\n' + write + '\n    record_registry_metadata_write(a, width, value);')
    prefix_path = args.output / 'independent-fixture-prefix.inc'; prefix_path.write_text(prefix, encoding='utf-8', newline='\n')
    registry = Path(domain['registry']['path']).with_suffix('.obj'); require(sha(registry) == domain['registry']['object_sha256'], 'registry_object_changed')
    command = []
    for item in reference['build_command']:
        if item.startswith('-DR192_REFERENCE_KERNEL_SOURCE='): item = '-DR192_REFERENCE_KERNEL_SOURCE="' + kernel.as_posix() + '"'
        elif item == str(fixture): item = str(source)
        elif Path(item).name == 'iop_pic_registry.obj': item = str(registry)
        command.append(item)
    command.insert(1, '-DR192_REGISTRY_FIXTURE_PREFIX="' + prefix_path.as_posix() + '"')
    compiler = Path(command[0]); require(sha(compiler) == reference['compiler_sha256'], 'compiler_changed')
    runtime = next(Path(x) for x in command if x.endswith('libps2runtime.a')).parent.parent
    env = {k: v for k, v in os.environ.items() if not k.startswith(('PS2_', 'MKA_', 'SDL_'))}
    env['PATH'] = str(runtime / 'out') + os.pathsep + str(compiler.parent) + os.pathsep + env.get('PATH', '')
    def compile_case(label, overlay=None):
        exe = args.output / (label + '.exe'); cmd = command[:-1] + [str(exe)]
        if overlay: cmd.insert(1, '-I' + str(overlay))
        rsp = args.output / (label + '.rsp'); rsp.write_text('\n'.join(builder.quote_response(x) for x in cmd[1:]) + '\n', encoding='utf-8', newline='\n')
        ran = subprocess.run([cmd[0], '@' + str(rsp)], capture_output=True, text=True)
        (args.output / (label + '-build.log')).write_text(ran.stdout + ran.stderr, encoding='utf-8', newline='\n')
        require(ran.returncode == 0, 'native_model_test_compile_failed:' + label)
        return exe, {'argv': cmd, 'executable_sha256': sha(exe)}
    def run(exe, label, original, mode):
        output = args.output / (label + '.json'); argv = [str(exe), original['original_copy'], mode, str(output)]
        ran = subprocess.run(argv, capture_output=True, text=True, env=env, timeout=120)
        (args.output / (label + '.log')).write_text(ran.stdout + ran.stderr, encoding='utf-8', newline='\n')
        return {'argv': argv, 'exit_code': ran.returncode, 'measurement': json.loads(output.read_text()) if output.exists() else None}
    exe, built = compile_case('model')
    cases = [run(exe, f'original-{i}-{mode}', original, mode) for i, original in enumerate(originals) for mode in ('reference', 'compiled')]
    controls = []
    mutations = [('notfound_zero', 'return not_found;', 'return ok;'), ('inuse_zero', 'if (current->clients) return in_use;', 'if (current->clients) return ok;'),
        ('equal_minor_allowed', '<= (old->version & 255u)', '< (old->version & 255u)'),
        ('locked_client_migrates', 'if (current->flags & 1u)', 'if (false)'),
        ('missing_canonical_restore', 'current->marker = Marker::canonical;', 'current->marker = Marker::linked;'),
        ('malformed_header_allowed', 'if (table->marker != Marker::canonical) return illegal;', 'if (false) return illegal;'),
        ('released_owner_retained', 'tables_.erase(current); validate();', 'validate();'),
        ('wrong_callable_slot', 'raw->functions[ordinal] : nullptr;', 'raw->functions[0] : nullptr;')]
    native = header.read_text()
    for label, before, after in mutations:
        require(native.count(before) >= 1 and (native.count(before) == 1 or label == 'notfound_zero'), 'semantic_mutation_anchor_changed:' + label)
        folder = args.output / ('mutant-' + label); (folder / 'iop').mkdir(parents=True)
        mutant = folder / header.name; mutant = folder / 'iop' / header.name
        mutant.write_text(native.replace(before, after), encoding='utf-8', newline='\n')
        bad, compiled = compile_case(label, folder); outcome = run(bad, label, originals[0], 'compiled')
        rejected = outcome['exit_code'] == 1 and outcome['measurement'] and outcome['measurement']['status'] == 'REFUSED'
        controls.append({'name': label, 'status': 'REJECTED' if rejected else 'NOT_REJECTED', 'mutant_header_sha256': sha(mutant), 'build': compiled, 'run': outcome})
    changes = [p['path'] for p in pins if sha(Path(p['path'])) != p['sha256']]
    passed = not changes and all(c['exit_code'] == 0 and c['measurement'] and c['measurement']['status'] == 'PASS' for c in cases) and all(c['status'] == 'REJECTED' for c in controls)
    report = {'status': 'TYPED_REGISTRY_ORIGINAL_PROJECTION_PASS' if passed else 'TYPED_REGISTRY_PROJECTION_REFUSED', 'classification': 'isolated native model capability only', 'native_credit': 0,
        'source_pins': pins, 'SDK': sdk, 'original_authority': authority, 'build': built, 'cases': cases, 'controls': controls, 'input_changes': changes,
        'limits': ['TEST-only raw metadata projection. No mutable .text or stub identity permission is installed.',
            'Original LOADCORE module boot is not run: zeroed BSS registry and absent IRQ callback table are explicit component preconditions.',
            'Native return and exact ordered projected metadata writes compared with complete original methods; stack/IRQ/cache transport remains original-reference-only.',
            'Existing-client and pending-client attachment are explicit seeded preconditions, not proof of full Link/UnLinkLibraryEntries or actual module unload.',
            'No game/kernel connector, arbitrary callback, ownership credit or null/malformed pointer execution.']}
    target = args.output / 'RESULT.json'; target.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'result': str(target), 'sha256': sha(target), 'status': report['status'], 'cases': [c['measurement'] for c in cases], 'controls': [(c['name'], c['status']) for c in controls]}))
    return 0 if passed else 1


if __name__ == '__main__': raise SystemExit(main())
