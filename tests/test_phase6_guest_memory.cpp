#include "mka/stage_a_guest_memory.hpp"
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>
using namespace mka::stage_a;
static void need(bool ok,const char*why){if(!ok)throw std::runtime_error(why);}
static void observe(void*p,const Access&a){static_cast<std::vector<Access>*>(p)->push_back(a);}
template<class T>static void edges(GuestMemory&m,std::vector<std::byte>&ram,std::vector<Access>&events,unsigned&checks){
 using U=std::make_unsigned_t<T>;
 for(const U bits:std::array<U,4>{U(0),U(1),U(U(1)<<(8*sizeof(T)-1)),U(~U(0))}){
  const auto value=std::bit_cast<T>(bits);m.store<T>(0x500,value);checks+=3;
  need(std::bit_cast<U>(m.load<T>(0x500))==bits,"typed sign/zero/high-bit roundtrip");
  need(events.back().width==sizeof(T)&&events.back().value==bits,"typed width observation");
  for(unsigned b=0;b<sizeof(T);++b)need(ram[0x500+b]==std::byte((bits>>(8*b))&255u),"typed independent endian bytes");
 }
}
int main(int argc,char**argv){try{
 const std::string mode=argc>1?argv[1]:"";std::vector<std::byte>ram(0x02000000u,std::byte{0x5a}),spr(0x4000u,std::byte{0x95});std::vector<Access>events;
 GuestMemory memory(ram,spr,{&events,&observe});unsigned checks=0;
 edges<std::int8_t>(memory,ram,events,checks);edges<std::uint8_t>(memory,ram,events,checks);
 edges<std::int16_t>(memory,ram,events,checks);edges<std::uint16_t>(memory,ram,events,checks);
 edges<std::int32_t>(memory,ram,events,checks);edges<std::uint32_t>(memory,ram,events,checks);
 edges<std::int64_t>(memory,ram,events,checks);edges<std::uint64_t>(memory,ram,events,checks);
 auto check=[&](bool ok,const char*why){++checks;need(ok,why);};
 memory.store<std::uint64_t>(0x100,0x80000001020304ffull);
 check(ram[0x100]==std::byte{0xff}&&ram[0x107]==std::byte{0x80},"little endian64");
 check(memory.load<std::int8_t>(0x100)==-1,"signed8");check(memory.load<std::uint8_t>(0x100)==255,"unsigned8");
 check(memory.load<std::int16_t>(0x106)<0,"signed16");check(memory.load<std::int32_t>(0x104)==std::bit_cast<std::int32_t>(0x80000001u),"signed32");
 check(memory.load<std::int64_t>(0x100)==std::bit_cast<std::int64_t>(0x80000001020304ffull),"signed64");
 for(auto base:{0u,0x20000000u,0x30000000u,0x80000000u,0xa0000000u}){
  check(memory.load<std::uint64_t>(base+0x100)==0x80000001020304ffull,"RAM mirror load");
  memory.store<std::uint32_t>(base+0x200,0x87654321u);check(memory.load<std::uint32_t>(0x200)==0x87654321u,"RAM mirror write");
 }
 memory.store<std::int16_t>(0x70003ffeu,-123);check(memory.load<std::int16_t>(0x70003ffeu)==-123,"scratchpad end");
 check(ram[0x3ffe]==std::byte{0x5a},"scratchpad is distinct");
 memory.store<std::uint32_t>(0x301,0xaabbccddu,Alignment::bytewise);
 check(memory.load<std::uint32_t>(0x301,Alignment::bytewise)==0xaabbccddu,"explicit unaligned bytewise");
 for(auto address:{0x10000000u,0x12000000u,0x1f801000u,0x1fc00000u,0xb0000000u,0x02000000u,0xffffffffu}){
  bool refused=false;try{memory.load<std::uint8_t>(address);}catch(const MemoryRefusal&){refused=true;}check(refused,"device/ROM/uninstalled/overflow must refuse");
 }
 for(auto address:{0x101u,0x01ffffffu,0x21ffffffu,0x70003fffu}){
  const auto r=ram,s=spr;const auto n=events.size();bool refused=false;
  try{memory.store<std::uint32_t>(address,0xdeadbeefu,address==0x101u?Alignment::natural:Alignment::bytewise);}catch(const MemoryRefusal&){refused=true;}
  check(refused&&ram==r&&spr==s&&events.size()==n,"failed access must not partly write/observe");
 }
 events.clear();memory.store<std::uint32_t>(0x900,0x11223344u);
 if(mode=="--plant-width")memory.store<std::uint32_t>(0x902,0x7788u,Alignment::bytewise);else memory.store<std::uint16_t>(0x902,0x7788u);
 check(ram[0x904]==std::byte{0x5a},"width plant changed adjacent bytes");
 events.clear();
 if(mode=="--plant-order"){memory.store<std::uint32_t>(0x20001004u,2u);memory.store<std::uint32_t>(0x1000,1u);}
 else {memory.store<std::uint32_t>(0x1000,1u);memory.store<std::uint32_t>(0x20001004u,2u);}
 check(events.size()==2&&events[0].address==0x1000&&events[1].address==0x20001004u&&events[1].offset==0x1004&&events[0].sequence+1==events[1].sequence,"ordered alias-aware stores");
 if(!mode.empty()){std::fprintf(stderr,"TEST_MUTANT_ACCEPTED\n");return 2;}
 std::printf("PASS typed_guest_memory checks=%u widths=8/16/32/64 mirrors=5 SPR=1 MMIO_refused=1 no_devices=1\n",checks);return 0;
}catch(const std::exception&e){std::fprintf(stderr,"REFUSED %s\n",e.what());return 1;}}
