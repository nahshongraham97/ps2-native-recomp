from pathlib import Path
import json,struct,hashlib,subprocess
p=Path(r'C:/Users/nahsh/mk port/Armageddon-backup/armageddon-reimplementation');o=Path(__file__).parent;elf=(p/'research/inputs/SLUS_214.10').read_bytes();sha=hashlib.sha256(elf).hexdigest();assert sha=='a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f'
phoff=struct.unpack_from('<I',elf,28)[0];psz,pnum=struct.unpack_from('<HH',elf,42);segs=[]
for i in range(pnum):
 h=struct.unpack_from('<8I',elf,phoff+i*psz)
 if h[0]==1:segs.append(dict(offset=h[1],vaddr=h[2],filesz=h[4],memsz=h[5]))
rows=[]
for f in sorted((o/'ee').glob('*.tsv')):
 lines=f.read_text().splitlines();raw=bytearray();und=[]
 for line in lines:
  a,b,asm=line.split('\t');a=int(a,16);s=next(s for s in segs if s['vaddr']<=a and a+4<=s['vaddr']+s['filesz']);offset=s['offset']+a-s['vaddr'];data=elf[offset:offset+4];assert data.hex()==b;raw+=data
  if asm=='UNDECODED':
   if a==0x245e84:
    assert struct.unpack('<I',data)[0]==0x8e450000
    und.append(dict(address=hex(a),decode='LW a1,0(s2); opcode35 rs18 rt5 imm0; sign-extends32 to64',role='unreachable duplicate in this function: taken BNEL245E74 executes load245E78 then targets245E88; not-taken annuls245E78 and B245E7C exits to246044'))
   else:
    assert data==b'\0'*4;und.append(dict(address=hex(a),decode='SLL zero,zero,0',role='zero padding after JR return delay slot; outside function body'))
 (o/'ee'/f.with_suffix('.words.bin').name).write_bytes(raw)
 rows.append(dict(listing=f.name,start=lines[0].split('\t')[0],end_inclusive=lines[-1].split('\t')[0],words=len(lines),joined_words=len(lines),omitted_resolution=und,sha256=hashlib.sha256(raw).hexdigest()))
def git(*a):return subprocess.check_output(['git',*a],text=True).strip()
record=dict(elf_sha256=sha,pt_load=segs,listings=rows,delivered_commit=git('rev-parse','b986341'),object_type=git('cat-file','-t','b986341'),commit_raw=git('cat-file','-p','b986341'),delivery_paths=git('diff-tree','--no-commit-id','--name-only','-r','b986341').splitlines(),published_heads=git('ls-remote','--heads','origin'),remote_contains_delivery=git('branch','-r','--contains','b986341'),private_branch=git('branch','--show-current'),applied_delivery=git('rev-parse','40ab870'),source_status='joined full code spans plus resolved listing omission and zero padding; no gameplay execution')
(o/'SOURCE_JOIN.json').write_text(json.dumps(record,indent=2)+'\n',newline='\n');print('PASS',sum(r['words'] for r in rows),'original words joined')
