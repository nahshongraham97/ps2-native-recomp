"""Publish exactly the verified native profile-state batch and its evidence."""
import importlib.util
import json
import os
import re
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
    n = int(os.environ.get('GIT_CONFIG_COUNT','0'))
    os.environ['GIT_CONFIG_COUNT'] = str(n+1)
    os.environ['GIT_CONFIG_KEY_'+str(n)] = 'core.longpaths'
    os.environ['GIT_CONFIG_VALUE_'+str(n)] = 'true'
    vpath = H / 'validation-01/verification.json'
    m.pub.need(m.pub.sha(vpath) == '8eeb9f0619a0a2befc400719d6949cf4b72d970cc746378ec418041392b9c8a8', 'exact current measurement')
    v = json.loads(vpath.read_text(encoding='utf-8'))
    m.pub.need(v['schema'] == 'native_profile_state_connected_verification_v1' and
        v['status'] == 'PASS_NATIVE_PROFILE_STATE_CONNECTED_CPU_CHECKS' and
        len(v['executed_clients']) == 7 and len(v['remaining_integration_symbols']) == 27 and
        len(v['required_profile_effects']) == 5 and not v['complete_game_entry_linkable'] and
        not v['game_executed'] and not v['physical_endpoints_opened'], 'actual bounded evidence scope')
    for group in [v['sources'],v['evidence'],v['protected_inputs']]:
        for path,pin in group.items():m.pub.need(m.pub.sha(path) == pin,'current measured bytes: '+path)
    build = F / 'build_native_services_only_20261119-01'
    for key,name in [('startup_archive_sha256','libmka_native_retail_startup.a'),
                     ('session_archive_sha256','libmka_native_retail_game_session.a')]:
        m.pub.need(m.pub.sha(build/name) == v[key], 'current production archive')
    for row in v['executed_clients']:
        r = json.loads((H/'validation-01'/row['receipt']).read_text(encoding='utf-8'))
        m.pub.need(r['exit'] == 0 and not r['stderr'] and r['executable_sha256'] ==
            r['executable_after_sha256'] == row['sha256'] == m.pub.sha(build/(row['target']+'.exe')),
            'actual passed client identity: '+row['target'])
    for name,expected in [('actual-entry-link.json',27),('required-native-effects-link.json',5)]:
        r = json.loads((H/'validation-01'/name).read_text(encoding='utf-8'))
        missing = re.findall(r'undefined symbol: ([^\n]+)',r['stdout']+r['stderr'])
        m.pub.need(r['exit'] != 0 and len(missing) == expected,'actual distinct required-link refusal')
    rpath = C/'native-retail-profile-settings-69/review02/REVIEW.json'
    review = json.loads(rpath.read_text(encoding='utf-8'))
    m.pub.need(review['schema'] == 'native_retail_profile_state_saved_review_v1' and
        review['status'] == 'PASS_BOUNDED_CONNECTED_CPU_CHECKS' and
        review['material_current_findings'] == [] and
        review['verification_sha256'] == m.pub.sha(vpath) and
        review['current_pins_rehashed'] == dict(sources=len(v['sources']),evidence=len(v['evidence']),protected_inputs=len(v['protected_inputs'])) and
        review['saved_execution']['exit_zero_current_executables'] == 7 and
        review['negative_links']['full_entry_exact_unresolved'] == 27 and
        review['negative_links']['required_profile_effects_exact_unresolved'] == 5,
        'actual independent source/result review schema')
    code = ['cmake/native_game.cmake','runtime/include/mka/retail_game_records_values.hpp',
        'runtime/include/mka/retail_game_session.hpp','runtime/src/native/retail_game_records.cpp',
        'runtime/src/native/retail_game_session.cpp','runtime/include/mka/retail_main.hpp',
        'runtime/src/native/retail_main.cpp','runtime/src/native/retail_profile_preferences_main.cpp',
        'tests/test_native_profile_required_effects.cpp']
    for stem in ['retail_profile_preferences','retail_profile_menu','retail_profile_resource_ids','retail_profile_aggregate_main']:
        code += ['runtime/include/mka/'+stem+'.hpp','runtime/src/native/'+stem+'.cpp','tests/test_'+stem+'.cpp']
    forks = [F/name for name in code] + [H/name for name in ['README.md','check.py','publish.py']]
    forks += sorted((H/'validation-01').glob('*.json'))
    parents = [P/'PORT_STATUS.md',P/'ROADMAP.md']
    for name in ['native-retail-profile-menu-67','native-retail-profile-resource-ids-68','native-retail-profile-settings-69']:
        parents += sorted(p for p in (C/name).rglob('*') if p.is_file() and '__pycache__' not in p.parts)
    immutable = {}
    for path in forks+parents:
        m.pub.need(path.suffix in {'.md','.json','.cpp','.hpp','.py','.cmake'},'only scoped text/source: '+str(path))
        data = path.read_bytes()
        data.decode('utf-8')
        preserved = any(part.startswith('before') for part in path.parts)
        if not preserved:m.pub.need(b'\r' not in data and data.endswith(b'\n'),'normalized active text: '+str(path))
        immutable[str(path)] = m.pub.sha(path)
    bases = {F:'3d8122cca04b5f9a269040fd7b2ddd9f7ade5136',P:'ca3eb803d22f356c6576d44b6be03769fbb6f1df'}
    remote = {}
    for root,base in bases.items():
        m.pub.need(m.pub.git(root,'rev-parse','HEAD') == base and
            m.pub.git(root,'branch','--show-current') == 'main','expected source base')
        remote[root] = m.pub.remote_probe(root,base)
    m.pub.need(all(m.pub.sha(path) == pin for path,pin in immutable.items()),'stable authored bytes before staging')
    fh,fs = m.commit_sync(F,forks,'Connect native current preferences and source-ordered profile state on the retained Session',remote[F])
    marker = 'Working-tree native profile-state checkpoint (base fork `3d8122cca04b5f9a269040fd7b2ddd9f7ade5136`)'
    for name in ['PORT_STATUS.md','ROADMAP.md']:
        path = P/name
        text = path.read_text(encoding='utf-8')
        m.pub.need(text.count(marker) == 1,'one live checkpoint marker')
        with path.open('w',encoding='utf-8',newline='\n') as stream:
            stream.write(text.replace(marker,'Published native profile-state checkpoint fork `'+fh+'`',1))
    ph,ps = m.commit_sync(P,parents,'Record verified native profile preferences, menu/ID state and exact open production effects',remote[P])
    for path,pin in v['protected_inputs'].items():m.pub.need(m.pub.sha(path) == pin,'protected source after publication')
    result = dict(schema='native_profile_state_publication_v1',fork_commit=fh,fork=fs,parent_commit=ph,parent=ps,
        verification_sha256=m.pub.sha(vpath),scope='Seven CPU/file clients;27 Main requirements and separate5 scene/actor requirements remain; no gameplay claim')
    with (H/'PUBLICATION.json').open('x',encoding='utf-8',newline='\n') as stream:
        json.dump(result,stream,indent=2)
        stream.write('\n')
    print(json.dumps(result))

if __name__ == '__main__':main()
