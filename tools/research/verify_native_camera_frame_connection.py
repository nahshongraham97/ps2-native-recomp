"""Compare actual native camera/frame owners to pinned generated retail bodies.

The reference uses private RAM only in an ignored test binary. Native runtime
has no guest-memory shim. Results are typed-state projections, not full Cpu,
FCR31, memory-write-order, scene integration or native admission claims.
"""
from pathlib import Path
import argparse
import csv
import hashlib
import json
import os
import re
import shlex
import shutil
import subprocess
import sys

from verify_native_camera_subsystem import ELF, LISTING, HOST, need, pin, sha

ROOT = Path(__file__).resolve().parents[2]
# Joined to the pre-existing authenticated AOT census, not newly accepted hashes.
AOT_CENSUS = ('build/oracle-telemetry/phase6-leaf-ownership-02.json',
              'd59c07a4fd4b5739345a2c856de3532cdd8e464002e890b26503d499ebd1f15c')
AOT_UNITS = {'unit_0029.cpp': 'e495b376081ed0335d84ece19aa96672c6460af9e13b385ee608e9106e1a4b33',
             'unit_002A.cpp': 'e8a65a4158ca0926cc8694d66f52f49f6aeccfcd10f7f041203dc905f73d702f'}
CAPTURE_MANIFEST = 'b2fe00631ad85f3e51e0234e02a1a5b3db259c5a7384ccd0ecf5913e8beacf46'
BODIES = (
    ('frame', 0x1a7510, 0x1a7578, 'unit_0029.cpp', '624ddc6f40f31d001aa72a3b386aa2ce2754f08918b32646261f254348ac5906'),
    ('attach', 0x1aa0b0, 0x1aa11c, 'unit_002A.cpp', 'd1df69db48be32aa568ccabf299bfd146172d7ca4694b4925bc527003e5aee2b'),
    ('camera', 0x1a64a0, 0x1a650c, 'unit_0029.cpp', 'c27ee861601bb38263e7ee430287b8ca18c9d65e6807fc158eb6feb73cadb398'),
)

CHECKS = r'''
#include "phase7_track_b_capture.hpp"
namespace connection {
using CameraHandle=mka::retail::NativeRetailRendererCameraHandle;
using FrameHandle=mka::retail::NativeRetailRendererFrameHandle;
using ObjectHandle=mka::retail::NativeRetailRendererFrameObjectHandle;
constexpr unsigned frame_base=0x1400000,camera_base=0x1500000,stride=512,sentinel=0x69f34c;
unsigned transitions{},windows{},capture_fields{},capture_single_queue{},capture_external_tail{};
void need_connection(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
unsigned read(unsigned address){unsigned v;std::memcpy(&v,ps2::g_ram+address,4);return v;}
void write(unsigned address,unsigned v){std::memcpy(ps2::g_ram+address,&v,4);}
unsigned bits(float f){return std::bit_cast<unsigned>(f);}
void reference(unsigned object,unsigned parent,bool window=false,unsigned x=0,unsigned y=0){
 ps2::Cpu cpu{};cpu.r[4].d[0]=object;cpu.r[5].d[0]=parent;cpu.r[29].d[0]=0x1e00000;cpu.r[31].d[0]=0x100000;cpu.budget=100000;
 if(window){write(parent,x);write(parent+4,y);original_camera::camera(cpu);}else original_camera::attach(cpu);
 need_connection(cpu.pc==0x100000&&cpu.r[29].d[0]==0x1e00000,"reference complete return/SP");
}
void projected(){
 CameraFrameFixture native("@CONTENT@");RetailRendererCameraFailure cf;RetailRendererFrameFailure ff;
 std::array<FrameHandle,3> frames;std::array<CameraHandle,3> cameras;std::array<ObjectHandle,3> objects;
 write(sentinel,sentinel);write(sentinel+4,sentinel);
 for(unsigned i=0;i<3;++i){
  frames[i]=native.new_frame();cameras[i]=native.new_camera();objects[i]=object_identity(native.cameras,cameras[i]);
  const unsigned f=frame_base+i*stride,c=camera_base+i*stride;
  std::memset(ps2::g_ram+f,0,176);std::memset(ps2::g_ram+c,0,400);
  write(f+160,f);write(f+144,f+144);write(f+148,f+144);
 }
 auto compare=[&]{
  for(unsigned i=0;i<3;++i){
   const unsigned f=frame_base+i*stride,c=camera_base+i*stride;
   const auto frame=native.cameras->frame(cameras[i],cf);need_connection(bool(frame),"native association getter");
   const auto parent=read(c+4);need_connection(parent?frame->same_identity(frames[(parent-frame_base)/stride]):frame->empty(),"parent projection");
   auto actual=native.frames->objects(frames[i],ff);need_connection(bool(actual),"native frame object list");
   std::vector<unsigned> expected;unsigned prev=f+144;
   for(unsigned node=read(f+144);node!=f+144;node=read(node)){
    need_connection(expected.size()<3&&node>=camera_base+8&&(node-camera_base-8)%stride==0,"reference object list extent");
    need_connection(read(node+4)==prev,"reference object backlink");expected.push_back((node-camera_base-8)/stride);prev=node;
   }
   need_connection(read(f+148)==prev&&actual->size()==expected.size(),"object list cardinality/tail projection");
   for(unsigned n=0;n<expected.size();++n)need_connection((*actual)[n].same_identity(objects[expected[n]]),"object order projection");
   auto state=native.frames->observe(frames[i],ff);need_connection(state&&state->private_flags==ps2::g_ram[f+3],"frame/root flags projection");
  }
  auto dirty=native.frames->dirty_roots(ff);need_connection(bool(dirty),"native dirty roots");
  std::vector<unsigned> expected;unsigned prev=sentinel;
  for(unsigned node=read(sentinel);node!=sentinel;node=read(node)){
   need_connection(expected.size()<3&&read(node+4)==prev,"reference dirty linkage");
   need_connection(node>=frame_base+8&&(node-frame_base-8)%stride==0,"reference dirty root extent");
   expected.push_back((node-frame_base-8)/stride);prev=node;
  }
  need_connection(read(sentinel+4)==prev&&dirty->size()==expected.size(),"dirty queue cardinality/tail projection");
  for(unsigned n=0;n<expected.size();++n)need_connection((*dirty)[n].same_identity(frames[expected[n]]),"dirty queue order projection");
 };
 for(auto [camera,frame]:std::array<std::pair<unsigned,unsigned>,10>{{{0,3},{0,0},{1,0},{2,1},{0,0},{1,3},{0,1},{2,0},{2,3},{1,2}}}){
  reference(camera_base+camera*stride,frame==3?0:frame_base+frame*stride);
  need_connection(native.cameras->set_frame(cameras[camera],frame==3?FrameHandle{}:frames[frame],cf),"native source association");
  compare();++transitions;
 }
 for(unsigned i=0;i<3;++i)for(auto [x,y]:std::array<std::pair<unsigned,unsigned>,4>{{{0x40000000,0xc0800000},{0x3fd9999a,0x40200000},{0x40800000,0x41000000},{0x3f800000,0xbf800000}}}){
  reference(camera_base+i*stride,0x1d01000,true,x,y);
  need_connection(native.cameras->set_view_window(cameras[i],{std::bit_cast<float>(x),std::bit_cast<float>(y)},cf),"native connected window setter");
  const auto state=native.cameras->observe(cameras[i],cf);const unsigned c=camera_base+i*stride;
  need_connection(state&&bits(state->view_window.x)==read(c+104)&&bits(state->view_window.y)==read(c+108)&&bits(state->inverse_window.x)==read(c+112)&&bits(state->inverse_window.y)==read(c+116),"window/inverse projection");
  compare();++windows;
 }
 native.retire();
}
void captures(const std::filesystem::path &list){
 std::ifstream input(list);need_connection(bool(input),"selected capture input");std::string name;
 while(std::getline(input,name)){
  auto cap=track_b::load(name);need_connection(cap.pc==0x1a64a0,"selected camera source identity");
  need_connection(cap.after.pc==cap.before.r[31].w[0]&&std::memcmp(&cap.before.r[29],&cap.after.r[29],sizeof(ps2::Reg128))==0,"historical ordinary return/full SP");
  const auto rd=[&](unsigned at){return track_b::read(cap.ram,at);};const auto out=[&](unsigned at){return track_b::read(cap.exit_ram,at);};
  const auto c=cap.before.r[4].w[0],f=rd(c+4),window=cap.before.r[5].w[0];
  need_connection(f&&rd(f+160)==f&&std::to_integer<unsigned>(cap.ram[f+3])==15,"historical self-root dirty domain");
  need_connection(rd(f+144)==c+8&&rd(f+148)==c+8&&rd(c+8)==f+144&&rd(c+12)==f+144,"historical single attached object domain");
  for(unsigned address:{c+4,c+8,c+12,f+144,f+148,f+160})
   need_connection(rd(address)==out(address),"captured exit preserves parent/object membership/root");
  CameraFrameFixture native("@CONTENT@");RetailRendererCameraFailure cf;RetailRendererFrameFailure ff;
  auto frame=native.new_frame();auto camera=native.new_camera();need_connection(native.cameras->set_frame(camera,frame,cf),"actual owner association for captured fields");
  need_connection(native.cameras->set_view_window(camera,{std::bit_cast<float>(rd(window)),std::bit_cast<float>(rd(window+4))},cf),"actual captured-window typed update");
  auto state=native.cameras->observe(camera,cf);auto fs=native.frames->observe(frame,ff);
  need_connection(state&&bits(state->view_window.x)==out(c+104)&&bits(state->view_window.y)==out(c+108)&&bits(state->inverse_window.x)==out(c+112)&&bits(state->inverse_window.y)==out(c+116),"captured logical camera fields");
  need_connection(fs&&fs->private_flags==std::to_integer<unsigned>(cap.exit_ram[f+3]),"captured selected/root flags");
  auto parent=native.cameras->frame(camera,cf);need_connection(parent&&parent->same_identity(frame),"captured association preserved");
  auto members=native.frames->objects(frame,ff);need_connection(members&&members->size()==1&&(*members)[0].same_identity(object_identity(native.cameras,camera)),"captured object membership projection");
  if(rd(f+8)==sentinel)++capture_single_queue;else++capture_external_tail;
  ++capture_fields;native.retire();
 }
 need_connection(capture_fields==38,"all 38 selected historical captures required");
}
}
int main(int argc,char**argv){try{
 connection::need_connection(argc==2,"capture-selection path list required");
 camera_frame_association_cases("@CONTENT@");connection::projected();connection::captures(argv[1]);
 std::printf("PASS_NATIVE_CAMERA_FRAME_PROJECTION transitions=%u windows=%u historical_fields=%u historical_single_dirty_queue=%u historical_external_tail_unmapped=%u; state_projection_only full_Cpu_FCR31_write_order_scene_and_admission_OPEN\n",connection::transitions,connection::windows,connection::capture_fields,connection::capture_single_queue,connection::capture_external_tail);return 0;
}catch(const std::exception&e){std::fprintf(stderr,"FAIL_NATIVE_CAMERA_FRAME_PROJECTION %s\n",e.what());return 1;}}
'''


def validate_selection(selection, manifest):
    need(selection['schema']=='mkar.native-camera-frame.capture-selection.v1','capture selection schema')
    captures=selection['captures'];need(len(captures)==38,'38 selected capture records')
    need(len({str(Path(r['path']).resolve())for r in captures})==38 and
         len({r['sha256']for r in captures})==38,'38 distinct capture paths and hashes')
    groups=('captures_cam','captures_cam_s60','captures_cam_multi')
    expected={(g,r['path']):(r['bytes'],r['sha256'])for g in groups for r in manifest[g]
              if '/cap_001a64a0_'in '/'+r['path']}
    selected={}
    for row in captures:
        key=(row['published_manifest_group'],row['published_capture_path'])
        need(key in expected and expected[key]==(row['bytes'],row['sha256']),'published capture membership')
        need(Path(row['path']).resolve()==(ROOT/'b/phase7-capture-run'/key[1]).resolve(),'published capture path join')
        selected[key]=(row['bytes'],row['sha256'])
    need(selected==expected and len(selected)==38,'complete selected published camera cohort')
    return captures


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    for name in ('parent-root', 'compiler', 'output', 'capture-selection'):
        ap.add_argument('--' + name, type=Path, required=True)
    args = ap.parse_args()
    parent = args.parent_root.resolve(); output = args.output.resolve()
    need(output.is_relative_to(ROOT / 'b/phase7-capture-run/native-camera-frame-01') and not output.exists(), 'fresh private output')
    need(shutil.disk_usage(ROOT).free >= 12 * (1 << 30), '12GiB reserve')
    elf=parent/'research/inputs/SLUS_214.10';listing=parent/'research/runs/ghidra-function-coverage-20261118/export-20261118-230132-1875-18160/instructions.tsv'
    need(sha(elf)==ELF and sha(listing)==LISTING,'pinned original inputs changed')
    from classify_stack_call_boundaries import reader
    census=ROOT/AOT_CENSUS[0];need(sha(census)==AOT_CENSUS[1],'pre-existing AOT census identity')
    measured=json.loads(census.read_bytes())['aot']['all_measured_source_sha256']
    original=reader(elf.read_bytes());refs=[];words={};units=[]
    for name,lo,hi,unit,digest in BODIES:
        need(hashlib.sha256(original(lo,hi-lo)).hexdigest()==digest,'original body changed')
        words.update({pc:original(pc,4) for pc in range(lo,hi,4)})
        path=ROOT/'build/generated-aot/phase4-all'/unit
        need(sha(path)==measured.get(unit)==AOT_UNITS[unit],'generated reference differs from authenticated AOT census')
        units.append(path);text=path.read_text();start=text.index(f'L_{lo:08X}:')
        end=next(start+m.start() for m in re.finditer(r'^L_([0-9A-F]{8}):',text[start:],re.M) if int(m[1],16)>=hi)
        body=text[start:end];delay=f'L_{hi-4:08X}:';need(delay in body and 'goto '+delay[:-1] not in body,'terminal raw-delay scope changed');body=body[:body.index(delay)]
        if name!='frame':
            body,count=re.subn(r'if \(depth < kMaxDepth\) unit_0029\(c, 265u, depth \+ 1u\); else \{ c.pc = 0x001A7510u; return; \}', 'frame(c);', body)
            need(count==1,'original frame call bundle changed')
        need(not re.search(r'\b(?:unit_\w+|dispatch|call)\(',body),'unbound reference call')
        refs.append('static void '+name+'(Cpu&c){'+body.replace('stop(c,','reference_invalid_return(c,')+'}\n')
    for row in csv.DictReader(listing.open(encoding='utf-8-sig'),delimiter='\t'):
        pc=int(row['address'],16)
        if pc in words:need(bytes.fromhex(row['bytes'])==words.pop(pc),'listing word differs')
    need(not words,'listing coverage gap')
    selection=json.loads(args.capture_selection.read_bytes())
    manifest=ROOT/'research/runs/phase7_capture_hashes.json'
    need(sha(manifest)==selection['published_manifest']['sha256']==CAPTURE_MANIFEST,'published capture manifest identity')
    published=json.loads(manifest.read_bytes());captures=validate_selection(selection,published)
    selection_controls=[]
    for name in ('duplicate_capture','forged_published_path','forged_capture_hash'):
        plant=json.loads(json.dumps(selection))
        if name=='duplicate_capture':plant['captures'][-1]=dict(plant['captures'][0])
        elif name=='forged_published_path':plant['captures'][0]['published_capture_path']='forged/cap_001a64a0_0000.bin'
        else:plant['captures'][0]['sha256']='0'*64
        try:validate_selection(plant,published)
        except ValueError as error:selection_controls.append(dict(name=name,status='REJECTED',reason=str(error)))
        else:raise AssertionError('capture selection plant admitted: '+name)
    for row in captures:
        path=Path(row['path']);need(path.stat().st_size==row['bytes'] and sha(path)==row['sha256'],'capture identity changed')
    fixture=ROOT/'tests/test_retail_renderer_cameras.cpp';ninja=ROOT/'b/build.ninja';raw=ninja.read_text();start=raw.index('build test_native_retail_renderer_cameras.exe:')
    libs=next(line.strip().split(' = ',1)[1]for line in raw[start:].splitlines()if line.strip().startswith('LINK_LIBRARIES ='))
    libraries=[x if x.startswith('-')else str((ROOT/'b'/x).resolve())for x in shlex.split(libs)]
    includes=[ROOT/'runtime/include',ROOT/'runtime/src',ROOT/'b/native-retail-startup',ROOT/'tests']
    inc=next(line for line in raw.splitlines()if line.strip().startswith('INCLUDES =')and'native-retail-startup'in line)
    includes += [Path(x[2:])for x in shlex.split(inc.split(' = ',1)[1])if x.startswith('-I')and Path(x[2:]).is_dir()]
    production=[ROOT/'runtime/src/native/retail_renderer_cameras.cpp',ROOT/'runtime/src/native/retail_renderer_frames.cpp']
    sources=[Path(__file__),Path(__file__).with_name('verify_native_camera_subsystem.py'),Path(__file__).with_name('classify_stack_call_boundaries.py'),census,manifest,elf,listing,args.capture_selection,ninja,fixture,args.compiler,*units,*production,*[Path(x)for x in libraries if not x.startswith('-')],*list((ROOT/'runtime/include/mka').glob('*.hpp')),*list((ROOT/'runtime/include/ps2').glob('*.hpp')),*list((ROOT/'tests').glob('*capture*.hpp'))]
    before=[pin(path)for path in dict.fromkeys(sources)];output.mkdir(parents=True);(output/'content').mkdir()
    lines=output/'captures.txt';lines.write_text(''.join(row['path']+'\n'for row in captures),encoding='utf8',newline='\n')
    head=HOST[:HOST.index('static unsigned bits')]
    source=(head+CHECKS).replace('@FIXTURE@',fixture.as_posix()).replace('@REFERENCE@','\n'.join(refs)).replace('@CONTENT@',(output/'content').as_posix())
    cpp=output/'connection.cpp';cpp.write_text(source,encoding='utf8',newline='\n');shutil.copyfile(ROOT/'b/SDL2.dll',output/'SDL2.dll')
    env=dict(os.environ,PATH=str(args.compiler.resolve().parent)+os.pathsep+os.environ['PATH']);commands=[]
    variants=[('positive',None,None),('missing_attach_dirty',1,('  mark_record(next.get());','  /* planted missing attach dirty */')),
              ('same_frame_noop',0,(' if(frame.empty()&&r->frame_link.frame_.empty()){',' if(!frame.empty()&&r->frame_link.frame_.same_identity(frame))return true;\n if(frame.empty()&&r->frame_link.frame_.empty()){')),
              ('missing_window_frame',0,(' if(!r->frame_link.frame_.empty()){',' if(false){')),
              ('missing_session_check',1,(' if(object.session_.lock().get()!=lease.get())\n  return fail(f,RetailRendererFrameError::foreign_owner,"1AA0B0 camera and frame require the same native Session");',' /* planted missing creator Session check */'))]
    for name,index,mutation in variants:
        selected=list(production)
        if mutation:
            text=production[index].read_text();need(text.count(mutation[0])==1,'mutation must affect exactly one actual runtime site')
            changed=output/(name+'.cpp');changed.write_text(text.replace(*mutation),encoding='utf8',newline='\n');selected[index]=changed
        binary=output/(name+'.exe');argv=[str(args.compiler),'-std=c++20','-O1','-mavx2','-ffp-contract=off','-static-libgcc','-static-libstdc++',*['-I'+str(path.resolve())for path in dict.fromkeys(includes)],str(cpp),*map(str,selected),*libraries,'-o',str(binary)]
        build=subprocess.run(argv,capture_output=True,env=env,timeout=180);(output/(name+'-build.log')).write_bytes(build.stdout+build.stderr)
        item=dict(name=name,build_argv=argv,build_exit=build.returncode,source_mutation=mutation,expected_exit=0 if name=='positive'else 1)
        if build.returncode==0:
            run=subprocess.run([str(binary),str(lines)],capture_output=True,env=env,timeout=180);(output/(name+'.log')).write_bytes(run.stdout+run.stderr)
            item.update(exit_code=run.returncode,stdout=run.stdout.decode(errors='replace'),stderr=run.stderr.decode(errors='replace'),binary=pin(binary))
        commands.append(item);print(json.dumps({k:item.get(k)for k in ('name','build_exit','exit_code','expected_exit')}),flush=True)
    changed=[row['path']for row in before if sha(row['path'])!=row['sha256']]
    result=dict(status='PASS_NATIVE_CAMERA_FRAME_PROJECTED_EFFECTS'if len(commands)==5 and not changed and all(row['build_exit']==0 and row.get('exit_code')==row['expected_exit']for row in commands)else'FAIL_NATIVE_CAMERA_FRAME_PROJECTED_EFFECTS',source_pins=before,input_changes=changed,selected_capture_pins=captures,checks=commands,ownership_admission='separate E1 gate',native_credit_added=0,oracle_runs=0,limits=['Typed camera/frame/list state projections only; not full Cpu/FCR31 or guest-write-order equivalence.','38 captures have already-dirty self roots and one attached object;25 retain an unmodelled external dirty tail. No current same-to-end replay claim.','No1AA0B0 historical invocation, attached clean-root window case, real scene/game callsite, plugins, view/frustum synchronization, GPU frames or physical audio is established.','Native SSE numeric policy remains unchanged; the earlier documented EE edge differences remain open.'])
    result['capture_selection_controls']=selection_controls
    result['authenticated_reference_units']=AOT_UNITS
    result['authenticated_reference_census']=dict(path=AOT_CENSUS[0],sha256=AOT_CENSUS[1])
    (output/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8',newline='\n');print(result['status']);return 0 if result['status'].startswith('PASS_')else 1


if __name__=='__main__':raise SystemExit(main())
