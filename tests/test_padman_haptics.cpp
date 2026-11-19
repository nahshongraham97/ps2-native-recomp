#include "iop/padman.hpp"
#include "mka/haptics.hpp"
#include "ps2/memory.hpp"
#include "ps2/state.hpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"padman haptics check %d: %s\n",__LINE__,#x);return 1;}}while(0)
namespace {
struct Bytes final:ps2::state::Stream{
    std::vector<std::uint8_t> data;std::size_t at=0;
    Bytes(bool save):Stream(save){set_version(14);}
    void bytes(void* p,std::size_t n)override{
        if(saving()){const auto* b=static_cast<std::uint8_t*>(p);data.insert(data.end(),b,b+n);}
        else {if(n>data.size()-at)throw std::runtime_error("truncated fixture state");std::memcpy(p,data.data()+at,n);at+=n;}
    }
};
}
int main(){
    using namespace mka::input;
    constexpr std::uint32_t args=0x1000,reply=0x3000,area=0x2000;
    const ActuatorBytes unused{{255,255,255,255,255,255}},normal{{0,1,255,255,255,255}},reverse{{1,0,255,255,255,255}};
    const auto rpc=[&](unsigned command,unsigned size){ps2::st32(args,command);return ps2::iop::padman::handle_rpc(1,args,size,reply,24);};
    ps2::iop::padman::activate();CHECK(rpc(0x10,4));
    ps2::st32(args+4,0);ps2::st32(args+8,0);ps2::st32(args+16,area);CHECK(rpc(1,20));
    CHECK(haptics().sample(0)&&haptics().sample(0)->alignment==unused);
    for(unsigned i=0;i<6;++i)CHECK(ps2::ld8(area+40+i)==255&&ps2::ld8(area+128+40+i)==255);
    CHECK(ps2::iop::padman::restore_haptic_alignment()); // unstarted exact reset record
    ps2::iop::padman::frame_tick();
    for(unsigned i=0;i<6;++i)ps2::st8(args+12+i,normal[i]);
    CHECK(rpc(8,18)&&ps2::ld32(reply+20)==1);
    CHECK(haptics().submit(0,{{1,73,0,0,0,0}})==HapticError::none);
    const auto running=haptics().sample(0);CHECK(running&&running->request.large==73);
    Bytes save(true);ps2::iop::padman::state(save);
    CHECK(save.data.size()==106); // unchanged tag+2bool+8 existing12-byte OpenPad records
    std::array<std::uint8_t,256> saved_area{};
    for(unsigned i=0;i<saved_area.size();++i)saved_area[i]=ps2::ld8(area+i);
    CHECK(haptics().set_alignment(0,reverse)==HapticError::none);
    CHECK(haptics().submit(0,{{129,1,0,0,0,0}})==HapticError::none);
    Bytes load(false);load.data=save.data;ps2::iop::padman::state(load);
    const auto restored=haptics().sample(0);
    CHECK(restored&&restored->alignment==normal&&!restored->request.small&&!restored->request.large&&restored->sequence>running->sequence);
    for(unsigned i=0;i<saved_area.size();++i)CHECK(ps2::ld8(area+i)==saved_area[i]);
    // Restore picks the newer record, not a fixed double-buffer side.
    for(unsigned i=0;i<128;++i)ps2::st8(area+i,saved_area[128+i]);
    ps2::st32(area+88,100);
    for(unsigned i=0;i<6;++i)ps2::st8(area+40+i,reverse[i]);
    ps2::st8(area+114,1);
    CHECK(ps2::iop::padman::restore_haptic_alignment());CHECK(haptics().sample(0)->alignment==reverse);
    ps2::st8(area+40,0);ps2::st8(area+41,0);
    CHECK(!ps2::iop::padman::restore_haptic_alignment());
    CHECK(!haptics().sample(0)->request.small&&!haptics().sample(0)->request.large);
    for(unsigned i=0;i<6;++i)ps2::st8(area+40+i,reverse[i]);
    CHECK(ps2::iop::padman::restore_haptic_alignment());
    CHECK(haptics().submit(0,{{129,1,0,0,0,0}})==HapticError::none);
    ps2::st32(args+4,0);ps2::st32(args+8,0);ps2::st32(args+16,1);CHECK(rpc(0x0e,20));
    CHECK(haptics().sample(0)->alignment==unused&&!haptics().sample(0)->request.large&&!haptics().sample(0)->request.small);
    ps2::iop::padman::reset();CHECK(!haptics().sample(0)->request.large);
    std::puts("native PADMAN alignment, unchanged state bytes, EE-record restore, close/reset stop PASS");
}
