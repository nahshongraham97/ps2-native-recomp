#pragma once
#include "mka/retail_package.hpp"
#include "mka/art_section_data.hpp"
#include "mka/font_atlas.hpp"
#include "mka/font_metrics.hpp"
#include <memory>
namespace mka::retail {
enum class RetailSystemArtCpuError {none,language,package,range,section,texture,metrics,unsupported_resource,allocation,native_exception};
struct RetailSystemArtCpuFailure {
 RetailSystemArtCpuError kind{RetailSystemArtCpuError::none};const char* operation{""};std::size_t ordinal{};
 RetailPackageFailure package;content::ArtSectionFailure section;FontAtlasFailure texture;text::FontMetricsFailure metrics;
};
struct RetailSystemArtTextureData {std::size_t ordinal;FontAtlas data;std::uint32_t sampler_metadata;};
struct RetailSystemArtMetricsData {std::size_t ordinal;text::NativeFontMetrics data;};
struct RetailSystemArtGroupRange {std::size_t ordinal;std::uint32_t first,last;};
struct RetailSystemArtDescriptor {std::string_view name;std::uint64_t offset,length;};
std::optional<RetailSystemArtDescriptor> system_art_descriptor_from_source(
 std::int32_t,RetailSystemArtCpuFailure&)noexcept;
// Immutable CPU result of the actual selected sysart descriptor. This is an
// explicitly PARTIAL section owner: no converted/loaded flag, GPU handle,
// managed-script readiness, source cache-slot publication or font readiness.
class NativeRetailSystemArtCpuSection final {
public:
 std::int32_t language_selector()const noexcept{return language_;}
 std::string_view descriptor_name()const noexcept{return descriptor_;}
 const content::NativeArtSectionData& directory()const & noexcept{return section_;}
 const content::NativeArtSectionData& directory()const &&=delete;
 std::span<const RetailSystemArtTextureData> textures()const & noexcept{return textures_;}
 std::span<const RetailSystemArtTextureData> textures()const &&=delete;
 std::span<const RetailSystemArtMetricsData> metrics()const & noexcept{return metrics_;}
 std::span<const RetailSystemArtMetricsData> metrics()const &&=delete;
 std::span<const RetailSystemArtGroupRange> group_ranges()const & noexcept{return groups_;}
 std::span<const RetailSystemArtGroupRange> group_ranges()const &&=delete;
private:
  friend std::shared_ptr<const NativeRetailSystemArtCpuSection> prepare_system_art_cpu_section(
    NativeRetailPackage&,std::int32_t,RetailSystemArtCpuFailure&)noexcept;
  friend std::shared_ptr<const NativeRetailSystemArtCpuSection> decode_system_art_cpu_section(
    std::vector<std::byte>,std::int32_t,RetailSystemArtCpuFailure&)noexcept;
 NativeRetailSystemArtCpuSection(std::int32_t l,std::string_view name,content::NativeArtSectionData&& s):language_(l),descriptor_(name),section_(std::move(s)){}
 std::int32_t language_;std::string_view descriptor_;content::NativeArtSectionData section_;
 std::vector<RetailSystemArtTextureData> textures_;std::vector<RetailSystemArtMetricsData> metrics_;std::vector<RetailSystemArtGroupRange> groups_;
};
// Required CPU ingress/decoding for 19ABF0->19B530->3011D0. The caller supplies
// the actual182E30/Main.post_overlay_state value. No fixed-locale fallback.
// Reads only the current NativeRetailPackage loose member, never MKDA.PAK.
std::shared_ptr<const NativeRetailSystemArtCpuSection> prepare_system_art_cpu_section(
 NativeRetailPackage&,std::int32_t,RetailSystemArtCpuFailure&)noexcept;
// Adopts completed native read bytes. Identical CPU decoding, with no second
// package read and no claim that GPU conversion or job retirement has occurred.
std::shared_ptr<const NativeRetailSystemArtCpuSection> decode_system_art_cpu_section(
 std::vector<std::byte>,std::int32_t,RetailSystemArtCpuFailure&)noexcept;
}
