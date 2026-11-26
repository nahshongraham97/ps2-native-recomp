"""Replay whole original SDRDRV start through both frozen loader name paths.

Reuses the published independent reference fixture verbatim except the literal
load path. Compiles the actual disposable candidate's kernel source and exact
registry/body objects. Original N and the published fixtures remain untouched.
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
    old = json.loads(args.reference_proof.read_text()); build = json.loads(args.candidate_build.read_text()); domain = json.loads(args.domain.read_text())
    require(old['status'] == 'WHOLE_START_REFERENCE_COMPATIBILITY_PASS' and
            build['status'] == 'DISPOSABLE_V8_IOP_AOT_BUILD_PASS' and domain['loader_name_extension']['literal_pairs'] == 140,
            'completed_reference_build_and_alias_domain_required')
    pins = old['source_pins'] + build['source_pins'] + domain['source_pins']
    for compiled in build['compile_receipts']:
        command = compiled['argv']; source = Path(command[command.index('-c') + 1]); obj = Path(command[command.index('-o') + 1])
        require(sha(source) == compiled['source_sha256'] and sha(obj) == compiled['object_sha256'], 'candidate_compiled_source_or_object_changed')
        pins += [{'path': str(source), 'sha256': sha(source)}, {'path': str(obj), 'sha256': sha(obj)}]
    kernel = args.candidate_build.parent / 'src/r192-v8-kernel.cpp'
    fixture = W / 'tests/test_r192_iop_v8_start.cpp'; text = fixture.read_text()
    require(text.count('load_module_buffer(elf, "SDRDRV.IRX", "")') == 1, 'published_loader_fixture_anchor_changed')
    pins += [{'path': str(p), 'sha256': sha(p)} for p in [Path(__file__).resolve(), fixture, args.reference_proof,
             args.candidate_build, args.domain, kernel, W / 'runtime/src/iop/r192_thbase42.hpp', Path(build['candidate'])]]
    require(sha(Path(build['candidate'])) == build['candidate_sha256'], 'candidate_binary_changed')
    require(all(sha(Path(p['path'])) == p['sha256'] for p in pins), 'source_or_object_pin_changed')
    registry = Path(domain['registry']['path']).with_suffix('.obj')
    require(sha(registry) == domain['registry']['object_sha256'], 'expanded_registry_object_changed')
    args.output.mkdir(parents=True, exist_ok=False)
    original = Path(old['cases'][0]['runs']['compiled']['argv'][1])
    expected = next(p['sha256'] for p in old['source_pins'] if Path(p['path']) == original)
    require(sha(original) == expected, 'original_SDRDRV_changed')
    old_command = old['build_command']; compiler = Path(old_command[0])
    require(sha(compiler) == old['compiler_sha256'], 'reference_compiler_changed')
    env = {k: v for k, v in os.environ.items() if not k.startswith(('PS2_', 'MKA_', 'SDL_'))}
    # The unchanged original fixture already pins and uses the reference's runtime libraries.
    archive = next(Path(x) for x in old_command if x.endswith('libps2runtime.a'))
    runtime = archive.parent.parent
    env['PATH'] = str(runtime / 'out') + os.pathsep + str(compiler.parent) + os.pathsep + env.get('PATH', '')
    cases, controls, builds = [], [], []
    for route, path in [('filename', 'SDRDRV.IRX'), ('buffer', 'buffer')]:
        directory = args.output / route; directory.mkdir()
        source = directory / 'whole-start.cpp'
        source.write_text(text.replace('load_module_buffer(elf, "SDRDRV.IRX", "")', f'load_module_buffer(elf, "{path}", "")'), encoding='utf-8', newline='\n')
        exe = directory / 'whole-start.exe'; command = []
        for item in old_command:
            if item.startswith('-DR192_REFERENCE_KERNEL_SOURCE='): item = '-DR192_REFERENCE_KERNEL_SOURCE="' + kernel.as_posix() + '"'
            elif item == str(fixture): item = str(source)
            elif Path(item).name == 'iop_pic_registry.obj': item = str(registry)
            elif item == old_command[-1]: item = str(exe)
            command.append(item)
        require(command[-2] == '-o' and command[-1] == str(exe) and str(registry) in command, 'reference_build_layout_changed')
        rsp = directory / 'build.rsp'; rsp.write_text('\n'.join(builder.quote_response(x) for x in command[1:]) + '\n', encoding='utf-8', newline='\n')
        result = subprocess.run([str(compiler), '@' + str(rsp)], capture_output=True, text=True)
        (directory / 'build.log').write_text(result.stdout + result.stderr, encoding='utf-8', newline='\n')
        require(result.returncode == 0, 'loader_path_whole_fixture_compile_failed:' + route)
        builds.append({'route': route, 'argv': command, 'source_sha256': sha(source), 'executable_sha256': sha(exe)})
        for alias in (0, 0x80000000, 0xA0000000):
            case_dir = directory / f'alias-{alias:08x}'; case_dir.mkdir(); runs = {}
            for mode in ('reference', 'compiled'):
                prefix = case_dir / mode; argv = [str(exe), str(original), '0x27000', mode, str(prefix), hex(alias)]
                ran = subprocess.run(argv, capture_output=True, text=True, env=env, timeout=120)
                (case_dir / (mode + '.log')).write_text(ran.stdout + ran.stderr, encoding='utf-8', newline='\n')
                target = prefix.with_suffix('.json')
                runs[mode] = {'argv': argv, 'exit_code': ran.returncode, 'measurement': json.loads(target.read_text()) if target.exists() else None}
            comparisons = []
            for suffix in ('cpu', 'ram', 'trace', 'iop-state'):
                a, b = case_dir / ('reference.' + suffix), case_dir / ('compiled.' + suffix)
                comparisons.append({'artifact': suffix, 'reference_sha256': sha(a) if a.exists() else None,
                    'compiled_sha256': sha(b) if b.exists() else None, 'equal': a.exists() and b.exists() and a.read_bytes() == b.read_bytes()})
            passed = all(r['exit_code'] == 0 and r['measurement'] and r['measurement']['status'] == 'PASS' for r in runs.values()) and all(c['equal'] for c in comparisons)
            cases.append({'route': route, 'execution_alias': alias, 'runs': runs, 'comparisons': comparisons, 'status': 'PASS' if passed else 'REFUSED'})
        prefix = directory / 'unknown-import'; argv = [str(exe), str(original), '0x27000', 'compiled', str(prefix), '0', '--planted-unknown-import']
        ran = subprocess.run(argv, capture_output=True, text=True, env=env, timeout=120)
        (directory / 'unknown-import.log').write_text(ran.stdout + ran.stderr, encoding='utf-8', newline='\n')
        measured = json.loads(prefix.with_suffix('.json').read_text()) if prefix.with_suffix('.json').exists() else None
        rejected = bool(ran.returncode == 1 and measured and 'unknown_or_synthesized_HLE' in measured['refusal'] and
                        measured['synthesized_success_callbacks_executed'] == 0 and measured['native_service_calls'] == 0)
        controls.append({'route': route, 'argv': argv, 'exit_code': ran.returncode, 'measurement': measured,
                         'status': 'REJECTED' if rejected else 'NOT_REJECTED'})
    changed = [p['path'] for p in pins if sha(Path(p['path'])) != p['sha256']]
    passed = not changed and all(c['status'] == 'PASS' for c in cases) and all(c['status'] == 'REJECTED' for c in controls)
    report = {'status': 'WHOLE_ORIGINAL_FILENAME_BUFFER_START_PASS' if passed else 'LOADER_EXECUTION_REFUSED',
        'classification': 'frozen-v8 reference compatibility capability only', 'native_credit': 0, 'source_pins': pins,
        'candidate_sha256': build['candidate_sha256'], 'builds': builds, 'cases': cases, 'unknown_import_controls': controls, 'input_changes': changed,
        'limits': ['One actual original whole-module start, not all original module entrypaths or native acceptance.',
                   'No filename-to-buffer cross-comparison: argv strings and module names intentionally differ; each path compares its own unchanged reference.',
                   'Original CPU/RAM, every instruction fingerprint/ordered write, native-service transitions and original IOP serializer compared.',
                   'Manual scheduler on_exit closure excluded exactly as the published fixture; no persistent card/game run here.',
                   'Reference immediate-load semantics are separate from queued R3000 correctness.']}
    target = args.output / 'RESULT.json'; target.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'result': str(target), 'sha256': sha(target), 'status': report['status'], 'cases': len(cases)}))
    return 0 if passed else 1


if __name__ == '__main__': raise SystemExit(main())
