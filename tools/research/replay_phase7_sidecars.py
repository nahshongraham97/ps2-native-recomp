"""Serial, headless Phase7 count/capture replay over the protected sidecars.

Only copied program data, owned caches, logs, cards and capture outputs may be
written. Requested length, observed endpoint and checkpoint parity stay separate.
No original executable, card, state, input, game asset or branch is changed.
"""
from pathlib import Path
import argparse
import csv
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
RESERVE = 12 * 1024**3
CAMERAS = ('001a6410', '001a6510', '001a6540', '001a65a0', '001a66a0', '001a68f0',
           '001a6a10', '001a6a20', '001a6b30', '001a6be0', '001a6c30', '001a6d70',
           '001a6dd0', '001a6f40', '001a6fd0')


def need(ok, why):
    if not ok:
        raise ValueError(why)


def sha(path):
    result = hashlib.sha256()
    with Path(path).open('rb') as source:
        for block in iter(lambda: source.read(4 << 20), b''):
            result.update(block)
    return result.hexdigest()


def dump(path, obj):
    path.write_text(json.dumps(obj, indent=2) + '\n', encoding='utf8', newline='\n')


def header(path):
    text = path.read_text(encoding='utf8')
    need(text.startswith('ps2-inputs 1\n'), 'unsupported_inputs_format:' + path.name)
    start = re.search(r'^start state (\S+) frame (\d+)\s*$', text, re.M)
    end = re.search(r'^e (\d+)\s*$', text, re.M)
    need(start and end and int(end[1]) > int(start[2]), 'state_recording_bounds_required:' + path.name)
    card = re.search(r'^card (.+)$', text, re.M)
    need(not card or card[1].strip() == 'none', 'recorded_card_recipe_requires_explicit_private_copy:' + path.name)
    return dict(state=start[1], begin=int(start[2]), end=int(end[1]),
                requested_frames=int(end[1]) - int(start[2]), sha256=sha(path))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--parent-root', type=Path, required=True)
    parser.add_argument('--states-dir', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--only', help='One explicitly named pilot sidecar; omitted means all72.')
    parser.add_argument('--plan-only', action='store_true')
    parser.add_argument('--resume', action='store_true')
    parser.add_argument('--timeout-seconds', type=int, default=3600)
    args = parser.parse_args()
    parent = args.parent_root.resolve()
    states = args.states_dir.resolve()
    output = args.output.resolve()
    need(output.is_relative_to(ROOT / 'build/oracle-telemetry'), 'owned_output_required')
    need(not output.exists() or args.resume, 'fresh_output_or_explicit_resume_required')
    need(shutil.disk_usage(ROOT).free >= RESERVE, '12GiB_reserve')
    output.mkdir(parents=True, exist_ok=args.resume)
    exe = ROOT / 'b/phase7-capture-out/ps2run.exe'
    original = ROOT / 'build/r192-hooks/game/disc/SLUS_214.10'
    disc = parent / 'external/forks/ps2-native-recomp-backup-20261110/games/mkarmageddon/disc.iso'
    need(exe.is_file() and original.is_file() and disc.is_file(), 'known_oracle_inputs_required')
    manifest = parent / 'launchers/oracle_original_manifest.json'
    expected = json.loads(manifest.read_text())
    unit_text = (ROOT / 'build/generated-aot/phase4-all/unit_0029.cpp').read_text()
    camera_entries = {pc.lower(): int(entry) for entry, pc in re.findall(r'case (\d+)u: goto L_([0-9A-Fa-f]+);', unit_text)}
    need(all(pc in camera_entries for pc in CAMERAS), 'camera_entry_map_required')
    paths = sorted(states.glob('*.inputs'))
    need(len(paths) == 72, 'original72_sidecars_required')
    if args.only:
        paths = [q for q in paths if q.name == args.only]
        need(len(paths) == 1, 'pilot_sidecar_not_found')
    plan = []
    for path in paths:
        meta = header(path)
        state = states / meta['state']
        need(state.is_file(), 'paired_state_missing:' + meta['state'])
        key = 'games/mkarmageddon/disc/states/' + path.name
        state_key = 'games/mkarmageddon/disc/states/' + state.name
        need(key in expected and state_key in expected, 'original_manifest_entry_missing')
        need(meta['sha256'] == expected[key]['sha256'] and path.stat().st_size == expected[key]['size'], 'original_input_identity_changed')
        need(sha(state) == expected[state_key]['sha256'] and state.stat().st_size == expected[state_key]['size'], 'original_state_identity_changed')
        plan.append(dict(sidecar=path.name, **meta, state_sha256=expected[state_key]['sha256']))
    pins = {str(q): sha(q) for q in (Path(__file__).resolve(), exe, original, manifest,
            ROOT / 'b/phase7-capture-src/runtime/src/phase7_capture.cpp',
            ROOT / 'b/phase7-capture-src/host/mka_unit_wrappers.cpp',
            ROOT / 'b/phase7-capture-src/runtime/src/quick_keys.inc',
            ROOT / 'build/generated-aot/phase4-all/unit_0029.cpp')}
    if (output / 'PLAN.json').exists():
        old = json.loads((output / 'PLAN.json').read_text())
        need(old['source_pins'] == pins and old['sidecars'] == plan, 'resume_source_or_input_change')
    else:
        dump(output / 'PLAN.json', dict(source_pins=pins, sidecars=plan,
            scope='All72 by default, one child oracle at a time. Overlay/internal same-unit call counting remains a separately reported coverage limit.'))
    if args.plan_only:
        print('PLAN_ONLY sidecars=' + str(len(plan)), flush=True)
        return 0
    game = output / 'private-program'
    game.mkdir(exist_ok=True)
    program = game / 'SLUS_214.10'
    if not program.exists():
        shutil.copyfile(original, program)
    need(sha(program) == pins[str(original)], 'private_program_copy_identity')
    cache = ROOT / 'b/phase7-capture-run'
    compiler = parent.parent / 'dependencies/compiler/llvm-mingw-20261006-ucrt-x86_64/bin'
    cases = []
    totals = {pc: 0 for pc in CAMERAS}
    # Whole batch lock: another invocation cannot start a second owned oracle.
    lock = (cache / 'serial-replay.lock').open('a+b')
    if os.name == 'nt':
        import msvcrt
        lock.seek(0)
        if lock.read(1) == b'':
            lock.write(b'0'); lock.flush()
        lock.seek(0); msvcrt.locking(lock.fileno(), msvcrt.LK_NBLCK, 1)
    try:
        for index, meta in enumerate(plan):
            need(shutil.disk_usage(ROOT).free >= RESERVE, '12GiB_reserve_pause')
            case = output / ('case_' + str(index).zfill(2))
            receipt = case / 'RESULT.json'
            if receipt.exists():
                row = json.loads(receipt.read_text())
                need(row['sidecar'] == meta['sidecar'], 'resume_case_identity')
                cases.append(row)
                for pc, count in row.get('capture_counts', {}).items():
                    totals[pc] = totals.get(pc, 0) + count
                continue
            case.mkdir(exist_ok=True)
            targets = [f'29:{camera_entries[pc]}:{pc}' for pc in CAMERAS if totals[pc] < 4]
            targets += ['18:309:00162e70']
            env = dict(os.environ)
            for key in list(env):
                if key.startswith(('PS2_CAPTURE', 'PS2_EE_LIVE', 'PS2_VU_', 'MKA_')):
                    del env[key]
            env.update(PS2_CAPTURE_COUNT='1', PS2_CAPTURE_DIR=str(case), PS2_CAPTURE_MAX='1',
                PS2_CAPTURE_TARGETS=','.join(targets), PS2_CAPTURE_REQUIRE_RETURN='1', PS2_RECORD='0',
                PS2_EE_LIVE=str(cache / 'ee-live'), PS2_VU_LIVE=str(cache / 'vu1-live'),
                PS2_VU_RECORD=str(case / 'vu-record'), PS2_THREADS='0',
                PATH=str(compiler) + os.pathsep + str(exe.parent) + os.pathsep + env.get('PATH', ''))
            argv = [str(exe), str(program), '--disc', str(disc), '--load-state', str(states / meta['state']),
                    '--replay', str(states / meta['sidecar']), '--replay-continue', '--frames', str(meta['end'] + 1),
                    '--quick-dir', str(case / 'quick'), '--no-record']
            print('START ' + meta['sidecar'] + ' requested=' + str(meta['requested_frames']), flush=True)
            started = time.monotonic(); reason = None
            with (case / 'oracle.log').open('wb') as log:
                child = subprocess.Popen(argv, cwd=case, stdout=log, stderr=subprocess.STDOUT, env=env,
                    creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
                while child.poll() is None:
                    try:
                        child.wait(timeout=30)
                    except subprocess.TimeoutExpired:
                        print('RUNNING ' + meta['sidecar'] + ' seconds=' + str(int(time.monotonic() - started)), flush=True)
                        if shutil.disk_usage(ROOT).free < RESERVE:
                            reason = '12GiB_reserve_pause'
                        elif time.monotonic() - started > args.timeout_seconds:
                            reason = 'owned_oracle_timeout'
                        if reason:
                            child.terminate(); child.wait(); break
            text = (case / 'oracle.log').read_text(errors='replace')
            observed = None
            if (case / 'observed_end.tsv').exists():
                observed = int((case / 'observed_end.tsv').read_text().split()[1])
            completed = observed is not None and observed >= meta['end'] and (
                '[replay] same to the end:' in text or '[replay] played on to frame ' in text)
            counts = []
            if (case / 'unit_entry_counts.tsv').exists():
                counts = list(csv.DictReader((case / 'unit_entry_counts.tsv').open(), delimiter='\t'))
            captures = {}
            for q in case.glob('cap_*.bin'):
                pc = q.name.split('_')[1]; captures[pc] = captures.get(pc, 0) + 1
            for pc, count in captures.items():
                totals[pc] = totals.get(pc, 0) + count
            row = dict(**meta, exit_code=child.returncode, seconds=time.monotonic() - started,
                completed_recording_length=completed, observed_end_frame=observed,
                parity='SAME_TO_END' if completed and child.returncode == 0 else 'DIVERGED_FULL_LENGTH' if completed else 'NOT_COMPLETED',
                stop_reason=reason, argv=argv, environment={k: v for k, v in env.items() if k.startswith('PS2_')},
                log_sha256=sha(case / 'oracle.log'), unit_entry_counts=counts, capture_counts=captures,
                captures=[dict(file=q.name, bytes=q.stat().st_size, sha256=sha(q)) for q in sorted(case.glob('cap_*.bin'))])
            dump(receipt, row); cases.append(row)
            dump(output / 'RESULT.json', dict(status='IN_PROGRESS', cases=cases, completed_cases=len(cases), planned_cases=len(plan)))
            print('END ' + meta['sidecar'] + ' ' + row['parity'] + ' observed=' + str(observed), flush=True)
            if reason == '12GiB_reserve_pause':
                break
        need(all(sha(q) == value for q, value in pins.items()), 'producer_or_inputs_changed_during_batch')
        dump(output / 'RESULT.json', dict(status='ALL_PLANNED_CASES_ATTEMPTED' if len(cases) == len(plan) else 'PAUSED_RESERVE',
            source_pins=pins, cases=cases, planned_cases=len(plan), attempted_cases=len(cases),
            full_length=sum(r['completed_recording_length'] for r in cases),
            same_to_end=sum(r['parity'] == 'SAME_TO_END' for r in cases),
            diverged_full_length=sum(r['parity'] == 'DIVERGED_FULL_LENGTH' for r in cases),
            limits=['Count-only call reachability is not native equivalence or ownership.',
                    'Only268 linked base AOT units are wrapped; overlay DLL and internal same-unit edge coverage is not asserted.',
                    'Ordinary caller return/SP is necessary for a saved capture, not sufficient for complete effect/ABI admission.']))
    finally:
        lock.close()
    return 0 if len(cases) == len(plan) else 2


if __name__ == '__main__':
    raise SystemExit(main())
