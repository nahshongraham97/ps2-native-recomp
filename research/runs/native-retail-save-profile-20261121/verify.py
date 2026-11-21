"""Compile/run only the four native save/profile dependency clients."""
import ctypes
import hashlib
import importlib.util
import json
import struct
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
F = HERE.parents[2]
P = F.parents[2]
C = P / 'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
spec = importlib.util.spec_from_file_location('native_client_receipts',
    F / 'research/runs/native-typed-retail-20261121/verify.py')
c = importlib.util.module_from_spec(spec)
spec.loader.exec_module(c)
c.OUT = HERE / (sys.argv[1] if len(sys.argv) > 1 else 'validation-01')

def load(path):
    return json.loads(path.read_text(encoding='utf-8'))

def word_value(value):
    return int(value, 16) if isinstance(value, str) else value

def main():
    ctypes.windll.kernel32.SetErrorMode(3)
    c.OUT.mkdir(exist_ok=False)
    try:
        sources = {}
        authors = [
            ('native-retail-save-service-58', 'native_retail_save_service_58_implementation_v1', 'sha256'),
            ('native-retail-profile-defaults-60', 'native_retail_profile_defaults_60_author_v1', 'source_pins'),
            ('native-retail-profile-codec-63', 'native_retail_profile_codec_63_author_v1', 'source_pins'),
            ('native-retail-profile-aggregate-64', 'native_retail_profile_aggregate_64_implementation_v1', 'sha256')]
        preflight = []
        for directory, schema, field in authors:
            path = C / directory / 'author01/IMPLEMENTATION.json'
            j = load(path)
            c.require(j['schema'] == schema and isinstance(j[field], dict), 'author schema: ' + directory)
            preflight.append(dict(path=str(path), schema=j['schema'], pin_field=field,
                                  pin_count=len(j[field]), sha256=c.sha(path)))
            sources[str(path)] = c.sha(path)
            corrections = {}
            if directory == 'native-retail-save-service-58':
                corrected_path = C / directory / 'root01/CORRECTION.json'
                corrected = load(corrected_path)
                c.require(corrected['schema'] == 'native_save_store_file_effect_correction_v1' and
                    corrected['original_author_sha256'] == c.sha(path), 'explicit reviewed correction identity')
                sources[str(corrected_path)] = c.sha(corrected_path)
                corrections = corrected['sources']
                c.require(set(corrections) == {str(F / name) for name in
                    ['runtime/src/native/native_save_store.cpp', 'tests/test_native_save_store.cpp']},
                    'exact corrected source scope')
                for original, original_sha in j[field].items():
                    if original in corrections:
                        snapshot = C / directory / 'root01/before-safe-file-effects' / Path(original).name
                        c.require(c.sha(snapshot) == original_sha, 'preserved original author bytes')
                        sources[str(snapshot)] = original_sha
                c.require(all(c.sha(p) == h for p, h in corrected['before'].items()), 'before correction pins')
            for name, expected in j[field].items():
                expected = corrections.get(name, expected)
                c.require(c.sha(name) == expected, 'author pin changed: ' + name)
                sources[name] = expected
        prior_path = F / 'research/runs/native-retail-pad-arguments-20261121/validation-02/verification.json'
        c.require(c.sha(prior_path) == 'edfc948d382b56e95c0a6dbe9382423e5354f63ffc40b59debc080fec533397c', 'prior evidence identity')
        prior = load(prior_path)
        c.require(prior['status'] == 'PASS_NATIVE_PAD_ARGUMENT_RESET_CONNECTED_CHECKS' and
                  isinstance(prior['current_clients'], list) and
                  isinstance(prior['remaining_integration_symbols'], list), 'prior schema')
        protected = dict(prior['protected_inputs'])
        for name in ['CMakeLists.txt', 'cmake/native_game.cmake', 'cmake/native_services.cmake',
                     'research/runs/native-typed-retail-20261121/verify.py']:
            path = F / name
            if path.exists(): sources[str(path)] = c.sha(path)
        sources[str(Path(__file__).resolve())] = c.sha(__file__)
        elf = P / 'research/inputs/SLUS_214.10'
        c.require(c.sha(elf) == 'a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f', 'retail ELF identity')
        protected[str(elf)] = c.sha(elf)
        c.require(all(c.sha(p) == h for p, h in protected.items()), 'protected inputs before build')
        raw_paths = [
            C / 'native-retail-scene-startup-56/source-agent01/RAW_SOURCE.json',
            C / 'native-retail-profile-defaults-60/author01/RAW_SOURCE.json',
            C / 'native-retail-profile-codec-63/author01/ORIGINAL_WORDS.json',
            C / 'native-retail-profile-aggregate-61/source-agent01/RAW_SOURCE.json']
        image = elf.read_bytes()
        phoff = struct.unpack_from('<I', image, 28)[0]
        phentsize, phnum = struct.unpack_from('<HH', image, 42)
        segments = []
        for i in range(phnum):
            typ, offset, vaddr, _, filesz, memsz, _, _ = struct.unpack_from('<8I', image, phoff + i * phentsize)
            if typ == 1: segments.append((offset, vaddr, filesz, memsz))
        def read(vma, size):
            for offset, base, filesz, _ in segments:
                if base <= vma and vma + size <= base + filesz:
                    return image[offset + vma - base:offset + vma - base + size]
            raise RuntimeError('unmapped file bytes: ' + hex(vma))
        def joins(value):
            count = 0
            if isinstance(value, dict):
                if 'address' in value and ('le32' in value or 'word' in value):
                    at = word_value(value['address'])
                    expected = word_value(value.get('le32', value.get('word')))
                    c.require(struct.unpack('<I', read(at, 4))[0] == expected, 'original word mismatch: ' + hex(at))
                    count += 1
                if 'address' in value and 'hex' in value:
                    expected = bytes.fromhex(value['hex'])
                    c.require(read(word_value(value['address']), len(expected)) == expected, 'original data mismatch')
                if 'pointer' in value and 'bytes_hex' in value:
                    expected = bytes.fromhex(value['bytes_hex']) + b'\0'
                    c.require(read(word_value(value['pointer']), len(expected)) == expected, 'original string mismatch')
                count += sum(joins(v) for v in value.values())
            elif isinstance(value, list): count += sum(joins(v) for v in value)
            return count
        original_joins = {}
        for path in raw_paths:
            sources[str(path)] = c.sha(path)
            raw = load(path)
            original_joins[str(path)] = joins(raw)
        c.require(read(0x600830, 2) == b' \0' and read(0x647020, 9) == b'BUG THIS\0' and
                  struct.unpack('<I', read(0x5f2550, 4))[0] == 1, 'original profile constants')
        archive = c.BUILD / 'libmka_native_retail_startup.a'
        c.require(c.sha(archive) == prior['startup_archive_sha256'], 'unchanged connected Main archive before')
        c.save('inputs.json', dict(schema='native_save_profile_inputs_v1', sources=sources,
            protected_inputs=protected, authors=preflight, original_word_joins=original_joins,
            prior_path=str(prior_path), prior_sha256=c.sha(prior_path)))
        expected_output = {
            'test_native_save_store': 'native_save_store: passed\n',
            'test_native_retail_saved_profile_defaults': 'PASS native retail saved profile default leaves; aggregate/menu and unknown profile consumers open\n',
            'test_native_retail_profile_image': 'PASS native retail profile card image codec; typed profile fields and saved-game acceptance open\n',
            'test_native_retail_profile_aggregate': 'retail_profile_aggregate: passed\n'}
        c.execute('build', [c.CMAKE, '--build', c.BUILD, '--target', *expected_output, '-j2'])
        clients = []
        for target, expected in expected_output.items():
            receipt = c.execute('run-' + target, [c.BUILD / (target + '.exe')])
            c.require(receipt['stdout'] == expected and not receipt['stderr'], 'exact client assertions: ' + target)
            clients.append(dict(target=target, sha256=receipt['executable_sha256'],
                                receipt='run-' + target + '.json'))
        c.require(c.sha(archive) == prior['startup_archive_sha256'], 'component builds did not alter Main archive')
        for row in prior['current_clients']:
            c.require(c.sha(c.BUILD / (row['target'] + '.exe')) == row['sha256'], 'prior client unchanged: ' + row['target'])
        c.require(all(c.sha(p) == h for p, h in sources.items()), 'inputs stable through checks')
        c.require(all(c.sha(p) == h for p, h in protected.items()), 'protected inputs after checks')
        result = dict(schema='native_save_profile_dependency_verification_v1',
            status='PASS_NATIVE_SAVE_PROFILE_DEPENDENCY_COMPONENTS', sources=sources,
            protected_inputs=protected, executed_clients=clients, original_word_joins=original_joins,
            retained_main_record=str(prior_path), retained_main_record_sha256=c.sha(prior_path),
            startup_archive_sha256=c.sha(archive), retained_unchanged_clients=prior['current_clients'],
            remaining_main_integrations_from_unchanged_archive=prior['remaining_integration_symbols'],
            game_executed=False, physical_endpoints_opened=False, frames=None, audio=None,
            limits=['Native file capability and bounded profile components only; no Main binding added.',
                    'Full typed gameplay profile, aggregate menu effects and live override producer remain open.',
                    'Prior connected link refusal and five client runs retained unchanged, not rerun.',
                    'No boot, main menu, physical input, render, sound or full-game acceptance.'])
        c.save('verification.json', result)
        print(json.dumps(dict(status=result['status'], clients=len(clients), game_executed=False)))
    except Exception as error:
        c.save('failure.json', dict(status='FAILED', type=type(error).__name__, message=str(error)))
        raise

if __name__ == '__main__': main()
