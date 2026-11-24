#include "mka/retail_renderer_world.hpp"
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
OTHER(040a, 0x40a)
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
        f = {StartupError::memory_owners, "0412 test scene refusal"};
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

        // 1. Initial State: No world module, module count is 0
        {
            RetailRendererCoreFailure cf;
            auto core = NativeRetailRendererCore::for_session(
                fix.startup.game_session, fix.allocator, fix.startup.content, cf);
            check(bool(core), "construct core");

            check(!core->world_module(cf), "no world module initially");
            check(core->module_0412_count(cf).value_or(1) == 0, "0412 initial count is 0");

            // 2. Core Plugin 0412 Construction (Module Count 0 -> 1)
            check(renderer_required::construct_core_plugin_0412(*core, cf), "construct 0412");
            check(core->module_0412_count(cf).value_or(0) == 1, "0412 count is 1 after construction");
            auto world_mgr = core->world_module(cf);
            check(bool(world_mgr), "world module available");

            RetailRendererWorldFailure wf;
            check(world_mgr->active_count() == 0, "active world count initially 0");

            // 3. World 4-byte Container Creation
            auto world1 = world_mgr->create_world(wf);
            check(bool(world1) && !world1->empty(), "create world 1");
            check(world_mgr->active_count() == 1, "active world count is 1");

            // 4. Resolve Typed NativeRetailWorld and Verify Session Inheritance
            auto concrete_world = world_mgr->resolve_world(*world1, wf);
            check(bool(concrete_world), "resolve concrete world 1");
            check(concrete_world->owner_session() == fix.startup.game_session.get(), "world inherits session");

            // 5. Multiple World Creation & Distinct Handle Identity
            auto world2 = world_mgr->create_world(wf);
            check(bool(world2) && !world2->empty(), "create world 2");
            check(world_mgr->active_count() == 2, "active world count is 2");
            check(!world1->same_identity(*world2), "worlds have distinct identity");

            // 6. World Destruction & Active Count Decrement
            check(world_mgr->destroy_world(*world2, wf), "destroy world 2");
            check(world_mgr->active_count() == 1, "active world count drops to 1");

            // 7. Generational Handle Invalidation (Fail-Closed Rollback Safety)
            RetailRendererWorldFailure ret_f;
            check(!world_mgr->resolve_world(*world2, ret_f), "reject resolve retired world");
            check(ret_f.kind == RetailRendererWorldError::retired_world, "reported retired_world error");

            // 8. Thread Affinity Checks
            std::thread([&]() {
                RetailRendererWorldFailure thread_f;
                check(!world_mgr->create_world(thread_f), "foreign thread rejected");
                check(thread_f.kind == RetailRendererWorldError::wrong_thread, "wrong_thread error reported");
            }).join();

            // 9. Core Plugin Teardown & Module Count Decrement (1 -> 0)
            renderer_required::destroy_core_plugin_0412(*core);
            check(core->module_0412_count(cf).value_or(1) == 0, "0412 count is 0 after destroy");
            check(!core->world_module(cf), "no world module after destroy");
        }

        std::puts("PASS: test_native_retail_renderer_world passed all 9 scoped verification stages.");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}
