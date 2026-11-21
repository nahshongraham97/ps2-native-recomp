"""Finish the exact parent publication after Git's long-path staging refusal."""
import importlib.util
import json
import os
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
    # Process-scoped Git configuration only. No repository/global setting is changed.
    n = int(os.environ.get('GIT_CONFIG_COUNT', '0'))
    os.environ['GIT_CONFIG_COUNT'] = str(n + 1)
    os.environ['GIT_CONFIG_KEY_' + str(n)] = 'core.longpaths'
    os.environ['GIT_CONFIG_VALUE_' + str(n)] = 'true'
    code = '642ae1a7c4c08cb2c4c8e5ce50e2af06b9ef38f7'
    base = '1581cfc53f4685f925c7b5498861c4d64e0616e8'
    m.pub.need(m.pub.git(F, 'rev-parse', 'HEAD') == code and
        m.pub.git(P, 'rev-parse', 'HEAD') == base, 'exact partial publication state')
    vpath = H / 'main-validation-03/verification.json'
    m.pub.need(m.pub.sha(vpath) == 'e9fd260d1d5dcea9a22d68938d26a793f8c04623694b743199ce901a949accae', 'exact verified result')
    v = json.loads(vpath.read_text(encoding='utf-8'))
    for group in [v['sources'], v['protected_inputs']]:
        for name, pin in group.items(): m.pub.need(m.pub.sha(name) == pin, 'measured source preserved')
    paths = [P / 'PORT_STATUS.md', P / 'ROADMAP.md']
    for name in ['native-retail-scene-startup-56', 'native-retail-current-sound-57',
        'native-retail-save-service-58', 'native-retail-sound-command-consumers-59',
        'native-retail-profile-defaults-60', 'native-retail-profile-aggregate-61',
        'native-retail-profile-layout-62', 'native-retail-profile-codec-63',
        'native-retail-profile-aggregate-64', 'native-retail-profile-child-65', 'native-retail-save-startup-66']:
        paths += sorted(p for p in (C / name).rglob('*') if p.is_file() and '__pycache__' not in p.parts)
    for path in paths:
        m.pub.need(path.suffix in {'.md', '.json', '.cpp', '.hpp', '.py', '.cmake'}, 'only scoped source/evidence')
        path.read_bytes().decode('utf-8')
    for name in ['PORT_STATUS.md', 'ROADMAP.md']:
        m.pub.need(('Published native save/profile checkpoint fork `' + code + '`') in
            (P / name).read_text(encoding='utf-8'), 'live docs identify actual code commit')
    remote = m.pub.remote_probe(P, base)
    parent, sync = m.commit_sync(P, paths,
        'Record native save Main integration, source corrections and exact remaining acceptance gaps', remote)
    result = dict(schema='native_save_profile_publication_v1', fork_code_commit=code,
        parent_commit=parent, parent_synchronization=sync,
        verification_sha256=m.pub.sha(vpath),
        publication_interruption='Initial fork commit/push succeeded; parent staging refused one long evidence path. No parent commit occurred before this recovery.',
        recovery='Process-only Git core.longpaths=true; no persistent configuration or file relocation.',
        scope='Four native components and five connected/generated clients; complete game still refuses28 Main effects; no gameplay claim')
    with (H / 'PUBLICATION.json').open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(result, stream, indent=2)
        stream.write('\n')
    for name, pin in v['protected_inputs'].items(): m.pub.need(m.pub.sha(name) == pin, 'protected source after parent publication')
    fork_remote = m.pub.remote_probe(F, code)
    metadata, metadata_sync = m.commit_sync(F, [Path(__file__).resolve(), H / 'PUBLICATION.json'],
        'Record synchronized native save checkpoint and long-path publication recovery', fork_remote)
    print(json.dumps(dict(fork_code_commit=code, fork_metadata_commit=metadata,
        fork_synchronization=metadata_sync, parent_commit=parent, parent_synchronization=sync)))

if __name__ == '__main__': main()
