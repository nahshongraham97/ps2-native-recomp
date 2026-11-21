"""Sync remaining authored records without staging preexisting local-only data."""
import importlib.util, json
from pathlib import Path
F = Path(__file__).resolve().parents[3]
P = F.parents[2]
H = Path(__file__).resolve().parent
C = P / 'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
D = C / 'native-retail-initial-task-29'
spec = importlib.util.spec_from_file_location('prior', F / 'research/runs/native-connected-main-dependencies-20261121/publish.py')
prior = importlib.util.module_from_spec(spec)
spec.loader.exec_module(prior)
pub = prior.pub
if __name__ == '__main__':
    bases = {F: 'a9614b46ec304b368e5b9c79c6963c162caba6ce', P: 'd0c182c4d50ae683c4b9e66f264ccb983b22ee94'}
    for root, head in bases.items():
        pub.need(pub.git(root, 'rev-parse', 'HEAD') == head and pub.git(root, 'branch', '--show-current') == 'main', 'exact current base')
    result_path = H / 'validation-01/verification.json'
    pub.need(pub.sha(result_path) == '78bd0c530e2a7689db2dcf05e4488a53498f4b6891f94af9ba19bf5ab65bb0c0', 'verified code checkpoint')
    v = json.loads(result_path.read_text(encoding='utf-8'))
    for file, digest in (v['sources'] | v['source_evidence'] | v['protected_inputs']).items():
        pub.need(pub.sha(file) == digest, 'measured code or protected input changed')
    expected = {'SOURCE.md': '909eb789d819975735c5663096919f3e1d53baac5c7404c3e6466076694a839d',
        'DESIGN.md': '180f73ef18a86aa45dfbb03d7c1b68cb8d61b1d40e4dd0715ff4f806fe5b77f0',
        'DESIGN.json': '0e2c51c9ba5d8c3621d77557696e6852205dcd468948ac9587f3a74d75493137',
        'RAW_PRODUCER.json': '0e40631f6db7ee1b0b13fdf214b062eadd798ddfe7f86c1acbe46850bc15ecf7'}
    for name, digest in expected.items():
        pub.need(pub.sha(D / 'source-agent01' / name) == digest, 'exact source-only follow-up')
    proof = json.loads((D / 'root-verification01/VERIFICATION.json').read_text(encoding='utf-8'))
    pub.need(proof['producer_word_entries_rejoined'] == 50 and proof['mismatches'] == 0 and not proof['game_executed'], 'source-only evidence scope')
    forks = {Path(__file__).resolve(), H / 'PUBLICATION.json'}
    parents = {P / 'PORT_STATUS.md', P / 'ROADMAP.md', C / 'native-retail-initial-task-25/SOURCE.md'}
    parents.update(q for q in D.rglob('*') if q.is_file() and '__pycache__' not in q.parts)
    fr, pr = pub.remote_probe(F, bases[F]), pub.remote_probe(P, bases[P])
    pub.need(fr['available'] and pr['available'], 'both remotes available for requested synchronization')
    fh, fs = prior.commit_sync(F, sorted(forks), 'Save verified native task checkpoint publication receipt', fr)
    pub.need(fs.get('synchronization') == 'verified', 'fork synchronized')
    ph, ps = prior.commit_sync(P, sorted(parents), 'Sync original startup task source plan and corrected menu index ordering', pr)
    pub.need(ps.get('synchronization') == 'verified', 'parent synchronized')
    for root, paths in [(F, forks), (P, parents)]:
        dirty = pub.names(root, 'diff', 'HEAD', '--name-only', '-z')
        pub.need(not ({q.relative_to(root).as_posix() for q in paths} & dirty), 'authored paths clean after sync')
    for file, digest in v['protected_inputs'].items():
        pub.need(pub.sha(file) == digest, 'protected input after sync')
    print(json.dumps(dict(fork_commit=fh, fork=fs, parent_commit=ph, parent=ps,
        source_runtime_checkpoint=bases[F], new_runtime_changes=False,
        scope='All authored changes from this resumed work synchronized; preexisting local-only files untouched')))
