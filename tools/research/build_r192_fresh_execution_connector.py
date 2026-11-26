"""Emit/compile isolated source-pinned v8 IOP owner wiring and scheduler proof.

No normal N, game/card/package, original or existing dispatcher writes. The
result is an IOP object bundle for the owner's combined disposable linker;
the separate machine-state owner must compose the declared restore hooks.
"""
from __future__ import annotations
import sys
sys.dont_write_bytecode=True
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import time
import generate_r192_iop_aot as common
import build_r192_iop_v8_candidate as builder
from verify_r192_loader_owner_corpus import Compression

W,need,sha=common.W,common.require,common.file_sha
COMPONENT_SHA='31ede959a15bc4247d4e9fdd52dd6610568e58e963a8e8b6ecbd04e01227b3c9'


def once(text,old,new):
    need(text.count(old)==1,'pinned_source_seam_changed:'+old[:70]);return text.replace(old,new)


def reserve(extra=0):need(shutil.disk_usage(W).free>(12<<30)+(8<<20)+extra,'12GiB_connector_reserve')


def dispatch_source(original):
    text=original.replace('\r\n','\n')
    text=once(text,'aot_v8::BoundPic &select(', 'aot_v8::BoundPic &legacy_select(')
    text=once(text,'[[noreturn]] void aot_refuse(const std::string &why, const Cpu &c) {','[[noreturn]] void aot_refuse(const std::string &why, const Cpu &c) {\n    fresh_bridge::dispatch_refused();')
    text=once(text,'void execute_aot_v8(Cpu &c, std::int64_t &budget) {','void execute_aot_v8(Cpu &c, std::int64_t &budget) {\n    fresh_bridge::MutationScope execution_failure;')
    text=once(text,'\nvoid scalar_step(', '\nconst aot_v8::BoundPic &select(Cpu &, std::map<BindingKey, std::uint64_t> &);\nvoid scalar_step(')
    text=once(text,'    while (budget-- > 0) {','    while (budget-- > 0) {\n        fresh_bridge::instruction_boundary();')
    text=once(text,'!identity(nullptr, pc, *entry)','!fresh_bridge::validate_dispatch_import(pc, *entry)')
    text=once(text,'identity(nullptr, pc - 4, *at_import(m, offset - 4))','fresh_bridge::validate_dispatch_import(pc - 4, *at_import(m, offset - 4))')
    text=once(text,'    const auto target = c.r[31];','    if (g_hle[id].library == "loadcore" && g_hle[id].index == 7) fresh_bridge::note_open_release_service();\n    const auto target = c.r[31];')
    return text


def kernel_source(raw,dispatch):
    text=builder.prepare_kernel(raw,dispatch)
    text='#include "iop/r192_fresh_execution_hooks.hpp"\n'+text
    text=once(text,'std::int32_t load_module_buffer(const std::vector<std::uint8_t> &data,','std::int32_t r192_actual_load_module_buffer(const std::vector<std::uint8_t> &data,')
    text=once(text,'void boot() {','void boot() {\n    fresh_bridge::MutationScope mutation;\n    fresh_bridge::reset_before_boot();')
    text=once(text,'void free(std::uint32_t address) { g_used.erase(phys(address)); }',
        '''void free(std::uint32_t address) {
    fresh_bridge::MutationScope mutation;
    const auto base = phys(address); const auto at = g_used.find(base);
    const auto size = at == g_used.end() ? 0u : at->second;
    if (at != g_used.end()) g_used.erase(at);
    fresh_bridge::allocation_erased(base, size, size != 0);
}''')
    text=once(text,'    g_threads[t.id] = t;','    g_threads[t.id] = t;\n    fresh_bridge::thread_created(t.id, entry);')
    signature='int create_thread(std::uint32_t entry, std::uint32_t stack_size, int priority, std::uint32_t gp, std::uint32_t attr, std::uint32_t option) {'
    text=once(text,signature,signature+'\n    fresh_bridge::MutationScope mutation;')
    signature='std::uint32_t call_guest(std::uint32_t address, std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3) {'
    text=once(text,signature,signature+'\n    fresh_bridge::MutationScope operation_failure;\n    fresh_bridge::CallScope source_context(address);')
    signature='void register_interrupt(unsigned line, std::uint32_t handler, std::uint32_t arg, std::uint32_t mode) {'
    text=once(text,signature,signature+'\n    fresh_bridge::MutationScope mutation;')
    text=once(text,'    g_interrupts[line] = Interrupt{handler, arg, mode};','    g_interrupts[line] = Interrupt{handler, arg, mode};\n    fresh_bridge::irq_registered(line, handler);')
    text=once(text,'void release_interrupt(unsigned line) { g_interrupts.erase(line); }','void release_interrupt(unsigned line) { fresh_bridge::MutationScope mutation; g_interrupts.erase(line); fresh_bridge::irq_released(line); }')
    text=once(text,'        const std::uint32_t result = call_guest(handler.handler, handler.arg);','        fresh_bridge::IrqScope source_irq(static_cast<unsigned>(line), handler.handler);\n        const std::uint32_t result = call_guest(handler.handler, handler.arg);')
    text=once(text,'void deliver_interrupts() {','void deliver_interrupts() {\n    fresh_bridge::MutationScope operation_failure;')
    # The actual register_library() only changes its host map. Do not invent
    # relinking there. Observe the real primitive and its exact source writes.
    signature='void link_imports(std::uint32_t base, std::uint32_t size) {'
    replacement='''void r192_actual_link_imports(std::uint32_t base, std::uint32_t size);
void link_imports(std::uint32_t base, std::uint32_t size) {
    fresh_bridge::MutationScope mutation;
    fresh_bridge::before_library_link();
    r192_actual_link_imports(base, size);
    fresh_bridge::after_library_link();
}
void r192_actual_link_imports(std::uint32_t base, std::uint32_t size) {'''
    text=once(text,signature,replacement)
    return text+'\n#include "iop/r192_fresh_execution_bridge.inc"\n'


def diagnostic_owner_sources(output):
    # Only pre-throw logging and include paths differ from the frozen checked
    # headers. Do not edit those headers or weaken any guard to diagnose boot.
    folder=W/'runtime/src/iop';result={}
    rows=[('r192_static_metadata_guard.hpp','why','std::string("IOP_METADATA_REFUSED:") + why'),
          ('r192_loader_code_owner.hpp','text','text'),
          ('r192_fresh_execution_owner.hpp','s','s')]
    for name,arg,thrown in rows:
        source=(folder/name).read_text().replace('\r\n','\n')
        needle='[[noreturn]] static void fail(const char *'+arg+') { throw std::runtime_error('+thrown+'); }'
        if name=='r192_fresh_execution_owner.hpp':needle='[[noreturn]]static void fail(const char*'+arg+'){throw std::runtime_error('+thrown+');}'
        replacement=needle.replace('throw std::runtime_error',
            'std::fprintf(stderr,"[iop-fresh] owner_REFUSED:'+name+':%s\\n",'+arg+');std::fflush(stderr);throw std::runtime_error')
        source=once(source,needle,replacement)
        source='#include <cstdio>\n'+source
        if name=='r192_loader_code_owner.hpp':source=once(source,'#include "iop/r192_static_metadata_guard.hpp"','#include "'+(output/'r192_static_metadata_guard.hpp').as_posix()+'"')
        if name=='r192_fresh_execution_owner.hpp':source=once(source,'#include "iop/r192_loader_code_owner.hpp"','#include "'+(output/'r192_loader_code_owner.hpp').as_posix()+'"')
        result[name]=source
    return result


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('component-proof','image','output'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();a.component_proof=a.component_proof.resolve();a.image=a.image.resolve();a.output=a.output.resolve()
    need(sha(a.component_proof)==COMPONENT_SHA,'frozen_execution_component_changed')
    need(a.output.is_relative_to(W/'build/oracle-telemetry') and not a.output.exists(),'fresh_owned_output_required')
    component=json.loads(a.component_proof.read_text());need(component['status']=='FRESH_SOURCE_OWNER_EXECUTION_COMPONENT_PASS' and not component['input_changes'],'passing_frozen_component_required')
    expected={x['path']:x['sha256'] for x in component['source_pins']}
    owner_path=next(Path(x) for x in expected if Path(x).parent.name=='r192-iop-code-owner-05' and Path(x).name=='RESULT.json')
    owner=json.loads(owner_path.read_text());anchor={x['path']:x['sha256'] for x in owner['source_pins']}
    catalogue=owner_path.parent/'original-sources.inc';need(sha(catalogue)==owner['catalogue']['generated_include_sha256'],'original70_catalogue_changed')
    command=list(component['build_command']);compiler=Path(command[0]);need(sha(compiler)==expected[str(compiler)],'compiler_changed')
    runtime=next(Path(x).parent.parent for x in command if x.endswith('libps2runtime.a'))
    original_kernel=runtime/'src/runtime/src/iop/kernel.cpp';old_dispatch=W/'runtime/src/iop/r192_v8_dispatch.inc'
    need(sha(old_dispatch)==anchor[str(old_dispatch)],'published_dispatcher_changed')
    authored=[Path(__file__).resolve(),W/'runtime/src/iop/r192_fresh_execution_hooks.hpp',W/'runtime/src/iop/r192_fresh_execution_bridge.inc',W/'tests/test_r192_fresh_execution_bridge.cpp']
    pins=[{'path':str(x),'sha256':sha(x)} for x in authored+[a.component_proof,owner_path,catalogue,original_kernel,old_dispatch]]
    for path,digest in expected.items():need(sha(Path(path))==digest,'frozen_component_input_changed:'+path)
    pins+=component['source_pins']
    # Construct/validate all source transforms before any filesystem mutation.
    dispatch=dispatch_source(old_dispatch.read_text());pending=a.output/'fresh-dispatch.inc'
    kernel=kernel_source(original_kernel.read_bytes(),pending)
    diagnostic=diagnostic_owner_sources(a.output)
    try:kernel_source(original_kernel.read_bytes()+b'\n',pending)
    except ValueError as e:need(str(e)=='frozen_original_kernel_source_pin_mismatch','wrong_lineage_control_reason')
    else:raise ValueError('wrong_kernel_lineage_accepted')
    reserve(128<<20);a.output.mkdir(parents=True);archive=a.output/'source-archive';archive.mkdir()
    for x in authored:(archive/x.name).write_bytes(x.read_bytes())
    pending.write_text(dispatch,encoding='utf-8',newline='\n');private_kernel=a.output/'fresh-v8-kernel.cpp';private_kernel.write_text(kernel,encoding='utf-8',newline='\n')
    for name,text in diagnostic.items():(a.output/name).write_text(text,encoding='utf-8',newline='\n')
    public_prefix=W/'tests/test_r192_iop_v8_start.cpp';text=public_prefix.read_text().split('int main(',1)[0]
    seam='extern "C" void __real__ZN3ps23iop7executeERNS0_3CpuERx'
    text=once(text,seam,'namespace { std::function<void(Cpu &, std::int64_t &)> execution_override; }\n'+seam)
    text=once(text,'    __real__ZN3ps23iop7executeERNS0_3CpuERx(cpu, budget);','    if (execution_override) execution_override(cpu, budget); else __real__ZN3ps23iop7executeERNS0_3CpuERx(cpu, budget);')
    prefix=a.output/'original-prefix.inc';prefix.write_text(text,encoding='utf-8',newline='\n')
    old_fixture=W/'tests/test_r192_fresh_execution_owner.cpp';fixture=W/'tests/test_r192_fresh_execution_bridge.cpp';build=[]
    for item in command:
        if item==str(old_fixture):item=str(fixture)
        elif item.startswith('-DR192_REFERENCE_KERNEL_SOURCE='):item='-DR192_REFERENCE_KERNEL_SOURCE="'+private_kernel.as_posix()+'"'
        elif item.startswith('-DR192_OWNER_FIXTURE_PREFIX='):item='-DR192_OWNER_FIXTURE_PREFIX="'+prefix.as_posix()+'"'
        build.append(item)
    build.insert(1,'-DR192_FRESH_ORIGINAL_SOURCES="'+catalogue.as_posix()+'"')
    build.insert(1,'-DR192_FRESH_DIAGNOSTIC_OWNER_HEADER="'+(a.output/'r192_fresh_execution_owner.hpp').as_posix()+'"')
    need(str(fixture) in build and build[-2]=='-o','frozen_build_layout_changed')
    exe=a.output/'fresh-scheduled-IOP.exe';build[-1]=str(exe);rsp=a.output/'build.rsp'
    rsp.write_text('\n'.join(builder.quote_response(x) for x in build[1:])+'\n',encoding='utf-8',newline='\n')
    began=time.perf_counter();compiled=subprocess.run([str(compiler),'@'+str(rsp)],capture_output=True,text=True,timeout=240)
    (a.output/'build.log').write_text(compiled.stdout+compiled.stderr,encoding='utf-8',newline='\n')
    if compiled.returncode:
        out=a.output/'RESULT.json';out.write_text(json.dumps({'status':'BUILD_REFUSED','source_pins':pins,'build_command':build,'exit_code':compiled.returncode},indent=2)+'\n',encoding='utf-8',newline='\n');print(json.dumps({'result':str(out),'status':'BUILD_REFUSED'}));return 1
    compression=Compression(a.output);compressed=[compression.compress(exe)];binary_sha=sha(exe)
    env={k:v for k,v in os.environ.items() if not k.startswith(('PS2_','MKA_','SDL_'))};env['PATH']=str(runtime/'out')+os.pathsep+str(compiler.parent)+os.pathsep+env.get('PATH','')
    env.update({'PS2_EE_LIVE':'0','PS2_VU_LIVE':'0','SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy'})
    cases=[]
    for route in ('filename','buffer'):
        runs={}
        for mode in ('reference','compiled'):
            reserve(16<<20);prefix_out=a.output/(route+'-'+mode);cmd=[str(exe),mode,str(a.image),route,str(prefix_out)]
            need(sha(exe)==binary_sha,'binary_changed_before_run');at=time.perf_counter();r=subprocess.run(cmd,cwd=a.output,env=env,capture_output=True,text=True,timeout=60)
            Path(str(prefix_out)+'.log').write_text(r.stdout+r.stderr,encoding='utf-8',newline='\n')
            runs[mode]={'argv':cmd,'exit_code':r.returncode,'seconds':time.perf_counter()-at,'measurement':json.loads(Path(str(prefix_out)+'.json').read_text())}
            for f in a.output.glob(prefix_out.name+'.*'):
                if f.is_file() and f.stat().st_size>65536:compressed.append(compression.compress(f))
        comparisons={s:Path(str(a.output/(route+'-reference'))+s).exists() and Path(str(a.output/(route+'-compiled'))+s).exists() and sha(Path(str(a.output/(route+'-reference'))+s))==sha(Path(str(a.output/(route+'-compiled'))+s)) for s in ('.cpu','.ram','.iop-state')}
        controls_ok=runs['compiled']['measurement'].get('production_controls')==11 and len(runs['compiled']['measurement'].get('control_reasons',[]))==11
        cases.append({'route':route,'runs':runs,'comparisons':comparisons,'production_controls_complete':controls_ok,
            'status':'PASS' if all(comparisons.values()) and controls_ok and all(x['exit_code']==0 and x['measurement']['status']=='PASS' for x in runs.values()) else 'REFUSED'})
    # Compile the SAME private kernel alone for the combined disposable linker.
    obj=a.output/'fresh-v8-kernel.obj';obj_cmd=[str(compiler),'-std=c++20','-O2','-mavx2','-ffp-contract=off','-DNDEBUG',
        '-DR192_FRESH_DIAGNOSTIC_OWNER_HEADER="'+(a.output/'r192_fresh_execution_owner.hpp').as_posix()+'"',
        '-DR192_FRESH_ORIGINAL_SOURCES="'+catalogue.as_posix()+'"','-I'+str(runtime/'src/runtime/include'),'-I'+str(runtime/'src/runtime/src'),
        '-I'+str(runtime/'src/runtime/src/iop'),'-I'+str(W/'runtime/src'),'-c',str(private_kernel),'-o',str(obj)]
    reserve(16<<20);built=subprocess.run(obj_cmd,capture_output=True,text=True,timeout=180);(a.output/'object-build.log').write_text(built.stdout+built.stderr,encoding='utf-8',newline='\n')
    changed=[x['path'] for x in pins if sha(Path(x['path']))!=x['sha256']]
    ok=all(c['status']=='PASS' for c in cases) and built.returncode==0 and not changed and sha(exe)==binary_sha
    result={'status':'FRESH_IOP_PRODUCTION_CONNECTOR_SCHEDULER_PASS' if ok else 'REFUSED','classification':'actual frozen-v8 loader/scheduler/exit/IRQ component and disposable object bundle; not whole-game or SDK-complete/native credit',
        'native_credit':0,'source_pins':pins,'input_changes':changed,'binary_sha256':binary_sha,'build_command':build,'private_kernel_sha256':sha(private_kernel),'private_dispatch_sha256':sha(pending),
        'diagnostic_headers':[{'path':str(a.output/name),'sha256':sha(a.output/name),'difference':'source-pinned include relocation and fprintf/fflush BEFORE existing throw only; guards unchanged'} for name in diagnostic],
        'object':{'path':str(obj),'sha256':sha(obj) if obj.exists() else None,'build_command':obj_cmd,'exit_code':built.returncode},'cases':cases,'transparent_compression':compressed,'elapsed_seconds':time.perf_counter()-began,
        'required_outer_restore_calls':['ps2::iop::kernel::r192_fresh_before_machine_restore() BEFORE progressive machine_state','ps2::iop::kernel::r192_fresh_after_machine_restore(true) only after full machine/endtag success','ps2::iop::kernel::r192_fresh_after_machine_restore(false) on any later parser/sidecar failure'],
        'integration':'Replace ONLY the old disposable IOP kernel object with object.path; keep its frozen-v8 CPU and service42 objects, all70 PIC/registry objects. Compose outer restore hooks in owner system.cpp. Do not link this object with another kernel definition.',
        'remaining':['Actual whole-game cold boot/replay with this bundle has not run. Normal N is unchanged.',
            'Restored AOT legacy selection is preserved and explicitly unproved; no rawSHA/fresh SDK history is reconstructed.',
            'Unregistered callback contexts on reused code ranges refuse; source thread/IRQ registration generations are captured, other callback registration families remain bounded gaps.',
            'Original call_guest does not install a different targetGP; this proof preserves the v8 ABI and proves only a real JR_RA/NOP export IRQ transport, not arbitrary SDK callable ABI.',
            'loadcore#7 retains its original zero-return reference stub, logs OPEN and is not SDK release closure.',
            'The filename test exercises the original filename-derived naming branch of load_module_buffer; cdrom file IO/cold whole-game loading has not run here.',
            'Queue-load hardware correctness, SDK registry root/version/lifecycle transitions and arbitrary exception/yield IRQ paths remain separately OPEN.']}
    out=a.output/'RESULT.json';out.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n');print(json.dumps({'result':str(out),'sha256':sha(out),'status':result['status'],'object_exit':built.returncode,'cases':[c['status'] for c in cases]}));return 0 if ok else 1


if __name__=='__main__':raise SystemExit(main())
