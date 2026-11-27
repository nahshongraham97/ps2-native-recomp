#pragma once
#include "mka/retail_archive.hpp"
#include "mka/retail_scene_resources.hpp"

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
    resource_owner, ingestion, allocation, native_exception
};
struct RetailSceneArchiveFailure {
    RetailSceneArchiveError kind{RetailSceneArchiveError::none};
    const char* operation{""};
    content::ContentVolumeFailure content{};
    RetailArchiveFailure archive{};
    RetailRendererWorldFailure renderer{};
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
} // namespace mka::retail
