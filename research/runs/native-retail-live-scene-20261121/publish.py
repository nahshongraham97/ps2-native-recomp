"""Scoped publication of the measured live-scene/profile transition batch."""
import importlib.util
import json
import os
from pathlib import Path

F = Path(__file__).resolve().parents[3]
P = F.parents[2]
H = Path(__file__).resolve().parent
C = P / 'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
s = importlib.util.spec_from_file_location('scene_scoped_publication',
    F / 'research/runs/native-connected-main-dependencies-20261121/publish.py')
m = importlib.util.module_from_spec(s)
s.loader.exec_module(m)


def load(path):
    return json.loads(path.read_text(encoding='utf-8'))


def main():
    n = int(os.environ.get('GIT_CONFIG_COUNT','0'))
    os.environ.update({'GIT_CONFIG_COUNT':str(n+1),
        'GIT_CONFIG_KEY_'+str(n):'core.longpaths','GIT_CONFIG_VALUE_'+str(n):'true'})
    vpath = H / 'validation-02/verification.json'
    m.pub.need(m.pub.sha(vpath) == 'b539548a5b8ad757983da032a545039df35716578da55c1e2a5edf2e464d9a7d',
        'exact measured connected result')
    v = load(vpath)
    m.pub.need(v['schema'] == 'native_live_scene_profile_transition_connected_verification_v1' and
        v['status'] == 'PASS_NATIVE_LIVE_SCENE_PROFILE_TRANSITION_CPU_CHECKS' and
        len(v['sources']) == 87 and len(v['evidence']) == 16 and
        len(v['executed_clients']) == 10 and len(v['remaining_integration_symbols']) == 27 and
        len(v['required_profile_effects']) == 3 and v['client_executions_reused_from_current_passed_build'] and
        not v['complete_game_entry_linkable'] and not v['game_executed'] and
        not v['physical_endpoints_opened'] and v['frames'] is None and v['audio'] is None,
        'actual measured scope and limits')
    for group in ['sources','evidence','protected_inputs']:
        for path,pin in v[group].items(): m.pub.need(m.pub.sha(path) == pin,'stable '+group+': '+path)
    build = F / 'build_native_services_only_20261119-01'
    for key,name in [('startup_archive_sha256','libmka_native_retail_startup.a'),
                     ('session_archive_sha256','libmka_native_retail_game_session.a')]:
        m.pub.need(m.pub.sha(build/name) == v[key],'exact production archive')
    for row in v['executed_clients']:
        r = load(H/'validation-02'/row['receipt'])
        m.pub.need(r['exit'] == 0 and not r['stderr'] and r['executable_sha256'] ==
            r['executable_after_sha256'] == row['sha256'] == m.pub.sha(build/(row['target']+'.exe')) and
            m.pub.sha(r['reused_from']) == r['reused_record_sha256'], 'current authenticated passed executable')
    source_path = C/'native-retail-live-scene-70/review-integration01/REVIEW.json'
    m.pub.need(m.pub.sha(source_path) == 'cf2e9f1f7589cef6a4524a4eaf47857799ee9f157b79102f009c1eaa5a32f652',
        'exact independent source review')
    source = load(source_path)
    m.pub.need(source['schema'] == 'native_live_scene_profile_transition_independent_source_review_v1' and
        source['status'] == 'SOURCE_REVIEW_PASS_BUILD_UNREVIEWED','source review scope')
    results_path = C/'native-retail-live-scene-70/review-results01/REVIEW.json'
    m.pub.need(m.pub.sha(results_path) == '80f7c262e50038b90ba37e904b7da35aec09f19ea15ee3995548111aaeb9a0ee',
        'exact independent saved-result review')
    results = load(results_path)
    m.pub.need(results['schema'] == 'native_live_scene_profile_transition_saved_execution_review_v1' and
        results['status'] == 'PASS_SAVED_CPU_FILE_RESULTS_NO_INTEGRITY_FINDING' and
        results['verification_sha256'] == m.pub.sha(vpath) and
        results['checker_sha256'] == m.pub.sha(H/'check.py') and
        results['source_pins_checked'] == 87 and results['evidence_pins_checked'] == 16 and
        results['protected_pins_checked'] == 4 and results['pin_mismatches'] == 0 and
        results['clients_executed_in_validation_01'] == results['clients_reused_in_validation_02'] == 10 and
        results['required_profile_effects'] == v['required_profile_effects'] and
        results['remaining_main_integration_symbols'] == v['remaining_integration_symbols'] and
        results['material_findings'] == [] and not results['game_executed'] and
        not results['reviewer_reran_clients'], 'actual saved-result review schema and scope')
    author = load(C/'native-retail-live-scene-70/root02/IMPLEMENTATION.json')
    forks = [F/name for name in author['native_sources_sha256']]
    forks += [H/name for name in ['README.md','check.py','publish.py']]
    for name in ['validation-01','validation-02']:
        forks += sorted((H/name).glob('*.json'))
    parents = [P/'PORT_STATUS.md',P/'ROADMAP.md',
        C/'native-retail-profile-child-65/README.md']
    for scope in ['native-retail-live-scene-70','native-retail-profile-transition-71',
        'native-retail-profile-transition-child-72']:
        parents += sorted(p for p in (C/scope).rglob('*')
            if p.is_file() and '__pycache__' not in p.parts)
    for path in forks+parents:
        raw = path.read_bytes()
        raw.decode('utf-8')
        m.pub.need(path.suffix in {'.hpp','.cpp','.py','.md','.json','.cmake'},'scoped text source/evidence only')
        preserved = any(part.startswith('before') or part == 'root-rule-before' for part in path.parts)
        if not preserved:
            m.pub.need(b'\r' not in raw and raw.endswith(b'\n') and not raw.endswith(b'\n\n'),
                'normalized active text: '+str(path))
    bases = {F:'a7274ae214e0b9f0a0d60cd41b4bb8c71417d4c4',P:'75b676c7812dade1a88a978d705b09bfc4d4f924'}
    remote = {}
    for root,base in bases.items():
        m.pub.need(m.pub.git(root,'rev-parse','HEAD') == base and
            m.pub.git(root,'branch','--show-current') == 'main','expected publication base')
        remote[root] = m.pub.remote_probe(root,base)
    fh,fs = m.commit_sync(F,forks,
        'Connect retained live scene identity and source-ordered retail profile transition',remote[F])
    marker = 'Working-tree live-scene/profile-transition checkpoint (base fork `'+bases[F]+'`)'
    for name in ['PORT_STATUS.md','ROADMAP.md']:
        path = P/name
        text = path.read_text(encoding='utf-8')
        m.pub.need(text.count(marker) == 1,'single live checkpoint marker')
        with path.open('w',encoding='utf-8',newline='\n') as stream:
            stream.write(text.replace(marker,'Published live-scene/profile-transition checkpoint fork `'+fh+'`',1))
    ph,ps = m.commit_sync(P,parents,
        'Record connected native scene and profile transition evidence and remaining production work',remote[P])
    for path,pin in v['protected_inputs'].items():m.pub.need(m.pub.sha(path) == pin,'preserved protected input')
    record = dict(schema='native_live_scene_profile_transition_publication_v1',fork_commit=fh,fork=fs,
        parent_commit=ph,parent=ps,verification_sha256=m.pub.sha(vpath),
        scope='Ten authenticated current CPU/file clients;three required profile effects;27 Main integrations;no gameplay')
    with (H/'PUBLICATION.json').open('x',encoding='utf-8',newline='\n') as stream:
        json.dump(record,stream,indent=2)
        stream.write('\n')
    print(json.dumps(record))


if __name__ == '__main__':
    main()
