#include "mka/audio_bank_service.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <utility>
using namespace mka::sound;
namespace {
thread_local bool throw_at_preparation = false;
unsigned faults = 0, real_calls = 0;
void check(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
AudioBankCommand load(AudioBankOpcode opcode) {
    AudioBankCommand command; command.opcode = opcode; command.pathname = "announcr";
    command.callback_token = 0x12349876; return command;
}
AudioBankService service(const std::filesystem::path& file) {
    AudioBanksFailure banks; auto session = AudioBankSession::open(file, std::nullopt, banks);
    check(bool(session), "actual session");
    AudioProtocolFailure protocol_failure;
    auto protocol = AudioBankProtocol::create(std::move(*session), protocol_failure);
    check(bool(protocol), "actual protocol");
    BankServiceFailure failure; auto result = AudioBankService::create(std::move(*protocol), failure);
    check(bool(result), "actual service"); return std::move(*result);
}
}
// Linker replacement is confined to THIS failure-injection executable. Every
// other fixture and the production native library call the real implementation.
extern "C" std::optional<PreparedBank> real_preparation(BankAssetSource,
    BankPreparationReport&, PreparationFailure&)
    __asm__("__real__ZN3mka5sound12PreparedBank7prepareENS0_15BankAssetSourceERNS0_21BankPreparationReportERNS0_18PreparationFailureE");
extern "C" std::optional<PreparedBank> wrapped_preparation(BankAssetSource,
    BankPreparationReport&, PreparationFailure&)
    __asm__("__wrap__ZN3mka5sound12PreparedBank7prepareENS0_15BankAssetSourceERNS0_21BankPreparationReportERNS0_18PreparationFailureE");
extern "C" std::optional<PreparedBank> wrapped_preparation(BankAssetSource source,
    BankPreparationReport& report, PreparationFailure& failure) {
    if (std::exchange(throw_at_preparation, false)) {
        ++faults; throw std::runtime_error("native preparation fixture fault");
    }
    ++real_calls; return real_preparation(std::move(source), report, failure);
}
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const std::filesystem::path original(argv[1]);
    {
        AudioBanksFailure banks; auto session = AudioBankSession::open(original, std::nullopt, banks);
        check(bool(session), "exception session open");
        auto handle = session->load_bank("announcr", banks); check(bool(handle), "actual private candidate");
        BankPreparationReport report; PreparationFailure preparation;
        throw_at_preparation = true; bool observed = false;
        try { session->prepare_bank(*handle, report, preparation, banks); }
        catch (const std::runtime_error&) { observed = true; }
        check(observed && !session->load_state(*handle, banks) && banks.kind == AudioBanksError::unloaded_handle,
            "unexpected preparation exception revokes whole failed candidate");
        auto retry = session->load_bank("announcr", banks);
        check(retry && session->prepare_bank(*retry, report, preparation, banks),
            "same native context prepares actual data after exception without stranded admission");
        check(session->close(banks), "candidate cleanup session closes");
    }
    {
        auto sound = service(original); BankServiceFailure failure;
        throw_at_preparation = true;
        check(!sound.load_sync(load(AudioBankOpcode::load_sync), {0xabcdef01}, failure) &&
            failure.kind == BankServiceError::native_exception, "sync exception is a named native failure");
        auto retry = sound.load_sync(load(AudioBankOpcode::load_sync), {0xabcdef01}, failure);
        check(retry && retry->bank.value == 0xabcdef01, "failed sync reservation retired before exact token retry");
        check(sound.close_when_idle(failure), "sync active-operation guard retired");
    }
    {
        auto sound = service(original); BankServiceFailure failure;
        check(sound.submit_async(load(AudioBankOpcode::load_async), {0x9876abcd}, failure) == AsyncBankSubmission::queued,
            "actual async request admitted");
        throw_at_preparation = true;
        check(!sound.run_one(failure) && failure.kind == BankServiceError::native_exception,
            "async executor reports actual native failure");
        auto completed = sound.poll_completion(failure);
        check(completed && completed->callback_token == 0x12349876 && completed->words[0] == 2 &&
            completed->words[1] == 1 && !completed->outcome.bank.value &&
            completed->outcome.native_failure == BankServiceError::native_exception,
            "admitted async failure completes once as failure, never a successful bank");
        check(!sound.poll_completion(failure), "no duplicate exception completion");
        auto retry = sound.load_sync(load(AudioBankOpcode::load_sync), {0x9876abcd}, failure);
        check(retry && retry->bank.value == 0x9876abcd && sound.close_when_idle(failure),
            "async failure retires busy worker and token before real same-token retry");
    }
    check(faults == 3 && real_calls == 3, "injection and real native retry call identities");
    std::puts("PASS injected native preparation exceptions/whole-candidate cleanup/exact-token retries/failure completion; no production fault switch");
    return std::fflush(stdout) || std::ferror(stdout) ? 1 : 0;
}
