#pragma once
#include "mka/retail_camera_world.hpp"
#include "mka/retail_renderer_core.hpp"
#include "mka/retail_renderer_world.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace mka::retail {

class NativeProductionDisplayService;

struct RetailFirstSceneConfig {
    std::string txd_resource_key{"scene/first.txd"};
    std::string world_resource_key{"scene/first.bsp"};
    std::string geometry_resource_key{"scene/first.dff"};
    std::uint64_t base_sector_id{1};
    std::uint64_t base_object_id{1};
    std::int32_t viewport_width{640};
    std::int32_t viewport_height{448};
    float fov_degrees{60.0f};
    float near_clip{0.5f};
    float far_clip{125.0f};
    std::array<float, 4> clear_color{0.05f, 0.08f, 0.14f, 1.0f};
    std::uint32_t clear_flags{0x3u};
    bool call_show_raster{true};
    std::shared_ptr<NativeProductionDisplayService> display;
};

struct NativeRetailSceneOwners {
    RetailFirstSceneConfig config;
    std::shared_ptr<NativeRetailRendererCore> core;
    std::shared_ptr<NativeRetailRendererWorldModule> world_module;
    NativeRetailWorldHandle world_handle;
    std::shared_ptr<NativeRetailWorld> world;
    std::shared_ptr<NativeRetailRendererFrames> frames;
    std::shared_ptr<NativeRetailRendererCameraModule> cameras;
    std::shared_ptr<NativeRetailRendererRasters> rasters;
    std::shared_ptr<NativeRetailRendererTextures> textures;
    std::shared_ptr<NativeRetailRendererMaterials> materials;
    std::shared_ptr<NativeProductionDisplayService> display;
    std::optional<RetailCameraInit180ED0Result> camera_init;
    RetailSceneStreamLoadResult stream_result{};
    std::uint64_t frames_rendered{0};
    bool scene_loaded{false};
    bool gl_context_active{false};
    bool geometry_ingested{false};
    ~NativeRetailSceneOwners() noexcept;
};

} // namespace mka::retail
