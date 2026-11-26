"""Group EVERY pinned EE cache metadata record without inventing missing words.

Uses the original decoder offline. Group identities retain register/immediate
fields and normalize only source-proved direct jump relocations. Sparse checks
remain sparse; hints based only on opcode names are NEVER family admission.
"""
from pathlib import Path
from collections import Counter,defaultdict
import argparse
import hashlib
import json
import struct
import subprocess

W=Path(__file__).resolve().parents[2]
SCREENSHOTS={
 '00818000_2A38A327':0x008c1730,'00818000_E718A3A4':0x009255d0,
 '00818000_ADF20942':0x00923da0,'00818000_791D04A0':0x00922570}


def digest(path):
 h=hashlib.sha256()
 with path.open('rb') as stream:
  for block in iter(lambda:stream.read(4*1024*1024),b''):h.update(block)
 return h.hexdigest()


def records(path):
 """Stream the frozen producer's indent-2 EE array; reject format drift."""
 active=False;parts=[];seen=0
 with path.open(encoding='utf8') as f:
  for line in f:
   if line.startswith('  "EE": ['):active=True;continue
   if active and line.startswith('  ],'):
    if parts:raise ValueError('truncated_EE_record')
    break
   if active and line.startswith('    {'):
    if parts:raise ValueError('nested_top_level_EE_record')
    parts=[line];continue
   if parts:
    parts.append(line)
    if line.startswith('    }'):
     yield json.loads(''.join(parts).strip().rstrip(','));parts=[];seen+=1
 if not active or seen!=3569:raise ValueError('EE_denominator_or_format_changed:'+str(seen))


def shape(checks,base,size,decoded):
 anchor=min(checks) if checks else 0;tokens=[];hints=[]
 for off,word in sorted(checks.items()):
  name,flow,delay=decoded[word];token=word
  if name in ('j','jal','J','Jal') or word>>26 in (2,3):
   pc=(base+off)&0xffffffff;target=((pc+4)&0xf0000000)|((word&0x3ffffff)<<2)
   if base<=target<base+size:token=('direct_internal_target',word>>26,target-base-anchor)
  tokens.append((off-anchor,token));hints.append((off-anchor,name))
 return hashlib.sha256(json.dumps(tokens,separators=(',',':')).encode()).hexdigest(),hashlib.sha256(json.dumps(hints,separators=(',',':')).encode()).hexdigest()


def first_unproved(entry,checks,base,size,decoded,limit=256):
 """Bounded original local-CFG input audit, NOT a transform/compiler verdict."""
 todo=[entry];seen=set();returns=0
 while todo:
  off=todo.pop()
  if off in seen:continue
  if len(seen)>=limit:return 'LOCAL_CFG_ANALYSIS_WORD_BOUND',off,len(seen)
  if off<0 or off+4>size:return 'OUT_OF_DECLARED_IMAGE_EDGE',off,len(seen)
  if off not in checks:return 'MISSING_AUTHENTICATED_INSTRUCTION_WORD',off,len(seen)
  seen.add(off);word=checks[off];name,flow,delay=decoded[word]
  if name.lower()=='invalid':return 'UNDECODED_ORIGINAL_WORD',off,len(seen)
  if delay:
   if off+4 not in checks:return 'MISSING_AUTHENTICATED_DELAY_WORD',off+4,len(seen)
   if decoded[checks[off+4]][1]!=0:return 'CONTROL_IN_DELAY_SLOT_UNPROVED',off+4,len(seen)
   seen.add(off+4)
  if flow in (4,6):return 'CALL_RETURN_OR_INDIRECT_TARGET_UNPROVED',off,len(seen)
  if flow==5:
   if ((word>>21)&31)==31:returns+=1;continue
   return 'INDIRECT_EXIT_TARGET_UNPROVED',off,len(seen)
  if flow==7:return 'STOP_OR_PRIVILEGED_EXIT_UNPROVED',off,len(seen)
  if flow==3:
   pc=base+off;target=((pc+4)&0xf0000000)|((word&0x3ffffff)<<2);todo.append(target-base);continue
  if flow in (1,2):
   immediate=word&65535;immediate-=65536 if immediate&32768 else 0
   target=off+4+4*immediate;rs=(word>>21)&31;rt=(word>>16)&31;op=word>>26
   if op in (4,20) and rs==rt:todo.append(target)
   elif op in (5,21) and rs==rt:todo.append(off+8)
   else:todo.extend([off+8,target])
   continue
  todo.append(off+4)
 return ('LOCAL_RETURNS_CLOSED_ABI_UNPROVED' if returns else 'NO_LOCAL_RETURN_PROVED'),entry,len(seen)


def self_test():
 a=0x08000000|((0x201000>>2)&0x3ffffff);b=0x08000000|((0x301000>>2)&0x3ffffff)
 decoded={a:('J',3,True),b:('J',3,True),0xd9640000:('Lqc2',0,False),0xd9650000:('Lqc2',0,False),0x3e00008:('Jr',5,True),0:('Sll',0,False)}
 if shape({0x100:a},0x200000,0x2000,decoded)[0]!=shape({0x100:b},0x300000,0x2000,decoded)[0]:raise ValueError('self_test_relocation_failed')
 if shape({0:0xd9640000},0,0x100,decoded)[0]==shape({0:0xd9650000},0,0x100,decoded)[0]:raise ValueError('self_test_register_change_erased')
 complete={0:0xd9640000,4:0x3e00008,8:0}
 if first_unproved(0,complete,0,12,decoded)[0]!='LOCAL_RETURNS_CLOSED_ABI_UNPROVED':raise ValueError('self_test_return_scope_failed')
 if first_unproved(0,{0:0xd9640000,4:0x3e00008},0,12,decoded)[0]!='MISSING_AUTHENTICATED_DELAY_WORD':raise ValueError('self_test_gap_accepted')
 return {'status':'PASS','controls':['internal_jump_relocation_equal','changed_register_retained','closed_return_still_ABI_unproved','missing_delay_refused'],
         'entry_offset_contract':'Producer entries.offset already includes unit.base; normalize uint32 only, never add unit.base again.'}


def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--metadata',type=Path,required=True);p.add_argument('--original-source',type=Path,required=True)
 p.add_argument('--compiler',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
 p.add_argument('--miss-dump-directory',type=Path,action='append',default=[])
 a=p.parse_args();out=a.output.resolve()
 if not out.is_relative_to((W/'build/oracle-telemetry').resolve()) or out.exists():raise ValueError('fresh_owned_output_required')
 metadata_pin=digest(a.metadata);out.mkdir(parents=True)
 unique=set();module_count=entry_count=0
 for row in records(a.metadata):
  for m in row['modules']:
   module_count+=1;entry_count+=m['entry_count'];unique.update(int(c['word'],16) for c in m['checks'])
 ordered=sorted(unique);decoder=out/'decode.cpp'
 decoder.write_text('#include "r5900.hpp"\n#include <cstdio>\n#include <io.h>\n#include <fcntl.h>\nint main(){_setmode(_fileno(stdin),_O_BINARY);unsigned w;while(std::fread(&w,4,1,stdin)==1){auto i=recomp::decode(w,0);std::printf("%08X %s %d %d\\n",w,recomp::name(i.op),int(recomp::flow(i.op)),recomp::has_delay_slot(i.op));}}\n',encoding='utf8',newline='\n')
 exe=out/'decode.exe';command=[str(a.compiler),'-std=c++20','-O2','-static-libgcc','-static-libstdc++','-I'+str((a.original_source/'recomp').resolve()),str(decoder),str((a.original_source/'recomp/r5900.cpp').resolve()),'-o',str(exe)]
 cp=subprocess.run(command,capture_output=True);(out/'decoder-compile.log').write_bytes(cp.stdout+cp.stderr)
 if cp.returncode:raise ValueError('original_decoder_compile_failed')
 cp=subprocess.run([str(exe)],input=struct.pack('<'+'I'*len(ordered),*ordered),capture_output=True)
 if cp.returncode:raise ValueError('original_decoder_failed')
 (out/'decoded-words.txt').write_bytes(cp.stdout);decoded={}
 for line in cp.stdout.decode().splitlines():
  word,name,flow,delay=line.split();decoded[int(word,16)]=(name,int(flow),bool(int(delay)))
 if set(decoded)!=unique:raise ValueError('decoded_word_denominator_mismatch')
 groups=defaultdict(list);hints=defaultdict(list);opcounts=Counter();firsts=Counter();screens=[];total=0
 with (out/'modules.jsonl').open('w',encoding='utf8',newline='\n') as modules,(out/'entries.jsonl').open('w',encoding='utf8',newline='\n') as entries:
  for index,row in enumerate(records(a.metadata)):
   for m in row['modules']:
    base,size=m['fixed_base'],m['size'];checks={}
    for c in m['checks']:
     off,word=c['offset'],int(c['word'],16)
     if off in checks and checks[off]!=word:raise ValueError('conflicting_checked_word')
     checks[off]=word
    body_hash,hint_hash=shape(checks,base,size,decoded);reference=dict(record=index,module=m['name'],artifact_sha256=row['artifact']['sha256'],rank=row['rank'])
    groups[body_hash].append(reference);hints[hint_hash].append(reference);ops=Counter(decoded[w][0] for w in checks.values());opcounts.update(ops)
    # Frozen producer harvest.py: offset=unit.base+4*k, already module-relative.
    eps=[e['offset']&0xffffffff for u in m['units'] for e in u['entries']]
    if len(eps)!=m['entry_count']:raise ValueError('entry_count_mismatch')
    local=Counter()
    for entry in eps:
     reason,where,visited=first_unproved(entry,checks,base,size,decoded);firsts[reason]+=1;local[reason]+=1;total+=1
     entries.write(json.dumps(dict(record=index,module=m['name'],entry=f'{(base+entry)&0xffffffff:08X}',first_unproved=reason,pc=f'{(base+where)&0xffffffff:08X}',checked_words_visited=visited),separators=(',',':'))+'\n')
    modules.write(json.dumps(dict(**reference,fixed_base=f'{base:08X}',declared_size=size,checked_words=len(checks),entries=len(eps),relocation_normalized_checked_code_hash=body_hash,opcode_only_hint_hash=hint_hash,opcodes=dict(ops),first_unproved=dict(local),body_identity_scope='Authenticated checked-code set only; unrecorded bytes are UNKNOWN, not zero-filled or reconstructed.'),separators=(',',':'))+'\n')
    if m['name'] in SCREENSHOTS:
     pc=SCREENSHOTS[m['name']];screens.append(dict(**reference,observed_pc=f'{pc:08X}',entry_in_metadata=pc-base in eps,checked_word_at_pc=f'{checks[pc-base]:08X}' if pc-base in checks else None,body_hash=body_hash,check_count=len(checks),declared_size=size,first_unproved=first_unproved(pc-base,checks,base,size,decoded)[0]))
 dumps=[]
 for folder in a.miss_dump_directory:
  for path in sorted(folder.glob('*.bin')):
   data=path.read_bytes();dumps.append(dict(path=str(path.resolve()),size=len(data),sha256=hashlib.sha256(data).hexdigest(),scope='Intact raw capture retained separately; no sparse reconstruction or cache mutation.'))
 if total!=entry_count or module_count!=3569:raise ValueError('all_record_denominator_not_preserved')
 if digest(a.metadata)!=metadata_pin:raise ValueError('metadata_changed_during_audit')
 result=dict(status='COMPLETE_PINNED_CACHE_GROUPING_NOT_UNIVERSAL_CLOSURE',metadata=dict(path=str(a.metadata.resolve()),sha256=metadata_pin),
  denominators=dict(DLL_records=3569,module_records=module_count,entry_records=entry_count,unique_original_words=len(unique),relocation_normalized_checked_code_groups=len(groups),opcode_only_hint_groups=len(hints)),
  opcode_counts_over_recorded_checked_words=dict(opcounts.most_common()),first_unproved_entry_counts=dict(firsts),
  normalized_groups=[dict(hash=k,count=len(v),records=v) for k,v in sorted(groups.items(),key=lambda kv:(-len(kv[1]),kv[0]))],
  opcode_hint_groups=[dict(hash=k,count=len(v),records=v) for k,v in sorted(hints.items(),key=lambda kv:(-len(kv[1]),kv[0]))],
  owner_screenshot_exact_image_joins=screens,owner_screenshot_images_absent=[x for x in SCREENSHOTS if x not in {r['module'] for r in screens}],
  historical_miss_dumps=dumps,self_test=self_test(),decoder_source_pins=[dict(path=str(x.resolve()),sha256=digest(x)) for x in [Path(__file__),a.original_source/'recomp/r5900.cpp',a.original_source/'recomp/r5900.hpp']],compile_command=command,
  raw_outputs=[dict(path=str(x.resolve()),sha256=digest(x)) for x in [out/'modules.jsonl',out/'entries.jsonl',out/'decoded-words.txt']],
  limits=['All3569 records and589356 entries processed; no sample denominator.',
          'First-unproved is a bounded256-word local-CFG input audit, not a failed compiler run or ABI proof. Bound exhaustion remains explicit.',
          'Checks often omit body tails or data; group identity is checked-code identity, not complete original image/constructor identity.',
          'Only actual direct internal J/JAL target relocation is normalized. Registers/immediates retained; opcode-only groups are non-admitting hints.',
          'No all-asset/high-level-emitter closure, native credit, runtime integration, harvesting or game launch.'])
 raw=(json.dumps(result,indent=2)+'\n').encode();(out/'RESULT.json').write_bytes(raw)
 print(json.dumps(dict(status=result['status'],denominators=result['denominators'],first_unproved=dict(firsts),sha256=hashlib.sha256(raw).hexdigest())))


if __name__=='__main__':main()
