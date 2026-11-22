"""Read the actual XBE descriptor words and bounded pointed strings before consumer inspection."""
from pathlib import Path
import hashlib,json,struct
RECORD=Path(__file__).resolve().parent;ROOT=RECORD.parents[2];PARENT=ROOT.parents[2]
image=(PARENT/"private-inputs/xbox-mka-20261121/default.xbe").read_bytes()
identity=json.loads((RECORD/"MODE_ANCHORS.json").read_text())
assert hashlib.sha256(image).hexdigest()==identity["xbe_sha256"]
def mapped(at,n):
    matches=[s for s in identity["sections"] if s["virtual_address"]<=at and at+n<=s["virtual_address"]+s["file_size"]]
    assert len(matches)==1
    s=matches[0];offset=s["file_offset"]+at-s["virtual_address"]
    return image[offset:offset+n],dict(address=hex(at),file_offset=offset,length=n,section=s["name"])
rows=[]
for at in [0x55ab68,0x55ab74,0x55abe0,0x4ed6c8]:
    data,mapping=mapped(at,48);words=struct.unpack("<12I",data);strings=[]
    for index,word in enumerate(words):
        try:raw,m=mapped(word,128)
        except AssertionError:continue
        value=raw.split(b"\0",1)[0]
        if value and all(32<=c<127 for c in value):strings.append(dict(word_index=index,mapping=m,text=value.decode()))
    rows.append(dict(mapping=mapping,bytes_hex=data.hex(),words=list(words),pointed_strings=strings))
    print(hex(at),[hex(v) for v in words],[(s["word_index"],s["text"]) for s in strings],flush=True)
with (RECORD/"KART_DESCRIPTORS.json").open("x",newline="\n") as f:
    json.dump(dict(xbe_sha256=identity["xbe_sha256"],rows=rows,scope="Original bytes; descriptor meanings require1B04C0 consumer"),f,indent=2);f.write("\n")
