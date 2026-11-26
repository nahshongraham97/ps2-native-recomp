"""Read-only join of ALL configured captured caches/logs to the sealed package.

Recordings are not a selection rule. No DLL is loaded and no game is started.
Unknown custom roots, missing original bytes and unrepresented entries stay open.
"""
from pathlib import Path
from collections import Counter,defaultdict
import argparse,hashlib,json,re,struct,sys
from group_r192_emitted_families import records,digest

ROOT=Path(__file__).resolve().parents[2]
P=ROOT.parents[2]
R=P/'external/forks/ps2-native-recomp/build/r192-hooks'
T=ROOT/'build/oracle-telemetry'

def pin(q):return dict(path=str(q.resolve()),size=q.stat().st_size,sha256=digest(q))
def need(ok,why):
    if not ok:raise ValueError(why)

def vu_records(path):
    active=False;parts=[];seen=0
    with path.open(encoding='utf8') as stream:
        for line in stream:
            if line.startswith('  "VU": ['):active=True;continue
            if active and line.startswith('  ]'):break
            if active and line.startswith('    {'):parts=[line];continue
            if parts:
                parts.append(line)
                if line.startswith('    }'):
                    yield json.loads(''.join(parts).strip().rstrip(','));parts=[];seen+=1
    need(active and not parts and seen==48,'VU_metadata_denominator_changed')

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args();out=a.output.resolve()
    need(out.is_relative_to(T) and not out.exists(),'fresh_owned_output_required')
    meta=T/'r192-cache-harvest-01/METADATA-03.json';package=T/'r192-cache-harvest-01/PACKAGE-03.json'
    host=R/'src/host/main.cpp';base=ROOT/'build/r192-resource-event-candidate-04-final/BUILD.json'
    deps=[Path(__file__),Path(__file__).with_name('group_r192_emitted_families.py'),meta,package,host,base]
    pins=[pin(q) for q in deps];frozen=json.loads(package.read_text());build=json.loads(base.read_text());sealed={}
    for domain in ('ee','vu'):
        q=Path(build['candidate']).parent/'aot-modules'/domain/'manifest.txt';pins.append(pin(q));lines=q.read_text().splitlines();sealed[domain]={line.split()[0] for line in lines[1:] if line.strip()}
    normal=ROOT/'build/r192-compiled-konquest/out/aot-modules/ee/manifest.txt';pins.append(pin(normal));need(digest(normal)==pins[-3]['sha256'],'installed_EE_manifest_differs_from_audited_package')
    # Old Live VU DLLs have a different ABI/source stamp from the offline
    # source-recompiled package. Compare original program keys, not DLL hashes.
    generation=T/'r192-vu-all-uploaded-generation-01/RESULT.json';pins.append(pin(generation));g=json.loads(generation.read_text());keys=set();manifest_lines=(Path(build['candidate']).parent/'aot-modules/vu/manifest.txt').read_text().splitlines()[1:]
    module_keys={row.split()[-1]:row.split()[1] for row in manifest_lines}
    for row in g['records']:
        q=Path(row['entries']);need(digest(q)==row['entries_sha256'],'original_VU_key_list_changed');pins.append(pin(q));values=sorted(int(line.split()[0],16) for line in q.read_text().splitlines());name='vu-%08x.dll'%row['original_file_offset']
        need(hashlib.sha256(b''.join(struct.pack('<Q',v) for v in values)).hexdigest()==module_keys[name],'sealed_VU_key_digest_changed');keys.update(values)
    old_keys={int(p['key'],16) for row in vu_records(meta) for p in row['programs']};need(len(old_keys)==217,'old_VU_key_denominator_changed');missing_vu_keys=sorted(old_keys-keys)
    artifact_by_hash={x['sha256']:x for x in frozen['artifacts']};entries=defaultdict(set);module_rows=Counter();entry_total=0
    for record in records(meta):
        need(record['artifact']['sha256'] in artifact_by_hash,'metadata_artifact_missing_from_package')
        need(record['artifact']['sha256'] in sealed['ee'],'metadata_DLL_not_sealed')
        for m in record['modules']:
            key=(m['name'],m['fixed_base']);module_rows[key]+=1
            for u in m['units']:
                for e in u['entries']:entries[key].add(e['offset']);entry_total+=1
    # Roots follow original host/main.cpp and the actual startup logs. Never
    # discover caches by walking unrelated user folders or model directories.
    roots={R/'game/ee-live':'ee',R/'game/vu1-live':'vu',R/'game/vu1':'vu-source',R/'game/overlays':'image-source'}
    logs=sorted((R/'game/disc/logs').glob('*.log'))
    logs+=sorted((ROOT/'build/r192-compiled-konquest/logs').glob('*.log'))
    unscanned=[];events=[];log_rows=[];counts=Counter()
    miss=re.compile(r'\[ee-live[^\]]*\bf(\d+)\]\s+no translated code at 0x([0-9a-fA-F]+) \(loaded code at 0x([0-9a-fA-F]+), image ([A-Za-z0-9_]+),')
    root_re=re.compile(r'\[(ee|vu)-live[^\]]*\].*(?:files in |, into )(.+)$')
    for q in logs:
        before=q.stat();data=q.read_bytes();row=pin(q);need(before.st_size==q.stat().st_size and before.st_mtime_ns==q.stat().st_mtime_ns,'log_changed_during_read')
        text=data.decode(errors='replace');restored=False;first=None;byimage=Counter();cold=Counter();n=0
        for line in text.splitlines():
            found=root_re.search(line)
            if found:
                path=Path(found[2].strip()).resolve();domain='ee' if found[1]=='ee' else 'vu'
                if path.is_relative_to(P) and not any(x.lower() in ('models','lmstudio','.lmstudio') for x in path.parts):roots[path]=domain
                else:unscanned.append(dict(log=str(q),root=str(path),reason='outside_explicit_project_cache_scope'))
            if '[state] loaded frame' in line:restored=True
            m=miss.search(line)
            if not m:continue
            frame=int(m[1]);pc=int(m[2],16);loaded=int(m[3],16);name=m[4];offset=pc-loaded
            candidate_keys=[(name,loaded),('main_'+name,loaded)]
            matching=[key for key in candidate_keys if offset in entries.get(key,())]
            represented=bool(matching);reason='compiled_exported_entry' if represented else 'NO_MATCHING_EXPORTED_ENTRY'
            events.append(dict(log=str(q),frame=frame,pc=f'{pc:08X}',loaded_at=f'{loaded:08X}',image=name,relative_offset=offset,pre_first_state_load=not restored,sealed_entry=represented,join=reason))
            byimage[name]+=1;n+=1
            if not restored:cold[name]+=1
            if first is None:first=dict(frame=frame,pc=f'{pc:08X}',image=name,pre_first_state_load=not restored)
        counts['miss_observations']+=n;counts['unrepresented_miss_observations']+=sum(not e['sealed_entry'] for e in events[-n:]) if n else 0
        log_rows.append({**row,'miss_observations':n,'unique_images':len(byimage),'images':dict(sorted(byimage.items())),'before_first_state_load_images':dict(sorted(cold.items())),'first_miss':first})
    files=[];missing=[];root_rows=[]
    for folder,domain in sorted(roots.items()):
        found=list(sorted(q for q in folder.rglob('*') if q.is_file())) if folder.is_dir() else []
        root_rows.append(dict(path=str(folder),domain=domain,exists=folder.is_dir(),files=len(found)))
        for q in found:
            before=q.stat();row=pin(q);need(before.st_size==q.stat().st_size and before.st_mtime_ns==q.stat().st_mtime_ns,'cache_changed_during_read')
            row.update(domain=domain,relative_to_root=str(q.relative_to(folder)))
            if q.suffix.lower()=='.dll' and domain in ('ee','vu'):
                row['sealed_exact_bytes']=row['sha256'] in sealed[domain]
                if not row['sealed_exact_bytes']:missing.append(row)
            files.append(row)
    changes=[x['path'] for x in pins if digest(Path(x['path']))!=x['sha256']]
    out.mkdir();detail=out/'LOG_ENTRIES.json';detail.write_text(json.dumps(events,indent=2)+'\n',newline='\n')
    manifest=out/'CACHE_FILES.json';manifest.write_text(json.dumps(files,indent=2)+'\n',newline='\n')
    result=dict(status='ALL_CONFIGURED_CACHES_AUDITED' if not changes else 'INPUT_CHANGED',invocation=[sys.executable,*sys.argv],source_pins=pins,input_changes=changes,roots=root_rows,unscanned_roots=unscanned,summary={**counts,'retained_original_R192_session_logs':sum(Path(r['path']).parent==R/'game/disc/logs' for r in log_rows),'current_launcher_log_files':sum(Path(r['path']).parent!=R/'game/disc/logs' for r in log_rows),'logs':len(log_rows),'cache_files':len(files),'EE_metadata_DLLs':3569,'EE_unique_module_names':len({k[0] for k in entries}),'EE_entry_records':entry_total,'EE_DLLs_not_sealed':sum(r['domain']=='ee' for r in missing),'old_VU_DLLs_recompiled_under_new_ABI':sum(r['domain']=='vu' for r in missing),'old_VU_program_keys':len(old_keys),'old_VU_program_keys_in_offline_package':len(old_keys&keys),'old_VU_program_keys_missing':missing_vu_keys,'replay_filter_used':False},logs=log_rows,nonidentical_DLLs=missing,details=[pin(detail),pin(manifest)],limits=['Pre-first-state-load is a log prefix classification, not proof of a cold boot.','Retained log-file count is not a lifetime launch count; the current launcher log is overwritten.','Existing original cache artifacts/entries are joined regardless of recording coverage; unplayed parameter variants still require source-defined family proof.','Word metadata authenticates sparse compiled entry checks; it does not prove all words in an image or equivalence of an arbitrary image variant.','VU original-key inclusion is source identity/entry coverage, not numerical equivalence or proof of all dynamic uploads.','Logs cannot reveal configured cache roots for sessions that left no log; outside-project roots are not silently enumerated.','No game/card/state/launcher writes or DLL execution. No whole-game or native credit.'])
    q=out/'RESULT.json';q.write_text(json.dumps(result,indent=2)+'\n',newline='\n');print(json.dumps(dict(status=result['status'],summary=result['summary'],unscanned_roots=unscanned,sha256=digest(q))))
    return 0 if not changes else 2

if __name__=='__main__':raise SystemExit(main())
