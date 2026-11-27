#include "mka/retail_ssf_bundle.hpp"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

using namespace mka::retail;
namespace {
std::size_t checks{},plants{};
void check(bool p,const char* msg){++checks;if(!p)throw std::runtime_error(msg);}
using Bytes=std::vector<std::byte>;
void word(Bytes& b,std::size_t p,std::uint32_t x){for(unsigned k=0;k<4;++k)b.at(p+k)=std::byte(x>>(8*k));}
std::uint32_t readword(const Bytes& b,std::size_t p){std::uint32_t x=0;for(unsigned k=0;k<4;++k)x|=std::to_integer<std::uint32_t>(b.at(p+k))<<(8*k);return x;}
void append(Bytes& a,const Bytes& b){a.insert(a.end(),b.begin(),b.end());}
Bytes chunk(std::uint32_t id,Bytes b){Bytes out(12);word(out,0,id);word(out,4,static_cast<std::uint32_t>(b.size()));word(out,8,0x1803ffff);append(out,b);return out;}
Bytes wrapped_clump(std::uint32_t first=0,std::uint32_t last=0){
    Bytes count(4);word(count,0,1);
    Bytes geometry_header(16);word(geometry_header,0,0x1003f);word(geometry_header,4,2);word(geometry_header,8,4);word(geometry_header,12,1);
    auto gl=chunk(1,count);append(gl,chunk(15,chunk(1,geometry_header)));
    Bytes body=chunk(1,Bytes(12));append(body,chunk(14,Bytes(4)));append(body,chunk(26,gl));
    Bytes result(8);word(result,0,first);word(result,4,last);append(result,chunk(16,body));return result;
}
// Independent serialization, no runtime writer. Named records and wrapper
// range are intentionally different namespaces: first type3 is ordinal1.
Bytes inner(){
    const std::vector<std::string> names{"SCENE","DIRECTORY_TEXTURE","EXTRA"};
    std::string text;std::vector<std::size_t> offsets;
    for(const auto& n:names){offsets.push_back(text.size());text+=n;text+='\0';}
    const auto payload=28+48+text.size();
    auto clump=wrapped_clump();Bytes b(payload+clump.size()+16);
    word(b,0,0x53454320);word(b,4,4);word(b,8,3);word(b,12,527);word(b,16,3);word(b,20,static_cast<std::uint32_t>(text.size()));word(b,24,static_cast<std::uint32_t>(b.size()));
    for(std::size_t i=0;i<text.size();++i)b[76+i]=std::byte(text[i]);
    const std::uint32_t types[]{4,3,2};
    const std::size_t starts[]{payload,payload+clump.size(),payload+clump.size()+8};
    for(std::size_t i=0;i<3;++i){word(b,28+i*16,types[i]);word(b,32+i*16,static_cast<std::uint32_t>(starts[i]));word(b,36+i*16,i?8:static_cast<std::uint32_t>(clump.size()));word(b,40+i*16,static_cast<std::uint32_t>(offsets[i]));}
    std::copy(clump.begin(),clump.end(),b.begin()+payload);return b;
}
Bytes outer(Bytes section=inner()){
    Bytes b(60);word(b,0,0x53454320);word(b,4,4);word(b,16,2);word(b,20,16);
    word(b,28,1);word(b,32,60);word(b,36,static_cast<std::uint32_t>(section.size()));word(b,40,0x7c801a4f);
    word(b,44,2);word(b,48,static_cast<std::uint32_t>(60+section.size()));word(b,52,4);word(b,56,0x7c801a4f);
    append(b,section);b.resize(b.size()+4);word(b,24,static_cast<std::uint32_t>(b.size()));return b;
}
auto parse(Bytes b,RetailSsfBundleFailure& f){return NativeRetailSsfBundle::parse(std::move(b),f);}
void constructed(){
    RetailSsfBundleFailure f;
    auto b=parse(outer(),f);check(bool(b),"constructed outer valid");check(b->sections().size()==2,"outer section count");
    auto s=b->open_art_section(0,f);check(bool(s),"existing named SEC parser reused");
    check(!b->open_art_section(1,f)&&f.kind==RetailSsfBundleError::unsupported_section_type,"non-art outer section named refusal");
    check(!b->open_art_section(2,f)&&f.kind==RetailSsfBundleError::invalid_section_index,"section index refusal");
    auto c=s->clump("SCENE",f);check(bool(c),"bounded direct type4 clump");
    check(c->rw_bytes().size()==wrapped_clump().size()-8,"RW root exact bounded size");
    check(c->texture_records().size()==1&&c->texture_records()[0]==1,"source range rebased from first type3");
    check(!s->resource("scene",f)&&f.kind==RetailSsfBundleError::missing_resource,"resource names exact case");
    check(!s->resource("DIRECTORY_TEXTURE/file",f),"no resource basename alias");
    check(!s->resource(3,f)&&f.kind==RetailSsfBundleError::invalid_resource_index,"resource index refusal");
    check(!s->clump(1,f)&&f.kind==RetailSsfBundleError::unsupported_resource_type,"texture not a clump");
    auto texture=s->resource(1,f);check(bool(texture)&&texture->bytes().size()==8,"word08 exact resource extent");
    b.reset();s.reset();check(c->resource().record().name=="SCENE"&&c->rw_bytes().size()>12,"clump retains section after bundle destruction");
    check(texture->record().name=="DIRECTORY_TEXTURE"&&texture->bytes().size()==8,"resource retains section independently");
    const auto bad_outer=[&](const std::function<void(Bytes&)>& change,RetailSsfBundleError expected){auto x=outer();change(x);++plants;check(!parse(std::move(x),f)&&f.kind==expected,"malformed outer refused by specific category");};
    bad_outer([](auto& x){x.resize(27);},RetailSsfBundleError::invalid_header);
    bad_outer([](auto& x){word(x,0,0);},RetailSsfBundleError::unsupported_format);
    bad_outer([](auto& x){word(x,4,5);},RetailSsfBundleError::unsupported_format);
    bad_outer([](auto& x){word(x,8,3);},RetailSsfBundleError::unsupported_format);
    bad_outer([](auto& x){word(x,24,0);},RetailSsfBundleError::invalid_header);
    bad_outer([](auto& x){word(x,16,0xffffffff);},RetailSsfBundleError::invalid_directory);
    bad_outer([](auto& x){word(x,32,28);},RetailSsfBundleError::invalid_payload_range);
    bad_outer([](auto& x){word(x,36,0xffffffff);},RetailSsfBundleError::invalid_payload_range);
    bad_outer([](auto& x){word(x,48,60);},RetailSsfBundleError::overlapping_payloads);
    bad_outer([](auto& x){x.push_back(std::byte{});},RetailSsfBundleError::invalid_header);
    const auto bad_inner=[&](const std::function<void(Bytes&)>& change,RetailSsfBundleError expected,bool clump=false){auto x=inner();change(x);auto z=parse(outer(std::move(x)),f);check(bool(z),"malformed resource outer remains valid");auto a=z->open_art_section(0,f);++plants;if(!a){check(f.kind==expected,"bad inner directory category");return;}if(clump)check(!a->clump(0,f)&&f.kind==expected,"malformed clump refused by specific category");else check(!a->resource(0,f)&&f.kind==expected,"malformed resource refused by specific category");};
    bad_inner([](auto& x){word(x,0,0);},RetailSsfBundleError::unsupported_section_format);
    bad_inner([](auto& x){word(x,4,5);},RetailSsfBundleError::unsupported_section_format);
    bad_inner([](auto& x){word(x,16,0xffffffff);},RetailSsfBundleError::art_section);
    bad_inner([](auto& x){word(x,40,0xffffffff);},RetailSsfBundleError::art_section);
    bad_inner([](auto& x){word(x,28,0x80000004);},RetailSsfBundleError::unsupported_resource_type);
    bad_inner([](auto& x){word(x,32,0);},RetailSsfBundleError::missing_payload);
    bad_inner([](auto& x){word(x,36,0xffffffff);},RetailSsfBundleError::invalid_resource_length);
    bad_inner([](auto& x){word(x,36,0);},RetailSsfBundleError::invalid_resource_length);
    bad_inner([](auto& x){word(x,48,readword(x,32));},RetailSsfBundleError::invalid_resource_length);
    const auto at=inner().size()-16-wrapped_clump().size();
    bad_inner([&](auto& x){word(x,at+8,3);},RetailSsfBundleError::unsupported_clump,true);
    bad_inner([&](auto& x){word(x,at+12,0xffffffff);},RetailSsfBundleError::malformed_clump,true);
    bad_inner([&](auto& x){word(x,at+16,0);},RetailSsfBundleError::unsupported_clump,true);
    bad_inner([&](auto& x){word(x,at+24,0xffffffff);},RetailSsfBundleError::malformed_clump,true);
    bad_inner([&](auto& x){word(x,at,0xfffffffe);},RetailSsfBundleError::invalid_texture_range,true);
    bad_inner([&](auto& x){word(x,at+4,0x7fffffff);},RetailSsfBundleError::invalid_texture_range,true);
    for(const auto all:{false,true}){auto x=inner();word(x,at,all?0xffffffff:1);word(x,at+4,0);auto z=parse(outer(std::move(x)),f);auto a=z->open_art_section(0,f);auto v=a->clump(0,f);check(bool(v),"source all or empty texture range admitted");check(v->texture_records().size()==(all?2:0),"all includes types2/3; reversed range remains empty");}
    auto duplicate=inner();word(duplicate,56,0);auto z=parse(outer(std::move(duplicate)),f);auto a=z->open_art_section(0,f);check(!a->resource("SCENE",f)&&f.kind==RetailSsfBundleError::ambiguous_resource,"duplicate exact name refused");
    std::printf("constructed: %zu malformed plants\n",plants);
}
void actual(const char* root){
    mka::content::ContentVolumeFailure cf;auto volume=mka::content::NativeContentVolume::open(std::filesystem::path(root),cf);check(bool(volume),"actual explicit content volume");
    auto file=volume->open_resource("mkda.pak",cf);check(bool(file),"actual retained archive file");RetailArchiveFailure af;auto archive=NativeRetailArchive::open(std::move(*file),af);check(bool(archive),"actual PAK TOC");check(archive->member_count()==245,"actual245 PAK members");
    std::size_t sections=0,type1=0,plain=0,unknown=0,type4=0,clumps=0,refused=0;RetailSsfBundleFailure f;
    for(std::size_t mi=0;mi<archive->member_count();++mi){auto b=NativeRetailSsfBundle::open(*archive,mi,f);if(!b)std::fprintf(stderr,"member%zu failure%u %s\n",mi,unsigned(f.kind),f.operation);check(bool(b),"actual outer SSF validates");sections+=b->sections().size();for(const auto& sr:b->sections())if(sr.raw_type==1){++type1;auto s=b->open_art_section(sr.index,f);if(!s){check(f.kind==RetailSsfBundleError::unsupported_section_format,"actual unknown section named format refusal");++unknown;continue;}++plain;for(std::size_t ri=0;ri<s->records().size();++ri)if(s->records()[ri].raw_type==4){++type4;auto c=s->clump(ri,f);if(c){++clumps;}else{++refused;check(mi==82&&(f.kind==RetailSsfBundleError::malformed_clump||f.kind==RetailSsfBundleError::unsupported_clump),"only previously bounded innerspire2 refusals");}}}}
    check(sections==4038&&type1==2012&&plain==1992&&unknown==20,"actual outer/inner separate denominators");
    check(type4==3779&&clumps==3774&&refused==5,"actual source-directed clump census");
    auto sky=NativeRetailSsfBundle::open(*archive,219,f);auto s=sky->open_art_section(0,f);auto c=s->clump("SKYTEMPLE_BANNERS",f);check(bool(c)&&c->texture_records().size()==1&&c->texture_records()[0]==65,"actual banners source texture range");auto t=s->resource(65,f);check(bool(t)&&t->bytes().size()==5888,"actual banners exact texture record");
    std::printf("actual:245 outer, %zu sections, %zu type1, %zu plain SEC4, %zu unsupported, %zu clumps, %zu refused\n",sections,type1,plain,unknown,clumps,refused);
}
}
int main(int argc,char** argv){try{check(argc==1||argc==2,"usage: test_retail_ssf_bundle [actual-content-root]");constructed();if(argc==2)actual(argv[1]);std::printf("PASS retail SSF bundle: %zu checks\n",checks);return 0;}catch(const std::exception& e){std::fprintf(stderr,"FAIL after%zu: %s\n",checks,e.what());return 1;}}
