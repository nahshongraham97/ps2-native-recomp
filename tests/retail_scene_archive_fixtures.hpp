#pragma once
#include <bit>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Constructed serialized fixtures only. No original asset/equivalence claim.
namespace mka::test::scene_archive {
using Bytes=std::vector<std::byte>;
inline void u16(Bytes& b,std::uint16_t x){for(unsigned i=0;i<2;++i)b.push_back(std::byte(x>>(8*i)));}
inline void u32(Bytes& b,std::uint32_t x){for(unsigned i=0;i<4;++i)b.push_back(std::byte(x>>(8*i)));}
inline void f32(Bytes& b,float x){u32(b,std::bit_cast<std::uint32_t>(x));}
inline void append(Bytes& a,const Bytes& b){a.insert(a.end(),b.begin(),b.end());}
inline Bytes chunk(std::uint32_t id,const Bytes& body){Bytes b;u32(b,id);u32(b,static_cast<std::uint32_t>(body.size()));u32(b,0x1803ffff);append(b,body);return b;}
inline Bytes text(std::string_view s){Bytes b;for(unsigned char c:s)b.push_back(std::byte(c));b.push_back(std::byte{});return b;}
inline Bytes txd(std::string_view name="archive_gold"){
 Bytes count;u16(count,1);u16(count,0);auto body=chunk(1,count);
 Bytes native(88+16,std::byte{});
 const auto set=[&](std::size_t i,std::uint32_t x){for(unsigned n=0;n<4;++n)native[i+n]=std::byte(x>>(8*n));};
 set(4,0x3301);for(std::size_t i=0;i<name.size()&&i<31;++i)native[8+i]=std::byte(name[i]);
 native[80]=std::byte{2};native[82]=std::byte{2};native[84]=std::byte{32};
 for(std::size_t i=88;i<native.size();i+=4){native[i]=std::byte{255};native[i+1]=std::byte{220};native[i+3]=std::byte{255};}
 append(body,chunk(0x15,chunk(1,native)));return chunk(0x16,body);
}
inline Bytes matlist(std::string_view texture="archive_gold"){
 Bytes material;u32(material,0);u32(material,0xffffffff);u32(material,0);u32(material,texture.empty()?0:1);
 f32(material,1);f32(material,0);f32(material,1);auto body=chunk(1,material);
 if(!texture.empty()){Bytes filter;u32(filter,0x3301);auto t=chunk(1,filter);append(t,chunk(2,text(texture)));append(body,chunk(6,t));}
 Bytes indices;u32(indices,1);u32(indices,0xffffffff);auto list=chunk(1,indices);append(list,chunk(7,body));return chunk(8,list);
}
inline Bytes world(std::string_view texture="archive_gold"){
 Bytes ws;u32(ws,1);for(float x:{-2.f,-2.f,-1.f,2.f,2.f,1.f})f32(ws,x);
 for(auto x:{1u,3u,0u,1u,0u,0u})u32(ws,x);
 Bytes leaf;u32(leaf,0);u32(leaf,1);u32(leaf,3);
 for(float x:{-2.f,-2.f,-1.f,2.f,2.f,1.f})f32(leaf,x);u32(leaf,0);u32(leaf,0);
 for(float x:{-1.f,-1.f,0.f,1.f,-1.f,0.f,0.f,1.f,0.f})f32(leaf,x);
 for(auto x:{0u,0u,1u,2u})u16(leaf,static_cast<std::uint16_t>(x));
 auto body=chunk(1,ws);append(body,matlist(texture));append(body,chunk(9,chunk(1,leaf)));return chunk(0xb,body);
}
inline Bytes geometry(bool invalid_material=false,std::string_view texture="archive_gold"){
 Bytes st;for(auto x:{0u,1u,3u,1u})u32(st,x);
 u16(st,1);u16(st,0);u16(st,invalid_material?1:0);u16(st,2);
 for(float x:{0.f,0.f,0.f,2.f})f32(st,x);u32(st,1);u32(st,0);
 for(float x:{-1.f,-1.f,0.f,1.f,-1.f,0.f,0.f,1.f,0.f})f32(st,x);
 auto body=chunk(1,st);append(body,matlist(texture));return chunk(0xf,body);
}
inline Bytes pak(const std::vector<std::pair<std::string,Bytes>>& members){
 Bytes b(20,std::byte{}),toc,names;
 for(const auto& [name,data]:members){u32(toc,static_cast<std::uint32_t>(names.size()));u32(toc,static_cast<std::uint32_t>(b.size()));u32(toc,static_cast<std::uint32_t>(data.size()));append(names,text(name));append(b,data);}
 const auto table=static_cast<std::uint32_t>(b.size());append(b,toc);append(b,names);
 const std::uint32_t header[]{0x50414b20,0x100,static_cast<std::uint32_t>(members.size()),table,static_cast<std::uint32_t>(names.size())};
 for(unsigned i=0;i<5;++i)for(unsigned k=0;k<4;++k)b[i*4+k]=std::byte(header[i]>>(8*k));return b;
}
} // namespace mka::test::scene_archive
