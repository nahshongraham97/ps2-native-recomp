#pragma once
#include "mka/audio_banks.hpp"
namespace mka::retail {
struct NativeRetailMainState;
struct StartupFailure;
// Game-state preparation and native context construction are implemented.
// Full source status/program effects remain strong requirements, with no
// successful defaults. Context creation receives a view of the
// exact same native bank registry used by reset's real work/completion queue.
namespace current_sound_required {
bool prepare_game_sound_state_12d7e0(NativeRetailMainState&, StartupFailure&);
bool create_playback_context_316320(NativeRetailMainState&, sound::AudioBankSession&&, StartupFailure&);
// Historical name: source3152C0 sends command99 to publish a context/status
// snapshot. Actual program advancement is the separate MSL5244 worker.
// Neither that worker nor full status publication is supplied by this declaration.
bool update_programs_after_bank_results_3152c0(NativeRetailMainState&, StartupFailure&);
}
} // namespace mka::retail
