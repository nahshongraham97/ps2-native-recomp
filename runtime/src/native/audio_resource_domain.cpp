#include "mka/audio_resource_domain.hpp"
#include <condition_variable>
#include <limits>
#include <mutex>
#include <new>
#include <utility>

namespace mka::sound {
struct AudioResourceLease::State {
    struct Work {
        std::thread::id owner;
        std::size_t depth = 0, waiting = 0;
    };
    std::array<Work, 4> work;
    AudioResourceBacking* retired_head = nullptr;
    AudioResourceBacking* retired_tail = nullptr;
    std::size_t retired_count = 0;
    mutable std::mutex mutex;
    std::condition_variable changed;
};
AudioResourceLease::AudioResourceLease(std::shared_ptr<State> state, std::size_t work,
                                     std::thread::id owner)
    : state_(std::move(state)), work_(work), owner_(owner) {}
AudioResourceLease::AudioResourceLease(AudioResourceLease&& other) noexcept
    : state_(std::move(other.state_)), work_(other.work_), owner_(other.owner_) {}
AudioResourceLease& AudioResourceLease::operator=(AudioResourceLease&& other) noexcept {
    if (this != &other) {
        cleanup(); state_ = std::move(other.state_);
        work_ = other.work_; owner_ = other.owner_;
    }
    return *this;
}
AudioResourceLease::~AudioResourceLease() { cleanup(); }
void AudioResourceLease::cleanup() noexcept {
    if (!state_) return;
    auto state = std::move(state_);
    auto expected_owner = owner_;
    for (;;) {
        AudioResourceBacking* retired = nullptr;
        {
            std::lock_guard lock(state->mutex);
            auto& work = state->work[work_];
            if (work.owner != expected_owner || !work.depth) std::terminate();
            if (!--work.depth) {
                if (work_ == static_cast<std::size_t>(AudioResourceWork::backing) && state->retired_head) {
                    retired = std::exchange(state->retired_head, nullptr);
                    state->retired_tail = nullptr; state->retired_count = 0;
                    // Final closes execute under a real backing acquisition,
                    // before another barrier can report that work complete.
                    expected_owner = std::this_thread::get_id();
                    work.owner = expected_owner; work.depth = 1;
                } else work.owner = {};
            }
        }
        if (!retired) break;
        while (retired) {
            auto* next = retired->next_;
            delete retired; retired = next;
        }
    }
    state->changed.notify_all();
}
bool AudioResourceLease::release(AudioResourceFailure& failure) {
    failure = {};
    if (!state_) { failure.kind = AudioResourceError::released; return false; }
    if (owner_ != std::this_thread::get_id()) {
        failure.kind = AudioResourceError::wrong_thread; return false;
    }
    cleanup(); return true;
}
std::shared_ptr<AudioResourceDomain> AudioResourceDomain::create(AudioResourceFailure& failure) {
    failure = {};
    try {
        return std::shared_ptr<AudioResourceDomain>(new AudioResourceDomain(
            std::make_shared<AudioResourceLease::State>()));
    } catch (const std::bad_alloc&) { failure.kind = AudioResourceError::allocation; }
    return {};
}
std::optional<AudioResourceLease> AudioResourceDomain::acquire(
    AudioResourceWork kind, AudioResourceFailure& failure) {
    failure = {};
    const auto index = static_cast<std::size_t>(kind);
    if (index >= state_->work.size()) {
        failure.kind = AudioResourceError::invalid_work; return std::nullopt;
    }
    const auto owner = std::this_thread::get_id();
    try {
        std::unique_lock lock(state_->mutex);
        auto& work = state_->work[index];
        const auto& publication = state_->work[static_cast<std::size_t>(AudioResourceWork::sample_publication)];
        if (index != static_cast<std::size_t>(AudioResourceWork::sample_publication) &&
            publication.depth && publication.owner == owner && work.depth && work.owner != owner) {
            // A publisher can be waiting for this very movie lease. Do not
            // turn recursive movie entry into a cross-category wait cycle.
            failure.kind = AudioResourceError::recursive_work_conflict; return std::nullopt;
        }
        if (work.waiting == std::numeric_limits<std::size_t>::max()) {
            failure.kind = AudioResourceError::host_capacity; return std::nullopt;
        }
        ++work.waiting;
        try {
            state_->changed.wait(lock, [&] { return !work.depth || work.owner == owner; });
        } catch (...) { --work.waiting; throw; }
        --work.waiting;
        if (work.depth == std::numeric_limits<std::size_t>::max()) {
            failure.kind = AudioResourceError::host_capacity; return std::nullopt;
        }
        work.owner = owner; ++work.depth;
        return AudioResourceLease(state_, index, owner);
    } catch (const std::system_error& error) {
        failure = {AudioResourceError::synchronization, error.code()};
    }
    return std::nullopt;
}
std::optional<AudioResourceLease> AudioResourceDomain::surround_movie(AudioResourceFailure& failure) {
    for (auto work : {AudioResourceWork::program, AudioResourceWork::sample, AudioResourceWork::backing}) {
        auto barrier = acquire(work, failure);
        if (!barrier) return std::nullopt;
    } // Each completed preliminary barrier has already been released here.
    return acquire(AudioResourceWork::sample_publication, failure);
}
std::array<AudioResourceObservation, 4> AudioResourceDomain::observe() const {
    std::lock_guard lock(state_->mutex);
    std::array<AudioResourceObservation, 4> observations;
    for (std::size_t i = 0; i < observations.size(); ++i)
        observations[i] = {state_->work[i].depth, state_->work[i].waiting};
    return observations;
}
void AudioResourceDomain::retire_backing(std::unique_ptr<AudioResourceBacking> opened) noexcept {
    if (!opened) return;
    const auto index = static_cast<std::size_t>(AudioResourceWork::backing);
    const auto owner = std::this_thread::get_id();
    std::optional<AudioResourceLease> access;
    {
        std::lock_guard lock(state_->mutex);
        auto& backing = state_->work[index];
        if (backing.depth && backing.owner != owner) {
            auto* retired = opened.release();
            if (state_->retired_tail) state_->retired_tail->next_ = retired;
            else state_->retired_head = retired;
            state_->retired_tail = retired; ++state_->retired_count;
            return;
        }
        if (backing.depth == std::numeric_limits<std::size_t>::max()) std::terminate();
        backing.owner = owner; ++backing.depth;
        access = AudioResourceLease(state_, index, owner);
    }
    // The actual NativeFile is retained intact until its serialized close.
    opened.reset();
}
std::size_t AudioResourceDomain::pending_backing_retirements() const {
    std::lock_guard lock(state_->mutex); return state_->retired_count;
}
} // namespace mka::sound
