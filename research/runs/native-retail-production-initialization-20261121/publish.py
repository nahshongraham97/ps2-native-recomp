"""Checkpoint the measured production initialization only."""
import importlib.util
import json
from pathlib import Path

F = Path(__file__).resolve().parents[3]
P = F.parents[2]
H = Path(__file__).resolve().parent
C = P / 'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
D = C / 'native-retail-production-initialization-36'
spec = importlib.util.spec_from_file_location('publisher', F / 'research/runs/native-connected-main-dependencies-20261121/publish.py')
h = importlib.util.module_from_spec(spec)
spec.loader.exec_module(h)
pub = h.pub

if __name__ == '__main__':
    vp = H / 'validation-01/verification.json'
    v = json.loads(vp.read_text(encoding='utf-8'))
    pub.need(v['schema'] == 'native_retail_production_initialization_checks_v1'
             and v['status'] == 'PASS_CONNECTED_PRODUCTION_INITIALIZATION'
             and isinstance(v['sources'], dict) and isinstance(v['evidence'], dict)
             and len(v['remaining_integration_symbols']) == 36
             and [r['target'] for r in v['executed_clients']] ==
                 ['test_native_retail_startup_initialization', 'test_native_retail_active_overlay']
             and not v['game_executed'] and not v['physical_endpoints_opened'], 'actual targeted result schema')
    for group in [v['sources'], v['evidence'], v['protected_inputs']]:
        for path, digest in group.items():
            pub.need(pub.sha(path) == digest, 'stable measured source/protection: ' + path)
    build = F / 'build_native_services_only_20261119-01'
    pub.need(pub.sha(build / 'libmka_native_retail_startup.a') == v['startup_archive_sha256'], 'actual startup archive')
    for row in v['executed_clients']:
        receipt = json.loads((H / 'validation-01' / (row['target'] + '.json')).read_text(encoding='utf-8'))
        pub.need(receipt['exit'] == 0 and 'reused_from' not in receipt
                 and receipt['executable_sha256'] == row['sha256'] == pub.sha(build / (row['target'] + '.exe')),
                 'actual targeted execution')
    bases = {F: '8b641d2941df55dab5f2f644fe4d1d20688ab778', P: 'c88e8672e7b91bd6eb92ee8819129f327bbbcb42'}
    for root, base in bases.items():
        pub.need(pub.git(root, 'rev-parse', 'HEAD') == base and pub.git(root, 'branch', '--show-current') == 'main', 'checkpoint base')
    fr, pr = pub.remote_probe(F, bases[F]), pub.remote_probe(P, bases[P])
    readme = F / 'README.md'
    text = readme.read_text(encoding='utf-8')
    addition = '\nProduction initialization checkpoint, 2026-11-21: profile reset, mode-one GAMER initializer and post-overlay state are wired into native Main. Two targeted clients pass; actual complete-entry link still refuses36 integration names. This is not game/physical-input/render/audio acceptance. See research/runs/native-retail-production-initialization-20261121/validation-01/verification.json and the parent current status.\n'
    if addition not in text:
        text += addition
    readme.write_text(text, encoding='utf-8', newline='\n')
    # Verification includes read-only dependency/tool inputs. Stage only this
    # batch's authored paths, never every measured dependency.
    authored = ['cmake/native_game.cmake', 'recomp/native_startup_codegen.cpp',
        'runtime/include/mka/retail_main.hpp', 'runtime/include/mka/allocation_class.hpp',
        'runtime/src/native/allocation_class.cpp', 'runtime/include/mka/retail_profile_runtime.hpp',
        'runtime/include/mka/retail_profile_main_state.hpp', 'runtime/src/native/retail_profile_main_state.cpp',
        'runtime/src/native/retail_profile_main.cpp', 'runtime/include/mka/retail_active_overlay.hpp',
        'runtime/src/native/retail_active_overlay.cpp', 'runtime/src/native/retail_active_overlay_main.cpp',
        'runtime/src/native/retail_post_overlay.cpp', 'tests/test_retail_active_overlay.cpp',
        'tests/test_retail_startup_initialization.cpp', 'research/runs/native-retail-production-initialization-20261121/check.py']
    forks = {F / n for n in authored} | {readme, Path(__file__).resolve()}
    forks.update(p for p in (H / 'validation-01').iterdir() if p.is_file())
    forks.update(p for p in (H / 'publication-before-source-scope').rglob('*') if p.is_file())
    fh, fs = h.commit_sync(F, sorted(forks), 'Connect real native profile GAMER and post-overlay initialization', fr)
    current = ('Published production-initialization checkpoint fork ' + fh + ': real182670 profile/reset, mode-one3DDA60 GAMER initializer and182E50 state reset are wired into the actual native startup/Main library. Two targeted clients pass freshly; complete-entry link refuses exactly36 remaining integrations. Other overlays, later resource callbacks/companion callpoint, complete initialization/game loop/physical input/render/audio and full-game acceptance remain open. See research/runs/native-retail-startup-20261120/connected-retail-entry-01/native-retail-production-initialization-36/RESULTS.md. Active work continues on2173D0 resource registry within1303F0/130350 and real19C4C0 section workspace. Broad regressions deferred until integrated candidate; owner no action.')
    for name in ['PORT_STATUS.md', 'ROADMAP.md']:
        path = P / name
        lines = path.read_text(encoding='utf-8').splitlines()
        found = [i for i, line in enumerate(lines) if line.startswith('Active production integration (unbuilt):')]
        pub.need(len(found) == 1, 'one current initialization paragraph')
        lines[found[0]] = current
        path.write_text('\n'.join(lines) + '\n', encoding='utf-8', newline='\n')
    record = dict(schema='native_production_initialization_publication_v1', fork_commit=fh, fork=fs,
        parent_commit='enclosing commit', verification_sha256=pub.sha(vp), remaining_integrations=36,
        scope='Production initialization CPU/file component; no accepted game executable')
    with (D / 'PUBLICATION.json').open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(record, stream, indent=2)
        stream.write('\n')
    parents = {P / 'PORT_STATUS.md', P / 'ROADMAP.md'}
    for name in ['native-retail-profile-binding-30', 'native-retail-task-main-31',
                 'native-retail-active-overlay-32', 'native-retail-post-overlay-33',
                 'native-retail-production-initialization-36']:
        parents.update(p for p in (C / name).rglob('*') if p.is_file() and '__pycache__' not in p.parts)
    ph, ps = h.commit_sync(P, sorted(parents), 'Record connected native production startup and remaining initialization work', pr)
    for path, digest in v['protected_inputs'].items():
        pub.need(pub.sha(path) == digest, 'preserved input through publication')
    print(json.dumps(dict(fork_commit=fh, fork=fs, parent_commit=ph, parent=ps)))
