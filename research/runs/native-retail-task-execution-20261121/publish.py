"""Publish bounded typed task execution with exact source and run reviews."""
import importlib.util, json
from pathlib import Path
F = Path(__file__).resolve().parents[3]
P = F.parents[2]
H = Path(__file__).resolve().parent
C = P / 'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
D = C / 'native-retail-task-dispatch-27'
spec = importlib.util.spec_from_file_location('previous', F / 'research/runs/native-connected-main-dependencies-20261121/publish.py')
previous = importlib.util.module_from_spec(spec)
spec.loader.exec_module(previous)
pub = previous.pub

def reviewed_path(name):
    if name == 'prior_validation_verification.json':
        return F / 'research/runs/native-typed-retail-consumer-20261121/validation-01/verification.json'
    if name.startswith(('validation-01/',)) or name == 'verify.py':
        return H / name
    if name.startswith(('runtime/', 'tests/', 'cmake/')):
        return F / name
    if name.startswith(('source-agent01/', 'review-results01/', 'review-implementation01/')):
        return D / name
    raise RuntimeError('unrecognized independent pin namespace: ' + name)

if __name__ == '__main__':
    vp = H / 'validation-01/verification.json'
    pub.need(pub.sha(vp) == '78bd0c530e2a7689db2dcf05e4488a53498f4b6891f94af9ba19bf5ab65bb0c0', 'exact CPU result')
    v = json.loads(vp.read_text(encoding='utf-8'))
    pub.need(v['schema'] == 'native_retail_task_execution_verification_v1' and v['status'] == 'PASS_BOUNDED_NATIVE_TASK_EXECUTION' and
        len(v['sources']) == 142 and len(v['source_evidence']) == 5 and
        [x['target'] for x in v['executed_clients']] == ['test_native_retail_task_registry', 'test_native_retail_task_execution'] and
        len(v['unchanged_prior_binary_identities']) == 40 and len(v['remaining_integration_symbols']) == 39 and
        not v['complete_game_entry_linkable'] and not v['game_executed'] and not v['physical_endpoints_opened'] and
        v['frames'] is None and v['audio'] is None, 'measured initial-branch scope')
    for file, digest in (v['sources'] | v['source_evidence'] | v['protected_inputs']).items():
        pub.need(pub.sha(file) == digest, 'stable current measured input')
    for row in v['executed_clients'] + v['unchanged_prior_binary_identities']:
        pub.need(pub.sha(F / 'build_native_services_only_20261119-01' / (row['target'] + '.exe')) == row['sha256'], 'current client identity')
    pub.need(pub.sha(F / 'build_native_services_only_20261119-01/libmka_native_retail_task_registry.a') == v['registry_archive_sha256'], 'current actual registry archive')
    pub.need(pub.sha(F / 'build_native_services_only_20261119-01/libmka_native_retail_startup.a') == v['main_archive_sha256'], 'unchanged actual main archive')
    sp = D / 'review-implementation01/REVIEW.json'
    rp = D / 'review-results01/REVIEW.json'
    pub.need(pub.sha(sp) == 'd3fbe9eb55172fd667617ce184eb1f244e725f3c725b4076de38c6da33489239', 'exact independent R3 source review')
    pub.need(pub.sha(rp) == '2c42dbada4492b6f258cdcd2c41836d11e29f4e774d7cf48fcb6c5c54ca7a6f6', 'exact independent saved result review')
    sr = json.loads(sp.read_text(encoding='utf-8'))
    rr = json.loads(rp.read_text(encoding='utf-8'))
    pub.need(sr['schema'] == 'native_retail_task_execution_independent_source_review_v1' and sr['status'] == 'SOURCE_GRADE_NO_CONFIRMED_BLOCKER_PREBUILD', 'actual source review status')
    pub.need(rr['schema'] == 'native_retail_task_execution_saved_result_review_v1' and rr['status'] == 'PASS_BOUNDED_SAVED_RECEIPT_REVIEW' and
        rr['counts'] == dict(current_source_pins_rehashed=142, source_evidence_pins_rehashed=5, protected_input_pins_rehashed=4,
            fresh_cpu_clients=2, prior_binary_identity_checks_only=40, remaining_entry_bindings=39) and
        not rr['reviewer_ran_build_or_client'], 'actual saved-result schema/counts')
    for review in [sr, rr]:
        for name, digest in review['reviewed_sha256'].items():
            pub.need(pub.sha(reviewed_path(name)) == digest, 'exact independent source/receipt pin')
    bases = {F: '1ebdb89a1a9fc28b61c92ff5b4a0513e056d52c8', P: '65984829f90d4642b42ead4b77ca08c1be3b1dc9'}
    for root, head in bases.items():
        pub.need(pub.git(root, 'rev-parse', 'HEAD') == head and pub.git(root, 'branch', '--show-current') == 'main', 'publication base')
    authored = ['README.md', 'cmake/native_typed.cmake', 'runtime/include/mka/retail_task_registry.hpp',
        'runtime/src/native/retail_task_registry.cpp', 'tests/test_retail_task_registry.cpp',
        'runtime/include/mka/retail_task_execution.hpp', 'runtime/src/native/retail_task_execution.cpp',
        'tests/test_retail_task_execution.cpp', 'research/runs/native-typed-retail-consumer-20261121/PUBLICATION.json']
    forks = {F / n for n in authored}
    forks.update(q for q in H.rglob('*') if q.is_file() and '__pycache__' not in q.parts and q.name != 'PUBLICATION.json')
    parents = {P / 'PORT_STATUS.md', P / 'ROADMAP.md', C / 'native-retail-task-registry-24/RESULTS.md',
        C / 'native-retail-abi-lifting-26/RESULTS.md'}
    parents.update(q for q in D.rglob('*') if q.is_file() and '__pycache__' not in q.parts)
    fr, pr = pub.remote_probe(F, bases[F]), pub.remote_probe(P, bases[P])
    fh, fs = previous.commit_sync(F, sorted(forks), 'Implement native typed task continuation wait and source-ordered cleanup', fr)
    for name in ['PORT_STATUS.md', 'ROADMAP.md']:
        file = P / name
        text = file.read_text(encoding='utf-8')
        marker = 'Current verified work27 (publication pending)'
        pub.need(text.count(marker) == 1, 'one live current marker')
        file.write_text(text.replace(marker, 'Published typed task-execution checkpoint fork ' + chr(96) + fh + chr(96) + ' (work27)'), encoding='utf-8', newline='\n')
    ph, ps = previous.commit_sync(P, sorted(parents), 'Record verified native task execution and corrected cleanup identity timing', pr)
    for file, digest in (v['sources'] | v['source_evidence'] | v['protected_inputs']).items():
        pub.need(pub.sha(file) == digest, 'measured input stable through publication')
    record = dict(fork_commit=fh, fork=fs, parent_commit=ph, parent=ps, verification_sha256=pub.sha(vp),
        scope='Two fresh native CPU clients/142 source pins; initial virtual10 branch only;39 main integrations and full game acceptance OPEN')
    with (H / 'PUBLICATION.json').open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(record, stream, indent=2)
        stream.write('\n')
    print(json.dumps(record))
