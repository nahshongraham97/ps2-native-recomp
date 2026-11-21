#include "mka/retail_sound_bank_jobs.hpp"
#include "../runtime/src/native/retail_main_control.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace mka::sound;
namespace {
void check(bool ok, const char* why) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}
void word(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint32_t value, unsigned width = 4) {
    for (unsigned i = 0; i < width; ++i) bytes[at + i] = static_cast<std::uint8_t>(value >> (i * 8));
}
std::vector<std::uint8_t> bank_bytes() {
    std::vector<std::uint8_t> bank(158);
    word(bank, 0, 11); word(bank, 4, 0x13);
    word(bank, 8, 1, 2); word(bank, 10, 1, 2); word(bank, 12, 1, 2);
    word(bank, 16, 48); word(bank, 20, 84); word(bank, 24, 100);
    word(bank, 28, 116); word(bank, 32, 156);
    word(bank, 48, 0xdeadbeef); word(bank, 52, 0xdeadbeef);
    word(bank, 56, 0x20000002); word(bank, 60, 22050);
    word(bank, 64, 1000); word(bank, 68, 36);
    word(bank, 100 + 8, 1); word(bank, 116, 0x20000002);
    bank[124] = 1; bank[126] = 0xfd; bank[127] = 0xff;
    word(bank, 156, 1, 2);
    return bank;
}
void write_archive(const std::filesystem::path& path) {
    std::vector<std::uint8_t> bytes(1200);
    word(bytes, 0, 2); word(bytes, 4, 3); word(bytes, 8, 58);
    word(bytes, 16, static_cast<std::uint32_t>(bytes.size()));
    word(bytes, 24, 1); word(bytes, 28, 128); word(bytes, 32, 158);
    word(bytes, 36, 2); word(bytes, 40, 512); word(bytes, 44, 158);
    const char names[] = "\0first.mst\0second.mst";
    std::copy_n(reinterpret_cast<const std::uint8_t*>(names), sizeof(names), bytes.begin() + 48);
    auto bank = bank_bytes();
    std::copy(bank.begin(), bank.end(), bytes.begin() + 128);
    std::copy(bank.begin(), bank.end(), bytes.begin() + 512);
    word(bytes, 1000 + 8, 1); word(bytes, 1000 + 12, 22050);
    word(bytes, 1000 + 16, 16);
    bytes[1020] = 12; bytes[1021] = 7;
    for (unsigned i = 2; i < 16; ++i) bytes[1020 + i] = 0x11;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    check(bool(out), "write exclusively owned archive fixture");
}
std::unique_ptr<NativeRetailSoundBankJobs> make_jobs(const std::filesystem::path& path) {
    AudioBanksFailure bank_failure;
    auto session = AudioBankSession::open(path, std::nullopt, bank_failure);
    check(bool(session), "real parsed AudioBankSession");
    RetailSoundBankJobFailure failure;
    auto jobs = NativeRetailSoundBankJobs::create(std::move(*session), failure);
    check(bool(jobs), "real typed bank jobs owner");
    return jobs;
}
}
int main(int argc, char** argv) {
    check(argc == 2, "usage: test_retail_sound_bank_jobs <NEW disposable directory>");
    const std::filesystem::path dir(argv[1]);
    check(std::filesystem::create_directory(dir), "exclusive disposable directory");
    const auto path = dir / "jobs.ms2";
    write_archive(path);
    auto jobs = make_jobs(path);
    RetailSoundBankJobFailure failure;
    std::vector<std::pair<int, RetailSoundBankJobStatus>> events;
    auto target = [&](int id) {
        auto result = NativeRetailSoundBankJobTarget::create(
            [&, id](const RetailSoundBankJobCompletion& c) { events.emplace_back(id, c.status); }, failure);
        check(bool(result), "required typed completion target");
        return result;
    };
    auto first = target(1), second = target(2), missing = target(3);
    check(jobs->submit("first.msb", first, failure) == RetailSoundBankSubmission::queued,
          "first source request queued");
    check(jobs->submit("second.msb", second, failure) == RetailSoundBankSubmission::queued,
          "second source request queued in order");
    check(jobs->submit("other-name.msb", first, failure) == RetailSoundBankSubmission::already_pending &&
          jobs->queued_count() == 2, "duplicate callback identity retains first job, not second pathname");
    check(jobs->submit("absent.msb", missing, failure) == RetailSoundBankSubmission::queued,
          "real absent bank request queued rather than defaulting");
    check(jobs->cancel_path_from_317960("second.msb", failure) == 1 && jobs->queued_count() == 2 &&
          events.size() == 1 && events[0] == std::make_pair(2, RetailSoundBankJobStatus::cancelled_queued),
          "different-path queued cancellation callback precedes unlink");
    check(jobs->run_one(failure) && jobs->queued_count() == 1 && events.size() == 2 &&
          events[1] == std::make_pair(1, RetailSoundBankJobStatus::completed),
          "actual bank load and preparation completes before next job");
    auto first_observation = first->observe(failure);
    check(first_observation && first_observation->bank &&
          first_observation->status == RetailSoundBankJobStatus::completed,
          "full native bank handle identity published, no wire token");
    check(jobs->unload(*first_observation->bank, failure), "real typed bank unload");
    check(jobs->run_one(failure) && events.size() == 3 &&
          events[2] == std::make_pair(3, RetailSoundBankJobStatus::failed),
          "missing bank completes with real failure, no substitute bank");
    check(!jobs->run_one(failure) && failure.kind == RetailSoundBankJobError::no_pending_work,
          "empty worker is named, not fake work");

    auto active = target(4), later = target(5);
    check(jobs->submit("first.msb", active, failure) == RetailSoundBankSubmission::queued &&
          jobs->submit("second.msb", later, failure) == RetailSoundBankSubmission::queued,
          "second batch has actual pending work");
    check(jobs->cancel_path_from_317960("first.msb", failure) == 1 && events.size() == 3,
          "head-path cancellation marks active request without premature callback");
    check(jobs->run_one(failure) && events.size() == 4 &&
          events[3] == std::make_pair(4, RetailSoundBankJobStatus::cancelled_after_active),
          "active cancellation loads then unloads actual returned handle before status4");
    check(jobs->run_one(failure) && events.size() == 5 &&
          events[4] == std::make_pair(5, RetailSoundBankJobStatus::completed),
          "next bank begins after cancelled completion");
    auto later_bank = later->observe(failure);
    check(later_bank && later_bank->bank && jobs->unload(*later_bank->bank, failure),
          "second typed bank unload");

    RetailSoundBankJobFailure foreign_failure;
    std::thread foreign([&] { jobs->submit("first.msb", target(6), foreign_failure); });
    foreign.join();
    check(foreign_failure.kind == RetailSoundBankJobError::wrong_thread,
          "foreign producer is refused before queue mutation");

    auto transfer_jobs = make_jobs(path);
    auto transfer = NativeRetailSoundBankJobTarget::create(
        [](const RetailSoundBankJobCompletion&) {
            throw mka::retail::detail::NativeMainReentry{nullptr, 3};
        }, failure);
    check(bool(transfer) && transfer_jobs->submit("first.msb", transfer, failure) ==
          RetailSoundBankSubmission::queued, "real transfer callback admitted");
    bool transferred = false;
    try { transfer_jobs->run_one(failure); }
    catch (const mka::retail::detail::NativeMainReentry&) { transferred = true; }
    check(transferred && transfer->observe(failure)->callback_delivered,
          "typed Main control transfer propagates after one completion publication");
    check(!transfer_jobs->run_one(failure) && failure.kind == RetailSoundBankJobError::no_pending_work,
          "later worker does not replay completed callback after transfer");

    auto failure_jobs = make_jobs(path);
    auto throwing = NativeRetailSoundBankJobTarget::create(
        [](const RetailSoundBankJobCompletion&) { throw std::runtime_error("fixture callback"); }, failure);
    check(bool(throwing) && failure_jobs->submit("first.msb", throwing, failure) ==
          RetailSoundBankSubmission::queued, "ordinary failure callback admitted");
    check(!failure_jobs->run_one(failure) &&
          failure.kind == RetailSoundBankJobError::after_effect_failure &&
          !failure_jobs->run_one(failure) && failure.kind == RetailSoundBankJobError::after_effect_failure,
          "ordinary callback failure latches after effect, never replays or falsely advances");
    std::puts("PASS_RETAIL_SOUND_BANK_JOBS");
    return 0;
}
