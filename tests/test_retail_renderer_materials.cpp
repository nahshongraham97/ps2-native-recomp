#include "mka/retail_renderer_materials.hpp"
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
OTHER(040b, 0x40b)
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
        f = {StartupError::memory_owners, "040a test scene refusal"};
        return RetailStartupGate::native_failure;
    }
    return RetailStartupGate::ready;
}
bool continue_scene_reset_at_125d60(NativeRetailMainState&, StartupFailure&) { std::abort(); }
} // namespace mka::retail::scene_bootstrap_required

int main() {
    try {
        const std::filesystem::path root = ".";
        Fixture fix(root);

        // 1. Initial State: No materials module, module count is 0
        {
            RetailRendererCoreFailure cf;
            auto core = NativeRetailRendererCore::for_session(
                fix.startup.game_session, fix.allocator, fix.startup.content, cf);
            check(bool(core), "construct core");

            check(!core->materials(cf), "no materials module initially");
            check(core->module_040a_count(cf).value_or(1) == 0, "040a initial count is 0");

            // 2. Core Plugin 040a Construction (Module Count 0 -> 1)
            check(renderer_required::construct_core_plugin_040a(*core, cf), "construct 040a");
            check(core->module_040a_count(cf).value_or(0) == 1, "040a count is 1 after construction");
            auto mat_mgr = core->materials(cf);
            check(bool(mat_mgr), "materials module available");

            RetailRendererMaterialFailure mf;
            check(mat_mgr->active_count() == 0, "active material count initially 0");

            // 3. Material Creation: Default Properties & Handle Identity
            auto mat1 = mat_mgr->create_material(mf);
            check(bool(mat1) && !mat1->empty(), "create material 1");
            check(mat_mgr->active_count() == 1, "active material count is 1");

            auto color1 = mat_mgr->get_color(*mat1, mf);
            check(bool(color1), "get color 1");
            check(*color1 == RetailRendererRgbaColor(255, 255, 255, 255), "default color is 255,255,255,255");

            auto surf1 = mat_mgr->get_surface_properties(*mat1, mf);
            check(bool(surf1), "get surface 1");
            check(surf1->ambient == 1.0f && surf1->specular == 1.0f && surf1->diffuse == 1.0f, "default surface is 1,1,1");

            auto obs1 = mat_mgr->observe(*mat1, mf);
            check(bool(obs1), "observe material 1");
            check(obs1->ref_count == 1, "initial ref count is 1");
            check(!obs1->has_texture, "initially no texture");

            // 4. Material Properties Mutation (Color & Surface)
            RetailRendererRgbaColor custom_color{128, 64, 32, 200};
            check(mat_mgr->set_color(*mat1, custom_color, mf), "set custom color");
            auto color_read = mat_mgr->get_color(*mat1, mf);
            check(bool(color_read) && *color_read == custom_color, "color updated correctly");

            RetailRendererSurfaceProperties custom_surf{0.25f, 0.5f, 0.75f};
            check(mat_mgr->set_surface_properties(*mat1, custom_surf, mf), "set custom surface");
            auto surf_read = mat_mgr->get_surface_properties(*mat1, mf);
            check(bool(surf_read) && *surf_read == custom_surf, "surface updated correctly");

            // 5. Multiple Material Creation & Distinct Handle Identity
            auto mat2 = mat_mgr->create_material(mf);
            check(bool(mat2) && !mat2->empty(), "create material 2");
            check(mat_mgr->active_count() == 2, "active material count is 2");
            check(!mat1->same_identity(*mat2), "materials have distinct identity");

            // 6. Reference Counting (add_ref, release, auto-destruction)
            auto new_ref = mat_mgr->add_ref(*mat2, mf);
            check(bool(new_ref) && *new_ref == 2, "add_ref incremented count to 2");
            check(mat_mgr->release(*mat2, mf), "release decrements count to 1");
            check(mat_mgr->active_count() == 2, "material 2 still alive at ref count 1");

            check(mat_mgr->release(*mat2, mf), "release decrements count to 0 (auto-destroy)");
            check(mat_mgr->active_count() == 1, "active count drops to 1 after release");

            // 7. Generational Handle Invalidation (Fail-Closed Safety)
            RetailRendererMaterialFailure ret_f;
            check(!mat_mgr->get_color(*mat2, ret_f), "reject get_color on retired material");
            check(ret_f.kind == RetailRendererMaterialError::retired_material, "reported retired_material error");

            // 8. Explicit Material Destruction
            check(mat_mgr->destroy_material(*mat1, mf), "destroy material 1");
            check(mat_mgr->active_count() == 0, "active material count drops to 0");
            check(!mat_mgr->observe(*mat1, ret_f), "reject observe on destroyed material");

            // 9. Thread Affinity Checks
            std::thread([&]() {
                RetailRendererMaterialFailure thread_f;
                check(!mat_mgr->create_material(thread_f), "foreign thread rejected");
                check(thread_f.kind == RetailRendererMaterialError::wrong_thread, "wrong_thread error reported");
            }).join();

            // 10. Core Plugin Teardown & Module Count Decrement (1 -> 0)
            renderer_required::destroy_core_plugin_040a(*core);
            check(core->module_040a_count(cf).value_or(1) == 0, "040a count is 0 after destroy");
            check(!core->materials(cf), "no materials module after destroy");
        }

        std::puts("PASS: test_native_retail_renderer_materials passed all 10 scoped verification stages.");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}
