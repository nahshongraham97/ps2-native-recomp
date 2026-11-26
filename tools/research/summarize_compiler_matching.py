"""Publish bounded compiler experiment metadata, never binaries or game C."""
from pathlib import Path
from fractions import Fraction
from collections import Counter,defaultdict
import argparse
import csv
import hashlib
import json

ROOT=Path(__file__).resolve().parents[2]

def sha(path):
    with path.open('rb')as stream:return hashlib.file_digest(stream,'sha256').hexdigest()

def load(path):
    return json.loads(path.read_bytes())

def write_csv(path,fields,rows):
    with path.open('w',encoding='utf8',newline='')as stream:
        writer=csv.DictWriter(stream,fields,lineterminator='\n',extrasaction='ignore')
        writer.writeheader();writer.writerows(rows)
    return dict(path=path.relative_to(ROOT).as_posix(),sha256=sha(path),rows=len(rows))

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--private-root',type=Path,required=True)
    parser.add_argument('--after-permuter',action='store_true')
    parser.add_argument('--preview',action='store_true')
    args=parser.parse_args()
    private=args.private_root.resolve()
    assert private.is_relative_to(ROOT/'build/oracle-telemetry')
    primary=load(private/'PRIMARY_MATRIX.json')
    matrix=load(private/'AFTER_PERMUTER_MATRIX.json')if args.after_permuter else primary
    assert primary['status']==matrix['status']=='COMPLETE'
    assert len(matrix['results'])==152
    sample=load(private/'REVISED_SAMPLE.json')
    assert sample['total']==51 and sample['threshold_denominator']==40
    expected={(row['module'],row['address'])for row in sample['sample']}
    inventory=load(private/'setup-track-b/TOOLCHAIN_INVENTORY.json')
    identity=load(private/'compiler-identification.json')
    controls=load(private/'strict-controls-final-02/RESULT.json')
    assert controls['status']=='PASS'and controls['count']==controls['passed']==38
    cells=[];function_rows=[];by_function=defaultdict(list);best_rows={}
    for index,cell in enumerate(matrix['results']):
        assert {(row['module'],row['address'])for row in cell['rows']}==expected
        statuses=Counter(row['status']for row in cell['rows'])
        cells.append(dict(compiler=cell['compiler'],flags=cell['flags'],g=cell['g'],
            exact_all51=cell['exact'],exact_nontrivial40=cell['nontrivial_exact'],
            total=51,threshold_total=40,compile_errors=statuses['COMPILER_ERROR'],
            draft_refusals=statuses['DRAFT_REFUSED'],checker_refusals=statuses['CHECK_REFUSED'],
            unavailable=statuses['COMPILER_UNAVAILABLE'],mismatches=statuses['MISMATCH']))
        for row in cell['rows']:
            key=(row['module'],row['address'])
            value=dict(row,compiler=cell['compiler'],flags=cell['flags'],g=cell['g'],
                       reason=(str(row.get('reason','')).splitlines()or[''])[0][:350])
            function_rows.append(value);by_function[key].append(value)
            if 'kit_matched'in row:
                score=Fraction(row['kit_matched'],max(1,row['kit_target'],row['kit_candidate']))
                if key not in best_rows or score>best_rows[key][0]:
                    best_rows[key]=(score,value)
    output=private/'preview'if args.preview else ROOT/'research/runs'
    output.mkdir(exist_ok=True)
    tables={}
    tables['matrix']=write_csv(output/'compiler_matching_matrix.csv',list(cells[0]),cells)
    fields=['compiler','flags','g','module','address','instructions','status','observation',
            'observed_in_matched_replay','source_variant','draft_sha256','original_sha256',
            'candidate_object_sha256','candidate_resolved_sha256','kit_matched','kit_target',
            'kit_candidate','reason']
    tables['functions']=write_csv(output/'compiler_matching_functions.csv',fields,function_rows)
    styles=identity['callable_style_heuristics']
    tables['styles']=write_csv(output/'compiler_style_observations.csv',
        ['key','module','entry_alignment_bytes','branch_likely_count','partition','compiler_attribution',
         'instruction_count','original_body_sha256','body_join'],styles['functions'])
    exact=[]
    per_function=[];common_cells=None;common_builds=None
    for key,rows in sorted(by_function.items()):
        wins=[row for row in rows if row['status']=='BYTE_EXACT']
        if wins:
            exact.append(dict(module=key[0],address=key[1],instructions=rows[0]['instructions'],
                observation=rows[0]['observation'],observed_in_matched_replay=rows[0]['observed_in_matched_replay'],
                matching_cells=[dict(compiler=row['compiler'],flags=row['flags'],g=row['g'])for row in wins]))
        scored=[row for row in rows if 'kit_matched'in row]
        if not scored:
            per_function.append(dict(module=key[0],address=key[1],comparison='NO_COMPARABLE_COMPILED_DRAFT',
                                     statuses=dict(Counter(row['status']for row in rows))))
            continue
        best=max(Fraction(row['kit_matched'],max(1,row['kit_target'],row['kit_candidate']))for row in scored)
        winners=[row for row in scored if Fraction(row['kit_matched'],max(1,row['kit_target'],row['kit_candidate']))==best]
        winner_cells={(row['compiler'],row['flags'],row['g'])for row in winners}
        winner_builds={row['compiler']for row in winners}
        common_cells=winner_cells if common_cells is None else common_cells&winner_cells
        common_builds=winner_builds if common_builds is None else common_builds&winner_builds
        per_function.append(dict(module=key[0],address=key[1],comparison='KIT_INSTRUCTION_FIT_NOT_EQUIVALENCE',
            best_fraction=str(best),first_or_tied_cells=[dict(compiler=c,flags=f,g=g)for c,f,g in sorted(winner_cells)]))
    compiler_aliases=defaultdict(list)
    for compiler in inventory['compilers']:
        if compiler['family']=='mwcps2':
            compiler_aliases[compiler['driver']['sha256']].append(compiler['version'])
    body_data={}
    for key,(score,row)in best_rows.items():
        if row['status']!='BYTE_EXACT':
            body_data[key[0]+':'+key[1]]=dict(compiler=row['compiler'],flags=row['flags'],g=row['g'],
                status=row['status'],kit_fraction=str(score),kit_diff=row.get('instruction_diff',[]),
                raw_word_diff=row.get('raw_instruction_diff',[]),limit=row.get('reason',''))
    best=max(cells,key=lambda row:(row['exact_nontrivial40'],row['exact_all51']))
    rate=Fraction(best['exact_nontrivial40'],40)
    verdict='GO'if rate>=Fraction(1,2)else'PILOT'if rate>=Fraction(1,10)else'NO_GO_UNDER_TESTED_DRAFT_AND_TOOL_CONFIGURATION'
    compact_identity={key:value for key,value in identity.items()if key!='callable_style_heuristics'}
    compact_identity['callable_style_heuristics']={key:value for key,value in styles.items()if key!='functions'}
    result=dict(status='COMPLETE'if args.after_permuter else'PRIMARY_COMPLETE_PERMUTER_IN_PROGRESS',
        inputs=dict(primary_matrix_sha256=sha(private/'PRIMARY_MATRIX.json'),
                    sample_sha256=sha(private/'REVISED_SAMPLE.json'),identity_sha256=sha(private/'compiler-identification.json')),
        sample=sample,toolkit=dict(origin='https://github.com/kira-502/decomp-kit',
            commit='3ae9a231d887f187617e25756c7fa866703fa5a1',license='CC0-1.0 for kit source only'),
        compilers=dict(installed_labels=inventory['compiler_counts'],working_MW_labels=26,working_EE_labels=11,
            unavailable_EE=['3.2-030210-beta2'],unique_MW_driver_hashes=len(compiler_aliases),
            alias_labels=[labels for labels in compiler_aliases.values()if len(labels)>1],
            identities=[dict(family=row['family'],version=row['version'],driver=row['driver'],
                version_probe_exit=row['version_smoke']['exit_code'])for row in inventory['compilers']],
            inventory_sha256=sha(private/'setup-track-b/TOOLCHAIN_INVENTORY.json')),
        identity=compact_identity,negative_controls=dict(sha256=sha(private/'strict-controls-final-02/RESULT.json'),result=controls),
        flag_probes=load(private/'FLAG_PROBES.json'),harvest=load(private/'HARVEST.json'),
        tables=tables,best_cell=best,exact_functions=exact,ranking_per_function=per_function,
        first_or_tied_on_every_comparable_function=dict(builds=sorted(common_builds or []),cells=sorted(common_cells or []),
            limits='Ranks fixed draft instruction fit; neither mixed winners nor no common winner proves original mixed toolchains. Uncompiled/refused functions cannot rank.'),
        closest_unmatched_diffs=body_data,
        verified_never_executed_exact_functions=0,
        not_observed_exact_functions=[row for row in exact if row['observation'].startswith('not-observed')],
        threshold_verdict=verdict if args.after_permuter else 'PENDING_PERMUTER_PRIMARY_RATE_BELOW_THRESHOLD',
        threshold_rate=f"{best['exact_nontrivial40']}/40",
        scope_limits=['Exact means full untrimmed function bytes after supported original-address relocation, not normalized objdiff score.',
            'Defined/local-section placement, unproved overlay GP and unsupported relocations remain CHECK_REFUSED, not compiler mismatches.',
            'Incomplete draft types, missing split-body/jump-table recovery and checker limits confound original compiler identification; a bounded no-go is not proof all matching approaches impossible.',
            'No native pointer-width, host ABI, portable-C defined behavior, ownership or production replacement is admitted.',
            'No replay/oracle execution or grinder; native component baseline is separate.'],
        deltas=dict(ownership=0,transformed=0,native=0),native_acceptance='OPEN')
    if args.after_permuter:
        result['permuter']=load(private/'PERMUTER.json')
        result['inputs']['after_permuter_matrix_sha256']=sha(private/'AFTER_PERMUTER_MATRIX.json')
    if args.preview:
        (output/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8',newline='\n')
    else:
        path=ROOT/'research/runs/renderer-scope.json'
        scope=load(path);scope['compiler_matching_feasibility_20261126']=result
        path.write_text(json.dumps(scope,indent=2)+'\n',encoding='utf8',newline='\n')
    print(json.dumps(dict(status=result['status'],cells=len(cells),exact_functions=len(exact),
        best_cell=best,threshold_verdict=result['threshold_verdict'],unique_MW_driver_hashes=len(compiler_aliases),tables=tables)))

if __name__=='__main__':
    main()
