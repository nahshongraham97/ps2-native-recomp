#include "mka/retail_game_controls.hpp"
#include "mka/retail_main_settings.hpp"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>
#include <stdexcept>
#include <thread>

using namespace mka::retail;
namespace {
bool fail_allocation = false;
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
} // namespace

void* operator new(std::size_t size) {
    if (fail_allocation) throw std::bad_alloc();
    if (void* value = std::malloc(size ? size : 1)) return value;
    throw std::bad_alloc();
}
void operator delete(void* value) noexcept { std::free(value); }
void operator delete(void* value, std::size_t) noexcept { std::free(value); }

int main() {
    try {
        RetailGameControlsFailure controls_failure;
        RetailMainSettingsFailure settings_failure;
        fail_allocation = true;
        auto failed_controls = NativeRetailGameControls::create(controls_failure);
        fail_allocation = false;
        check(!failed_controls && controls_failure.kind == RetailGameControlsError::allocation,
            "game-control creation allocation failure");
        auto controls = NativeRetailGameControls::create(controls_failure);
        check(bool(controls) && controls_failure.kind == RetailGameControlsError::none,
            "source game-control owner creation");
        check(!NativeRetailMainSettings::create({}, settings_failure) &&
            settings_failure.kind == RetailMainSettingsError::invalid_owner,
            "settings reject missing shared game-control owner");
        fail_allocation = true;
        auto failed_settings = NativeRetailMainSettings::create(controls, settings_failure);
        fail_allocation = false;
        check(!failed_settings && settings_failure.kind == RetailMainSettingsError::allocation,
            "settings allocation failure does not publish owner");
        auto settings = NativeRetailMainSettings::create(controls, settings_failure);
        check(bool(settings) && settings_failure.kind == RetailMainSettingsError::none,
            "settings owner shares real game-control owner");
        check(controls->selection_flags(controls_failure) == 0 &&
            controls->selection_state_word(controls_failure) == 0 &&
            settings->profile_selector(settings_failure) == 13 &&
            settings->stored_frame_rate(settings_failure) == 60 &&
            settings->override_word(settings_failure) == 0 &&
            settings->timing_override_active(settings_failure) == false &&
            settings->effective_frame_rate(settings_failure) == 60,
            "original ELF and BSS initial scalar values");

        check(settings->store_profile_selector_from_1225d0(2, settings_failure) &&
            settings->profile_selector(settings_failure) == 2 &&
            settings->store_profile_selector_from_1225d0(0xffffffffu, settings_failure) &&
            settings->profile_selector(settings_failure) == 0xffffffffu,
            "transition selector store retains full source word");
        check(settings->store_video_rate_from_220460(0, settings_failure) &&
            settings->stored_frame_rate(settings_failure) == 50 &&
            settings->effective_frame_rate(settings_failure) == 50,
            "source video mode zero stores 50");
        check(settings->store_override_word_from_source(7, settings_failure) &&
            controls->store_selection_flags_from_source(2, controls_failure) &&
            settings->timing_override_active(settings_failure) == false &&
            settings->effective_frame_rate(settings_failure) == 50,
            "override word alone and other flag bits do not force 60");
        check(controls->store_selection_flags_from_source(3, controls_failure) &&
            controls->selection_state_word(controls_failure) == 3 &&
            settings->timing_override_active(settings_failure) == true &&
            settings->effective_frame_rate(settings_failure) == 60,
            "same live game-control low bit and override word force 60");
        check(settings->store_override_word_from_source(0, settings_failure) &&
            settings->timing_override_active(settings_failure) == false &&
            settings->effective_frame_rate(settings_failure) == 50,
            "temporary override word clear restores stored source rate");
        check(controls->store_selection_flags_from_source(0xff, controls_failure) &&
            controls->clear_selection_state_from_1303f0(controls_failure) &&
            controls->selection_state_word(controls_failure) == 0 &&
            settings->timing_override_active(settings_failure) == false,
            "1303F0 clears whole known 32-bit state word");
        check(settings->store_video_rate_from_220460(1, settings_failure) &&
            settings->stored_frame_rate(settings_failure) == 60 &&
            settings->store_video_rate_from_220460(2, settings_failure) &&
            settings->stored_frame_rate(settings_failure) == 60,
            "source video modes one and two store 60");
        check(!settings->store_video_rate_from_220460(3, settings_failure) &&
            settings_failure.kind == RetailMainSettingsError::invalid_video_mode &&
            settings->stored_frame_rate(settings_failure) == 60 &&
            settings_failure.kind == RetailMainSettingsError::none,
            "unknown video mode fails without mutating source rate; success clears stale error");

        bool wrong_thread = false;
        std::thread worker([&] {
            RetailGameControlsFailure other_controls;
            RetailMainSettingsFailure other_settings;
            const auto cross_created = NativeRetailMainSettings::create(controls, other_settings);
            wrong_thread = !controls->selection_state_word(other_controls) &&
                other_controls.kind == RetailGameControlsError::wrong_thread &&
                !controls->store_selection_flags_from_source(1, other_controls) &&
                other_controls.kind == RetailGameControlsError::wrong_thread &&
                !settings->effective_frame_rate(other_settings) &&
                other_settings.kind == RetailMainSettingsError::wrong_thread &&
                !settings->store_profile_selector_from_1225d0(0, other_settings) &&
                other_settings.kind == RetailMainSettingsError::wrong_thread &&
                !cross_created && other_settings.kind == RetailMainSettingsError::wrong_thread;
        });
        worker.join();
        check(wrong_thread && settings->profile_selector(settings_failure) == 0xffffffffu &&
            controls->selection_flags(controls_failure) == 0,
            "owner-thread refusal leaves source scalar state unchanged");

        std::weak_ptr<NativeRetailGameControls> retained = controls;
        controls.reset();
        check(!retained.expired(), "settings retains same game-control owner");
        auto same_controls = retained.lock();
        check(same_controls && same_controls->store_selection_flags_from_source(1, controls_failure) &&
            settings->store_override_word_from_source(1, settings_failure) &&
            settings->timing_override_active(settings_failure) == true,
            "retained owner remains the live source flag producer");
        same_controls.reset();
        settings.reset();
        check(retained.expired(), "settings retirement releases shared game-control owner");
        std::puts("PASS native main scalar settings and shared game-control flag; game and endpoints open");
        return 0;
    } catch (const std::exception& error) {
        fail_allocation = false;
        std::fprintf(stderr, "FAIL native main settings: %s\n", error.what());
        return 1;
    }
}
