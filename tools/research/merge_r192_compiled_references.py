"""Combine measured IOP and typed EE reference components with static retail AOT.

Only a fresh disposable executable/package is written. This is an integration
test build, with the previously verified 1175-key VU archive, not full closure.
"""
from pathlib import Path
import argparse
import json
import os
import shutil
import subprocess

from extract_r192_elf_vu import require, sha

ROOT = Path(__file__).resolve().parents[2]


def quote(value):
    return '"' + str(value).replace('\\', '/').replace('"', '\\"') + '"'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('iop-build', 'preferred-freeze', 'static-build', 'compiler', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    for name in ('iop', 'preferred', 'static'):
        parser.add_argument('--expected-' + name + '-sha', required=True)
    args = parser.parse_args()
    out = args.output.resolve()
    require(out.is_relative_to(ROOT / 'build') and not out.exists(), 'fresh_owned_output_required')
    require(sha(args.iop_build) == args.expected_iop_sha and sha(args.preferred_freeze) == args.expected_preferred_sha and
            sha(args.static_build / 'RESULT.json') == args.expected_static_sha, 'build_receipt_pin')
    iop = json.loads(args.iop_build.read_text())
    preferred = json.loads(args.preferred_freeze.read_text())
    static = json.loads((args.static_build / 'RESULT.json').read_text())
    require(iop['status'] == 'DISPOSABLE_V8_IOP_AOT_BUILD_PASS' and not iop['input_changes'], 'measured_IOP_build_required')
    require(preferred['status'] == 'MEASURED_OBJECTS_WITH_EXACT_RECONSTRUCTED_ARGV' and not preferred['input_changes'] and
            preferred['source_state_version'] == 8 and preferred['sizeof_Cpu'] == 1456, 'measured_original_v8_preferred_build_required')
    require(sha(args.compiler) == preferred['compiler']['sha256'], 'compiler_identity_changed')
    require(static['binary_sha256'] == 'bfced6b44da3faa702e9bc88c0aae1e0a0b9510eb6b20f30ce7cd694fdd3cb31' and
            sha(args.static_build / 'out/ps2run.exe') == static['binary_sha256'], 'static_original_reference_changed')
    pins = list(iop['source_pins'])
    pins += [preferred[name] for name in ('factory_snapshot', 'corrected_factory', 'compiler', 'contract', 'contract_origin', 'original_N_lineage_record')]
    pins += preferred['runtime_witnesses'] + preferred['source_header_pins']
    pins += [check['pin'] for check in preferred['original_N_checks']]
    for record in preferred['private_objects']:
        pins += [record['source'], record['object']]
    objects = [Path(record['object']['path']) for record in preferred['private_objects']]
    require({p.name for p in objects} == {'dispatch.obj', 'kernel.obj', 'system.obj', 'reference_adapter.obj'}, 'exact_preferred_components_required')
    link = list(iop['link_arguments'])
    for item in link:
        if not item.startswith('-') and Path(item).suffix in ('.obj', '.a'):
            path = Path(item).resolve()
            pins.append({'path': str(path), 'sha256': sha(path)})
    pins += [{'path': str(p.resolve()), 'sha256': sha(p)} for p in [Path(__file__), args.iop_build,
             args.preferred_freeze, args.static_build / 'RESULT.json', args.static_build / 'out/libps2game.a']]
    require(all(sha(Path(pin['path'])) == pin['sha256'] for pin in pins), 'measured_component_changed')
    normal = ROOT / 'build/r192-compiled-konquest'
    for path in (normal / 'src/runtime/include/ps2').glob('*.hpp'):
        require(sha(path) == sha(args.static_build / 'src/runtime/include/ps2' / path.name), 'static_ABI_header_changed')
    require(shutil.disk_usage(ROOT / 'build').free > (12 << 30) + (1 << 30), 'disk_reserve_exhausted')
    out.mkdir(parents=True)
    target = out / 'out'
    target.mkdir()
    replaced = 0
    for index, value in enumerate(link):
        if value.startswith('-Wl,-Map,'):
            link[index] = '-Wl,-Map,' + str(out / 'merged-reference-link.map')
        elif Path(value).name == 'libps2game.a':
            link[index] = str((args.static_build / 'out/libps2game.a').resolve())
            replaced += 1
    require(replaced == 1 and link[-2] == '-o', 'original_link_shape_changed')
    link[-1] = str(target / 'ps2run.exe')
    link[0:0] = [str(path) for path in objects]
    response = out / 'link.rsp'
    response.write_text('\n'.join(quote(x) for x in link) + '\n', newline='\n')
    command = [str(args.compiler), '@' + str(response)]
    run = subprocess.run(command, capture_output=True)
    (out / 'link.log').write_bytes(run.stdout + run.stderr)
    require(run.returncode == 0, 'measured_component_merge_link_failed')
    for source in (normal / 'out').glob('*.dll'):
        shutil.copy2(source, target / source.name)
    original_ee = Path(iop['candidate']).parent / 'aot-modules/ee'
    manifest = original_ee / 'manifest.txt'
    require(sha(manifest) == '20e7a347164805453fe608d7226d3cb87f318dc0c3e285ea4ba8856fc4b4f86d', 'sealed_EE_manifest_changed')
    ee = target / 'aot-modules/ee'
    ee.mkdir(parents=True)
    for line in manifest.read_text().splitlines()[1:]:
        digest, rank, count, name = line.split()
        require(Path(name).name == name and sha(original_ee / name) == digest, 'sealed_EE_payload_changed')
        os.link(original_ee / name, ee / name)
        require(sha(ee / name) == digest, 'sealed_EE_link_changed')
    shutil.copy2(manifest, ee / 'manifest.txt')
    symbols = subprocess.run([str(args.compiler.with_name('llvm-nm.exe')), '-C', '--defined-only', str(target / 'ps2run.exe')], capture_output=True, text=True)
    (out / 'defined-symbols.log').write_text(symbols.stdout + symbols.stderr, newline='\n')
    require(symbols.returncode == 0 and 'mka::emitted::reference::preferred_family(' in symbols.stdout and
            'ps2::iop::kernel::execute_aot_v8(' in symbols.stdout, 'required_compiled_component_missing')
    require('ps2::iop::(anonymous namespace)::load_probes(' not in symbols.stdout, 'old_IOP_interpreter_object_selected')
    changes = [pin['path'] for pin in pins if sha(Path(pin['path'])) != pin['sha256']]
    require(not changes, 'component_changed_during_merge')
    result = {'status': 'DISPOSABLE_COMPILED_REFERENCE_MERGE_BUILT', 'native_credit': 0,
              'candidate': str(target / 'ps2run.exe'), 'candidate_sha256': sha(target / 'ps2run.exe'),
              'source_pins': pins, 'input_changes': changes, 'link_command': command, 'link_arguments': link,
              'scope': 'Measured typed EE + static original IOP + source-bounded static EE/1175 VU reference merge. No game run, normal rollout, complete source closure or final native acceptance.'}
    receipt = out / 'BUILD.json'
    receipt.write_text(json.dumps(result, indent=2) + '\n', newline='\n')
    print(json.dumps({'status': result['status'], 'candidate_sha256': result['candidate_sha256'], 'receipt_sha256': sha(receipt)}))


if __name__ == '__main__':
    main()
