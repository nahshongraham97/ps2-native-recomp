#include "mka/runtime_preparation.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <thread>

using namespace mka::sound;
namespace {
void check(bool value, const char* reason) {
    if (!value) {
        std::fprintf(stderr, "FAIL: %s\n", reason);
        std::abort();
    }
}
struct Fixture {
    std::shared_ptr<AudioResourceDomain> domain;
    std::optional<PreparedBank> bank;
    explicit Fixture(const char* path, const char* name = "announcr") {
        AudioResourceFailure rf;
        domain = AudioResourceDomain::create(rf);
        check(bool(domain), "actual resource domain");
        ArchiveFailure af;
        auto archive = SoundArchive::open(path, domain, af);
        check(bool(archive), "actual archive");
        auto source = archive->read_bank(name, af);
        check(bool(source), "actual named source bank");
        BankPreparationReport report;
        PreparationFailure f;
        bank = PreparedBank::prepare(std::move(*source), report, f);
        check(bool(bank), "actual prepared bank");
    }
    std::shared_ptr<RuntimeProgramWork> admit() {
        PreparationFailure f;
        auto work = bank->admit_runtime_program(0, f);
        check(bool(work), "actual runtime work admission");
        return work;
    }
    RuntimeCacheObservation record() {
        PreparationFailure f;
        auto result = bank->runtime_cache(f);
        check(result && !result->empty(), "actual runtime cache record");
        return (*result)[0];
    }
};
void selected_before_construction(const char* path) {
    for(bool cached:{false,true}) {
        Fixture x(path,cached?"bg_armory":"announcr");auto work=x.admit();PreparationFailure f;
        check(work->cached_runtime_available()==cached&&!x.domain->observe()[0].depth,
              "queue preference observes real cache without acquiring program resources");
        std::promise<void> selected,resume;auto ready=selected.get_future();auto go=resume.get_future();
        std::unique_ptr<RuntimeProgramReference> reference;
        std::thread worker([&]{PreparationFailure error;
            check(work->begin_build(error),"selected worker publishes running before construction");
            check(!work->begin_build(error)&&error.kind==PreparationError::invalid_work_state,"selection cannot run twice");
            selected.set_value();go.wait();
            check(work->build(error),"same selected worker constructs real program once");
            check(!work->build(error)&&error.kind==PreparationError::invalid_work_state,"construction cannot replay");
            if(cached){check(work->finish_build(error),"cached branch bypasses pre-construction request");reference=work->take_result(error);check(bool(reference),"actual cached reference returned");}
            else check(!work->finish_build(error)&&error.kind==PreparationError::cancelled,"fresh branch suppresses pre-construction request");
            check(work->retire(error),"selected work retires on actual native thread");});
        check(ready.wait_for(std::chrono::seconds(10))==std::future_status::ready,"actual worker selected");
        auto observation=work->observe();
        check(observation.state==RuntimeProgramWorkState::building&&observation.cancellation_open&&
              observation.branch==RuntimeProgramWorkBranch::unselected&&!x.domain->observe()[0].depth,
              "selection owns running identity without resources or guessed cache branch");
        check(!work->build(f)&&f.kind==PreparationError::wrong_thread,"foreign construction cannot steal selected job");
        check(work->cancel()==ProgramWorkCancellation::running_requested,"selected unstarted job has running cancellation");
        resume.set_value();worker.join();
        check(!work->cached_runtime_available(),"retired ticket supplies no queue preference");
        if(reference)check(reference->release(f),"real cached reference released");
    }
}
void fresh_commit(const char* path) {
    Fixture x(path);
    auto work = x.admit();
    PreparationFailure f;
    auto queued = work->observe();
    check(queued.state == RuntimeProgramWorkState::queued &&
          queued.branch == RuntimeProgramWorkBranch::unselected && !x.domain->observe()[0].depth,
          "queued ticket owns no active program lease");
    check(work->build(f), "build actual fresh runtime candidate");
    auto ready = work->observe();
    auto record = x.record();
    check(ready.state == RuntimeProgramWorkState::ready &&
          ready.branch == RuntimeProgramWorkBranch::fresh && ready.cancellation_open &&
          !ready.runtime_published && !record.cached && record.references == 0,
          "ready private candidate leaves the real runtime record unpublished");
    std::thread wrong([&] {
        PreparationFailure error;
        check(!work->finish_build(error) && error.kind == PreparationError::wrong_thread,
              "foreign thread cannot commit a ready candidate");
        check(!work->take_result(error) && error.kind == PreparationError::wrong_thread,
              "foreign thread cannot take the real reference");
        check(!work->retire(error) && error.kind == PreparationError::wrong_thread,
              "foreign thread cannot retire the real program lease");
    });
    wrong.join();
    check(work->finish_build(f), "commit actual runtime on its worker");
    auto committed = work->observe();
    check(committed.state == RuntimeProgramWorkState::committed && committed.runtime_published &&
          !committed.cancellation_open && x.record().cached && x.record().references == 1,
          "fresh commit publishes exact runtime with one reference");
    check(work->cancel() == ProgramWorkCancellation::not_cancellable,
          "committed result cannot be canceled");
    auto reference = work->take_result(f);
    check(reference && reference->active() && reference->reference_count(f) == 1 &&
          work->observe().result_taken, "take transfers the actual semantic reference");
    check(!work->take_result(f), "reference is delivered only once");
    check(work->retire(f) && work->observe().state == RuntimeProgramWorkState::retired &&
          !x.domain->observe()[0].depth, "retire joins actual work before release");
    check(reference->active() && reference->preparation() &&
          !reference->preparation()->data()->normal_instances[0].sample->pcm[0].samples.empty(),
          "real prepared data and semantic reference survive work retirement");
    check(reference->release(f) && !x.record().cached && x.record().references == 0,
          "last deferred reference detaches actual runtime");
}
void cancellation(const char* path) {
    Fixture x(path);
    PreparationFailure f;
    auto queued = x.admit();
    check(queued->cancel() == ProgramWorkCancellation::queued_removed &&
          queued->observe().state == RuntimeProgramWorkState::cancelled &&
          queued->retire(f), "remove queued work without starting program resources");
    check(!x.record().cached, "queued cancellation publishes no runtime");
    auto ready = x.admit();
    check(ready->build(f) && ready->observe().cancellation_open && !x.record().cached,
          "fresh fully built candidate stays cancellable before commit");
    check(ready->cancel() == ProgramWorkCancellation::running_requested,
          "cancel after actual build before runtime publication");
    check(!ready->finish_build(f) && f.kind == PreparationError::cancelled &&
          ready->observe().state == RuntimeProgramWorkState::cancelled &&
          !ready->observe().runtime_published && !x.record().cached,
          "fresh cancellation discards candidate without publishing runtime");
    check(ready->retire(f) && !x.domain->observe()[0].depth, "cancelled candidate fully retires");
}
void blocked_publication_cancel(const char* path) {
    Fixture x(path);
    AudioResourceFailure rf;
    auto film = x.domain->surround_movie(rf);
    check(bool(film), "actual publication exclusion");
    auto work = x.admit();
    std::promise<void> done;
    auto completed = done.get_future();
    std::thread worker([&] {
        PreparationFailure error;
        work->build(error);
        check(!work->finish_build(error) && error.kind == PreparationError::cancelled,
              "worker resolves running cancellation without publication");
        check(work->retire(error), "worker retires actual publication work");
        done.set_value();
    });
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!x.domain->observe()[3].waiting) {
        check(std::chrono::steady_clock::now() < until, "actual worker reaches publication wait");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    check(work->cancel() == ProgramWorkCancellation::running_requested &&
          work->observe().cancellation_requested, "running worker receives exact cancellation request");
    check(film->release(rf) && completed.wait_for(std::chrono::seconds(10)) == std::future_status::ready,
          "publish gate release lets real worker finish retirement");
    worker.join();
    check(!x.record().cached && !work->observe().runtime_published,
          "running cancellation leaves cache absent");
}
void cached_and_unload(const char* path) {
    Fixture eager(path, "bg_armory");
    PreparationFailure f;
    check(eager.record().cached && eager.record().references == 0, "actual eager starts with zero refs");
    auto work = eager.admit();
    check(work->build(f) && work->observe().branch == RuntimeProgramWorkBranch::cached &&
          !work->observe().cancellation_open && eager.record().references == 1,
          "cached worker acquires the existing actual runtime reference");
    check(work->cancel() == ProgramWorkCancellation::not_cancellable &&
          work->finish_build(f) && !work->observe().runtime_published,
          "cached worker bypasses fresh cancellation and publication");
    auto ref = work->take_result(f);
    check(ref && work->retire(f) && ref->reference_count(f) == 1,
          "actual cached reference remains valid beyond work");
    check(ref->release(f) && eager.record().cached && eager.record().references == 0,
          "eager cache survives semantic reference zero");
    auto barrier = eager.domain->acquire(AudioResourceWork::program, f.resource);
    check(bool(barrier), "actual program barrier holds cached selection");
    auto racing = eager.admit();
    std::promise<void> done;
    auto completed = done.get_future();
    std::unique_ptr<RuntimeProgramReference> cached_result;
    std::thread worker([&] {
        PreparationFailure error;
        check(racing->build(error) && racing->finish_build(error),
              "worker commits existing cached runtime despite earlier request");
        cached_result = racing->take_result(error);
        check(cached_result && racing->retire(error), "cached worker transfers and retires");
        done.set_value();
    });
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!eager.domain->observe()[0].waiting) {
        check(std::chrono::steady_clock::now() < until, "cached worker reaches actual program barrier");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    auto before_selection = racing->observe();
    check(before_selection.branch == RuntimeProgramWorkBranch::unselected &&
          before_selection.cancellation_open &&
          racing->cancel() == ProgramWorkCancellation::running_requested,
          "cancellation requested before actual cached branch selection");
    check(barrier->release(f.resource) &&
          completed.wait_for(std::chrono::seconds(10)) == std::future_status::ready,
          "cached worker resolves after barrier release");
    worker.join();
    auto selected = racing->observe();
    check(selected.branch == RuntimeProgramWorkBranch::cached &&
          selected.cancellation_requested && !selected.runtime_published &&
          selected.result_taken && cached_result->reference_count(f) == 1,
          "cached branch bypasses requested fresh cancellation and returns real reference");
    check(cached_result->release(f) && eager.record().cached && eager.record().references == 0,
          "cached race releases its exact acquisition without dropping eager cache");

    Fixture deferred(path);
    auto ready = deferred.admit();
    check(ready->build(f) && !deferred.record().cached, "unload race starts with actual private candidate");
    deferred.bank->unload();
    check(!ready->finish_build(f) && f.kind == PreparationError::unloaded &&
          !ready->observe().runtime_published && !ready->take_result(f),
          "unload revokes actual publication before commit");
    check(ready->retire(f), "unloaded private candidate retires");

    Fixture committed(path);
    auto published = committed.admit();
    check(published->build(f) && published->finish_build(f) &&
          published->observe().runtime_published && committed.record().references == 1,
          "second bank publishes a fresh runtime before the unload");
    auto peer = committed.bank->acquire_runtime_program(0, f);
    check(peer && peer->reference_count(f) == 2, "independent cached acquisition observes both references");
    committed.bank->unload();
    check(!published->take_result(f) && f.kind == PreparationError::unloaded &&
          !published->observe().result_taken, "post-commit unload revokes undelivered runtime reference");
    check(peer->reference_count(f) == 1, "revoked committed acquisition releases exactly its own reference");
    check(published->retire(f) && !committed.domain->observe()[0].depth && peer->release(f),
          "revoked work and independent post-unload reference retire cleanly");
}
}
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    selected_before_construction(argv[1]);
    fresh_commit(argv[1]);
    cancellation(argv[1]);
    blocked_publication_cancel(argv[1]);
    cached_and_unload(argv[1]);
    std::puts("PASS_NATIVE_RUNTIME_PROGRAM_WORK: 5 actual selection/private build, cancellation, commit and retirement groups");
    return std::fflush(stdout) || std::ferror(stdout) ? 1 : 0;
}
