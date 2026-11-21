"""Compile the connected viewport binding and run affected CPU/file clients."""
import csv
import importlib.util
import json
import re
import struct
from pathlib import Path

F = Path(__file__).resolve().parents[3]
P = F.parents[2]
H = Path(__file__).resolve().parent
C = P / 'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
spec = importlib.util.spec_from_file_location('checks', F / 'research/runs/native-typed-retail-20261121/verify.py')
c = importlib.util.module_from_spec(spec)
spec.loader.exec_module(c)
c.OUT = H / 'validation-01'

if __name__ == '__main__':
    c.ctypes.windll.kernel32.SetErrorMode(3)
    c.OUT.mkdir(exist_ok=False)
    try:
        previous = F / 'research/runs/native-retail-native-file-stage-20261121/validation-01/verification.json'
        prior = json.loads(previous.read_text(encoding='utf-8'))
        c.require(prior['status'] == 'PASS_NATIVE_FILE_TRANSPORT_REMOVAL' and
                  isinstance(prior['remaining_integration_symbols'], list), 'actual prior file-stage result')
        connected = json.loads((F / 'research/runs/native-retail-connected-initialization-20261121/validation-03/verification.json').read_text(encoding='utf-8'))
        package = json.loads((F / 'research/runs/native-connected-main-dependencies-20261121/validation-03/test_native_retail_package.json').read_text(encoding='utf-8'))
        c.require(package['exit'] == 0 and len(package['argv']) == 3, 'original package comparison arguments')
        paths = ['runtime/include/mka/retail_viewport.hpp', 'runtime/include/mka/retail_main.hpp',
                 'runtime/include/mka/retail_main_timing.hpp', 'runtime/include/mka/retail_platform.hpp',
                 'runtime/src/native/retail_main.cpp', 'runtime/src/native/retail_main_timing.cpp',
                 'runtime/src/native/retail_platform.cpp', 'runtime/src/native/retail_crt.cpp',
                 'runtime/src/native/retail_game_records_main.cpp', 'runtime/src/native/retail_initial_task.cpp',
                 'runtime/src/native/retail_file_startup.cpp', 'runtime/src/native/retail_package.cpp',
                 'tests/test_retail_main_timing.cpp', 'tests/test_retail_game_records_main.cpp',
                 'tests/test_retail_initial_task.cpp', 'tests/test_retail_file_startup.cpp',
                 'tests/test_retail_package.cpp', 'cmake/native_game.cmake', 'cmake/native_typed.cmake']
        sources = {str(F / name): c.sha(F / name) for name in paths}
        sources[str(Path(__file__).resolve())] = c.sha(__file__)
        source_dir = C / 'native-retail-timer-viewport-43/source-agent01'
        manifest = json.loads((source_dir / 'SOURCE.json').read_text(encoding='utf-8'))
        c.require(c.sha(source_dir / 'SOURCE.json') == '40880fa5658683e49160aaa7e052fd725197f50995c493781bb71ae6e79643dd',
                  'exact original viewport/timer source input')
        evidence = {str(previous): c.sha(previous), str(source_dir / 'SOURCE.json'): c.sha(source_dir / 'SOURCE.json'),
                    str(C / 'native-retail-viewport-clock-47/SOURCE_JOIN.json'): c.sha(C / 'native-retail-viewport-clock-47/SOURCE_JOIN.json'),
                    package['argv'][2]: c.sha(package['argv'][2])}
        elf = P / 'research/inputs/SLUS_214.10'
        blob = elf.read_bytes()
        c.require(blob[:6] == b'\x7fELF\x01\x01' and c.sha(elf) ==
                  'a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f', 'original LE ELF32')
        phoff = struct.unpack_from('<I', blob, 28)[0]
        phsize, phcount = struct.unpack_from('<HH', blob, 42)
        segments = [struct.unpack_from('<8I', blob, phoff + i * phsize) for i in range(phcount)]
        joined = {}
        for name, pin in manifest['raw_words_sha256'].items():
            path = source_dir / name
            c.require(c.sha(path) == pin, 'original source words: ' + name)
            evidence[str(path)] = pin
            rows = list(csv.DictReader(path.read_text(encoding='utf-8').splitlines()))
            for row in rows:
                va = int(row['va'], 16)
                loads = [s for s in segments if s[0] == 1 and s[2] <= va and va + 4 <= s[2] + s[4]]
                c.require(len(loads) == 1, 'unambiguous file-backed PT_LOAD instruction')
                offset = loads[0][1] + va - loads[0][2]
                c.require(offset == int(row['file_offset'], 16) and
                          struct.unpack_from('<I', blob, offset)[0] == int(row['le32'], 16), 'actual ELF instruction mapping')
            joined[name] = len(rows)
        c.require(joined == {'225DB0.csv': 38, '220EB0.csv': 14, '220EF0.csv': 360}, 'exact original instruction ranges')
        def stable():
            for group in (sources, evidence, connected['protected_inputs']):
                c.require(all(c.sha(path) == pin for path, pin in group.items()), 'stable source/evidence/protected inputs')
        stable()
        c.save('source-preflight.json', dict(sources=sources, evidence=evidence, original_instruction_words=joined))
        targets = ['test_native_retail_main_timing', 'test_native_retail_game_records_main',
                   'test_native_retail_initial_task', 'test_native_retail_file_startup', 'test_native_retail_package']
        c.execute('build', [c.CMAKE, '--build', c.BUILD, '--target', *targets, '-j2'])
        empty = c.OUT / 'disposable-task-content'
        files = c.OUT / 'disposable-file-parent'
        empty.mkdir()
        files.mkdir()
        arguments = [[], [P / 'research/inputs/modules'], [empty], [files], package['argv'][1:]]
        clients = []
        for target, args in zip(targets, arguments):
            result = c.execute(target, [c.BUILD / (target + '.exe'), *args])
            c.require('PASS' in result['stdout'] and not result['stderr'], 'affected native client: ' + target)
            clients.append(dict(target=target, sha256=result['executable_sha256']))
        link = c.execute('actual-entry-link', [c.CMAKE, '--build', c.BUILD, '--target',
                         'test_native_startup_missing_bindings', '-j2'], expected=None)
        missing = sorted(set(re.findall(r'undefined symbol: mka::retail::main_binding::(\w+)\(', link['stdout'] + link['stderr'])))
        c.require(missing == sorted(set(prior['remaining_integration_symbols']) - {'initialize_retail_timer_and_viewport'}),
                  'real entry drops exactly the implemented timer/viewport dependency')
        c.require(not (c.BUILD / 'test_native_startup_missing_bindings.exe').exists(), 'incomplete entry remains unlinked')
        stable()
        c.save('verification.json', dict(status='PASS_NATIVE_VIEWPORT_CLOCK_CONNECTED_CHECKS',
               sources=sources, evidence=evidence, original_instruction_words=joined, protected_inputs=connected['protected_inputs'],
               executed_clients=clients, remaining_integration_symbols=missing,
               startup_archive_sha256=c.sha(c.BUILD / 'libmka_native_retail_startup.a'),
               complete_game_entry_linkable=False, game_executed=False, physical_endpoints_opened=False,
               frames=None, audio=None,
               limitations=['Actual logical viewport binding is compiled; physical presentation branch was not executed.',
                            'Clock behavior uses the real retained native steady clock; no interrupt/timer device is recreated.',
                            'Later display positioning and actual renderer consumption remain open.',
                            'Full startup, physical input, main menu, combat, audio and whole-game acceptance remain open.']))
        print(json.dumps(dict(status='PASS_NATIVE_VIEWPORT_CLOCK_CONNECTED_CHECKS',
                              fresh_clients=len(clients), remaining_Main_integrations=len(missing))))
    except Exception as error:
        c.save('failure.json', dict(status='FAILED', error=type(error).__name__, message=str(error)))
        raise
