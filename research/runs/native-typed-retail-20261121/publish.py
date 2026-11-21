"""Publish the reviewed typed AOT/task slice, preserving unrelated work."""
import importlib.util, json
from pathlib import Path

F = Path(__file__).resolve().parents[3]
P = F.parents[2]
H = Path(__file__).resolve().parent
C = P / 'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
spec = importlib.util.spec_from_file_location('previous', F / 'research/runs/native-connected-main-dependencies-20261121/publish.py')
previous = importlib.util.module_from_spec(spec)
spec.loader.exec_module(previous)
pub = previous.pub

if __name__ == '__main__':
    path = H / 'validation-02/verification.json'
    pub.need(pub.sha(path) == '8d1d48e7c3ac60babea430dd1d8e29944c0c1975fa142974d048671a990269ed', 'exact current result')
    result = json.loads(path.read_text(encoding='utf-8'))
    pub.need(result['schema'] == 'native_typed_retail_cpu_verification_v1' and
        result['status'] == 'PASS_TYPED_AOT_AND_TASK_REGISTRY_CPU_CHECKS' and
        len(result['sources']) == 137 and len(result['client_identities']) == 3 and
        [x['target'] for x in result['new_executions']] == ['test_native_typed_retail'] and
        [x['target'] for x in result['reused_component_executions']] == ['test_native_retail_task_registry', 'test_native_typed_codegen'] and
        len(result['unchanged_prior_binary_identities']) == 38 and len(result['remaining_integration_symbols']) == 39 and
        not result['complete_game_entry_linkable'] and not result['game_executed'] and
        not result['physical_endpoints_opened'] and result['frames'] is None and result['audio'] is None,
        'exact measured component/dated identity scope')
    for file, digest in (result['sources'] | result['protected_inputs']).items():
        pub.need(pub.sha(file) == digest, 'stable measured source ' + file)
    for row in result['client_identities'] + result['unchanged_prior_binary_identities']:
        pub.need(pub.sha(F / 'build_native_services_only_20261119-01' / (row['target'] + '.exe')) == row['sha256'], 'current binary')
    for key, name in [('typed_archive_sha256', 'libmka_native_typed_retail.a'), ('registry_archive_sha256', 'libmka_native_retail_task_registry.a')]:
        pub.need(pub.sha(F / 'build_native_services_only_20261119-01' / name) == result[key], 'current archive')
    typed_path = C / 'native-retail-abi-lifting-26/review-results01/REVIEW.json'
    pub.need(pub.sha(typed_path) == '44bc60a8d322672280313d15772bd5f77678c0821da4fc8094f437a868fcc54f', 'independent typed review')
    typed = json.loads(typed_path.read_text(encoding='utf-8'))
    pub.need(typed['kind'] == 'read_only_saved_cpu_results_review' and typed['status'] == 'PASS_BOUNDED_TYPED_AOT_CPU_REVIEW' and
        typed['fresh_validation02'] == ['test_native_typed_retail'] and typed['reused_validation01'] == ['test_native_retail_task_registry', 'test_native_typed_codegen'], 'typed review actual schema')
    for row in typed['pins']:
        pub.need(pub.sha(P / row['path']) == row['sha256'], 'independent typed pin')
    registry_path = C / 'native-retail-task-registry-24/review-results01/REVIEW.json'
    registry = json.loads(registry_path.read_text(encoding='utf-8'))
    pub.need(registry['schema'] == 'native_retail_task_registry_saved_review_v1' and
        registry['status'] == 'PASS_BOUNDED_CPU_REGISTRY_EVIDENCE' and registry['verification_sha256'] == pub.sha(path) and
        registry['source_pins_checked'] == 137 and registry['findings'] == [], 'actual bounded registry review')
    for file, digest in registry['registry_source_sha256'].items():
        pub.need(pub.sha(F / file) == digest, 'independent registry pin')
    bases = {F: 'a87cc58e8d7cfb139c0bc79fe6b036d47efc0c33', P: 'e09c5f63daa5a3a35fae5602749a275e00ba51ff'}
    for root, head in bases.items():
        pub.need(pub.git(root, 'rev-parse', 'HEAD') == head and pub.git(root, 'branch', '--show-current') == 'main', 'publication base')
    authored = ['CMakeLists.txt', 'README.md', 'cmake/native_typed.cmake', 'recomp/native_typed_ir.hpp',
        'recomp/native_typed_ir.cpp', 'recomp/native_typed_codegen.hpp', 'recomp/native_typed_codegen.cpp',
        'recomp/native_typed_main.cpp', 'runtime/include/mka/retail_task_registry.hpp',
        'runtime/src/native/retail_task_registry.cpp', 'tests/test_retail_task_registry.cpp',
        'tests/test_native_typed_codegen.cpp', 'tests/test_native_typed_retail.cpp']
    forks = {F / n for n in authored} | {F / 'research/runs/native-retail-input-selection-20261121/PUBLICATION.json'}
    forks.update(q for q in H.rglob('*') if q.is_file() and '__pycache__' not in q.parts and q.name != 'PUBLICATION.json')
    parents = {P / n for n in ['AGENTS.md', 'PORT_STATUS.md', 'ROADMAP.md']}
    for dirname in ['native-retail-task-registry-24', 'native-retail-initial-task-25', 'native-retail-abi-lifting-26']:
        parents.update(q for q in (C / dirname).rglob('*') if q.is_file() and '__pycache__' not in q.parts)
    for q in forks | parents:
        relative = str(q.relative_to(F) if q.is_relative_to(F) else q.relative_to(P))
        if 'before' in relative or q.name.startswith('RAW') or q.suffix in ['.bin', '.csv', '.tsv']:
            continue
        raw = q.read_bytes()
        pub.need(b'\r' not in raw and raw.endswith(b'\n') and not raw.endswith(b'\n\n'), 'authored LF/EOF ' + relative)
        pub.need(all(line == line.rstrip(b' \t') for line in raw.splitlines()), 'authored whitespace ' + relative)
    fr, pr = pub.remote_probe(F, bases[F]), pub.remote_probe(P, bases[P])
    fh, fs = previous.commit_sync(F, sorted(forks), 'Compile typed retail AOT against native owners and preserve ordered task lookup', fr)
    for name in ['PORT_STATUS.md', 'ROADMAP.md']:
        file = P / name
        text = file.read_text(encoding='utf-8')
        marker = 'Current verified work24/26 (publication pending)'
        pub.need(text.count(marker) == 1, 'current doc checkpoint marker')
        file.write_text(text.replace(marker, 'Published typed AOT/task checkpoint fork ' + chr(96) + fh + chr(96) + ' (work24/26)'), encoding='utf-8', newline='\n')
    ph, ps = previous.commit_sync(P, sorted(parents), 'Record reviewed typed native generation and task registry checkpoint', pr)
    for file, digest in (result['sources'] | result['protected_inputs']).items():
        pub.need(pub.sha(file) == digest, 'source preserved through publication')
    record = dict(fork_commit=fh, fork=fs, parent_commit=ph, parent=ps,
        verification_sha256=pub.sha(path), scope='Typed getter/setter and ordered task lookup only;39 main integrations and full game acceptance OPEN')
    with (H / 'PUBLICATION.json').open('x', encoding='utf-8', newline='\n') as stream:
        json.dump(record, stream, indent=2)
        stream.write('\n')
    print(json.dumps(record))
