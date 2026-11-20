#include "mka/geometry_attributes.hpp"
#include "mka/material_maintenance.hpp"
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <thread>
using namespace mka::game;
namespace {
int fail_after=-1;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
}
void* operator new(std::size_t n){
    if(fail_after>=0){if(fail_after--==0){fail_after=-1;throw std::bad_alloc();}}
    if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();
}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
namespace {
MaterialVec3 words(std::uint32_t x,std::uint32_t y,std::uint32_t z){return {{x},{y},{z}};}
GeometryAttributeData basic(std::size_t count=3){
    GeometryAttributeData data{count,{},std::nullopt,std::nullopt,std::nullopt,GeometryExtraChannels::none,{},std::nullopt};
    GeometryStreamAttributes first;first.primary.resize(count);
    for(std::size_t i=0;i<count;++i)first.primary[i]=words(0x10000000u+static_cast<std::uint32_t>(i),0xdeadbeef,0x80000000);
    data.streams.push_back(std::move(first));return data;
}
std::shared_ptr<const NativeGeometryAttributes> own(GeometryAttributeData data){
    GeometryAttributeFailure f;auto x=NativeGeometryAttributes::create(std::move(data),f);check(bool(x),"owned geometry source");return x;
}
void defaults_and_literal_recipes(){
    auto source=own(basic());GeometryAttributeFailure f;
    auto a=source->vertex(2,{7,8,9,10},f),b=source->vertex(2,{17,18,19,20},f);
    check(a&&b&&a->source_index==2&&a->primary[0]==words(0x10000002,0xdeadbeef,0x80000000),"ordered indexed primary words unchanged");
    check(a->companion[0]==words(0,0,0)&&a->first_pair==GeometryPair{}&&a->second_pair==GeometryPair{}&&a->quadruple==GeometryQuad{}&&a->components_not_below_threshold==0,"only proved missing attributes are zeroed");
    check(a->packed==GeometryPacked{7,8,9,10}&&b->packed==GeometryPacked{17,18,19,20}&&a->extra.empty(),"fallback observed on EACH vertex, no asset-time default or guessed white");
    auto emitted=emit_geometry_vertex_attributes(*a,GeometryAttributeProgram::with_quad,f);
    check(emitted&&emitted->first.primary==a->primary[0]&&emitted->first.companion==std::array<std::uint8_t,3>{0,0,0}&&emitted->quadruple==std::optional<GeometryQuad>{GeometryQuad{}}&&!emitted->second,"actual absent companion/quad payload literals");
    auto data=basic();data.streams[0].companion=std::vector<MaterialVec3>(3,words(0x3f800000,0xbf800000,0x3f000000));
    GeometryStreamAttributes second;second.primary.assign(3,words(0x11111111,0x22222222,0x33333333));
    second.companion=std::vector<MaterialVec3>(3,words(0xbf000000,0x40000000,0xc0000000));data.streams.push_back(std::move(second));
    data.first_pairs=std::vector<GeometryPair>(3,GeometryPair{{{0x01020304},{0x05060708}}});
    data.second_pairs=std::vector<GeometryPair>(3,GeometryPair{{{0x91929394},{0x95969798}}});
    data.packed=std::vector<GeometryPacked>(3,GeometryPacked{1,2,3,4});
    data.quadruples=std::vector<GeometryQuad>(3,GeometryQuad{{{0x3d4ccccc},{0x3d4ccccd},{0xffffffff},{0x7fffffff}}});
    source=own(std::move(data));a=source->vertex(1,{77,88,99,111},f);
    check(a&&a->packed==GeometryPacked{1,2,3,4}&&a->components_not_below_threshold==2,"actual packed primary and literal below/equal/extended signed threshold cases");
    for(auto program:{GeometryAttributeProgram::with_quad,GeometryAttributeProgram::pair_banks,GeometryAttributeProgram::first_pair}){
        emitted=emit_geometry_vertex_attributes(*a,program,f);
        check(emitted&&emitted->first.companion==std::array<std::uint8_t,3>{127,129,63}&&
            emitted->second&&emitted->second->primary==words(0x11111111,0x22222222,0x33333333)&&
            emitted->second->companion==std::array<std::uint8_t,3>{193,254,2},"actual ordered127 quantization/low8 wrap for primary and second companions");
        check(emitted->pair_words[0].bits==0x01020304&&emitted->pair_words[1].bits==0x05060708&&
            emitted->pair_word_count==(program==GeometryAttributeProgram::pair_banks?4:2),"actual program pair count");
        if(program==GeometryAttributeProgram::pair_banks)check(emitted->pair_words[2].bits==0x91929394&&emitted->pair_words[3].bits==0x95969798,"corrected309A60 emits ACTUAL second bank, not zero padding");
        check(emitted->quadruple.has_value()==(program==GeometryAttributeProgram::with_quad),"only source31C100 emits quadruple");
    }
}
void extra_routes_and_native_extents(){
    auto data=basic(1);data.packed=std::vector<GeometryPacked>{{1,2,3,4}};data.extra_mode=GeometryExtraChannels::current_default;
    auto source=own(std::move(data));GeometryAttributeFailure f;auto vertex=source->vertex(0,{9,10,11,12},f);
    check(vertex&&vertex->packed==GeometryPacked{1,2,3,4}&&vertex->extra==std::vector<GeometryPacked>{{9,10,11,12}},"source count1 extra uses current fallback EVEN with explicit primary bytes");
    data=basic(1);data.extra_mode=GeometryExtraChannels::explicit_streams;
    data.extra_channels={{{1,2,3,4}},{{5,6,7,8}},{{13,14,15,16}}};
    source=own(std::move(data));vertex=source->vertex(0,{90,91,92,93},f);
    check(vertex&&vertex->extra==std::vector<GeometryPacked>{{1,2,3,4},{5,6,7,8},{13,14,15,16}},"source count>1 gathers exactly explicit channels, no fallback prepend");
    data=basic(70003);data.streams[0].primary[70002]=words(0x3f800000,0x40000000,0x40400000);
    source=own(std::move(data));vertex=source->vertex(70002,{1,2,3,4},f);
    check(vertex&&vertex->source_index==70002&&vertex->primary[0]==words(0x3f800000,0x40000000,0x40400000),"native resource/index extent exceeds serialized U16 without source scratch capacity");
    data=basic(1);for(unsigned i=1;i<129;++i)data.streams.push_back({{words(i,i+1,i+2)},std::nullopt});
    source=own(std::move(data));vertex=source->vertex(0,{1,2,3,4},f);
    check(vertex&&vertex->primary.size()==129&&vertex->primary.back()==words(128,129,130),"ALL source streams remain dynamically owned/gathered; no fixed scratch stream array");
    auto emitted=emit_geometry_vertex_attributes(*vertex,GeometryAttributeProgram::first_pair,f);
    check(emitted&&emitted->second&&emitted->second->primary==words(1,2,3),"closed source recipe selects second stream only; higher-stream draw semantics not inferred");
}
void failure_and_ownership(){
    GeometryAttributeFailure f;auto data=basic();data.first_pairs=std::vector<GeometryPair>(2);
    check(!NativeGeometryAttributes::create(std::move(data),f)&&f.kind==GeometryAttributeError::invalid_extent&&data.streams.size()==1,"invalid extent rejected before consuming input");
    data=basic();data.extra_mode=GeometryExtraChannels::explicit_streams;
    check(!NativeGeometryAttributes::create(std::move(data),f)&&f.kind==GeometryAttributeError::invalid_extra_mode,"missing explicit channels is not successful omission");
    data=basic();data.extra_mode=static_cast<GeometryExtraChannels>(99);
    check(!NativeGeometryAttributes::create(std::move(data),f)&&f.kind==GeometryAttributeError::invalid_extra_mode,"unknown extra route");
    data=basic();fail_after=0;auto failed=NativeGeometryAttributes::create(std::move(data),f);
    check(!failed&&f.kind==GeometryAttributeError::allocation,"real native owner allocation failure");
    auto source=own(basic());fail_after=1;auto absent=source->vertex(0,{1,2,3,4},f);
    check(!absent&&f.kind==GeometryAttributeError::allocation,"partial gather allocation publishes no output");
    auto vertex=source->vertex(0,{1,2,3,4},f);check(bool(vertex),"immutable source survives failed gather");
    check(!source->vertex(3,{1,2,3,4},f)&&f.kind==GeometryAttributeError::invalid_index,"actual source bound");
    auto no_primary=own({1,{},std::nullopt,std::nullopt,std::nullopt,GeometryExtraChannels::none,{},std::nullopt});
    check(!no_primary->vertex(0,{1,2,3,4},f)&&f.kind==GeometryAttributeError::missing_primary,"missing primary for real referenced vertex never reads old scratch bytes");
    auto bad=*vertex;bad.companion.clear();check(!emit_geometry_vertex_attributes(bad,GeometryAttributeProgram::first_pair,f)&&f.kind==GeometryAttributeError::invalid_extent,"paired emission extents");
    check(!emit_geometry_vertex_attributes(*vertex,static_cast<GeometryAttributeProgram>(99),f)&&f.kind==GeometryAttributeError::invalid_program,"unbound emitter cannot silently succeed");
    bool reader_ok=false;std::thread reader([&]{GeometryAttributeFailure why;auto x=source->vertex(1,{5,6,7,8},why);reader_ok=x&&x->packed==GeometryPacked{5,6,7,8};});reader.join();check(reader_ok,"immutable native resource may be read on render worker");
    MatFxBindingFailure bf;auto unique=NativeMatFxBinding::create({MatFxMode::disabled,{}},bf);check(bool(unique),"material owner");
    auto m=std::shared_ptr<NativeMatFxBinding>(std::move(unique));MaterialMaintenanceFailure mf;
    auto group=NativeMaterialGroup::create({m},mf);auto geometry=NativeMaterialGeometry::create_with_attributes(group,MatFxHierarchyRoute::plugin_absent,source,mf);
    check(geometry&&geometry->snapshot(mf)->attributes.get()==source.get()&&geometry->snapshot(mf)->imported.source_count==1,"same material geometry owns SAME actual immutable attributes and native count");
    std::weak_ptr<const NativeGeometryAttributes> weak=source;source.reset();check(!weak.expired(),"geometry holds attribute resource lifetime");
    geometry.reset();check(weak.expired()&&emit_geometry_vertex_attributes(*vertex,GeometryAttributeProgram::first_pair,f).has_value(),"owned gathered vertex survives source/geometry retirement");
}
}
int main(){try{
    defaults_and_literal_recipes();extra_routes_and_native_extents();failure_and_ownership();
    std::puts("PASS native owned geometry gather/attribute payloads/material binding; topology/program construction and draw remain open");return 0;
}catch(const std::exception& e){fail_after=-1;std::fprintf(stderr,"FAIL native geometry attributes %s\n",e.what());return 1;}}
