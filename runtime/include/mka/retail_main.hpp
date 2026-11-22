#pragma once
#include "mka/allocation_profile.hpp"
#include "mka/controller_input.hpp"
#include "mka/frontend_scheduler.hpp"
#include "mka/game_action_input.hpp"
#include "mka/playback_owner.hpp"
#include "mka/retail_action_history.hpp"
#include "mka/retail_file_paths.hpp"
#include "mka/retail_audio_statistics.hpp"
#include "mka/retail_main_timing.hpp"
#include "mka/retail_main_lifetime.hpp"
#include "mka/retail_viewport.hpp"
#include "mka/retail_mode_state.hpp"
#include "mka/retail_package.hpp"
#include "mka/retail_remaining_constructors.hpp"
#include "mka/native_save_store.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace mka::retail {
struct NativeRetailStartupState;
struct StartupFailure;
class NativeRetailPlatform;
class NativeRetailParticipants;
class NativeRetailInputRoles;
class NativeRetailInputAssignments;
struct NativeRetailConstructorOwners;
class NativeRetailMiddlewareWorkGroup;
class NativeRetailMiddlewareDispatcher;
class NativeRetailPrefixDispatchDescriptor;
class NativeRetailRecordOwners;
class NativeRetailGameResourceRegistry;
class NativeRetailGameInfoResources;
class NativeRetailSoundBankCatalog;
class NativeRetailSoundRuntime;
struct NativeRetailSoundStatus;
struct NativeRetailProfileOwners;
struct NativeRetailSceneOwners;

enum class RetailStartupGate { ready, source_refused, native_failure };
enum class RetailCurrentSoundPhase { empty, game_state_prepared, bank_catalog_opened, ready, failed };
enum class RetailMainTaskBody { startup_18e100 };
struct RetailMainTaskRequest {
    std::uint32_t kind, priority;
    RetailMainTaskBody body;
    std::uint32_t argument, argument_size;
};
struct RetailActionBinding {
    std::uint32_t rising_mask;
    // Compatibility identifiers name the two source records to port.
    std::uint32_t source_body_id, source_argument_id;
};
struct RetailActionTaskRequest {
    std::uint32_t role, table_index, frame;
    std::uint32_t kind, priority, argument_size;
    RetailActionBinding action;
};
struct RetailFrameRole {
    bool initialized, blocked, assigned;
};
struct RetailActionRowObservation {
    RetailActionBinding action;
    std::uint32_t rising;
};

// Original12C030/12C300 game references; native handles replace serialized
// playback/bank words. The identifier is game data, not a host address/token.
struct NativeRetailGameSoundReference {
    sound::NativePlayHandle playback;
    sound::AudioBankHandle bank;
    std::uint32_t sound_identifier{};
};

// 281E00 installs these rows as defaults. A live role can use a different
// owned table. The default table is also the source 27FB30 lookup reference.
const std::array<RetailActionBinding, 16>& retail_default_action_bindings() noexcept;
std::uint32_t retail_canonical_action_index(std::uint32_t mask, std::uint32_t row) noexcept;

struct NativeRetailMainState {
    NativeRetailMainState(NativeRetailStartupState&, input::ControllerInput&);
    ~NativeRetailMainState();
    NativeRetailMainState(const NativeRetailMainState&) = delete;
    NativeRetailMainState& operator=(const NativeRetailMainState&) = delete;
    NativeRetailMainState(NativeRetailMainState&&) = delete;
    NativeRetailMainState& operator=(NativeRetailMainState&&) = delete;
    const std::shared_ptr<NativeRetailMainLifetime> lifetime;
    NativeRetailStartupState& startup;
    NativeRetailMainTiming main_timing;
    // These owners are populated only at their original game initialization
    // points. Their existence never substitutes for completed startup.
    std::shared_ptr<NativeRetailParticipants> participants;
    std::shared_ptr<NativeRetailInputRoles> input_roles;
    std::shared_ptr<NativeRetailInputAssignments> input_assignments;
    std::shared_ptr<NativeRetailConstructorOwners> constructors;
    std::shared_ptr<NativeRetailMiddlewareWorkGroup> middleware_work_group;
    std::shared_ptr<NativeRetailMiddlewareDispatcher> middleware_dispatcher;
    std::shared_ptr<NativeRetailPrefixDispatchDescriptor> dispatch_object;
    std::shared_ptr<NativeRetailPrefixDispatchDescriptor> auxiliary_dispatch_object;
    std::shared_ptr<NativeRetailRecordOwners> record_owners;
    // Created only at the real2173D0 point of game-record initialization.
    std::unique_ptr<NativeRetailGameResourceRegistry> resource_registry;
    std::unique_ptr<NativeRetailGameInfoResources> game_info_resources;
    std::unique_ptr<NativeRetailAudioStatistics> audio_statistics;
    std::optional<NativeRetailModeState> mode_history;
    std::unique_ptr<NativeRetailPackage> package;
    std::unique_ptr<NativeRetailFilePaths> file_paths;
    // Real writable native storage, reached through 225000 before 171B10
    // changes saved-profile records. It is separate from the content volume.
    std::optional<save::NativeSaveStore> saved_profile_storage;
    std::uint32_t saved_profile_service_word{}; // source64E8D8, reset on readiness
    std::shared_ptr<NativeRetailTwoServiceObjects> two_service_objects;
    std::optional<NativeRetailEmptyBinding> empty_binding;
    std::shared_ptr<NativeRetailStaticKeyOwners> static_keys;
    RetailStaticKeyObserver static_key_observer;
    bool static_key_initialization_failed{};
    bool static_key_initialization_active{};
    std::shared_ptr<NativeRetailProfileOwners> profile;
    std::shared_ptr<NativeRetailSceneOwners> scene;
    // The startup platform owns the only controller sample store. Its lifetime
    // spans this entire main state and all role adapters.
    input::ControllerInput& controller_samples;
    std::unique_ptr<input::NativeGameActionInput> action_input;
    std::unique_ptr<NativeRetailActionHistory> action_history;
    std::unique_ptr<game::NativeFrontendScheduler> task_scheduler;
    std::shared_ptr<sound::NativePlaybackOwner> playback;
    std::optional<sound::NativeSoundContext> current_sound;
    std::unique_ptr<NativeRetailSoundRuntime> sound_runtime;
    std::unique_ptr<NativeRetailSoundStatus> sound_status;
    std::shared_ptr<NativeRetailSoundBankCatalog> sound_bank_catalog;
    RetailCurrentSoundPhase current_sound_phase{RetailCurrentSoundPhase::empty};
    std::uint32_t sound_frame_counter{};
    // size() replaces the source list count; no original16-entry capacity.
    std::vector<NativeRetailGameSoundReference> game_sound_references;
    std::uint32_t sound_auxiliary_word{}; // only the12D7E0 clear is established
    bool game_sound_state_prepared{};

    std::array<float, 3> timing_factors{};
    float milliseconds_per_frame{};
    std::optional<RetailLogicalViewport> logical_viewport;
    std::uint32_t iteration{}, ordinary_iteration{}, reentry_value{};
    std::uint32_t accumulated_input{}, previous_input{}, rising_input{};
    std::uint32_t mode_tick{}, sixty_step_intervals{}, mode_six_intervals{};
    // Persistent Main game value cleared by original 182E50 (64E29C).
    std::uint32_t post_overlay_state{};
    bool pre_loop_source_flag{};
};

// The retail task transition unwinds the active frame and resumes at its head.
// This typed transfer carries the returned value only, with no saved CPU context.
[[noreturn]] void reenter_native_retail_main(NativeRetailMainState&, std::uint32_t source_value);

// These are required direct native implementations. No successful defaults
// are provided for outstanding constructors, subsystem effects, or bodies.
namespace main_binding {
bool construct_retail_initial_aggregate(NativeRetailMainState&, StartupFailure&); // 648680
bool construct_retail_middleware_work_group(NativeRetailMainState&, StartupFailure&); // 6487D0
bool construct_retail_dispatch_object(NativeRetailMainState&, StartupFailure&); // 648810
bool construct_retail_record_owners(NativeRetailMainState&, StartupFailure&); // 648860
bool construct_retail_audio_statistics(NativeRetailMainState&, StartupFailure&); // 648930
bool construct_retail_aux_dispatch_object(NativeRetailMainState&, StartupFailure&); // 648980
bool construct_retail_two_service_objects(NativeRetailMainState&, StartupFailure&); // 6489D0
bool construct_retail_key_action_records(NativeRetailMainState&, StartupFailure&); // 648AB0
bool construct_retail_first_paired_key_records(NativeRetailMainState&, StartupFailure&); // 648C60
bool construct_retail_empty_binding(NativeRetailMainState&, StartupFailure&); // 648E50
bool construct_retail_second_paired_key_records(NativeRetailMainState&, StartupFailure&); // 648E80
bool construct_retail_first_handler_records(NativeRetailMainState&, StartupFailure&); // 6491A0
bool construct_retail_second_handler_records(NativeRetailMainState&, StartupFailure&); // 649350
bool initialize_retail_middleware_files(NativeRetailMainState&, StartupFailure&); // 2DF470(0)
// Native owner teardown: pending work is retained and reported, never discarded.
bool shutdown_retail_middleware_files(NativeRetailMainState&, StartupFailure&) noexcept;
bool initialize_retail_active_overlay(NativeRetailMainState&, StartupFailure&); // 3DDA60
bool initialize_retail_post_overlay_state(NativeRetailMainState&, StartupFailure&); // 182E50
bool clear_retail_mode_stack(NativeRetailMainState&, StartupFailure&); // 185EC0
bool enter_retail_mode_zero(NativeRetailMainState&, StartupFailure&); // 185D60(0)
bool initialize_retail_game_records(NativeRetailMainState&, StartupFailure&); // 1303F0
bool initialize_retail_loaded_sections(NativeRetailMainState&, StartupFailure&); // 19C4C0
bool reset_retail_auxiliary_words(NativeRetailMainState&, StartupFailure&); // 26F1E0, not PRNG
bool initialize_retail_action_service(NativeRetailMainState&, StartupFailure&); // 163210
bool initialize_retail_package_stage(NativeRetailMainState&, StartupFailure&); // 219050
bool initialize_retail_game_options(NativeRetailMainState&, StartupFailure&); // 164130, not audio configuration
bool initialize_retail_current_sound(NativeRetailMainState&, StartupFailure&); // 12D7E0
bool initialize_retail_mode_objects(NativeRetailMainState&, StartupFailure&); // 2460B0
bool initialize_retail_current_profile_preferences(NativeRetailMainState&, StartupFailure&); // 295660
bool initialize_retail_mode_service(NativeRetailMainState&, StartupFailure&); // 12A4F0(0)
bool initialize_retail_section_service(NativeRetailMainState&, StartupFailure&); // 19A630
// 1861B0 is full scene reset/bootstrap, including 12F030 and repeated130350.
// The historical binding name does not imply a clock-only operation.
bool initialize_retail_post_scene_timing(NativeRetailMainState&, StartupFailure&);
bool initialize_retail_post_scene_sections(NativeRetailMainState&, StartupFailure&); // 19A5E0
bool initialize_retail_first_frame_graphics(NativeRetailMainState&, StartupFailure&); // 2E9930 thunk
bool retire_retail_deferred_storage(NativeRetailMainState&, StartupFailure&); // 12D930
bool install_retail_text_allocator_pair_16fd80(NativeRetailMainState&, StartupFailure&);
bool poll_retail_controller_roles(NativeRetailMainState&, StartupFailure&); // 2214D0
bool advance_retail_game_services(NativeRetailMainState&, StartupFailure&); // 3429E0
bool dispatch_retail_enabled_mode(NativeRetailMainState&, StartupFailure&); // 27B1D0
bool service_retail_frame_input(NativeRetailMainState&, StartupFailure&); // 210570
bool dispatch_retail_task_lists(NativeRetailMainState&, StartupFailure&); // 12E4E0
bool pump_retail_middleware(NativeRetailMainState&, StartupFailure&); // 328CE0
bool pump_retail_current_sound(NativeRetailMainState&, StartupFailure&); // 3152C0
std::optional<sound::NativePlayHandle> submit_retail_queued_sound(NativeRetailMainState&,
    const sound::AudioBankHandle&,std::int32_t selector,std::uint32_t semantic_sound_id,
    const sound::NativeActivationParameters&,StartupFailure&);
//12C1D0 reverse removal for actual ready native play references. Public pending
// jobs/command50 resume require their separate producers; no queued success.
bool retire_retail_game_sound_references(NativeRetailMainState&, StartupFailure&);
bool render_retail_frame(NativeRetailMainState&, StartupFailure&); // 126940
bool prune_retail_associations(NativeRetailMainState&, StartupFailure&); // 1304F0

std::optional<std::uint32_t> read_retail_profile_selector(NativeRetailMainState&, StartupFailure&);
bool reset_and_initialize_retail_profile(NativeRetailMainState&, const NativeClassProfilePlan&, StartupFailure&);
bool start_retail_stopwatch(NativeRetailMainState&, unsigned slot, StartupFailure&);
bool initialize_retail_parsed_timing_setting(NativeRetailMainState&, StartupFailure&);
std::optional<bool> retail_timing_override_active(const NativeRetailMainState&, StartupFailure&);
std::optional<std::int32_t> read_retail_frame_rate(const NativeRetailMainState&, StartupFailure&);
RetailStartupGate initialize_retail_timer_and_viewport(NativeRetailMainState&, StartupFailure&);
std::optional<std::uint64_t> read_retail_coarse_milliseconds(NativeRetailMainState&, StartupFailure&);
RetailStartupGate initialize_retail_pad_service(NativeRetailMainState&, unsigned mode, StartupFailure&);
bool initialize_retail_saved_profile_storage(NativeRetailMainState&, StartupFailure&); // 225000 native service
// Remaining 171B10 record/default/aggregate/menu effects. No successful default.
RetailStartupGate initialize_retail_saved_profile_records(NativeRetailMainState&, StartupFailure&);
std::uint32_t read_retail_audio_boot_word(const NativeRetailMainState&);
bool initialize_retail_audio_stage(NativeRetailMainState&, const std::array<std::uint32_t, 5>&, StartupFailure&);
RetailStartupGate initialize_retail_first_scene(NativeRetailMainState&, StartupFailure&);
std::optional<std::uint32_t> stop_retail_stopwatch(NativeRetailMainState&, unsigned slot, StartupFailure&);
std::optional<std::uint16_t> read_retail_startup_seed_component(NativeRetailMainState&, StartupFailure&);
bool seed_retail_startup_random_state(NativeRetailMainState&, std::uint32_t, StartupFailure&);
// Source task allocation can return null without failing main. Return false
// only for a native integration error that prevents preserving that behavior.
bool create_retail_initial_mode_task(NativeRetailMainState&, const RetailMainTaskRequest&, StartupFailure&);
std::optional<bool> retail_frame_processing_inhibited(const NativeRetailMainState&, StartupFailure&);
std::optional<bool> retail_ordinary_counter_inhibited(const NativeRetailMainState&, StartupFailure&);
std::optional<std::int32_t> read_retail_top_mode(const NativeRetailMainState&, StartupFailure&);
// Source task lookup can retire stale records; use the live mutable task authority.
std::optional<bool> retail_task_exists(NativeRetailMainState&, std::uint32_t kind, StartupFailure&);
std::optional<bool> retail_mode_dispatch_enabled(const NativeRetailMainState&, StartupFailure&);
std::optional<RetailFrameRole> observe_retail_frame_role(const NativeRetailMainState&, std::uint32_t role, StartupFailure&);
// Fetch the live role table row and live rising bits together for this attempt.
std::optional<RetailActionRowObservation> observe_retail_action_row(const NativeRetailMainState&,
    std::uint32_t role, std::uint32_t row, StartupFailure&);
// The source skips an ordinary null task result. On actual admission this
// binding must install the original task hooks, reload the current assignment
// and, if still present, reload the live mask/argument for 162E70. The canonical
// index is derived from that reloaded mask, with the caller row as fallback.
bool create_retail_action_task(NativeRetailMainState&, const RetailActionTaskRequest&, StartupFailure&);
std::optional<bool> retail_render_enabled(const NativeRetailMainState&, StartupFailure&);
} // namespace main_binding
} // namespace mka::retail
