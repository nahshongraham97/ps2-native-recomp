#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace mka::retail;
namespace {
bool fail_allocation = false;
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
struct Expected { std::string text; std::uint32_t hash; };
}
void* operator new(std::size_t n) {
    if (fail_allocation) throw std::bad_alloc();
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
int main(int argc, char** argv) { try {
    check(argc == 4, "expected disposable root, original44-label manifest and independent hash vectors");
    mka::content::ContentVolumeFailure vf;
    auto volume = mka::content::NativeContentVolume::open(std::filesystem::path(argv[1]), vf);
    check(volume.has_value(), "actual native disposable content owner");
    NativeRetailStartupState startup(std::move(*volume), {});
    mka::input::ControllerInput samples;
    NativeRetailMainState state(startup, samples);
    std::ifstream labels(argv[2]); check(bool(labels), "independent source label manifest opened");
    std::vector<Expected> expected;
    std::string line;
    while (std::getline(labels, line)) {
        const auto tab = line.find('\t'); check(tab != line.npos, "complete label manifest");
        expected.push_back({line.substr(0, tab), static_cast<std::uint32_t>(std::stoul(line.substr(tab + 1), nullptr, 16))});
    }
    check(labels.eof() && expected.size() == 44, "all original44 constructor hash calls");
    for (const auto& row : expected) check(retail_static_key_hash(row.text) == row.hash, "native hash equals independent source-label MD5-XOR");
    std::ifstream vectors(argv[3]); check(bool(vectors), "independent padding/byte hash vectors opened");
    std::size_t vector_count = 0;
    while (std::getline(vectors, line)) {
        const auto tab = line.find('\t'); check(tab != line.npos && tab % 2 == 0, "complete byte vector");
        std::string bytes;
        for (std::size_t i = 0; i < tab; i += 2) bytes.push_back(static_cast<char>(std::stoul(line.substr(i, 2), nullptr, 16)));
        const auto hash = static_cast<std::uint32_t>(std::stoul(line.substr(tab + 1), nullptr, 16));
        check(retail_static_key_hash(bytes) == hash, "explicit-byte MD5 leaf padding/embedded-NUL behavior");
        ++vector_count;
    }
    check(vectors.eof() && vector_count >= 12, "independent hash boundaries exercised");
    StartupFailure f;
    check(!main_binding::construct_retail_first_paired_key_records(state, f) && !state.static_keys,
        "later group cannot fabricate earlier constructor readiness");
    fail_allocation = true;
    bool ok = main_binding::construct_retail_two_service_objects(state, f);
    fail_allocation = false;
    check(!ok && !state.two_service_objects && f.kind == StartupError::allocation, "real service allocation failure unpublished");
    check(main_binding::construct_retail_two_service_objects(state, f) && state.two_service_objects->first_state() == 0 &&
        !state.two_service_objects->prefix_descriptor().configured(), "actual two-object source birth with unbound callback");
    check(main_binding::construct_retail_empty_binding(state, f) && state.empty_binding->state == 0 &&
        state.empty_binding->source_record == 0 && state.empty_binding->source_cursor == 0,
        "actual null-backed source branch creates three zero words");
    std::size_t calls = 0;
    state.static_key_observer = [&](std::string_view label, std::uint32_t hash) {
        check(calls < expected.size() && label == expected[calls].text && hash == expected[calls].hash,
            "observer order equals original44 call sites");
        check(state.static_keys && state.static_keys->construction_active, "actual owner retained during callback");
        if (calls == 0) check(state.static_keys->initial_keys[0] == 0, "observer precedes first destination write");
        if (calls == 1) check(state.static_keys->initial_keys[0] == expected[0].hash && state.static_keys->initial_keys[1] == 0,
            "previous destination committed before next observer");
        if (calls == 4) check(state.static_keys->first_pairs[0].group == 0 && state.static_keys->first_pairs[0].field == 0,
            "first pair unmodified before group callback");
        if (calls == 5) check(state.static_keys->first_pairs[0].group == expected[4].hash && state.static_keys->first_pairs[0].field == 0,
            "pair group stored before field callback");
        ++calls;
    };
    check(main_binding::construct_retail_key_action_records(state, f) &&
        main_binding::construct_retail_first_paired_key_records(state, f) &&
        main_binding::construct_retail_second_paired_key_records(state, f) &&
        main_binding::construct_retail_first_handler_records(state, f) &&
        main_binding::construct_retail_second_handler_records(state, f), "actual five production static key constructors");
    check(calls == 44 && state.static_keys->second_handlers_ready && !state.static_keys->construction_active &&
        !state.static_keys->construction_failed, "all original key groups published without invoking runtime handlers");
    const std::array<RetailStaticHandlerBody, 4> order{RetailStaticHandlerBody::release_other_player,
        RetailStaticHandlerBody::two_player_animation, RetailStaticHandlerBody::execute_mk_background_script,
        RetailStaticHandlerBody::execute_script_function};
    const std::array<std::size_t, 4> source_order{1,0,2,3};
    for (std::size_t i = 0; i < 4; ++i)
        check(state.static_keys->initial_keys[i] == expected[i].hash, "all four initial destination keys");
    for (std::size_t i = 0; i < 6; ++i)
        check(state.static_keys->first_pairs[i].group == expected[4 + 2*i].hash &&
            state.static_keys->first_pairs[i].field == expected[5 + 2*i].hash, "all six first group/field destinations");
    for (std::size_t i = 0; i < 10; ++i)
        check(state.static_keys->second_pairs[i].group == expected[16 + 2*i].hash &&
            state.static_keys->second_pairs[i].field == expected[17 + 2*i].hash, "all ten second group/field destinations");
    for (std::size_t i = 0; i < 4; ++i)
        check(state.static_keys->initial_handlers[i].key == expected[source_order[i]].hash &&
            state.static_keys->initial_handlers[i].body == order[i] && state.static_keys->first_handlers[i].body == order[i] &&
            state.static_keys->second_handlers[i].body == order[i] &&
            state.static_keys->first_handlers[i].key == expected[36 + source_order[i]].hash &&
            state.static_keys->second_handlers[i].key == expected[40 + source_order[i]].hash,
            "source handler keys/identities and second-first-third-fourth ordering");
    check(!main_binding::construct_retail_key_action_records(state, f) && f.kind == StartupError::already_initialized && calls == 44,
        "duplicate constructor cannot replay observer effects");
    NativeRetailMainState throwing(startup, samples);
    throwing.static_key_observer = [](std::string_view, std::uint32_t) { throw std::length_error("explicit observer capacity failure"); };
    check(!main_binding::construct_retail_key_action_records(throwing, f) && f.kind == StartupError::host_capacity &&
        throwing.static_keys->construction_failed && throwing.static_keys->initial_keys[0] == 0,
        "observer failure retains category and prevents destination write");
    check(!main_binding::construct_retail_key_action_records(throwing, f), "partially observed construction cannot replay effects");
    NativeRetailMainState detached(startup, samples);
    std::weak_ptr<NativeRetailStaticKeyOwners> old;
    detached.static_key_observer = [&](std::string_view, std::uint32_t) { old = detached.static_keys; detached.static_keys.reset(); };
    check(!main_binding::construct_retail_key_action_records(detached, f) && f.kind == StartupError::native_exception &&
        !detached.static_keys && old.expired(), "detached owner survives callback then fails by name without stale write");
    NativeRetailMainState replaced(startup, samples);
    unsigned replacement_calls = 0;
    replaced.static_key_observer = [&](std::string_view, std::uint32_t) {
        ++replacement_calls;
        replaced.static_keys = std::make_shared<NativeRetailStaticKeyOwners>();
    };
    check(!main_binding::construct_retail_key_action_records(replaced, f) && replaced.static_key_initialization_failed &&
        replacement_calls == 1, "replaced owner records irreversible partial failure in main lifetime");
    check(!main_binding::construct_retail_key_action_records(replaced, f) && replacement_calls == 1,
        "replacement cannot erase failure history and replay observer effects");
    NativeRetailMainState reentry(startup, samples);
    unsigned reentry_calls = 0;
    bool nested_accepted = false;
    reentry.static_key_observer = [&](std::string_view, std::uint32_t) {
        ++reentry_calls;
        if (reentry_calls != 1) return; // A broken guard remains a finite failing test.
        reentry.static_keys = std::make_shared<NativeRetailStaticKeyOwners>();
        StartupFailure nested;
        nested_accepted = main_binding::construct_retail_key_action_records(reentry, nested);
    };
    check(!main_binding::construct_retail_key_action_records(reentry, f) && !nested_accepted && reentry_calls == 1 &&
        reentry.static_key_initialization_failed && !reentry.static_key_initialization_active,
        "main-lifetime active guard rejects reentry across owner replacement");
    NativeRetailMainState self_clear(startup, samples);
    unsigned self_calls = 0;
    self_clear.static_key_observer = [&](std::string_view, std::uint32_t) { ++self_calls; self_clear.static_key_observer = {}; };
    check(main_binding::construct_retail_key_action_records(self_clear, f) && self_calls == 1,
        "observer lifetime survives clearing its current registration");
    std::puts("PASS native static constructors all44 source keys/observer/lifetime checks; runtime handlers/game/endpoints unexercised");
    return 0;
} catch (const std::exception& e) {
    fail_allocation = false; std::fprintf(stderr, "FAIL native static constructors: %s\n", e.what()); return 1;
}}
