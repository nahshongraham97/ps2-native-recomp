"""Patch authenticated copied AOT labels, so direct calls observe lifetime hooks.

No archive mutation or executable link/launch. Four override objects retain the
exact source units/entry tables of the frozen game archive. The external host
handler must authenticate transactions; a hook event itself admits no lease.
"""
from pathlib import Path
import argparse
import csv
import json
import re
import shutil
import subprocess
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
import analyze_r192_builder_domain as auth

ARCHIVE_SHA='81115b9a4e8a2ba14c1347ad7adcc0fe5d3a9f24651b2f300f2b2cb0629c3dc4'
BUILD_SHA='26fe709e599210c3e14ec1c3ea9bbbc72410cc96f63e6cac957a244dc63405e6'
SITES={
 '000C':[(0x130ab0,'identity_override_before_write','original A0 is explicit next identity; revoke all host leases before original mutation')],
 '0044':[(0x210780,'cache_cleanup_before_callbacks','A0 cache; revoke and mark retiring before any original mutation/callback/free'),
         (0x211c50,'cache_constructor_entry','A0 metadata, r8 callback, r10 optional task; constructed cache not allocated yet'),
         (0x212200,'backing_allocation_bound','S5 cache, S3 allocated backing, S1 allocated bytes; prior complete call bundle zeroed allocation and stored cache+60')],
 '007B':[(0x2ed3e0,'producer_entry','A0 original sentinel list,A1 temporary928 descriptor,A2 temporary code,A3 capacity; capture before wrapper stack/scratch changes'),
         (0x2ed460,'producer_return_before_scratch_free','V0 returned words; S5/S4 scratch still alive; V0->S6 assignment follows hook')],
 '007E':[(0x2fa9d0,'parent_producer_entry','Original parent input A0/A1; source parent SP before1504 allocation'),
         (0x2faf20,'cache_allocation_return','cache pointer at current SP+1500; null result has not yet taken original failure branch'),
         (0x2faf4c,'final_code_copy_return','Original aggregate code copy completed; cache at SP+1500'),
         (0x2fafb0,'temporary_code_pointers_patched','Per-program temporary+76 pointers patched for nonzero length slots'),
         (0x2fb078,'descriptor_copy_return','Original2E8110 returned; exact count match/copy must be authenticated separately'),
         (0x2fb0d0,'descriptor_relocation_return','Original2E7A30 returned; linked filter references are now final descriptor pointers'),
         (0x2fb158,'owner_binding_return','Original2E1EB0 returned; descriptor+72 and resource binding now final'),
         (0x2fb38c,'source_success_commit','Reachable through successful2E9780 branch; cache at SP+1500; require matching producer/allocation/copy transaction'),
         (0x2fb3c4,'parent_exit_abort_uncommitted','All exits through this label retire remaining pending producer state; committed leases survive only their own authenticated epoch')],
}


def patch(text,unit):
    text=text.replace('\r\n','\n')
    declaration='extern "C" void r192_resource_lifecycle_hook(ps2::Cpu&, std::uint32_t);\n'
    auth.need(text.startswith('#include "units.hpp"\n') and 'r192_resource_lifecycle_hook' not in text,
              'original_generated_unit_header_changed')
    text=text.replace('#include "units.hpp"\n','#include "units.hpp"\n'+declaration,1)
    for pc,_,_ in SITES[unit]:
        label=f'L_{pc:08X}:\n'
        auth.need(text.count(label)==1,f'exact_original_hook_label_missing:{pc:08X}')
        text=text.replace(label,label+f'    ::r192_resource_lifecycle_hook(c,0x{pc:08X}u);\n')
    return text


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('game-build','runtime-build','elf','listing','compiler','output'):
        p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--compile',action='store_true')
    a=p.parse_args();game=a.game_build.resolve();runtime=a.runtime_build.resolve();out=a.output.resolve()
    auth.need(out.is_relative_to(auth.W/'build/oracle-telemetry') and not out.exists(),'fresh_owned_output_required')
    auth.need(shutil.disk_usage(out.parent).free>(12<<30)+(32<<20),'12GiB_hook_prepare_reserve_required')
    receipt=game/'RESULT.json';archive=game/'out/libps2game.a';generated=game/'generated_ee'
    auth.need(auth.pin(receipt)['sha256']==BUILD_SHA and auth.pin(archive)['sha256']==ARCHIVE_SHA,'frozen_game_archive_build_changed')
    record=json.loads(receipt.read_text())
    cache=game/'out/CMakeCache.txt';ninja=game/'out/build.ninja'
    expected='PS2_GENERATED:PATH='+generated.as_posix()
    auth.need(expected in cache.read_text(encoding='utf8'),'archive_generated_source_root_changed')
    auth.need(auth.pin(a.elf)['sha256']==auth.ELF_SHA and auth.pin(a.listing)['sha256']==auth.LISTING_SHA,'retail_inputs_changed')
    paths=[Path(__file__),Path(auth.__file__),receipt,archive,cache,ninja,a.elf,a.listing,a.compiler,generated/'units.hpp',generated/'table.cpp']
    for path in [generated/'units.hpp',generated/'table.cpp',*(generated/f'unit_{u}.cpp' for u in SITES)]:
        auth.need(auth.pin(path)['sha256']==record['source_hashes'][str(path.relative_to(game))],
                  'archive_matching_generated_source_changed:'+path.name)
        paths.append(path)
    # The source units are compiled against their original v8 header ABI.
    for path in (game/'src/runtime/include/ps2').glob('*.hpp'):
        other=runtime/'src/runtime/include/ps2'/path.name
        auth.need(auth.pin(path)['sha256']==auth.pin(other)['sha256'],'runtime_header_ABI_changed:'+path.name)
        paths.extend([path,other])
    original=auth.Original(a.elf.read_bytes());selected={pc for sites in SITES.values() for pc,_,_ in sites};joins=[]
    with a.listing.open(encoding='utf8',newline='') as stream:
        for row in csv.DictReader(stream,delimiter='\t'):
            pc=int(row['address'],16)
            if any(site-8<=pc<=site+8 for site in selected):
                word=int.from_bytes(bytes.fromhex(row['bytes']),'little');auth.need(original.word(pc)==word,'hook_original_word_join_failed');joins.append(row)
    auth.need(selected<={int(r['address'],16) for r in joins},'hook_site_original_listing_missing')
    pins=[auth.pin(q) for q in dict.fromkeys(paths)];out.mkdir(parents=True);units=[];commands=[];controls=[]
    for unit in SITES:
        path=generated/f'unit_{unit}.cpp';raw=path.read_bytes();text=raw.decode('utf8');changed=patch(text,unit)
        for pc,_,_ in SITES[unit]:
            planted=text.replace(f'L_{pc:08X}:',f'L_BAD_{pc:08X}:',1)
            try:patch(planted,unit)
            except ValueError as error:controls.append(dict(unit=unit,missing_site=f'{pc:08X}',status='REJECTED',reason=str(error)))
            else:raise ValueError('planted_missing_source_hook_accepted')
        # Strip ONLY inserted declaration/hooks and require byte-normalized
        # original equality, including every unit/case/entry-array value.
        restored=changed.replace('extern "C" void r192_resource_lifecycle_hook(ps2::Cpu&, std::uint32_t);\n','')
        restored=re.sub(r'    ::r192_resource_lifecycle_hook\(c,0x[0-9A-F]{8}u\);\n','',restored)
        auth.need(restored==text.replace('\r\n','\n'),'original_generated_body_changed')
        copied=out/path.name;copied.write_text(changed,encoding='utf8',newline='\n');obj=copied.with_suffix('.obj')
        argv=[str(a.compiler),'-std=gnu++20','-O3','-DNDEBUG','-DPS2_GPU','-mavx2','-ffp-contract=off','-Wno-unused-label','-Wno-unused-variable',
              '-I'+str(generated),'-I'+str(runtime/'src/runtime/include'),'-I'+str(runtime/'src/runtime/src'),'-c',str(copied),'-o',str(obj)]
        status=None
        if a.compile:
            auth.need(shutil.disk_usage(out).free>(12<<30)+(16<<20),'12GiB_hook_compile_reserve_required')
            cp=subprocess.run(argv,capture_output=True,timeout=120);(out/(unit+'-compile.log')).write_bytes(cp.stdout+cp.stderr);status=cp.returncode
            auth.need(status==0,'hook_override_compile_failed:'+unit)
        units.append(dict(unit=unit,original=auth.pin(path),patched=auth.pin(copied),object=auth.pin(obj) if a.compile else None,
                          sites=[dict(pc=f'{pc:08X}',event=name,register_witness=info) for pc,name,info in SITES[unit]],original_body_unchanged_after_hook_removal=True))
        commands.append(dict(argv=argv,exit=status))
    changes=[pin['path'] for pin in pins if auth.pin(Path(pin['path']))!=pin]
    auth.need(not changes,'hook_prepare_original_input_changed')
    result=dict(status='SOURCE_LABEL_LIFECYCLE_HOOK_OBJECTS_PREPARED' if a.compile else 'SOURCE_LABEL_LIFECYCLE_HOOK_SOURCES_PREPARED',
                source_pins=pins,input_changes=changes,archive=auth.pin(archive),units=units,commands=commands,controls=controls,original_word_joins=joins,
                hook_API='extern C void r192_resource_lifecycle_hook(ps2::Cpu&,std::uint32_t source_site);',
                link_requirement='Place all four override objects before this exact static game archive. Each object includes matching original entries_XXXX and unit_XXXX definitions. Host handler implementation/link/controls are separate.',
                scope='Actual source-label instrumentation catches direct generated-unit calls. No executable link, game execution, host registry admission or runtime mutation in this tool.',
                unresolved=['Handler must authenticate producer transaction/copy/bind/success history; events alone do not authenticate history.',
                            'Invalidate before state restore/reset through actual runtime hook, not these game-unit labels.',
                            'Snapshot rehydration requires new host epoch plus current body/cache-table/owner membership validation; no invented allocation history.'],
                invocation=[sys.executable,str(Path(__file__).resolve()),*sys.argv[1:]])
    (out/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8',newline='\n')
    print(json.dumps(dict(status=result['status'],units=len(units),sites=sum(len(v) for v in SITES.values()),controls=len(controls),receipt=auth.pin(out/'RESULT.json'))))


if __name__=='__main__':main()
