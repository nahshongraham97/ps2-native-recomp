"""Publish only the measured native save/profile and real Main service batch."""
import importlib.util
import json
import re
import struct
from pathlib import Path

F = Path(__file__).resolve().parents[3]
P = F.parents[2]
H = Path(__file__).resolve().parent
C = P / 'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
s = importlib.util.spec_from_file_location('scoped_publication',
    F / 'research/runs/native-connected-main-dependencies-20261121/publish.py')
m = importlib.util.module_from_spec(s)
s.loader.exec_module(m)

def main():
    vpath = H / 'main-validation-03/verification.json'
    m.pub.need(m.pub.sha(vpath) == 'e9fd260d1d5dcea9a22d68938d26a793f8c04623694b743199ce901a949accae', 'exact measured result')
    v = json.loads(vpath.read_text(encoding='utf-8'))
    m.pub.need(v['status'] == 'PASS_MAIN_NATIVE_SAVE_SERVICE_CONNECTED_CHECKS' and
        len(v['executed_clients']) == 2 and len(v['retained_main_clients']) == 3 and
        len(v['retained_component_clients']) == 4 and len(v['remaining_integration_symbols']) == 28 and
        not v['complete_game_entry_linkable'] and not v['game_executed'] and not v['physical_endpoints_opened'],
        'exact bounded scope, no gameplay/endpoint claim')
    for group in [v['sources'], v['protected_inputs']]:
        for path, expected in group.items(): m.pub.need(m.pub.sha(path) == expected, 'measured input stable: ' + path)
    build = F / 'build_native_services_only_20261119-01'
    m.pub.need(m.pub.sha(build / 'libmka_native_retail_startup.a') == v['startup_archive_sha256'], 'current Main archive')
    for group in ['executed_clients', 'retained_main_clients', 'retained_component_clients']:
        for row in v[group]:
            path = Path(row['receipt'])
            if not path.is_absolute(): path = H / 'main-validation-03' / path
            if 'receipt_sha256' in row: m.pub.need(m.pub.sha(path) == row['receipt_sha256'], 'retained receipt identity')
            r = json.loads(path.read_text(encoding='utf-8'))
            m.pub.need(r['exit'] == 0 and r['stdout'] and not r['stderr'] and
                r['executable_sha256'] == r['executable_after_sha256'] == row['sha256'] ==
                m.pub.sha(build / (row['target'] + '.exe')), 'current passed client: ' + row['target'])
    link = json.loads((H / 'main-validation-03/actual-entry-link.json').read_text(encoding='utf-8'))
    missing = re.findall(r'undefined symbol: ([^\n]+)', link['stdout'] + link['stderr'])
    m.pub.need(link['exit'] != 0 and len(missing) == 28 and
        all(x.startswith('mka::retail::main_binding::') for x in missing), 'actual remaining Main-only link refusal')
    review_path = C / 'native-retail-save-startup-66/review01/RESULT_CORRECTION.json'
    review = json.loads(review_path.read_text(encoding='utf-8'))
    m.pub.need(review['schema'] == 'native_retail_save_startup_review_result_correction_v1' and
        review['status'] == 'CORRECTED_SOURCE_COMPILED_AND_BOUNDED_CLIENT_PASSED' and
        review['main_validation_03']['verification_sha256'] == m.pub.sha(vpath) and
        review['corrected_source_sha256'] == m.pub.sha(F / 'runtime/src/native/retail_save_startup.cpp'),
        'actual current independent source/result review')
    code = ['cmake/native_game.cmake', 'cmake/native_media.cmake', 'recomp/native_startup_codegen.cpp',
        'runtime/include/mka/retail_main.hpp', 'runtime/src/native/retail_main.cpp',
        'runtime/src/native/retail_save_startup.cpp', 'tests/test_retail_save_startup.cpp',
        'tests/test_native_retail_startup.cpp']
    for stem in ['native_save_store', 'retail_saved_profile_defaults', 'retail_profile_image', 'retail_profile_aggregate']:
        code += ['runtime/include/mka/' + stem + '.hpp', 'runtime/src/native/' + stem + '.cpp',
                 'tests/test_' + stem + '.cpp']
    forks = [F / name for name in code] + [H / name for name in ['README.md', 'verify.py', 'check_main.py', 'publish.py']]
    for name in ['validation-01', 'main-validation-01', 'main-validation-02', 'main-validation-03']:
        forks += sorted((H / name).glob('*.json'))
    directories = ['native-retail-scene-startup-56', 'native-retail-current-sound-57',
        'native-retail-save-service-58', 'native-retail-sound-command-consumers-59',
        'native-retail-profile-defaults-60', 'native-retail-profile-aggregate-61',
        'native-retail-profile-layout-62', 'native-retail-profile-codec-63',
        'native-retail-profile-aggregate-64', 'native-retail-profile-child-65', 'native-retail-save-startup-66']
    parents = [P / 'PORT_STATUS.md', P / 'ROADMAP.md']
    for directory in directories:
        parents += sorted(path for path in (C / directory).rglob('*')
            if path.is_file() and '__pycache__' not in path.parts)
    allowed = {'.md', '.json', '.cpp', '.hpp', '.py', '.cmake'}
    immutable = {}
    for path in forks + parents:
        m.pub.need(path.suffix in allowed, 'only reviewed text/source: ' + str(path))
        data = path.read_bytes()
        data.decode('utf-8')
        # Original source/review receipts and before bytes keep exact identities.
        frozen = path.is_relative_to(C) and path.name not in ['README.md', 'PLAN.md', 'RESULTS.md']
        if not frozen:
            m.pub.need(b'\r' not in data and data.endswith(b'\n'), 'normalized active text: ' + str(path))
        immutable[str(path)] = m.pub.sha(path)
    blob = (P / 'research/inputs/SLUS_214.10').read_bytes()
    phoff = struct.unpack_from('<I', blob, 28)[0]
    phsize, phnum = struct.unpack_from('<HH', blob, 42)
    segments = [struct.unpack_from('<8I', blob, phoff + i * phsize) for i in range(phnum)]
    joined = 0
    def join(value):
        nonlocal joined
        if isinstance(value, dict):
            if 'address' in value and ('le32' in value or 'word' in value):
                address = int(value['address'], 16)
                word = value.get('le32', value.get('word'))
                word = int(word, 16) if isinstance(word, str) else word
                matching = [s for s in segments if s[0] == 1 and s[2] <= address and address + 4 <= s[2] + s[4]]
                m.pub.need(len(matching) == 1, 'actual source65 mapped word')
                seg = matching[0]
                m.pub.need(struct.unpack_from('<I', blob, seg[1] + address - seg[2])[0] == word,
                           'actual original source65 bytes')
                joined += 1
            for item in value.values(): join(item)
        elif isinstance(value, list):
            for item in value: join(item)
    raw65 = C / 'native-retail-profile-child-65/source-agent02/RAW_SOURCE.json'
    m.pub.need(m.pub.sha(raw65) == '7369f441aeca1ed708dd58e2e6cb2d5d7142ce398ac0d46c2c219a44bbb009fc', 'current source65 raw identity')
    join(json.loads(raw65.read_text(encoding='utf-8')))
    m.pub.need(joined > 0 and 4 * 0xa28 + 0x154 == 0x29f4, 'corrected repeated field versus full-record distinction')
    bases = {F: '0fc0c428120e4069409ff931960196f4f35c0fd2', P: '1581cfc53f4685f925c7b5498861c4d64e0616e8'}
    remote = {}
    for root, base in bases.items():
        m.pub.need(m.pub.git(root, 'rev-parse', 'HEAD') == base and
                   m.pub.git(root, 'branch', '--show-current') == 'main', 'expected source base')
        remote[root] = m.pub.remote_probe(root, base)
    m.pub.need(all(m.pub.sha(path) == pin for path, pin in immutable.items()), 'authored bytes stable before staging')
    fh, fs = m.commit_sync(F, forks, 'Connect native save storage to retail Main and verify retail profile components', remote[F])
    marker = 'Working-tree native save/profile checkpoint (base fork `0fc0c428120e4069409ff931960196f4f35c0fd2`)'
    for name in ['PORT_STATUS.md', 'ROADMAP.md']:
        path = P / name
        text = path.read_text(encoding='utf-8')
        m.pub.need(text.count(marker) == 1, 'one live checkpoint marker')
        with path.open('w', encoding='utf-8', newline='\n') as stream:
            stream.write(text.replace(marker, 'Published native save/profile checkpoint fork `' + fh + '`', 1))
    ph, ps = m.commit_sync(P, parents, 'Record native save Main integration, source corrections and exact remaining acceptance gaps', remote[P])
    for path, expected in v['protected_inputs'].items(): m.pub.need(m.pub.sha(path) == expected, 'protected inputs after publication')
    result = dict(fork_commit=fh, fork=fs, parent_commit=ph, parent=ps,
        verification_sha256=m.pub.sha(vpath), source65_original_words_joined=joined,
        scope='Four native profile components and five Main/codegen clients; actual entry still refuses28 Main effects; no game/endpoint acceptance')
    with (H / 'PUBLICATION.json').open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(result, stream, indent=2)
        stream.write('\n')
    print(json.dumps(result))

if __name__ == '__main__': main()
