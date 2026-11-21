"""Publish the source- and result-reviewed task execution checkpoint."""
import importlib.util
import json
import os
from pathlib import Path

H=Path(__file__).resolve().parent
F=H.parents[2]
P=F.parents[2]
C=P/'research/runs/native-retail-startup-20261120/connected-retail-entry-01'
s=importlib.util.spec_from_file_location('task_publication',F/'research/runs/native-connected-main-dependencies-20261121/publish.py')
m=importlib.util.module_from_spec(s)
s.loader.exec_module(m)


def load(path):
    return json.loads(path.read_text(encoding='utf-8'))


def main():
    n=int(os.environ.get('GIT_CONFIG_COUNT','0'))
    os.environ.update(GIT_CONFIG_COUNT=str(n+1),**{'GIT_CONFIG_KEY_'+str(n):'core.longpaths',
        'GIT_CONFIG_VALUE_'+str(n):'true'})
    vpath=H/'validation-02/verification.json'
    m.pub.need(m.pub.sha(vpath)=='0995ee96ec310339b4bbe47ea8c881f741b8d5699f5bf469d8ce77f1fbdb662d','exact result')
    v=load(vpath)
    m.pub.need(v['status']=='PASS_NATIVE_TASK_CLASS_EXECUTION_RETIREMENT_CPU_CHECKS' and
        len(v['executed_clients'])==23 and len(v['remaining_integration_symbols'])==27 and
        len(v['required_profile_effects'])==5 and not v['game_executed'],'bounded measured scope')
    for group in ['sources','evidence','protected_inputs']:
        for path,pin in v[group].items():m.pub.need(m.pub.sha(path)==pin,'current '+path)
    review_path=C/'native-retail-task-class-execution-77/review-results01/REVIEW.json'
    m.pub.need(m.pub.sha(review_path)=='a09d3686d2d0e4e2e479d81db65bcd72d413a87aa772ba32463caf988e7128fb','exact saved review')
    r=load(review_path)
    m.pub.need(r['schema']=='native_retail_task_class_execution_saved_review_v1' and
        r['status']=='PASS_BOUNDED_SAVED_RESULT_REVIEW' and r['reviewed_verification_sha256']==m.pub.sha(vpath)
        and r['checker_sha256']==m.pub.sha(H/'check.py') and
        [x['target'] for x in r['clients']]==[x['target'] for x in v['executed_clients']],
        'actual review schema and exact target identity list')
    for row in v['executed_clients']:
        receipt=load(H/'validation-02'/row['receipt'])
        m.pub.need(receipt['exit']==0 and not receipt['stderr'] and receipt['executable_sha256']==
            receipt['executable_after_sha256']==row['sha256']==m.pub.sha(F/'build_native_services_only_20261119-01'/(row['target']+'.exe')),
            'current tested client '+row['target'])
    source_review=load(C/'native-retail-task-class-execution-77/review-current01/REVIEW.json')
    m.pub.need(source_review['schema']=='native_retail_task_class_execution_77_current_source_review_v1' and
        source_review['status']=='BOUNDED_CLASS_AWARE_SOURCE_REVIEW_PASSED_UNBUILT' and not source_review['findings'],
        'actual bounded source review')
    for path,pin in source_review['reviewed_source_pins_sha256'].items():
        path=Path(path) if Path(path).is_absolute() else F/path
        m.pub.need(m.pub.sha(path)==pin,'current source review bytes')
    files=['cmake/native_game.cmake','runtime/include/mka/retail_task_registry.hpp',
        'runtime/src/native/retail_task_registry.cpp','runtime/include/mka/retail_task_execution.hpp',
        'runtime/src/native/retail_task_execution.cpp','tests/test_retail_task_registry.cpp',
        'tests/test_retail_task_execution.cpp','tests/test_retail_initial_task.cpp',
        'runtime/include/mka/retail_profile_task_retirement.hpp','runtime/src/native/retail_profile_task_retirement.cpp',
        'tests/test_retail_profile_task_retirement.cpp','tests/test_retail_task_class_birth.cpp',
        'tests/test_retail_stock_task_execution.cpp']
    forks=[F/name for name in files]+[H/'README.md',H/'check.py',Path(__file__)]
    for folder in ['validation-01','validation-02']:forks+=sorted((H/folder).glob('*.json'))
    parents=[P/'AGENTS.md',P/'PORT_STATUS.md',P/'ROADMAP.md']
    for scope in ['native-retail-profile-task-retirement-75','native-retail-profile-scene-producer-76',
                  'native-retail-task-class-execution-77']:
        parents+=sorted(x for x in (C/scope).rglob('*') if x.is_file() and '__pycache__' not in x.parts)
    for path in forks+parents:
        path.read_bytes().decode('utf-8')
    bases={F:'462247408bb45358b59a5dcdf2b4094d60f9efa3',P:'3a2a7a9a2319338073b55e8c6954c8db6046877f'}
    for root,base in bases.items():
        m.pub.need(m.pub.git(root,'rev-parse','HEAD')==base and m.pub.git(root,'branch','--show-current')=='main','expected base')
    remote={root:m.pub.remote_probe(root,base) for root,base in bases.items()}
    fh,fs=m.commit_sync(F,forks,'Correct native task classes, execution and selective retirement',remote[F])
    for name in ['PORT_STATUS.md','ROADMAP.md']:
        path=P/name;text=path.read_text(encoding='utf-8')
        text=text.replace('Current task-class/retirement batch is implemented and component-tested on fork4622474 plus recorded working changes.',
            'Published native task-class/retirement checkpoint fork `'+fh+'` is implemented and component-tested.')
        text=text.replace('Prior published profile-effects code5b5f074 remains historical evidence until this batch is published.',
            'Prior profile-effects code5b5f074 is historical evidence; the task-class changes are now published.')
        path.write_bytes(text.encode('utf-8'))
    ph,ps=m.commit_sync(P,parents,'Record verified native task execution and retirement checkpoint',remote[P])
    for path,pin in v['protected_inputs'].items():m.pub.need(m.pub.sha(path)==pin,'protected after publication')
    record=dict(schema='native_retail_task_retirement_publication_v1',fork_commit=fh,fork=fs,parent_commit=ph,parent=ps,
        verification_sha256=m.pub.sha(vpath),scope='23 CPU/file clients;27 Main and5 separate effect requirements;game acceptance open')
    with (H/'PUBLICATION.json').open('x',encoding='utf-8',newline='\n') as stream:
        json.dump(record,stream,indent=2);stream.write('\n')
    print(json.dumps(record))


if __name__=='__main__':
    main()
