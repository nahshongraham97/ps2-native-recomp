#include "mka/retail_sound_status.hpp"
#include "mka/retail_current_sound_required.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_sound_bank_catalog.hpp"
#include "native_retail_startup.hpp"
#include <bit>
#include <new>
#include <stdexcept>
namespace mka::retail {
namespace {
float source_unsigned_float(std::uint32_t word) noexcept {
    if(!(word&0x80000000u))return static_cast<float>(word);
    //314F30's high-bit conversion uses a sticky low bit before signed CVT.S.W.
    const auto half=(word>>1)|(word&1u);
    volatile float converted=static_cast<float>(half);
    return converted+converted;
}
float gain(std::int32_t value) noexcept {
    return source_unsigned_float(std::bit_cast<std::uint32_t>(value))/16384.0f;
}
float pan(std::int32_t value) noexcept {
    const auto adjusted=std::bit_cast<std::uint32_t>(value)+std::uint32_t{16383};
    volatile float scaled=4.0f*source_unsigned_float(adjusted);
    volatile float divided=scaled/32766.0f;
    return -2.0f+divided;
}
float pitch(std::int32_t value) noexcept {
    volatile float scaled=4.0f*source_unsigned_float(std::bit_cast<std::uint32_t>(value));
    return scaled/16384.0f;
}
}
std::array<float,6> retail_sound_status_controls(const sound::AudioControlFactors& c) noexcept {
    return {gain(c.gain_q14),pan(c.pan_q14),pitch(c.pitch_q12),
        gain(c.secondary_gain_q14),pan(c.secondary_pan_q14),pitch(c.secondary_pitch_q12)};
}
namespace current_sound_required {
bool update_programs_after_bank_results_3152c0(NativeRetailMainState& main,StartupFailure& f){
    f={};
    if(!main.lifetime||!main.lifetime->active_on_owner_thread()||
       main.current_sound_phase!=RetailCurrentSoundPhase::ready||!main.sound_bank_catalog||
       !main.sound_runtime||!main.playback||!main.current_sound||
       main.sound_runtime->playback_owner()!=main.playback||
       !main.sound_runtime->sound_context().same_identity(*main.current_sound)){
        f.kind=StartupError::sound_catalog;f.operation="3152C0 status requires the actual ready native Main context";return false;
    }
    auto status=main.sound_runtime->snapshot_status(f.sound_catalog.runtime);
    if(!status){
        f.kind=StartupError::sound_catalog;f.sound_catalog.kind=RetailSoundCatalogError::runtime;
        f.operation=f.sound_catalog.runtime.operation;return false;
    }
    try {
        auto publication=std::make_unique<NativeRetailSoundStatus>();
        publication->controls=retail_sound_status_controls(status->controls);
        publication->active_plays=std::move(status->active_plays);publication->native=status->native;
        // Actual typed delivery replaces314F30's transport/context/count gates.
        // Failed construction delivers nothing and retains the prior status.
        main.sound_frame_counter=0;
        main.sound_status=std::move(publication);
        return true;
    }catch(const std::bad_alloc&){f.kind=StartupError::allocation;f.operation="314F30 native status publication allocation";}
     catch(const std::length_error&){f.kind=StartupError::sound_catalog;f.operation="314F30 native status publication capacity";}
    return false;
}
}
}
