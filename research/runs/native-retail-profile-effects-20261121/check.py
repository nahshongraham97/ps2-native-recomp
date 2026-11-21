"""Verify connected retail profile effects with CPU/file clients only."""
import ctypes
import hashlib
import importlib.util
import json
import re
import struct
import sys
from pathlib import Path

H = Path(__file__).resolve().parent
F = H.parents[2]
P = F.parents[2]
C = P/'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
D = C/'native-retail-profile-transition-child-72/root01'
s = importlib.util.spec_from_file_location('profile_effect_cpu_receipts',
    F/'research/runs/native-typed-retail-20261121/verify.py')
c = importlib.util.module_from_spec(s)
s.loader.exec_module(c)
c.OUT = H/(sys.argv[1] if len(sys.argv)>1 else 'validation-01')


def load(path):
    return json.loads(path.read_text(encoding='utf-8'))


def main():
    ctypes.windll.kernel32.SetErrorMode(3)
    c.OUT.mkdir(exist_ok=False)
    try:
        previous = F/'research/runs/native-retail-live-scene-20261121/validation-02/verification.json'
        c.require(c.sha(previous) == 'b539548a5b8ad757983da032a545039df35716578da55c1e2a5edf2e464d9a7d',
            'exact previous connected result')
        prior = load(previous)
        c.require(prior['schema'] == 'native_live_scene_profile_transition_connected_verification_v1',
            'actual previous result producer')
        manifest = D/'IMPLEMENTATION.json'
        c.require(c.sha(manifest) == '1a213d9b639f9cbcb6e787360c39f50535cff64be10a7ddd8f7f70148d0d7ab5',
            'exact source freeze')
        author = load(manifest)
        c.require(author['schema'] == 'native_retail_profile_transition_child_72_integration_v1',
            'actual author schema')
        sources = dict(author['sources'])
        changes = {}
        for path,pin in prior['sources'].items():
            now = c.sha(path)
            if now != pin:
                c.require(path in sources and sources[path] == now,'explicit prior source change: '+path)
                changes[path] = dict(previous_sha256=pin,current_sha256=now)
            else:
                sources.setdefault(path,pin)
        sources[str(Path(__file__))] = c.sha(__file__)
        evidence = {str(manifest):c.sha(manifest)}
        for scope in ['native-retail-profile-transition-child-72/source-agent01',
            'native-retail-profile-transition-child-72/source-agent02',
            'native-retail-profile-entry-decision-73/source-agent01',
            'native-retail-profile-entry-decision-73/author01']:
            for path in (C/scope).glob('*'):
                if path.is_file(): evidence[str(path)] = c.sha(path)
        for path in D.glob('GHIDRA_*.json'):
            evidence[str(path)] = c.sha(path)
        for name in ['SOURCE_DATA.json','IMPLEMENTATION.md']:
            evidence[str(D/name)] = c.sha(D/name)
        decision = load(C/'native-retail-profile-entry-decision-73/author01/IMPLEMENTATION.json')
        c.require(decision['schema'] == 'native_retail_profile_entry_decision_73_implementation_v1',
            'actual decision author schema')
        c.require(all(sources[str(F/name)] == pin for name,pin in decision['files_sha256'].items()),
            'unchanged source-reader handoff bytes')
        protected = prior['protected_inputs']
        for group in [sources,evidence,protected]:
            c.require(all(c.sha(path) == pin for path,pin in group.items()),'stable source/evidence/protected before')
        elf = (P/'research/inputs/SLUS_214.10').read_bytes()
        phoff = struct.unpack_from('<I',elf,28)[0]
        size,count = struct.unpack_from('<HH',elf,42)
        segments = [struct.unpack_from('<8I',elf,phoff+i*size) for i in range(count)]

        def image(address,length):
            found = [x for x in segments if x[0] == 1 and x[2] <= address and address+length <= x[2]+x[5]]
            c.require(len(found) == 1,'unique original PT_LOAD file/memory range')
            x = found[0]
            backed = max(0,min(length,x[2]+x[4]-address))
            at = x[1]+address-x[2]
            return elf[at:at+backed]+bytes(length-backed)

        def word(address):
            return struct.unpack('<I',image(address,4))[0]

        joins = {}
        for name,pin in [
            ('native-retail-profile-transition-child-72/source-agent01/RAW_SOURCE.json',
             'b0052879d08bb486e5120a4decc700a62676a45d82e25e32d6d232b29d6cc062'),
            ('native-retail-profile-transition-child-72/source-agent02/RAW_SOURCE.json',
             '477fd9fcf9d6f0eb5980034b69b04366dadaa591018e20c6aa0465e543a861b6'),
            ('native-retail-profile-entry-decision-73/source-agent01/RAW_SOURCE.json',
             'fd5803ca98773a91a74d91adac172ab4ba15d8f1bb4661febea8df84ee740e19')]:
            path = C/name
            c.require(c.sha(path) == pin,'exact full-body source input')
            joined = 0
            for row in load(path)['ranges']:
                address = int(row.get('address',row.get('vma')),16)
                data = image(address,row['length'])
                c.require(hashlib.sha256(data).hexdigest() == row['sha256'],'full original range '+hex(address))
                if 'bytes_hex' in row: c.require(data.hex() == row['bytes_hex'],'full original bytes')
                for i,value in enumerate(row['words_le32']):
                    at = address+i*4
                    expected = value
                    if isinstance(value,dict):
                        at = int(value['address'],16)
                        expected = value['word']
                    c.require(word(at) == int(expected,16),'original word '+hex(at))
                    joined += 1
            joins[name] = joined
        program = load(D/'GHIDRA_PROGRAM.json')
        c.require(program['sha256'] == protected[str(P/'research/inputs/SLUS_214.10')] and
            program['language_id'] == 'r5900:LE:32:default','actual existing EE target')
        listing = load(D/'GHIDRA_0018B0D0.json')
        c.require(not listing['has_more'] and listing['total'] == len(listing['items']) == 99,
            'actual complete function-body listing')
        for item in listing['items']:
            c.require(image(int(item['address'],16),4) == bytes.fromhex(item['bytes']),
                'actual EE instruction bytes')
        listed = {int(item['address'],16) for item in listing['items']}
        gaps = set(range(0x18b0d0,0x18b268,4))-listed
        c.require(gaps == {0x18b130,0x18b144,0x18b1d0} and
            [word(x) for x in sorted(gaps)] == [0x0000302d,0x24d1003c,0x8cc2003c],
            'all omitted real words explicitly decoded')
        for branch,target in [(0x18b128,0x18b134),(0x18b13c,0x18b148),(0x18b1c8,0x18b1d4)]:
            w = word(branch)
            offset = struct.unpack('<h',struct.pack('<H',w&0xffff))[0]
            c.require(w>>16 == 0x1000 and branch+4+offset*4 == target,
                'ordinary-entry unconditional branch bypasses omitted word')
        c.require(word(0x18b210) == 0xae330000 and word(0x18b244) == 0xae330000,
            'both scene stores retained at their actual call-order positions')
        keys = [word(0x5a2bc0+i*16) for i in range(64)]
        c.require(keys == load(D/'SOURCE_DATA.json')['key_table_first_le32'] and keys[30] == keys[39] == 62,
            'all64 source table records and duplicated key62')
        c.save('inputs.json',dict(sources=sources,evidence=evidence,protected_inputs=protected,
            prior_path=str(previous),prior_sha256=c.sha(previous),explicit_prior_source_changes=changes,
            original_word_joins=joins,listed_18b0d0_words=99,full_18b0d0_words=102,
            explicitly_decoded_listing_gaps=sorted(gaps),original_table_keys=keys))
        modules = P/'research/inputs/modules'
        clients = {
            'test_native_retail_profile_transition_child':[modules],
            'test_native_retail_profile_entry_decision':[],
            'test_native_retail_profile_transition':[],
            'test_native_retail_profile_menu':[],
            'test_native_retail_profile_aggregate_main':[modules],
            'test_native_retail_live_scene_reference':[],
            'test_native_retail_profile_scene_object':[],
            'test_native_retail_game_session':[],
            'test_native_retail_game_records_main':[modules],
            'test_native_retail_profile_preferences':[modules],
            'test_native_retail_profile_resource_ids':[modules],
            'test_native_retail_save_startup':[]}
        c.execute('build',[c.CMAKE,'--build',c.BUILD,'--target',*clients,'-j2'])
        executions = []
        for target,args in clients.items():
            r = c.execute('run-'+target,[c.BUILD/(target+'.exe'),*args])
            c.require(not r['stderr'],'no fixture failure output: '+target)
            if target == 'test_native_retail_profile_transition_child':
                lines = r['stdout'].splitlines()
                c.require(len(lines) == 2 and lines[0].split()[0] == 'KEYS' and
                    list(map(int,lines[0].split()[1:])) == keys and lines[1].startswith('PASS'),
                    'compiled native table lookup matches every independent original row')
            elif target == 'test_native_retail_profile_entry_decision':
                c.require(r['stdout'] == 'native retail profile entry decision source fixture passed\n',
                    'actual decision fixture assertions')
            elif target in ['test_native_retail_live_scene_reference','test_native_retail_profile_resource_ids']:
                c.require(not r['stdout'],'quiet fixture success')
            else:
                c.require(r['stdout'].startswith('PASS'),'actual fixture assertions: '+target)
            executions.append(dict(target=target,sha256=r['executable_sha256'],receipt='run-'+target+'.json'))
        symbol_clients = list(clients)[:7]
        imports = {}
        for target in symbol_clients:
            r = c.execute('symbols-'+target,[c.BIN/'llvm-nm.exe','-C',c.BUILD/(target+'.exe')])
            c.require(not re.search(r'(?<![A-Za-z0-9_])(?:ps2|gs|spu2|sif|Cpu|recomp)::|\b(?:g_ram|g_spr)\b',
                r['stdout']),'no guest/device/compiler symbols: '+target)
            imports[target] = sorted(set(re.findall(r'\bSDL_[A-Za-z0-9_]+',r['stdout'])))
            if target in ['test_native_retail_live_scene_reference','test_native_retail_profile_scene_object']:
                c.require(not imports[target],'pure Session clients have no SDL linkage')
        required = set(author['required_native_children'])
        r = c.execute('required-native-effects-link',[c.CMAKE,'--build',c.BUILD,
            '--target','test_native_profile_required_effects','-j2'],expected=None)
        missing = re.findall(r'undefined symbol: ([^\n]+)',r['stdout']+r['stderr'])
        c.require(len(missing) == len(required) == 5 and all(any('::'+name+'(' in symbol for symbol in missing)
            for name in required) and not (c.BUILD/'test_native_profile_required_effects.exe').exists(),
            'all five actual transitive native effects remain required')
        r = c.execute('actual-entry-link',[c.CMAKE,'--build',c.BUILD,'--target',
            'test_native_startup_missing_bindings','-j2'],expected=None)
        undefined = re.findall(r'undefined symbol: ([^\n]+)',r['stdout']+r['stderr'])
        names = {m.group(1) for symbol in undefined if (m := re.search(r'main_binding::(\w+)\(',symbol))}
        c.require(names == set(prior['remaining_integration_symbols']) and len(undefined) == len(names) == 27 and
            not (c.BUILD/'test_native_startup_missing_bindings.exe').exists(),'exact remaining Main requirements')
        for group in [sources,evidence,protected]:
            c.require(all(c.sha(path) == pin for path,pin in group.items()),'stable inputs after current checks')
        result = dict(schema='native_retail_profile_effects_connected_verification_v1',
            status='PASS_NATIVE_PROFILE_TRANSITION_DECISION_CPU_FILE_CHECKS',sources=sources,evidence=evidence,
            protected_inputs=protected,executed_clients=executions,original_word_joins=joins,
            original_table_keys=keys,explicitly_decoded_listing_gaps=sorted(gaps),native_sdl_linkage=imports,
            prior_path=str(previous),prior_sha256=c.sha(previous),explicit_prior_source_changes=changes,
            startup_archive_sha256=c.sha(c.BUILD/'libmka_native_retail_startup.a'),
            session_archive_sha256=c.sha(c.BUILD/'libmka_native_retail_game_session.a'),
            remaining_integration_symbols=sorted(names),required_profile_effects=sorted(required),
            complete_game_entry_linkable=False,game_executed=False,physical_endpoints_opened=False,frames=None,audio=None,
            limits=['Actual18B0D0 and1892D0 are used by menu/aggregate/18A260 clients; missing leaf effects test-only.',
                'Permission4EEE60,parent/profile owners,189690/188720 event/task effects and scene producers remain open.',
                'Proved0/1 caller scope; other participant records and all scene-index producers remain unbound.',
                'No accepted full boot/menu/combat/input/render/audio/gameplay evidence.'])
        c.save('verification.json',result)
        print(json.dumps(dict(status=result['status'],fresh_clients=len(executions),
            required_transitive_effects=len(required),remaining_main=len(names),game_executed=False)))
    except Exception as error:
        c.save('failure.json',dict(status='FAILED',type=type(error).__name__,message=str(error)))
        raise


if __name__ == '__main__':
    main()
