"""Bounded differential expansion of existing typed native camera/frame services.

Production behavior is unchanged. Original reference code is copied only into
an ignored test fixture; seeded snapshots are not historical camera invocations.
SSE/EE numerical differences are retained, never relabelled as exact parity.
"""
from pathlib import Path
import argparse,csv,hashlib,json,os,re,shlex,shutil,struct,subprocess,sys

ROOT=Path(__file__).resolve().parents[2]
ELF='a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f'
LISTING='4611757f817e6c4ace87bb2d1203bca9c5d8523c33e0ae3e086890ab4b5ce148'
BODIES={0x1a64a0:(0x1a650c,'c27ee861601bb38263e7ee430287b8ca18c9d65e6807fc158eb6feb73cadb398'),0x1a7510:(0x1a7578,'624ddc6f40f31d001aa72a3b386aa2ce2754f08918b32646261f254348ac5906')}

def need(v,s):
    if not v:raise ValueError(s)
def sha(p):
    h=hashlib.sha256()
    with Path(p).open('rb')as f:
        for b in iter(lambda:f.read(4<<20),b''):h.update(b)
    return h.hexdigest()
def pin(p):return dict(path=str(Path(p).resolve()),sha256=sha(p))

HOST=r'''
#define main existing_camera_component_main_not_invoked
#include "@FIXTURE@"
#undef main
#include "ps2/cpu.hpp"
#include "ps2/memory.hpp"
#include "ps2/fpu.hpp"
#include "ps2/generated.hpp"
#include <array>
#include <random>
#include <iostream>
namespace ps2 {alignas(64) std::uint8_t g_ram[kRamSize+64],g_spr[kScratchpadSize];bool g_watch_on=false;std::uint32_t g_watch_base{},g_watch_end{};void watch_hit(std::uint32_t,std::uint32_t,const char*){};void watch_span(std::uint32_t,std::uint32_t,const char*){}
#define BADLD(B,T) T slow_ld##B(std::uint32_t){throw std::runtime_error("unexpected reference read outside private RAM");}
BADLD(8,std::uint8_t) BADLD(16,std::uint16_t) BADLD(32,std::uint32_t) BADLD(64,std::uint64_t)
void slow_ld128(std::uint32_t,Reg128&){throw std::runtime_error("reference quad read bounds");}
#define BADST(B,T) void slow_st##B(std::uint32_t,T){throw std::runtime_error("unexpected reference write outside private RAM");}
BADST(8,std::uint8_t) BADST(16,std::uint16_t) BADST(32,std::uint32_t) BADST(64,std::uint64_t)
void slow_st128(std::uint32_t,const Reg128&){throw std::runtime_error("reference quad write bounds");}}
namespace original_camera {
using namespace ps2;
void reference_invalid_return(Cpu&,std::uint32_t,const char*){throw std::runtime_error("original invalid return");}
@REFERENCE@
}
static unsigned bits(float x){return std::bit_cast<unsigned>(x);}
static void word(unsigned at,unsigned v){std::memcpy(ps2::g_ram+at,&v,4);}
static unsigned word(unsigned at){unsigned v;std::memcpy(&v,ps2::g_ram+at,4);return v;}
static std::string mode;
static unsigned exact{},finite_match{},finite_difference{},edge_match{},edge_difference{},seeded{},entry_matches{},frames_checked{},errors_checked{};
static void must(bool v,const char*s){if(!v)throw std::runtime_error(s);}
static void camera_case(const std::shared_ptr<NativeRetailRendererCameraModule>&m,const NativeRetailRendererCameraHandle&h,ps2::Cpu seed,unsigned x,unsigned y,unsigned domain,const char*label){
 constexpr unsigned camera=0x1d00000,input=0x1d01000,stack=0x1e00000;
 std::memset(ps2::g_ram+camera,0,512);word(input,x);word(input+4,y);word(camera+4,0);
 auto c=seed;c.pc=0x1a64a0;c.r[4].d[0]=camera;c.r[5].d[0]=input;c.r[29].d[0]=stack;c.r[31].d[0]=0x100000;c.budget=100000;
 original_camera::camera(c);must(c.pc==0x100000&&c.r[29].d[0]==stack&&c.r[2].d[0]==camera,"original camera complete null-parent return/stack identity");
 RetailRendererCameraFailure f;must(m->set_view_window(h,{std::bit_cast<float>(x),std::bit_cast<float>(y)},f),"real native camera setter failed");auto o=m->observe(h,f);must(bool(o),"real native camera observation failed");
 if(mode=="--planted-window")o->view_window.x=std::bit_cast<float>(bits(o->view_window.x)^1u);
 if(mode=="--planted-reciprocal")o->inverse_window.x=std::bit_cast<float>(bits(o->inverse_window.x)^1u);
 must(bits(o->view_window.x)==word(camera+104)&&bits(o->view_window.y)==word(camera+108),"raw window payload mismatch");
 const bool same=bits(o->inverse_window.x)==word(camera+112)&&bits(o->inverse_window.y)==word(camera+116);
 if(domain==0){must(same,"exact representable finite camera domain mismatch");++exact;}
 else if(domain==1){if(same)++finite_match;else++finite_difference;}
 else{if(same)++edge_match;else++edge_difference;}
 std::printf("CAMERA label=%s domain=%u x=%08X y=%08X source_inverse=%08X,%08X native_inverse=%08X,%08X equal=%u source_FCR31=%08X source_parent=null scope=logical_fields_only\n",label,domain,x,y,word(camera+112),word(camera+116),bits(o->inverse_window.x),bits(o->inverse_window.y),same,c.fcr31);
}
static void frame_cases(Fixture&x){
 auto core=x.core();check(renderer_required::construct_core_plugin_0403(*core,x.f),"real frame module constructor");auto m=core->frames(x.f);must(bool(m),"real frame module missing");RetailRendererFrameFailure f;
 std::vector<NativeRetailRendererFrameHandle>handles;for(unsigned i=0;i<3;++i){auto h=m->create_base_frame(f);must(bool(h),"real frame allocation");handles.push_back(*h);}
 constexpr unsigned sentinel=0x69f34c,base=0x1600000;word(sentinel,sentinel);word(sentinel+4,sentinel);
 for(unsigned i=0;i<3;++i){std::memset(ps2::g_ram+base+i*512,0,176);word(base+i*512+160,base+i*512);}
 for(unsigned index:{0u,1u,0u,2u,1u}){
  ps2::Cpu c{};c.r[4].d[0]=base+index*512;c.r[31].d[0]=0x100000;c.budget=10000;original_camera::frame(c);must(c.pc==0x100000&&c.r[2].w[0]==base+index*512,"original frame return identity");
  must(m->mark_dirty(handles[index],f),"native frame dirty operation");auto roots=m->dirty_roots(f);must(bool(roots),"native dirty queue observation");std::vector<unsigned>order;
  for(unsigned node=word(sentinel),prev=sentinel;node!=sentinel;){must(order.size()<3&&word(node+4)==prev,"source list linkage/bounds");order.push_back((node-8-base)/512);prev=node;node=word(node);}
  if(mode=="--planted-frame-queue")order.push_back(0);
  must(order.size()==roots->size(),"frame queue cardinality mismatch");for(unsigned i=0;i<order.size();++i)must((*roots)[i].same_identity(handles[order[i]]),"frame queue order/identity mismatch");
  for(unsigned i=0;i<3;++i){auto o=m->observe(handles[i],f);must(o&&o->private_flags==ps2::g_ram[base+i*512+3],"frame flags mismatch");}++frames_checked;
 }
 auto foreign=Fixture(x.startup.content.root()); // never reached: no invented foreign-root API
}
int main(int argc,char**argv){try{
 must(argc>=3&&argc<=4,"owned content root and Cpu seed file required");mode=argc==4?argv[3]:"";must(mode.empty()||mode=="--planted-window"||mode=="--planted-reciprocal"||mode=="--planted-frame-queue","unknown plant");
 must(std::fegetround()==FE_TONEAREST,"existing nearest rounding required, never changed");
 const std::filesystem::path root(argv[1]);Fixture fixture(root);auto m=fixture.module();auto h=create(m);ps2::Cpu base{};
 for(int e=-20;e<=20;++e){const unsigned x=unsigned(127+e)<<23,y=(unsigned(127-e)<<23)|0x80000000u;camera_case(m,h,base,x,y,0,"power2");}
 std::mt19937 rng(0x1a64a0);for(unsigned i=0;i<128;++i){auto value=[&](){return (rng()&0x807fffffu)|((100+rng()%55)<<23);};camera_case(m,h,base,value(),value(),1,"seeded_finite");}
 constexpr unsigned edges[]={0,0x80000000,1,0x007fffff,0x00800000,0x7f7fffff,0xff7fffff,0x7f800000,0xff800000,0x7fc00000,0x7f800001,0xffffffff};
 for(unsigned i=0;i<std::size(edges);++i)camera_case(m,h,base,edges[i],edges[(i+1)%std::size(edges)],2,"edge");
 std::ifstream seeds(argv[2],std::ios::binary);for(unsigned i=0;i<61;++i){ps2::Cpu c{};seeds.read(reinterpret_cast<char*>(&c),sizeof c);must(bool(seeds),"61 prefix Cpu seed extent");entry_matches+=c.pc==0x1a64a0;camera_case(m,h,c,0x40000000,0xc0800000,0,"snapshot_seeded_constructed_ABI");++seeded;}must(seeds.peek()==EOF,"extra Cpu seed data");
 frame_cases(fixture);
 RetailRendererCameraFailure f;must(m->destroy_base_camera(h,f),"native camera retire");must(!m->set_view_window(h,{1,1},f)&&f.kind==RetailRendererCameraError::retired_camera,"retired camera refusal");++errors_checked;
 std::printf("PASS_NATIVE_CAMERA_DIFFERENTIAL exact=%u finite_match=%u finite_difference=%u edge_match=%u edge_difference=%u snapshot_seeded=%u historical_entry_matches=%u frame_queue_steps=%u lifetime_controls=%u new_implementation_credit=0 scope=null_parent_camera_and_separate_frame_effects\n",exact,finite_match,finite_difference,edge_match,edge_difference,seeded,entry_matches,frames_checked,errors_checked);return 0;
}catch(const std::exception&e){std::fprintf(stderr,"FAIL_NATIVE_CAMERA_DIFFERENTIAL %s\n",e.what());return 1;}}
'''.replace(' auto foreign=Fixture(x.startup.content.root()); // never reached: no invented foreign-root API\n','')


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    for n in ('parent-root','reference-fork','compiler','output'):ap.add_argument('--'+n,type=Path,required=True)
    a=ap.parse_args();P=a.parent_root.resolve();F=a.reference_fork.resolve();O=a.output.resolve();need(O.is_relative_to(ROOT/'build/oracle-telemetry')and not O.exists(),'fresh_owned_output_required');need(shutil.disk_usage(O.parent).free>12<<30,'12GiB_reserve_required')
    elf=P/'research/inputs/SLUS_214.10';listing=P/'research/runs/ghidra-function-coverage-20261118/export-20261118-230132-1875-18160/instructions.tsv';need(sha(elf)==ELF and sha(listing)==LISTING,'original_image_or_listing_changed')
    sys.path.insert(0,str(ROOT/'tools/research'));from classify_stack_call_boundaries import reader
    original=reader(elf.read_bytes());wanted={};
    for lo,(hi,digest)in BODIES.items():need(hashlib.sha256(original(lo,hi-lo)).hexdigest()==digest,'original_body_changed');wanted.update({pc:original(pc,4) for pc in range(lo,hi,4)})
    joined=[]
    for r in csv.DictReader(listing.open(encoding='utf-8-sig'),delimiter='\t'):
        pc=int(r['address'],16)
        if pc in wanted:need(bytes.fromhex(r['bytes'])==wanted.pop(pc),'listing_word_mismatch');joined.append(r)
    need(not wanted,'listing_body_gap')
    unit=F/'build/generated-aot/phase4-all/unit_0029.cpp';text=unit.read_text();functions=[]
    for lo,(hi,_) in reversed(list(BODIES.items())):
        start=text.index(f'L_{lo:08X}:');labels=list(re.finditer(r'^L_([0-9A-F]{8}):',text[start:],re.M));end=next(start+m.start() for m in labels if int(m[1],16)>=hi);body=text[start:end]
        # Generated units expose the raw delay PC as a second entry. Normal
        # JR entry executes its delay INLINE and returns; that duplicate is
        # not reachable from this complete function's authenticated entry.
        delay=f'L_{hi-4:08X}:';need(delay in body and 'goto '+delay[:-1]not in body,'terminal_delay_entry_scope_changed')
        terminal=body.index(delay);need('c.pc = t; return;'in body[:terminal],'inline_JR_and_delay_missing');body=body[:terminal]
        if lo==0x1a64a0:
            pat=r'if \(depth < kMaxDepth\) unit_0029\(c, 265u, depth \+ 1u\); else \{ c.pc = 0x001A7510u; return; \}'
            body,n=re.subn(pat,'frame(c);',body);need(n==1,'original_camera_call_bundle_changed')
        need(not re.search(r'\b(?:unit_\w+|dispatch|call)\(',body),'unjoined_original_call')
        body=body.replace('stop(c,','reference_invalid_return(c,')
        functions.append('static void '+('camera'if lo==0x1a64a0 else'frame')+'(Cpu&c){'+body+'}\n')
    manifest=P/'launchers/oracle_original_manifest.json';pins=json.loads(manifest.read_text());state_root=P.parent.parent/'milestone_work_verified_playable_20261110/games/mkarmageddon/disc/states';states=[];cpu_seeds=bytearray()
    for name,v in sorted(pins.items()):
        if name.startswith('games/mkarmageddon/disc/states/')and name.endswith('.state'):
            q=state_root/Path(name).name;need(q.stat().st_size==v['size']and sha(q)==v['sha256'],'original_snapshot_changed');states.append(pin(q))
            with q.open('rb')as f:head=f.read(20+1456)
            need(head[:8]==b'PS2STATE'and struct.unpack_from('<I',head,8)[0]==8 and head[12:20]==b'ee\0\0\0\0\0\0','v8_Cpu_prefix_changed');cpu_seeds+=head[20:]
    need(len(states)==61,'61_states_required')
    ledger=ROOT/'research/ledger/functions.csv';admitted=[r for r in csv.DictReader(ledger.open(encoding='utf-8-sig'))if r['ownership_status'].startswith('verified-')];need(len(admitted)>=18 and any(r['address']=='001a64a0'for r in admitted),'current18_ownership_admission_changed')
    ninja=F/'b/build.ninja';raw=ninja.read_text();start=raw.index('build test_native_retail_renderer_cameras.exe:');line=next(x.strip().split(' = ',1)[1] for x in raw[start:].splitlines()if x.strip().startswith('LINK_LIBRARIES ='))
    libraries=[]
    for x in shlex.split(line):libraries.append(x if x.startswith('-')else str((F/'b'/x).resolve()))
    includes=[ROOT/'runtime/include',ROOT/'runtime/src',F/'b/native-retail-startup',ROOT/'tests'];include_line=next(x for x in raw.splitlines() if x.strip().startswith('INCLUDES =')and 'native-retail-startup'in x)
    for x in shlex.split(include_line.split(' = ',1)[1]):
        if x.startswith('-I')and Path(x[2:]).is_dir():includes.append(Path(x[2:]))
    production=[ROOT/'runtime/src/native/retail_renderer_cameras.cpp',ROOT/'runtime/src/native/retail_renderer_frames.cpp'];fixture=ROOT/'tests/test_retail_renderer_cameras.cpp'
    sdl=F/'b/SDL2.dll'
    inputs=[Path(__file__),elf,listing,unit,manifest,ledger,ninja,fixture,a.compiler,sdl,*production,*[Path(x)for x in libraries if not x.startswith('-')],*list((ROOT/'runtime/include/mka').glob('*.hpp')),*list((ROOT/'runtime/include/ps2').glob('*.hpp'))]
    before=[pin(q)for q in dict.fromkeys(inputs)];O.mkdir(parents=True);(O/'content').mkdir();(O/'Cpu-seeds.bin').write_bytes(cpu_seeds);shutil.copyfile(sdl,O/'SDL2.dll');need(sha(sdl)==sha(O/'SDL2.dll'),'private_SDL_dependency_changed')
    cpp=O/'native_camera_differential.cpp';cpp.write_text(HOST.replace('@FIXTURE@',fixture.as_posix()).replace('@REFERENCE@','\n'.join(functions)),encoding='utf8',newline='\n');binary=O/'native_camera_differential.exe'
    argv=[str(a.compiler),'-std=c++20','-O1','-mavx2','-ffp-contract=off','-static-libgcc','-static-libstdc++',*[ '-I'+str(q.resolve())for q in dict.fromkeys(includes)],str(cpp),*map(str,production),*libraries,'-o',str(binary)]
    env=dict(os.environ,PATH=str(a.compiler.resolve().parent)+os.pathsep+os.environ['PATH']);commands=[]
    def run(cmd,name):
        cp=subprocess.run(cmd,capture_output=True,env=env,timeout=180);log=O/(name+'.log');log.write_bytes(cp.stdout+cp.stderr);commands.append(dict(argv=cmd,exit=cp.returncode,log=pin(log)));return cp
    c=run(argv,'compile');checks=[]
    if c.returncode==0:
        for name,flag,expected in [('positive','',0),('window','--planted-window',1),('reciprocal','--planted-reciprocal',1),('frame-queue','--planted-frame-queue',1)]:
            cp=run([str(binary),str(O/'content'),str(O/'Cpu-seeds.bin')]+([flag]if flag else[]),name);checks.append(dict(name=name,exit=cp.returncode,expected=expected,stdout=cp.stdout.decode(errors='replace'),stderr=cp.stderr.decode(errors='replace')))
    changes=[p['path']for p in before+states if sha(p['path'])!=p['sha256']];passed=c.returncode==0 and len(checks)==4 and all(x['exit']==x['expected']for x in checks)and not changes
    result=dict(status='PASS_BOUNDED_EXISTING_NATIVE_CAMERA_DIFFERENTIAL'if passed else'NATIVE_CAMERA_DIFFERENTIAL_HAS_FAILURES',source_pins=before,state_pins=states,input_changes=changes,original_listing_words=joined,admitted_count=18,commands=commands,checks=checks,invocation=[sys.executable,*sys.argv],credit_delta=0,production_changes=False,limits=['Existing handwritten native implementation only; no new transformed/native function credit.','Camera logical fields are compared on the original nullable-parent path; full Cpu/FCR/store-order and attached-frame caller integration remain unproved.','Finite and edge numerical differences between existing native SSE and original EE remain explicit. Only exact representable power-of-two inputs are a pass gate.','61 saved Cpu prefixes seed constructed camera ABIs; historical entry matches are counted separately. No state is restored or modified.','Separate frame queue/flag effects compare existing typed production service to original1A7510, not a fabricated camera attachment implementation.','No window, endpoint, physical audio/input, game process or protected file write.'])
    target=O/'RESULT.json';target.write_text(json.dumps(result,indent=2)+'\n',encoding='utf8',newline='\n');print(json.dumps(dict(status=result['status'],receipt=pin(target),checks=[(x['name'],x['exit'])for x in checks])));return 0 if passed else 2

if __name__=='__main__':raise SystemExit(main())
