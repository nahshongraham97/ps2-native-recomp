"""Publish only the source-verified native file/viewport/clock batch."""
import importlib.util
import json
import re
from pathlib import Path

F = Path(__file__).resolve().parents[3]
P = F.parents[2]
H = Path(__file__).resolve().parent
C = P / 'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
s = importlib.util.spec_from_file_location('publisher', F / 'research/runs/native-connected-main-dependencies-20261121/publish.py')
m = importlib.util.module_from_spec(s)
s.loader.exec_module(m)

if __name__ == '__main__':
    receipt = H / 'validation-01/verification.json'
    m.pub.need(m.pub.sha(receipt) == '8d9b66403a452f952d4c5578c9fee7988566d9f3eb09d06f4587837f8f45e435', 'exact completed viewport/clock result')
    v = json.loads(receipt.read_text(encoding='utf-8'))
    targets = ['test_native_retail_main_timing', 'test_native_retail_game_records_main',
               'test_native_retail_initial_task', 'test_native_retail_file_startup', 'test_native_retail_package']
    m.pub.need(v['status'] == 'PASS_NATIVE_VIEWPORT_CLOCK_CONNECTED_CHECKS' and
               [r['target'] for r in v['executed_clients']] == targets and
               len(v['remaining_integration_symbols']) == 31 and not v['complete_game_entry_linkable'] and
               not v['game_executed'] and not v['physical_endpoints_opened'], 'actual evidence scope')
    for group in (v['sources'], v['evidence'], v['protected_inputs']):
        for name, pin in group.items():
            m.pub.need(m.pub.sha(name) == pin, 'current measured bytes: ' + name)
    build = F / 'build_native_services_only_20261119-01'
    m.pub.need(m.pub.sha(build / 'libmka_native_retail_startup.a') == v['startup_archive_sha256'], 'current startup archive')
    for row in v['executed_clients']:
        r = json.loads((H / 'validation-01' / (row['target'] + '.json')).read_text(encoding='utf-8'))
        m.pub.need(r['exit'] == 0 and 'PASS' in r['stdout'] and not r['stderr'] and
                   r['executable_sha256'] == r['executable_after_sha256'] == row['sha256'] ==
                   m.pub.sha(build / (row['target'] + '.exe')), 'actual successful execution: ' + row['target'])
    r = json.loads((H / 'validation-01/actual-entry-link.json').read_text(encoding='utf-8'))
    symbols = re.findall(r'undefined symbol: ([^\n]+)', r['stdout'] + r['stderr'])
    m.pub.need(r['exit'] != 0 and len(symbols) == 31 and
               all(n.startswith('mka::retail::main_binding::') for n in symbols), 'actual remaining entry dependencies only')
    code = ['runtime/include/mka/retail_viewport.hpp', 'runtime/include/mka/retail_main.hpp',
            'runtime/include/mka/retail_main_timing.hpp', 'runtime/include/mka/retail_platform.hpp',
            'runtime/src/native/retail_main.cpp', 'runtime/src/native/retail_main_timing.cpp',
            'runtime/src/native/retail_platform.cpp', 'tests/test_retail_main_timing.cpp']
    forks = [F / n for n in code]
    forks += [H / 'check.py', H / 'publish.py',
              F / 'research/runs/native-retail-native-file-stage-20261121/check.py']
    forks += sorted((H / 'validation-01').glob('*.json'))
    forks += sorted((F / 'research/runs/native-retail-native-file-stage-20261121/validation-01').glob('*.json'))
    parents = [P / n for n in ['PORT_STATUS.md', 'ROADMAP.md']]
    for folder in ['native-retail-native-file-stage-46', 'native-retail-viewport-clock-47']:
        parents += sorted(path for path in (C / folder).iterdir() if path.is_file())
    for path in forks + parents:
        data = path.read_bytes()
        data.decode('utf-8')
        m.pub.need(b'\r' not in data and data.endswith(b'\n'), 'normalized authored text: ' + str(path))
    bases = {F: '459569b864a7bacb1c4ab0e4517572ae8a67e250', P: '1b7e998eba584e4fe37e09a2dae2d85f371cf844'}
    remotes = {}
    for root, base in bases.items():
        m.pub.need(m.pub.git(root, 'rev-parse', 'HEAD') == base and
                   m.pub.git(root, 'branch', '--show-current') == 'main', 'current authored base')
        remotes[root] = m.pub.remote_probe(root, base)
    outputs = []
    for root, paths, title in [(F, forks, 'Bind native retail viewport and shared clock; remove private file-slot transport'),
                               (P, parents, 'Record connected native file and viewport clock implementation evidence')]:
        head, sync = m.commit_sync(root, paths, title, remotes[root])
        outputs.append(dict(repository=str(root), commit=head, synchronization=sync))
    with (H / 'PUBLICATION.json').open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(outputs, stream, indent=2)
        stream.write('\n')
    print(json.dumps(outputs))
