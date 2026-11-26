"""Run one pinned, headless R192 AOT case beside its sealed DLL package.

Only owned saved-state/recording/card copies are accepted. A frame probe is a
coverage diagnostic; a replay additionally requires every recorded checkpoint.
Neither result is full-game/native acceptance. --self-test-only never launches.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import time

W = Path(__file__).resolve().parents[2]
OWNED = W/'build/oracle-telemetry'
ELF_SHA = 'a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f'
ISO_SHA = '48e395b56fe6888d9013d36b4b04d2a1decf2f52259063936a8e9f53e824966d'
PRODUCER_SHA = '8ab43bda05e7a621b946883f3195a583f0c2455f0351bf8608282cd3e97b4617'
MANIFEST_SHA = '20e7a347164805453fe608d7226d3cb87f318dc0c3e285ea4ba8856fc4b4f86d'
SOURCE_PINS = {
    'runtime/src/system.cpp': '7a75bfed5758f57d4979f919ffa9526a7af337fd7a86871fb9bdcc1c6ea93384',
    'runtime/include/ps2/cpu.hpp': '2eb8c482859337add6480f0a1f0ad79ab908dda6d878d575f84f554ecf81c0fe',
    'runtime/include/ps2/state.hpp': 'fc49043208f3ec9f7593f0aeb55de15090637c96c3de4d036563c5d9ab0b5aac',
}
FRAME_OFFSET = 0x020045D4


def require(ok, reason):
    if not ok:
        raise ValueError(reason)


def hash_file(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for data in iter(lambda: stream.read(4*1024*1024), b''):
            h.update(data)
    return h.hexdigest()


def pin(path, expected=None):
    before = path.stat()
    digest = hash_file(path)
    after = path.stat()
    require((before.st_size, before.st_mtime_ns) == (after.st_size, after.st_mtime_ns), 'file_changed_during_hash:'+str(path))
    require(expected is None or digest == expected.lower(), 'pin_mismatch:'+str(path))
    return {'path': str(path.resolve()), 'sha256': digest, 'size': after.st_size, 'mtime_ns': after.st_mtime_ns}


def within(path, root):
    return path.resolve().is_relative_to(root.resolve())


def saved_frame(path):
    # Authenticated fixed prefix: magic8/version4/tag8/Cpu1456/RAM32MiB/scratch16KiB/base8/given8/frame8.
    with path.open('rb') as stream:
        head = stream.read(20)
        require(head == b'PS2STATE\x08\0\0\0ee\0\0\0\0\0\0', 'unsupported_saved_state_header')
        stream.seek(FRAME_OFFSET)
        raw = stream.read(8)
    require(len(raw) == 8, 'truncated_saved_state_prefix')
    return struct.unpack('<Q', raw)[0]


def recording(path, state, frame):
    lines = path.read_text(encoding='utf-8').splitlines()
    require(lines and lines[0] == 'ps2-inputs 1', 'unsupported_recording')
    starts = [re.fullmatch(r'start state (\S+) frame (\d+)', x) for x in lines if x.startswith('start ')]
    require(len(starts) == 1 and starts[0] is not None, 'ambiguous_recording_start')
    require(starts[0][1] == state.name and int(starts[0][2]) == frame, 'recording_saved_start_mismatch')
    endings = [re.fullmatch(r'e (\d+)', x) for x in lines if x.startswith('e ')]
    require(len(endings) == 1 and endings[0] is not None and lines[-1] == endings[0][0], 'recording_not_closed')
    end = int(endings[0][1])
    require(end > frame, 'recording_has_zero_new_frames')
    cards = [x.partition(' ')[2] for x in lines if x.startswith('card ')]
    require(len(cards) == 1 and cards[0] in ('state', 'file'), 'unproved_recording_card_kind')
    checks = [int(m[1]) for x in lines if (m := re.fullmatch(r'h (\d+) [0-9a-fA-F]{8}', x))]
    require(checks and len(checks) == len(set(checks)), 'missing_or_duplicate_checkpoints')
    require(all(frame <= x <= end for x in checks), 'checkpoint_outside_recording')
    events = [int(m[1]) for x in lines if (m := re.match(r'(?:p[12]|h) (\d+) ', x))]
    require(events and max(events) <= end, 'recording_footer_before_events')
    return {'start': frame, 'end': end, 'checkpoints': len(checks), 'card_kind': cards[0], 'footer': lines[-1]}


def assess(text, code, timed_out, start, end, mode, miss_text='', changed=None, checkpoint_count=None):
    objections = []
    if code != 0:
        objections.append('nonzero_exit')
    if timed_out:
        objections.append('timeout')
    loads = [int(x) for x in re.findall(r'^\[state\] loaded frame (\d+)\s*$', text, re.M)]
    if loads != [start]:
        objections.append('wrong_or_ambiguous_loaded_frame')
    stops = re.findall(r'^\[ee\] stopped after (\d+) frames, (\d+) cycles: (.*)$', text, re.M)
    expected_reason = 'frame limit reached' if mode == 'frames' else 'the replay is over'
    if len(stops) != 1 or int(stops[0][0]) != end or stops[0][2].strip() != expected_reason:
        objections.append('expected_clean_stop_missing')
    actual_new = int(stops[0][0])-start if len(stops) == 1 else None
    if end <= start or actual_new != end-start:
        objections.append('requested_additional_frames_not_measured')
    bad_patterns = {
        'EE_miss': r'missing code at|untranslated code|\[ee\].*\bmissing\b',
        'VU_miss': r'VU[01] AOT missing|\[vu[01]?[^\]]*\].*(?:unknown|missing|not compiled)',
        'bad_entry': r'bad[_ -]?entry',
        'runtime_compiler': r'\[(?:ee|vu)-live\b|\bcompiling\b|\btranslating code\b',
        'replay_difference': r'\[replay\].*(?:DIFFERENT|went another way|cannot|not a recording)',
        'unexpected_window_audio': r'\[window\]|\[audio\] sound device',
    }
    bad_lines = {name: [line for line in text.splitlines() if re.search(pattern, line, re.I)]
                 for name, pattern in bad_patterns.items()}
    objections += [name for name, lines in bad_lines.items() if lines]
    if miss_text.strip():
        objections.append('miss_file_not_empty')
    package = re.findall(r'^\[sealed-ee-aot\] registered (\d+) modules from (\d+) sealed precompiled DLLs; no runtime compiler\s*$', text, re.M)
    if package != [('3569', '3569')]:
        objections.append('sealed_package_registration_not_proved')
    summaries = re.findall(r'^\[replay\] same to the end: frames (\d+) to (\d+), (\d+) checkpoints all equal\s*$', text, re.M)
    if mode == 'replay':
        if summaries != [(str(start), str(end), str(checkpoint_count))] or not checkpoint_count:
            objections.append('full_recorded_checkpoint_match_not_proved')
    elif summaries:
        objections.append('unexpected_replay_summary')
    if changed:
        objections.append('input_or_binary_changed')
    return {'status': 'PASS' if not objections else 'REFUSED', 'objections': objections,
            'loaded_frames': loads, 'stop_summaries': stops, 'measured_new_frames': actual_new,
            'requested_new_frames': end-start, 'bad_lines': bad_lines,
            'miss_file_tokens': miss_text.split(), 'replay_summaries': summaries}


def self_test():
    start, end = 80225, 80315
    good = ('[sealed-ee-aot] registered 3569 modules from 3569 sealed precompiled DLLs; no runtime compiler\n'
            '[state] loaded frame 80225\n[ee] stopped after 80315 frames, 123 cycles: frame limit reached\n')
    require(assess(good, 0, False, start, end, 'frames')['status'] == 'PASS', 'self_test_positive_failed')
    cases = [
        ('absolute_limit_below_saved_frame', good, 0, False, start, 90, ''),
        ('nonzero_exit_with_clean_stop', good, 4, False, start, end, ''),
        ('wrong_saved_start', good.replace('loaded frame 80225', 'loaded frame 80224'), 0, False, start, end, ''),
        ('missing_code_with_clean_stop', good+'[ee] halted: missing code at 0x008A5140 (ra 0x002E6180)\n', 0, False, start, end, ''),
        ('timeout_with_clean_text', good, 0, True, start, end, ''),
        ('ambiguous_reload', good+'[state] loaded frame 80225\n', 0, False, start, end, ''),
        ('miss_file_without_stderr', good, 0, False, start, end, '8a5140\n'),
    ]
    controls = []
    for name, text, code, timeout, first, last, misses in cases:
        outcome = assess(text, code, timeout, first, last, 'frames', misses)
        require(outcome['status'] == 'REFUSED', 'planted_probe_accepted:'+name)
        controls.append({'name': name, 'status': 'REJECTED', 'objections': outcome['objections']})
    replay = good.replace('frame limit reached', 'the replay is over')+'[replay] same to the end: frames 80225 to 80315, 6 checkpoints all equal\n'
    require(assess(replay, 0, False, start, end, 'replay', checkpoint_count=6)['status'] == 'PASS', 'replay_positive_failed')
    require(assess(replay, 0, False, start, end, 'replay', checkpoint_count=7)['status'] == 'REFUSED', 'dropped_checkpoint_accepted')
    return {'status': 'PASS', 'synthetic_positive_controls': 2, 'earlier_seven_plants': controls,
            'extra_checkpoint_count_plant': 'REJECTED', 'measured_gameplay_cases': 0}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('state', 'inputs', 'card-copy', 'candidate', 'source-fork'):
        parser.add_argument('--'+name, type=Path)
    parser.add_argument('--expected-binary-sha')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--additional-frames', type=int)
    parser.add_argument('--self-test-only', action='store_true')
    args = parser.parse_args()
    require(within(args.output, OWNED) and not args.output.exists(), 'fresh_owned_output_required')
    tests = self_test()
    if args.self_test_only:
        args.output.mkdir(parents=True, exist_ok=False)
        tests['source_sha256'] = hash_file(Path(__file__))
        (args.output/'RESULT.json').write_text(json.dumps(tests, indent=2)+'\n', encoding='utf-8', newline='\n')
        print(json.dumps(tests))
        return 0
    require(all((args.state, args.card_copy, args.candidate, args.source_fork, args.expected_binary_sha)), 'missing_explicit_run_arguments')
    require(re.fullmatch(r'[0-9a-fA-F]{64}', args.expected_binary_sha) is not None, 'invalid_expected_binary_SHA')
    require(within(args.candidate, W/'build') and args.candidate.suffix.lower() == '.exe', 'candidate_must_stay_in_owned_build_with_package')
    require(within(args.state, OWNED) and args.state.suffix == '.state', 'owned_saved_state_copy_required')
    require(within(args.card_copy, OWNED) and args.card_copy.name == args.state.name+'.ps2' or
            (args.inputs is not None and within(args.card_copy, OWNED) and args.card_copy.name == args.inputs.name+'.ps2'),
            'explicit_owned_companion_card_required')
    if args.inputs:
        require(within(args.inputs, OWNED) and args.inputs.name.endswith('.inputs'), 'owned_recording_copy_required')
        require(args.additional_frames is None, 'replay_must_not_have_frame_cap')
    additional = args.additional_frames if args.additional_frames is not None else 90
    require(args.inputs is not None or additional > 0, 'positive_additional_frames_required')
    frame = saved_frame(args.state)
    session = recording(args.inputs, args.state, frame) if args.inputs else None
    if session:
        required_card = args.state.name+'.ps2' if session['card_kind'] == 'state' else args.inputs.name+'.ps2'
        require(args.card_copy.name == required_card, 'recording_companion_kind_mismatch')
    expected_end = session['end'] if session else frame+additional
    mode = 'replay' if session else 'frames'
    source = args.source_fork/'build/r192-hooks'
    producer = pin(source/'out/ps2run.exe', PRODUCER_SHA)
    source_pins = [pin(source/'src'/name, expected) for name, expected in SOURCE_PINS.items()]
    binary = pin(args.candidate, args.expected_binary_sha)
    manifest = args.candidate.parent/'aot-modules/ee/manifest.txt'
    manifest_pin = pin(manifest, MANIFEST_SHA)
    package_lines = manifest.read_text(encoding='ascii').splitlines()
    require(package_lines[0] == 'R192_AOT_EE_V1 A2659FA9BD846661 1456 328 3569' and len(package_lines) == 3570,
            'wrong_sealed_package_header')
    package_pins = []
    for line in package_lines[1:]:
        match = re.fullmatch(r'([0-9a-f]{64}) ([01]) (\d+) ([A-Za-z0-9_.-]+\.dll)', line)
        require(match is not None, 'invalid_sealed_package_row')
        package_pins.append(pin(manifest.parent/match[4], match[1]))
    iso = args.source_fork.parent/'ps2-native-recomp-backup-20261110/games/mkarmageddon/disc.iso'
    elf = source/'game/disc/SLUS_214.10'
    input_pins = [pin(args.state), pin(args.card_copy), pin(elf, ELF_SHA), pin(iso, ISO_SHA)]
    if args.inputs:
        input_pins.append(pin(args.inputs))
    args.output.mkdir(parents=True, exist_ok=False)
    copied_elf = args.output/'SLUS_214.10'
    copied_elf.write_bytes(elf.read_bytes())
    pin(copied_elf, ELF_SHA)
    disposable = args.output/'disposable-case-card.ps2'
    disposable.write_bytes(args.card_copy.read_bytes())
    pin(disposable, next(row['sha256'] for row in input_pins if row['path'] == str(args.card_copy.resolve())))
    quick = args.output/'quick'; quick.mkdir()
    miss_file = args.output/'misses.txt'
    command = [str(args.candidate.resolve()), str(copied_elf.resolve()), '--disc', str(iso.resolve()),
               '--memcard', str(disposable.resolve()), '--load-state', str(args.state.resolve()),
               '--no-record', '--quick-dir', str(quick.resolve()), '--misses', str(miss_file.resolve())]
    command += ['--replay', str(args.inputs.resolve())] if session else ['--frames', str(expected_end)]
    overrides = {'PS2_EE_LIVE': '0', 'PS2_VU_LIVE': '0', 'PS2_RECORD': '0', 'PS2_THREADS': '0',
                 'PS2_EE_LIVE_BG': '0', 'PS2_EE_LIVE_JOBS': '2', 'MKA_INTERCEPT_COMBAT': '1',
                 'MKA_TRACE_MODE_INPUT': '1', 'PS2_OBSERVER_HOOKS': '1', 'PS2_PROFILE_SUBSYSTEMS': '1',
                 'PS2_NATIVE_VERTEX': '1', 'PS2_MISS_DUMP_DIR': str((args.output/'misses').resolve()),
                 'PS2_VU_RECORD': str((args.output/'vu-record').resolve())}
    environment = dict(os.environ)
    # Profile/check/trace injection from another task must not alter this controlled run.
    removed = []
    for name in ('PS2_PROFILE', 'PS2_PROFILE_FROM', 'PS2_PRESS', 'PS2_VU1_EXACT', 'PS2_VU1_INTERPRET', 'PS2_VU1_CHECK'):
        if name in environment:
            removed.append(name); del environment[name]
    environment.update(overrides)
    stdout, stderr = args.output/'stdout.log', args.output/'stderr.log'
    timed_out = False
    began = time.monotonic()
    with stdout.open('xb') as out, stderr.open('xb') as err:
        process = subprocess.Popen(command, cwd=args.candidate.parent, env=environment, stdout=out, stderr=err,
                                   creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
        try:
            code = process.wait(timeout=600)
        except subprocess.TimeoutExpired:
            timed_out = True
            process.kill()  # Only this driver-owned child; no existing user process is touched.
            code = process.wait()
    elapsed = time.monotonic()-began
    changed = []
    for before in input_pins+[binary, manifest_pin]+package_pins:
        path = Path(before['path'])
        try:
            after = pin(path)
            if after['sha256'] != before['sha256']:
                changed.append({'before': before, 'after': after})
        except OSError as exc:
            changed.append({'before': before, 'error': str(exc)})
    text = stdout.read_text(encoding='utf-8', errors='replace')+'\n'+stderr.read_text(encoding='utf-8', errors='replace')
    misses = miss_file.read_text(encoding='utf-8', errors='replace') if miss_file.exists() else ''
    outcome = assess(text, code, timed_out, frame, expected_end, mode, misses, changed,
                     session['checkpoints'] if session else None)
    result = dict(outcome, mode=mode, command=command, cwd=str(args.candidate.parent.resolve()),
                  saved_start_frame=frame, expected_end_frame=expected_end, recording=session,
                  environment_overrides=overrides, removed_inherited_environment=removed,
                  exit_code=code, timed_out=timed_out, timeout_seconds=600, elapsed_seconds=elapsed,
                  input_pins=input_pins, candidate=binary, package_manifest=manifest_pin,
                  package_DLL_pins=package_pins, input_changes=changed, original_producer=producer,
                  prefix_source_pins=source_pins, source_sha256=hash_file(Path(__file__)), self_test=tests,
                  disposable_card={'path': str(disposable.resolve()), 'sha256_after': hash_file(disposable)},
                  log_pins=[pin(stdout), pin(stderr)],
                  scope='One actual headless case with no Live compiler, no window/audio, disposable companion card. Partial prefix parsing only; successful runtime load is separate. No full-game/native credit.')
    target = args.output/'RESULT.json'
    target.write_text(json.dumps(result, indent=2)+'\n', encoding='utf-8', newline='\n')
    print(json.dumps({'status': outcome['status'], 'result': str(target.resolve()), 'sha256': hash_file(target),
                      'new_frames': outcome['measured_new_frames'], 'objections': outcome['objections']}))
    return 0 if outcome['status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
