#include "mka/retail_renderer_geometry.hpp"
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
        f = {StartupError::memory_owners, "040b test scene refusal"};
        return RetailStartupGate::native_failure;
    }
    return RetailStartupGate::ready;
}
bool continue_scene_reset_at_125d60(NativeRetailMainState&, StartupFailure&) { std::abort(); }
} // namespace mka::retail::scene_bootstrap_required

// Verification Classifications:
// - [source-derived]: Directly verified against instructions in Xbox default.xbe or PS2 SLUS_214.10.
// - [native-design-check]: Fail-closed RAII, thread affinity, rollback, or generational handle safety.
// - [oracle-differential]: NONE. None of these stages is oracle-differential because no usable retail
//   oracle execution trace exists for offline isolated component runs.
//
// Source Provenance for Geometry:
// - Registration: Plugin ID 0x040b, size 40 bytes (Xbox default.xbe 0x002c021a, PS2 SLUS_214.10 0x001a6858).
// - Constructor: Xbox 0x002c2d40, PS2 0x001a1b30. Module count tracking at Xbox 0x0083531c, PS2 -23648($gp).
// - Destructor: Xbox 0x002c30a0, PS2 0x001a1b90. Clears base and decrements module count.

int main() {
    try {
        const std::filesystem::path root = ".";
        Fixture fix(root);

        // Stage 1 [source-derived]: Initial State: No geometry module, module count is 0
        {
            RetailRendererCoreFailure cf;
            auto core = NativeRetailRendererCore::for_session(
                fix.startup.game_session, fix.allocator, fix.startup.content, cf);
            check(bool(core), "construct core");

            check(!core->geometry(cf), "no geometry module initially");
            check(core->module_040b_count(cf).value_or(1) == 0, "040b initial count is 0");

            // Stage 2 [source-derived]: Core Plugin 040b Construction (Module Count 0 -> 1)
            // Provenance: Xbox default.xbe 0x002c2d40 / 0x002c021a; PS2 SLUS_214.10 0x001a1b30 / 0x001a6858
            check(renderer_required::construct_core_plugin_040b(*core, cf), "construct 040b");
            check(core->module_040b_count(cf).value_or(0) == 1, "040b count is 1 after construction");
            auto geom_mgr = core->geometry(cf);
            check(bool(geom_mgr), "geometry module available");

            RetailRendererGeometryFailure gf;
            check(geom_mgr->active_count() == 0, "active geometry count initially 0");

            // Stage 3 [source-derived]: Geometry 40-byte Container Creation & Handle Identity
            // Provenance: Xbox 0x002c2d40 (40-byte container allocation)
            auto geom1 = geom_mgr->create_geometry(gf);
            check(bool(geom1) && !geom1->empty(), "create geometry 1");
            check(geom_mgr->active_count() == 1, "active geometry count is 1");

            // Stage 4 [source-derived]: Geometry Description & Observation
            RetailRendererGeometryDescription desc{0x00000008, 128, 64};
            auto geom2 = geom_mgr->create_geometry(desc, gf);
            check(bool(geom2) && !geom2->empty(), "create geometry 2 with desc");
            check(geom_mgr->active_count() == 2, "active geometry count is 2");

            auto obs2 = geom_mgr->observe(*geom2, gf);
            check(bool(obs2), "observe geometry 2");
            check(obs2->flags == 0x00000008, "flags match");
            check(obs2->num_triangles == 128, "triangles match");
            check(obs2->num_vertices == 64, "vertices match");
            check(obs2->ref_count == 1, "ref count is 1");

            // Stage 5 [native-design-check]: Multiple Geometry Creation & Distinct Handle Identity
            check(!geom1->same_identity(*geom2), "geometries have distinct identity");

            // Stage 6 [source-derived]: Reference Counting (add_ref, release, auto-destruction)
            // Provenance: Xbox 0x002c30a0
            auto new_ref = geom_mgr->add_ref(*geom2, gf);
            check(bool(new_ref) && *new_ref == 2, "add_ref incremented count to 2");
            check(geom_mgr->release(*geom2, gf), "release decrements count to 1");
            check(geom_mgr->active_count() == 2, "geometry 2 still alive at ref count 1");

            check(geom_mgr->release(*geom2, gf), "release decrements count to 0 (auto-destroy)");
            check(geom_mgr->active_count() == 1, "active count drops to 1 after release");

            // Stage 7 [native-design-check]: Generational Handle Invalidation (Fail-Closed Safety)
            RetailRendererGeometryFailure ret_f;
            check(!geom_mgr->observe(*geom2, ret_f), "reject observe on retired geometry");
            check(ret_f.kind == RetailRendererGeometryError::retired_geometry, "reported retired_geometry error");

            // Stage 8 [source-derived]: Explicit Geometry Destruction
            // Provenance: Xbox 0x002c30a0, PS2 0x001a1b90
            check(geom_mgr->destroy_geometry(*geom1, gf), "destroy geometry 1");
            check(geom_mgr->active_count() == 0, "active geometry count drops to 0");
            check(!geom_mgr->observe(*geom1, ret_f), "reject observe on destroyed geometry");

            // Stage 9 [native-design-check]: Thread Affinity Checks
            std::thread([&]() {
                RetailRendererGeometryFailure thread_f;
                check(!geom_mgr->create_geometry(thread_f), "foreign thread rejected");
                check(thread_f.kind == RetailRendererGeometryError::wrong_thread, "wrong_thread error reported");
            }).join();

            // Stage 10 [source-derived]: Core Plugin Teardown & Module Count Decrement (1 -> 0)
            // Provenance: Xbox 0x002c30a0, PS2 0x001a1b90
            renderer_required::destroy_core_plugin_040b(*core);
            check(core->module_040b_count(cf).value_or(1) == 0, "040b count is 0 after destroy");
            check(!core->geometry(cf), "no geometry module after destroy");
        }

        std::puts("PASS: test_native_retail_renderer_geometry passed all 10 scoped verification stages.");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}
