#include "mka/retail_renderer_texture_dictionaries.hpp"
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
OTHER(0412, 0x412)
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
        f = {StartupError::memory_owners, "0409 test scene refusal"};
        return RetailStartupGate::native_failure;
    }
    return RetailStartupGate::ready;
}
bool continue_scene_reset_at_125d60(NativeRetailMainState&, StartupFailure&) { std::abort(); }
} // namespace mka::retail::scene_bootstrap_required

int main() {
    try {
        const std::filesystem::path root = ".";

        // 1. Core Registration & Lifecycle of 0409
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

            auto tex_mgr = core->textures(cf);
            check(bool(tex_mgr), "textures manager published");

            // 2. Reentrant Construction Rejection
            RetailRendererCoreFailure reentrant_f;
            check(!renderer_required::construct_core_plugin_0409(*core, reentrant_f), "reentrant construction rejected");
            check(reentrant_f.kind == RetailRendererCoreError::reentrant, "reentrant error reported");

            // 3. Dictionary Creation & Observation
            RetailRendererTextureDictionaryFailure df;
            auto dict1 = dict_mgr->create_dictionary({"arena_textures"}, df);
            check(bool(dict1) && !dict1->empty(), "create dictionary 1");
            check(dict_mgr->active_count() == 1, "active count is 1");

            auto obs1 = dict_mgr->observe(*dict1, df);
            check(bool(obs1), "observe dict 1");
            check(obs1->name == "arena_textures", "dictionary 1 name matches");
            check(obs1->texture_count == 0, "dictionary 1 initial count is 0");
            check(!obs1->is_current, "dictionary 1 initially not current");

            // 4. Current Dictionary Management
            check(dict_mgr->set_current_dictionary(*dict1, df), "set dict1 as current");
            check(dict_mgr->current_dictionary().same_identity(*dict1), "current dictionary is dict1");

            obs1 = dict_mgr->observe(*dict1, df);
            check(obs1->is_current, "dict1 is_current is true");

            check(dict_mgr->set_current_dictionary({}, df), "clear current dictionary");
            check(dict_mgr->current_dictionary().empty(), "current dictionary is cleared");

            check(dict_mgr->set_current_dictionary(*dict1, df), "restore dict1 as current");

            // 5. Texture Creation & Containment
            auto rasters = core->rasters(cf);
            check(bool(rasters), "rasters service available");
            RetailRendererRasterFailure rf;
            auto r_handle1 = rasters->create_raster({16, 16, 32, 4, 4, 0x0500}, rf);
            check(bool(r_handle1), "create raster 1");
            auto r_handle2 = rasters->create_raster({16, 16, 32, 4, 4, 0x0500}, rf);
            check(bool(r_handle2), "create raster 2");

            RetailRendererTextureFailure tf;
            RetailRendererTextureDescription tdesc1{"subzero", ""};
            auto tex1 = tex_mgr->create_texture(tdesc1, *r_handle1, tf);
            check(bool(tex1) && !tex1->empty(), "create texture 1");

            RetailRendererTextureDescription tdesc2{"scorpion", ""};
            auto tex2 = tex_mgr->create_texture(tdesc2, *r_handle2, tf);
            check(bool(tex2) && !tex2->empty(), "create texture 2");

            check(dict_mgr->add_texture(*dict1, *tex1, df), "add tex1 to dict1");
            check(dict_mgr->texture_count(*dict1, df).value_or(0) == 1, "dict1 count is 1");

            check(dict_mgr->add_texture(*dict1, *tex2, df), "add tex2 to dict1");
            check(dict_mgr->texture_count(*dict1, df).value_or(0) == 2, "dict1 count is 2");

            // 6. Duplicate & Cross-Dictionary Rejection
            RetailRendererTextureDictionaryFailure dup_f;
            check(!dict_mgr->add_texture(*dict1, *tex1, dup_f), "reject duplicate texture in same dict");
            check(dup_f.kind == RetailRendererTextureDictionaryError::texture_already_in_dictionary, "duplicate error");

            auto dict2 = dict_mgr->create_dictionary({"stage_textures"}, df);
            check(bool(dict2), "create dictionary 2");
            RetailRendererTextureDictionaryFailure cross_f;
            check(!dict_mgr->add_texture(*dict2, *tex1, cross_f), "reject cross-dictionary texture reuse");
            check(cross_f.kind == RetailRendererTextureDictionaryError::texture_already_in_dictionary, "cross-dict error");

            // 7. Case-Insensitive Lookup (by dictionary handle & current)
            auto found1 = dict_mgr->find_texture(*dict1, "SUBZERO", df);
            check(bool(found1) && found1->same_identity(*tex1), "find texture case-insensitive SUBZERO");

            auto found2 = dict_mgr->find_texture(*dict1, "ScOrPiOn", df);
            check(bool(found2) && found2->same_identity(*tex2), "find texture case-insensitive ScOrPiOn");

            auto not_found = dict_mgr->find_texture(*dict1, "raiden", df);
            check(!not_found.has_value(), "nonexistent texture returns empty");

            auto current_found = dict_mgr->find_texture_current("subzero", df);
            check(bool(current_found) && current_found->same_identity(*tex1), "find texture current subzero");

            // 8. Texture Removal
            check(dict_mgr->remove_texture(*tex1, df), "remove tex1");
            check(dict_mgr->texture_count(*dict1, df).value_or(0) == 1, "dict1 count drops to 1");
            check(!dict_mgr->find_texture(*dict1, "subzero", df).has_value(), "subzero no longer found");

            RetailRendererTextureDictionaryFailure rem_f;
            check(!dict_mgr->remove_texture(*tex1, rem_f), "reject removing already-removed texture");
            check(rem_f.kind == RetailRendererTextureDictionaryError::texture_not_found, "texture not found error");

            // 9. Dictionary Destruction & Current Auto-Clear
            auto dict3 = dict_mgr->create_dictionary({"temp_dict"}, df);
            check(dict_mgr->set_current_dictionary(*dict3, df), "set dict3 as current");
            check(dict_mgr->current_dictionary().same_identity(*dict3), "dict3 is current");

            check(dict_mgr->destroy_dictionary(*dict3, df), "destroy dict3");
            check(dict_mgr->current_dictionary().empty(), "current dictionary cleared on destruction");

            RetailRendererTextureDictionaryFailure ret_f;
            check(!dict_mgr->observe(*dict3, ret_f), "observe retired dictionary rejected");
            check(ret_f.kind == RetailRendererTextureDictionaryError::retired_dictionary, "retired error reported");

            // 10. Thread Affinity & Teardown
            std::thread([&]() {
                RetailRendererTextureDictionaryFailure thread_f;
                check(!dict_mgr->create_dictionary({"foreign"}, thread_f), "foreign thread rejected");
                check(thread_f.kind == RetailRendererTextureDictionaryError::wrong_thread, "wrong_thread error");
            }).join();

            renderer_required::destroy_core_plugin_0409(*core);
            check(core->module_0409_count(cf).value_or(1) == 0, "0409 count is 0 after destroy");
            check(!core->texture_dictionaries(cf), "no dictionaries manager after destroy");
        }

        std::puts("PASS: test_native_retail_renderer_texture_dictionaries passed all 10 verification stages.");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}
