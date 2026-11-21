"""Verify retained reset control and source table selection; no game endpoints."""
import ctypes
import hashlib
import importlib.util
import json
import re
import struct
import sys
from pathlib import Path

H=Path(__file__).resolve().parent
F=H.parents[2]
P=F.parents[2]
C=P/'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
s=importlib.util.spec_from_file_location('reset_section_receipts',F/'research/runs/native-typed-retail-20261121/verify.py')
c=importlib.util.module_from_spec(s);s.loader.exec_module(c)
c.OUT=H/(sys.argv[1] if len(sys.argv)>1 else 'validation-01')


def load(path):
    return json.loads(path.read_text(encoding='utf-8'))


def main():
    ctypes.windll.kernel32.SetErrorMode(3)
    c.OUT.mkdir(exist_ok=False)
    try:
        previous=F/'research/runs/native-retail-main-task-connection-20261121/validation-01/verification.json'
        c.require(c.sha(previous)=='06f980c5b4c1c7693ade638e9050edf519a312d0050bebf2d867c2e1962308b3','exact prior result')
        prior=load(previous)
        c.require(prior['schema']=='native_retail_main_task_connection_verification_v1' and
                  isinstance(prior['sources'],dict) and isinstance(prior['executed_clients'],list),'actual prior schema')
        changed=['cmake/native_game.cmake','runtime/include/mka/retail_initial_task.hpp',
            'runtime/include/mka/retail_initial_task_required.hpp','runtime/src/native/retail_initial_task.cpp',
            'tests/test_retail_initial_task.cpp','runtime/include/mka/retail_section_workspace.hpp',
            'runtime/src/native/retail_section_workspace.cpp']
        added=['tests/test_retail_section_selection.cpp','tests/test_retail_section_workspace.cpp',
            'tests/test_retail_startup_initialization.cpp']
        sources=dict(prior['sources']);changes={};allowed={str(F/n) for n in changed}
        for path,pin in list(sources.items()):
            now=c.sha(path)
            if now!=pin:
                c.require(path in allowed,'unexpected source change: '+path)
                changes[path]=dict(previous_sha256=pin,current_sha256=now);sources[path]=now
        sources.update({str(F/n):c.sha(F/n) for n in changed+added})
        sources[str(Path(__file__))]=c.sha(__file__)
        manifest=C/'native-retail-section-selection-81/author02/IMPLEMENTATION.json'
        c.require(c.sha(manifest)=='d3502ab8128e9359745d2050ea32fd9a2fd9eb7725ad31be0f58026314a814e2','frozen correction manifest')
        author=load(manifest)
        c.require(author['schema']=='native_retail_section_selection_implementation_correction_v2' and
                  isinstance(author['pins'],list),'actual correction producer')
        for row in author['pins']:
            base=F if row['path'].startswith(('runtime/','tests/')) else C/'native-retail-section-selection-81'
            c.require(c.sha(base/row['path'])==row['sha256'],'frozen correction bytes: '+row['path'])
        evidence={str(manifest):c.sha(manifest)}
        for scope in ['native-retail-section-selection-81/source-agent01','native-retail-section-selection-81/review01',
                      'native-retail-section-selection-81/review02',
                      'native-retail-initial-reset-wait-82/source-agent01','native-retail-reset-continuation-83/root01',
                      'native-retail-reset-continuation-83/review01']:
            for path in (C/scope).glob('*.json'):evidence[str(path)]=c.sha(path)
        protected=prior['protected_inputs']
        for group in [sources,evidence,protected]:
            c.require(all(c.sha(path)==pin for path,pin in group.items()),'stable inputs before')
        elf=(P/'research/inputs/SLUS_214.10').read_bytes()
        phoff=struct.unpack_from('<I',elf,28)[0];size,count=struct.unpack_from('<HH',elf,42)
        segments=[struct.unpack_from('<8I',elf,phoff+i*size) for i in range(count)]
        def image(at,length):
            seg=[x for x in segments if x[0]==1 and x[2]<=at and at+length<=x[2]+x[4]]
            c.require(len(seg)==1,'unique original file-backed range')
            x=seg[0];offset=x[1]+at-x[2];return elf[offset:offset+length]
        def word(at):return struct.unpack('<I',image(at,4))[0]
        raw81=C/'native-retail-section-selection-81/source-agent01/RAW_SOURCE.json'
        c.require(c.sha(raw81)=='b007724316b97c3ca08e4cfdb723474e1014cd183112eb355b129b03de75b0ee','exact section authority')
        section=load(raw81);section_words=0
        for rows in [section['function_words']]+[r['words'] for r in section['linked_children_ranges']]:
            addresses=[int(row['va'],16) for row in rows]
            c.require(addresses==list(range(addresses[0],addresses[-1]+4,4)),'full contiguous original section span')
            for row in rows:
                c.require(word(int(row['va'],16))==int(row['word_le'],16),'original section instruction')
                section_words+=1
        c.require(word(int(section['pointer_va'],16))==int(section['pointer_word'],16),'original table3 pointer')
        for row in section['records']:
            at=int(section['table_va'],16)+12*row['ordinal']
            c.require(list(struct.unpack('<3I',image(at,12)))==[row['id'],int(row['subtable_va'],16),row['descriptor']],
                      'original serialized section triple')
            for sub in row['subrecords']:
                c.require(list(struct.unpack('<2I',image(int(sub['source_va'],16),8)))==[sub['id'],sub['packed_size']],
                          'original section subrecord')
        raw82=C/'native-retail-initial-reset-wait-82/source-agent01/RAW_SOURCE.json'
        c.require(c.sha(raw82)=='c519658ca694853a67fb3af15446c5c27832a0a9690d0816d1e4c6222efd5958','exact wait authority')
        wait=load(raw82);wait_words=0
        c.require(isinstance(wait['ranges'],dict),'actual wait raw range map')
        for row in wait['ranges'].values():
            data=image(int(row['address'],16),row['size'])
            c.require(hashlib.sha256(data).hexdigest()==row['sha256'],'full original wait/producer range')
            for value in row['words_le32']:
                c.require(word(int(value['address'],16))==int(value['word'],16),'original wait/producer word')
                wait_words+=1
        c.require(word(0x12ac34)==0x3c033f80 and word(0x12ac38)==0xaf839e68 and
            word(0x59fcd0+0x14)==0x12e410,'actual one-unit wait and retained concrete slot')
        modules=P/'research/inputs/modules';root=c.OUT/'content';root.mkdir()
        clients={r['target']:[] for r in prior['executed_clients']}
        for name in ['test_native_retail_profile_transition_child','test_native_retail_profile_aggregate_main',
                     'test_native_retail_game_records_main','test_native_retail_profile_preferences',
                     'test_native_retail_profile_resource_ids']:clients[name]=[modules]
        for name in ['test_native_retail_initial_task','test_native_retail_task_dispatch_main']:clients[name]=[root]
        clients.update(test_native_retail_section_selection=[],test_native_retail_section_workspace=[],
                       test_native_retail_startup_initialization=[modules])
        c.save('inputs.json',dict(sources=sources,evidence=evidence,protected_inputs=protected,
            explicit_prior_source_changes=changes,targets=list(clients),section_words=section_words,wait_words=wait_words))
        c.execute('build',[c.CMAKE,'--build',c.BUILD,'--target',*clients,'-j2'])
        old={r['target']:r for r in prior['executed_clients']}
        identities=[];fresh=[];reused=[]
        for name,args in clients.items():
            current=c.sha(c.BUILD/(name+'.exe'))
            if name in old and current==old[name]['sha256'] and name!='test_native_retail_initial_task':
                path=previous.parent/old[name]['receipt'];r=load(path)
                c.require(r['exit']==0 and not r['stderr'] and r['executable_sha256']==r['executable_after_sha256']==current,
                          'authenticated unchanged executable and scenario')
                r=dict(r,reused_from=str(path),reused_record_sha256=c.sha(path))
                c.save('run-'+name+'.json',r);reused.append(name)
            else:
                r=c.execute('run-'+name,[c.BUILD/(name+'.exe'),*args]);fresh.append(name)
            c.require(not r['stderr'] and (not r['stdout'] or 'PASS' in r['stdout'] or 'passed' in r['stdout']),
                      'actual component assertions: '+name)
            identities.append(dict(target=name,sha256=current,receipt='run-'+name+'.json',
                execution='fresh' if name in fresh else 'authenticated_previous'))
            if name in reused:
                path=previous.parent/('symbols-'+name+'.json');symbol=load(path)
                c.require(symbol['exit']==0 and c.sha(c.BIN/'llvm-nm.exe')==symbol['executable_sha256']==
                          symbol['executable_after_sha256'],'unchanged symbol tool/target identity')
                symbol=dict(symbol,reused_from=str(path),reused_record_sha256=c.sha(path));c.save('symbols-'+name+'.json',symbol)
            else:symbol=c.execute('symbols-'+name,[c.BIN/'llvm-nm.exe','-C',c.BUILD/(name+'.exe')])
            c.require(not re.search(r'(?<![A-Za-z0-9_])(?:ps2|gs|spu2|sif|Cpu|recomp)::|\b(?:g_ram|g_spr)\b',symbol['stdout']),
                      'no guest/device runtime symbols')
        children=[('begin_reset_18d970' if n=='run_reset_18d970' else n) for n in prior['required_initial_task_children']]
        plans=[('profile-effects','test_native_profile_required_effects',prior['required_profile_effects']),
            ('initial-children','test_native_retail_initial_task_missing_children',children),
            ('entry-effects','test_native_startup_missing_bindings',prior['remaining_integration_symbols']+children)]
        for label,target,expected in plans:
            r=c.execute(label,[c.CMAKE,'--build',c.BUILD,'--target',target,'-j2'],expected=None)
            missing=re.findall(r'undefined symbol: ([^\n]+)',r['stdout']+r['stderr'])
            c.require(len(missing)==len(expected) and all(any('::'+n+'(' in x for x in missing) for n in expected)
                and not (c.BUILD/(target+'.exe')).exists(),'exact separate strong dependency plan: '+target)
        for group in [sources,evidence,protected]:
            c.require(all(c.sha(path)==pin for path,pin in group.items()),'stable inputs after')
        result=dict(schema='native_retail_reset_section_connected_verification_v1',
            status='PASS_RETAINED_RESET_CALLER_AND_SECTION_SELECTION_CPU_CHECKS',sources=sources,evidence=evidence,
            protected_inputs=protected,client_identities=identities,freshly_executed_targets=fresh,reused_unchanged_targets=reused,
            explicit_prior_source_changes=changes,prior_path=str(previous),prior_sha256=c.sha(previous),
            original_word_joins=dict(section=section_words,reset_wait_and_producers=wait_words),
            remaining_integration_symbols=prior['remaining_integration_symbols'],required_initial_task_children=children,
            required_profile_effects=prior['required_profile_effects'],
            startup_archive_sha256=c.sha(c.BUILD/'libmka_native_retail_startup.a'),
            section_scope='source tables0/3;loaded child retirement and other selections remain named unbound',
            complete_game_entry_linkable=False,game_executed=False,physical_endpoints_opened=False,frames=None,audio=None,
            limits=['Parent retained reset contract only; actual18D970 reset and shared344-record producer/countdown still required.',
                    'Section selection preserves empty-child source prefix/storage; populated resource cleanup unported.',
                    'No full boot/input/render/sound/gameplay acceptance.'])
        c.save('verification.json',result)
        print(json.dumps(dict(status=result['status'],fresh=len(fresh),reused=len(reused),total=len(identities),game_executed=False)))
    except Exception as error:
        c.save('failure.json',dict(status='FAILED',type=type(error).__name__,message=str(error)))
        raise


if __name__=='__main__':main()
