#include "mka/movie_decoder_mode.hpp"
#include <limits>
#include <new>

namespace mka::video {
struct MovieDecoderModeEpochIdentity {};
namespace {
bool fail(MovieDecoderModeFailure& f,MovieDecoderModeError e) noexcept { f.kind=e;return false; }
}
std::unique_ptr<NativeMovieDecoderMode> NativeMovieDecoderMode::activate_native_movie_content(
        MovieDecoderModeFailure& f) noexcept {
    f={};
    try {
        auto identity=std::make_shared<const MovieDecoderModeEpochIdentity>();
        return std::unique_ptr<NativeMovieDecoderMode>(new NativeMovieDecoderMode(std::move(identity)));
    } catch(const std::bad_alloc&) { fail(f,MovieDecoderModeError::allocation);return {}; }
}
bool NativeMovieDecoderMode::admit(MovieDecoderModeFailure& f) const noexcept {
    f={};
    if(std::this_thread::get_id()!=owner_)return fail(f,MovieDecoderModeError::wrong_thread);
    if(!active_)return fail(f,MovieDecoderModeError::inactive_epoch);
    return true;
}
bool NativeMovieDecoderMode::assign_source_word_bits(std::uint64_t bits,MovieDecoderModeFailure& f) noexcept {
    if(!admit(f))return false;
    if(bits>std::numeric_limits<std::uint32_t>::max())return fail(f,MovieDecoderModeError::source_word_range);
    if(bits==bits_)return true;
    if(generation_==std::numeric_limits<std::uint64_t>::max())return fail(f,MovieDecoderModeError::generation_overflow);
    bits_=static_cast<std::uint32_t>(bits);++generation_;
    return true;
}
std::optional<MovieDecoderModeCapture> NativeMovieDecoderMode::capture_dispatch(MovieDecoderModeFailure& f) const noexcept {
    if(!admit(f))return {};
    return MovieDecoderModeCapture(identity_,bits_,generation_);
}
std::optional<bool> NativeMovieDecoderMode::is_current(const MovieDecoderModeCapture& capture,
        MovieDecoderModeFailure& f) const noexcept {
    if(!admit(f))return {};
    if(capture.identity_!=identity_){fail(f,MovieDecoderModeError::wrong_epoch);return {};}
    return capture.generation_==generation_;
}
bool NativeMovieDecoderMode::retire_content(MovieDecoderModeFailure& f) noexcept {
    f={};
    if(std::this_thread::get_id()!=owner_)return fail(f,MovieDecoderModeError::wrong_thread);
    active_=false;return true;
}
} // namespace mka::video
