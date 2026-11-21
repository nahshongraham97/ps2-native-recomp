#include "mka/retail_active_overlay.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
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
    check(argc == 2, "usage: test_retail_active_overlay GAMER_directory");
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
    std::puts("native retail active overlay: PASS");
}
