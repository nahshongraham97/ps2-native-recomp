"""Recheck original metadata and preserve frozen/raw versus Git newline identities."""
from pathlib import Path
import hashlib
import json
import re
import struct
import subprocess
import sys

record=Path(__file__).resolve().parent
fork=record.parents[2]
source_record=fork/'research/runs/parallel-sound-parameters-source-20261122'
script=source_record/'join.py'
def digest(raw):return hashlib.sha256(raw).hexdigest()
proof=source_record/'SOURCE_JOIN.json'
original=proof.read_bytes()
saved=json.loads(original)
prefix=script.read_text().split("p=r/'SOURCE_JOIN.json';render=",1)[0]
assert prefix!=script.read_text() and 'write_text' not in prefix and 'write_bytes' not in prefix
namespace={'__file__':str(script),'__name__':'readonly_source_recheck'}
exec(compile(prefix,str(script),'exec'),namespace)
current=json.loads(json.dumps(namespace['value']))
identities={}
for name,expected in saved['ps2']['reused_evidence'].items():
    path=fork/name
    raw=path.read_bytes()
    blob=subprocess.check_output(['git','show','HEAD:'+name.replace('\\','/')],cwd=fork)
    if digest(blob)==expected:
        saved_form='committed-LF'
    else:
        assert b'\r' not in blob and digest(blob.replace(b'\n',b'\r\n'))==expected,name
        saved_form='private-checkout-CRLF'
    assert raw.replace(b'\r\n',b'\n')==blob,name
    assert current['ps2']['reused_evidence'][name]==digest(raw)
    identities[name]=dict(saved_sha256=expected,committed_sha256=digest(blob),
        canonical_sha256=digest(raw),canonical_crlf_pairs=raw.count(b'\r\n'),
        saved_form=saved_form,
        identity='byte-identical' if raw==blob else 'canonical-CRLF-to-committed-LF-only')
current['ps2']['reused_evidence']=saved['ps2']['reused_evidence']
assert current==saved
words=struct.unpack('<9656I',bytes.fromhex(saved['ps2']['data']['sound_parameters']['bytes']))
categories=struct.unpack('<78I',bytes.fromhex(saved['ps2']['data']['categories']['bytes']))
generated=fork/'runtime/src/native/retail_sound_metadata_data.inc'
text=generated.read_text()
literals=[int(x,16) for x in re.findall(r'0x([0-9a-f]{8})u',text)]
assert literals[:9656]==list(words) and literals[9656:]==list(categories[::2])
gain_indices=[int(x) for x in re.findall(r'\{0x[0-9a-f]{8}u,([0-9]+)\}',text)]
assert gain_indices==[x&255 for x in categories[1::2]]
corpus=fork/'research/runs/parallel-sound-metadata-resolver-20261122/expected-metadata.tsv'
rows=corpus.read_text().splitlines()[1:]
assert len(rows)==9656
for index,row in enumerate(rows):
    word=words[index];category=word>>25;output=(word>>20)&7
    output=output if output<4 else output-8
    assert list(map(int,row.split()))==[index,word&1023,(word>>13)&127,category,
        categories[category*2+1]&255,(word>>23)&3,output,categories[category*2]]
assert proof.read_bytes()==original
result=dict(status='PASS_ORIGINAL_METADATA_AND_NEWLINE_IDENTITIES',proof_sha256=digest(original),
    source_helper_sha256=digest(script.read_bytes()),reused_evidence_identities=identities,
    generated_data_sha256=digest(generated.read_bytes()),expected_corpus_sha256=digest(corpus.read_bytes()),
    original_word_data_unchanged=True,words=9656,categories=39,rows_checked=9656,
    note='Original joins/data equal frozen proof; only3 prior raw reference hashes require verified newline identity mapping',
    game_run=False,physical_endpoints_opened=False,frames=[],audio=[],acceptance='OPEN')
output=record/'SOURCE_RECHECK.json'
assert not output.exists()
output.write_text(json.dumps(result,indent=2)+'\n',newline='\n')
print(result['status'],len(identities),'prior identities;9656 records; original proof preserved')
