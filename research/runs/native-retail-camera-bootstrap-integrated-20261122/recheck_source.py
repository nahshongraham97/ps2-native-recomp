"""Authenticate original camera bootstrap words and zero-storage mapping."""
from pathlib import Path
import hashlib,json,struct
record=Path(__file__).resolve().parent
fork=record.parents[2];parent=fork.parents[2]
source=fork/'research/runs/parallel-scene-bootstrap-continuation-20261122/SOURCE_JOIN.json'
proof=json.loads(source.read_text())
elf=parent/'private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/SLUS_214.10'
raw=elf.read_bytes()
def sha(raw):return hashlib.sha256(raw).hexdigest()
assert sha(raw)==proof['elf_sha256']
assert raw[:6]==b"\x7fELF\x01\x01"
phoff=struct.unpack_from('<I',raw,28)[0];entsize,count=struct.unpack_from('<HH',raw,42)
segments=[struct.unpack_from('<8I',raw,phoff+i*entsize) for i in range(count)]
def mapping(vma,size,file=True):
    matches=[s for s in segments if s[0]==1 and s[2]<=vma and vma+size<=s[2]+s[4 if file else 5]]
    assert len(matches)==1,(hex(vma),size);s=matches[0];return s,s[1]+vma-s[2]
rows=[]
for f in proof['functions']:
    begin,end=int(f['begin'],16),int(f['end'],16);size=end-begin
    s,offset=mapping(begin,size);expected=bytes.fromhex(f['original_bytes'])
    assert offset==f['offset'] and len(expected)==size and raw[offset:offset+size]==expected
    path=fork/Path(f['path']);data=path.read_bytes()
    variants={'raw':data,'lf':data.replace(b'\r\n',b'\n'),'crlf':data.replace(b'\r\n',b'\n').replace(b'\n',b'\r\n')}
    identities=[k for k,v in variants.items() if sha(v)==f['sha256']];assert identities,str(path)
    listing={}
    for line in data.decode().splitlines():
        fields=line.split('\t');assert len(fields)>=3,line
        address=int(fields[0],16);word=bytes.fromhex(fields[1]);assert len(word)==4
        assert begin<=address<end and address not in listing
        assert raw[offset+address-begin:offset+address-begin+4]==word
        listing[address]=word
    omitted=[a for a in range(begin,end,4) if a not in listing]
    assert len(listing)==f['words'] and size//4==f['span_words']
    assert omitted==[int(a,16) for a in f['omitted']]
    for address in omitted:
        assert raw[offset+address-begin:offset+address-begin+4]==b'\0'*4
        branch=struct.unpack_from('<I',raw,offset+address-begin-8)[0]
        assert branch>>26==4 and (branch>>21&31)==(branch>>16&31)
        immediate=struct.unpack('<h',struct.pack('<H',branch&65535))[0]
        target=address-8+4+immediate*4
        assert address<target<end and target in listing
        for pc,data in listing.items():
            word=int.from_bytes(data,'little');op=word>>26;rs=word>>21&31;rt=word>>16&31
            is_branch=op in (4,5,6,7,20,21,22,23) or (op==1 and rt in (0,1,2,3,16,17,18,19)) or (op in (16,17,18,19) and rs==8)
            if is_branch:
                displacement=struct.unpack('<h',struct.pack('<H',word&65535))[0]
                assert pc+4+displacement*4!=address
            if op in (2,3):assert ((pc+4)&0xf0000000)|((word&0x3ffffff)*4)!=address
    rows.append(dict(path=f['path'],listed_words=len(listing),span_words=size//4,omitted_zero_after_unconditional_branch=[hex(a) for a in omitted],raw_sha256=sha(data),saved_hash_identities=identities))
for store in proof['storage_birth']:
    vma,size=int(store['vma'],16),store['size'];segment,_=mapping(vma,size,False)
    assert list(segment)==store['segment'] and vma>=segment[2]+segment[4]
    assert store['loader_zero'] and not store['file_backed']
result=dict(status='PASS_ORIGINAL_CAMERA_BOOTSTRAP_WORDS_AND_BSS',source_join_sha256=sha(source.read_bytes()),elf_sha256=sha(raw),functions=rows,listed_words=sum(r['listed_words'] for r in rows),storage_birth_records=len(proof['storage_birth']),omitted_words_proved=2,semantic_review='parallel-camera-bootstrap-review-20261122/REVIEW.md and correction/REVIEW.md; no new live decompiler claim',game_run=False,acceptance='OPEN')
output=record/'SOURCE_RECHECK.json';assert not output.exists();output.write_text(json.dumps(result,indent=2)+'\n',newline='\n')
print(result['status'],len(rows),result['listed_words'])
