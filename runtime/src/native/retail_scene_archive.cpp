#include "mka/retail_scene_archive.hpp"
#include <array>
#include <cstdio>
#include <limits>
#include <new>

namespace mka::retail {
bool RetailSceneResources::empty() const noexcept {
    return rasters.empty() && textures.empty() && materials.empty() && frames.empty() &&
        scene_objects.empty() && !owns_bsp;
}
bool RetailSceneResources::matches(NativeRetailRendererRasters* r, NativeRetailRendererTextures* t,
    NativeRetailRendererMaterials* m, NativeRetailRendererFrames* f, NativeRetailWorld* w) const noexcept {
    return thread == std::this_thread::get_id() &&
        (!r || raster_owner.get() == r) && (!t || texture_owner.get() == t) &&
        (!m || material_owner.get() == m) && (!f || frame_owner.get() == f) && (!w || world_owner.get() == w);
}
RetailSceneResources::~RetailSceneResources() {
    RetailRendererWorldFailure f;
    if (!cleanup(f)) std::fprintf(stderr, "native scene resource cleanup refused: %s\n", f.operation);
}
bool RetailSceneResources::cleanup(RetailRendererWorldFailure& failure) noexcept {
    failure = {};
    if (empty()) return true;
    if (thread != std::this_thread::get_id()) {
        failure.kind = RetailRendererWorldError::wrong_thread;
        failure.operation = "scene resource cleanup requires its owner thread"; return false;
    }
    const auto reject = [&](const char* operation) {
        failure.kind = RetailRendererWorldError::owner; failure.operation = operation; return false;
    };
    try {
        while (!scene_objects.empty()) {
            if (!world_owner) return reject("scene resource cleanup missing world");
            if (!world_owner->remove_scene_object(scene_objects.back(), failure)) return false;
            scene_objects.pop_back();
        }
        if (owns_bsp) {
            if (!world_owner) return reject("scene BSP cleanup missing world");
            world_owner->clear_bsp_sectors(); owns_bsp = false;
        }
        while (!frames.empty()) {
            RetailRendererFrameFailure f;
            if (!frame_owner) return reject("scene frame cleanup missing owner");
            if (!frame_owner->destroy_leaf(frames.back(), f)) return reject(f.operation);
            frames.pop_back();
        }
        while (!materials.empty()) {
            RetailRendererMaterialFailure f;
            if (!material_owner) return reject("scene material cleanup missing owner");
            if (!material_owner->destroy_material(materials.back(), f)) return reject(f.operation);
            materials.pop_back();
        }
        while (!textures.empty()) {
            RetailRendererTextureFailure f;
            if (!texture_owner) return reject("scene texture cleanup missing owner");
            if (!texture_owner->destroy_texture(textures.back(), f)) return reject(f.operation);
            textures.pop_back();
        }
        while (!rasters.empty()) {
            RetailRendererRasterFailure f;
            if (!raster_owner) return reject("scene raster cleanup missing owner");
            if (!raster_owner->destroy_raster(rasters.back(), f)) return reject(f.operation);
            rasters.pop_back();
        }
        return true;
    } catch (...) {
        failure.kind = RetailRendererWorldError::native_exception;
        failure.operation = "scene resource cleanup native exception"; return false;
    }
}

namespace {
std::uint32_t u32(const std::vector<std::uint8_t>& b, std::size_t i) {
    return std::uint32_t(b[i]) | (std::uint32_t(b[i+1]) << 8) |
        (std::uint32_t(b[i+2]) << 16) | (std::uint32_t(b[i+3]) << 24);
}
bool root(const std::vector<std::uint8_t>& b, unsigned role, RetailSceneArchiveFailure& f) {
    if (b.size() >= 4 && u32(b, 0) == 0x53454320) {
        f.kind = RetailSceneArchiveError::unsupported_sec;
        f.operation = "selected archive member is SEC; nested SEC selection is not proved"; return false;
    }
    if (b.size() < 12 || u32(b, 4) != b.size() - 12) {
        f.kind = RetailSceneArchiveError::invalid_rw_root;
        f.operation = "selected archive member requires one complete bounded RW root"; return false;
    }
    const auto id = u32(b, 0);
    if ((role == 0 && id == 0x16) || (role == 1 && id == 0xb) ||
        (role == 2 && (id == 0xf || id == 0x10))) return true;
    f.kind = RetailSceneArchiveError::invalid_rw_root;
    f.operation = "selected archive member RW root does not match its explicit role"; return false;
}
}

std::optional<RetailArchiveSceneLoadResult> load_retail_scene_streams_from_archive(
    const content::NativeContentVolume& volume, const RetailArchiveSceneStreams& config,
    std::uint64_t base_sector_id, std::uint64_t base_object_id,
    NativeRetailRendererRasters& rasters, NativeRetailRendererTextures& textures,
    NativeRetailRendererMaterials& materials, NativeRetailRendererFrames& frames, NativeRetailWorld& world,
    const NativeRetailRendererFrameHandle& parent_frame, RetailSceneArchiveFailure& failure,
    RetailSceneResources& resources) noexcept {
    failure = {};
    if (!resources.empty() || !resources.matches(&rasters, &textures, &materials, &frames, &world)) {
        failure.kind = RetailSceneArchiveError::resource_owner;
        failure.operation = "archive scene requires a fresh collector bound to these actual owners"; return {};
    }
    bool committed = false;
    struct Rollback {
        RetailSceneResources& resources; bool& committed;
        ~Rollback() { if (!committed) { RetailRendererWorldFailure ignored; (void)resources.cleanup(ignored); } }
    } rollback{resources, committed};
    try {
        if (config.archive_resource_key.empty() || (!config.txd && !config.world && !config.geometry) ||
            (config.world && !base_sector_id) || (config.geometry && !base_object_id)) {
            failure.kind = RetailSceneArchiveError::invalid_selection;
            failure.operation = "archive scene requires a resource, explicit members and nonzero selected base IDs"; return {};
        }
        if (!world.admit_render_modules(&materials, &textures, &rasters, failure.renderer)) {
            failure.kind = RetailSceneArchiveError::resource_owner; failure.operation = failure.renderer.operation; return {};
        }
        auto file = volume.open_resource(config.archive_resource_key, failure.content);
        if (!file) { failure.kind = RetailSceneArchiveError::content; failure.operation = "archive scene source file open"; return {}; }
        auto archive = NativeRetailArchive::open(std::move(*file), failure.archive);
        if (!archive) { failure.kind = RetailSceneArchiveError::archive; failure.operation = failure.archive.operation; return {}; }
        std::array<std::vector<std::uint8_t>, 3> bytes;
        const std::array<const std::optional<RetailArchiveMemberSelector>*, 3> selected{&config.txd, &config.world, &config.geometry};
        RetailArchiveSceneLoadResult result;
        for (unsigned role = 0; role < selected.size(); ++role) {
            if (!*selected[role]) continue;
            const auto& selector = **selected[role];
            if (selector.name.has_value() == selector.index.has_value()) {
                failure.kind = RetailSceneArchiveError::invalid_selection;
                failure.operation = "archive member selector requires exactly one full name or ordinal"; return {};
            }
            auto member = selector.name ? archive->find(*selector.name, failure.archive) : archive->at(*selector.index, failure.archive);
            if (!member) { failure.kind = RetailSceneArchiveError::archive; failure.operation = failure.archive.operation; return {}; }
            auto extracted = archive->extract(member->index, failure.archive);
            if (!extracted) { failure.kind = RetailSceneArchiveError::archive; failure.operation = failure.archive.operation; return {}; }
            bytes[role] = std::move(*extracted);
            if (!root(bytes[role], role, failure)) return {};
            if (bytes[role].size() > std::numeric_limits<std::size_t>::max() - result.bytes_read) {
                failure.kind = RetailSceneArchiveError::allocation; failure.operation = "archive scene aggregate byte count overflow"; return {};
            }
            result.bytes_read += bytes[role].size();
        }
        const auto span = [&](unsigned i) { return std::as_bytes(std::span(bytes[i])); };
        if (config.txd) {
            RetailRendererTextureFailure f;
            if (!ingest_rw_tex_dictionary_stream(span(0), rasters, textures, f, &resources)) {
                failure.kind = RetailSceneArchiveError::ingestion; failure.operation = f.operation; return {};
            }
            result.txd_ingested = true;
        }
        if (config.world) {
            if (!ingest_rw_world_bsp_stream(span(1), base_sector_id, materials, textures, world, failure.renderer, &resources)) {
                failure.kind = RetailSceneArchiveError::ingestion; failure.operation = failure.renderer.operation; return {};
            }
            result.world_ingested = true;
        }
        if (config.geometry) {
            if (!ingest_rw_geometry_stream(span(2), base_object_id, frames, materials, textures, world, parent_frame, failure.renderer, &resources)) {
                failure.kind = RetailSceneArchiveError::ingestion; failure.operation = failure.renderer.operation; return {};
            }
            result.geometry_ingested = true;
        }
        committed = true;
        return result;
    } catch (const std::bad_alloc&) {
        failure.kind = RetailSceneArchiveError::allocation; failure.operation = "archive scene allocation";
    } catch (...) {
        failure.kind = RetailSceneArchiveError::native_exception; failure.operation = "archive scene native exception";
    }
    return {};
}
} // namespace mka::retail
