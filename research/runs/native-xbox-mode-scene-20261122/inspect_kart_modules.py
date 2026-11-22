"""Save original selected module byte slices privately and inspect their bounded data."""
from pathlib import Path
import hashlib,json,re,struct
RECORD=Path(__file__).resolve().parent;ROOT=RECORD.parents[2];PARENT=ROOT.parents[2]
image=(PARENT/"private-inputs/xbox-mka-20261121/default.xbe").read_bytes()
identity=json.loads((RECORD/"MODE_ANCHORS.json").read_text())
assert hashlib.sha256(image).hexdigest()==identity["xbe_sha256"]
container_path=PARENT/"private-inputs/Mortal Kombat - Armageddon (USA).xiso/art/kart.ssf"
container=container_path.read_bytes()
private=RECORD/"private-kart-modules/attempt-02";private.mkdir(parents=True,exist_ok=False)
out=[]
for descriptor,name in [(0x55ab68,"kart_core.mko"),(0x55ab74,"kart_fx.mko")]:
    needle=struct.pack("<I",descriptor);cursor=0;found=[]
    for s in identity["sections"]:
        if not s["virtual_address"]<=0x4ed6c8<s["virtual_address"]+s["file_size"]:continue
        start=s["file_offset"];end=start+s["file_size"];cursor=start
        while True:
            at=image.find(needle,cursor,end)
            if at<0:break
            cursor=at+1
            if at+12>end:continue
            pointer,offset,length=struct.unpack_from("<3I",image,at)
            if length<32 or offset+length>len(container):continue
            raw=container[offset:offset+length]
            strings=[m.group().decode("ascii") for m in re.finditer(rb"[ -~]{5,}",raw)]
            strings=[v for v in strings if re.search("profil|kart|control|stat|player|pad|race",v,re.I)]
            entry=dict(descriptor=hex(descriptor),table_entry_vma=hex(s["virtual_address"]+at-start),
                table_entry_file_offset=at,table_entry_bytes_hex=image[at:at+12].hex(),
                container_offset=offset,length=length,sha256=hashlib.sha256(raw).hexdigest(),
                header_words=list(struct.unpack_from("<8I",raw)),matching_ascii=strings)
            found.append(entry)
    assert len(found)==1,(name,len(found))
    entry=found[0];raw=container[entry["container_offset"]:entry["container_offset"]+entry["length"]]
    with (private/name).open("xb") as f:f.write(raw)
    out.append(dict(name=name,**entry))
    print(name,"table",entry["table_entry_vma"],"offset",entry["container_offset"],"length",entry["length"],
          "header",entry["header_words"],"strings",entry["matching_ascii"][:35],flush=True)
with (RECORD/"KART_MODULE_SLICES.json").open("x",newline="\n") as f:
    json.dump(dict(xbe_sha256=identity["xbe_sha256"],container_path=str(container_path),
        container_size=len(container),container_sha256=hashlib.sha256(container).hexdigest(),
        modules=out,scope="Original table byte slices; no module execution and no assumed ISA or runtime ABI"),f,indent=2);f.write("\n")
