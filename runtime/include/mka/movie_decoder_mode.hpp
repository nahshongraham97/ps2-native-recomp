#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>

namespace mka::video {
enum class MovieDecoderPlaneRoute { preserve_source_planes,downmix_duplicate };
enum class MovieDecoderModeError {
    none,allocation,wrong_thread,inactive_epoch,wrong_epoch,source_word_range,generation_overflow
};
struct MovieDecoderModeFailure { MovieDecoderModeError kind=MovieDecoderModeError::none; };
struct MovieDecoderModeEpochIdentity;
class NativeMovieDecoderMode;

// Immutable data captured once per SOURCE dispatch invocation. Captures remain
// owned after a later setter, module replacement or owner destruction. They do
// not claim that a host packet/frame is an original dispatch invocation.
class MovieDecoderModeCapture {
public:
    std::uint32_t source_word_bits() const noexcept { return bits_; }
    std::uint64_t write_generation() const noexcept { return generation_; }
    MovieDecoderPlaneRoute route() const noexcept {
        return bits_==0?MovieDecoderPlaneRoute::preserve_source_planes:MovieDecoderPlaneRoute::downmix_duplicate;
    }
private:
    MovieDecoderModeCapture(std::shared_ptr<const MovieDecoderModeEpochIdentity> id,
        std::uint32_t bits,std::uint64_t generation) noexcept
        :identity_(std::move(id)),bits_(bits),generation_(generation){}
    std::shared_ptr<const MovieDecoderModeEpochIdentity> identity_;
    std::uint32_t bits_;
    std::uint64_t generation_;
    friend class NativeMovieDecoderMode;
};

// Shared GAME/content lifetime state, not per-player state or a PS2 module
// loader. The native module-activation producer calls the explicit factory
// once for a validated compiled-content activation; same-role reuse keeps the
// existing owner. The initializer below is AOT source data, never heap/BSS zero
// inference. Normal setter ingress/batch/cache chronology remain separate.
class NativeMovieDecoderMode {
public:
    static std::unique_ptr<NativeMovieDecoderMode> activate_native_movie_content(MovieDecoderModeFailure&) noexcept;
    NativeMovieDecoderMode(const NativeMovieDecoderMode&)=delete;
    NativeMovieDecoderMode& operator=(const NativeMovieDecoderMode&)=delete;
    // A wider host carrier is checked before narrowing to the serialized source
    // word. Source negative words arrive as their explicit uint32 bit pattern.
    bool assign_source_word_bits(std::uint64_t,MovieDecoderModeFailure&) noexcept;
    std::optional<MovieDecoderModeCapture> capture_dispatch(MovieDecoderModeFailure&) const noexcept;
    // Old-generation captures in the SAME active epoch are valid immutable
    // observations; false means a later setter changed the current value.
    std::optional<bool> is_current(const MovieDecoderModeCapture&,MovieDecoderModeFailure&) const noexcept;
    bool retire_content(MovieDecoderModeFailure&) noexcept;
private:
    explicit NativeMovieDecoderMode(std::shared_ptr<const MovieDecoderModeEpochIdentity> id) noexcept
        :identity_(std::move(id)),owner_(std::this_thread::get_id()){}
    bool admit(MovieDecoderModeFailure&) const noexcept;
    std::shared_ptr<const MovieDecoderModeEpochIdentity> identity_;
    const std::thread::id owner_;
    // Source MOVIER image SHA3a34e383...91adb8: file57520 is00000000,
    // installed by the complete content copy. The native compiled initializer
    // retains that software value without copying/executing a guest image.
    std::uint32_t bits_=0;
    std::uint64_t generation_=0;
    bool active_=true;
};
} // namespace mka::video
