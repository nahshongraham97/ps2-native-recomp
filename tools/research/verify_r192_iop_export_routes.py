"""Prove real original LIBSD registration and SDRDRV import routing, or refuse.

Uses actual scheduler/on_exit callbacks and the source-pinned v8 interpreter as
independent reference. Never installs a fabricated original export callback.
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('reference-proof', 'candidate-build', 'domain', 'output'):
        parser.add_argument('--' + key, type=Path, required=True)
        if key != 'output': parser.add_argument('--expected-' + key + '-sha', required=True)
    args = parser.parse_args()
    for key in ('reference_proof', 'candidate_build', 'domain', 'output'):
        setattr(args, key, getattr(args, key).resolve())
    require(args.output.is_relative_to(W / 'build') and not args.output.exists(), 'fresh_owned_output_required')
    for key in ('reference_proof', 'candidate_build', 'domain'):
        require(sha(getattr(args, key)) == getattr(args, 'expected_' + key + '_sha').lower(), 'input_receipt_pin_mismatch:' + key)
    reference = json.loads(args.reference_proof.read_text()); build = json.loads(args.candidate_build.read_text()); domain = json.loads(args.domain.read_text())
    require(reference['status'] == 'WHOLE_START_REFERENCE_COMPATIBILITY_PASS' and build['status'] == 'DISPOSABLE_V8_IOP_AOT_BUILD_PASS' and
            domain['loader_name_extension']['literal_pairs'] == 140, 'completed_pinned_sources_required')
    provider = next(r for r in domain['records'] if r['original_sha256'] == '94915a3d9673e8a370e144ae493552d5e1f3956bb9ae5bd91e2da5f36c83ce7d')
    consumer = next(r for r in domain['records'] if r['original_sha256'] == '85a465ab251bde0c3d33151b3a67ec2780d57cc40ebb7df9d5ecb7bad3396b16')
    irx = fixed.load_tool('original_export_call_irx', W / 'scripts/iop_irx_imports.py')
    image, load, _ = irx._parse_elf_load_image(Path(consumer['original_copy']).read_bytes())
    call_words = {0x85C: 0x8CE40004, 0x860: 0x0C000435, 0x864: 0, 0x868: 0x3C010000}
    for at, word in call_words.items():
        require(int.from_bytes(image[at:at + 4], 'little') == word, 'original_LIBSD4_caller_word_changed')
    pins = reference['source_pins'] + build['source_pins'] + domain['source_pins']
    for compiled in build['compile_receipts']:
        cmd = compiled['argv']; source = Path(cmd[cmd.index('-c') + 1]); obj = Path(cmd[cmd.index('-o') + 1])
        pins += [{'path': str(source), 'sha256': compiled['source_sha256']}, {'path': str(obj), 'sha256': compiled['object_sha256']}]
    kernel = args.candidate_build.parent / 'src/r192-v8-kernel.cpp'
    fixture = W / 'tests/test_r192_iop_v8_start.cpp'; source = W / 'tests/test_r192_iop_export_routes.cpp'
    pins += [{'path': str(p), 'sha256': sha(p)} for p in [Path(__file__).resolve(), fixture, source, args.reference_proof, args.candidate_build,
        args.domain, kernel, W / 'runtime/src/iop/r192_thbase42.hpp', Path(build['candidate'])]]
    pins += [{'path': r['original_copy'], 'sha256': r['original_sha256']} for r in (provider, consumer)]
    require(sha(Path(build['candidate'])) == build['candidate_sha256'] and all(sha(Path(p['path'])) == p['sha256'] for p in pins), 'original_source_object_or_binary_changed')
    registry = Path(domain['registry']['path']).with_suffix('.obj')
    require(sha(registry) == domain['registry']['object_sha256'], 'expanded_registry_object_changed')
    args.output.mkdir(parents=True, exist_ok=False)
    text = fixture.read_text(); require(text.count('int main(') == 1, 'published_fixture_main_anchor_changed')
    prefix = text[:text.index('int main(')]
    anchor = 'extern "C" void __real__ZN3ps23iop7executeERNS0_3CpuERx(Cpu &, std::int64_t &);'
    call = '    __real__ZN3ps23iop7executeERNS0_3CpuERx(cpu, budget);'
    require(prefix.count(anchor) == 1 and prefix.count(call) == 1, 'published_execute_observer_anchor_changed')
    prefix = prefix.replace(anchor, 'static bool r192_route_reference;\n' + anchor)
    prefix = prefix.replace(call, '    if (r192_route_reference) ps2::iop::reference_execute(cpu, budget);\n    else __real__ZN3ps23iop7executeERNS0_3CpuERx(cpu, budget);')
    # Observe the real pre-existing return stub after its original body; the
    # source adapter's 50M guard must never be mistaken for a completed return.
    observer_anchor = 'void service(std::uint32_t id, Cpu &cpu) {'
    reference_return = '    kernel::g_hle[id].function(cpu); // Actual, pre-existing registered native body.'
    compiled_return = '    } else if (observe) checkpoint(cpu);'
    require(prefix.count(observer_anchor) == 1 and prefix.count(reference_return) == 1 and prefix.count(compiled_return) == 1,
            'original_return_observer_anchors_changed')
    prefix = prefix.replace(observer_anchor, 'void record_original_export_return(std::uint32_t, Cpu &);\n' + observer_anchor)
    prefix = prefix.replace(reference_return, reference_return + '\n    record_original_export_return(id, cpu);')
    prefix = prefix.replace(compiled_return, compiled_return + '\n    if (!before) record_original_export_return(id, cpu);')
    prefix_path = args.output / 'published-fixture-prefix.inc'; prefix_path.write_text(prefix, encoding='utf-8', newline='\n')
    exe = args.output / 'test-original-export-routes.exe'; command = []
    for item in reference['build_command']:
        if item.startswith('-DR192_REFERENCE_KERNEL_SOURCE='): item = '-DR192_REFERENCE_KERNEL_SOURCE="' + kernel.as_posix() + '"'
        elif item == str(fixture): item = str(source)
        elif Path(item).name == 'iop_pic_registry.obj': item = str(registry)
        elif item == reference['build_command'][-1]: item = str(exe)
        command.append(item)
    command.insert(1, '-DR192_EXPORT_ROUTE_FIXTURE_PREFIX="' + prefix_path.as_posix() + '"')
    compiler = Path(command[0]); require(sha(compiler) == reference['compiler_sha256'], 'reference_compiler_changed')
    rsp = args.output / 'build.rsp'; rsp.write_text('\n'.join(builder.quote_response(x) for x in command[1:]) + '\n', encoding='utf-8', newline='\n')
    compiled = subprocess.run([command[0], '@' + str(rsp)], capture_output=True, text=True)
    (args.output / 'build.log').write_text(compiled.stdout + compiled.stderr, encoding='utf-8', newline='\n')
    require(compiled.returncode == 0, 'original_export_route_test_compile_failed')
    runtime = next(Path(x) for x in command if x.endswith('libps2runtime.a')).parent.parent
    env = {k: v for k, v in os.environ.items() if not k.startswith(('PS2_', 'MKA_', 'SDL_'))}
    env['PATH'] = str(runtime / 'out') + os.pathsep + str(compiler.parent) + os.pathsep + env.get('PATH', '')
    def run(route, mode, plant):
        label = route + '-' + mode + '-' + plant; prefix = args.output / label
        argv = [str(exe), provider['original_copy'], consumer['original_copy'], mode, route, str(prefix), plant]
        ran = subprocess.run(argv, capture_output=True, text=True, env=env, timeout=300)
        (args.output / (label + '.log')).write_text(ran.stdout + ran.stderr, encoding='utf-8', newline='\n')
        result = prefix.with_suffix('.json')
        return {'argv': argv, 'exit_code': ran.returncode, 'prefix': str(prefix), 'measurement': json.loads(result.read_text()) if result.exists() else None}
    cases, controls = [], []
    for route in ('filename', 'buffer'):
        runs = {mode: run(route, mode, 'none') for mode in ('reference', 'compiled')}
        comparisons = []
        for suffix in ('cpu', 'ram', 'trace', 'iop-state', 'export-return-cpu', 'export-return-value'):
            a = Path(runs['reference']['prefix'] + '.' + suffix); b = Path(runs['compiled']['prefix'] + '.' + suffix)
            comparisons.append({'artifact': suffix, 'reference_sha256': sha(a) if a.exists() else None,
                'compiled_sha256': sha(b) if b.exists() else None, 'equal': a.exists() and b.exists() and a.read_bytes() == b.read_bytes()})
        passed = all(r['exit_code'] == 0 and r['measurement'] and r['measurement']['status'] == 'PASS' for r in runs.values()) and all(c['equal'] for c in comparisons)
        cases.append({'route': route, 'runs': runs, 'comparisons': comparisons, 'status': 'PASS' if passed else 'REFUSED'})
        for plant, reason in [('provider-absent', 'original_libsd_export_not_registered'), ('wrong-version', 'original_library_version_or_ordinal_set_mismatch'),
                              ('wrong-export-target', 'IOP_AOT_REFUSED:'), ('unproved-zero-result', 'original_call_guest_guard_or_return_unproved')]:
            outcome = run(route, 'compiled', plant); measured = outcome['measurement'] or {}
            rejected = outcome['exit_code'] == 1 and (measured.get('failure') == reason or
                (plant == 'wrong-export-target' and measured.get('phase') == 'original_caller_stub_prefix' and measured.get('failure', '').startswith(reason)))
            controls.append({'route': route, 'plant': plant, 'run': outcome, 'status': 'REJECTED' if rejected else 'NOT_EXERCISED_OR_NOT_REJECTED'})
    changes = [p['path'] for p in pins if sha(Path(p['path'])) != p['sha256']]
    passed = not changes and all(c['status'] == 'PASS' for c in cases) and all(c['status'] == 'REJECTED' for c in controls)
    report = {'status': 'ORIGINAL_EXPORT_ROUTE_SCHEDULER_PASS' if passed else 'ORIGINAL_EXPORT_ROUTE_REFUSED',
        'classification': 'source-pinned original export connection capability only', 'native_credit': 0, 'source_pins': pins,
        'build_argv': command, 'executable_sha256': sha(exe), 'fixture_prefix_sha256': sha(prefix_path), 'cases': cases, 'controls': controls, 'input_changes': changes,
        'constructed_original_call_bundle': {'consumer_sha256': consumer['original_sha256'], 'words': [{'image_offset': at, 'file_offset': load['file_offset'] + at, 'word': f'{word:08X}'} for at, word in call_words.items()],
            'mode': 0, 'source': 'Actual LW A0,4(A3), JAL libsd#4 stub, NOP; actual loader-created constructed thread/stack/GP, source packet field=0.',
            'scope': 'Constructed five-instruction caller/stub prefix followed by a separate unchanged production call_guest export invocation. No historical RPC entry, contiguous full caller or native signature/ownership admission.'},
        'limits': ['Only the real LIBSD provider / SDRDRV consumer pair; no generic admission of 247 original export candidates.',
            'Reference compatibility uses frozen immediate loads; native ownership and hardware-correctness remain separate.',
            'Actual scheduler/start return/on_exit/lifetime evidence is measured; no manually cleared pending closure.',
            'Inside the unchanged call_guest adapter kSlice batches, compare ordered writes/services, raw returned CPU and full final CPU/RAM/IOP serializer; no per-instruction fingerprint claim there.',
            'An existing native HLE requiring nested guest execution still refuses under the independent fixture; this is reported rather than bypassed.',
            'ReleaseLibraryEntries/unload/version-compatibility beyond this exact original pair remain unproved.']}
    target = args.output / 'RESULT.json'; target.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'result': str(target), 'sha256': sha(target), 'status': report['status'],
                     'cases': [{'route': c['route'], 'status': c['status'], 'compiled': c['runs']['compiled']['measurement']} for c in cases]}))
    return 0 if passed else 1


if __name__ == '__main__': raise SystemExit(main())
