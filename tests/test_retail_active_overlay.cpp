#include "mka/retail_active_overlay.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>

namespace {
void check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "retail active overlay: %s\n", what);
        std::abort();
    }
}
}

int main(int argc, char** argv) {
    using namespace mka;
    check(argc == 2 || argc == 3,
          "usage: test_retail_active_overlay GAMER_directory [disposable_fixture_directory]");
    content::ContentVolumeFailure cf;
    auto volume = content::NativeContentVolume::open(
        std::filesystem::u8path(argv[1]), cf);
    check(bool(volume), "original module directory opens");

    retail::RetailActiveOverlayFailure f;
    auto overlay = retail::NativeRetailActiveOverlay::initialize_mode_one(*volume, f);
    check(bool(overlay) && f.kind == retail::RetailActiveOverlayError::none,
          "source GAMER initializer and native owner publish");
    auto state = overlay->observe(f);
    auto parent = overlay->retained_companion_parent(f);
    check(state && state->selected_mode == 1 && state->active &&
              state->companion_parent && parent &&
              parent->observe().identity == *state->companion_parent,
          "published parent has one real immutable identity");

    std::thread foreign([&] {
        retail::RetailActiveOverlayFailure foreign_failure;
        check(!overlay->retained_companion_parent(foreign_failure) &&
                  foreign_failure.kind == retail::RetailActiveOverlayError::wrong_thread,
              "foreign thread cannot retain parent");
        check(!overlay->reload_mode_one(*volume, foreign_failure) &&
                  foreign_failure.kind == retail::RetailActiveOverlayError::wrong_thread,
              "foreign thread cannot reload or mutate selection");
    });
    foreign.join();

    memory::NativeHeapFailure hf;
    auto child = memory::NativeHeapOwner::create("actual profile child", parent, hf);
    check(bool(child), "real child retains parent");
    check(!overlay->reload_mode_one(*volume, f) &&
              f.kind == retail::RetailActiveOverlayError::heap_retirement &&
              f.heap.kind == memory::NativeHeapError::owner_busy,
          "reload refuses to discard live child");
    check(overlay->retained_companion_parent(f) == parent,
          "refused reload preserves old parent");
    check(child->shutdown(hf), "child retires");
    child.reset();

    const auto prior_identity = parent->observe().identity;
    check(overlay->reload_mode_one(*volume, f),
          "same selected mode retires and recreates parent");
    const auto replacement = overlay->retained_companion_parent(f);
    check(replacement && replacement->observe().identity != prior_identity &&
              !parent->observe().active,
          "new owner replaces retired identity without replaying initializer");
    state = overlay->observe(f);
    check(state && state->selected_mode == 1 && state->active,
          "7C4980 generated selection is retained in the actual owner");

    if (argc == 3) {
        const auto fixtures = std::filesystem::u8path(argv[2]);
        const auto missing = fixtures / "missing";
        const auto changed = fixtures / "changed";
        check(!std::filesystem::exists(fixtures), "fixture destination must be fresh");
        std::filesystem::create_directories(missing);
        std::filesystem::create_directories(changed);
        auto absent_volume = content::NativeContentVolume::open(missing, cf);
        check(bool(absent_volume), "empty disposable volume opens");
        check(!retail::NativeRetailActiveOverlay::initialize_mode_one(*absent_volume, f) &&
                  f.kind == retail::RetailActiveOverlayError::missing_module,
              "missing module cannot publish an owner");
        std::filesystem::copy_file(std::filesystem::u8path(argv[1]) / "GAMER.ovl",
                                   changed / "GAMER.ovl");
        {
            std::fstream file(changed / "GAMER.ovl",
                              std::ios::binary | std::ios::in | std::ios::out);
            check(bool(file), "disposable module copy opens");
            file.seekp(0x100);
            file.put('\x02');
            check(bool(file), "plant changed initializer word in copy only");
        }
        auto changed_volume = content::NativeContentVolume::open(changed, cf);
        check(bool(changed_volume), "changed disposable volume opens");
        check(!retail::NativeRetailActiveOverlay::initialize_mode_one(*changed_volume, f) &&
                  f.kind == retail::RetailActiveOverlayError::unsupported_initializer,
              "changed original word cannot reach generated initializer");
        const auto selected_identity = replacement->observe().identity;
        check(overlay->reload_mode_one(*absent_volume, f),
              "already selected mode skips module admission and initializer");
        state = overlay->observe(f);
        check(state && state->selected_mode == 1 && state->active &&
                  *state->companion_parent != selected_identity &&
                  !replacement->observe().active,
              "same mode on empty volume preserves selection and creates real replacement heap");
    }
    std::puts("native retail active overlay: PASS");
}
