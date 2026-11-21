"""Check the real reset caller with bounded CPU substitutes, never a game run."""
import ctypes
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys

HERE = Path(__file__).resolve().parent
FORK = HERE.parents[2]
PARENT = FORK.parents[2]
BUILD = FORK / 'build_native_services_only_20261119-01'
DEPENDENCIES = PARENT.parent / 'dependencies'
BIN = DEPENDENCIES / 'compiler/llvm-mingw-20261006-ucrt-x86_64/bin'
CMAKE = DEPENDENCIES / 'cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe'
OUT = HERE / (sys.argv[1] if len(sys.argv) > 1 else 'validation-01')
ENV = dict(os.environ, PATH=str(BIN) + os.pathsep + os.environ['PATH'])


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def save(name, data):
    with (OUT / name).open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(data, stream, indent=2)
        stream.write('\n')


def require(value, why):
    if not value:
        raise RuntimeError(why)


def execute(name, argv, expected=0, timeout=180):
    argv = list(map(str, argv))
    tool_hash = sha(argv[0])
    proc = subprocess.run(argv, cwd=FORK, env=ENV, capture_output=True,
                          text=True, timeout=timeout, creationflags=subprocess.CREATE_NO_WINDOW)
    receipt = dict(argv=argv, cwd=str(FORK), exit=proc.returncode,
                   stdout=proc.stdout, stderr=proc.stderr, executable_sha256=tool_hash,
                   executable_after_sha256=sha(argv[0]))
    save(name + '.json', receipt)
    require(receipt['executable_after_sha256'] == tool_hash, 'tool changed during ' + name)
    require(proc.returncode == expected if expected is not None else proc.returncode != 0,
            'unexpected exit in ' + name)
    return receipt


def main():
    ctypes.windll.kernel32.SetErrorMode(3)
    OUT.mkdir(exist_ok=False)
    try:
        initial = json.loads((HERE / 'initial.json').read_text())
        protected = dict(initial['protected_inputs'])
        # This file had preexisting additions. Their exact baseline is retained
        # before our two source/test integration hunks, never discarded.
        protected.pop(str(FORK / 'cmake/native_game.cmake'))
        backup = FORK.parent / 'ps2-native-recomp-backup-20261110/build/native-mkarmageddon/ps2run.exe'
        protected[str(backup)] = 'b9eef7dad3cfa0e1216493cf12301f14aa4b3a8b1456fcdc60d9c39502a0d860'
        elf_path = PARENT / 'research/inputs/SLUS_214.10'
        protected[str(elf_path)] = 'a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f'
        require(all(sha(p) == h for p, h in protected.items()), 'protected inputs before checks')
        source_names = [
            'cmake/native_game.cmake', 'runtime/include/mka/retail_participants.hpp',
            'runtime/src/native/retail_participants.cpp',
            'runtime/include/mka/retail_initial_task_state.hpp',
            'runtime/src/native/retail_initial_task_state.cpp',
            'runtime/include/mka/retail_initial_task_required.hpp',
            'runtime/include/mka/retail_initial_reset_required.hpp',
            'runtime/src/native/retail_initial_reset.cpp',
            'tests/test_retail_initial_reset.cpp', 'tests/test_retail_initial_task.cpp']
        sources = {str(FORK / p): sha(FORK / p) for p in source_names}
        sources[str(Path(__file__))] = sha(__file__)
        for path in sources:
            require(b'\r\r\n' not in Path(path).read_bytes(), 'doubled carriage return: ' + path)
        evidence = PARENT / 'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
        state = evidence / 'native-retail-reset-state-85/source-agent01'
        raw = json.loads((state / 'RAW_SOURCE.json').read_text())
        listing = json.loads((state / 'LISTINGS.json').read_text())
        elf = elf_path.read_bytes()
        offset = struct.unpack_from('<I', elf, 28)[0]
        stride, count = struct.unpack_from('<HH', elf, 42)
        segments = [struct.unpack_from('<8I', elf, offset + i * stride) for i in range(count)]

        def image(at, length):
            matches = [s for s in segments if s[0] == 1 and s[2] <= at and at + length <= s[2] + s[4]]
            require(len(matches) == 1, 'unique original file-backed mapping')
            s = matches[0]
            start = s[1] + at - s[2]
            return elf[start:start + length]

        reset = raw['ranges']['reset_18d970']
        require(hashlib.sha256(image(0x18d970, 240)).hexdigest() == reset['sha256'], 'original complete reset span')
        words = reset['words_le32']
        require(len(words) == 60, 'complete60-word reset')
        for row in words:
            require(struct.unpack('<I', image(int(row['address'], 16), 4))[0] == int(row['word'], 16), 'original reset word')
        listed = listing['functions']['0018d970']['items']
        require(len(listed) == 60 and all(image(int(r['address'], 16), 4).hex() == r['bytes'] for r in listed),
                'saved actual R5900 listing covers and matches all60 original words')
        wait = json.loads((evidence / 'native-retail-initial-reset-wait-82/source-agent01/RAW_SOURCE.json').read_text())
        joined_wait_words = 0
        for row in wait['ranges'].values():
            body = image(int(row['address'], 16), row['size'])
            require(hashlib.sha256(body).hexdigest() == row['sha256'], 'original wait/producer range')
            joined_wait_words += len(row['words_le32'])
        setter = image(0x165780, 16)
        # LUI v1,0x66; JR RA; delay-slot SW a0,0x98(v1).
        setter_words = list(struct.unpack('<4I', setter))
        require(setter_words == [0x3c030066, 0x03e00008, 0xac640098, 0], 'actual165780 full-word writer')
        save('source-join.json', dict(original_elf_sha256=sha(elf_path), reset_words=60,
             reset_listing_words=60, wait_range_words=joined_wait_words,
             setter_words_le32=[hex(x) for x in setter_words],
             input_evidence={str(p): sha(p) for p in [state / 'RAW_SOURCE.json', state / 'LISTINGS.json',
                 evidence / 'native-retail-initial-reset-wait-82/source-agent01/RAW_SOURCE.json',
                 evidence / 'native-retail-initial-reset-wait-82/source-agent01/SOURCE.md',
                 evidence / 'native-retail-initial-reset-80/source-agent01/SOURCE.md']}))
        targets = ['test_native_retail_initial_reset', 'test_native_retail_initial_task',
                   'test_native_retail_participants', 'test_native_retail_game_session']
        execute('build', [CMAKE, '--build', BUILD, '--target', *targets, '-j2'])
        content = OUT / 'content'
        content.mkdir()
        identities = []
        for target in targets:
            args = [content] if target in targets[:2] else []
            receipt = execute('run-' + target, [BUILD / (target + '.exe'), *args], timeout=30)
            require(not receipt['stderr'] and receipt['stdout'].startswith('PASS'), 'component assertions: ' + target)
            symbol = execute('symbols-' + target, [BIN / 'llvm-nm.exe', '-C', BUILD / (target + '.exe')])
            require(not re.search(r'(?<![A-Za-z0-9_])(?:ps2|gs|spu2|sif|Cpu|recomp)::|\b(?:g_ram|g_spr)\b', symbol['stdout']),
                    'no guest/device/compiler symbols in ' + target)
            identities.append(dict(target=target, sha256=receipt['executable_sha256']))
        prior = json.loads((FORK / 'research/runs/native-retail-reset-section-20261121/validation-01/verification.json').read_text())
        required_children = [n for n in prior['required_initial_task_children'] if n != 'begin_reset_18d970']
        required_reset = ['reset_saved_profile_children_246070', 'reset_selected_resource_334700',
            'request_mode_one_banks_12a4f0', 'banks_pending_12abd0',
            'clear_nonowning_references_18da30', 'configure_selected_resource_zero_16fc70']
        plans = [
            ('initial-children', 'test_native_retail_initial_task_missing_children', required_children + required_reset),
            ('entry', 'test_native_startup_missing_bindings', prior['remaining_integration_symbols'] + required_children + required_reset)]
        obligations = {}
        for label, target, expected in plans:
            receipt = execute('link-' + label, [CMAKE, '--build', BUILD, '--target', target, '-j2'], expected=None)
            missing = re.findall(r'undefined symbol: ([^\n]+)', receipt['stdout'] + receipt['stderr'])
            require(len(missing) == len(expected) and all(any('::' + n + '(' in x for x in missing) for n in expected),
                    'actual exact unresolved production effects: ' + target)
            require(not (BUILD / (target + '.exe')).exists(), 'incomplete native executable must remain absent')
            obligations[target] = missing
        for pins in [sources, protected]:
            require(all(sha(p) == h for p, h in pins.items()), 'stable inputs after checks')
        execute('diff-check', ['C:/Program Files/Git/cmd/git.exe', 'diff', '--check', '--', *source_names])
        save('verification.json', dict(schema='native_retail_reset_connected_v1',
             status='PASS_BOUNDED_NATIVE_RESET_CALLER_COMPONENT_CHECKS',
             fork_build_commit=initial['fork_commit'], dirty_source=True, sources=sources,
             initial_provenance_sha256=sha(HERE / 'initial.json'), protected_inputs=protected,
             build_runtime_flags=[], component_identities=identities, remaining_link_symbols=obligations,
             game_executed=False, frames=None, audio=None, native_only_game_acceptance='OPEN',
             limits=['Six reset resource children substituted only in CPU fixture; absent from production.',
                     'Full boot, physical input, renderer, sound, saves, combat and modes remain unverified.',
                     'Native transfer test checks caller state only; unfinished child owns its continuation.']))
        print('PASS four native component clients; incomplete full entry still refuses41 native effect names; gameplay OPEN')
    except Exception as error:
        save('failure.json', dict(type=type(error).__name__, message=str(error)))
        raise


if __name__ == '__main__':
    main()
