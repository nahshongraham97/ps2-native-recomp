#include "mka/audio_bank_service.hpp"
#include "mka/program_executor.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <future>
#include <thread>
using namespace mka::sound;
namespace {
void check(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
template<class Predicate> void await(Predicate predicate, const char* message) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!predicate()) {
        check(std::chrono::steady_clock::now() < end, message);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
AudioBankCommand load(AudioBankOpcode opcode, const char* name) {
    AudioBankCommand command; command.opcode = opcode; command.pathname = name;
    command.callback_token = 0x97ac1357; return command;
}
AudioBankCommand play(std::uint32_t token) {
    AudioBankCommand command; command.opcode = AudioBankOpcode::play;
    command.bank_token = {token}; command.selector = 0; return command;
}
AudioBankService service(const std::filesystem::path& file,
                         const std::shared_ptr<AudioResourceDomain>& domain) {
    AudioBanksFailure banks; auto session = AudioBankSession::open(file, std::nullopt, domain, banks);
    check(session && session->resource_domain() == domain, "real session uses supplied context");
    AudioProtocolFailure protocol_failure;
    auto protocol = AudioBankProtocol::create(std::move(*session), protocol_failure);
    check(protocol && protocol->resource_domain() == domain, "same protocol owner");
    BankServiceFailure failure; auto result = AudioBankService::create(std::move(*protocol), failure);
    check(result && result->resource_domain() == domain, "same service owner");
    return std::move(*result);
}
struct ProgramResult {
    std::shared_ptr<const PreparedProgram> program;
    PreparationFailure preparation;
    BankServiceFailure service;
};
}
int main(int argc, char** argv) {
    if (argc != 3) return 2;
    const std::filesystem::path original(argv[1]), out(argv[2]);
    check(std::filesystem::create_directory(out), "exclusive diagnostic directory");
    AudioResourceFailure failure; auto domain = AudioResourceDomain::create(failure);
    check(bool(domain), "native resource owner");
    check(!domain->acquire(static_cast<AudioResourceWork>(9), failure) &&
        failure.kind == AudioResourceError::invalid_work, "unknown work rejected");
    {
        auto movie = domain->surround_movie(failure); check(bool(movie), "actual movie lease");
        auto observation = domain->observe();
        check(!observation[0].depth && !observation[1].depth && !observation[2].depth &&
            observation[3].depth == 1, "only publication retained after three distinct barriers");
        auto nested = domain->surround_movie(failure);
        check(nested && domain->observe()[3].depth == 2, "same-thread recursive acquisition is counted");
        check(nested->release(failure) && domain->observe()[3].depth == 1, "nested release retains original lease");
        AudioResourceFailure foreign;
        std::thread wrong([&] { check(!movie->release(foreign), "foreign explicit release fails"); });
        wrong.join();
        check(foreign.kind == AudioResourceError::wrong_thread && domain->observe()[3].depth == 1,
            "foreign release does not retire admission");
        check(movie->release(failure) && !movie->release(failure) &&
            failure.kind == AudioResourceError::released, "checked exact release pairing");
    }
    {
        auto movie = domain->surround_movie(failure); check(bool(movie), "recursive conflict fixture");
        std::thread pending([&] {
            AudioResourceFailure f; auto program = domain->acquire(AudioResourceWork::program, f);
            check(bool(program), "real pending resource job owns program work");
            auto publication = domain->acquire(AudioResourceWork::sample_publication, f);
            check(bool(publication), "pending publication resumes after original movie ends");
        });
        await([&] { return domain->observe()[3].waiting == 1; }, "actual cross-category dependency");
        auto repeated = domain->surround_movie(failure);
        check(!repeated && failure.kind == AudioResourceError::recursive_work_conflict && domain->observe()[3].depth == 1,
            "recursive cross-category cycle fails by name without releasing first lease");
        check(movie->release(failure), "original movie still ends normally");
        pending.join();
    }
    for (auto category : {AudioResourceWork::program, AudioResourceWork::sample, AudioResourceWork::backing}) {
        auto active = domain->acquire(category, failure); check(bool(active), "actual active operation");
        std::promise<bool> finished; auto future = finished.get_future();
        std::thread movie([&] { AudioResourceFailure f; auto held = domain->surround_movie(f); finished.set_value(bool(held)); });
        const auto index = static_cast<std::size_t>(category);
        await([&] { return domain->observe()[index].waiting == 1; }, "movie waits on actual active operation");
        check(future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready,
            "no synthetic early movie completion");
        check(active->release(failure), "actual work completed");
        movie.join(); check(future.get(), "barrier completes after real work release");
    }
    {
        ArchiveFailure archive_failure;
        auto archive = SoundArchive::open(original, domain, archive_failure);
        // The associated MS4 is a backing identity fixture only; no stream codec claim.
        const auto ms4 = out / "explicit-associated.ms4";
        { std::ofstream file(ms4, std::ios::binary); file << "owned associated backing"; check(bool(file), "fixture backing write"); }
        StreamFailure stream_failure; auto stream = StreamSource::open_associated_ms4(ms4, domain, stream_failure);
        check(archive && stream && archive->resource_domain() == domain && stream->resource_domain() == domain,
            "associated opened MS2 and MS4 share explicit owner");
        AudioBanksFailure banks;
        auto paired = AudioBankSession::open(original, ms4, domain, banks);
        check(paired && paired->resource_domain() == domain && paired->close(banks), "paired session same domain and close");
        check(!AudioBankSession::open(original, std::nullopt, nullptr, banks) &&
            banks.kind == AudioBanksError::resource_access && banks.resource.kind == AudioResourceError::invalid_owner,
            "no silent replacement for invalid shared context");
    }
    auto sound = service(original, domain); BankServiceFailure service_failure;
    auto loaded = sound.load_sync(load(AudioBankOpcode::load_sync, "announcr"), {0xe123a45b}, service_failure);
    check(loaded && loaded->bank.value == 0xe123a45b && loaded->preparation.programs.size() == 192,
        "actual original source-deferred bank ready without invented PCM");
    auto movie = domain->surround_movie(failure); check(bool(movie), "exclude actual sample publication");
    std::promise<ProgramResult> result; auto future = result.get_future();
    std::thread worker([&] {
        ProgramResult r; r.program = sound.prepare_program(play(0xe123a45b), r.preparation, r.service);
        result.set_value(std::move(r));
    });
    await([&] { return domain->observe()[3].waiting == 1; }, "real deferred PCM reaches blocked publication");
    check(future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready,
        "private decoded PCM not published during movie exclusion");
    check(!sound.poll_completion(service_failure) && service_failure.kind == BankServiceError::none,
        "movie does not invent a bank completion");
    check(sound.submit_async(load(AudioBankOpcode::load_async, "no-such-retail-bank"), {0x12345678}, service_failure)
        == AsyncBankSubmission::queued, "future queue admission is not globally frozen");
    check(movie->release(failure), "release actual shared publication owner");
    worker.join(); auto actual = future.get();
    check(actual.program && actual.program->normal_instances.size() == 1 &&
        actual.program->normal_instances[0].sample->pcm[0].samples.size() == 34720,
        "release publishes the original announcer resource with expected sample extent");
    auto original_sample = actual.program->normal_instances[0].sample;
    check(sound.run_one(service_failure), "queued work still executes");
    auto failed = sound.poll_completion(service_failure);
    check(failed && failed->words[0] == 2 && failed->words[1] == 1 &&
        failed->outcome.protocol_failure.bank.archive.kind == ArchiveError::absent_bank,
        "actual backing failure remains failure after movie exclusion");
    {
        auto held = domain->surround_movie(failure); check(bool(held), "cached sample movie interval");
        std::promise<ProgramResult> ready; auto ready_future = ready.get_future();
        std::thread cached([&] { ProgramResult r; r.program = sound.prepare_program(play(0xe123a45b), r.preparation, r.service); ready.set_value(std::move(r)); });
        await([&] { return ready_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready; },
            "existing immutable sample does not need new publication");
        cached.join(); auto again = ready_future.get();
        check(again.program && again.program->normal_instances[0].sample == original_sample,
            "same actual sample ownership survives movie interval");
        ProgramFailure program_failure; auto executor = ProgramExecutor::create(actual.program, {1000}, program_failure);
        check(bool(executor), "actual prepared program executor");
        auto event = executor->step({1000}, program_failure);
        check(event && event->kind == ProgramStepKind::sample_start && event->clip == original_sample &&
            executor->acknowledge_start(true, program_failure), "resource exclusion does not pause or rewrite program playback");
        check(held->release(failure), "cached interval release");
    }
    {
        AudioBanksFailure banks; auto session = AudioBankSession::open(original, std::nullopt, domain, banks);
        check(bool(session), "unload fixture session opens");
        auto bank = session->load_bank("announcr", banks); BankPreparationReport report; PreparationFailure prep;
        check(bank && session->prepare_bank(*bank, report, prep, banks), "independent deferred owner prepared");
        auto held = domain->surround_movie(failure); check(bool(held), "unload during private publication wait");
        std::promise<std::pair<std::shared_ptr<const PreparedProgram>, PreparationFailure>> retired;
        auto retired_future = retired.get_future();
        std::thread preparing([&] {
            PreparationFailure p; AudioBanksFailure b; auto value = session->prepare_program(*bank, 0, p, b);
            retired.set_value({std::move(value), p});
        });
        await([&] { return domain->observe()[3].waiting == 1; }, "private build waits with local locks released");
        check(session->unload_bank(*bank, banks), "unload completes while private publication is excluded");
        check(held->release(failure), "end needs no prepared bank or session mutex");
        preparing.join(); auto revoked = retired_future.get();
        check(!revoked.first && revoked.second.kind == PreparationError::unloaded,
            "unloaded private candidate cannot publish stale PCM");
        check(session->close(banks), "revoked session closes");
    }
    {
        auto revocable = service(original, domain); BankServiceFailure sf;
        auto loaded = revocable.load_sync(load(AudioBankOpcode::load_sync, "announcr"), {0x7654abcd}, sf);
        check(loaded && loaded->bank.value, "service revocation source bank");
        auto held = domain->surround_movie(failure); check(bool(held), "service publication exclusion");
        std::promise<ProgramResult> retired; auto retired_future = retired.get_future();
        std::thread preparing([&] { ProgramResult r; r.program = revocable.prepare_program(play(0x7654abcd), r.preparation, r.service); retired.set_value(std::move(r)); });
        await([&] { return domain->observe()[3].waiting == 1; }, "service and protocol reach actual publication wait");
        check(!revocable.close_when_idle(sf) && sf.kind == BankServiceError::worker_active,
            "close rejects actual active work instead of blocking movie owner");
        AudioBankCommand unload; unload.opcode = AudioBankOpcode::unload; unload.bank_token = {0x7654abcd};
        check(revocable.unload(unload, sf), "real service unload completes while worker publication is blocked");
        check(held->release(failure), "service movie owner can release without any request-table mutex");
        preparing.join(); auto revoked = retired_future.get();
        check(!revoked.program && revoked.preparation.kind == PreparationError::unloaded &&
            revoked.service.kind == BankServiceError::protocol_failed,
            "full service and protocol reject stale private sample after real unload");
        check(revocable.close_when_idle(sf), "actual active-operation count retires after failed publication");
    }
    {
        AudioBanksFailure banks; auto paired = AudioBankSession::open(original,
            out / "explicit-associated.ms4", domain, banks);
        check(bool(paired), "final opened backings retirement fixture");
        auto held = domain->surround_movie(failure); check(bool(held), "movie owns publication during final close");
        std::promise<void> entered, release; auto entered_future = entered.get_future(), release_future = release.get_future();
        std::thread reading([&] {
            AudioResourceFailure f; auto work = domain->acquire(AudioResourceWork::backing, f);
            check(bool(work), "actual independent backing operation");
            entered.set_value(); release_future.wait();
        });
        entered_future.wait();
        check(paired->close(banks) && domain->pending_backing_retirements() == 2,
            "final MS2 and associated MS4 handles retained for serialized retirement, never terminate");
        check(domain->observe()[3].depth == 1, "deferred final close does not drop movie exclusion");
        release.set_value(); reading.join();
        check(domain->pending_backing_retirements() == 0 && !domain->observe()[2].depth,
            "actual backing completion closes retained handles before retirement barrier completes");
        check(std::filesystem::remove(out / "explicit-associated.ms4"), "owned retired backing can be removed");
        check(held->release(failure), "final close leaves original movie lease valid");
    }
    for (unsigned trial = 0; trial < 32; ++trial) {
        auto retrying = service(original, domain);
        std::promise<void> start; auto begin = start.get_future().share();
        bool actual_loaded = false;
        std::thread missing([&] {
            begin.wait(); BankServiceFailure f;
            auto failed = retrying.load_sync(load(AudioBankOpcode::load_sync, "no-such-retail-bank"), {0x13579abc}, f);
            check((failed && !failed->bank.value) || (!failed && f.kind == BankServiceError::duplicate_bank_token),
                "concurrent failed request has only its real result or existing reservation");
        });
        std::thread retry([&] {
            begin.wait(); BankServiceFailure f;
            auto loaded = retrying.load_sync(load(AudioBankOpcode::load_sync, "announcr"), {0x13579abc}, f);
            check((loaded && loaded->bank.value == 0x13579abc) || (!loaded && f.kind == BankServiceError::duplicate_bank_token),
                "old failed cleanup cannot erase newer successful reservation");
            actual_loaded = bool(loaded);
        });
        start.set_value(); missing.join(); retry.join();
        BankServiceFailure f;
        if (!actual_loaded) {
            auto loaded = retrying.load_sync(load(AudioBankOpcode::load_sync, "announcr"), {0x13579abc}, f);
            check(loaded && loaded->bank.value == 0x13579abc, "exact token remains reusable after failed admission");
        }
        check(retrying.resolve_play_kind1(play(0x13579abc), 0, f).has_value() && retrying.close_when_idle(f),
            "concurrent retry retains real reachable bank and retires operation counts");
    }
    check(sound.close_when_idle(service_failure), "actual service drains and closes");
    for (const auto& observation : domain->observe())
        check(!observation.depth && !observation.waiting, "all actual leases and waiters retired");
    std::puts("PASS native audio resource barriers/shared backings/private PCM publication/unload; no game or physical audio");
    return std::fflush(stdout) || std::ferror(stdout) ? 1 : 0;
}
