"""Actual-boundary comparison and falsification for descriptor-derived dispatch."""
from pathlib import Path
import argparse
import json
import re
import subprocess
import sys

import verify_r192_resource_word_equality as auth

ROOT=Path(__file__).resolve().parents[2]


def need(ok,why):
    if not ok:raise ValueError(why)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--compiler',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();out=a.output.resolve();need(out.is_relative_to(ROOT/'build/oracle-telemetry') and not out.exists(),'fresh_owned_output_required')
    prior=ROOT/'build/oracle-telemetry/emitted-resource-connected-controls-03';receipt=prior/'RESULT.json';need(auth.digest(receipt)=='2c38e43a5dda99505218c35c2b15a7984f1b2de3be6c0e6ec8ec91a7448d2aa8','actual_boundary_reference_changed')
    record=json.loads(receipt.read_text());cpp0=prior/'adapter_controls.cpp';need(any(Path(r['path'])==cpp0 and auth.digest(cpp0)==r['sha256'] for r in record['artifacts']),'actual_boundary_fixture_changed')
    need(auth.digest(a.compiler)=='ee85bef2c733e24a26c22be3df94bd631cf9528a048fce91241c182a936737fe','original_compiler_required')
    core=ROOT/'tools/research/r192_aot_modules';header=core/'reference_general_resource_adapter.hpp';old=core/'reference_resource_event_adapter.hpp';oldcontract=ROOT/'build/r192-resource-event-candidate-04-final/source/resource_contract.hpp'
    dependencies=[Path(__file__),Path(auth.__file__),receipt,cpp0,header,oldcontract,a.compiler,*[Path(r['path']) for r in record['source_pins']],core/'reference_resource_word_equality.hpp',core/'reference_resource_owner_epochs.hpp',core/'effective_resource_construction.hpp']
    pins=[dict(path=str(q.resolve()),sha256=auth.digest(q)) for q in dict.fromkeys(dependencies)]
    out.mkdir();contract=out/'general_contract.hpp';match=re.search(r'c\.caller=\{[^}]+\};',oldcontract.read_text());need(match is not None,'caller_contract_missing');contract.write_text('static const mka::emitted::general_resource_reference::Contract resource_contract=[](){mka::emitted::general_resource_reference::Contract c{};'+match[0]+'return c;}();\n',newline='\n')
    text=cpp0.read_text();need(text.count(old.as_posix())==text.count(oldcontract.as_posix())==1,'original_fixture_include_seam_changed')
    text=text.replace(old.as_posix(),header.as_posix()).replace(oldcontract.as_posix(),contract.as_posix()).replace('mka::emitted::resource_reference::Adapter','mka::emitted::general_resource_reference::Adapter')
    text=text.replace('retained executable words changed','retained code/caller identity changed')
    need(text.count('Adapter adapter(resource_contract);')==1,'owner_poison_fixture_seam_changed')
    text=text.replace('Adapter adapter(resource_contract);','mka::emitted::resource_owner_epochs::Registry owners;Adapter adapter(resource_contract,&owners);if(mode=="--planted-owner-poison")owners.begin_restore();')
    needle='if(mode=="--planted-reset-during-guard")'
    need(text.count(needle)==1,'actual_guard_seam_changed')
    text=text.replace(needle,'if(mode=="--planted-pending-after-revocation"){active_adapter->retire_cache(ld32(c.r[19].w[0]+72)-64);if(!active_adapter->pending())throw std::runtime_error("TEST_SAVE_BARRIER_LOST_ACTIVE_LEASE");}if(mode=="--planted-cache-cleanup")active_adapter->retire_cache(ld32(c.r[19].w[0]+72)-64);if(mode=="--planted-cache-id")active_adapter->retire_identity(ld32(ld32(c.r[19].w[0]+72)-60));if(mode=="--planted-unrelated-id")active_adapter->retire_identity(0x01234567u);'+needle)
    needle='{"--planted-reset-during-guard","context revoked during original callback"}'
    need(text.count(needle)==1,'actual_refusal_seam_changed')
    text=text.replace(needle,'{"--planted-owner-poison","machine state owner poisoned"},{"--planted-pending-after-revocation","context revoked during original callback"},{"--planted-cache-cleanup","context revoked during original callback"},{"--planted-cache-id","context revoked during original callback"},{"--planted-reset-during-guard","context revoked during original callback"}')
    needle='if(mode=="--planted-unknown-code")'
    need(text.count(needle)==1,'source_required_test_seam_changed')
    text=text.replace(needle,'if(mode=="--planted-source-required"){const Cpu before=g_cpu;const std::vector<unsigned char>ram(g_ram,g_ram+kRamSize);if(adapter.try_dispatch(g_cpu,true)||adapter.pending()||std::memcmp(&before,&g_cpu,sizeof before)||std::memcmp(g_ram,ram.data(),kRamSize))throw std::runtime_error("TEST_MISSING_SOURCE_WITNESS_ADMITTED");std::printf("REJECTED source-required: source witness absent; compiled AOT unchanged\\n");return 1;}'+needle)
    need(text.count('if(!mode.empty())throw')==1,'actual_positive_mode_seam_changed')
    text=text.replace('if(!mode.empty())throw','if(!mode.empty()&&mode!="--planted-unrelated-id")throw')
    cpp=out/'general_controls.cpp';cpp.write_text(text,newline='\n');flags=[str(a.compiler),'-std=c++20','-O1','-mavx2','-ffp-contract=off','-static-libgcc','-static-libstdc++','-I'+str(ROOT/'build/r192-compiled-konquest/src/runtime/include'),'-I'+str(ROOT/'build/r192-compiled-konquest/src/runtime/src')];commands=[]
    def run(argv,name):
        c=subprocess.run(argv,capture_output=True,timeout=120);q=out/(name+'.log');q.write_bytes(c.stdout+c.stderr);commands.append(dict(argv=argv,exit=c.returncode,log=str(q),sha256=auth.digest(q)));return c
    exe=out/'general_controls.exe';need(run([*flags,str(cpp),'-o',str(exe)],'compile').returncode==0,'general_controls_compile_failed');checks=[]
    names=['','owner-poison','source-required','pending-after-revocation','cache-cleanup','cache-id','unrelated-id','reset-during-guard','retire-during-guard','private-VU','owned-RAM','changed-code','changed-key','changed-resume-PC','changed-caller','service-private','output-alias','unknown-code','orphan-resume']
    for name in names:
        c=run([str(exe)]+(['--planted-'+name] if name else []),name or 'positive');expected=0 if name in ('','unrelated-id') else 1
        checks.append(dict(name=name or 'positive',exit=c.returncode,expected=expected,stdout=c.stdout.decode(errors='replace'),stderr=c.stderr.decode(errors='replace')))
    changes=[r['path'] for r in pins if auth.digest(r['path'])!=r['sha256']];passed=not changes and all(r['exit']==r['expected'] for r in checks)
    result=dict(status='PASS_GENERAL_SOURCE_EQUATION_ACTUAL_CONTROLS' if passed else 'GENERAL_ADAPTER_HAS_REFUSALS',invocation=[sys.executable,*sys.argv],source_pins=pins,input_changes=changes,commands=commands,checks=checks,artifacts=[dict(path=str(q),sha256=auth.digest(q)) for q in [cpp,contract,exe]],scope='Original three actual source-boundary sessions/full Cpu+RAM, including real recorded guard results. General descriptor equations replace shape matching. No new whole-game/owner/native-credit claim.')
    q=out/'RESULT.json';q.write_text(json.dumps(result,indent=2)+'\n',newline='\n');print(json.dumps(dict(status=result['status'],checks=[dict(name=c['name'],exit=c['exit'],expected=c['expected'],stderr=c['stderr']) for c in checks if c['exit']!=c['expected']],sha256=auth.digest(q))))
    return 0 if passed else 2


if __name__=='__main__':raise SystemExit(main())
