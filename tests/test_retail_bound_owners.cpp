#include "mka/retail_main.hpp"
#include "mka/retail_dispatch_owners.hpp"
#include "mka/retail_record_owners.hpp"
#include "native_retail_startup.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <new>
#include <stdexcept>
using namespace mka::retail;
namespace {
bool fail_allocation = false;
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
}
void* operator new(std::size_t n) {
    if (fail_allocation) throw std::bad_alloc();
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
int main(int argc, char** argv) { try {
    check(argc == 2, "fixture needs its disposable content directory");
    mka::content::ContentVolumeFailure volume_failure;
    auto volume = mka::content::NativeContentVolume::open(std::filesystem::path(argv[1]), volume_failure);
    check(volume.has_value(), "actual native content root opened");
    NativeRetailStartupState startup(std::move(*volume), {});
    mka::input::ControllerInput samples;
    NativeRetailMainState state(startup, samples);
    check(&state.controller_samples == &samples && !state.mode_history && !state.audio_statistics,
        "main retains same real native sample store, does not manufacture readiness");
    StartupFailure f;
    check(!main_binding::read_retail_top_mode(state, f) && f.kind == StartupError::native_exception,
        "uninitialized mode reader fails by name");
    check(!main_binding::enter_retail_mode_zero(state, f) && f.kind == StartupError::native_exception,
        "missing ordered reset cannot be replaced by implicit source success");
    check(main_binding::clear_retail_mode_stack(state, f) && state.mode_history, "original reset creates actual history owner");
    fail_allocation = true;
    const bool entered = main_binding::enter_retail_mode_zero(state, f);
    fail_allocation = false;
    check(!entered && f.kind == StartupError::allocation && !state.mode_history->contains(0),
        "production mode wrapper retains actual allocation failure");
    check(main_binding::enter_retail_mode_zero(state, f) && main_binding::read_retail_top_mode(state, f) == 0 &&
        state.mode_history->contains(0), "actual native zero entry and actual bound top read");
    RetailModeFailure mode_failure;
    check(state.mode_history->push(6, mode_failure) && main_binding::read_retail_top_mode(state, f) == 6,
        "1830A0 read uses live history top");
    check(main_binding::clear_retail_mode_stack(state, f) && main_binding::read_retail_top_mode(state, f) == 0 &&
        !state.mode_history->contains(6), "later source reset clears same main-owned history");
    fail_allocation = true;
    const bool audio = main_binding::construct_retail_audio_statistics(state, f);
    fail_allocation = false;
    check(!audio && !state.audio_statistics && f.kind == StartupError::allocation,
        "actual audio constructor wrapper does not publish failed owner");
    check(main_binding::construct_retail_audio_statistics(state, f) && state.audio_statistics,
        "actual original audio-statistics constructor bound");
    const auto* exact_audio = state.audio_statistics.get();
    check(!main_binding::construct_retail_audio_statistics(state, f) && f.kind == StartupError::already_initialized &&
        state.audio_statistics.get() == exact_audio, "duplicate audio owner cannot reset actual statistics");
    check(main_binding::construct_retail_dispatch_object(state, f) &&
        main_binding::construct_retail_record_owners(state, f) &&
        main_binding::construct_retail_aux_dispatch_object(state, f), "real production constructor wrappers publish native owners");
    check(state.dispatch_object != state.auxiliary_dispatch_object &&
        !state.dispatch_object->configured() && !state.auxiliary_dispatch_object->configured(),
        "distinct prefixes are not falsely registered message handlers");
    const auto* exact_records = state.record_owners.get();
    check(!main_binding::construct_retail_record_owners(state, f) && f.kind == StartupError::already_initialized &&
        state.record_owners.get() == exact_records, "duplicate request owner cannot replace live identity");
    std::puts("PASS actual native main constructor/mode bindings and allocation failures; no game or endpoint");
    return 0;
} catch (const std::exception& e) {
    fail_allocation = false; std::fprintf(stderr, "FAIL bound owners: %s\n", e.what()); return 1;
}}
