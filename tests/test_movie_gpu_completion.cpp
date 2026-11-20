#include "mka/movie_gpu_completion.hpp"
#include <cstdlib>
#include <cstdio>
#include <new>
#include <thread>
#include <vector>
using namespace mka::video;
thread_local bool gpu_test_fail_next_allocation = false;
// Fixture-only allocation injection. No production runtime flag or allocator.
void* operator new(std::size_t size) {
    if (gpu_test_fail_next_allocation) { gpu_test_fail_next_allocation = false; throw std::bad_alloc(); }
    if (void* pointer = std::malloc(size ? size : 1)) return pointer;
    throw std::bad_alloc();
}
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
namespace {
void check(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
struct Frame final : VideoFrameResource {
    std::uint64_t identity() const noexcept override { return 1; }
    std::uint32_t width() const noexcept override { return 2053; }
    std::uint32_t height() const noexcept override { return 769; }
};
struct Fence final : MovieNativeFence { explicit Fence(std::size_t value) : index(value) {} std::size_t index; };
struct Port final : NativeMovieFencePort {
    std::vector<MovieFenceResult> states;
    std::vector<unsigned> polls, releases;
    MovieGpuError current_error = MovieGpuError::none;
    MovieGpuError next_error = MovieGpuError::none;
    bool null_next = false;
    std::optional<std::size_t> fail_release;
    MovieGpuError current() const noexcept override { return current_error; }
    MovieNativeFencePlacement insert_after_commands() override {
        auto error = next_error; next_error = MovieGpuError::none;
        if (null_next) { null_next = false; return {error, {}}; }
        auto fence = std::make_shared<Fence>(states.size());
        states.push_back({MovieFenceState::pending, MovieGpuError::none});
        polls.push_back(0); releases.push_back(0); return {error, std::move(fence)};
    }
    MovieFenceResult poll(const std::shared_ptr<MovieNativeFence>& value) noexcept override {
        auto fence = std::dynamic_pointer_cast<Fence>(value);
        if (!fence || fence->index >= states.size()) return {MovieFenceState::failed, MovieGpuError::stale_resource};
        ++polls[fence->index]; return states[fence->index];
    }
    MovieGpuError release(std::shared_ptr<MovieNativeFence>& value) noexcept override {
        auto fence = std::dynamic_pointer_cast<Fence>(value);
        if (!fence) return MovieGpuError::stale_resource;
        if (fail_release == fence->index) { fail_release.reset(); return MovieGpuError::fence_failed; }
        ++releases[fence->index]; value.reset(); return MovieGpuError::none;
    }
    void complete(std::size_t index) { states[index] = {MovieFenceState::complete, MovieGpuError::none}; }
};
struct Rig {
    std::shared_ptr<Port> port = std::make_shared<Port>();
    MovieGpuError error;
    std::shared_ptr<NativeMovieGpuWorkOwner> owner = NativeMovieGpuWorkOwner::create(port, error);
    Rig() { check(owner && error == MovieGpuError::none, "mandatory native fence port"); }
    MovieGpuDrain drain() { auto value = owner->begin_drain(); check(bool(value.receipt), "actual tail receipt"); return value; }
    void finish(MovieGpuDrain value) { port->complete(port->states.size() - 1); check(owner->poll_drain(value.receipt).state == MovieFenceState::complete, "actual tail completes"); }
};
struct RetiredSession final : NativeMovieGpuRetirementTask {
    explicit RetiredSession(std::shared_ptr<NativeMovieGpuWorkOwner> value) : owner(std::move(value)) {}
    std::shared_ptr<NativeMovieGpuWorkOwner> owner;
    unsigned calls = 0, retries = 0;
    MovieFenceResult poll_retirement() noexcept override {
        ++calls; auto tail = owner->begin_drain();
        return tail.result.state == MovieFenceState::failed ? tail.result : owner->poll_drain(tail.receipt);
    }
    MovieFenceResult retry_retirement() noexcept override {
        ++retries; auto tail = owner->begin_drain();
        if (tail.result.state == MovieFenceState::failed) tail = owner->retry_drain(tail.receipt);
        return tail.result.state == MovieFenceState::failed ? tail.result : owner->poll_drain(tail.receipt);
    }
};
}
int main() {
    { MovieGpuError error; check(!NativeMovieGpuWorkOwner::create({}, error) && error == MovieGpuError::unavailable, "no successful default fence port"); }
    {
        Rig r; auto source = std::make_shared<Frame>(); std::weak_ptr<Frame> weak = source;
        auto job = r.owner->admit(source); source.reset();
        check(job.receipt && r.owner->finish_submission(job.receipt, MovieGpuError::none) == MovieGpuError::none, "actual admitted submission");
        job.receipt->retire(); check(r.owner->collect() == MovieGpuError::none && !weak.expired(), "retired is not completed");
        auto tail = r.drain(); const auto inserts = r.port->states.size();
        check(r.owner->begin_drain().receipt == tail.receipt && r.port->states.size() == inserts, "same pending drain never inserts duplicate tail");
        check(r.owner->admission() == MovieGpuError::admission_closed && !r.owner->admit(std::make_shared<Frame>()).receipt,
            "sealed device rejects actual new admission");
        check(r.owner->poll(job.receipt).state == MovieFenceState::pending && r.owner->resume(tail.receipt) == MovieGpuError::stale_resource,
            "active completion stays pollable and pending tail cannot resume");
        MovieFenceResult foreign; std::thread thread([&] { foreign = r.owner->poll_drain(tail.receipt); }); thread.join();
        check(foreign.error == MovieGpuError::wrong_thread && !weak.expired(), "foreign poll does not release native ownership");
        r.finish(tail); check(weak.expired() && r.owner->poll(job.receipt).state == MovieFenceState::complete,
            "tail resource safety retains terminal consumer receipt after collection");
        check(r.owner->poll_drain(tail.receipt).state == MovieFenceState::complete && r.owner->resume(tail.receipt) == MovieGpuError::none,
            "completed exact receipt resumes once");
        check(r.owner->poll_drain(tail.receipt).error == MovieGpuError::stale_resource && r.owner->admission() == MovieGpuError::none,
            "stale drain cannot affect reopened owner");
    }
    {
        Rig r; auto job = r.owner->admit(std::make_shared<Frame>());
        check(r.owner->finish_submission(job.receipt, MovieGpuError::none) == MovieGpuError::none, "consumer release-failure source");
        r.port->complete(0); r.port->fail_release = 0;
        check(r.owner->poll(job.receipt).state == MovieFenceState::failed, "actual native release error is reported");
        check(r.owner->poll(job.receipt).state == MovieFenceState::failed && r.port->releases[0] == 1,
            "cleanup retry never upgrades a failed consumer receipt");
        auto tail = r.drain(); r.finish(tail);
        check(r.owner->poll(job.receipt).state == MovieFenceState::failed, "tail proof does not rewrite earlier terminal failure");
    }
    {
        Rig left, right; auto a = left.drain(), b = right.drain();
        check(left.owner->poll_drain(b.receipt).error == MovieGpuError::stale_resource &&
            left.owner->resume(b.receipt) == MovieGpuError::stale_resource &&
            left.owner->retry_drain(b.receipt).result.error == MovieGpuError::stale_resource,
            "foreign drain cannot poll, retry or reopen another sealed context");
        left.finish(a); right.finish(b);
    }
    for (bool missing_sync : {false, true}) {
        Rig r; auto source = std::make_shared<Frame>(); std::weak_ptr<Frame> weak = source;
        auto job = r.owner->admit(source); source.reset();
        r.port->null_next = missing_sync;
        const auto original = missing_sync ? MovieGpuError::none : MovieGpuError::present_failed;
        check(r.owner->finish_submission(job.receipt, original) != MovieGpuError::none, "failed native submission recorded");
        check(r.owner->poll(job.receipt).state == MovieFenceState::failed && !weak.expired(), "failed submission retains real source");
        job.receipt->retire(); auto tail = r.drain(); r.finish(tail);
        check(weak.expired() && r.owner->poll(job.receipt).state == MovieFenceState::failed,
            "later tail establishes safety without upgrading failed/no-sync consumer success");
    }
    {
        Rig r; check(r.owner->commands_issued() == MovieGpuError::none, "unpresented upload commands owned");
        r.port->next_error = MovieGpuError::fence_failed; auto failed = r.drain();
        check(failed.result.state == MovieFenceState::failed && r.owner->admission() == MovieGpuError::admission_closed, "failed tail remains sealed");
        const auto inserts = r.port->states.size(); gpu_test_fail_next_allocation = true;
        auto allocation = r.owner->retry_drain(failed.receipt);
        check(!gpu_test_fail_next_allocation && allocation.result.error == MovieGpuError::host_capacity &&
            r.owner->begin_drain().receipt == failed.receipt && r.port->states.size() == inserts &&
            r.owner->admission() == MovieGpuError::admission_closed, "replacement allocation failure retains exact failed seal");
        auto retry = r.owner->retry_drain(failed.receipt);
        check(retry.receipt != failed.receipt && r.owner->poll_drain(failed.receipt).error == MovieGpuError::stale_resource,
            "explicit retry creates a distinct checked receipt");
        r.finish(retry);
    }
    {
        Rig r; r.port->next_error = MovieGpuError::fence_failed; auto first = r.drain();
        r.port->next_error = MovieGpuError::fence_failed; auto second = r.owner->retry_drain(first.receipt);
        auto third = r.owner->retry_drain(second.receipt); r.port->complete(2); r.port->fail_release = 1;
        check(r.owner->poll_drain(third.receipt).state == MovieFenceState::failed && r.port->releases[0] == 1,
            "partial native deletion failure remains visible");
        check(r.owner->poll_drain(third.receipt).state == MovieFenceState::complete && r.port->polls[2] == 1 &&
            r.port->releases[0] == 1 && r.port->releases[1] == 1 && r.port->releases[2] == 1,
            "cleanup retry preserves tail proof and never reuses deleted prior sync");
    }
    {
        Rig r; std::vector<std::shared_ptr<MovieGpuWorkReceipt>> jobs;
        for (unsigned index = 0; index < 273; ++index) {
            auto job = r.owner->admit(std::make_shared<Frame>());
            check(job.receipt && r.owner->finish_submission(job.receipt, MovieGpuError::none) == MovieGpuError::none, "dynamic native work admission");
            jobs.push_back(job.receipt);
        }
        Rig foreign; check(foreign.owner->poll(jobs.back()).error == MovieGpuError::stale_resource, "foreign work identity rejected");
        auto tail = r.drain(); r.finish(tail);
        for (auto& job : jobs) check(r.owner->poll(job).state == MovieFenceState::complete, "all active receipts remain observable after drain");
    }
    {
        Rig r; check(r.owner->commands_issued() == MovieGpuError::none, "host-retired native context work");
        MovieGpuError error; auto host = NativeMovieGpuRetirementOwner::create(error);
        auto task = std::make_shared<RetiredSession>(r.owner); std::weak_ptr<RetiredSession> weak = task;
        auto registration = host->register_task(task, error); check(registration && !host->empty(), "preallocated actual host retirement registration");
        check(host->poll().state == MovieFenceState::pending && task->calls == 0 && r.port->states.empty(), "registration is not submission or completion");
        struct BackendLifetime { MovieGpuRetirementRegistration retirement; };
        auto backend=std::make_unique<BackendLifetime>();
        backend->retirement=MovieGpuRetirementRegistration(registration);
        std::thread retire([&] { backend.reset(); }); retire.join(); // Same production marker destructor as WGL backend.
        r.port->next_error = MovieGpuError::fence_failed;
        check(host->poll().state == MovieFenceState::failed && task->calls == 1, "dropped backend retains actual failed tail");
        check(host->poll().state == MovieFenceState::failed && task->calls == 1, "host keeps failure until explicit retry");
        check(host->retry_failed().state == MovieFenceState::pending && task->retries == 1, "host-owned retry remains available after backend destruction");
        task.reset(); check(!weak.expired(), "host pins real pending session without self-cycle");
        r.port->complete(r.port->states.size() - 1);
        check(host->poll().state == MovieFenceState::complete && host->empty(), "actual native completion retires host entry");
        registration.reset(); check(weak.expired(), "completed retirement has no hidden ownership cycle");
    }
    std::puts("PASS native GPU work/tail completion/sealed receipts/failed-source retention/host retirement; deterministic fence ports only");
    return std::fflush(stdout) || std::ferror(stdout) ? 1 : 0;
}
