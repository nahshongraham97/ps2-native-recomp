#pragma once
#include "mka/audio_banks.hpp"
namespace mka::retail {
struct NativeRetailMainState;
struct StartupFailure;
// Required game/context effects separated from the implemented native file and
// bank queue route. No defaults exist. Context creation receives a view of the
// exact same native bank registry used by reset's real work/completion queue.
namespace current_sound_required {
bool prepare_game_sound_state_12d7e0(NativeRetailMainState&, StartupFailure&);
bool create_playback_context_316320(NativeRetailMainState&, sound::AudioBankSession&&, StartupFailure&);
bool update_programs_after_bank_results_3152c0(NativeRetailMainState&, StartupFailure&);
}
} // namespace mka::retail
