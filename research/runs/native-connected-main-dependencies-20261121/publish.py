"""Publish verified connected main dependencies with exact scoped staging."""
import importlib.util,json,subprocess
from pathlib import Path
F=Path(__file__).resolve().parents[3];P=F.parents[2];H=Path(__file__).resolve().parent
C=P/'research/runs/native-retail-startup-20261120/connected-retail-entry-01';D=C/'native-main-connected-16'
spec=importlib.util.spec_from_file_location('pub',F/'research/runs/native-connected-retail-entry-20261120/publish.py')
pub=importlib.util.module_from_spec(spec);spec.loader.exec_module(pub)
def commit_sync(root,paths,title,remote):
    approved={p.relative_to(root).as_posix() for p in paths}
    pub.need(not pub.names(root,'diff','--cached','--name-only','-z'),'empty initial scoped index')
    # NUL-delimited stdin avoids Windows command-line limits and path quoting.
    payload=b''.join(name.encode('utf-8')+b'\0' for name in sorted(approved))
    subprocess.run(['git','-c','core.autocrlf=false','add','--pathspec-from-file=-','--pathspec-file-nul'],input=payload,cwd=root,check=True)
    staged=pub.names(root,'diff','--cached','--name-only','-z')
    pub.need(staged and staged==(pub.names(root,'diff','HEAD','--name-only','-z') & approved),'exact authored staged paths')
    subprocess.run(['git','-c','core.whitespace=blank-at-eol,blank-at-eof,space-before-tab,cr-at-eol','diff','--cached','--check'],cwd=root,check=True)
    for name in staged:pub.need(pub.git(root,'cat-file','blob',':'+name,raw=True)==(root/name).read_bytes(),'exact staged bytes '+name)
    subprocess.run(['git','commit','-m',title],cwd=root,check=True,stdout=subprocess.DEVNULL)
    head=pub.git(root,'rev-parse','HEAD')
    if not remote['available']:return head,dict(remote,local_commit=head)
    pushed=subprocess.run(['git','push','origin','main'],cwd=root,capture_output=True,text=True)
    if pushed.returncode:return head,dict(available=False,local_commit=head,exit=pushed.returncode,stderr=pushed.stderr,synchronization='pending')
    words=pub.git(root,'ls-remote','origin','refs/heads/main').split()
    pub.need(words and words[0]==head,'verified remote HEAD')
    return head,dict(available=True,local_commit=head,remote_commit=head,synchronization='verified')
if __name__=='__main__':
    vpath=H/'validation-03/verification.json'
    pub.need(pub.sha(vpath)=='9f79ae001de23e72967920d991ba924e2ae54ba455367763d21556f2f40a4000','exact measured result')
    result=json.loads(vpath.read_text(encoding='utf-8'))
    pub.need(result['status']=='PASS_NATIVE_CONNECTED_MAIN_DEPENDENCY_CPU_FILE_CHECKS' and len(result['sources'])==92 and len(result['fixtures'])==28 and len(result['remaining_integration_symbols'])==41 and not result['complete_game_entry_linkable'] and not result['game_executed'] and not result['physical_endpoints_opened'],'actual measured CPU/file scope')
    for path,digest in result['sources'].items():pub.need(pub.sha(path)==digest,'stable measured source '+path)
    rpath=D/'review-alias-main01/EXECUTION_REVIEW.json'
    pub.need(pub.sha(rpath)=='eb599084f9bbe34ce93b701dc69fba2f8c36dbb3314ee1f5191e0fc3e2908fc9','exact independent saved review')
    review=json.loads(rpath.read_text(encoding='utf-8'))
    pub.need(review['schema']=='native-main-connected-16-saved-execution-independent-review-v1' and review['status']=='PASS_SAVED_NATIVE_CONNECTED_MAIN_DEPENDENCY_CPU_FILE_EVIDENCE' and review['source_pins_checked']==92 and review['fixture_count']==28 and review['sources']==result['sources'] and review['findings']==[] and review['remaining_integration_symbols']==result['remaining_integration_symbols'],'actual review schema/pins')
    pub.need(pub.sha(D/'IMPLEMENTATION.json')==review['implementation_manifest_sha256']=='7763e7dbd985e097f9aa3cc91ea2ca4fff73e5834f9e84fcdb8c626f52c62d73','actual R3 source freeze')
    profile=json.loads((D/'review-profile-main01/SOURCE_REVIEW.json').read_text(encoding='utf-8'))
    pub.need(profile['schema']=='native-main-connected16-independent-source-review-v1' and profile['status']=='SOURCE_REVIEW_PASS_BOUNDED_R2_BUILD_PENDING' and profile['frozen_source_pins']==91 and profile['frozen_source_pin_mismatches']==0,'dated profile source review before R3 fixture-only delta')
    for key,name in [('archive_sha256','libmka_native_retail_startup.a'),('alias_archive_sha256','libmka_native_retail_file_paths.a'),('profile_archive_sha256','libmka_native_retail_profile_owners.a'),('game_session_archive_sha256','libmka_native_retail_game_session.a'),('keyboard_archive_sha256','libmka_native_retail_keyboard.a'),('platform_archive_sha256','libmka_native_retail_platform.a')]:
        pub.need(pub.sha(F/'build_native_services_only_20261119-01'/name)==result[key],'exact measured archive '+name)
    for row in result['fixtures']:pub.need(pub.sha(F/'build_native_services_only_20261119-01'/(row['target']+'.exe'))==row['sha256'],'retained client identity')
    for path,digest in review['protected_sha256'].items():pub.need(pub.sha(path)==digest,'protected source')
    bases={F:'a98ac281c933ff7d87535ec39e1a9fa9116154fc',P:'b7a92a30df314a29ffd891a557b15f1ccb9a79ad'}
    for root,expected in bases.items():pub.need(pub.git(root,'rev-parse','HEAD')==expected and pub.git(root,'branch','--show-current')=='main','publication base')
    authored=['README.md','cmake/native_game.cmake','runtime/include/mka/allocation_profile.hpp','runtime/src/native/allocation_profile.cpp','tests/test_allocation_profile.cpp',
        'runtime/include/mka/retail_main.hpp','runtime/src/native/retail_main.cpp','runtime/include/mka/retail_memory_owners.hpp',
        'runtime/include/mka/retail_package.hpp','runtime/src/native/retail_package.cpp','tests/test_retail_package.cpp','tests/test_retail_package_errors.cpp',
        'runtime/include/mka/retail_file_paths.hpp','runtime/src/native/retail_file_paths.cpp','tests/test_retail_file_paths.cpp',
        'runtime/include/mka/retail_profile_owners.hpp','runtime/src/native/retail_profile_owners.cpp','tests/test_retail_profile_owners.cpp']
    forks={F/name for name in authored}|{F/'research/runs/native-retail-main-session-20261121/PUBLICATION.json',F/'research/runs/native-retail-main-session-20261121/resume_parent_publication.py'}
    forks.update(p for p in H.rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.name!='PUBLICATION.json' and
        not {'owned-content','disposable-package-errors','exclusive-startup-content'} & set(p.relative_to(H).parts))
    parents={P/'PORT_STATUS.md',P/'ROADMAP.md'}
    for scope in [C/'native-retail-profile-14',C/'native-retail-middleware-files-15',D,C/'native-main-session-13/next-main-gates-source01']:
        parents.update(p for p in scope.rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.suffix!='.exe')
    for path in forks|parents:
        rel=path.relative_to(F) if path.is_relative_to(F) else path.relative_to(P)
        if path.suffix in ['.bin','.raw','.tsv','.txt','.log','.csv']:continue
        if path.name.startswith(('RAW','ORIGINAL_WORDS')) or path.stem.endswith('_RAW'):continue
        raw=path.read_bytes()
        pub.need(b'\r' not in raw and raw.endswith(b'\n') and not raw.endswith(b'\n\n'),'LF/EOF '+str(path))
        pub.need(all(line==line.rstrip(b' \t') for line in raw.splitlines()),'whitespace '+str(path))
    fr,pr=pub.remote_probe(F,bases[F]),pub.remote_probe(P,bases[P])
    fh,fs=commit_sync(F,sorted(forks),'Connect native source file aliases and profile storage to real main owners',fr)
    marker='Latest measured working-tree connected-main-dependencies-16 (base fork '+chr(96)+bases[F]+chr(96)+')'
    for name in ['PORT_STATUS.md','ROADMAP.md']:
        path=P/name;text=path.read_text(encoding='utf-8');pub.need(text.count(marker)==1,'one current marker')
        path.write_text(text.replace(marker,'Published native connected-dependency checkpoint fork '+chr(96)+fh+chr(96)+' (work16)'),encoding='utf-8',newline='\n')
    ph,ps=commit_sync(P,sorted(parents),'Record verified native main file-path and profile dependency checkpoint',pr)
    for path,digest in result['sources'].items():pub.need(pub.sha(path)==digest,'source through publication')
    for path,digest in review['protected_sha256'].items():pub.need(pub.sha(path)==digest,'protected bytes through publication')
    record=dict(fork_commit=fh,fork=fs,parent_commit=ph,parent=ps,verification_sha256=pub.sha(vpath),scope='28 native CPU/file clients,92 pins,41 unresolved integration names; full game/endpoint acceptance OPEN')
    with (H/'PUBLICATION.json').open('x',encoding='utf-8',newline='\n') as stream:json.dump(record,stream,indent=2);stream.write('\n')
    print(json.dumps(record))
