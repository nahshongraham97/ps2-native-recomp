#pragma once
#include "mka/retail_renderer_world.hpp"
#include <thread>
#include <utility>

namespace mka::retail {
// Scene keeps this owner until explicit cleanup, before retiring its modules.
// Shared owners also make destructor rollback safe during construction unwind.
class RetailSceneResources final {
public:
    RetailSceneResources(std::shared_ptr<NativeRetailRendererRasters> r,
        std::shared_ptr<NativeRetailRendererTextures> t,
        std::shared_ptr<NativeRetailRendererMaterials> m,
        std::shared_ptr<NativeRetailRendererFrames> f,
        std::shared_ptr<NativeRetailWorld> w) noexcept
        : raster_owner(std::move(r)), texture_owner(std::move(t)), material_owner(std::move(m)),
          frame_owner(std::move(f)), world_owner(std::move(w)) {}
    ~RetailSceneResources();
    RetailSceneResources(const RetailSceneResources&) = delete;
    RetailSceneResources& operator=(const RetailSceneResources&) = delete;
    RetailSceneResources(RetailSceneResources&&) = delete;
    RetailSceneResources& operator=(RetailSceneResources&&) = delete;
    bool cleanup(RetailRendererWorldFailure&) noexcept;
    bool matches(NativeRetailRendererRasters*, NativeRetailRendererTextures*,
        NativeRetailRendererMaterials*, NativeRetailRendererFrames*, NativeRetailWorld*) const noexcept;
    bool empty() const noexcept;

    // Ingestion journals only successfully created, actual generational handles.
    std::vector<NativeRetailRasterHandle> rasters;
    std::vector<NativeRetailTextureHandle> textures;
    std::vector<NativeRetailMaterialHandle> materials;
    std::vector<NativeRetailRendererFrameHandle> frames;
    std::vector<std::uint64_t> scene_objects;
    bool owns_bsp{false};
private:
    std::shared_ptr<NativeRetailRendererRasters> raster_owner;
    std::shared_ptr<NativeRetailRendererTextures> texture_owner;
    std::shared_ptr<NativeRetailRendererMaterials> material_owner;
    std::shared_ptr<NativeRetailRendererFrames> frame_owner;
    std::shared_ptr<NativeRetailWorld> world_owner;
    const std::thread::id thread{std::this_thread::get_id()};
};
} // namespace mka::retail
