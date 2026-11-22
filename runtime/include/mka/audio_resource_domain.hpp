#pragma once
#include "mka/native_file.hpp"
#include "mka/content_volume.hpp"
#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <system_error>
#include <thread>

namespace mka::sound {
// Native resource work categories, not device channels or hardware capacities.
enum class AudioResourceWork { program, sample, backing, sample_publication };
enum class AudioResourceError { none, invalid_owner, invalid_work, released,
    wrong_thread, recursive_work_conflict, allocation, host_capacity, synchronization };
struct AudioResourceFailure {
    AudioResourceError kind = AudioResourceError::none;
    std::error_code host;
};
struct AudioResourceObservation { std::size_t depth = 0, waiting = 0; };
class AudioResourceDomain;
// Preallocated opened-file ownership. Final retirement needs no allocation and
// never waits while a movie owner can exclude the current backing worker.
struct AudioResourceBacking {
    content::NativeFile file;
    std::optional<content::NativeContentFile> content_file;
private:
    AudioResourceBacking* next_ = nullptr;
    friend class AudioResourceDomain;
    friend class AudioResourceLease;
};
// A real completed acquisition. Destruction releases this acquisition only;
// explicit release checks its admitting thread. Cleanup is safe even if an
// owning native task is destroyed elsewhere; it never reports source success.
class AudioResourceLease {
public:
    AudioResourceLease(const AudioResourceLease&) = delete;
    AudioResourceLease& operator=(const AudioResourceLease&) = delete;
    AudioResourceLease(AudioResourceLease&&) noexcept;
    AudioResourceLease& operator=(AudioResourceLease&&) noexcept;
    ~AudioResourceLease();
    bool release(AudioResourceFailure&);
    bool active() const noexcept { return bool(state_); }
private:
    struct State;
    AudioResourceLease(std::shared_ptr<State>, std::size_t, std::thread::id);
    void cleanup() noexcept;
    std::shared_ptr<State> state_;
    std::size_t work_ = 0;
    std::thread::id owner_;
    friend class AudioResourceDomain;
};
// One application sound-resource context. Associated archives and all native
// preparation/publication operations retain the SAME owner. No callback is
// invented, and queued jobs, voices and playback clocks are not suspended.
class AudioResourceDomain {
public:
    static std::shared_ptr<AudioResourceDomain> create(AudioResourceFailure&);
    std::optional<AudioResourceLease> acquire(AudioResourceWork, AudioResourceFailure&);
    // Source44C: three distinct completed barriers, then retained publication.
    // The returned lease is the actual source44D pairing, not a paused boolean.
    std::optional<AudioResourceLease> surround_movie(AudioResourceFailure&);
    std::array<AudioResourceObservation, 4> observe() const;
    void retire_backing(std::unique_ptr<AudioResourceBacking>) noexcept;
    std::size_t pending_backing_retirements() const;
private:
    explicit AudioResourceDomain(std::shared_ptr<AudioResourceLease::State> state)
        : state_(std::move(state)) {}
    std::shared_ptr<AudioResourceLease::State> state_;
};
} // namespace mka::sound
