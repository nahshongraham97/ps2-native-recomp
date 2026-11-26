"""Audit complete offline VU entry generation before any module registration."""
from pathlib import Path
import argparse
import hashlib
import json
import re
from extract_r192_elf_vu import require,sha

ROOT=Path(__file__).resolve().parents[2]
FUNCTION=re.compile(r'^void vu1_([0-9A-F]{16})\(Vu0Regs &r, std::uint8_t \*mem\) \{',re.M)

def bodies(folder,expected=None):
    found={};prefixes=set();unsupported=[]
    paths=sorted(folder.glob('vu1_*.cpp'))
    if expected is not None:
        require({p.name for p in paths}=={Path(r['path']).name for r in expected},'generated_file_set_changed')
        for row in expected:require(sha(folder/Path(row['path']).name)==row['sha256'],'generated_file_changed')
    for path in paths:
        if path.name=='vu1_table.cpp':continue
        text=path.read_text();matches=list(FUNCTION.finditer(text))
        require(matches,'missing_body:'+str(path));prefixes.add(text[:matches[0].start()])
        for m in matches:
            end=re.search(r'^\}',text[m.end():],re.M);require(end is not None,'unclosed_body')
            body=text[m.start():m.end()+end.end()]
            require(body.count('{')==body.count('}'),'unbalanced_body')
            require(m[1] not in found,'duplicate_key_inside_module')
            found[m[1]]={'sha256':hashlib.sha256(body.encode()).hexdigest(),'source':str(path)}
            for line in body.splitlines():
                if re.match(r'L_[0-9A-F]{4}:',line) and 'vu::interpret(' in line:
                    unsupported.append({'key':m[1],'source':str(path),'line':line})
    table=(folder/'vu1_table.cpp').read_text()
    keys=re.findall(r'\{0x([0-9A-F]{16})ull, vu1_\1\}',table)
    require(len(keys)==len(set(keys)) and set(keys)==set(found),'table_body_set_changed')
    require(len(prefixes)==1,'inconsistent_generated_prefix')
    return found,prefixes.pop(),unsupported

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('generation','prior-generated','output'):p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--expected-generation-sha',required=True)
    a=p.parse_args()
    require(a.output.resolve().is_relative_to(ROOT/'build') and not a.output.exists(),'fresh_owned_output_required')
    require(sha(a.generation)==a.expected_generation_sha,'generation_pin')
    generation=json.loads(a.generation.read_text())
    require(generation['generation_requested'] and generation['generation_all_pass'],'complete_generation_required')
    prior,prefix,_=bodies(a.prior_generated)
    merged={};conflicts=[];unsupported=[];changed=[];same=set();aliases=0
    for row in generation['records']:
        require(row['generation_pass'],'payload_generation_refused')
        folder=Path(row['entries']).parent/'generated'
        current,current_prefix,limits=bodies(folder,row['generated_files'])
        require(prefix==current_prefix,'prior_prefix_changed')
        require(len(current)==row['counts']['admitted'],'module_entry_denominator')
        aliases+=len(current);unsupported.extend(limits)
        for key,value in current.items():
            if key in prior:
                if value['sha256']!=prior[key]['sha256']:changed.append({'key':key,'prior':prior[key],'new':value})
                else:same.add(key)
            if key in merged:
                if merged[key]['sha256']!=value['sha256']:conflicts.append({'key':key,'first':merged[key],'next':value})
            else:merged[key]=value
    require(aliases==generation['totals']['admitted'] and len(merged)==generation['unique_keys'],'global_entry_denominator')
    status='BODY_IDENTITY_AND_SUPPORTED_LOWER_AUDIT_PASS' if not conflicts and not unsupported and not changed else 'REFUSED_GLOBAL_BODY_ADMISSION'
    a.output.mkdir(parents=True)
    result={'status':status,'generated_instances':aliases,'unique_keys':len(merged),
            'prior_exact_common_bodies':len(same),'prior_keys_not_in_original_source_domain':sorted(prior.keys()-merged.keys()),
            'changed_common_bodies':changed,'same_key_different_bodies':conflicts,
            'unsupported_lower_emissions':unsupported,'body_manifest':merged,
            'input_pins':{'generation':{'path':str(a.generation),'sha256':sha(a.generation)},
                          'script':{'path':str(Path(__file__)),'sha256':sha(Path(__file__))}},
            'scope':'Actual generated function bodies and tables, not opcode hints. Lower fallback sites refuse; dynamic compiled-entry chaining remains distinct. This is not native-object compilation, equivalence, launch-root or full-game acceptance.'}
    out=a.output/'RESULT.json';out.write_text(json.dumps(result,indent=2)+'\n',newline='\n')
    print(json.dumps({'status':status,'result':str(out),'sha256':sha(out),'unique_keys':len(merged),
                      'conflicts':len(conflicts),'unsupported':len(unsupported),'changed_prior':len(changed),
                      'prior_exact':len(same),'prior_not_in_domain':len(result['prior_keys_not_in_original_source_domain'])}))
    return 0 if status.endswith('_PASS') else 2

if __name__=='__main__':
    raise SystemExit(main())
