#pragma once
#include "ps2/cpu.hpp"
#include "native_test_sha256.hpp"
#include <filesystem>
#include <fstream>
#include <span>
#include <cstring>
namespace track_b {
struct Store {std::uint32_t address,value;bool operator==(const Store&)const=default;};
struct Capture {
    std::uint32_t pc{},entry{};std::uint64_t frame{};
    ps2::Cpu before{},after{};
    std::vector<std::byte> ram,exit_ram,scratch,exit_scratch;
    std::vector<Store> stores;
    bool returned()const{return after.pc==before.r[31].w[0] && after.r[29].w[0]==before.r[29].w[0];}
};
inline std::uint32_t read(std::span<const std::byte> b,std::size_t at) {
    if(at>b.size()||4>b.size()-at)throw std::runtime_error("capture read bounds");
    std::uint32_t x;std::memcpy(&x,b.data()+at,4);return x;
}
inline Capture load(const std::filesystem::path& p) {
    std::ifstream f(p,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("capture open");
    const auto size=f.tellg();if(size<72)throw std::runtime_error("capture truncated");
    std::vector<std::byte>b(static_cast<std::size_t>(size));f.seekg(0);f.read(reinterpret_cast<char*>(b.data()),size);
    if(!f)throw std::runtime_error("capture read");
    NativeTestSha256 hash;hash.add(b.data(),b.size()-32);auto digest=hash.finish();
    constexpr char h[]="0123456789abcdef";std::string trailer;
    for(std::size_t n=b.size()-32;n<b.size();++n){auto c=std::to_integer<unsigned>(b[n]);trailer+=h[c>>4];trailer+=h[c&15];}
    if(digest!=trailer)throw std::runtime_error("capture sha256 mismatch");
    if(std::memcmp(b.data(),"PS2CAP01",8)||read(b,24)!=sizeof(ps2::Cpu)||read(b,28)!=(32u<<20)||read(b,32)!=16384)throw std::runtime_error("capture ABI");
    Capture c;c.pc=read(b,8);c.entry=read(b,12);std::memcpy(&c.frame,b.data()+16,8);std::size_t pos=36;
    auto copy=[&](auto* out,std::size_t count){if(pos>b.size()||count>b.size()-pos)throw std::runtime_error("capture section bounds");std::memcpy(out,b.data()+pos,count);pos+=count;};
    c.ram.resize(32u<<20);c.exit_ram.resize(c.ram.size());c.scratch.resize(16384);c.exit_scratch.resize(16384);
    copy(&c.before,sizeof(c.before));copy(c.ram.data(),c.ram.size());copy(c.scratch.data(),c.scratch.size());
    copy(&c.after,sizeof(c.after));copy(c.exit_ram.data(),c.exit_ram.size());copy(c.exit_scratch.data(),c.exit_scratch.size());
    const auto count=read(b,pos);pos+=4;if(std::uint64_t(pos)+std::uint64_t(count)*8+32!=b.size())throw std::runtime_error("capture store bounds");
    c.stores.resize(count);copy(c.stores.data(),count*8);return c;
}
inline std::vector<std::filesystem::path> files(const std::filesystem::path& root,const std::string& prefix) {
    std::vector<std::filesystem::path> out;
    for(auto& e:std::filesystem::recursive_directory_iterator(root))if(e.is_regular_file()&&e.path().filename().string().starts_with(prefix)&&e.path().extension()==".bin")out.push_back(e.path());
    std::sort(out.begin(),out.end());return out;
}
}
