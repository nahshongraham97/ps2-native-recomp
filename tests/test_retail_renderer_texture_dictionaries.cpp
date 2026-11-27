#include "mka/retail_renderer_texture_dictionaries.hpp"
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
        f = {StartupError::memory_owners, "0409 test scene refusal"};
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

int main() {
    try {
        const std::filesystem::path root = ".";

        // Stage 1 [source-derived]: Core Registration & Lifecycle of 0409
        // Provenance: Xbox default.xbe 0x002c9840 / 0x002c0173, PS2 SLUS_214.10 0x001a4a80 / 0x001a67e8
        {
            Fixture fix(root);
            RetailRendererCoreFailure cf;
            auto core = NativeRetailRendererCore::for_session(fix.startup.game_session, fix.allocator, fix.startup.content, cf);
            check(bool(core), "core creation");
            check(core->initialize_core(cf).value_or(false), "initialize core");
            check(core->module_0409_count(cf).value_or(1) == 0, "0409 initial count is 0");
            check(!core->texture_dictionaries(cf), "no dictionaries manager before construct");

            later = Later::allow;
            check(core->construct_registered(cf), "construct registered up through 0409");
            check(core->module_0409_count(cf).value_or(0) == 1, "0409 constructed count is 1");

            auto dict_mgr = core->texture_dictionaries(cf);
            check(bool(dict_mgr), "dictionaries manager published");
            check(dict_mgr->active_count() == 0, "initially 0 active dictionaries");
            check(dict_mgr->current_dictionary().empty(), "initially no current dictionary");

            // Stage 2 [native-design-check]: Reentrant Construction Rejection
            RetailRendererCoreFailure reentrant_f;
            check(!renderer_required::construct_core_plugin_0409(*core, reentrant_f), "reentrant construction rejected");
            check(reentrant_f.kind == RetailRendererCoreError::reentrant, "reentrant error reported");

            // Stage 3 [source-derived]: Dictionary Creation & Observation (FreeList-backed container, 96 bytes)
            // Provenance: Xbox default.xbe 0x002c9910, PS2 SLUS_214.10 0x001a7690
            RetailRendererTextureDictionaryFailure df;
            auto dict1 = dict_mgr->create_dictionary({"arena_textures"}, df);
            check(bool(dict1) && !dict1->empty(), "create dictionary 1");
            check(dict_mgr->active_count() == 1, "active count is 1");

            auto obs1 = dict_mgr->observe(*dict1, df);
            check(bool(obs1), "observe dict 1");
            check(obs1->name == "arena_textures", "dictionary 1 name matches");
            check(!obs1->is_current, "dictionary 1 initially not current");

            // Stage 4 [source-derived]: Current Dictionary Tracking (matching 0x005c75e8 in default.xbe)
            // Provenance: Xbox default.xbe 0x002c9bf0 (RwTexDictionaryGetCurrent reading global 0x005c75e8)
            check(dict_mgr->set_current_dictionary(*dict1, df), "set dict1 as current");
            check(dict_mgr->current_dictionary().same_identity(*dict1), "current dictionary is dict1");

            obs1 = dict_mgr->observe(*dict1, df);
            check(obs1->is_current, "dict1 is_current is true");

            check(dict_mgr->set_current_dictionary({}, df), "clear current dictionary to null");
            check(dict_mgr->current_dictionary().empty(), "current dictionary is cleared");

            check(dict_mgr->set_current_dictionary(*dict1, df), "restore dict1 as current");

            // Stage 5 [source-derived]: Multiple Dictionary Creation & Circular List Tracking
            // Provenance: Xbox default.xbe 0x002c9910 (circular linked list head insertion)
            auto dict2 = dict_mgr->create_dictionary({"stage_textures"}, df);
            check(bool(dict2) && !dict2->empty(), "create dictionary 2");
            check(dict_mgr->active_count() == 2, "active count is 2");

            auto obs2 = dict_mgr->observe(*dict2, df);
            check(bool(obs2) && obs2->name == "stage_textures", "dict2 name matches");
            check(!obs2->is_current, "dict2 is not current");

            // Stage 6 [source-derived]: Dictionary Destruction & Auto-Clear of Current Dictionary
            // Provenance: Xbox default.xbe 0x002c9890 (clearing active pointer if current destroyed)
            check(dict_mgr->set_current_dictionary(*dict2, df), "set dict2 as current");
            check(dict_mgr->current_dictionary().same_identity(*dict2), "dict2 is current");

            check(dict_mgr->destroy_dictionary(*dict2, df), "destroy dict2");
            check(dict_mgr->active_count() == 1, "active count drops to 1");
            check(dict_mgr->current_dictionary().empty(), "current dictionary cleared on destruction");

            // Stage 7 [native-design-check]: Retired Dictionary Handle Rejection (Fail-Closed Rollback Safety)
            RetailRendererTextureDictionaryFailure ret_f;
            check(!dict_mgr->observe(*dict2, ret_f), "observe retired dictionary rejected");
            check(ret_f.kind == RetailRendererTextureDictionaryError::retired_dictionary, "retired error reported");

            // Stage 8 [native-design-check]: Thread Affinity Check
            std::thread([&]() {
                RetailRendererTextureDictionaryFailure thread_f;
                check(!dict_mgr->create_dictionary({"foreign"}, thread_f), "foreign thread rejected");
                check(thread_f.kind == RetailRendererTextureDictionaryError::wrong_thread, "wrong_thread error");
            }).join();

            // Stage 9 [source-derived]: Core Plugin Teardown & Module Decrement
            // Provenance: Xbox default.xbe 0x002c9860, PS2 SLUS_214.10 0x001a4ac0
            renderer_required::destroy_core_plugin_0409(*core);
            check(core->module_0409_count(cf).value_or(1) == 0, "0409 count is 0 after destroy");
            check(!core->texture_dictionaries(cf), "no dictionaries manager after destroy");
        }

        std::puts("PASS: test_native_retail_renderer_texture_dictionaries passed all 9 scoped verification stages.");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}
