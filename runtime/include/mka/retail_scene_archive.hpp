#pragma once
#include "mka/retail_archive.hpp"
#include "mka/retail_scene_resources.hpp"
#include "mka/retail_ssf_bundle.hpp"
#include "mka/retail_ssf_geometry.hpp"
#include "mka/retail_ssf_texture.hpp"

namespace mka::retail {
struct RetailArchiveMemberSelector {
    std::optional<std::string> name;
    std::optional<std::size_t> index;
};
struct RetailArchiveSceneStreams {
    std::string archive_resource_key;
    std::optional<RetailArchiveMemberSelector> txd;
    std::optional<RetailArchiveMemberSelector> world;
    std::optional<RetailArchiveMemberSelector> geometry;
};
enum class RetailSceneArchiveError {
    none, invalid_selection, content, archive, unsupported_sec, invalid_rw_root,
    resource_owner, ingestion, allocation, native_exception, ssf_bundle,
    ssf_geometry, ssf_texture, texture_binding, render_policy
};
struct RetailSceneArchiveFailure {
    RetailSceneArchiveError kind{RetailSceneArchiveError::none};
    const char* operation{""};
    content::ContentVolumeFailure content{};
    RetailArchiveFailure archive{};
    RetailRendererWorldFailure renderer{};
    RetailSsfBundleFailure ssf{};
    RetailSsfGeometryFailure geometry{};
    RetailSsfTextureFailure texture{};
    RetailRendererWorldFailure cleanup{};
};
struct RetailArchiveSceneLoadResult {
    std::size_t bytes_read{};
    bool txd_ingested{}, world_ingested{}, geometry_ingested{};
};
// Selects only complete, explicitly named/indexed RW payloads. All payloads
// are extracted and root-checked before any renderer mutation. Raw SEC and
// unknown substream layouts fail by name; no suffix/signature searching.
std::optional<RetailArchiveSceneLoadResult> load_retail_scene_streams_from_archive(
    const content::NativeContentVolume&, const RetailArchiveSceneStreams&,
    std::uint64_t base_sector_id, std::uint64_t base_object_id,
    NativeRetailRendererRasters&, NativeRetailRendererTextures&,
    NativeRetailRendererMaterials&, NativeRetailRendererFrames&, NativeRetailWorld&,
    const NativeRetailRendererFrameHandle& parent_frame, RetailSceneArchiveFailure&,
    RetailSceneResources&) noexcept;

enum class RetailSsfSceneRenderPolicy {
    unbound_retail,
    diagnostic_unlit_raw_rgba8,
    diagnostic_native_vif_unlit_raw_rgba8
};
struct RetailSsfSceneSelection {
    std::string archive_resource_key;
    RetailArchiveMemberSelector member;
    std::size_t section_index{};
    RetailArchiveMemberSelector clump_resource;
    std::size_t atomic_index{}, morph_index{};
    std::vector<std::size_t> extra_atomic_indices;
    std::vector<std::size_t> texture_resource_indices;
    RetailSsfSceneRenderPolicy render_policy{RetailSsfSceneRenderPolicy::unbound_retail};
};
struct RetailDecodedSsfScene {
    RetailSsfGeometryData geometry;
    std::vector<RetailSsfGeometryData> extra_geometries;
    std::vector<RetailSsfTextureData> textures;
    // Bounded selected record extents supplied to admission/decoding, including
    // source wrappers and opaque record padding. This is not a file IO receipt
    // or a count of just the bytes inside the RW clump root.
    std::size_t serialized_resource_bytes{};
};
// Explicit table-based selection and CPU decoding. Texture body names must
// match the selected geometry's material references; SEC directory names are
// never substituted. No native owner or GPU publication occurs in this stage.
std::optional<RetailDecodedSsfScene> decode_retail_ssf_scene_from_archive(
    const content::NativeContentVolume&, const RetailSsfSceneSelection&,
    RetailSceneArchiveFailure&) noexcept;
// Explicit component preview policy only. It retains real geometry, selected
// morph, frames, texture pixels and raw source metadata, but does not implement
// a retail scene/morph producer or claim PS2 material/color equivalence.
std::optional<RetailDecodedSsfScene> load_retail_ssf_scene_from_archive(
    const content::NativeContentVolume&, const RetailSsfSceneSelection&, std::uint64_t base_object_id,
    NativeRetailRendererRasters&, NativeRetailRendererTextures&, NativeRetailRendererMaterials&,
    NativeRetailRendererFrames&, NativeRetailWorld&, const NativeRetailRendererFrameHandle& parent_frame,
    RetailSceneArchiveFailure&, RetailSceneResources&) noexcept;
} // namespace mka::retail
