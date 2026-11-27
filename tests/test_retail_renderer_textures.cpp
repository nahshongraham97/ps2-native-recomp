#include "mka/retail_renderer_textures.hpp"
#include "mka/retail_renderer_rasters.hpp"
#include "mka/retail_renderer_images.hpp"
#include "mka/retail_renderer_transforms.hpp"
#include "mka/retail_renderer_matrix.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_scene_bootstrap_required.hpp"
#include "mka/retail_task_domain.hpp"
#include "native_retail_startup.hpp"
#include "../runtime/src/native/retail_main_control.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <new>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

extern "C" int SDL_InitSubSystem(unsigned) { std::abort(); }

namespace {
thread_local int fail_after = -1;
}

void* operator new(std::size_t n) {
    if (fail_after == 0) throw std::bad_alloc();
    if (fail_after > 0) --fail_after;
    if (auto p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

using namespace mka::retail;

namespace {

void check(bool v, const char* why) {
    if (!v) throw std::runtime_error(why);
}

enum class Later { refuse, transfer, allow };
Later later{Later::refuse};

bool fixture_construct(unsigned id, NativeRetailRendererCore&, RetailRendererCoreFailure&) {
    if (later != Later::allow) return false;
    return true;
}

void fixture_destroy(unsigned) noexcept {}

struct Fixture {
    NativeRetailStartupState startup;
    mka::input::ControllerInput input;
    NativeRetailMainState main;
    StartupFailure sf;
    RetailRendererCoreFailure f;
    RetailRendererAllocatorFailure af;
    std::shared_ptr<NativeRetailRendererAllocatorService> allocator;

    explicit Fixture(const std::filesystem::path& root)
        : startup(open(root), {}),
          main(startup, input) {
        check(initialize_native_retail_game_session(startup, sf) &&
              initialize_retail_memory_owners(startup, sf), "actual Session/heaps");
        allocator = NativeRetailRendererAllocatorService::create(*startup.memory_owners, af);
        check(bool(allocator), "actual native Permanent service");
    }

    static mka::content::NativeContentVolume open(const std::filesystem::path& root) {
        mka::content::ContentVolumeFailure cf;
        auto v = mka::content::NativeContentVolume::open(root, cf);
        check(bool(v), "actual native content");
        return std::move(*v);
    }
};

} // namespace

namespace mka::retail::renderer_required {
#define OTHER(ID, HEX) \
    bool construct_core_plugin_##ID(NativeRetailRendererCore& c, RetailRendererCoreFailure& f) { \
        return fixture_construct(HEX, c, f); \
    } \
    void destroy_core_plugin_##ID(NativeRetailRendererCore&) noexcept { \
        fixture_destroy(HEX); \
    }
OTHER(0409, 0x409)
OTHER(040b, 0x40b)
// Actual0412/040A containers are linked dependencies; keep remaining
// fixture suffix boundaries and every resource/lifetime assertion.
#undef OTHER

void transform_point_exact(RetailRendererPoint3&, const RetailRendererPoint3&, const RetailRendererAffine&) { std::abort(); }
void transform_points_exact(std::span<RetailRendererPoint3>, std::span<const RetailRendererPoint3>, const RetailRendererAffine&) { std::abort(); }
void transform_vector_exact(RetailRendererPoint3&, const RetailRendererPoint3&, const RetailRendererAffine&) { std::abort(); }
void transform_vectors_exact(std::span<RetailRendererPoint3>, std::span<const RetailRendererPoint3>, const RetailRendererAffine&) { std::abort(); }
bool multiply_affines_19f680(RetailRendererAffine&, const RetailRendererAffine&, const RetailRendererAffine&, RetailRendererMatrixFailure&) { std::abort(); }
bool classify_matrix_1a0240(RetailRendererMatrixValue&, const RetailRendererMatrixTolerances&, RetailRendererMatrixFailure&) { std::abort(); }
bool invert_nonidentity_matrix_1a07a0(RetailRendererMatrixValue&, const RetailRendererMatrixValue&, RetailRendererMatrixFailure&) { std::abort(); }
} // namespace mka::retail::renderer_required

namespace mka::retail::scene_bootstrap_required {
RetailStartupGate continue_first_scene_at_225d40(
    NativeRetailMainState&,
    std::shared_ptr<NativeRetailRendererCore> c,
    StartupFailure& f) {
    RetailRendererCoreFailure cf;
    if (!c->construct_registered(cf)) {
        f = {StartupError::memory_owners, "0408 test scene refusal"};
        return RetailStartupGate::native_failure;
    }
    return RetailStartupGate::ready;
}
bool continue_scene_reset_at_125d60(NativeRetailMainState&, StartupFailure&) { std::abort(); }
} // namespace mka::retail::scene_bootstrap_required

int main() {
    try {
        const std::filesystem::path root = ".";

        // 1. Initial State & Construction of 0408
        {
            Fixture fix(root);
            RetailRendererCoreFailure cf;
            auto core = NativeRetailRendererCore::for_session(fix.startup.game_session, fix.allocator, fix.startup.content, cf);
            check(bool(core), "core creation");
            check(core->initialize_core(cf).value_or(false), "initialize core");
            check(core->module_0408_count(cf).value_or(1) == 0, "0408 initial count is 0");

            later = Later::allow;
            check(core->construct_registered(cf), "construct registered up through 0408");
            check(core->module_0408_count(cf).value_or(0) == 1, "0408 constructed count is 1");

            auto tex_mgr = core->textures(cf);
            check(bool(tex_mgr), "textures manager published");

            // 2. Default Sentinel / Dummy Texture Verification
            auto def_tex = tex_mgr->default_texture();
            check(!def_tex.empty(), "default texture exists");

            RetailRendererTextureFailure tf;
            auto def_obs = tex_mgr->observe(def_tex, tf);
            check(bool(def_obs), "observe default texture");
            check(def_obs->is_default, "default texture flagged is_default");
            check(def_obs->filter == RetailRendererTextureFilter::linear, "default linear filter");
            check(def_obs->address_u == RetailRendererTextureAddress::wrap, "default wrap U");
            check(def_obs->address_v == RetailRendererTextureAddress::wrap, "default wrap V");
            check(def_obs->ref_count == 1, "default refcount 1");

            // Attempting to destroy default texture does not retire it
            check(tex_mgr->destroy_texture(def_tex, tf), "destroy default texture tolerated");
            check(tex_mgr->observe(def_tex, tf).has_value(), "default texture still observable");

            // 3. Create Custom Texture with Valid 0407 Raster
            auto rasters = core->rasters(cf);
            check(bool(rasters), "rasters service available");
            RetailRendererRasterFailure rf;
            auto r_handle = rasters->create_raster({16, 16, 32, 4, 4, 0x0500}, rf);
            check(bool(r_handle), "create raster for texture");

            RetailRendererTextureDescription desc;
            desc.name = "kombat_logo";
            desc.mask = "kombat_mask";
            desc.filter = RetailRendererTextureFilter::nearest;
            desc.address_u = RetailRendererTextureAddress::clamp;
            desc.address_v = RetailRendererTextureAddress::mirror;

            auto t_handle = tex_mgr->create_texture(desc, *r_handle, tf);
            check(bool(t_handle), "create texture succeeds");
            check(!t_handle->empty(), "texture handle non-empty");

            auto t_obs = tex_mgr->observe(*t_handle, tf);
            check(bool(t_obs), "observe texture");
            check(t_obs->name == "kombat_logo", "texture name matches");
            check(t_obs->mask == "kombat_mask", "texture mask matches");
            check(t_obs->filter == RetailRendererTextureFilter::nearest, "filter matches");
            check(t_obs->address_u == RetailRendererTextureAddress::clamp, "address U matches");
            check(t_obs->address_v == RetailRendererTextureAddress::mirror, "address V matches");
            check(t_obs->has_raster, "texture has raster");
            check(t_obs->ref_count == 1, "initial refcount is 1");

            // 4. Lookup by Name
            auto found = tex_mgr->find_texture("kombat_logo", tf);
            check(found.has_value() && found->same_identity(*t_handle), "exact name lookup");
            auto found_ci = tex_mgr->find_texture("KOMBAT_LOGO", tf);
            check(found_ci.has_value() && found_ci->same_identity(*t_handle), "case-insensitive lookup");
            auto found_none = tex_mgr->find_texture("missing_tex", tf);
            check(!found_none.has_value(), "missing name returns nullopt");

            // 5. Reference Counting Lifecycle
            auto new_rc = tex_mgr->add_ref(*t_handle, tf);
            check(new_rc.value_or(0) == 2, "add_ref returns 2");
            check(tex_mgr->destroy_texture(*t_handle, tf), "destroy_texture decrements");
            t_obs = tex_mgr->observe(*t_handle, tf);
            check(t_obs.has_value() && t_obs->ref_count == 1, "refcount decremented to 1");

            check(tex_mgr->destroy_texture(*t_handle, tf), "destroy_texture retires at 0");
            check(!tex_mgr->observe(*t_handle, tf).has_value(), "retired texture cannot be observed");
            check(tf.kind == RetailRendererTextureError::retired_texture, "error is retired_texture");

            // 6. Negative Path: Create Texture with Empty Raster
            NativeRetailRasterHandle empty_raster;
            auto fail_tex = tex_mgr->create_texture(desc, empty_raster, tf);
            check(!fail_tex.has_value(), "empty raster creation rejected");
            check(tf.kind == RetailRendererTextureError::raster_required, "error is raster_required");

            // 7. Reentrant Plugin Construction
            check(!renderer_required::construct_core_plugin_0408(*core, cf), "reentrant 0408 rejected");
            check(cf.kind == RetailRendererCoreError::reentrant, "reentrant error flagged");

            // 8. Destruction & Teardown
            check(core->destroy_constructed(cf), "destroy constructed");
            check(core->module_0408_count(cf).value_or(1) == 0, "0408 count reset to 0");
            check(!core->textures(cf), "textures service cleared after destroy");
        }

        // 9. Thread Affinity Negative Check
        {
            Fixture fix(root);
            RetailRendererCoreFailure cf;
            auto core = NativeRetailRendererCore::for_session(fix.startup.game_session, fix.allocator, fix.startup.content, cf);
            core->initialize_core(cf);
            later = Later::allow;
            core->construct_registered(cf);
            auto tex_mgr = core->textures(cf);

            std::thread([&] {
                RetailRendererTextureFailure tf;
                auto def = tex_mgr->default_texture();
                auto obs = tex_mgr->observe(def, tf);
                check(!obs.has_value(), "cross-thread observe rejected");
                check(tf.kind == RetailRendererTextureError::wrong_thread, "error is wrong_thread");
            }).join();
        }

        std::puts("PASS: test_native_retail_renderer_textures passed all 9 verification stages.");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}
