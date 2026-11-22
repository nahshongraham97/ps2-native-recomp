"""Read only original selected SSFs; inventory bounded SEC candidates, never execute modules."""
from pathlib import Path
import hashlib,json,struct
RECORD=Path(__file__).resolve().parent;ROOT=RECORD.parents[2];PARENT=ROOT.parents[2]
inputs=[
 ("xbox","kart.ssf",PARENT/"private-inputs/Mortal Kombat - Armageddon (USA).xiso/art/kart.ssf"),
 ("xbox","gameart.ssf",PARENT/"private-inputs/Mortal Kombat - Armageddon (USA).xiso/art/gameart.ssf"),
 ("ps2","kart.ssf",PARENT/"private-inputs/native-ps2-loose-20261121/art/kart.ssf")]
out=[]
for platform,name,path in inputs:
    data=path.read_bytes();sections=[];cursor=0
    while True:
        start=data.find(b" CES",cursor)
        if start<0:break
        cursor=start+4
        if start+28>len(data):continue
        header=struct.unpack_from("<7I",data,start);count=header[4]
        if count>10000 or start+28+count*16>len(data):continue
        rows=[struct.unpack_from("<4I",data,start+28+i*16) for i in range(count)]
        table_end=28+count*16
        nonzero=[r[1] for r in rows if r[1]]
        if not nonzero or min(nonzero)<table_end:continue
        names_end=min(nonzero);names_begin=start+table_end;matches=[];valid=True
        for index,row in enumerate(rows):
            at=names_begin+row[3]
            if at>=start+names_end or at>=len(data):valid=False;break
            end=data.find(b"\0",at,min(start+names_end,len(data)))
            if end<0:valid=False;break
            text=data[at:end]
            if any(v<32 or v>126 for v in text):valid=False;break
            name_text=text.decode("ascii")
            if "kart" in name_text.lower():
                payload=start+row[1]
                if not row[1] or payload+32>len(data):valid=False;break
                matches.append(dict(index=index,name=name_text,record_words=list(row),
                    payload_file_offset=payload,payload_head_hex=data[payload:payload+64].hex()))
        if valid and matches:
            sections.append(dict(section_file_offset=start,header_words=list(header),resource_count=count,matches=matches))
    entry=dict(platform=platform,name=name,path=str(path),size=len(data),sha256=hashlib.sha256(data).hexdigest(),sections=sections)
    out.append(entry)
    print(platform,name,"sections",[(s["section_file_offset"],s["resource_count"],[(r["name"],r["payload_file_offset"],r["payload_head_hex"][:64]) for r in s["matches"]]) for s in sections],flush=True)
with (RECORD/"KART_RESOURCE_HEADERS.json").open("x",newline="\n") as f:
    json.dump(dict(status="BOUNDED_ORIGINAL_SEC_CANDIDATES_NOT_RUNTIME_ADOPTION",inputs=out,
        scope="Source-derived SEC directory shape applied read-only; candidate section bounds and module ABI still require source proof"),f,indent=2);f.write("\n")
