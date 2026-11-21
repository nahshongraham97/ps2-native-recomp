"""Publish only the verified generated-field consumer and cited source correction."""
import importlib.util, json
from pathlib import Path
F = Path(__file__).resolve().parents[3]
P = F.parents[2]
H = Path(__file__).resolve().parent
C = P / 'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
D = C / 'native-retail-typed-consumer-28'
spec = importlib.util.spec_from_file_location('prior', F / 'research/runs/native-connected-main-dependencies-20261121/publish.py')
prior = importlib.util.module_from_spec(spec)
spec.loader.exec_module(prior)
pub = prior.pub
if __name__ == '__main__':
    vpath = H / 'validation-01/verification.json'
    pub.need(pub.sha(vpath) == 'c4258d699bb82e0b61926ffd2f2b7fb3448f258907c882699d8c069375f8e577', 'measured consumer result')
    v = json.loads(vpath.read_text(encoding='utf-8'))
    pub.need(v['schema'] == 'native_typed_scalar_consumer_verification_v1' and v['status'] == 'PASS_ACTUAL_NATIVE_SELECTOR_TYPED_FIELD' and
        len(v['sources']) == 138 and len(v['unchanged_prior_binary_identities']) == 40 and
        v['executed_client']['target'] == 'test_native_retail_input_selection' and
        len(v['remaining_integration_symbols']) == 39 and not v['game_executed'] and not v['physical_endpoints_opened'] and
        not v['complete_game_entry_linkable'], 'exact component scope')
    for file, digest in v['sources'].items():
        pub.need(pub.sha(file) == digest, 'current measured source')
    for row in [v['executed_client']] + v['unchanged_prior_binary_identities']:
        pub.need(pub.sha(F / 'build_native_services_only_20261119-01' / (row['target'] + '.exe')) == row['sha256'], 'current binary')
    review_path = D / 'review-results01/REVIEW.json'
    pub.need(pub.sha(review_path) == '632e88b4eff2e65471b30c2dd058fc7c7dbc065c33a0c74d9085f301a00dc2f6', 'independent saved consumer review')
    r = json.loads(review_path.read_text(encoding='utf-8'))
    pub.need(r['kind'] == 'read_only_saved_typed_consumer_results_review' and r['status'] == 'PASS_BOUNDED_ACTUAL_SELECTOR_CPU_REVIEW' and
        r['current_source_pins_checked'] == 138 and r['source_pin_mismatches'] == 0 and
        r['prior_binary_identities_checked'] == 40 and r['prior_binary_mismatches'] == 0 and
        r['fresh_executions'] == ['test_native_retail_input_selection'] and r['prior_binary_executions_in_this_run'] == 0 and
        r['forbidden_symbol_matches'] == 0 and r['remaining_main_bindings'] == 39, 'actual independent schema')
    for row in r['pins']:
        pub.need(pub.sha(P / row['path']) == row['sha256'], 'independent pin')
    protected = json.loads((F / 'research/runs/native-typed-retail-20261121/validation-02/verification.json').read_text(encoding='utf-8'))['protected_inputs']
    for file, digest in protected.items():
        pub.need(pub.sha(file) == digest, 'protected input')
    bases = {F: '0f77ee383020ef12d5b7101bb5ca49b3200932f9', P: 'ab07e1b0fbd9651ef25497213d5670b7d6f415af'}
    for root, head in bases.items():
        pub.need(pub.git(root, 'rev-parse', 'HEAD') == head and pub.git(root, 'branch', '--show-current') == 'main', 'base branch')
    forks = {F / n for n in ['README.md', 'cmake/native_typed.cmake', 'runtime/src/native/retail_input_selection.cpp',
        'research/runs/native-typed-retail-20261121/PUBLICATION.json']}
    forks.update(q for q in H.rglob('*') if q.is_file() and '__pycache__' not in q.parts and q.name != 'PUBLICATION.json')
    parents = {P / 'PORT_STATUS.md', P / 'ROADMAP.md', C / 'native-retail-initial-task-25/SOURCE.md',
        C / 'native-retail-task-dispatch-27/source-agent01/RAW_SOURCE.json', C / 'native-retail-task-dispatch-27/root-independent-join01/REJOIN.json'}
    for scope in [D, C / 'native-retail-initial-task-25/dispatch-correction01']:
        parents.update(q for q in scope.rglob('*') if q.is_file() and '__pycache__' not in q.parts)
    fr, pr = pub.remote_probe(F, bases[F]), pub.remote_probe(P, bases[P])
    fh, fs = prior.commit_sync(F, sorted(forks), 'Connect generated typed scalar access to actual retail selector component', fr)
    for name in ['PORT_STATUS.md', 'ROADMAP.md']:
        file = P / name
        text = file.read_text(encoding='utf-8')
        marker = 'Current verified work28 (publication pending)'
        pub.need(text.count(marker) == 1, 'one current work28 marker')
        file.write_text(text.replace(marker, 'Published typed selector checkpoint fork ' + chr(96) + fh + chr(96) + ' (work28)'), encoding='utf-8', newline='\n')
    ph, ps = prior.commit_sync(P, sorted(parents), 'Record verified selector integration and correct original task continuation evidence', pr)
    for file, digest in protected.items():
        pub.need(pub.sha(file) == digest, 'protected after publication')
    record = dict(fork_commit=fh, fork=fs, parent_commit=ph, parent=ps, verification_sha256=pub.sha(vpath),
        scope='Actual selector typed field reuse; task control source correction;39 main integrations and full game acceptance OPEN')
    with (H / 'PUBLICATION.json').open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(record, stream, indent=2)
        stream.write('\n')
    print(json.dumps(record))
