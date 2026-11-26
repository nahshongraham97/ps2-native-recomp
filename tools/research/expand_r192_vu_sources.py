"""Regenerate preserved VU inputs separately and merge exact generated bodies.

No full image variant is dropped. A common program key is merged only when its
actual generated C++ body is byte-identical. Conflicting full images additionally
require exact original reachable-word identity for their shared recorded key.
This emits a static source stage only; it launches no game or DLL.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import hashlib
import json
import re
import subprocess

REACHABLE_SOURCE = '#include "ps2/vu_ops.hpp"\n#include <fstream>\n#include <cstdio>\n#include <vector>\nint main(int argc,char**argv){\n if(argc!=3)return 2; std::vector<unsigned char> bytes(16384);\n std::ifstream in(argv[1],std::ios::binary);in.read((char*)bytes.data(),16384);if(!in||in.gcount()!=16384)return 3;\n const auto entry=(unsigned)std::stoul(argv[2],nullptr,16);\n const auto pcs=ps2::vu::reachable(bytes.data(),16383u,entry);\n const auto key=ps2::vu::program_key(bytes.data(),pcs,entry);\n std::printf("{\\"key\\":\\"%016llX\\",\\"entry\\":%u,\\"words\\":[",(unsigned long long)key,entry);\n bool first=true; for(auto pc:pcs){unsigned lo,hi;std::memcpy(&lo,bytes.data()+pc,4);std::memcpy(&hi,bytes.data()+pc+4,4);\n std::printf("%s{\\"pc\\":%u,\\"lower\\":\\"%08X\\",\\"upper\\":\\"%08X\\"}",first?"":",",pc,lo,hi);first=false;}\n std::puts("]}");return 0;\n}\n'

def sha(d):return hashlib.sha256(d).hexdigest()

def require(ok,reason):
 if not ok:raise ValueError(reason)

def capture_admission(name,digest,size,rejected):
 # A renamed copy of disproved bytes is still disproved. Preserve originals;
 # exclude only their admission into fresh compiler inputs.
 for old_name,row in rejected.items():
  if digest==row['sha256']:
   require(size==row['bytes'],'rejected_capture_extent_mismatch')
   return dict(rejected_name=old_name,reason=row['reason'],sha256=digest,bytes=size)
  require(name!=old_name,'rejected_capture_name_changed_requires_new_provenance')
 return None

def run(name,args):
 r=subprocess.run([str(x) for x in args],capture_output=True,text=True)
 (O/(name+'.log')).write_text(r.stdout+r.stderr,encoding='utf-8',newline='\n')
 return {'command':[str(x) for x in args],'exit_code':r.returncode,'stdout':r.stdout,'stderr':r.stderr}

def functions(folder):
 result={};prefixes=set();origins={}
 for path in sorted(folder.glob('vu1_*.cpp')):
  if path.name=='vu1_table.cpp':continue
  text=path.read_text();matches=list(re.finditer(r'^void vu1_([0-9A-F]{16})\(Vu0Regs &r, std::uint8_t \*mem\) \{',text,re.M))
  require(bool(matches),'missing_generated_function:'+str(path))
  prefixes.add(text[:matches[0].start()])
  for match in matches:
   end=re.search(r'^\}',text[match.end():],re.M);require(end is not None,'unclosed_function')
   body=text[match.start():match.end()+end.end()]
   require(body.count('{')==body.count('}'),'unbalanced_generated_function')
   require(match[1] not in result,'duplicate_generated_key')
   result[match[1]]=body;origins[match[1]]={'path':str(path),'file_sha256':sha(path.read_bytes()),'function_sha256':sha(body.encode())}
 require(len(prefixes)==1,'incompatible_generated_prefix')
 table=(folder/'vu1_table.cpp').read_text();keys=re.findall(r'\{0x([0-9A-F]{16})ull, vu1_\1\}',table)
 require(len(keys)==len(set(keys)) and set(keys)==set(result),'batch_table_function_key_mismatch')
 return result,prefixes.pop(),origins

def merge(destination,key,body):
 if key in destination:require(destination[key]==body,'conflicting_generated_body:'+key)
 destination[key]=body

def main():
 global O, S
 parser=argparse.ArgumentParser(description=__doc__)
 for name in ('baseline','captured','vurecomp','source-root','compiler','stage','output'):
  parser.add_argument('--'+name,type=Path,required=True)
 parser.add_argument('--input-manifest',type=Path,required=True)
 parser.add_argument('--expected-input-manifest-sha',required=True)
 parser.add_argument('--reference-result',type=Path)
 parser.add_argument('--expected-reference-sha')
 args=parser.parse_args()
 source_manifest_raw=args.input_manifest.read_bytes()
 require(sha(source_manifest_raw)==args.expected_input_manifest_sha.lower(),'input_manifest_pin_mismatch')
 source_manifest=json.loads(source_manifest_raw)
 for group,root in [('baseline',args.baseline),('captured',args.captured)]:
  expected={row['name']:row['sha256'] for row in source_manifest['aliases'] if row['group']==group}
  actual={path.name:sha(path.read_bytes()) for path in root.glob('*.bin')}
  require(bool(expected) and actual==expected,'unknown_missing_or_changed_input:'+group)
 admission_path=Path(__file__).resolve().parents[2]/'research/ledger/aot_coverage_inventory.json'
 admission_raw=admission_path.read_bytes()
 rejected=json.loads(admission_raw)['vu_microprograms']['rejected_captures']
 require(bool(rejected),'missing_rejected_capture_provenance')
 exclusions=[]
 owned=Path(__file__).resolve().parents[2]/'build'
 require(args.stage.resolve().is_relative_to(owned) and args.output.resolve().is_relative_to(owned),'owned_build_output_required')
 require(not args.stage.exists() and not args.output.exists(),'fresh_stage_and_output_required')
 require(not args.stage.resolve().is_relative_to(args.source_root.resolve()),'stage_must_not_modify_source_root')
 require(args.vurecomp.resolve().is_relative_to(owned),'owned_vurecomp_required')
 for root in (args.baseline,args.captured):
  require(root.is_dir(),'missing_input_directory')
  require(not args.stage.resolve().is_relative_to(root.resolve()),'stage_must_not_modify_input_root')
 if args.reference_result:
  require(args.expected_reference_sha is not None,'reference_SHA_required')
  require(sha(args.reference_result.read_bytes())==args.expected_reference_sha.lower(),'reference_result_pin_mismatch')
 O=args.output;S=args.stage
 O.mkdir(parents=True,exist_ok=False);S.mkdir(parents=True,exist_ok=False)
 helper_source=O/'reachable.cpp'
 helper_source.write_text(REACHABLE_SOURCE,encoding='utf-8',newline='\n')
 build=run('build-reachable',[args.compiler,'-std=c++20','-O0','-static','-I'+str(args.source_root/'runtime/include'),helper_source,'-o',O/'reachable.exe'])
 require(build['exit_code']==0,'reachable_helper_build_failed')
 aliases=[];contents={};batches={}
 for group,root in [('baseline',args.baseline),('captured',args.captured)]:
  inputs=S/group/'inputs';inputs.mkdir(parents=True,exist_ok=False);rows=[]
  for path in sorted(root.glob('*.bin')):
   require(not path.is_symlink(),'symlink_input_refused')
   require(re.fullmatch(r'[0-9A-Fa-f]{16}_[0-9A-Fa-f]{4}\.bin',path.name),'unexpected_VU_input_name')
   before=path.stat();data=path.read_bytes();after=path.stat();require(len(data)==16384,'wrong_micro_memory_size')
   require((before.st_size,before.st_mtime_ns)==(after.st_size,after.st_mtime_ns),'source_changed')
   h=sha(data)
   exclusion=capture_admission(path.name,h,len(data),rejected)
   if exclusion:
    exclusions.append(dict(group=group,source=str(path),**exclusion))
    continue
   contents[h]=data;target=inputs/path.name
   with target.open('xb') as stream:require(stream.write(data)==len(data),'short_input_write')
   require(sha(target.read_bytes())==h,'copied_input_hash_mismatch')
   row={'group':group,'source':str(path),'copy':str(target),'name':path.name,'sha256':h,
        'key':path.stem.split('_')[0].upper(),'entry':int(path.stem.split('_')[1],16)}
   rows.append(row);aliases.append(row)
  batches[group]={'inputs':inputs,'aliases':rows}
  require(bool(rows),'empty_input_batch:'+group)
 store=S/'original-content';store.mkdir(exist_ok=False)
 for h,data in contents.items():
  with (store/(h+'.bin')).open('xb') as stream:require(stream.write(data)==len(data),'short_blob_write')
  require(sha((store/(h+'.bin')).read_bytes())==h,'blob_hash_mismatch')
 tool=args.vurecomp;tool_sha=sha(tool.read_bytes())
 for group,batch in batches.items():
  generated=S/group/'generated';command=run('generate-'+group,[tool,batch['inputs'],generated,'--require-complete'])
  require(command['exit_code']==0,'batch_generation_refused:'+group)
  count=re.fullmatch(r'(\d+) VU1 programs, (\d+) of them jump targets, (\d+) skipped\s*',command['stdout'])
  require(count is not None and int(count[1])-int(count[2])==len(batch['aliases']) and int(count[3])==0 and not command['stderr'].strip(),
          'input_key_mismatch_or_incomplete_generation:'+group)
  batch['generation']=command;batch['functions'],batch['prefix'],batch['origins']=functions(generated)
 require(batches['baseline']['prefix']==batches['captured']['prefix'],'different_generated_prefix')
 # Complete code-word and claimed-body proof for the sole same-name/different-image conflict.
 byname={}
 for row in aliases:byname.setdefault(row['name'],[]).append(row)
 conflicts={name:rows for name,rows in byname.items() if len({x['sha256'] for x in rows})>1}
 helper=O/'reachable.exe';helper_sha=sha(helper.read_bytes());proofs=[]
 for name,rows in conflicts.items():
  variants=[]
  for row in rows:
   invocation=run('reachable-'+row['group'],[helper,row['copy'],f"{row['entry']:04X}"])
   require(invocation['exit_code']==0,'reachable_helper_failed');actual=json.loads(invocation['stdout'])
   require(actual['key']==row['key'],'recomputed_key_mismatch')
   body=batches[row['group']]['functions'][row['key']]
   variants.append({'alias':row,'reachable':actual,'body_sha256':sha(body.encode()),'helper_command':invocation})
  require(len(variants)==2,'unexpected_conflict_cardinality')
  require(variants[0]['reachable']==variants[1]['reachable'],'conflicting_original_reachable_words')
  require(variants[0]['body_sha256']==variants[1]['body_sha256'],'conflicting_claimed_generated_body')
  proofs.append({'name':name,'variants':variants,'complete_reachable_words_identical':True,'claimed_generated_body_identical':True,
                 'scope':'Raw full images differ outside the selected reachable body. Both batches and all extra jump-target outputs retained.'})
 merged={};origins={}
 for group,batch in batches.items():
  for key,body in sorted(batch['functions'].items()):
   merge(merged,key,body);origins.setdefault(key,[]).append(dict(batch['origins'][key],group=group))
 expected=set(batches['baseline']['functions'])|set(batches['captured']['functions'])
 require(set(merged)==expected,'merged_key_union_mismatch')
 out=S/'generated';out.mkdir(exist_ok=False);prefix=batches['baseline']['prefix'];keys=sorted(merged)
 for slot,start in enumerate(range(0,len(keys),8)):
  text=prefix+'\n\n'.join(merged[key] for key in keys[start:start+8])+'\n\n}\n'
  (out/f'vu1_{slot:03}.cpp').write_text(text,encoding='utf-8',newline='\n')
 table='#include "ps2/vu_ops.hpp"\n\nnamespace gen {\n'
 table+=''.join(f'void vu1_{key}(ps2::Vu0Regs &r, std::uint8_t *mem);\n' for key in keys)
 table+='\nconst ps2::vu::Program kVuPrograms[] = {\n'
 table+=''.join(f'    {{0x{key}ull, vu1_{key}}},\n' for key in keys)
 table+='};\n\n}\n\nvoid register_generated_vu() { ps2::vu::register_programs(gen::kVuPrograms, '+str(len(keys))+'u); }\n'
 (out/'vu1_table.cpp').write_text(table,encoding='utf-8',newline='\n')
 reloaded,_,_=functions(out);require(reloaded==merged,'merged_generated_body_changed')
 controls=[]
 for name,row in rejected.items():
  require(capture_admission(name,row['sha256'],row['bytes'],rejected) is not None,'known_bad_capture_admitted')
  require(capture_admission('0000000000000000_0000.bin',row['sha256'],row['bytes'],rejected) is not None,'renamed_bad_capture_admitted')
  controls.append(dict(name='known_and_renamed_disproved_capture',status='REJECTED',sha256=row['sha256']))
  try:capture_admission(name,'0'*64,row['bytes'],rejected)
  except ValueError:controls.append(dict(name='changed_rejected_name_without_provenance',status='REJECTED'))
  else:raise ValueError('changed_rejected_capture_accepted')
 try:merge(dict(merged),keys[0],merged[keys[0]]+'\n/* planted change */')
 except ValueError as ex:require(str(ex).startswith('conflicting_generated_body:'),'wrong_control_reason');controls.append({'name':'same_key_changed_body','status':'REJECTED','reason':str(ex)})
 else:raise ValueError('changed_body_accepted')
 try:require(set(keys[:-1])==expected,'merged_key_union_mismatch')
 except ValueError as ex:controls.append({'name':'dropped_table_key','status':'REJECTED','reason':str(ex)})
 else:raise ValueError('missing_key_accepted')
 require(sha(tool.read_bytes())==tool_sha and sha(helper.read_bytes())==helper_sha,'producer_changed')
 result={'status':'OFFLINE_VU_GENERATION_AND_EXACT_BODY_UNION_PASS','stage':str(S),'merged_generated':str(out),
  'tool_script':{'path':str(Path(__file__)),'sha256':sha(Path(__file__).read_bytes())},
  'source_counts':{group:len(batch['aliases']) for group,batch in batches.items()},'alias_count':len(aliases),
  'unique_content_images':len(contents),'unique_key_entry_aliases':len({(x['key'],x['entry'],x['sha256']) for x in aliases}),
  'batch_program_counts':{group:len(batch['functions']) for group,batch in batches.items()},'merged_program_keys':len(keys),
  'same_body_common_keys':len(set(batches['baseline']['functions'])&set(batches['captured']['functions'])),
  'new_keys_over_baseline':sorted(set(batches['captured']['functions'])-set(batches['baseline']['functions'])),
  'aliases':aliases,'conflicts':proofs,'controls':controls,'function_origins':origins,
  'excluded_provenance':exclusions,'admission_manifest':{'path':str(admission_path),'sha256':sha(admission_raw)},
  'input_manifest':{'path':str(args.input_manifest),'sha256':sha(source_manifest_raw)},
  'generation':{group:batch['generation'] for group,batch in batches.items()},
  'generated_files':[{'path':str(x),'sha256':sha(x.read_bytes()),'size':x.stat().st_size} for x in sorted(out.iterdir())],
  'producer':{'vurecomp':str(tool),'sha256':tool_sha,'reachable_helper':str(helper),'reachable_helper_sha256':helper_sha,
              'reachable_source_sha256':sha(helper_source.read_bytes()),'reachable_build':build,
              'compiler':str(args.compiler),'compiler_sha256':sha(args.compiler.read_bytes()),
              'reachable_header_sha256':sha((args.source_root/'runtime/include/ps2/vu_ops.hpp').read_bytes()),
              'vurecomp_cpp_sha256':sha((args.source_root/'recomp/vurecomp.cpp').read_bytes()),'vurecomp_gen_cpp_sha256':sha((args.source_root/'recomp/vurecomp_gen.cpp').read_bytes())},
  'filename_policy':'Source sscanf accepts suffixes, but initial Program vector retains duplicate recorded keys; duplicate-key suffix workaround deliberately not used. Two independent generations preserve all source variants; exact common function bodies deduplicate only after generation.',
  'scope':'Offline AOT C++ generation only. Every merged function body is byte-identical to actual generator output. Generated table is exact sorted key union. No game execution, native credit, N/generated_ee/out metadata edit, or originalC write.'}
 if args.reference_result:
  reference_raw=args.reference_result.read_bytes()
  require(args.expected_reference_sha and sha(reference_raw)==args.expected_reference_sha.lower(),'reference_result_pin_mismatch')
  reference=json.loads(reference_raw)
  comparable=['source_counts','alias_count','unique_content_images','unique_key_entry_aliases','batch_program_counts','merged_program_keys','same_body_common_keys','new_keys_over_baseline']
  checks={name:result[name]==reference[name] for name in comparable}
  checks['every_generated_file_sha256']={Path(x['path']).name:x['sha256'] for x in result['generated_files']}=={Path(x['path']).name:x['sha256'] for x in reference['generated_files']}
  result['reference_comparison']={'path':str(args.reference_result.resolve()),'sha256':sha(reference_raw),'checks':checks}
  if not all(checks.values()):result['status']='REFERENCE_COMPARISON_FAILED'
 target=O/'RESULT.json';target.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
 print(json.dumps({'result':str(target),'sha256':sha(target.read_bytes()),'status':result['status'],
  'source_counts':result['source_counts'],'unique_content_images':len(contents),'batch_program_counts':result['batch_program_counts'],
  'merged_program_keys':len(keys),'new_keys':result['new_keys_over_baseline'],'generated_cpp_files':len(list(out.glob('*.cpp')))}))

 return 0 if result['status']=='OFFLINE_VU_GENERATION_AND_EXACT_BODY_UNION_PASS' else 1


if __name__ == '__main__':
 raise SystemExit(main())
