"""Publish measured native keyboard feed and preserved failure/correction history."""
import importlib.util,json
from pathlib import Path
F=Path(__file__).resolve().parents[3];P=F.parents[2];HERE=Path(__file__).resolve().parent
S=P/'research/runs/native-retail-startup-20261120/connected-retail-entry-01';D=S/'native-keyboard-feed-07'
spec=importlib.util.spec_from_file_location('pub',F/'research/runs/native-connected-retail-entry-20261120/publish.py');pub=importlib.util.module_from_spec(spec);spec.loader.exec_module(pub)
if __name__=='__main__':
    result=json.loads((HERE/'validation-02/verification.json').read_text(encoding='utf-8'))
    pub.need(result['status']=='PASS_NATIVE_KEYBOARD_FEED_CPU_FILE_CHECKS' and len(result['sources'])==47 and len(result['fixtures'])==13 and len(result['remaining_integration_symbols'])==49 and not result['complete_game_entry_linkable'],'actual measured keyboard scope')
    for path,digest in result['sources'].items():pub.need(pub.sha(path)==digest,'current source '+path)
    report=D/'EXECUTION_REVIEW.json';pub.need(pub.sha(report)=='a3642a76bcc3fc1b1ca728b2d510cceec7ba6244264f2fb13e5be18d7654be56','exact independent result review')
    review=json.loads(report.read_text(encoding='utf-8'))
    pub.need(review['schema']=='native-retail-keyboard-feed-saved-execution-independent-review-v1' and review['status']=='SCOPED_SAVED_EXECUTION_VERIFIED' and review['source_pins_checked']==47 and review['fixture_executable_receipts_checked']==13 and review['negative_entry_link']['exact_saved_link_set_match'] and review['negative_entry_link']['current_count']==49 and not review['negative_entry_link']['new_success_bodies_for_removed_USB_calls'] and review['findings']==[],'actual review schema')
    for key,filename in [('startup','libmka_native_retail_startup.a'),('keyboard','libmka_native_retail_keyboard.a'),('platform','libmka_native_retail_platform.a')]:pub.need(pub.sha(F/'build_native_services_only_20261119-01'/filename)==review['archives'][key],'actual archive '+key)
    for row in result['fixtures']:pub.need(pub.sha(F/'build_native_services_only_20261119-01'/(row['target']+'.exe'))==row['sha256'],'actual executable '+row['target'])
    for path,digest in result['independent_manifest_sha256'].items():pub.need(pub.sha(path)==digest,'independent manifest')
    pub.need(pub.sha(D/'source-key-table.bin')==result['source_key_table_sha256'],'source raw table')
    protected={F/'runtime/src/iop/mcserv.cpp':'ad2731a341409d87ffe1b171e99f28d7cedc5004aaf375a565d78673333414fb',F/'runtime/src/iop/msl_native.cpp':'756cfb3867ebb440223ddb8ab6fe441ad6f9f54d35e4119a8342f8dd9063bb23',P/'research/doc-audit-20261118/inventory.json':'26093151039b949348184f2e8a960b92d64e7f08408d43e58e1e0ed481a959f6'}
    for path,digest in protected.items():pub.need(pub.sha(path)==digest,'protected bytes')
    bases={F:'8eec087303670c6ff04af516d78102ad26f7c196',P:'4a0bf1c946f3058b03278e600f83e99fe9e1cfc4'}
    for root,expected in bases.items():pub.need(pub.git(root,'rev-parse','HEAD')==expected and pub.git(root,'branch','--show-current')=='main','expected publication base')
    authored=['runtime/include/mka/retail_keyboard.hpp','runtime/src/native/retail_keyboard.cpp','runtime/include/mka/retail_keyboard_sdl.hpp','runtime/src/native/retail_keyboard_sdl.cpp','runtime/include/mka/retail_platform.hpp','runtime/src/native/retail_platform.cpp','runtime/include/mka/retail_main.hpp','runtime/src/native/retail_main.cpp','cmake/native_game.cmake','tests/test_retail_keyboard.cpp','README.md']
    forks={F/n for n in authored}|{F/'research/runs/native-retail-input-history-20261121/PUBLICATION.json'}
    forks.update(p for p in HERE.rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.name!='PUBLICATION.json' and not {'owned-content','disposable-package-errors'} & set(p.relative_to(HERE).parts))
    parents={P/'PORT_STATUS.md',P/'ROADMAP.md'}
    for folder in ['native-keyboard-feed-07','exit-lifecycle-20261121']:
        parents.update(p for p in (S/folder).rglob('*') if p.is_file() and '__pycache__' not in p.parts)
    parents.update(p for p in (S/'native-controller-poll-08').iterdir() if p.is_file())
    path=D/'RESULTS.md';data=path.read_text(encoding='utf-8');pub.need('Independent saved-execution review verifies all47 source/tool pins' in data,'actual reviewed result marker')
    raw_exports=json.loads((D/'RAW_EVIDENCE_EXPORT_PINS.json').read_text(encoding='utf-8'))['files']
    for name,digest in raw_exports.items():pub.need(pub.sha(name)==digest,'raw evidence export exact bytes '+name)
    for path in forks|parents:
        relative=path.relative_to(F) if path.is_relative_to(F) else path.relative_to(P)
        if 'before' in str(relative) or 'retired-unbuilt-usb-draft' in relative.parts or path.suffix in ['.bin','.csv','.log','.txt']:continue
        if str(path) in raw_exports:continue
        raw=path.read_bytes();pub.need(b'\r' not in raw and raw.endswith(b'\n') and not raw.endswith(b'\n\n'),'authored LF/EOF '+str(path));pub.need(all(line==line.rstrip(b' \t') for line in raw.splitlines()),'authored whitespace '+str(path))
    fr,pr=pub.remote_probe(F,bases[F]),pub.remote_probe(P,bases[P])
    fh,fs=pub.commit_sync(F,sorted(forks),'Replace USB bookkeeping with native platform keyboard event ownership and feed',fr)
    for name in ['PORT_STATUS.md','ROADMAP.md']:
        path=P/name;data=path.read_text(encoding='utf-8');marker='Latest measured working-tree native-keyboard-feed-07 (base fork `8eec087303670c6ff04af516d78102ad26f7c196`)';pub.need(data.count(marker)==1,'single live marker');path.write_text(data.replace(marker,'Published native keyboard-feed checkpoint fork `'+fh+'` (integration07)'),encoding='utf-8',newline='\n')
    ph,ps=pub.commit_sync(P,sorted(parents),'Record native keyboard integration verification and exact retail input seams',pr)
    for path,digest in result['sources'].items():pub.need(pub.sha(path)==digest,'source stable after publication')
    for path,digest in protected.items():pub.need(pub.sha(path)==digest,'protected bytes after publication')
    record=dict(fork_commit=fh,fork=fs,parent_commit=ph,parent=ps,verification_sha256=pub.sha(HERE/'validation-02/verification.json'),scope='Thirteen CPU/file clients and platform/archive compile only; UI, physical input and game acceptance OPEN')
    with (HERE/'PUBLICATION.json').open('x',encoding='utf-8',newline='\n') as out:json.dump(record,out,indent=2);out.write('\n')
    print(json.dumps(record))
