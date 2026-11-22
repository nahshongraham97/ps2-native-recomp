#include "mka/retail_system_art_cpu.hpp"
#include <bit>
#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <new>
namespace mka::retail {
namespace {
// Actual5B4728 locale descriptors and610980 containing-file directory.
constexpr RetailSystemArtDescriptor source[]{
 {"sysart.sec",0x800,0x60880},{"sysart_spa.sec",0x61800,0x60880},
 {"sysart_ger.sec",0xc2800,0x60880},{"sysart_fre.sec",0x123800,0x60880},
 {"sysart_ita.sec",0x184800,0x60880}};
constexpr std::array<std::string_view,9> source_metrics{
 "MOVELIST_FONT_MET","DANTON20_MET","DANTONSILVER20_MET","DANTONSILVER24_MET",
 "EDDMONDGRAY14_MET","EDDMONDGRAY24_MET","BAR_NUMBERFONT_MET","ARIAL_BLACK_10PT_MET","MINIONCOND_18_MET"};
std::uint32_t word(std::span<const std::byte> b,std::size_t i){return std::to_integer<unsigned>(b[i])|(std::to_integer<unsigned>(b[i+1])<<8)|(std::to_integer<unsigned>(b[i+2])<<16)|(std::to_integer<unsigned>(b[i+3])<<24);}
}
std::optional<RetailSystemArtDescriptor> system_art_descriptor_from_source(
 std::int32_t language,RetailSystemArtCpuFailure& f)noexcept{
 f={};
 if(language<0||language>=std::int32_t(std::size(source))){
  f.kind=RetailSystemArtCpuError::language;f.operation="26EF70 actual sysart locale descriptor admission";return {};
 }
 return source[language];
}
std::shared_ptr<const NativeRetailSystemArtCpuSection> prepare_system_art_cpu_section(
 NativeRetailPackage& package,std::int32_t language,RetailSystemArtCpuFailure& f)noexcept{
 f={};if(language<0||language>=std::int32_t(std::size(source))){f.kind=RetailSystemArtCpuError::language;f.operation="26EF70 actual sysart locale descriptor admission";return {};}
 try{
  const auto& selected=source[language];auto member=package.find("gameart.ssf",f.package);
  if(!member){f.kind=RetailSystemArtCpuError::package;f.operation="actual native package gameart.ssf";return {};}
  if(selected.offset>member->size||selected.length>member->size-selected.offset){f.kind=RetailSystemArtCpuError::range;f.operation="actual sysart descriptor range";return {};}
  std::vector<std::byte> serialized(std::size_t(selected.length));
  // Ordinary native object representation: package reads directly into the
  // CPU section's byte owner, which the existing parser then adopts.
  auto writable=std::span<std::uint8_t>(reinterpret_cast<std::uint8_t*>(serialized.data()),serialized.size());
  auto got=package.read(member->name,selected.offset,writable,f.package);
  if(!got||*got!=serialized.size()){f.kind=RetailSystemArtCpuError::package;f.operation="complete actual sysart section read";return {};}
  return decode_system_art_cpu_section(std::move(serialized),language,f);
 }catch(const std::bad_alloc&){f.kind=RetailSystemArtCpuError::allocation;f.operation="owned native sysart CPU data";}
 catch(...){f.kind=RetailSystemArtCpuError::native_exception;f.operation="native sysart CPU preparation";}
 return {};
}
std::shared_ptr<const NativeRetailSystemArtCpuSection> decode_system_art_cpu_section(
 std::vector<std::byte> serialized,std::int32_t language,RetailSystemArtCpuFailure& f)noexcept{
 const auto selected=system_art_descriptor_from_source(language,f);if(!selected)return {};
 if(serialized.size()!=selected->length){f.kind=RetailSystemArtCpuError::range;f.operation="complete selected sysart descriptor bytes";return {};}
 try{
  auto parsed=content::NativeArtSectionData::parse(std::move(serialized),f.section);
  if(!parsed){f.kind=RetailSystemArtCpuError::section;f.operation="3011D0 owned SEC directory";return {};}
  auto owner=std::shared_ptr<NativeRetailSystemArtCpuSection>(new NativeRetailSystemArtCpuSection(language,selected->name,std::move(*parsed)));
  const auto records=owner->section_.records();std::uint32_t first_type3=0;
  for(std::size_t i=0;i<records.size();++i)if((records[i].raw_type&0x3fffffffu)==3){first_type3=std::uint32_t(i);break;}
  for(std::size_t i=0;i<records.size();++i){
   f.ordinal=i;const auto& record=records[i];const auto type=record.raw_type&0x3fffffffu;
   // 300440 can match a type7 record with a null payload, still reporting its
   // length. Do not manufacture metrics or reject that source-null data case.
   if(type==7&&record.payload_offset==0)continue;
   auto bytes=owner->section_.resource_bytes(i,f.section);
   if(!bytes||record.raw_word_08>bytes->size()){f.kind=RetailSystemArtCpuError::section;f.operation="bounded actual SEC payload length";return {};}
   auto payload=bytes->first(record.raw_word_08);
   if(type==2||type==3){
    std::vector<std::uint8_t> input(payload.size());std::memcpy(input.data(),payload.data(),input.size());
    auto texture=FontAtlas::decode(std::move(input),f.texture);
    if(!texture){f.kind=RetailSystemArtCpuError::texture;f.operation="3015E0/229420 actual serialized texture data";return {};}
    // The supported serialized profiles all carry filter/address metadata1101.
    // 3015E0/1A8680 selects filter2 for their non-mip raster, preserving U/V.
    // This is retail texture metadata, not a GS register/device representation.
    const auto& serialized_texture=texture->serialized();
    const auto metadata_at=std::size_t(5)+serialized_texture[4]+8+16;
    const auto raw=std::uint32_t(serialized_texture[metadata_at])|
     (std::uint32_t(serialized_texture[metadata_at+1])<<8)|
     (std::uint32_t(serialized_texture[metadata_at+2])<<16)|
     (std::uint32_t(serialized_texture[metadata_at+3])<<24);
    owner->textures_.push_back({i,std::move(*texture),(raw&0xffffff00u)|2u});
   }else if(type==7&&std::find(source_metrics.begin(),source_metrics.end(),record.name)!=source_metrics.end()){
    std::vector<std::uint8_t> input(payload.size());std::memcpy(input.data(),payload.data(),input.size());
    auto metrics=text::NativeFontMetrics::parse(std::move(input),f.metrics);
    if(!metrics){f.kind=RetailSystemArtCpuError::metrics;f.operation="actual named font MET payload";return {};}
    owner->metrics_.push_back({i,std::move(*metrics)});
   }else if(type==4){
    if(payload.size()<8){f.kind=RetailSystemArtCpuError::section;f.operation="3011D0 group range words";return {};}
    auto first=word(payload,0),last=word(payload,4);
    if(first<=last){first+=first_type3;last+=first_type3;} // Source unsigned low32 addition.
    owner->groups_.push_back({i,first,last});
   }else if(type!=7){f.kind=RetailSystemArtCpuError::unsupported_resource;f.operation="unproved sysart CPU resource kind";return {};}
   // Other type7 data (actual SCR_MANAGED) stays in the immutable directory.
   // This does not implement3DE190 or mark its script/resource effects ready.
  }
  f={};return owner;
 }catch(const std::bad_alloc&){f.kind=RetailSystemArtCpuError::allocation;f.operation="owned native sysart CPU data";}
 catch(...){f.kind=RetailSystemArtCpuError::native_exception;f.operation="native sysart CPU preparation";}
 return {};
}
}
