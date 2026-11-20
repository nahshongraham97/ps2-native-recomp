"""Verify the actual typed LOGON input/task lift; decoder content stays frozen."""
import ctypes
import datetime
import importlib.util
import json
import os
import pathlib
import re
import struct
import subprocess
import sys

FORK = pathlib.Path(__file__).resolve().parents[3]
PARENT = FORK.parents[2]
COMMON = FORK / 'research/runs/native-movie-kernel-policies-20261119/validate.py'
spec = importlib.util.spec_from_file_location('_native_input_evidence', COMMON)
common = importlib.util.module_from_spec(spec)
spec.loader.exec_module(common)


def cache_contract():
    cache = (common.BUILD / 'CMakeCache.txt').read_text(encoding='utf-8-sig')
    common.require(bool(re.search(r'^PS2_GENERATED:PATH=$', cache, re.M)), 'no generated game target')
    common.require(bool(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$', cache, re.M)), 'Release native components')


def original_words(original, emitter):
    """Independent ELF mapping of both compile-time word arrays; no game execution."""
    data = original.read_bytes()
    phoff = struct.unpack_from('<I', data, 28)[0]
    stride, count = struct.unpack_from('<HH', data, 42)
    loads = []
    for index in range(count):
        kind, offset, address, _, size = struct.unpack_from('<IIIII', data, phoff + index * stride)
        if kind == 1:
            loads.append((offset, address, size))
    text = emitter.read_text(encoding='utf-8')
    arrays = re.findall(r'constexpr NativeClosureWord (?:words|input_effect_words)\[\]=\{(.*?)\};', text, re.S)
    common.require(len(arrays) == 2, 'explicit original caller and input effect word arrays')
    result = []
    for array in arrays:
        for pc, word in re.findall(r'\{0x([0-9a-f]+)u,0x([0-9a-f]+)u\}', array):
            pc, word = int(pc, 16), int(word, 16)
            matching = [(offset + pc - address) for offset, address, size in loads if address <= pc and pc + 4 <= address + size]
            common.require(len(matching) == 1, 'unique original load mapping')
            actual = struct.unpack_from('<I', data, matching[0])[0]
            common.require(actual == word, 'actual original LE32 word ' + hex(pc))
            result.append(dict(original_pc=hex(pc), word=hex(actual), file_offset=matching[0]))
    common.require(len(result) == 60, '28 caller plus32 input/task source words')
    return result


if __name__ == '__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    common.require(len(sys.argv) == 2 and re.fullmatch(r'validation-[0-9]+', sys.argv[1]), 'usage: verify.py validation-NN')
    out = pathlib.Path(__file__).resolve().parent / sys.argv[1]
    out.mkdir(exist_ok=False)
    common.OUT = out
    common.ENV = dict(os.environ, PATH=str(common.BIN) + os.pathsep + os.environ['PATH'])
    sha, write, require = common.sha, common.write, common.require
    cache_contract()
    prior = FORK / 'research/runs/native-retail-caller-lift-20261120/validation-native-logon-02/verification.json'
    previous = json.loads(prior.read_bytes())
    require(previous['status'] == 'PASS_NATIVE_TYPED_LOGON_CALLER_INTEGRATION', 'saved prior producer status/schema')
    require(isinstance(previous['source_after_sha256'], dict) and len(previous['fixtures']) == 18, 'saved prior pin/fixture schema')
    changed = [
        'runtime/include/mka/game_action_input.hpp', 'runtime/src/native/game_action_input.cpp',
        'tests/test_game_action_input.cpp', 'cmake/native_game.cmake',
        'recomp/native_closure_codegen.cpp', 'tests/test_native_closure_codegen.cpp',
        'tests/test_native_logon_runtime.cpp']
    new = ['runtime/include/mka/game_input_tasks.hpp', 'runtime/src/native/game_input_tasks.cpp', 'tests/test_game_input_tasks.cpp']
    allowed = {str(FORK / name) for name in changed}
    untouched = {name: digest for name, digest in previous['source_after_sha256'].items() if name not in allowed}
    require(all(sha(name) == digest for name, digest in untouched.items()), 'all unchanged prior source/compiler/asset/library pins')
    source_records = [PARENT / 'research/runs/native-movie-ingress-20261119' / name for name in [
        'NATIVE_GAME_INPUT_TASK_EFFECT_SOURCE_20261120.md', 'NATIVE_GAME_INPUT_TASK_EFFECT_SOURCE_20261120.json',
        'NATIVE_GAME_INPUT_TASK_IMPLEMENTATION_20261120.md', 'NATIVE_GAME_INPUT_TASK_IMPLEMENTATION_20261120.json',
        'NATIVE_GAME_INPUT_TASK_RETIREMENT_CORRECTION_20261120.md', 'NATIVE_GAME_INPUT_TASK_RETIREMENT_CORRECTION_20261120.json',
        'NATIVE_GAME_INPUT_TASK_ATOMICITY_TEST_CORRECTION_20261120.md', 'NATIVE_GAME_INPUT_TASK_ATOMICITY_TEST_CORRECTION_20261120.json',
        'NATIVE_LOGON_INPUT_BINDING_SOURCE_REVIEW_20261120.md', 'NATIVE_LOGON_INPUT_BINDING_SOURCE_REVIEW_20261120.json',
        'NATIVE_LOGON_INPUT_TASK_SOURCE_REVIEW_20261120.md', 'NATIVE_LOGON_INPUT_TASK_SOURCE_REVIEW_20261120.json',
        'NATIVE_LOGON_INPUT_TASK_ATOMICITY_REVIEW_20261120.md', 'NATIVE_LOGON_INPUT_TASK_ATOMICITY_REVIEW_20261120.json']]
    record_root = source_records[0].parent
    binding = json.loads((record_root / 'NATIVE_LOGON_INPUT_BINDING_SOURCE_REVIEW_20261120.json').read_bytes())
    require(binding['status'] == 'SOURCE_REVIEW_NO_CONCRETE_DEFECT_NOT_EXECUTED' and isinstance(binding['pins'], dict), 'binding source review schema/status')
    for name in ['recomp/native_closure_codegen.cpp', 'cmake/native_game.cmake', 'tests/test_native_closure_codegen.cpp', 'tests/test_native_logon_runtime.cpp']:
        require(binding['pins'][str(FORK / name)] == sha(FORK / name), 'current caller binding reviewed ' + name)
    closure = json.loads((record_root / 'NATIVE_LOGON_INPUT_TASK_ATOMICITY_REVIEW_20261120.json').read_bytes())
    require(closure['status'] == 'SOURCE_REVIEW_LIT01_CLOSED_NOT_EXECUTED' and closure['closed_finding'] == 'LIT01 masked wrong-thread atomicity test', 'independent correction closure schema/status')
    for key in ['initial_report', 'correction_report']:
        require(sha(closure[key + '_path']) == closure[key + '_sha256'], 'preserved independent ' + key)
    for key in ['before_fixture', 'current_fixture']:
        require(sha(closure[key]['path']) == closure[key]['sha256'], 'explicit fixture before/current identity')
    require(all(sha(name) == digest for name, digest in closure['production_current_sha256'].items()), 'current input/task production source independently reviewed')
    paths = set(map(pathlib.Path, previous['source_after_sha256']))
    paths.update(FORK / name for name in changed + new)
    paths.update(source_records + [COMMON, pathlib.Path(__file__).resolve(), prior])
    before = {str(path): sha(path) for path in sorted(paths)}
    for name in changed + new:
        data = (FORK / name).read_bytes()
        require(b'\r' not in data, 'LF current authored source ' + name)
    for name, expected in common.PRESERVE.items():
        require(sha(FORK / name) == expected, 'preserved IOP source ' + name)
    original = PARENT / 'research/inputs/SLUS_214.10'
    source_words = original_words(original, FORK / 'recomp/native_closure_codegen.cpp')
    write('provenance.json', dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        fork_head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=FORK, text=True).strip(),
        parent_head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=PARENT, text=True).strip(),
        source_before_sha256=before, preserved_dirty_sha256=common.PRESERVE,
        unchanged_prior_pins=len(untouched), original_words=source_words,
        scope='Source-native LOGON post-movie input/task operation; no offline codec/content/corpus rerun'))
    try:
        libraries = PARENT.parent / 'dependencies/blender/blender-4.5.15-windows-x64/blender.shared'
        assets = FORK / 'games/mkarmageddon/disc-files/MOVIEPS2'
        overlay = PARENT / 'research/inputs/modules/MOVIER.ovl'
        cases = [
            ('actions', 'test_native_game_action_input', [], 'PASS native game action input ordered mappings/poll edges/LOGON skip; same mask suspension/typed existing task restore; no task execution',
             ['mka::input::NativeGameActionInput::suspend_after_logon', 'mka::input::NativeGameInputTasks::write_existing_restore']),
            ('tasks', 'test_native_game_input_tasks', [], 'PASS native owned game input task restore/lifetime/first match/allocation; no task body execution',
             ['mka::input::NativeGameInputTasks::retire', 'mka::input::NativeGameInputTasks::first']),
            ('emitter', 'test_native_closure_codegen', [original, out / 'validator-emission'],
             'PASS strict typed LOGON source recipe and owned native emission; runtime chain not executed', ['recomp::emit_native_logon_closure']),
            ('native-logon', 'test_native_logon_runtime', [libraries, assets, 'LOGON.SFD', overlay],
             'PASS generated native LOGON actual Service/input-task suspension/restore/identity; deterministic ports not full game effects',
             ['mka::native_logon::NativeLogonState::poll', 'mka::input::NativeGameActionInput::suspend_after_logon',
              'mka::input::NativeGameInputTasks::write_existing_restore'])]
        common.execute('build.json', [common.CMAKE, '--build', common.BUILD, '--target', *[row[1] for row in cases], 'eerecomp', '-j2'])
        fixtures = []
        for label, target, arguments, footer, symbols in cases:
            print('RUN ' + label, flush=True)
            executable = common.BUILD / (target + '.exe')
            result = common.execute(label + '.json', [executable, *arguments])
            require(result['stdout'].splitlines()[-1] == footer, 'complete ' + label + ' assertions')
            audit = common.symbols(label + '-symbols', executable, result['executable_sha256'], symbols, True)
            fixtures.append(dict(component=label, executed_target_sha256=result['executable_sha256'], audit=audit))
        generated = common.BUILD / ('native-retail-logon-' + sha(FORK / 'recomp/native_closure_codegen.cpp'))
        generated_pins = {str(generated / name): sha(generated / name) for name in ['native_logon.hpp', 'native_logon.cpp']}
        for name in ['native_logon.hpp', 'native_logon.cpp']:
            require(sha(generated / name) == sha(out / 'validator-emission' / name), 'actual generated/runtime source equals separate emission ' + name)
        compiler = common.execute('compiler-native-reuse.json', [common.BUILD / 'eerecomp.exe', '--native-logon', original, '--out', generated])
        require(compiler['stdout'].splitlines()[-1] == 'native LOGON exact existing source verified', 'actual strict compiler source reuse')
        require(before == {name: sha(name) for name in before}, 'stable source/compiler/asset/library pins')
        cache_contract()
        for name, expected in common.PRESERVE.items():
            require(sha(FORK / name) == expected, 'preserved IOP after execution')
        write('verification.json', dict(status='PASS_NATIVE_LOGON_INPUT_TASK_BINDING', source_after_sha256=before,
            generated_source_sha256=generated_pins, fixtures=fixtures, unchanged_prior_pins=len(untouched),
            original_source_words=len(source_words), runtime_scenarios=20,
            limits=['Actual linked source-native LOGON directly executes post-movie input/task state operation; no suspension callback remains',
                'Four affected executables rebuilt/executed; untouched prior source pins checked without rerunning unrelated fixtures',
                'Actual real LOGON control/session/resource counts through deterministic clock/audio/GPU ports, no content or corpus comparison',
                'Native task restore state is owned/retired, but actual task bodies/exit cleanup and full inhibit setter are still open',
                'Game preparation/render-surround/transition probes remain required; game bootstrap/settings/physical output and whole-game acceptance are open',
                'No game, physical input/audio/GPU endpoint, local model/server or driver/settings action']))
        print(json.dumps(dict(status='PASS_NATIVE_LOGON_INPUT_TASK_BINDING', fixtures=len(fixtures), pins=len(before), scenarios=20)), flush=True)
    except Exception as error:
        write('aborted.json', dict(status='NATIVE_LOGON_INPUT_TASK_BINDING_FAILED', error=str(error)))
        raise
