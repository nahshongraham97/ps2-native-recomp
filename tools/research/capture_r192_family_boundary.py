"""Observe one actual owned R192-v8 stub boundary; never modify N or the oracle.

Relinks an observer-only host object with pinned existing owned libraries.
The observer copies const Cpu/RAM once, then original execution continues.
The established headless driver owns the disposable card and 90-new-frame cap.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys

W=Path(__file__).resolve().parents[2]
N_SHA='8823021a23e2a3fdd3b2dc4ca0c8eac9e7359beb3e5bce489b965587a0c2479b'


def pin(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        for data in iter(lambda:f.read(4*1024*1024),b''):h.update(data)
    return dict(path=str(path.resolve()),size=path.stat().st_size,sha256=h.hexdigest())


def need(ok,reason):
    if not ok:raise ValueError(reason)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['candidate-build','source-fork','state','card-copy','output']:
        p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--additional-frames',type=int,default=90)
    a=p.parse_args();out=a.output.resolve();n=a.candidate_build.resolve()
    need(out.is_relative_to((W/'build/oracle-telemetry').resolve()) and not out.exists(),'fresh_owned_output_required')
    need(n.is_relative_to((W/'build').resolve()),'candidate_must_be_owned')
    need(pin(n/'out/ps2run.exe')['sha256']==N_SHA,'N_reference_binary_changed')
    out.mkdir(parents=True);run=out/'run';run.mkdir();capture=out/'capture';capture.mkdir()
    source=n/'src/host/main.cpp';text=source.read_text()
    need(text.count('        if (ps2::hook::is_enabled()) {')==1,'observer_registration_seam_changed')
    need(0<a.additional_frames<=600,'bounded_capture_frame_request_required')
    text=text.replace('#include "mka_combat_hooks.hpp"','#include "mka_combat_hooks.hpp"\n#include "kernel.hpp"\n#include <fstream>\n#include <map>\n#include <tuple>\n'+OBSERVER,1)
    text=text.replace('        if (ps2::hook::is_enabled()) {','        ps2::hook::set_observer_hooks_enabled(true);\n        ps2::hook::register_observer(0x008A5140u, &capture_family_boundary, "family_boundary_copy_only", 0, 1);\n        ps2::hook::register_observer(0x008A5170u, &capture_family_boundary, "family_natural_resume_copy_only", 1, 0);\n        if (ps2::hook::is_enabled()) {',1)
    copied=out/'observer_main.cpp';copied.write_text(text,encoding='utf8',newline='\n')
    config=json.loads((n/'CONFIGURE.json').read_text());cxx=config['compiler'];obj=out/'observer_main.obj'
    compile_args=[cxx,'-std=c++20','-mavx2','-ffp-contract=off','-O3','-DNDEBUG','-DPS2_WINDOW','-DPS2_GPU','-DPS2_PROFILE',
                  '-I'+str(n/'src/runtime/include'),'-I'+str(n/'src/runtime/src'),'-I'+str(n/'src/host'),
                  '-isystem',str(n/'src/third_party/sdl2/include/SDL2'),'-c',str(copied),'-o',str(obj)]
    objects=[n/'out/CMakeFiles/ps2run.dir/host'/name for name in ['mka_unit_wrappers.cpp.obj','r192_aot_modules/loader.cpp.obj','profile_win.cpp.obj','window.cpp.obj','keymap.cpp.obj','stretch.cpp.obj']]
    libraries=[n/'out/libps2game.a',n/'out/libps2runtime.a',n/'src/third_party/sdl2/lib/libSDL2.dll.a',n/'src/third_party/wgpu-native/lib/libwgpu_native.a']
    inputs=[pin(x) for x in [source,n/'out/ps2run.exe',*objects,*libraries,a.state,a.card_copy,Path(__file__)]]
    binary=run/'ps2run.exe'
    link_args=[cxx,'-mavx2','-ffp-contract=off','-O3','-DNDEBUG','-mwindows','-static-libgcc','-static-libstdc++',
               *['-Wl,--wrap=_ZN3gen9unit_'+u+'ERN3ps23CpuEjj' for u in ['0018','0020','0025']],
               str(obj),*[str(x) for x in objects],'-o',str(binary),*[str(x) for x in libraries],'-lbcrypt','-lwinmm',
               *['-l'+x for x in ['ws2_32','userenv','ntdll','synchronization','pthread','m','kernel32','user32','gdi32','winspool','shell32','ole32','oleaut32','uuid','comdlg32','advapi32']]]
    commands=[]
    for label,args in [('compile-observer-only',compile_args),('link-owned-fixture',link_args)]:
        cp=subprocess.run(args,capture_output=True);(out/(label+'.log')).write_bytes(cp.stdout+cp.stderr)
        commands.append(dict(name=label,argv=args,exit=cp.returncode));need(cp.returncode==0,label+'_failed')
    for name in ['SDL2.dll','dxcompiler.dll','dxil.dll']:shutil.copyfile(n/'out'/name,run/name)
    package=run/'aot-modules/ee';package.mkdir(parents=True)
    for file in (n/'out/aot-modules/ee').iterdir():
        if file.is_file():shutil.copyfile(file,package/file.name)
    helper=W/'tools/research/replay_r192_aot_sessions.py'
    argv=[sys.executable,str(helper),'--candidate',str(binary),'--expected-binary-sha',pin(binary)['sha256'],
          '--source-fork',str(a.source_fork.resolve()),'--state',str(a.state.resolve()),'--card-copy',str(a.card_copy.resolve()),
          '--additional-frames',str(a.additional_frames),'--output',str(out/'probe')]
    env=dict(os.environ);env['R192_FAMILY_CAPTURE_DIR']=str(capture)
    cp=subprocess.run(argv,env=env,capture_output=True);(out/'driver.log').write_bytes(cp.stdout+cp.stderr)
    commands.append(dict(name='actual-state90-with-observer',argv=argv,exit=cp.returncode))
    changed=[x['path'] for x in inputs if pin(Path(x['path']))['sha256']!=x['sha256']]
    files=[pin(x) for x in capture.iterdir() if x.is_file()]
    required=['Cpu.bin','RAM.bin','SPR.bin','Thread.bin']
    record=dict(status='PASS_ACTUAL_BOUNDARY_CAPTURE' if cp.returncode==0 and all((capture/x).is_file() for x in required) and not changed else 'REFUSED',
                commands=commands,input_pins=inputs,input_changes=changed,capture_files=files,observer_binary=pin(binary),
                additional_frames=a.additional_frames,natural_resume_observed=(capture/'Cpu-resume.bin').exists(),
                scope='Actual original-call boundary during bounded copied-state execution. Read-only observers copy Cpu/RAM/SPR/actual kernel thread ID, including a natural resume only if observed; no family replacement or N/protected write.')
    raw=(json.dumps(record,indent=2)+'\n').encode();(out/'RESULT.json').write_bytes(raw)
    print(json.dumps(dict(status=record['status'],result=str(out/'RESULT.json'),sha256=hashlib.sha256(raw).hexdigest())))
    return 0 if record['status']=='PASS_ACTUAL_BOUNDARY_CAPTURE' else 1


OBSERVER=r'''
namespace {
void capture_family_boundary(const ps2::hook::GuestCallContext& ctx){
 static bool captured=false,resumed=false,low=false;
 struct Prior {ps2::Cpu cpu;std::vector<unsigned char> ram,spr;std::uint64_t count;};
 using Key=std::tuple<int,std::uint64_t,std::uint32_t>;
 static std::map<Key,Prior> previous;
 const std::int32_t thread=ps2::kernel::current_thread();
 const Key key{thread,ctx.cpu.r[29].d[0],ctx.cpu.r[19].w[0]};
 if(ctx.pc==0x008A5140u&&!resumed&&(previous.contains(key)||previous.size()<8)){
  auto& prior=previous[key];prior.cpu=ctx.cpu;prior.count=ctx.call_count;
  prior.ram.assign(ps2::g_ram,ps2::g_ram+ps2::kRamSize);prior.spr.assign(ps2::g_spr,ps2::g_spr+ps2::kScratchpadSize);
 }
 const char* suffix=nullptr;
 if(ctx.pc==0x008A5170u&&!resumed){suffix="-resume";resumed=true;}
 else if(ctx.pc==0x008A5140u&&!captured){suffix="";captured=true;}
 else if(ctx.pc==0x008A5140u&&!low&&ctx.cpu.r[10].d[0]<100000&&ctx.cpu.budget<static_cast<std::int64_t>(ctx.cpu.r[10].d[0]*48+16)){suffix="-low";low=true;}
 else return;
 const char* output=std::getenv("R192_FAMILY_CAPTURE_DIR");if(!output||!*output)return;
 auto write=[&](const char*name,const void*data,std::size_t size){std::ofstream f(std::filesystem::path(output)/(std::string(name)+suffix+".bin"),std::ios::binary);f.write(static_cast<const char*>(data),size);if(!f)throw std::runtime_error("[family-capture] output write failed");};
 write("Cpu",&ctx.cpu,sizeof ctx.cpu);write("RAM",ps2::g_ram,ps2::kRamSize);write("SPR",ps2::g_spr,ps2::kScratchpadSize);write("Thread",&thread,sizeof thread);
 if(ctx.pc==0x008A5170u){
  const auto prior=previous.find(key);
  if(prior!=previous.end()){
   suffix="-prior";write("Cpu",&prior->second.cpu,sizeof ctx.cpu);write("RAM",prior->second.ram.data(),prior->second.ram.size());write("SPR",prior->second.spr.data(),prior->second.spr.size());write("Thread",&thread,sizeof thread);
   std::ofstream pair(std::filesystem::path(output)/"PAIR.json",std::ios::binary);
   pair<<"{\"key\":\"actual thread/SP/object\",\"thread\":"<<thread<<",\"stack\":"<<ctx.cpu.r[29].d[0]<<",\"object\":"<<ctx.cpu.r[19].w[0]<<",\"last_initial_observer_count\":"<<prior->second.count<<",\"first_resume_observer_count\":"<<ctx.call_count<<"}\n";suffix="-resume";
  }
  previous.clear();
 }
 std::fprintf(stderr,"[family-capture] copied original boundary%s pc=%08X ra=%08X sp=%08X thread=%d budget=%d; original execution continues\n",suffix,ctx.pc,ctx.ra,ctx.sp,thread,ctx.cpu.budget);
}
}
'''

if __name__=='__main__':raise SystemExit(main())
