"""Two source-ordered constructor owners; no game or endpoint execution."""
import ctypes
import datetime
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess

F = Path(__file__).resolve().parents[3]
P = F.parents[2]
HERE = Path(__file__).resolve().parent
OUT = HERE / 'validation-01'
BUILD = F / 'build_native_services_only_20261119-01'
BIN = P.parent / 'dependencies/compiler/llvm-mingw-20261006-ucrt-x86_64/bin'
CMAKE = P.parent / 'dependencies/cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write(name, value):
    with (OUT / name).open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(value, stream, indent=2)
        stream.write('\n')


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


if __name__ == '__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    OUT.mkdir(parents=True, exist_ok=False)
    c = module('native_checks', F / 'research/runs/native-movie-kernel-policies-20261119/validate.py')
    old = module('previous_connected_source', F / 'research/runs/native-connected-retail-entry-20261120/verify.py')
    c.OUT = OUT
    c.ENV = dict(os.environ, PATH=str(BIN) + os.pathsep + os.environ['PATH'])
    try:
        cache = (BUILD / 'CMakeCache.txt').read_bytes().decode('utf-8-sig').replace('\r\n', '\n')
        c.require(re.search(r'^PS2_GENERATED:PATH=$', cache, re.M), 'native-only build, no legacy game')
        extra = [
            'runtime/include/mka/retail_initial_objects.hpp', 'runtime/src/native/retail_initial_objects.cpp',
            'runtime/include/mka/retail_scene_events.hpp', 'runtime/src/native/retail_scene_events.cpp',
            'runtime/include/mka/retail_middleware_work.hpp', 'runtime/src/native/retail_middleware_work.cpp',
            'tests/test_retail_scene_owners.cpp', 'tests/test_retail_middleware_work.cpp',
        ]
        paths = sorted({*[F / name for name in old.NAMES + extra], Path(__file__).resolve()})
        source = {str(path): sha(path) for path in paths}
        protected = {
            F / 'runtime/src/iop/mcserv.cpp': 'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',
            F / 'runtime/src/iop/msl_native.cpp': '756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23',
        }
        for path, expected in protected.items():
            c.require(sha(path) == expected, 'protected source')
        write('provenance.json', {
            'at_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
            'fork_head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=F, text=True).strip(),
            'parent_head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=P, text=True).strip(),
            'sources': source, 'protected': {str(k): v for k, v in protected.items()},
            'compiler': str(BIN / 'clang++.exe'), 'compiler_sha256': sha(BIN / 'clang++.exe'),
            'scope': 'Native scene structure and typed middleware test ports; no retail resource/node/job bodies or physical endpoints.',
        })
        targets = ['test_native_retail_scene_owners', 'test_native_retail_middleware_work',
            'test_native_retail_crt', 'test_native_retail_main_timing', 'test_native_retail_startup']
        c.execute('build.json', [CMAKE, '--build', BUILD, '--target', *targets, '-j2'])
        fixtures = OUT / 'owned-fixtures'
        fixtures.mkdir()
        crt_root = fixtures / 'crt'
        crt_root.mkdir()
        runs = []
        for name, args, footer, required in [
            ('test_native_retail_scene_owners', [],
             'PASS native scene constructor/gates/structural ownership; no populated retail effects or game/device',
             ['mka::retail::NativeRetailSceneEventGraph::']),
            ('test_native_retail_middleware_work', [],
             'PASS native middleware transfer/retry/ticket/lifetime checks; test ports only, no retail jobs or game/device',
             ['mka::retail::NativeRetailMiddlewareDispatcher::', 'mka::retail::NativeRetailMiddlewareWorkGroup::']),
            ('test_native_retail_crt', [crt_root],
             'PASS production native CRT ownership/lifecycle, deferred work and callbacks; no game or device',
             ['mka::retail::initialize_native_retail_crt(']),
            ('test_native_retail_main_timing', [],
             'PASS native main shared-clock stopwatches and source parsed setting; no game/device',
             ['mka::retail::NativeRetailMainTiming::']),
            ('test_native_retail_startup', [fixtures / 'startup'],
             'PASS generated retail startup owns actual arguments/content/memory group; failed initialization is unpublished and full entry link remains blocked',
             ['mka::retail::initialize_retail_memory_owners(']),
        ]:
            exe = BUILD / (name + '.exe')
            result = c.execute(name + '.json', [exe, *args])
            c.require(result['stdout'].strip() == footer and not result['stderr'], name + ' full assertions')
            c.symbols(name + '-symbols', exe, result['executable_sha256'], required, True)
            runs.append({'name': name, 'binary_sha256': result['executable_sha256']})
        archive = BUILD / 'libmka_native_retail_startup.a'
        c.symbols('archive-symbols', archive, sha(archive), [
            'mka::retail::main_binding::construct_retail_initial_aggregate(',
            'mka::retail::main_binding::construct_retail_scene_event_graph(',
            'mka::retail::main_binding::construct_retail_middleware_work_group('], False)
        nm = c.execute('link-obligations.json', [BIN / 'llvm-nm.exe', '-C', archive])
        declared = set(re.findall(r' U mka::retail::main_binding::(\w+)\(', nm['stdout']))
        defined = set(re.findall(r' [Tt] mka::retail::main_binding::(\w+)\(', nm['stdout']))
        missing = sorted(declared - defined)
        prior = json.loads((F / 'research/runs/native-connected-retail-entry-20261120/validation-03/verification.json').read_bytes().decode('utf-8'))
        expected = set(prior['missing_main_bodies']) - {'construct_retail_initial_aggregate', 'construct_retail_controller_owner'}
        expected |= {'reset_and_bind_retail_scene_event_graph', 'deliver_retail_builtin_scene_event'}
        c.require(set(missing) == expected, 'exact source-joined remaining bodies, not a guessed count')
        args = [str(CMAKE), '--build', str(BUILD), '--target', 'test_native_startup_missing_bindings', '-j2']
        process = subprocess.run(args, cwd=F, env=c.ENV, text=True, capture_output=True, creationflags=subprocess.CREATE_NO_WINDOW)
        write('full-entry-negative-link.json', {'argv': args, 'exit': process.returncode,
            'stdout': process.stdout, 'stderr': process.stderr, 'executable_sha256': sha(CMAKE), 'missing': missing})
        text = process.stdout + process.stderr
        c.require(process.returncode != 0 and all(('main_binding::' + name + '(') in text for name in missing),
            'every required body still blocks complete entry')
        c.require(not (BUILD / 'test_native_startup_missing_bindings.exe').exists(), 'no incomplete game entry delivered')
        c.require(source == {str(path): sha(path) for path in paths}, 'unchanged source through execution')
        for path, expected_hash in protected.items():
            c.require(sha(path) == expected_hash, 'protected source unchanged')
        value = {'status': 'PASS_NATIVE_RETAIL_CONSTRUCTOR_OWNERS_CPU_CHECKS',
            'sources': source, 'fixtures': runs, 'startup_archive_sha256': sha(archive),
            'missing_bodies': missing, 'complete_entry_linkable': False,
            'limits': ['Only source-verified structural graph and middleware ownership with test ports',
                'No populated scene effects, actual middleware jobs or complete frame pump',
                'No main-menu/combat, physical input/audio/GPU, movie/profile/save or all-game proof'],
            'game_executed': False, 'endpoints_opened': False}
        write('verification.json', value)
        print(json.dumps({k: value[k] for k in ['status', 'complete_entry_linkable', 'game_executed', 'endpoints_opened']}))
    except Exception as error:
        write('failure.json', {'status': 'FAILED', 'type': type(error).__name__, 'message': str(error)})
        raise
