"""Publish measured first/second retail owners; exclude later draft owners."""
import importlib.util
import json
from pathlib import Path
import subprocess

F = Path(__file__).resolve().parents[3]
P = F.parents[2]
S = P / 'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('checkpoint_publication',
    F / 'research/runs/native-connected-retail-entry-20261120/publish.py')
pub = importlib.util.module_from_spec(spec)
spec.loader.exec_module(pub)

if __name__ == '__main__':
    result = json.loads((HERE / 'validation-01/verification.json').read_bytes().decode('utf-8'))
    pub.need(result['status'] == 'PASS_NATIVE_RETAIL_CONSTRUCTOR_OWNERS_CPU_CHECKS'
        and result['complete_entry_linkable'] is False, 'measured constructor scope')
    for path, expected in result['sources'].items():
        pub.need(pub.sha(path) == expected, 'measured current source: ' + path)
    review_path = S / 'constructor-integration-02/EXECUTION_REVIEW.json'
    review = json.loads(review_path.read_bytes().decode('utf-8'))
    pub.need(review['status'] == 'SAVED_NATIVE_RETAIL_CONSTRUCTOR_OWNERS_EXECUTION_VERIFIED', 'saved execution review')
    fork_base = '7e14e90c16cc746c764bf3ea80dce6ecf4685516'
    parent_base = '7603f3382db65f7b43faa0e7609ff94a6de96c60'
    pub.need(pub.git(F, 'rev-parse', 'HEAD') == fork_base and pub.git(P, 'rev-parse', 'HEAD') == parent_base, 'checkpoint bases')
    pub.need(pub.git(F, 'branch', '--show-current') == pub.git(P, 'branch', '--show-current') == 'main', 'main branches')
    files = [
        'cmake/native_game.cmake', 'runtime/include/mka/retail_main.hpp', 'runtime/src/native/retail_main.cpp',
        'runtime/include/mka/retail_initial_objects.hpp', 'runtime/src/native/retail_initial_objects.cpp',
        'runtime/include/mka/retail_scene_events.hpp', 'runtime/src/native/retail_scene_events.cpp',
        'runtime/include/mka/retail_middleware_work.hpp', 'runtime/src/native/retail_middleware_work.cpp',
        'tests/test_retail_scene_owners.cpp', 'tests/test_retail_middleware_work.cpp',
        'research/runs/native-connected-retail-entry-20261120/PUBLICATION.json',
    ]
    forks = [F / name for name in files]
    forks += [q for q in HERE.rglob('*') if q.is_file()
        and 'owned-fixtures' not in q.relative_to(HERE).parts and q.name != 'PUBLICATION.json']
    parents = [P / name for name in ['PORT_STATUS.md', 'ROADMAP.md']]
    for folder in ['initial-constructors', 'controller-constructor', 'constructor-integration-02']:
        parents += [q for q in (S / folder).rglob('*') if q.is_file()]
    forks, parents = sorted(set(forks)), sorted(set(parents))
    for path in forks + parents:
        relative = path.relative_to(F) if path.is_relative_to(F) else path.relative_to(P)
        if 'before' in str(relative):
            continue  # Raw source/history retains exact prior bytes.
        data = path.read_bytes()
        pub.need(b'\r' not in data and data.endswith(b'\n') and not data.endswith(b'\n\n'), 'authored LF/EOF: ' + str(path))
        pub.need(all(line == line.rstrip() for line in data.splitlines()), 'authored whitespace: ' + str(path))
    protected = {
        F / 'runtime/src/iop/mcserv.cpp': 'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',
        F / 'runtime/src/iop/msl_native.cpp': '756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23',
        P / 'research/doc-audit-20261118/inventory.json': '26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6',
    }
    for path, expected in protected.items():
        pub.need(pub.sha(path) == expected, 'protected bytes')
    fr, pr = pub.remote_probe(F, fork_base), pub.remote_probe(P, parent_base)
    fh, fs = pub.commit_sync(F, forks,
        'Bind first retail constructors to native scene and middleware ownership with strict completion identity', fr)
    for name in ['PORT_STATUS.md', 'ROADMAP.md']:
        path = P / name
        text = path.read_bytes().decode('utf-8')
        text = text.replace('independent saved-results review is pending', 'independent source and saved-results reviews are verified', 1)
        anchor = 'The first scene-event constructor and correctly identified middleware work group'
        pub.need(anchor in text, 'current constructor documentation')
        text = text.replace(anchor, 'Published constructor checkpoint fork ' + chr(96) + fh + chr(96)
            + ': the first scene-event constructor and correctly identified middleware work group', 1)
        path.write_bytes(text.encode('utf-8'))
    ph, ps = pub.commit_sync(P, parents,
        'Record verified native constructor checkpoint and corrected subsystem identities', pr)
    for path, expected in protected.items():
        pub.need(pub.sha(path) == expected, 'protected bytes after publication')
    with (HERE / 'PUBLICATION.json').open('x', encoding='utf-8', newline='\n') as stream:
        json.dump({'fork_commit': fh, 'fork': fs, 'parent_commit': ph, 'parent': ps,
            'verification_sha256': pub.sha(HERE / 'validation-01/verification.json'),
            'scope': 'Two native constructor owners and test-port lifecycle evidence; no complete game.'}, stream, indent=2)
        stream.write('\n')
    print(json.dumps({'fork_commit': fh, 'fork': fs, 'parent_commit': ph, 'parent': ps}))
