#include "mka/retail_frame_work_storage.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <new>
#include <stdexcept>
#include <thread>
#include <vector>
using namespace mka::retail;
extern "C" int SDL_InitSubSystem(unsigned){std::abort();}
namespace {thread_local int allocation_cut=-1;}
void* operator new(std::size_t n){
    if(allocation_cut>=0&&allocation_cut--==0)throw std::bad_alloc();
    if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();
}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
namespace {
void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
struct Game {
    mka::input::ControllerInput input;
    std::unique_ptr<NativeRetailStartupState> startup;
    std::unique_ptr<NativeRetailMainState> main;
    explicit Game(const std::filesystem::path& root){
        mka::content::ContentVolumeFailure cf;auto volume=mka::content::NativeContentVolume::open(root,cf);
        check(bool(volume),"actual native content owner");
        startup=std::make_unique<NativeRetailStartupState>(std::move(*volume),std::vector<std::string>{});
        StartupFailure f;check(initialize_native_retail_game_session(*startup,f),"actual persistent Session");
        main=std::make_unique<NativeRetailMainState>(*startup,input);
        RetailMemoryOwnerFailure mf;startup->memory_owners=NativeRetailMemoryOwners::create(mf);
        check(bool(startup->memory_owners),"actual ordered memory owners");
    }
};
}
int main(int argc,char** argv)try{
    check(argc==2,"existing empty content root");
    Game game(argv[1]);StartupFailure sf;RetailFrameWorkFailure f;RetailMemoryOwnerFailure mf;
    const auto before=game.startup->memory_owners->observe(RetailMemoryRole::permanent,mf);
    check(before.has_value(),"actual Permanent owner");
    check(main_binding::initialize_retail_frame_work_buffers(*game.main,sf),"production2E9930 binding");
    auto& storage=*game.main->frame_work_storage;
    const auto born=storage.observe(f);
    check(born&&born->heap==before->heap.identity,"same existing Permanent heap, not a new heap");
    check(born->categories[0].native_blocks==3&&born->categories[1].native_blocks==2&&
        !born->categories[0].pending&&!born->categories[1].pending,"source warm initialization and empty logical state");
    const auto allocated=game.startup->memory_owners->observe(RetailMemoryRole::permanent,mf);
    check(allocated&&allocated->heap.live_allocations==before->heap.live_allocations+5,"five actual initial native backings");
    auto request=storage.reserve(0,17,f);check(request.has_value(),"real reservation");
    auto bytes=request->writable_bytes(f);check(bytes&&bytes->size()==17,"typed writable source-sized span");
    check(reinterpret_cast<std::uintptr_t>(bytes->data())%16==0,"native source-format alignment");
    for(const auto b:*bytes)check(b==std::byte{0},"actual loader-zero initial backing");
    for(unsigned i=0;i<17;++i)(*bytes)[i]=std::byte(i+1);
    check(!storage.reserve(0,1,f)&&f.kind==RetailFrameWorkError::pending,"one pending per category");
    auto other_category=storage.reserve(1,1,f);check(other_category.has_value(),"other category independent");
    auto committed=storage.commit(*request,17,f);
    check(committed&&committed->size()==17&&committed->aligned_size()==32,"source sixteen-byte commit rounding");
    check(!request->writable_bytes(f)&&f.kind==RetailFrameWorkError::stale_reservation,"commit revokes mutable reservation");
    check(!storage.commit(*request,1,f)&&f.kind==RetailFrameWorkError::stale_reservation,"double commit refused");
    auto stable=committed->bytes(f);check(stable&&(*stable)[16]==std::byte{17},"actual committed bytes");
    auto bad=storage.reserve(0,8,f);check(bad.has_value(),"second reservation");
    check(!storage.commit(*bad,9,f)&&f.kind==RetailFrameWorkError::invalid_commit,"overcommit refused");
    check(!storage.observe(f)->categories[0].pending,"source consumes pending before invalid count");
    auto zero=storage.reserve(0,0,f);check(zero.has_value(),"source zero-length reservation");
    auto empty=storage.commit(*zero,0,f);
    check(empty&&empty->bytes(f).has_value()&&empty->bytes(f)->empty(),"zero commit is an owned empty value, not missing storage");
    check(!storage.reserve(2,1,f)&&f.kind==RetailFrameWorkError::invalid_category,"invalid logical category");
    check(!storage.reserve(0,std::size_t(-1),f)&&f.kind==RetailFrameWorkError::host_capacity,"host alignment overflow named");
    std::vector<NativeRetailFrameWorkData> retained;
    for(unsigned i=0;i<8;++i){
        auto r=storage.reserve(0,32,f);check(r.has_value(),"native growth beyond source three-window count");
        auto w=r->writable_bytes(f);check(w.has_value(),"real retained work backing");
        (*w)[0]=std::byte(40+i);
        auto data=storage.commit(*r,32,f);check(data.has_value(),"native retained work commit");
        retained.push_back(std::move(*data));
    }
    check(storage.observe(f)->categories[0].native_blocks>3,"no PS2 window-count ceiling");
    auto large=storage.reserve(1,0x249f0+4096,f);
    check(!large&&f.kind==RetailFrameWorkError::pending,"independent pending state survives other category growth");
    other_category.reset();
    large=storage.reserve(1,0x249f0+4096,f);
    check(large&&large->writable_bytes(f)->size()==0x249f0+4096,"no source per-window byte ceiling");
    auto large_data=storage.commit(*large,0x249f0+4096,f);check(large_data.has_value(),"actual larger host backing");
    bool wrong=false;
    std::thread thread([&]{RetailFrameWorkFailure other;wrong=!storage.reserve(0,4,other)&&other.kind==RetailFrameWorkError::wrong_thread;});
    thread.join();check(wrong,"wrong thread cannot mutate storage");
    for(unsigned i=0;i<retained.size();++i)check(retained[i].bytes(f)->front()==std::byte(40+i),"growth does not overwrite retained consumer data");
    auto pending=storage.reserve(0,16,f);check(pending.has_value(),"pending before repeated initialization");
    auto* identity=game.main->frame_work_storage.get();
    check(main_binding::initialize_retail_frame_work_buffers(*game.main,sf)&&game.main->frame_work_storage.get()==identity,"repeat initializes same native owner");
    check(!pending->writable_bytes(f),"repeat invalidates old pending request");
    check(committed->bytes(f)->front()==std::byte{1},"repeat preserves committed native consumer lease");
    Game foreign(argv[1]);check(main_binding::initialize_retail_frame_work_buffers(*foreign.main,sf),"independent native owner");
    auto own=storage.reserve(0,4,f);check(own.has_value(),"owned reservation");
    check(!foreign.main->frame_work_storage->commit(*own,4,f)&&f.kind==RetailFrameWorkError::foreign_reservation,"foreign commit does not steal reservation");
    check(storage.commit(*own,4,f).has_value(),"original owner still commits");
    auto escaped=storage.reserve(0,64,f);check(escaped.has_value(),"escaped mutable reservation");
    game.main.reset();
    check(!escaped->writable_bytes(f)&&f.kind==RetailFrameWorkError::retired_main,"mutable work cannot use destroyed Main");
    check(committed->bytes(f)->back()==std::byte{17},"committed actual backing survives Main until consumer retires");
    unsigned cuts=0;
    for(int cut=0;cut<128;++cut){
        Game g(argv[1]);allocation_cut=cut;
        const auto result=main_binding::initialize_retail_frame_work_buffers(*g.main,sf);allocation_cut=-1;
        if(result)break;
        check(!g.main->frame_work_storage,"failed initialization publishes no successful owner");++cuts;
    }
    check(cuts>=10&&cuts<128,"native initial backing/metadata allocation cuts");
    std::printf("PASS production2E9930 actual Permanent backing, native growth/alignment/commit/leases/reset/thread/lifetime and%u allocation cuts; no window/GPU/audio/game acceptance\n",cuts);
    return 0;
}catch(const std::exception& e){allocation_cut=-1;std::fprintf(stderr,"FAIL frame work: %s\n",e.what());return 1;}
