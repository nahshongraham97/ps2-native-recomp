"""Publish the exact verified connected profile-effect source and evidence."""
import importlib.util
import json
import os
from pathlib import Path

F = Path(__file__).resolve().parents[3]
P = F.parents[2]
H = Path(__file__).resolve().parent
C = P/'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
s = importlib.util.spec_from_file_location('profile_effect_publication',
    F/'research/runs/native-connected-main-dependencies-20261121/publish.py')
m = importlib.util.module_from_spec(s)
s.loader.exec_module(m)


def load(path):
    return json.loads(path.read_text(encoding='utf-8'))


def main():
    n = int(os.environ.get('GIT_CONFIG_COUNT','0'))
    os.environ.update({'GIT_CONFIG_COUNT':str(n+1),
        'GIT_CONFIG_KEY_'+str(n):'core.longpaths','GIT_CONFIG_VALUE_'+str(n):'true'})
    vpath = H/'validation-01/verification.json'
    m.pub.need(m.pub.sha(vpath) == 'f9ed93e4e439510a0b9ae3667ef2bdf0d615f7b9f80a481cbdebe05de5a10515',
        'exact measured profile-effects result')
    v = load(vpath)
    m.pub.need(v['schema'] == 'native_retail_profile_effects_connected_verification_v1' and
        v['status'] == 'PASS_NATIVE_PROFILE_TRANSITION_DECISION_CPU_FILE_CHECKS' and
        len(v['sources']) == 94 and len(v['evidence']) == 17 and len(v['protected_inputs']) == 4 and
        len(v['executed_clients']) == 12 and len(v['required_profile_effects']) == 5 and
        len(v['remaining_integration_symbols']) == 27 and not v['complete_game_entry_linkable'] and
        not v['game_executed'] and not v['physical_endpoints_opened'] and v['frames'] is None and v['audio'] is None,
        'actual bounded current evidence scope')
    for group in ['sources','evidence','protected_inputs']:
        for path,pin in v[group].items():m.pub.need(m.pub.sha(path) == pin,'stable '+group+': '+path)
    build = F/'build_native_services_only_20261119-01'
    for key,name in [('startup_archive_sha256','libmka_native_retail_startup.a'),
                     ('session_archive_sha256','libmka_native_retail_game_session.a')]:
        m.pub.need(m.pub.sha(build/name) == v[key],'current production archive')
    for row in v['executed_clients']:
        r = load(H/'validation-01'/row['receipt'])
        m.pub.need(r['exit'] == 0 and not r['stderr'] and r['executable_sha256'] ==
            r['executable_after_sha256'] == row['sha256'] == m.pub.sha(build/(row['target']+'.exe')),
            'actual current passed executable')
    src_path = C/'native-retail-profile-transition-child-72/review01/REVIEW.json'
    m.pub.need(m.pub.sha(src_path) == '88068642be97e2067fb6c8ab4be9391e5ff257eb88a18d98cd52c4806350e8a8',
        'exact independent source review')
    src = load(src_path)
    m.pub.need(src['schema'] == 'native_retail_profile_transition_child_72_independent_source_review_v1' and
        src['status'] == 'SOURCE_REVIEW_PASSED_BOUNDED_UNBUILT' and src['source_pins_checked'] == 13 and
        src['findings'] == [],'actual source review scope')
    rev_path = C/'native-retail-profile-transition-child-72/review-results01/REVIEW.json'
    m.pub.need(m.pub.sha(rev_path) == '4bfe4a6ed9f7f47e2af473d993b665f99858305d282a0c00e77e039107c0af7d',
        'exact independent saved-result review after reporting correction')
    rev = load(rev_path)
    m.pub.need(rev['schema'] == 'native_retail_profile_effects_connected_saved_review_v1' and
        rev['status'] == 'SAVED_CPU_FILE_EVIDENCE_VERIFIED_BOUNDED' and
        rev['verification_sha256'] == m.pub.sha(vpath) and rev['checker_sha256'] == m.pub.sha(H/'check.py') and
        rev['source_pins_checked'] == 94 and rev['evidence_pins_checked'] == 17 and
        rev['protected_pins_checked'] == 4 and rev['source_pins'] == v['sources'] and
        rev['evidence_pins'] == v['evidence'] and rev['protected_pins'] == v['protected_inputs'] and
        len(rev['executed_clients']) == 12 and len(rev['symbol_audits']) == 7 and
        rev['original_table_keys_match'] and rev['original_table_keys_count'] == 64 and
        rev['required_profile_effects'] == v['required_profile_effects'] and
        rev['remaining_main_integration_symbols'] == v['remaining_integration_symbols'] and
        rev['required_profile_executable_absent'] and rev['full_entry_executable_absent'] and
        not rev['game_executed'] and not rev['physical_endpoints_opened'] and rev['findings'] == [],
        'actual saved-result review schema and scope')
    authored = load(C/'native-retail-profile-transition-child-72/root01/IMPLEMENTATION.json')
    forks = [Path(path) for path in authored['sources']]
    forks += [H/name for name in ['README.md','check.py','publish.py']]
    forks += sorted((H/'validation-01').glob('*.json'))
    forks += [F/'research/runs/native-retail-live-scene-20261121/README.md']
    parents = [P/'AGENTS.md',P/'PORT_STATUS.md',P/'ROADMAP.md',C/'native-retail-live-scene-70/README.md']
    for scope in ['native-retail-profile-transition-child-72','native-retail-profile-entry-decision-73',
                  'native-retail-profile-participant-transition-74']:
        parents += sorted(p for p in (C/scope).rglob('*') if p.is_file() and '__pycache__' not in p.parts)
    for path in forks+parents:
        raw = path.read_bytes()
        raw.decode('utf-8')
        m.pub.need(path.suffix in {'.md','.json','.hpp','.cpp','.py','.cmake'},'scoped text/code only')
        preserved = any(part.startswith('before') for part in path.parts)
        if not preserved:m.pub.need(b'\r' not in raw and raw.endswith(b'\n'),'normalized active text: '+str(path))
    bases = {F:'59250ac18b43baa3c7b58b7c8283629b06ed734f',P:'79c6ec77e2753e8765aa157d00b0fb395a1b97d9'}
    remote = {}
    for root,base in bases.items():
        m.pub.need(m.pub.git(root,'rev-parse','HEAD') == base and
            m.pub.git(root,'branch','--show-current') == 'main','expected source base')
        remote[root] = m.pub.remote_probe(root,base)
    fh,fs = m.commit_sync(F,forks,
        'Connect source-ordered retail profile transition and live mask decision',remote[F])
    marker = 'Working-tree profile-effects checkpoint (base fork `'+bases[F]+'`)'
    for name in ['PORT_STATUS.md','ROADMAP.md']:
        path = P/name
        text = path.read_text(encoding='utf-8')
        m.pub.need(text.count(marker) == 1,'one current checkpoint marker')
        with path.open('w',encoding='utf-8',newline='\n') as stream:
            stream.write(text.replace(marker,'Published profile-effects checkpoint fork `'+fh+'`',1))
    ph,ps = m.commit_sync(P,parents,
        'Record native profile transition and decision checks with exact remaining effects',remote[P])
    for path,pin in v['protected_inputs'].items():m.pub.need(m.pub.sha(path) == pin,'protected bytes after publication')
    record = dict(schema='native_retail_profile_effects_publication_v1',fork_commit=fh,fork=fs,parent_commit=ph,
        parent=ps,verification_sha256=m.pub.sha(vpath),
        scope='Twelve CPU/file clients;actual18B0D0/1892D0;five native leaf requirements and27 Main;no gameplay')
    with (H/'PUBLICATION.json').open('x',encoding='utf-8',newline='\n') as stream:
        json.dump(record,stream,indent=2)
        stream.write('\n')
    print(json.dumps(record))


if __name__ == '__main__':
    main()
