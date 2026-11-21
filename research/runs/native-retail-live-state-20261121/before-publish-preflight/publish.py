"""Publish measured native role/options/Main-lookup changes and source records."""
import importlib.util
import json
import re
from pathlib import Path
F=Path(__file__).resolve().parents[3];P=F.parents[2];H=Path(__file__).resolve().parent
C=P/'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
s=importlib.util.spec_from_file_location('publisher',F/'research/runs/native-connected-main-dependencies-20261121/publish.py')
m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
if __name__=='__main__':
    path=H/'validation-02/verification.json'
    m.pub.need(m.pub.sha(path)=='3695233a08b22b48fee4d7cb1f670d6fd2d72fdc11ccb4d19de7e9501ed1ac7b','exact completed live-state result')
    v=json.loads(path.read_text(encoding='utf-8'))
    targets=['test_native_retail_game_options','test_native_retail_input_roles','test_native_retail_game_records_main',
             'test_native_retail_game_session','test_native_retail_initial_task']
    m.pub.need(v['status']=='PASS_NATIVE_LIVE_OPTIONS_ROLE_AND_MAIN_LOOKUP' and
               [r['target'] for r in v['executed_clients']]==targets and
               len(v['remaining_integration_symbols'])==29 and not v['complete_game_entry_linkable'] and
               not v['game_executed'] and not v['physical_endpoints_opened'],'exact measured scope')
    for group in (v['sources'],v['evidence'],v['protected_inputs']):
        for name,pin in group.items():m.pub.need(m.pub.sha(name)==pin,'stable measured bytes '+name)
    build=F/'build_native_services_only_20261119-01'
    m.pub.need(m.pub.sha(build/'libmka_native_retail_startup.a')==v['startup_archive_sha256'],'measured actual startup archive')
    for row in v['executed_clients']:
        r=json.loads((H/'validation-02'/(row['target']+'.json')).read_text(encoding='utf-8'))
        m.pub.need(r['exit']==0 and 'PASS' in r['stdout'] and not r['stderr'] and
                   r['executable_sha256']==r['executable_after_sha256']==row['sha256']==
                   m.pub.sha(build/(row['target']+'.exe')),'successful current client '+row['target'])
    r=json.loads((H/'validation-02/actual-entry-link.json').read_text(encoding='utf-8'))
    symbols=re.findall(r'undefined symbol: ([^\n]+)',r['stdout']+r['stderr'])
    m.pub.need(r['exit']!=0 and len(symbols)==29 and all(n.startswith('mka::retail::main_binding::') for n in symbols),
               'actual full-entry remaining Main integrations only')
    code=['runtime/include/mka/retail_game_options.hpp','runtime/src/native/retail_game_options.cpp',
          'runtime/src/native/retail_game_options_main.cpp','tests/test_retail_game_options.cpp',
          'runtime/include/mka/retail_game_session.hpp','runtime/src/native/retail_game_session.cpp',
          'runtime/include/mka/retail_input_roles.hpp','runtime/src/native/retail_input_roles.cpp',
          'tests/test_retail_input_roles.cpp','runtime/include/mka/retail_main.hpp','runtime/src/native/retail_main.cpp',
          'runtime/src/native/retail_task_lookup_main.cpp','tests/test_retail_game_records_main.cpp','cmake/native_game.cmake']
    forks=[F/n for n in code]+[H/'check.py',H/'publish.py',H/'check.py.before-snapshot-path-correction',H/'CHECKER_CORRECTION.json']
    forks+=sorted((H/'validation-01').glob('*.json'))+sorted((H/'validation-02').glob('*.json'))
    folders=['native-retail-audio-startup-48','native-retail-input-actions-49','native-retail-pad-startup-51',
             'native-retail-game-options-52','native-retail-task-lookup-main-53','native-retail-task-arguments-54']
    parents=[P/'PORT_STATUS.md',P/'ROADMAP.md']
    for folder in folders:parents+=sorted(p for p in (C/folder).rglob('*') if p.is_file())
    for path in forks+parents:
        data=path.read_bytes();data.decode('utf-8')
        preserved=any(part.startswith('before') for part in path.parts)
        m.pub.need(preserved or (b'\r' not in data and data.endswith(b'\n')),'normalized active text; preserved snapshots unchanged '+str(path))
        m.pub.need(path.suffix.lower() in {'.md','.json','.csv','.cpp','.hpp','.py','.before-snapshot-path-correction'},'only authored text/evidence paths')
    bases={F:'67ec1754596c6ec8d701f20a4a600745950a9adf',P:'6288d45db462c43400b599f2ba52cb9b6be3b1bd'}
    remote={}
    for root,base in bases.items():
        m.pub.need(m.pub.git(root,'rev-parse','HEAD')==base and m.pub.git(root,'branch','--show-current')=='main','expected authored base')
        remote[root]=m.pub.remote_probe(root,base)
    result=[]
    for root,paths,title in [(F,forks,'Connect native Main task lookup and live game options; correct linked-role reset'),
                             (P,parents,'Record verified native live-state integration and exact next input dependencies')]:
        head,sync=m.commit_sync(root,paths,title,remote[root]);result.append(dict(repository=str(root),commit=head,synchronization=sync))
    with (H/'PUBLICATION.json').open('x',encoding='utf-8',newline='\n') as stream:json.dump(result,stream,indent=2);stream.write('\n')
    print(json.dumps(result))
