"""Join read-only candidate-code observations to exact completed replay receipts.

Counts are workload invocations and descriptor diagnostic keys, never a whole
producer census, function count, execution admission or native credit.
"""
from pathlib import Path
import argparse
import json
import re
import sys

from verify_r192_resource_word_equality import digest

ROOT=Path(__file__).resolve().parents[2]
PREFIX='[resource-code-observation] '
SUMMARY=re.compile(r'summary entries=(\d+) matched_invocations=(\d+) matched_signatures=(\d+) total_signatures=(\d+) nonentry_invocations=(\d+) diagnostic_errors=(\d+) reset_hooks=(\d+) runtime_admission=(\d+)')
NAMES=['entries','matched_invocations','matched_signatures','diagnostic_keys','nonentry_invocations','diagnostic_errors','reset_hooks','runtime_admission']


def require(ok,reason):
    if not ok:raise ValueError(reason)


def parse(text):
    rows=[];summaries=[]
    for line in text.splitlines():
        if not line.startswith(PREFIX):continue
        value=line[len(PREFIX):]
        if value.startswith('summary '):
            m=SUMMARY.fullmatch(value);require(m is not None,'malformed_summary');summaries.append(dict(zip(NAMES,map(int,m.groups()))))
        else:
            row=json.loads(value);require(isinstance(row,dict),'observation_row_type')
            require(type(row.get('calls')) is int and row['calls']>0,'calls_type_or_range')
            require(type(row.get('equal')) is bool and type(row.get('intact')) is bool,'flags_type')
            require(type(row.get('code_words')) is int and row['code_words']>=0,'code_count_type')
            require(type(row.get('reason')) is str and type(row.get('candidate_signature')) is str,'signature_or_reason_type');rows.append(row)
    require(len(summaries)==1,'one_observer_summary_required');s=summaries[0]
    require(s['diagnostic_errors']==s['runtime_admission']==0,'observation_error_or_admission')
    require(len({r['candidate_signature'] for r in rows})==len(rows),'duplicate_diagnostic_key')
    require(s['entries']==sum(r['calls'] for r in rows),'entry_denominator_mismatch')
    require(s['matched_invocations']==sum(r['calls'] for r in rows if r['equal']),'matched_denominator_mismatch')
    require(s['matched_signatures']==sum(r['equal'] for r in rows) and s['diagnostic_keys']==len(rows),'signature_denominator_mismatch')
    return dict(summary=s,rows=rows)


def self_test():
    row=dict(calls=2,equal=True,intact=True,code_words=17,reason='code_identity_only',candidate_signature='a1:e:f|code_identity_only')
    good=PREFIX+json.dumps(row)+'\n'+PREFIX+'summary entries=2 matched_invocations=2 matched_signatures=1 total_signatures=1 nonentry_invocations=0 diagnostic_errors=0 reset_hooks=2 runtime_admission=0\n'
    require(parse(good)['summary']['entries']==2,'positive_summary_failed');rejected=[]
    for name,text in [('missing_row',good.split('\n',1)[1]),('forged_count',good.replace('entries=2','entries=3')),('duplicate_row',PREFIX+json.dumps(row)+'\n'+good),('diagnostic_error',good.replace('diagnostic_errors=0','diagnostic_errors=1')),('invented_admission',good.replace('runtime_admission=0','runtime_admission=1')),('bad_row_type',good.replace('"calls": 2','"calls": true'))]:
        try:parse(text)
        except (ValueError,TypeError):rejected.append(name)
        else:raise ValueError('summary_plant_accepted:'+name)
    return dict(positive=True,rejected=rejected)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['probes-report','full-report','build-receipt','output']:p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--expected-binary-sha',required=True);a=p.parse_args();out=a.output.resolve()
    require(out.is_relative_to(ROOT/'build/oracle-telemetry') and not out.exists(),'fresh_owned_output_required')
    before={}
    def pin(q,expected=None):
        q=Path(q).resolve();actual=digest(q);require(expected is None or actual==expected,'input_pin_changed:'+str(q));require(str(q) not in before or before[str(q)]==actual,'input_identity_rebound');before[str(q)]=actual;return q
    pin(Path(__file__));build=json.loads(pin(a.build_receipt).read_text());require(build['status']=='READ_ONLY_RESOURCE_CODE_OBSERVER_BUILT' and not build['input_changes'],'completed_read_only_build_required')
    require(build['candidate_sha256']==a.expected_binary_sha,'candidate_build_mismatch');pin(build['candidate'],a.expected_binary_sha)
    for row in build['source_pins']+build['private_sources']+build['private_objects']+build['package_pins']:pin(row['path'],row['sha256'])
    probes=json.loads(pin(a.probes_report).read_text());full=json.loads(pin(a.full_report).read_text());require(probes['status']=='PASS' and probes['passed_cases']==probes['total_cases']==len(probes['cases'])==14,'fourteen_completed_cases_required');require(probes['candidate_sha256']==a.expected_binary_sha,'probe_candidate_mismatch')
    require(full['status']=='PASS' and full['mode']=='replay' and full['measured_new_frames']==22785 and full['candidate']['sha256']==a.expected_binary_sha,'complete_recorded_replay_required')
    cases=[];union={}
    def case(name,path,expected=None):
        q=pin(path,expected);r=json.loads(q.read_text());require(r['status']=='PASS' and r['candidate']['sha256']==a.expected_binary_sha,'case_not_same_completed_candidate')
        for row in r['log_pins']:pin(row['path'],row['sha256'])
        log=next(Path(row['path']) for row in r['log_pins'] if Path(row['path']).name=='stderr.log');parsed=parse(log.read_text(errors='strict'))
        for row in parsed['rows']:
            key=row['candidate_signature'];union.setdefault(key,dict(equal=row['equal'],reason=row['reason'],code_words=row['code_words'],case_names=[]));require(union[key]['equal']==row['equal'] and union[key]['code_words']==row['code_words'],'diagnostic_key_meaning_changed');union[key]['case_names'].append(name)
        cases.append(dict(name=name,receipt=str(q),receipt_sha256=digest(q),new_frames=r['measured_new_frames'],**parsed))
    case('full_recording',a.full_report)
    for c in probes['cases']:
        require(c['status']=='PASS' and c['new_frames']==90,'fresh_probe_frame_count_changed');case('probe_'+str(c['slot']),c['receipt'],c['receipt_sha256'])
    tests=self_test();changes=[q for q,d in before.items() if digest(q)!=d];require(not changes,'summary_input_changed')
    result=dict(status='PASS_BOUNDED_READ_ONLY_CODE_OBSERVATIONS',invocation=[sys.executable,*sys.argv],source_pins=[dict(path=q,sha256=d) for q,d in before.items()],input_changes=changes,candidate_sha256=a.expected_binary_sha,cases=cases,distinct_matched_candidate_signatures=sum(r['equal'] for r in union.values()),diagnostic_keys=union,self_test=tests,scope='Per-workload candidate code identity only; probes overlap recording. Parsed descriptor signatures can share executable words; unparsed candidates are coalesced refusal buckets. No unique-function/family/whole-producer count, ownership, numerical/runtime admission or native credit.')
    out.mkdir();q=out/'RESULT.json';q.write_text(json.dumps(result,indent=2)+'\n',newline='\n');print(json.dumps(dict(status=result['status'],cases=len(cases),distinct_matched_candidate_signatures=result['distinct_matched_candidate_signatures'],full=cases[0]['summary'],result_sha256=digest(q))))


if __name__=='__main__':main()
