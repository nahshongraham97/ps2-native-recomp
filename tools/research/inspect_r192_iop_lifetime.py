"""Read only known typed IOP references through the pinned actual v8 loader.

This is a lifecycle inventory, not an unload/boot/SDK-root reconstruction.
Original kernel, library and SIF sources compile in separate translation units;
there are no private-layout casts, source mutations or guessed state offsets.
"""
from __future__ import annotations
import argparse
from collections import Counter
import json
import os
from pathlib import Path
import shutil
import subprocess

import generate_r192_iop_aot as common

W, require, sha = common.W, common.require, common.file_sha
KERNEL_SHA = '694c9e1fe073d383ffa8b7151468ccfca4271c5aec277f2f6f22929253f279bc'
RUNTIME_SHA = '2f7d85506da2298aea415a343b099e2f280e6349ac2604ac55cc4c3a126e08ec'
COMPILER_SHA = 'ee85bef2c733e24a26c22be3df94bd631cf9528a048fce91241c182a936737fe'
ORIGINAL_SOURCES = {
    'iop/kernel.cpp': KERNEL_SHA,
    'iop/libs.cpp': 'eede03dab9428e6dba8459ced623641348ad032ec90e1435e9ea352f6082d468',
    'iop/sif.cpp': '3733e2ce5d2ea691f313ce0c1389d86cf5231342ddad7dd063a2da123813ccf0',
    'system.cpp': '7a75bfed5758f57d4979f919ffa9526a7af337fd7a86871fb9bdcc1c6ea93384',
}
FIELDS = {
    'STATE': 'frame version cpu_bytes', 'CONTEXT': 'current nesting suspended pc npc ra now',
    'MODULE': 'id base size entry gp name_hex', 'ALLOCATION': 'base size',
    'PROVIDER': 'name_hex address header0 clients version_flags name0 name1',
    'THREAD': 'id status entry pc npc ra stack stack_size rpc_server on_exit',
    'INTERRUPT': 'line handler argument mode', 'JOB': 'at sequence job_kind a b c d opaque_action',
    'VBLANK': 'edge handler argument', 'ALARM': 'id handler argument active',
    'TIMER': 'id handler common running generation',
    'SERVER': 'id hle sd sid function callback queue', 'GUEST_SERVER': 'id hle sd sid function callback queue',
    'COMMAND': 'id handler data', 'RPC_THREAD': 'queue thread',
}


def decode(text):
    records = []
    for line in text.splitlines():
        values = line.split(); require(values and values[0] in FIELDS, 'unknown_native_inventory_record')
        names = FIELDS[values[0]].split(); require(len(values) == len(names) + 1, 'native_record_field_count')
        row = {'kind': values[0]}
        for name, value in zip(names, values[1:]):
            if name.endswith('_hex'):
                row[name[:-4]] = bytes.fromhex(value if value != '-' else '').decode('latin1')
                row[name] = value
            else: row[name] = int(value)
        records.append(row)
    require(sum(r['kind'] == 'STATE' for r in records) == 1 and sum(r['kind'] == 'CONTEXT' for r in records) == 1,
            'one_actual_restored_state_required')
    return records


def analyze(records):
    modules = [r for r in records if r['kind'] == 'MODULE' and r['size']]
    allocations = [r for r in records if r['kind'] == 'ALLOCATION']
    def owners(address):
        address &= 0x1fffffff
        return [m['id'] for m in modules if m['base'] <= address < m['base'] + m['size']]
    def allocated(base, size):
        return [a for a in allocations if a['base'] <= base and base + size <= a['base'] + a['size']]
    references = []
    for row in records:
        kind = row['kind']
        keys = {'CONTEXT': ['pc', 'npc', 'ra'], 'THREAD': ['entry', 'pc', 'npc', 'ra'],
                'INTERRUPT': ['handler'], 'VBLANK': ['handler'], 'ALARM': ['handler'],
                'TIMER': ['handler'], 'SERVER': ['function', 'callback'],
                'GUEST_SERVER': ['function', 'callback'], 'COMMAND': ['handler']}.get(kind, [])
        # Source sif.cpp schedules Job{kJobSifDmaCallback, callback, data}.
        # Other job.a fields are IDs/data and must not be scanned as code.
        if kind == 'JOB' and row['job_kind'] == 11: keys = ['a']
        for field in keys:
            address = row[field]
            if not address: continue
            references.append({'source_kind': kind, 'source_id': row.get('id', row.get('line', row.get('sequence'))),
                               'field': field, 'address': f'{address:08x}', 'module_range_candidates': owners(address),
                               'source_activity': (bool(row['current']) if kind == 'CONTEXT' else
                                   row.get('active', row.get('running'))),
                               'scope': 'retained typed reference only; range does not prove active execution, callable ABI or code identity'})
    providers = []
    for row in records:
        if row['kind'] != 'PROVIDER': continue
        providers.append(dict(row, module_range_candidates=owners(row['address']),
                              header_marker_canonical=row['header0'] == 0x41c00000,
                              original_source_identity='not serialized', sdk_registration_history='not serialized'))
    overlaps = []
    for i, a in enumerate(modules):
        for b in modules[i + 1:]:
            if max(a['base'], b['base']) < min(a['base'] + a['size'], b['base'] + b['size']):
                overlaps.append({'module_ids': [a['id'], b['id']], 'names': [a['name'], b['name']]})
    return {'record_counts': dict(sorted(Counter(r['kind'] for r in records).items())),
            'modules': [dict(m, containing_allocations=allocated(m['base'], m['size'])) for m in modules],
            'overlapping_module_ranges': overlaps, 'providers': providers, 'known_code_references': references,
            'retirement_admission': 'REFUSED:opaque_guest_callbacks_and_lifetime_unproved',
            'SDK_registry_reconstruction': 'REFUSED:serialized_host_map_has_no_SDK_root_or_registration_history'}


def classification_controls():
    fixture = [dict(kind='MODULE', id=i, base=0x20000, size=0x100, entry=0x20020, gp=0, name=str(i)) for i in (1, 2)]
    fixture += [dict(kind='PROVIDER', name='test', address=0x20000, header0=0x41c00000),
                dict(kind='JOB', job_kind=1, a=0x20020), dict(kind='JOB', job_kind=11, a=0x20024)]
    out = analyze(fixture)
    require(out['providers'][0]['module_range_candidates'] == [1, 2], 'ambiguous_owner_was_selected')
    require(len(out['known_code_references']) == 1 and out['known_code_references'][0]['address'] == '00020024',
            'job_ID_was_invented_callback')
    require(out['retirement_admission'].startswith('REFUSED:') and out['SDK_registry_reconstruction'].startswith('REFUSED:'),
            'inventory_invented_lifetime_or_SDK_history')
    return [{'name': name, 'status': 'REJECTED'} for name in
            ('choose_one_overlapping_owner', 'reinterpret_timer_ID_as_callback', 'invent_retirement_from_known_refs', 'invent_SDK_root_from_provider_map')]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for key in ('runtime-root', 'compiler', 'output'): p.add_argument('--' + key, type=Path, required=True)
    p.add_argument('--state', type=Path, action='append', required=True)
    args = p.parse_args()
    for name in ('runtime_root', 'compiler', 'output'): setattr(args, name, getattr(args, name).resolve())
    states = [s.resolve() for s in args.state]
    require(args.output.is_relative_to(W / 'build/oracle-telemetry') and not args.output.exists(), 'fresh_owned_output_required')
    require(all(s.is_relative_to(W / 'build/oracle-telemetry') and s.suffix == '.state' for s in states), 'owned_state_copies_only')
    require(len(set(states)) == len(states), 'duplicate_state_input')
    n = args.runtime_root; runtime = n / 'out/libps2runtime.a'; src = n / 'src/runtime/src'
    require(sha(runtime) == RUNTIME_SHA and sha(args.compiler) == COMPILER_SHA and
            all(sha(src / path) == digest for path, digest in ORIGINAL_SOURCES.items()),
            'frozen_runtime_compiler_identity_required')
    helper = Path(__file__).with_suffix('.cpp')
    files = [Path(__file__), helper, runtime, args.compiler] + [src / x for x in
        ('system.cpp', 'iop/kernel.cpp', 'iop/libs.cpp', 'iop/sif.cpp', 'iop/cpu.cpp', 'iop/kernel.hpp', 'iop/iop.hpp')] + states
    pins = [{'path': str(f), 'sha256': sha(f)} for f in files]
    args.output.mkdir(parents=True, exist_ok=False)
    archive = args.output / 'source-archive'; archive.mkdir()
    for f in [Path(__file__), helper]: (archive / f.name).write_bytes(f.read_bytes())
    (args.output / 'INPUT_PINS.json').write_text(json.dumps(pins, indent=2) + '\n', encoding='utf-8', newline='\n')
    base = [str(args.compiler), '-std=c++20', '-O2', '-mavx2', '-static-libgcc', '-static-libstdc++',
            '-I' + str(n / 'src/runtime/include'), '-I' + str(src), '-I' + str(src / 'iop'),
            '-DR192_ORIGINAL_KERNEL="' + (src / 'iop/kernel.cpp').as_posix() + '"',
            '-DR192_ORIGINAL_LIBS="' + (src / 'iop/libs.cpp').as_posix() + '"',
            '-DR192_ORIGINAL_SIF="' + (src / 'iop/sif.cpp').as_posix() + '"']
    builds, objects = [], []
    for part in range(4):
        require(shutil.disk_usage(W).free > (12 << 30) + (128 << 20), 'reserved_disk_space_required')
        obj = args.output / f'component-{part}.obj'; objects.append(obj)
        cmd = base + [f'-DR192_LIFETIME_COMPONENT={part}', '-c', str(helper), '-o', str(obj)]
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=180)
        (args.output / f'build-{part}.log').write_text(result.stdout + result.stderr, encoding='utf-8', newline='\n')
        require(result.returncode == 0, 'inventory_compile_failed:' + str(part))
        builds.append({'argv': cmd, 'object_sha256': sha(obj)})
    exe = args.output / 'inspect-lifetime.exe'
    cmd = base + list(map(str, objects)) + [str(runtime), '-lbcrypt', '-lwinmm',
        str(n / 'src/third_party/sdl2/lib/libSDL2.dll.a'), str(n / 'src/third_party/wgpu-native/lib/libwgpu_native.a'),
        '-lws2_32', '-luserenv', '-lntdll', '-lsynchronization', '-lpthread', '-lm', '-lkernel32', '-luser32', '-lgdi32',
        '-lwinspool', '-lshell32', '-lole32', '-loleaut32', '-luuid', '-lcomdlg32', '-ladvapi32', '-o', str(exe)]
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=180)
    (args.output / 'link.log').write_text(result.stdout + result.stderr, encoding='utf-8', newline='\n')
    require(result.returncode == 0, 'inventory_link_failed')
    env = {k: v for k, v in os.environ.items() if not k.startswith(('PS2_', 'MKA_', 'SDL_'))}
    env['PATH'] = str(n / 'out') + os.pathsep + str(args.compiler.parent) + os.pathsep + env.get('PATH', '')
    def run(state, label):
        cmd = [str(exe), str(state)]; r = subprocess.run(cmd, capture_output=True, text=True, env=env, timeout=120)
        (args.output / (label + '.stdout')).write_text(r.stdout, encoding='utf-8', newline='\n')
        (args.output / (label + '.stderr')).write_text(r.stderr, encoding='utf-8', newline='\n')
        return r, {'argv': cmd, 'exit_code': r.returncode, 'stdout_sha256': common.sha(r.stdout.encode()), 'stderr': r.stderr}
    cases = []
    for i, state in enumerate(states):
        r, receipt = run(state, f'state-{i:02}')
        require(r.returncode == 0, 'actual_state_restore_refused:' + str(i))
        records = decode(r.stdout); data = analyze(records)
        data.update({'state': str(state), 'state_sha256': sha(state), 'run': receipt, 'records': records})
        target = args.output / f'state-{i:02}.json'; target.write_text(json.dumps(data, indent=2) + '\n', encoding='utf-8', newline='\n')
        cases.append({'path': str(target), 'sha256': sha(target), 'frame': next(r['frame'] for r in records if r['kind'] == 'STATE'),
                      'record_counts': data['record_counts'], 'overlapping_module_ranges': data['overlapping_module_ranges'],
                      'provider_count': len(data['providers']), 'canonical_active_provider_headers': sum(p['header_marker_canonical'] for p in data['providers'])})
    # Fail-closed front-end and full-parser controls; originals stay untouched.
    original = states[0].read_bytes(); controls = classification_controls()
    for name, data, expected in [('wrong_version', original[:8] + (9).to_bytes(4, 'little') + original[12:64], 'only_pinned_v8_admitted'),
                                 ('truncated_header', original[:11], 'state_magic_or_header'),
                                 ('truncated_body', original[:1024], 'actual_serializer_refused')]:
        mutant = args.output / (name + '.state'); mutant.write_bytes(data)
        r, receipt = run(mutant, name)
        rejected = r.returncode == 1 and 'REFUSED:' + expected in r.stderr
        controls.append({'name': name, 'status': 'REJECTED' if rejected else 'NOT_REJECTED', 'run': receipt})
    changed = [pin['path'] for pin in pins if sha(Path(pin['path'])) != pin['sha256']]
    passed = not changed and all(c['status'] == 'REJECTED' for c in controls)
    report = {'status': 'READ_ONLY_ACTUAL_SERIALIZER_LIFETIME_INVENTORY_PASS' if passed else 'REFUSED',
        'classification': 'read-only actual-state ownership inputs; no module boot/unload/native proof', 'native_credit': 0,
        'source_pins': pins, 'compile_receipts': builds, 'link_argv': cmd, 'executable_sha256': sha(exe), 'cases': cases,
        'controls': controls, 'input_changes': changed,
        'next_dependencies': ['Fresh original loader must authenticate raw input before relocation and assign native owner generation.',
            'Byte-load replaces module/provider/reference maps; invalidate all old owner/record/callable handles before restore.',
            'Restored host provider map omits SDK root, registration order, original SHA and generations; do not fabricate them.',
            'Module range/allocator presence alone cannot establish code identity; preserve overlap/equivalent-code ambiguity.',
            'Known typed references do not enumerate opaque guest callback storage; generic retirement remains refused.',
            'IRQ/context and allocation/observer failure policies must close before runtime transition adoption.'],
        'limits': ['No boot, run, execute, callback, card/media mount or path-overload state loading.',
            'Three original implementation TUs use actual private source/types; no strict-alias/ODR/layout casts.',
            'Only named typed reference fields are read; no memory-value scan or inferred callback addresses.',
            'Actual serializer relinks existing HLE import IDs in private restored RAM, as original v8 specifies.',
            'An observed reference is not a proved callable ABI or an original SDK registration history.']}
    target = args.output / 'RESULT.json'; target.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    print(json.dumps({'result': str(target), 'sha256': sha(target), 'status': report['status']}))
    return 0 if passed else 1


if __name__ == '__main__': raise SystemExit(main())
