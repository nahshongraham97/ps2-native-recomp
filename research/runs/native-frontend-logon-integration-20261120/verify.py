"""Native queued retail LOGON/state/scheduler verification; offline codec frozen."""
import ctypes
import datetime
import importlib.util
import json
import os
import pathlib
import re
import subprocess
import sys

FORK = pathlib.Path(__file__).resolve().parents[3]
PARENT = FORK.parents[2]
COMMON = FORK / 'research/runs/native-movie-kernel-policies-20261119/validate.py'
spec = importlib.util.spec_from_file_location('_queued_logon_evidence', COMMON)
common = importlib.util.module_from_spec(spec)
spec.loader.exec_module(common)


def cache_contract():
    cache = (common.BUILD / 'CMakeCache.txt').read_text(encoding='utf-8-sig')
    common.require(bool(re.search(r'^PS2_GENERATED:PATH=$', cache, re.M)), 'native component build; no generated game target')
    common.require(bool(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$', cache, re.M)), 'Release')


if __name__ == '__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    require, sha, write = common.require, common.sha, common.write
    require(len(sys.argv) == 2 and re.fullmatch(r'validation-[0-9]+', sys.argv[1]), 'usage: verify.py validation-NN')
    out = pathlib.Path(__file__).resolve().parent / sys.argv[1]
    out.mkdir(exist_ok=False)
    common.OUT = out
    common.ENV = dict(os.environ, PATH=str(common.BIN) + os.pathsep + os.environ['PATH'])
    cache_contract()
    prior_path = FORK / 'research/runs/native-logon-input-task-lift-20261120/validation-01/verification.json'
    prior = json.loads(prior_path.read_bytes())
    require(prior['status'] == 'PASS_NATIVE_LOGON_INPUT_TASK_BINDING' and isinstance(prior['source_after_sha256'], dict), 'prior actual producer schema/status')
    require(len(prior['fixtures']) == 4 and prior['runtime_scenarios'] == 20, 'prior scoped result')
    changed = ['runtime/include/mka/game_action_input.hpp', 'runtime/src/native/game_action_input.cpp', 'cmake/native_game.cmake']
    new = [
        'runtime/include/mka/game_transition_state.hpp', 'runtime/src/native/game_transition_state.cpp', 'tests/test_game_transition_state.cpp',
        'runtime/include/mka/frontend_scheduler.hpp', 'runtime/src/native/frontend_scheduler.cpp', 'tests/test_frontend_scheduler.cpp',
        'runtime/include/mka/logon_frontend_task.hpp', 'runtime/src/native/logon_frontend_task.cpp', 'tests/test_logon_frontend_task.cpp']
    allowed = {str(FORK / name) for name in changed}
    untouched = {name: digest for name, digest in prior['source_after_sha256'].items() if name not in allowed}
    require(all(sha(name) == digest for name, digest in untouched.items()), 'all untouched prior source/compiler/asset/library identities')
    record_root = PARENT / 'research/runs/native-movie-ingress-20261119'
    reviews = [record_root / name for name in [
        'NATIVE_LOGON_FRONTEND_FINAL_SOURCE_CLOSURE_20261120.json', 'NATIVE_LOGON_FRONTEND_FINAL_PEER_CLOSURE_20261120.json']]
    review_status = ['SOURCE_REVIEW_LFB01_LFB02_HISTORY_POLICY_CLOSED_NOT_EXECUTED', 'SOURCE_REVIEW_NO_CONCRETE_DEFECT_NOT_BUILT_NOT_EXECUTED']
    for path, status in zip(reviews, review_status):
        review = json.loads(path.read_bytes())
        require(review['status'] == status and isinstance(review['pins'], dict), 'current independent source closure schema/status')
        require(all(sha(name) == digest for name, digest in review['pins'].items()), 'all final reviewed current source pins')
    evidence = [record_root / (stem + '_20261120.' + ext) for stem in [
        'NATIVE_LOGON_TRANSITION_GAME_STATE_SOURCE', 'NATIVE_LOGON_TRANSITION_STATE_IMPLEMENTATION',
        'NATIVE_LOGON_TRANSITION_MODE_ADDRESS_CORRECTION', 'NATIVE_GAME_TRANSITION_HISTORY_POLICY_CORRECTION',
        'NATIVE_FRONTEND_SCHEDULER_SOURCE', 'NATIVE_FRONTEND_SCHEDULER_IMPLEMENTATION',
        'NATIVE_FRONTEND_SCHEDULER_ALLOCATOR_REENTRY_CORRECTION', 'NATIVE_FRONTEND_SCHEDULER_TRANSITION_RECEIPT',
        'NATIVE_LOGON_FRONTEND_BINDING_SOURCE_REVIEW', 'NATIVE_LOGON_FRONTEND_STATE_BINDING_PEER_REVIEW',
        'NATIVE_LOGON_FRONTEND_MAINTENANCE_RECEIPT_CORRECTION', 'NATIVE_LOGON_FRONTEND_FINAL_SOURCE_CLOSURE',
        'NATIVE_LOGON_FRONTEND_FINAL_PEER_CLOSURE'] for ext in ['md', 'json']]
    paths = set(map(pathlib.Path, prior['source_after_sha256']))
    paths.update(FORK / name for name in changed + new)
    paths.update(evidence + [COMMON, pathlib.Path(__file__).resolve(), prior_path])
    before = {str(path): sha(path) for path in sorted(paths)}
    for name in changed + new:
        require(b'\r' not in (FORK / name).read_bytes(), 'LF current source ' + name)
    for name, expected in common.PRESERVE.items():
        require(sha(FORK / name) == expected, 'preserved IOP source ' + name)
    for name, digest in prior['generated_source_sha256'].items():
        require(sha(name) == digest, 'unchanged actual AOT native caller source')
    write('provenance.json', dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        fork_head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=FORK, text=True).strip(),
        parent_head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=PARENT, text=True).strip(),
        source_before_sha256=before, unchanged_prior_pins=len(untouched), preserved_dirty_sha256=common.PRESERVE,
        generated_source_sha256=prior['generated_source_sha256'],
        scope='Actual AOT LOGON owned native task/state/scheduler integration; game-effect probes and deterministic output ports; no codec content/corpus rerun'))
    try:
        libraries = PARENT.parent / 'dependencies/blender/blender-4.5.15-windows-x64/blender.shared'
        assets = FORK / 'games/mkarmageddon/disc-files/MOVIEPS2'
        overlay = PARENT / 'research/inputs/modules/MOVIER.ovl'
        cases = [
            ('state', 'test_native_game_transition_state', [], 'PASS native game transition context/state/allocation/ownership; no world teardown or scheduler execution', ['mka::game::NativeGameTransitionState::push_context']),
            ('scheduler', 'test_native_frontend_scheduler', [], 'PASS native frontend owned queue/live ordering/blocked handoff/lifetime; no game scheduler bootstrap or hardware tick', ['mka::game::NativeFrontendScheduler::dispatch', 'mka::game::NativeFrontendExecution::validate_transition_task', 'mka::input::NativeGameActionInput::suspend_inhibited']),
            ('queued-logon', 'test_native_logon_frontend', [libraries, assets, 'LOGON.SFD', overlay], 'PASS native queued LOGON blocking/transition/game state/owned jobs; deterministic ports and explicit game-effect probes',
             ['mka::native_logon::NativeLogonFrontendTask::update', 'mka::native_logon::NativeLogonState::poll', 'mka::game::NativeFrontendScheduler::dispatch', 'mka::game::NativeGameTransitionState::enter_transition']),
            ('actions', 'test_native_game_action_input', [], 'PASS native game action input ordered mappings/poll edges/LOGON skip; same mask suspension/typed existing task restore; no task execution', ['mka::input::NativeGameActionInput::suspend_after_logon']),
            ('input-tasks', 'test_native_game_input_tasks', [], 'PASS native owned game input task restore/lifetime/first match/allocation; no task body execution', ['mka::input::NativeGameInputTasks::retire']),
            ('caller', 'test_native_logon_runtime', [libraries, assets, 'LOGON.SFD', overlay], 'PASS generated native LOGON actual Service/input-task suspension/restore/identity; deterministic ports not full game effects', ['mka::native_logon::NativeLogonState::poll'])]
        common.execute('build.json', [common.CMAKE, '--build', common.BUILD, '--target', *[row[1] for row in cases], '-j2'])
        fixtures = []
        for label, target, args, footer, symbols in cases:
            print('RUN ' + label, flush=True)
            executable = common.BUILD / (target + '.exe')
            result = common.execute(label + '.json', [executable, *args])
            require(result['stdout'].splitlines()[-1] == footer, 'complete ' + label + ' assertions')
            audit = common.symbols(label + '-symbols', executable, result['executable_sha256'], symbols, True)
            fixtures.append(dict(component=label, executed_target_sha256=result['executable_sha256'], audit=audit))
        require(before == {name: sha(name) for name in before}, 'stable before/after source/compiler/asset/library/report pins')
        for name, digest in prior['generated_source_sha256'].items():
            require(sha(name) == digest, 'same source-checked AOT caller after execution')
        cache_contract()
        for name, expected in common.PRESERVE.items():
            require(sha(FORK / name) == expected, 'preserved IOP after execution')
        write('verification.json', dict(status='PASS_NATIVE_QUEUED_LOGON_STATE_SCHEDULER_INTEGRATION',
            source_after_sha256=before, unchanged_prior_pins=len(untouched), generated_source_sha256=prior['generated_source_sha256'],
            fixtures=fixtures, queued_runtime_scenarios=16, retained_caller_scenarios=20,
            limits=['Actual compiled native task calls the actual AOT LOGON/Service/input and game-state/scheduler components',
                'Six affected executables rebuilt/executed; unchanged prior pins checked; unrelated fixtures and offline decoder corpus/content not rerun',
                'Queued16 scenarios use real LOGON source with deterministic clock/audio/GPU and explicitly labeled game-effect probes',
                'Maintenance registration receipt proves active owned pending priority16 task placement, not task body or world cleanup semantics',
                'Source business history ninth-entry replacement preserved with host-owned storage; no guessed hardware-capacity classification',
                'Normal main-iteration/task outcome subset only; unported wait/delay/dependency outcomes are not claimed',
                'Real world/preparation/surround/maintenance/frontend bodies, bootstrap/settings/physical output and full-game acceptance remain OPEN',
                'No game, physical GPU/audio/input endpoint, local model/server or driver/settings action']))
        print(json.dumps(dict(status='PASS_NATIVE_QUEUED_LOGON_STATE_SCHEDULER_INTEGRATION', fixtures=len(fixtures), pins=len(before), queued_scenarios=16)), flush=True)
    except Exception as error:
        write('aborted.json', dict(status='NATIVE_QUEUED_LOGON_STATE_SCHEDULER_CHECK_FAILED', error=str(error)))
        raise
