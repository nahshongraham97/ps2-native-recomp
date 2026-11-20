#include "mka/audio_bank_service.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <thread>

using namespace mka::sound;
namespace {
constexpr std::uint32_t first_bank = 0xaa123721, second_bank = 0x9c343721;
void check(bool value, const char* reason) {
    if (!value) {
        std::fprintf(stderr, "FAIL: %s\n", reason);
        std::abort();
    }
}
AudioBankCommand command(AudioBankOpcode op, std::uint32_t bank = first_bank) {
    AudioBankCommand result;
    result.opcode = op;
    result.bank_token = {bank};
    return result;
}
struct Fixture {
    std::optional<AudioBankService> service;
    explicit Fixture(const char* path, const char* name = "announcr") {
        AudioBanksFailure bf;
        auto session = AudioBankSession::open(path, std::nullopt, bf);
        check(bool(session), "actual source Session");
        AudioProtocolFailure pf;
        auto protocol = AudioBankProtocol::create(std::move(*session), pf);
        check(bool(protocol), "actual bank Protocol");
        BankServiceFailure f;
        service = AudioBankService::create(std::move(*protocol), f);
        check(bool(service), "actual bank Service");
        load(first_bank, name);
    }
    void load(std::uint32_t token, const char* name = "announcr") {
        auto request = command(AudioBankOpcode::load_sync, token);
        request.pathname = name;
        BankServiceFailure f;
        auto result = service->load_sync(request, {token}, f);
        check(result && result->bank.value == token, "actual full bank token published");
    }
    std::shared_ptr<BankRuntimeProgramWork> admit(const AudioBankCommand& request) {
        PreparationFailure p;
        BankServiceFailure f;
        auto work = service->admit_runtime_program(request, p, f);
        check(work && work->observe().admission_retained, "actual Service-held runtime job");
        return work;
    }
    void blocked(BankServiceError why, std::uint32_t token = first_bank) {
        BankServiceFailure f;
        check(!service->unload(command(AudioBankOpcode::unload, token), f) && f.kind == why,
              "exact bank unload names the retained ownership");
    }
    void finish() {
        BankServiceFailure f;
        check(service->unload(command(AudioBankOpcode::unload), f) && service->close_when_idle(f),
              "exact bank unload and idle Service close after cleanup");
    }
};
void fresh_and_result(const char* path) {
    Fixture x(path);
    auto request = command(AudioBankOpcode::play_extended);
    request.callback_token = 0x86b73348;
    request.context_word = 0x1001;
    request.payload = {0x6a, 0x12, 0x34, 0x56};
    auto work = x.admit(request);
    request.bank_token = {second_bank};
    request.selector = -10;
    request.payload[0] = 0;
    check(work->request().bank_token.value == first_bank && work->request().selector == 0 &&
          work->request().payload[0] == 0x6a && work->request().callback_token == 0x86b73348 &&
          work->request().context_word == 0x1001, "runtime work owns immutable actual request fields");
    x.blocked(BankServiceError::program_work_active);
    PreparationFailure p;
    BankServiceFailure f;
    check(!x.service->close_when_idle(f), "queued actual work prevents idle close");
    check(work->build(p) && !work->observe().preparation.runtime_published &&
          work->observe().preparation.state == RuntimeProgramWorkState::ready,
          "actual Service build keeps fresh runtime private");
    std::thread other([&] {
        PreparationFailure failure;
        check(!work->retire(failure) && failure.kind == PreparationError::wrong_thread &&
              work->observe().admission_retained, "foreign failed retirement keeps job guard");
    });
    other.join();
    check(work->finish_build(p) && work->observe().preparation.runtime_published,
          "actual Service worker commits fresh runtime");
    auto result = work->take_result(p);
    check(result && result->active() && result->reference_count(p) == 1,
          "actual Service work transfers one reference");
    x.blocked(BankServiceError::program_work_active);
    check(work->retire(p) && !work->observe().admission_retained,
          "job retires independently of transferred result");
    x.blocked(BankServiceError::runtime_reference_active);
    check(!x.service->close_when_idle(f) && f.kind == BankServiceError::runtime_reference_active,
          "returned reference retains actual Service close guard");
    check(!result->preparation()->data()->normal_instances[0].sample->pcm[0].samples.empty() &&
          result->release(p), "actual result survives job retirement and owns PCM");
    x.finish();
}
void cancel_and_reuse(const char* path) {
    Fixture x(path);
    auto work = x.admit(command(AudioBankOpcode::play));
    PreparationFailure p;
    check(work->build(p), "fresh actual private build before cancellation");
    check(work->cancel() == ProgramWorkCancellation::running_requested &&
          !work->finish_build(p) && p.kind == PreparationError::cancelled &&
          !work->observe().preparation.runtime_published,
          "actual canceled candidate cannot publish runtime");
    x.blocked(BankServiceError::program_work_active);
    check(!work->take_result(p) && work->retire(p), "cancel produces no reference but work retires");
    auto again = x.admit(command(AudioBankOpcode::play));
    check(again->build(p) &&
          again->observe().preparation.branch == RuntimeProgramWorkBranch::fresh,
          "canceled work leaves bank usable with no cached runtime");
    check(again->cancel() == ProgramWorkCancellation::running_requested &&
          !again->finish_build(p) && again->retire(p), "retry can independently retire");
    auto untaken = x.admit(command(AudioBankOpcode::play));
    check(untaken->build(p) && untaken->finish_build(p) &&
          untaken->observe().preparation.runtime_published &&
          !untaken->observe().preparation.result_taken, "actual committed result remains untaken");
    x.blocked(BankServiceError::program_work_active);
    check(untaken->retire(p) && !untaken->observe().admission_retained &&
          !x.service->resource_domain()->observe()[0].depth,
          "explicit untaken retirement releases semantic reference and actual work barrier");
    {
        auto dropped = x.admit(command(AudioBankOpcode::play));
        check(dropped->build(p) && dropped->observe().preparation.branch == RuntimeProgramWorkBranch::fresh &&
              !dropped->observe().preparation.runtime_published,
              "untaken retirement left no cached runtime and next ready result is private");
    }
    check(!x.service->resource_domain()->observe()[0].depth,
          "ready work destruction releases actual program barrier");
    {
        auto dropped = x.admit(command(AudioBankOpcode::play));
        check(dropped->build(p) && dropped->observe().preparation.branch == RuntimeProgramWorkBranch::fresh &&
              dropped->finish_build(p), "ready destructor cleared private cache marker for fresh retry");
        x.blocked(BankServiceError::program_work_active);
    }
    check(!x.service->resource_domain()->observe()[0].depth,
          "committed work destruction releases actual program barrier");
    auto retry = x.admit(command(AudioBankOpcode::play));
    check(retry->build(p) && retry->observe().preparation.branch == RuntimeProgramWorkBranch::fresh &&
          retry->finish_build(p), "committed destructor detached its untaken semantic runtime result");
    check(retry->retire(p), "committed retry retires untaken result");
    x.finish();
}
void failed_build_guard(const char* path) {
    Fixture x(path);
    x.load(second_bank);
    auto domain = x.service->resource_domain();
    AudioResourceFailure rf;
    auto movie = domain->surround_movie(rf);
    check(bool(movie), "actual publication exclusion for failed build");
    auto foreign = x.admit(command(AudioBankOpcode::play, second_bank));
    std::promise<void> done;
    auto complete = done.get_future();
    std::thread worker([&] {
        PreparationFailure p;
        foreign->build(p);
        check(!foreign->finish_build(p) && p.kind == PreparationError::cancelled &&
              foreign->retire(p), "foreign work retires its real running cancellation");
        done.set_value();
    });
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!domain->observe()[3].waiting) {
        check(std::chrono::steady_clock::now() < until, "foreign worker actually waits on publication");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    auto rejected = x.admit(command(AudioBankOpcode::play));
    PreparationFailure p;
    check(!rejected->build(p) && p.kind == PreparationError::resource_access &&
          p.resource.kind == AudioResourceError::recursive_work_conflict,
          "actual resource conflict produces a named failed private build");
    check(!rejected->finish_build(p) && p.resource.kind == AudioResourceError::recursive_work_conflict &&
          rejected->observe().preparation.state == RuntimeProgramWorkState::failed,
          "worker decision preserves actual build failure");
    x.blocked(BankServiceError::program_work_active);
    check(rejected->retire(p) && !rejected->observe().admission_retained,
          "actual failed job releases its guard only on retirement");
    check(foreign->cancel() == ProgramWorkCancellation::running_requested && movie->release(rf) &&
          complete.wait_for(std::chrono::seconds(10)) == std::future_status::ready,
          "actual publication wait and retained worker resolve");
    worker.join();
    BankServiceFailure f;
    check(x.service->unload(command(AudioBankOpcode::unload, second_bank), f),
          "other bank guard retires independently");
    x.finish();
}
void cached_and_rejections(const char* path) {
    Fixture x(path, "bg_armory");
    PreparationFailure p;
    BankServiceFailure f;
    check(!x.service->admit_runtime_program(command(AudioBankOpcode::unload), p, f) &&
          f.kind == BankServiceError::wrong_command, "wrong command has no Service admission");
    check(!x.service->admit_runtime_program(command(AudioBankOpcode::play, 0x3721), p, f) &&
          f.kind == BankServiceError::unknown_bank_token, "full-width bank token cannot truncate");
    auto bad = command(AudioBankOpcode::play);
    bad.selector = 0x7fffffff;
    check(!x.service->admit_runtime_program(bad, p, f) && p.kind == PreparationError::absent_selector,
          "failed downstream selector rolls back Service guard");
    auto job = x.admit(command(AudioBankOpcode::play));
    check(job->build(p) && job->observe().preparation.branch == RuntimeProgramWorkBranch::cached &&
          !job->observe().preparation.cancellation_open,
          "actual eager cached branch is admitted by this runtime API");
    check(job->cancel() == ProgramWorkCancellation::not_cancellable && job->finish_build(p) &&
          !job->observe().preparation.runtime_published, "cached readiness has no new publication");
    auto cached = job->take_result(p);
    check(cached && job->retire(p) && cached->reference_count(p) == 1,
          "cached semantic reference keeps actual owner beyond job");
    x.load(second_bank);
    auto other = x.admit(command(AudioBankOpcode::play, second_bank));
    check(other->cancel() == ProgramWorkCancellation::queued_removed && other->retire(p) &&
          x.service->unload(command(AudioBankOpcode::unload, second_bank), f),
          "unrelated exact bank work retires and unloads independently");
    x.blocked(BankServiceError::runtime_reference_active);
    check(cached->release(p), "release actual cached reference");
    x.finish();
}
}
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    fresh_and_result(argv[1]);
    cancel_and_reuse(argv[1]);
    failed_build_guard(argv[1]);
    cached_and_rejections(argv[1]);
    std::puts("PASS_NATIVE_SERVICE_RUNTIME_WORK: 4 actual Service job and independent runtime-reference ownership groups");
    return std::fflush(stdout) || std::ferror(stdout) ? 1 : 0;
}
