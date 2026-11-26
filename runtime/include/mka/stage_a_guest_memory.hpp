#pragma once
// Phase6 Stage A only. Explicit storage supplied by the differential harness;
// no fixed host address, device forwarding, process globals or StageB claim.
#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <type_traits>

namespace mka::stage_a {
enum class Space { ram, scratchpad };
enum class Alignment { natural, bytewise };
struct Access { Space space;std::uint32_t address,offset;unsigned width;std::uint64_t value,sequence; };
struct Observer {void*user{};void(*write)(void*,const Access&){};};
class MemoryRefusal:public std::runtime_error {public:using std::runtime_error::runtime_error;};
class GuestMemory {
    std::span<std::byte>ram_,scratch_;Observer observer_;std::uint64_t sequence_{};
    struct Location {std::span<std::byte>bytes;Space space;std::uint32_t offset;};
    Location locate(std::uint32_t address,unsigned width,Alignment alignment)const {
        if(width!=1&&width!=2&&width!=4&&width!=8)throw MemoryRefusal("stageA memory: unproved access width");
        if(alignment==Alignment::natural&&address%width)throw MemoryRefusal("stageA memory: alignment");
        // SDK UNCACHED_SEG/UCAB_SEG, source-proved kernel mirrors and SPR.
        // No arbitrary TLB mapping, ROM, uninstalled memory or device aliases.
        std::uint32_t offset=address;Space space=Space::ram;
        if(address>=0x70000000u&&address<0x70004000u){offset=address-0x70000000u;space=Space::scratchpad;}
        else {
            bool mapped=false;
            for(const std::uint32_t base:std::array<std::uint32_t,5>{0u,0x20000000u,0x30000000u,0x80000000u,0xa0000000u})
                if(address>=base&&std::uint64_t(address)-base<0x02000000u){offset=address-base;mapped=true;break;}
            if(!mapped)throw MemoryRefusal("stageA memory: device/MMIO/unmapped address refused");
        }
        const auto bytes=space==Space::ram?ram_:scratch_;
        if(offset>bytes.size()||width>bytes.size()-offset)throw MemoryRefusal("stageA memory: cross-region access");
        return {bytes,space,offset};
    }
public:
    GuestMemory(std::span<std::byte>ram,std::span<std::byte>scratch,Observer observer={}):ram_(ram),scratch_(scratch),observer_(observer){
        if(ram.size()!=0x02000000u||scratch.size()!=0x4000u)throw MemoryRefusal("stageA memory: wrong supplied storage extents");
        const auto a=reinterpret_cast<std::uintptr_t>(ram.data()),b=reinterpret_cast<std::uintptr_t>(scratch.data());
        if(a<b+scratch.size()&&b<a+ram.size())throw MemoryRefusal("stageA memory: overlapping region storage");
    }
    template<std::integral T> requires (!std::same_as<T,bool> && sizeof(T)<=8)
    T load(std::uint32_t address,Alignment alignment=Alignment::natural)const {
        const auto at=locate(address,sizeof(T),alignment);using U=std::make_unsigned_t<T>;U value{};
        for(unsigned i=0;i<sizeof(T);++i)value|=U(std::to_integer<unsigned>(at.bytes[at.offset+i]))<<(8*i);
        return std::bit_cast<T>(value);
    }
    template<std::integral T> requires (!std::same_as<T,bool> && sizeof(T)<=8)
    void store(std::uint32_t address,T value,Alignment alignment=Alignment::natural){
        const auto at=locate(address,sizeof(T),alignment);using U=std::make_unsigned_t<T>;const U bits=std::bit_cast<U>(value);
        if(sequence_==UINT64_MAX)throw MemoryRefusal("stageA memory: ordered write sequence exhausted");
        // Complete bounds/alignment checking precedes observer or mutation.
        // A throwing observer refuses BEFORE the write. No callback may mutate
        // supplied storage; product ownership/transactions are a separate gate.
        const Access event{at.space,address,at.offset,sizeof(T),std::uint64_t(bits),sequence_};
        if(observer_.write)observer_.write(observer_.user,event);
        for(unsigned i=0;i<sizeof(T);++i)at.bytes[at.offset+i]=std::byte((bits>>(8*i))&255u);
        ++sequence_;
    }
};
}
