"""Production connected startup build; only native CPU/file fixtures execute."""
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
S = P / 'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
OUT = Path(__file__).resolve().parent / 'validation-03'
BUILD = F / 'build_native_services_only_20261119-01'
BIN = P.parent / 'dependencies/compiler/llvm-mingw-20261006-ucrt-x86_64/bin'
CMAKE = P.parent / 'dependencies/cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe'
NAMES = [
    'cmake/native_game.cmake', 'recomp/native_startup_codegen.cpp',
    'runtime/include/mka/retail_crt.hpp', 'runtime/src/native/retail_crt.cpp',
    'runtime/src/native/retail_crt_startup.cpp',
    'runtime/include/mka/retail_platform.hpp', 'runtime/src/native/retail_platform.cpp',
    'runtime/src/native/retail_platform_startup.cpp', 'runtime/src/native/retail_loading_picture.cpp',
    'runtime/include/mka/retail_main.hpp', 'runtime/src/native/retail_main.cpp',
    'runtime/include/mka/retail_main_timing.hpp', 'runtime/src/native/retail_main_timing.cpp',
    'tests/test_retail_crt.cpp', 'tests/test_retail_loading_picture.cpp',
    'tests/test_retail_main_timing.cpp', 'tests/test_native_retail_startup.cpp',
    'tests/test_native_startup_codegen.cpp', 'tests/test_native_startup_missing_bindings.cpp',
    'runtime/include/mka/content_volume.hpp', 'runtime/src/native/content_volume.cpp',
    'runtime/include/mka/retail_memory_owners.hpp', 'runtime/src/native/retail_memory_owners.cpp',
    'host/keymap.hpp', 'host/keymap.cpp',
    'third_party/sdl2/lib/cmake/SDL2/sdl2-config.cmake',
]


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write(name, value):
    with (OUT / name).open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(value, stream, indent=2)
        stream.write('\n')


if __name__ == '__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    OUT.mkdir(parents=True, exist_ok=False)
    spec = importlib.util.spec_from_file_location('connected_checks',
        F / 'research/runs/native-movie-kernel-policies-20261119/validate.py')
    c = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(c)
    c.OUT = OUT
    c.ENV = dict(os.environ, PATH=str(BIN) + os.pathsep + os.environ['PATH'])
    try:
        cache = (BUILD / 'CMakeCache.txt').read_bytes().decode('utf-8-sig').replace('\r\n', '\n')
        c.require(re.search(r'^PS2_GENERATED:PATH=$', cache, re.M), 'no legacy generated game in this build')
        elf = P / 'research/inputs/SLUS_214.10'
        loading = F / 'games/mkarmageddon/disc-files/LOADING.RAW'
        c.require(digest(elf) == 'a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f', 'original ELF')
        c.require(digest(loading) == '38286bcd8f0475d4bc5eb3390f31b348510404bf8a730774020b8e186fde2c71', 'original loading RGB')
        protected = {
            F / 'runtime/src/iop/mcserv.cpp': 'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',
            F / 'runtime/src/iop/msl_native.cpp': '756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23',
            P / 'research/doc-audit-20261118/inventory.json': '26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6',
        }
        for path, expected in protected.items():
            c.require(digest(path) == expected, 'protected prior data: ' + str(path))
        inputs = [*[F / name for name in NAMES], elf, loading, Path(__file__).resolve()]
        source = {str(path): digest(path) for path in inputs}
        write('provenance.json', {
            'at_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
            'fork_head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=F, text=True).strip(),
            'parent_head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=P, text=True).strip(),
            'sources': source, 'protected': {str(k): v for k, v in protected.items()},
            'compiler': str(BIN / 'clang++.exe'), 'compiler_sha256': digest(BIN / 'clang++.exe'),
            'cache_before_sha256': digest(BUILD / 'CMakeCache.txt'),
            'scope': 'Production objects compile; native CRT/timing/content tests only. No game, endpoints, model or driver operations.',
        })
        targets = ['test_native_retail_crt', 'test_native_retail_main_timing',
            'test_native_retail_loading', 'test_native_retail_startup', 'test_native_startup_codegen']
        c.execute('build.json', [CMAKE, '--build', BUILD, '--target', *targets, '-j2'])
        fixtures = OUT / 'owned-fixtures'
        fixtures.mkdir()
        crt_root = fixtures / 'crt-volume'
        crt_root.mkdir()
        runs = []
        for name, args, completion, required in [
            ('test_native_retail_crt', [crt_root], 'PASS production native CRT ownership/lifecycle',
                ['mka::retail::initialize_native_retail_crt(', 'mka::retail::finalize_native_retail_pre_main(']),
            ('test_native_retail_main_timing', [], 'PASS native main shared-clock stopwatches',
                ['mka::retail::NativeRetailMainTiming::start(']),
            ('test_native_retail_loading', [loading.parent, fixtures / 'loading'], 'PASS native production loading ingress',
                ['mka::retail::load_retail_loading_picture(']),
            ('test_native_retail_startup', [fixtures / 'startup'], 'PASS generated retail startup owns',
                ['mka::retail::initialize_retail_memory_owners(']),
            ('test_native_startup_codegen', [elf], 'PASS native retail startup source identity',
                ['recomp::emit_native_startup_closure(']),
        ]:
            exe = BUILD / (name + '.exe')
            result = c.execute(name + '.json', [exe, *args])
            c.require(result['stdout'].startswith(completion) and not result['stderr'], name + ' assertions')
            audit = c.symbols(name + '-symbols', exe, result['executable_sha256'], required, True)
            runs.append({'name': name, 'binary_sha256': result['executable_sha256'], 'audit': audit})
        archive = BUILD / 'libmka_native_retail_startup.a'
        c.symbols('production-startup-symbols', archive, digest(archive),
            ['mka::retail::initialize_native_retail_crt(', 'mka::retail::finalize_native_retail_pre_main(',
             'mka::retail::initialize_native_retail_platform(', 'mka::retail::continue_native_retail_main(',
             'mka::retail::main_binding::start_retail_stopwatch('], False)
        platform = BUILD / 'libmka_native_retail_platform.a'
        c.symbols('production-platform-symbols', platform, digest(platform),
            ['mka::retail::NativeRetailPlatform::create(', 'mka::retail::NativeRetailPlatform::poll_events_and_input('], False)
        nm = c.execute('production-link-obligations.json', [BIN / 'llvm-nm.exe', '-C', archive])
        declared = set(re.findall(r' U mka::retail::main_binding::(\w+)\(', nm['stdout']))
        defined = set(re.findall(r' [Tt] mka::retail::main_binding::(\w+)\(', nm['stdout']))
        missing = sorted(declared - defined)
        c.require(len(missing) == 67, 'exact remaining real main obligations')
        args = [str(CMAKE), '--build', str(BUILD), '--target', 'test_native_startup_missing_bindings', '-j2']
        process = subprocess.run(args, cwd=F, env=c.ENV, text=True, capture_output=True, creationflags=subprocess.CREATE_NO_WINDOW)
        negative = {'argv': args, 'exit': process.returncode, 'stdout': process.stdout, 'stderr': process.stderr,
            'executable_sha256': digest(CMAKE), 'expected_missing_main_bodies': missing}
        write('full-entry-negative-link.json', negative)
        text = process.stdout + process.stderr
        c.require(process.returncode != 0 and all(('main_binding::' + name + '(') in text for name in missing),
            'full entry rejected for every actual missing body')
        c.require(not (BUILD / 'test_native_startup_missing_bindings.exe').exists(), 'no incomplete executable delivered')
        c.require(source == {str(path): digest(path) for path in inputs}, 'source changed during verification')
        for path, expected in protected.items():
            c.require(digest(path) == expected, 'protected bytes unchanged after verification')
        value = {
            'status': 'PASS_CONNECTED_STARTUP_BUILD_AND_NATIVE_CPU_FILE_CHECKS',
            'sources': source, 'fixtures': runs, 'startup_archive_sha256': digest(archive),
            'platform_archive_sha256': digest(platform), 'missing_main_bodies': missing,
            'complete_entry_linkable': False, 'game_executed': False, 'endpoints_opened': False,
            'scope': 'Five native CPU/file fixtures; actual native platform objects compiled only. Main control source reviewed, not executed.',
            'limits': ['67 actual constructor/profile/task/render/sound/main bindings missing',
                'No retail boot/main menu/combat or physical input/audio/GPU execution',
                'Loading physical pixel aspect and complete render parity open',
                'No accepted complete game executable or all-game regression evidence'],
        }
        write('verification.json', value)
        print(json.dumps({k: value[k] for k in ['status', 'complete_entry_linkable', 'game_executed', 'endpoints_opened']}))
    except Exception as error:
        write('failure.json', {'status': 'FAILED', 'type': type(error).__name__, 'message': str(error)})
        raise
