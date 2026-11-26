"""Read-only disc executable/container census, including bounded PAK/SEC/AFS.

Magic recognition is an input inventory, not proof of all executable producers.
Compressed/other resource formats and runtime constructors need their own proof.
"""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import struct

ROOT=Path(__file__).resolve().parents[2]
ISO_SHA='48e395b56fe6888d9013d36b4b04d2a1decf2f52259063936a8e9f53e824966d'
MAGIC={b'\x7fELF':'ELF',b'MWo3':'MWo3',b'SNR2':'SNR2',b'xff2':'xff2'}

def require(ok,reason):
    if not ok:
        raise ValueError(reason)

def digest(stream,offset,size):
    stream.seek(offset);h=hashlib.sha256()
    while size:
        data=stream.read(min(size,4<<20));require(data,'truncated_input')
        size-=len(data);h.update(data)
    return h.hexdigest()

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--iso',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    require(a.output.resolve().is_relative_to(ROOT/'build') and not a.output.exists(),'fresh_owned_output_required')
    before=a.iso.stat()
    with a.iso.open('rb') as stream:
        require(digest(stream,0,before.st_size)==ISO_SHA,'original_ISO_pin')
    source=ROOT/'scripts/disc.py'
    spec=importlib.util.spec_from_file_location('r192_disc_inventory',source)
    disc=importlib.util.module_from_spec(spec);spec.loader.exec_module(disc)
    image=disc.Image(str(a.iso));files=list(image.files())
    names=set();records=[];containers=[];headers={};members=0
    sections=[];limits=[];pak_members=0
    def scan(name,offset,size,depth=0):
        nonlocal members,pak_members
        require(depth<8,'container_depth_unproved')
        require(0<=offset<=before.st_size and 0<=size<=before.st_size-offset,'file_outside_ISO')
        image.file.seek(offset);head=image.file.read(min(size,64))
        if head[:4] in MAGIC:
            row={'path':name,'ISO_offset':offset,'bytes':size,'format':MAGIC[head[:4]],
                 'sha256':digest(image.file,offset,size)}
            if row['format']=='ELF':
                require(len(head)>=52 and head[4:6]==b'\x01\x01','ELF32_LE_required')
                row.update(ELF_type=struct.unpack_from('<H',head,16)[0],
                           machine=struct.unpack_from('<H',head,18)[0],
                           entry=struct.unpack_from('<I',head,24)[0],
                           flags=struct.unpack_from('<I',head,36)[0])
            records.append(row)
        elif len(head)>=20 and struct.unpack_from('<I',head)[0] in (0x204b4150,0x50414b20):
            _,version,count,table_at,strings_size=struct.unpack_from('<5I',head)
            require(version in (1,256),'PAK_version_unproved')
            require(table_at<=size and count<=(size-table_at)//12,'PAK_index_bounds')
            strings_at=table_at+count*12
            require(strings_size<=size-strings_at,'PAK_strings_bounds')
            image.file.seek(offset+table_at);table=image.file.read(count*12);strings=image.file.read(strings_size)
            require(len(table)==count*12 and len(strings)==strings_size,'PAK_index_truncated')
            containers.append({'format':'PAK','path':name,'ISO_offset':offset,'bytes':size,'members':count,
                               'index_sha256':hashlib.sha256(head[:20]+table+strings).hexdigest()})
            for i in range(count):
                name_at,start,length=struct.unpack_from('<III',table,i*12)
                require(start<=size and length<=size-start,'PAK_member_bounds')
                require(name_at<len(strings),'PAK_member_name_bounds')
                end=strings.find(b'\0',name_at);require(end>=0,'PAK_unterminated_name')
                child=strings[name_at:end].decode('ascii',errors='strict');pak_members+=1
                if length:scan(name+'!'+child,offset+start,length,depth+1)
        elif head[:4]==b' CES':
            require(len(head)>=28,'SEC_header_truncated')
            fields=struct.unpack_from('<7I',head);count,strings_size=fields[4:6];table_end=28+count*16
            if count>20000 or table_end>size:
                limits.append({'path':name,'reason':'SEC_table_variant_unproved','raw_fields':fields});return
            image.file.seek(offset+28);table=image.file.read(count*16)
            require(len(table)==count*16,'SEC_index_truncated')
            local_strings=strings_size<=size-table_end
            strings=image.file.read(strings_size) if local_strings else b''
            container={'format':'SEC','path':name,'ISO_offset':offset,'bytes':size,'members':count,
                       'raw_fields':fields,'local_string_table_bounded':local_strings,
                       'index_sha256':hashlib.sha256(head[:28]+table+strings).hexdigest()}
            containers.append(container)
            for i in range(count):
                kind,start,length,name_at=struct.unpack_from('<IIII',table,i*16)
                if start>size or length>size-start:
                    limits.append({'path':name,'index':i,'reason':'SEC_nonlocal_payload_variant_unproved',
                                   'raw_record':[kind,start,length,name_at]});continue
                child='section_'+str(i);end=strings.find(b'\0',name_at) if name_at<len(strings) else -1
                if end>=0:child+='_'+strings[name_at:end].decode('ascii',errors='replace')
                sections.append({'path':name+'!'+child,'type':kind,'ISO_offset':offset+start,'bytes':length})
                if length:scan(name+'!'+child,offset+start,length,depth+1)
        elif head[:4]==b'AFS\0':
            require(len(head)>=8,'AFS_header_truncated')
            count=struct.unpack_from('<I',head,4)[0]
            require(count<=(size-8)//8,'AFS_index_bounds')
            image.file.seek(offset+8);table=image.file.read(count*8)
            require(len(table)==count*8,'AFS_index_truncated')
            containers.append({'format':'AFS','path':name,'ISO_offset':offset,'bytes':size,'members':count,
                               'index_sha256':hashlib.sha256(head[:8]+table).hexdigest()})
            for i in range(count):
                start,length=struct.unpack_from('<II',table,i*8)
                require(start<=size and length<=size-start,'AFS_member_bounds')
                members+=1
                if length:
                    scan(name+'!AFS_'+str(i),offset+start,length,depth+1)
        else:
            label=head[:4].hex().upper();headers[label]=headers.get(label,0)+1
    try:
        for name,layer,sector,size in files:
            require((layer.index,name) not in names,'duplicate_disc_path')
            names.add((layer.index,name));scan(name,sector*2048,size)
    finally:
        image.file.close()
    after=a.iso.stat()
    require((before.st_size,before.st_mtime_ns)==(after.st_size,after.st_mtime_ns),'original_ISO_changed')
    a.output.mkdir(parents=True)
    result={'status':'READ_ONLY_KNOWN_CONTAINER_CODE_CENSUS','ISO_sha256':ISO_SHA,
            'ISO_bytes':before.st_size,'ISO_mtime_ns':before.st_mtime_ns,'disc_files':len(files),
            'containers':containers,'AFS_members_scanned':members,'PAK_members_scanned':pak_members,
            'SEC_sections':sections,'unparsed_container_limits':limits,'code_inputs':records,
            'other_header_counts':headers,'parser_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
            'script_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
            'limits':['No claim that magic absence proves non-executable data.',
                      'SEC raw types/headers do not establish EE code, script semantics or callability. Other/compressed formats, IOPRP ROMDIR members and code-generating functions have separate domain records.',
                      'No original writes, game launch, ownership or native credit.']}
    output=a.output/'RESULT.json';output.write_text(json.dumps(result,indent=2)+'\n',newline='\n')
    print(json.dumps({'result':str(output),'sha256':hashlib.sha256(output.read_bytes()).hexdigest(),
                      'files':len(files),'AFS_members':members,'PAK_members':pak_members,
                      'SEC_sections':len(sections),'unparsed_limits':len(limits),'code_inputs':len(records)}))

if __name__=='__main__':
    main()
