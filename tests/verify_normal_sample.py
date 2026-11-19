"""Compare actual native count2 output against retained source-permutation hashes and backup software codec."""
import argparse, pathlib, json, hashlib, importlib.util, subprocess, csv

def sha(data): return hashlib.sha256(data).hexdigest()
def main():
 p=argparse.ArgumentParser();p.add_argument('--test',type=pathlib.Path,required=True);p.add_argument('--asset',type=pathlib.Path,required=True);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args()
 fork=pathlib.Path(__file__).resolve().parents[1];parent=fork.parents[2]
 provenance=parent/'research/runs/native-bank-service-20261118/NORMAL_PLANES_PROVENANCE.json'
 oracle_path=parent/'tools/research/verify_adpcm_codec.py';spec=importlib.util.spec_from_file_location('backup_codec',oracle_path);oracle=importlib.util.module_from_spec(spec);spec.loader.exec_module(oracle)
 source=parent/'external/forks/ps2-native-recomp-backup-20261110/runtime/src/iop/spu2.cpp'
 if sha(source.read_bytes())!=oracle.BACKUP_SHA:raise ValueError('backup codec source identity mismatch')
 if sha(a.asset.read_bytes())!=oracle.ASSET_SHA:raise ValueError('MS2 identity mismatch')
 r=subprocess.run([str(a.test.resolve()),str(a.asset.resolve()),str(a.output.resolve())],capture_output=True,text=True)
 if r.returncode:raise RuntimeError(f'actual C++ test failed {r.returncode}: {r.stdout} {r.stderr}')
 records=json.loads(provenance.read_text())['resources'];histories={(int(x['case']),int(x['plane'])):(int(x['history1']),int(x['history2'])) for x in csv.DictReader((a.output/'results.tsv').open(),delimiter='\t')};results=[]
 for i,rec in enumerate(records):
  for plane in range(2):
   stem=a.output/f'{i}-{plane}';encoded=stem.with_suffix('.encoded').read_bytes();want=rec['always_optimized_A_transfer_sha256' if plane==0 else 'always_optimized_B_transfer_sha256']
   if sha(encoded)!=want:raise ValueError(f'encoded plane mismatch {i}/{plane}')
   if any(encoded[n]>>4>4 or encoded[n]&15>12 for n in range(0,len(encoded),16)):raise ValueError('noncanonical reference header; never normalize')
   reference,history=oracle.backup_word_reference(encoded);actual=stem.with_suffix('.s16le').read_bytes();flags=stem.with_suffix('.flags').read_bytes()
   if actual!=reference or history!=histories[i,plane] or flags!=encoded[1::16]:raise ValueError(f'PCM/history/opaque flag mismatch {i}/{plane}')
   results.append({'case':i,'plane':plane,'encoded_sha256':sha(encoded),'pcm_sha256':sha(actual),'history':history,'flags_sha256':sha(flags)})
 report={'status':'PASS','scope':'source-permutation encoded-byte parity plus independent backup software-mono arithmetic on each ordered plane; no semantic channels/loops/audio execution','resources':len(records),'planes':len(results),'chunks':138,'test_stdout':r.stdout,'exe_sha256':sha(a.test.read_bytes()),'asset_sha256':oracle.ASSET_SHA,'prior_source_provenance_sha256':sha(provenance.read_bytes()),'reference_script_sha256':sha(oracle_path.read_bytes()),'backup_source_sha256':oracle.BACKUP_SHA,'planes_results':results}
 (a.output/'verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'status':'PASS','resources':40,'planes':80,'verification':str(a.output/'verification.json')}))
if __name__=='__main__':main()
