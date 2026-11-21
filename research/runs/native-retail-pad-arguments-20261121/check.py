"""Bounded native task-argument/reset/pad binding checks; no endpoints."""
import importlib.util
import json
import re
import struct
import subprocess
from pathlib import Path
F=Path(__file__).resolve().parents[3];P=F.parents[2];H=Path(__file__).resolve().parent
C=P/'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
D=C/'native-retail-pad-arguments-55'
s=importlib.util.spec_from_file_location('checks',F/'research/runs/native-typed-retail-20261121/verify.py')
c=importlib.util.module_from_spec(s);s.loader.exec_module(c);c.OUT=H/'validation-02'
if __name__=='__main__':
    c.ctypes.windll.kernel32.SetErrorMode(3);c.OUT.mkdir(exist_ok=False)
    try:
        prior_path=F/'research/runs/native-retail-live-state-20261121/validation-02/verification.json'
        c.require(c.sha(prior_path)=='3695233a08b22b48fee4d7cb1f670d6fd2d72fdc11ccb4d19de7e9501ed1ac7b','exact previous connected result')
        prior=json.loads(prior_path.read_text(encoding='utf-8'))
        authors=[(D/'observer-author01/IMPLEMENTATION.json','620ae6f676bcbf3ffc5925470c5d1ceaa4fa68643f6d9ae7b19e80f371c537f5','after'),
                 (D/'argument-author01/IMPLEMENTATION.json','757a1115861e48c82b37a69c450c08236401daf1d9bb0d5a0512f2e211453363','files_sha256')]
        evidence={str(prior_path):c.sha(prior_path)};sources={}
        manifest=[]
        for path,pin,key in authors:
            c.require(c.sha(path)==pin,'frozen author handoff '+str(path))
            value=json.loads(path.read_text(encoding='utf-8'))
            c.require(isinstance(value[key],dict),'exact author pin-map schema')
            evidence[str(path)]=pin;sources.update({str(F/name):digest for name,digest in value[key].items()});manifest.append(value)
        paths=['runtime/include/mka/retail_task_registry.hpp','runtime/src/native/retail_task_registry.cpp',
               'runtime/include/mka/retail_pad_control.hpp','runtime/src/native/retail_pad_control.cpp',
               'runtime/src/native/retail_pad_startup.cpp','runtime/include/mka/retail_platform.hpp',
               'runtime/src/native/retail_platform.cpp','runtime/src/native/retail_game_record_reset.cpp',
               'runtime/src/native/retail_input_roles.cpp','runtime/src/native/retail_task_domain.cpp',
               'runtime/src/native/retail_game_controls.cpp','runtime/src/native/retail_game_records_main.cpp',
               'tests/test_retail_pad_control.cpp','tests/test_retail_task_registry.cpp',
               'tests/test_retail_game_records_main.cpp','cmake/native_game.cmake']
        sources.update({str(F/n):c.sha(F/n) for n in paths});sources[str(Path(__file__).resolve())]=c.sha(__file__)
        for path in sources:
            data=Path(path).read_bytes();c.require(b'\r' not in data and data.endswith(b'\n'),'normalized current compiler input '+path)
        whitespace=subprocess.run(['git','-c','core.autocrlf=false','diff','--check'],cwd=F,capture_output=True,text=True)
        c.save('source-whitespace.json',dict(exit=whitespace.returncode,stdout=whitespace.stdout,stderr=whitespace.stderr))
        c.require(whitespace.returncode==0,'scoped compiler inputs pass whitespace preflight')
        elf=P/'research/inputs/SLUS_214.10';blob=elf.read_bytes()
        c.require(c.sha(elf)==manifest[0]['original_elf_sha256']=='a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f','original ELF input')
        off=struct.unpack_from('<I',blob,28)[0];size,count=struct.unpack_from('<HH',blob,42)
        segments=[struct.unpack_from('<8I',blob,off+i*size) for i in range(count)]
        def original(va):
            rows=[r for r in segments if r[0]==1 and r[2]<=va and va+4<=r[2]+r[4]]
            c.require(len(rows)==1,'one original file-backed instruction range')
            offset=rows[0][1]+va-rows[0][2];return blob[offset:offset+4]
        raw_inputs=[(D/'observer-author01/RAW_SOURCE.json',manifest[0]['raw_sha256'],'vaddr'),
                    (C/'native-retail-task-arguments-54/source-agent01/RAW_SOURCE.json',manifest[0]['prior_source54_raw_sha256'],'address')]
        joined={}
        for path,pin,address_key in raw_inputs:
            c.require(c.sha(path)==pin,'frozen original argument/observer words');evidence[str(path)]=pin
            raw=json.loads(path.read_text(encoding='utf-8'));counts={}
            c.require(isinstance(raw['ranges'],dict),'actual raw word map schema')
            for name,region in raw['ranges'].items():
                va=int(region[address_key],16)
                c.require(all(struct.unpack('<I',original(va+4*i))[0]==int(word,16) for i,word in enumerate(region['words'])),
                          'original PT_LOAD word join '+name)
                counts[name]=len(region['words'])
            joined[str(path)]=counts
        proof=D/'argument-author01/SOURCE_JOIN.json'
        c.require(c.sha(proof)==manifest[1]['source_join_sha256'],'frozen argument birth/cookie/lifetime join')
        evidence[str(proof)]=c.sha(proof)
        live=D/'root01/LIVE_FLAG_READS.json';evidence[str(live)]=c.sha(live)
        listing=json.loads(live.read_text(encoding='utf-8'))
        c.require(listing['language']=='r5900:LE:32:default' and listing['original_elf_sha256']==c.sha(elf),'same EE program')
        for function in listing['functions']:
            c.require(not function['has_more'] and len(function['items'])==function['total'],'complete small pad-flag listing')
            for row in function['items']:
                c.require(original(int(row['address'],16))==bytes.fromhex(row['bytes']),'actual live-flag reload listing bytes')
        def stable():
            for group in (sources,evidence,prior['protected_inputs']):
                c.require(all(c.sha(path)==pin for path,pin in group.items()),'stable source/evidence/protected inputs')
        before_path=H/'validation-01/source-preflight.json'
        before=json.loads(before_path.read_text(encoding='utf-8'))
        permitted={str(F/'tests/test_retail_pad_control.cpp'),str(Path(__file__).resolve())}
        c.require(all(path in permitted or c.sha(path)==pin for path,pin in before['sources'].items()),
                  'only fixture and check runner changed after first compiled result')
        evidence[str(before_path)]=c.sha(before_path)
        stable();c.save('source-preflight.json',dict(sources=sources,evidence=evidence,original_word_joins=joined))
        targets=['test_native_retail_observer_list','test_native_retail_input_task_argument',
                 'test_native_retail_pad_control','test_native_retail_task_registry','test_native_retail_game_records_main']
        c.execute('build',[c.CMAKE,'--build',c.BUILD,'--target',*targets,'-j2'])
        clients=[]
        for name in targets:
            if name in targets[:2]:
                previous=H/'validation-01'/(name+'.json')
                r=json.loads(previous.read_text(encoding='utf-8'))
                c.require(r['exit']==0 and 'PASS' in r['stdout'] and not r['stderr'] and
                          r['executable_sha256']==r['executable_after_sha256']==c.sha(c.BUILD/(name+'.exe')),
                          'retained exact unchanged observer/argument execution '+name)
                r['retained_from']=str(previous);r['retained_receipt_sha256']=c.sha(previous)
                c.save(name+'.json',r)
                clients.append(dict(target=name,sha256=r['executable_sha256'],execution='retained',receipt=name+'.json'))
                continue
            args=[P/'research/inputs/modules'] if name=='test_native_retail_game_records_main' else []
            r=c.execute('run-'+name,[c.BUILD/(name+'.exe'),*args])
            c.require(not r.get('reused_from') and 'PASS' in r['stdout'] and not r['stderr'],'fresh affected client '+name)
            clients.append(dict(target=name,sha256=r['executable_sha256'],execution='fresh',receipt='run-'+name+'.json'))
        r=c.execute('actual-entry-link',[c.CMAKE,'--build',c.BUILD,'--target','test_native_startup_missing_bindings','-j2'],expected=None)
        text=r['stdout']+r['stderr'];all_symbols=re.findall(r'undefined symbol: ([^\n]+)',text)
        missing=sorted(set(re.findall(r'undefined symbol: mka::retail::main_binding::(\w+)\(',text)))
        c.require(missing==sorted(set(prior['remaining_integration_symbols'])-{'initialize_retail_pad_service'}) and
                  len(all_symbols)==len(missing) and all(s.startswith('mka::retail::main_binding::') for s in all_symbols) and
                  not (c.BUILD/'test_native_startup_missing_bindings.exe').exists(),'actual entry resolves pad service without invented other integrations')
        stable()
        c.save('verification.json',dict(status='PASS_NATIVE_PAD_ARGUMENT_RESET_CONNECTED_CHECKS',sources=sources,evidence=evidence,
            protected_inputs=prior['protected_inputs'],current_clients=clients,
            executed_clients=[r for r in clients if r['execution']=='fresh'],
            retained_clients=[r for r in clients if r['execution']=='retained'],original_word_joins=joined,
            remaining_integration_symbols=missing,startup_archive_sha256=c.sha(c.BUILD/'libmka_native_retail_startup.a'),
            complete_game_entry_linkable=False,game_executed=False,physical_endpoints_opened=False,frames=None,audio=None,
            limits=['Pure CPU native observer/argument/reset behavior; fixture task bodies are not retail input bodies.',
                    'Actual Main pad service is compiled; successful physical platform branch unexecuted.',
                    'Real2065/2066/208B body/producers, general virtual getters and full game input remain open.',
                    'No main menu, combat, rendering, actual sound or whole-game acceptance.']))
        print(json.dumps(dict(status='PASS_NATIVE_PAD_ARGUMENT_RESET_CONNECTED_CHECKS',fresh_clients=3,retained_clients=2,remaining_Main_integrations=len(missing))))
    except Exception as error:
        c.save('failure.json',dict(status='FAILED',error=type(error).__name__,message=str(error)));raise
