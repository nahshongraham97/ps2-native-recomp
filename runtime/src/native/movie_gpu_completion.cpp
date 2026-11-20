#include "mka/movie_gpu_completion.hpp"
#include <cstdio>
#include <new>
#include <stdexcept>
#include <thread>
#include <vector>

namespace mka::video {
namespace {
MovieFenceResult failed(MovieGpuError error) { return {MovieFenceState::failed, error}; }
MovieFenceResult checked(MovieFenceResult result) {
    if ((result.state == MovieFenceState::pending || result.state == MovieFenceState::complete) && result.error == MovieGpuError::none)
        return result;
    if (result.state == MovieFenceState::failed && result.error != MovieGpuError::none) return result;
    return failed(MovieGpuError::fence_failed);
}
}
struct NativeMovieGpuWorkOwner::State {
    explicit State(std::shared_ptr<NativeMovieFencePort> p) : port(std::move(p)) {}
    std::shared_ptr<NativeMovieFencePort> port;
    const std::shared_ptr<const void> cookie = std::make_shared<const int>(0);
    const std::thread::id thread = std::this_thread::get_id();
    std::vector<std::shared_ptr<MovieGpuWorkReceipt>> work;
    std::shared_ptr<MovieGpuDrainReceipt> drain;
    std::vector<std::shared_ptr<MovieNativeFence>> prior_tails;
    bool dirty = false;
};
NativeMovieGpuWorkOwner::NativeMovieGpuWorkOwner(std::unique_ptr<State> state) : state_(std::move(state)) {}
std::shared_ptr<NativeMovieGpuWorkOwner> NativeMovieGpuWorkOwner::create(
    std::shared_ptr<NativeMovieFencePort> port, MovieGpuError& error) {
    error = MovieGpuError::unavailable;
    if (!port) return {};
    if ((error = port->current()) != MovieGpuError::none) return {};
    try { return std::shared_ptr<NativeMovieGpuWorkOwner>(new NativeMovieGpuWorkOwner(std::make_unique<State>(std::move(port)))); }
    catch (const std::bad_alloc&) { error = MovieGpuError::host_capacity; }
    catch (const std::length_error&) { error = MovieGpuError::host_capacity; }
    return {};
}
NativeMovieGpuWorkOwner::~NativeMovieGpuWorkOwner() {
    if (state_->dirty || (state_->drain && state_->drain->result_.state != MovieFenceState::complete)) {
        std::fputs("[native-movie-gpu] completion owner released before verified native drain\n", stderr);
        std::terminate();
    }
}
MovieGpuError NativeMovieGpuWorkOwner::access() const noexcept {
    return std::this_thread::get_id() == state_->thread ? state_->port->current() : MovieGpuError::wrong_thread;
}
MovieGpuError NativeMovieGpuWorkOwner::admission() const noexcept {
    if (auto error = access(); error != MovieGpuError::none) return error;
    return state_->drain ? MovieGpuError::admission_closed : MovieGpuError::none;
}
MovieGpuError NativeMovieGpuWorkOwner::commands_issued() noexcept {
    if (auto error = admission(); error != MovieGpuError::none) return error;
    state_->dirty = true; return MovieGpuError::none;
}
MovieGpuWorkAdmission NativeMovieGpuWorkOwner::admit(std::shared_ptr<const VideoFrameResource> source) noexcept {
    if (auto error = admission(); error != MovieGpuError::none) return {error, {}};
    if (!source) return {MovieGpuError::invalid_frame, {}};
    try {
        auto work = std::shared_ptr<MovieGpuWorkReceipt>(new MovieGpuWorkReceipt(state_->cookie, std::move(source)));
        state_->work.push_back(work); return {MovieGpuError::none, std::move(work)};
    } catch (const std::bad_alloc&) { return {MovieGpuError::host_capacity, {}}; }
      catch (const std::length_error&) { return {MovieGpuError::host_capacity, {}}; }
}
MovieGpuError NativeMovieGpuWorkOwner::finish_submission(
    const std::shared_ptr<MovieGpuWorkReceipt>& work, MovieGpuError error) noexcept {
    if (auto current = access(); current != MovieGpuError::none) return current;
    if (!work || work->owner_ != state_->cookie || work->submitted_) return MovieGpuError::stale_resource;
    state_->dirty = true; work->submitted_ = true; work->submission_error_ = error;
    try {
        auto placed = state_->port->insert_after_commands(); work->fence_ = std::move(placed.fence);
        if (placed.error != MovieGpuError::none || !work->fence_)
            work->submission_error_ = error != MovieGpuError::none ? error :
                placed.error != MovieGpuError::none ? placed.error : MovieGpuError::fence_failed;
    } catch (...) { work->submission_error_ = error != MovieGpuError::none ? error : MovieGpuError::host_capacity; }
    return work->submission_error_;
}
MovieGpuError NativeMovieGpuWorkOwner::observe(MovieGpuWorkReceipt& work) noexcept {
    if (!work.submitted_) return MovieGpuError::invalid_plan;
    if (!work.safe_ && work.fence_) {
        auto status = checked(state_->port->poll(work.fence_));
        if (status.state == MovieFenceState::complete) work.safe_ = true;
        if (status.state == MovieFenceState::failed && !work.observed_) work.observed_ = status;
    }
    if (work.safe_) {
        if (work.fence_) {
            auto error = state_->port->release(work.fence_);
            if (error != MovieGpuError::none) return error;
            if (work.fence_) return MovieGpuError::fence_failed;
        }
        work.source_.reset();
        if (!work.observed_) work.observed_ = work.submission_error_ == MovieGpuError::none ?
            MovieFenceResult{MovieFenceState::complete, MovieGpuError::none} : failed(work.submission_error_);
    }
    return MovieGpuError::none;
}
MovieFenceResult NativeMovieGpuWorkOwner::poll(const std::shared_ptr<MovieGpuWorkReceipt>& work) noexcept {
    if (auto error = access(); error != MovieGpuError::none) return failed(error);
    if (!work || work->owner_ != state_->cookie) return failed(MovieGpuError::stale_resource);
    if (!work->submitted_) return failed(MovieGpuError::invalid_plan);
    const auto cleanup = observe(*work);
    if (work->submission_error_ != MovieGpuError::none) return failed(work->submission_error_);
    if (cleanup != MovieGpuError::none) {
        if (!work->observed_) work->observed_ = failed(cleanup);
        return *work->observed_;
    }
    return work->observed_.value_or(MovieFenceResult{MovieFenceState::pending, MovieGpuError::none});
}
MovieGpuError NativeMovieGpuWorkOwner::collect() noexcept {
    if (auto error = access(); error != MovieGpuError::none) return error;
    for (auto i = state_->work.begin(); i != state_->work.end();) {
        auto& work = **i;
        if (auto error = observe(work); error != MovieGpuError::none) return error;
        if (work.safe_ && !work.fence_ && work.retired_.load(std::memory_order_acquire)) i = state_->work.erase(i);
        else ++i;
    }
    return MovieGpuError::none;
}
MovieGpuDrain NativeMovieGpuWorkOwner::begin_drain() noexcept {
    if (auto error = access(); error != MovieGpuError::none) return {failed(error), {}};
    if (state_->drain) return {state_->drain->result_, state_->drain};
    for (const auto& work : state_->work) if (!work->submitted_) return {failed(MovieGpuError::invalid_plan), {}};
    try {
        auto receipt = std::shared_ptr<MovieGpuDrainReceipt>(new MovieGpuDrainReceipt(state_->cookie));
        state_->drain = receipt; // Seal before calling the actual context port.
        auto tail = state_->port->insert_after_commands(); receipt->fence_ = std::move(tail.fence);
        if (tail.error != MovieGpuError::none || !receipt->fence_)
            receipt->result_ = failed(tail.error != MovieGpuError::none ? tail.error : MovieGpuError::fence_failed);
        return {receipt->result_, std::move(receipt)};
    } catch (...) {
        if (state_->drain) state_->drain->result_ = failed(MovieGpuError::host_capacity);
        return {failed(MovieGpuError::host_capacity), state_->drain};
    }
}
MovieGpuDrain NativeMovieGpuWorkOwner::retry_drain(const std::shared_ptr<MovieGpuDrainReceipt>& receipt) noexcept {
    if (auto error = access(); error != MovieGpuError::none) return {failed(error), {}};
    if (!receipt || receipt != state_->drain || receipt->owner_ != state_->cookie || receipt->result_.state != MovieFenceState::failed)
        return {failed(MovieGpuError::stale_resource), {}};
    std::shared_ptr<MovieGpuDrainReceipt> replacement;
    try {
        replacement = std::shared_ptr<MovieGpuDrainReceipt>(new MovieGpuDrainReceipt(state_->cookie));
        if (receipt->fence_) state_->prior_tails.push_back(receipt->fence_);
    } catch (...) { return {failed(MovieGpuError::host_capacity), receipt}; }
    receipt->fence_.reset(); state_->drain = replacement; // Never reopen the failed seal.
    try {
        auto placed = state_->port->insert_after_commands(); replacement->fence_ = std::move(placed.fence);
        if (placed.error != MovieGpuError::none || !replacement->fence_)
            replacement->result_ = failed(placed.error != MovieGpuError::none ? placed.error : MovieGpuError::fence_failed);
    } catch (...) { replacement->result_ = failed(MovieGpuError::host_capacity); }
    return {replacement->result_, std::move(replacement)};
}
MovieFenceResult NativeMovieGpuWorkOwner::poll_drain(const std::shared_ptr<MovieGpuDrainReceipt>& receipt) noexcept {
    if (auto error = access(); error != MovieGpuError::none) return failed(error);
    if (!receipt || receipt != state_->drain || receipt->owner_ != state_->cookie) return failed(MovieGpuError::stale_resource);
    if (receipt->result_.state == MovieFenceState::failed) return receipt->result_;
    if (!receipt->tail_complete_) {
        auto result = checked(state_->port->poll(receipt->fence_));
        if (result.state != MovieFenceState::complete) { receipt->result_ = result; return result; }
        // The same native context tail orders all earlier issued commands,
        // including uploads and a failed submission with no individual fence.
        receipt->tail_complete_ = true;
        for (auto& work : state_->work) work->safe_ = true;
    }
    if (auto error = collect(); error != MovieGpuError::none) return failed(error);
    for (auto& tail : state_->prior_tails) {
        if (!tail) continue;
        if (auto error = state_->port->release(tail); error != MovieGpuError::none) return failed(error);
        if (tail) return failed(MovieGpuError::fence_failed);
    }
    state_->prior_tails.clear();
    if (receipt->fence_) {
        if (auto error = state_->port->release(receipt->fence_); error != MovieGpuError::none) return failed(error);
        if (receipt->fence_) return failed(MovieGpuError::fence_failed);
    }
    state_->dirty = false; receipt->result_ = {MovieFenceState::complete, MovieGpuError::none};
    return receipt->result_;
}
MovieGpuError NativeMovieGpuWorkOwner::resume(const std::shared_ptr<MovieGpuDrainReceipt>& receipt) noexcept {
    if (auto error = access(); error != MovieGpuError::none) return error;
    if (!receipt || receipt != state_->drain || receipt->owner_ != state_->cookie || receipt->result_.state != MovieFenceState::complete)
        return MovieGpuError::stale_resource;
    state_->drain.reset(); return MovieGpuError::none;
}
bool NativeMovieGpuWorkOwner::drained() const noexcept {
    return state_->drain && state_->drain->result_.state == MovieFenceState::complete && !state_->dirty;
}
MovieGpuRetirementRegistration::MovieGpuRetirementRegistration(std::shared_ptr<MovieGpuRetirementReceipt> receipt) noexcept
    : receipt_(std::move(receipt)) {}
MovieGpuRetirementRegistration::MovieGpuRetirementRegistration(MovieGpuRetirementRegistration&& other) noexcept
    : receipt_(std::move(other.receipt_)) {}
MovieGpuRetirementRegistration& MovieGpuRetirementRegistration::operator=(MovieGpuRetirementRegistration&& other) noexcept {
    if (this != &other) { retire(); receipt_ = std::move(other.receipt_); } return *this;
}
MovieGpuRetirementRegistration::~MovieGpuRetirementRegistration() { retire(); }
void MovieGpuRetirementRegistration::retire() noexcept { if (receipt_) receipt_->retire(); }
struct NativeMovieGpuRetirementOwner::State {
    const std::thread::id thread = std::this_thread::get_id();
    std::vector<std::shared_ptr<MovieGpuRetirementReceipt>> entries;
};
NativeMovieGpuRetirementOwner::NativeMovieGpuRetirementOwner(std::unique_ptr<State> state) : state_(std::move(state)) {}
std::shared_ptr<NativeMovieGpuRetirementOwner> NativeMovieGpuRetirementOwner::create(MovieGpuError& error) {
    error = MovieGpuError::none;
    try { return std::shared_ptr<NativeMovieGpuRetirementOwner>(new NativeMovieGpuRetirementOwner(std::make_unique<State>())); }
    catch (...) { error = MovieGpuError::host_capacity; return {}; }
}
NativeMovieGpuRetirementOwner::~NativeMovieGpuRetirementOwner() {
    if (!state_->entries.empty()) {
        std::fputs("[native-movie-gpu] host retirement owner released with retained sessions\n", stderr); std::terminate();
    }
}
std::shared_ptr<MovieGpuRetirementReceipt> NativeMovieGpuRetirementOwner::register_task(
    std::shared_ptr<NativeMovieGpuRetirementTask> task, MovieGpuError& error) {
    error = MovieGpuError::none;
    if (std::this_thread::get_id() != state_->thread) { error = MovieGpuError::wrong_thread; return {}; }
    if (!task) { error = MovieGpuError::unavailable; return {}; }
    try {
        auto entry = std::shared_ptr<MovieGpuRetirementReceipt>(new MovieGpuRetirementReceipt(std::move(task)));
        state_->entries.push_back(entry); return entry;
    } catch (...) { error = MovieGpuError::host_capacity; return {}; }
}
MovieFenceResult NativeMovieGpuRetirementOwner::poll() noexcept {
    if (std::this_thread::get_id() != state_->thread) return failed(MovieGpuError::wrong_thread);
    bool pending = false;
    for (auto i = state_->entries.begin(); i != state_->entries.end();) {
        if (!(*i)->retired_.load(std::memory_order_acquire)) { pending = true; ++i; continue; }
        if ((*i)->failure_) return *(*i)->failure_;
        auto result = checked((*i)->task_->poll_retirement());
        if (result.state == MovieFenceState::failed) { (*i)->failure_ = result; return result; }
        if (result.state == MovieFenceState::complete) i = state_->entries.erase(i);
        else { pending = true; ++i; }
    }
    return {pending ? MovieFenceState::pending : MovieFenceState::complete, MovieGpuError::none};
}
MovieFenceResult NativeMovieGpuRetirementOwner::retry_failed() noexcept {
    if (std::this_thread::get_id() != state_->thread) return failed(MovieGpuError::wrong_thread);
    for (auto i = state_->entries.begin(); i != state_->entries.end();) {
        if (!(*i)->failure_) { ++i; continue; }
        auto result = checked((*i)->task_->retry_retirement());
        if (result.state == MovieFenceState::failed) { (*i)->failure_ = result; return result; }
        (*i)->failure_.reset();
        if (result.state == MovieFenceState::complete) i = state_->entries.erase(i);
        else ++i;
    }
    return poll();
}
bool NativeMovieGpuRetirementOwner::empty() const noexcept { return state_->entries.empty(); }
} // namespace mka::video
