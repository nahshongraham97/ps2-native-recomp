"""Complete exact parent transaction after preserved-source snapshot whitespace gate."""
import base64,hashlib,importlib.util,json,subprocess
from pathlib import Path
F=Path(__file__).resolve().parents[3];P=F.parents[2];H=Path(__file__).resolve().parent
D=P/'research/runs/native-retail-startup-20261120/connected-retail-entry-01/native-main-session-13'
spec=importlib.util.spec_from_file_location('pub',F/'research/runs/native-connected-retail-entry-20261120/publish.py')
pub=importlib.util.module_from_spec(spec);spec.loader.exec_module(pub)
if __name__=='__main__':
    fork='a98ac281c933ff7d87535ec39e1a9fa9116154fc';parent='8ec545657b6eb777b1b8ecb71e4d850c3bf5d22f'
    pub.need(pub.git(F,'rev-parse','HEAD')==fork and pub.git(P,'rev-parse','HEAD')==parent,'known partial transaction')
    pub.need(pub.git(F,'ls-remote','origin','refs/heads/main').split()[0]==fork,'fork checkpoint already published')
    result=json.loads((H/'validation-03/verification.json').read_text())
    for path,digest in result['sources'].items():pub.need(pub.sha(path)==digest,'measured source unchanged')
    review=json.loads((D/'independent-review01/EXECUTION_REVIEW.json').read_text())
    for path,digest in review['protected_sha256'].items():pub.need(pub.sha(path)==digest,'protected bytes')
    staged=pub.git(P,'diff','--cached','--name-only','-z').strip('\0').split('\0')
    pub.need(len(staged)==85 and len(set(staged))==85,'exact failed parent scope')
    pub.need(all(name in ['PORT_STATUS.md','ROADMAP.md'] or name.startswith('research/runs/native-retail-startup-20261120/connected-retail-entry-01/native-') for name in staged),'no unrelated index content')
    bad=[P/'research/runs/native-retail-startup-20261120/connected-retail-entry-01/native-main-settings-10'/part/'SOURCE.md' for part in ['before-entry-join-clarification','before-reference-count-correction']]
    for path in bad:pub.need(str(path.relative_to(P)).replace('\\','/') in staged,'preserved original snapshot in failed scope')
    R=D/'publication-recovery01';R.mkdir(exist_ok=False)
    pack=dict(schema='exact-historical-source-snapshots-v1',status='ORIGINAL_LOCAL_BYTES_PRESERVED_REVERSIBLE_ARCHIVE',files=[])
    for path in bad:
        raw=path.read_bytes()
        pack['files'].append(dict(path=str(path),sha256=pub.sha(path),bytes_base64=base64.b64encode(raw).decode('ascii')))
    archive=R/'PRESERVED_BEFORE_BYTES.json'
    archive.write_text(json.dumps(pack,indent=2)+'\n',encoding='utf-8',newline='\n')
    for row in pack['files']:pub.need(hashlib.sha256(base64.b64decode(row['bytes_base64'])).hexdigest()==row['sha256'],'reversible exact snapshot')
    record=R/'RECOVERY.json'
    record.write_text(json.dumps(dict(schema='scoped-native-session-parent-publication-recovery-v1',fork_commit=fork,
        failed_parent_base=parent,failed_action='parent cached whitespace check before commit',
        reason='Two immutable historical before-source markdown exports contain redundant EOF lines.',
        resolution='Keep original local bytes unchanged; commit reversible SHA-checked base64 archive in place of those two raw snapshots. No source checker or cached whitespace rule weakened.',
        original_staged_paths=staged,excluded_raw_paths=[str(path.relative_to(P)).replace('\\','/') for path in bad],
        archive_sha256=pub.sha(archive),production_source_changed=False),indent=2)+'\n',encoding='utf-8',newline='\n')
    approved={P/name for name in staged}-set(bad)|{archive,record}
    # Only clear our previously-empty transaction's exact staged paths, never working files.
    pub.git(P,'restore','--staged','--',*staged)
    pr=pub.remote_probe(P,parent)
    ph,ps=pub.commit_sync(P,sorted(approved),'Record verified native session and preserved original recomp oracle evidence',pr)
    for path,digest in result['sources'].items():pub.need(pub.sha(path)==digest,'source after publication')
    for path,digest in review['protected_sha256'].items():pub.need(pub.sha(path)==digest,'protected after publication')
    pub.need(pub.git(F,'ls-remote','origin','refs/heads/main').split()[0]==fork,'fork remote checkpoint retained')
    receipt=dict(fork_commit=fork,fork='synced',parent_commit=ph,parent=ps,
        verification_sha256=pub.sha(H/'validation-03/verification.json'),
        scope='22 native CPU/file clients,74 pins,41 unresolved native integrations; full game acceptance OPEN',
        recovery=str(record),recovery_sha256=pub.sha(record))
    with (H/'PUBLICATION.json').open('x',encoding='utf-8',newline='\n') as out:json.dump(receipt,out,indent=2);out.write('\n')
    print(json.dumps(receipt))
