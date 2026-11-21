"""Publish only measured native argument/reset/pad integration."""
import importlib.util
import json
import re
import struct
from pathlib import Path
F=Path(__file__).resolve().parents[3];P=F.parents[2];H=Path(__file__).resolve().parent
D=P/'research/runs/native-retail-startup-20261120/connected-retail-entry-01/native-retail-pad-arguments-55'
s=importlib.util.spec_from_file_location('publisher',F/'research/runs/native-connected-main-dependencies-20261121/publish.py')
m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
if __name__=='__main__':
    path=H/'validation-02/verification.json'
    m.pub.need(m.pub.sha(path)=='edfc948d382b56e95c0a6dbe9382423e5354f63ffc40b59debc080fec533397c','exact completed pad result')
    v=json.loads(path.read_text(encoding='utf-8'))
    targets=['test_native_retail_observer_list','test_native_retail_input_task_argument',
             'test_native_retail_pad_control','test_native_retail_task_registry','test_native_retail_game_records_main']
    m.pub.need(v['status']=='PASS_NATIVE_PAD_ARGUMENT_RESET_CONNECTED_CHECKS' and
               [r['target'] for r in v['current_clients']]==targets and len(v['executed_clients'])==3 and
               len(v['retained_clients'])==2 and len(v['remaining_integration_symbols'])==28 and
               not v['complete_game_entry_linkable'] and not v['game_executed'] and not v['physical_endpoints_opened'],
               'actual measured scope: three fresh, two retained, no game/endpoint')
    for group in (v['sources'],v['evidence'],v['protected_inputs']):
        for name,pin in group.items():m.pub.need(m.pub.sha(name)==pin,'stable measured source '+name)
    build=F/'build_native_services_only_20261119-01'
    m.pub.need(m.pub.sha(build/'libmka_native_retail_startup.a')==v['startup_archive_sha256'],'current compiled startup')
    for row in v['current_clients']:
        r=json.loads((H/'validation-02'/row['receipt']).read_text(encoding='utf-8'))
        m.pub.need(r['exit']==0 and 'PASS' in r['stdout'] and not r['stderr'] and
                   r['executable_sha256']==r['executable_after_sha256']==row['sha256']==
                   m.pub.sha(build/(row['target']+'.exe')),'actual current client '+row['target'])
        if row['execution']=='retained':
            m.pub.need(m.pub.sha(r['retained_from'])==r['retained_receipt_sha256'],'exact retained execution')
    r=json.loads((H/'validation-02/actual-entry-link.json').read_text(encoding='utf-8'))
    symbols=re.findall(r'undefined symbol: ([^\n]+)',r['stdout']+r['stderr'])
    m.pub.need(r['exit']!=0 and len(symbols)==28 and all(n.startswith('mka::retail::main_binding::') for n in symbols),
               'only actual remaining Main integrations')
    proof=D/'root01/COOKIE_POLICY_RECHECK.json'
    data=json.loads(proof.read_text(encoding='utf-8'))
    elf=P/'research/inputs/SLUS_214.10';blob=elf.read_bytes()
    m.pub.need(m.pub.sha(elf)==data['original_elf_sha256'],'original cookie-policy input')
    off=struct.unpack_from('<I',blob,28)[0];size,count=struct.unpack_from('<HH',blob,42)
    loads=[struct.unpack_from('<8I',blob,off+i*size) for i in range(count)]
    for function in data['functions']:
        m.pub.need(not function['has_more'] and len(function['items'])==function['total'],'complete cookie-policy listing')
        for row in function['items']:
            va=int(row['address'],16);parts=[s for s in loads if s[0]==1 and s[2]<=va and va+4<=s[2]+s[4]]
            m.pub.need(len(parts)==1,'original cookie-policy segment')
            s=parts[0];at=s[1]+va-s[2]
            m.pub.need(blob[at:at+4]==bytes.fromhex(row['bytes']),'original policy word '+row['address'])
    m.pub.need(m.pub.sha(D/'root-review01/REVIEW_CORRECTION.json')==
               'd5fca3f8608fcc891530ec5274c94ec617be32ea42dc576e01b93c745f080b1a','explicit withdrawn-review correction')
    code=['runtime/include/mka/retail_observer_list.hpp','runtime/src/native/retail_observer_list.cpp',
          'tests/test_retail_observer_list.cpp','runtime/include/mka/retail_input_task_argument.hpp',
          'runtime/src/native/retail_input_task_argument.cpp','tests/test_retail_input_task_argument.cpp',
          'runtime/include/mka/retail_task_registry.hpp','runtime/src/native/retail_task_registry.cpp',
          'runtime/include/mka/retail_platform.hpp','runtime/src/native/retail_platform.cpp',
          'runtime/src/native/retail_game_record_reset.cpp','runtime/include/mka/retail_pad_control.hpp',
          'runtime/src/native/retail_pad_control.cpp','runtime/src/native/retail_pad_startup.cpp',
          'tests/test_retail_pad_control.cpp','cmake/native_game.cmake']
    forks=[F/n for n in code]+[H/'check.py',H/'publish.py']
    for directory in ['validation-01','validation-02']:forks+=sorted((H/directory).glob('*.json'))
    parents=[P/'PORT_STATUS.md',P/'ROADMAP.md']+sorted(path for path in D.rglob('*') if path.is_file())
    allowed={'.md','.json','.csv','.cpp','.hpp','.py','.cmake'}
    for path in forks+parents:
        m.pub.need(path.suffix.lower() in allowed,'only authored source/evidence '+str(path))
        raw=path.read_bytes();raw.decode('utf-8')
        preserved=any(part.startswith('before') for part in path.parts)
        # Frozen source/author/review receipts are measured inputs, not text rewrites.
        if path.suffix.lower()=='.json' and (str(path) in v['evidence'] or path==D/'root-review01/REVIEW_CORRECTION.json'):
            preserved=True
        m.pub.need(preserved or (b'\r' not in raw and raw.endswith(b'\n')),'normalized current text '+str(path))
    bases={F:'342d92e04639d85922c341bdab537a605c884d69',P:'02416c9d30bbdca4fa23ee3973df28e322439c86'};remote={}
    for root,base in bases.items():
        m.pub.need(m.pub.git(root,'rev-parse','HEAD')==base and m.pub.git(root,'branch','--show-current')=='main','expected local source base')
        remote[root]=m.pub.remote_probe(root,base)
    results=[]
    for root,paths,title in [(F,forks,'Connect native task arguments and source-ordered pad startup resets'),
                             (P,parents,'Record verified native pad argument integration and remaining retail startup work')]:
        head,sync=m.commit_sync(root,paths,title,remote[root]);results.append(dict(repository=str(root),commit=head,synchronization=sync))
    with (H/'PUBLICATION.json').open('x',encoding='utf-8',newline='\n') as stream:json.dump(results,stream,indent=2);stream.write('\n')
    print(json.dumps(results))
