from pathlib import Path
p=Path('tests/test_retail_renderer_core.cpp');b=p.read_bytes().replace(b'\r\n',b'\n');s=b.decode();s=s.replace('#include "mka/retail_renderer_core.hpp"','#include "mka/retail_renderer_core.hpp"\n#include "mka/retail_renderer_transforms.hpp"');s=s.replace('if(id==0x401','if(id==0x402');s=s.replace('bool construct_core_plugin_0401(NativeRetailRendererCore& c,RetailRendererCoreFailure& f){return fixture_construct(0x0401,c,f);}\nvoid destroy_core_plugin_0401(NativeRetailRendererCore&)noexcept{fixture_destroy(0x0401);}\n','');s=s.replace('const int expected_failure[]{0x401,0x402,0x403,-0x402,-0x401};check(event_count==5','const int expected_failure[]{0x402,0x403,-0x402};check(event_count==3').replace('for(unsigned i=0;i<5;++i)check(events[i]==expected_failure[i]','for(unsigned i=0;i<3;++i)check(events[i]==expected_failure[i]');s=s.replace('namespace mka::retail::renderer_required {','''namespace {unsigned default_calls[4]{};unsigned custom_calls{};
void custom_one(RetailRendererPoint3& out,const RetailRendererPoint3& in,const RetailRendererAffine&){++custom_calls;out=in;}
void custom_many(std::span<RetailRendererPoint3> out,std::span<const RetailRendererPoint3> in,const RetailRendererAffine&){++custom_calls;for(std::size_t i=0;i<in.size();++i)out[i]=in[i];}
}
namespace mka::retail::renderer_required {
// Explicit unresolved-math fixtures: test dispatch/alias/lifetime only,not EE math.
void transform_point_exact(RetailRendererPoint3& o,const RetailRendererPoint3& i,const RetailRendererAffine& m){++default_calls[0];custom_one(o,i,m);}
void transform_points_exact(std::span<RetailRendererPoint3> o,std::span<const RetailRendererPoint3> i,const RetailRendererAffine& m){++default_calls[1];custom_many(o,i,m);}
void transform_vector_exact(RetailRendererPoint3& o,const RetailRendererPoint3& i,const RetailRendererAffine& m){++default_calls[2];custom_one(o,i,m);}
void transform_vectors_exact(std::span<RetailRendererPoint3> o,std::span<const RetailRendererPoint3> i,const RetailRendererAffine& m){++default_calls[3];custom_many(o,i,m);}
''',1)
p.write_bytes(s.encode())
