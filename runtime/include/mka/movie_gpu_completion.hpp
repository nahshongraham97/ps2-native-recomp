#pragma once
#include "mka/movie_gpu.hpp"
#include <atomic>
#include <memory>

namespace mka::video {
class MovieNativeFence {
public:
    virtual ~MovieNativeFence() = default;
};
struct MovieNativeFencePlacement {
    MovieGpuError error = MovieGpuError::unavailable;
    std::shared_ptr<MovieNativeFence> fence;
};
// Actual native context operations. The WGL implementation inserts GLsyncs;
// deterministic ports are used only by endpoint-free ownership tests.
class NativeMovieFencePort {
public:
    virtual ~NativeMovieFencePort() = default;
    virtual MovieGpuError current() const noexcept = 0;
    virtual MovieNativeFencePlacement insert_after_commands() = 0;
    virtual MovieFenceResult poll(const std::shared_ptr<MovieNativeFence>&) noexcept = 0;
    // Failure retains the handle unchanged; success clears it.
    virtual MovieGpuError release(std::shared_ptr<MovieNativeFence>&) noexcept = 0;
};
class NativeMovieGpuWorkOwner;
class MovieGpuWorkReceipt {
public:
    void retire() noexcept { retired_.store(true, std::memory_order_release); }
private:
    explicit MovieGpuWorkReceipt(std::shared_ptr<const void> owner,
                                std::shared_ptr<const VideoFrameResource> source)
        : owner_(std::move(owner)), source_(std::move(source)) {}
    std::shared_ptr<const void> owner_;
    std::shared_ptr<const VideoFrameResource> source_;
    std::shared_ptr<MovieNativeFence> fence_;
    std::atomic<bool> retired_{false};
    bool submitted_ = false, safe_ = false;
    MovieGpuError submission_error_ = MovieGpuError::none;
    std::optional<MovieFenceResult> observed_;
    friend class NativeMovieGpuWorkOwner;
};
class MovieGpuDrainReceipt {
private:
    explicit MovieGpuDrainReceipt(std::shared_ptr<const void> owner) : owner_(std::move(owner)) {}
    std::shared_ptr<const void> owner_;
    std::shared_ptr<MovieNativeFence> fence_;
    MovieFenceResult result_{MovieFenceState::pending, MovieGpuError::none};
    bool tail_complete_ = false;
    friend class NativeMovieGpuWorkOwner;
};
struct MovieGpuDrain {
    MovieFenceResult result{MovieFenceState::failed, MovieGpuError::unavailable};
    std::shared_ptr<MovieGpuDrainReceipt> receipt;
};
struct MovieGpuWorkAdmission {
    MovieGpuError error = MovieGpuError::unavailable;
    std::shared_ptr<MovieGpuWorkReceipt> receipt;
};
// Owns native command completion, not video scheduling or a console device.
// The host must retain it through actual completion. No fixed job capacity.
class NativeMovieGpuWorkOwner {
public:
    static std::shared_ptr<NativeMovieGpuWorkOwner> create(std::shared_ptr<NativeMovieFencePort>, MovieGpuError&);
    ~NativeMovieGpuWorkOwner();
    MovieGpuError admission() const noexcept;
    MovieGpuError commands_issued() noexcept;
    MovieGpuWorkAdmission admit(std::shared_ptr<const VideoFrameResource>) noexcept;
    MovieGpuError finish_submission(const std::shared_ptr<MovieGpuWorkReceipt>&, MovieGpuError) noexcept;
    MovieFenceResult poll(const std::shared_ptr<MovieGpuWorkReceipt>&) noexcept;
    MovieGpuError collect() noexcept;
    MovieGpuDrain begin_drain() noexcept;
    MovieGpuDrain retry_drain(const std::shared_ptr<MovieGpuDrainReceipt>&) noexcept;
    MovieFenceResult poll_drain(const std::shared_ptr<MovieGpuDrainReceipt>&) noexcept;
    MovieGpuError resume(const std::shared_ptr<MovieGpuDrainReceipt>&) noexcept;
    bool drained() const noexcept;
private:
    struct State;
    explicit NativeMovieGpuWorkOwner(std::unique_ptr<State>);
    std::unique_ptr<State> state_;
    MovieGpuError access() const noexcept;
    MovieGpuError observe(MovieGpuWorkReceipt&) noexcept;
};
class NativeMovieGpuRetirementTask {
public:
    virtual ~NativeMovieGpuRetirementTask() = default;
    virtual MovieFenceResult poll_retirement() noexcept = 0;
    virtual MovieFenceResult retry_retirement() noexcept = 0;
};
class MovieGpuRetirementReceipt {
public:
    void retire() noexcept { retired_.store(true, std::memory_order_release); }
private:
    explicit MovieGpuRetirementReceipt(std::shared_ptr<NativeMovieGpuRetirementTask> task)
        : task_(std::move(task)) {}
    std::shared_ptr<NativeMovieGpuRetirementTask> task_;
    std::atomic<bool> retired_{false};
    std::optional<MovieFenceResult> failure_;
    friend class NativeMovieGpuRetirementOwner;
};
// Shared production backend marker. Destruction only marks preallocated work;
// the actual host owner remains responsible for polling native completion.
class MovieGpuRetirementRegistration {
public:
    MovieGpuRetirementRegistration() = default;
    explicit MovieGpuRetirementRegistration(std::shared_ptr<MovieGpuRetirementReceipt>) noexcept;
    MovieGpuRetirementRegistration(const MovieGpuRetirementRegistration&) = delete;
    MovieGpuRetirementRegistration& operator=(const MovieGpuRetirementRegistration&) = delete;
    MovieGpuRetirementRegistration(MovieGpuRetirementRegistration&&) noexcept;
    MovieGpuRetirementRegistration& operator=(MovieGpuRetirementRegistration&&) noexcept;
    ~MovieGpuRetirementRegistration();
    void retire() noexcept;
    explicit operator bool() const noexcept { return bool(receipt_); }
private:
    std::shared_ptr<MovieGpuRetirementReceipt> receipt_;
};
// Explicit host owner. Register before issuing GPU work; destruction of a
// backend only marks its preallocated receipt. Host polling performs the real
// current-context cleanup. This owner must be drained before host teardown.
class NativeMovieGpuRetirementOwner {
public:
    static std::shared_ptr<NativeMovieGpuRetirementOwner> create(MovieGpuError&);
    ~NativeMovieGpuRetirementOwner();
    std::shared_ptr<MovieGpuRetirementReceipt> register_task(std::shared_ptr<NativeMovieGpuRetirementTask>, MovieGpuError&);
    MovieFenceResult poll() noexcept;
    MovieFenceResult retry_failed() noexcept;
    bool empty() const noexcept;
private:
    struct State;
    explicit NativeMovieGpuRetirementOwner(std::unique_ptr<State>);
    std::unique_ptr<State> state_;
};
} // namespace mka::video
