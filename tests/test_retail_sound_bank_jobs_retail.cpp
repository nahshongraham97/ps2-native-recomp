#include "mka/retail_sound_bank_jobs.hpp"
#include <array>
#include <cstdio>
#include <stdexcept>
#include <string_view>
#include <vector>

using namespace mka::sound;
namespace {
void require(bool ok, const char* why) {
    if (!ok) throw std::runtime_error(why);
}
struct Event {
    std::string_view name;
    RetailSoundBankJobCompletion completion;
};
}
int main(int argc, char** argv) {
    try {
        require(argc == 3, "expected pinned native MS2 and explicitly associated MS4");
        AudioBanksFailure bank_failure;
        auto session = AudioBankSession::open(argv[1], std::filesystem::path(argv[2]), bank_failure);
        require(bool(session), "actual native sound files must open");
        RetailSoundBankJobFailure failure;
        auto jobs = NativeRetailSoundBankJobs::create(std::move(*session), failure);
        require(bool(jobs), "real native bank worker owner");
        // Original PS2 mode-one descriptor orders catalog0,8,2,1. Source and
        // donor data join is recorded in the parent xbox-donor-20261121 record.
        constexpr std::array<std::string_view, 4> names{
            "permanent", "announcr", "shell_music", "shell"};
        std::vector<Event> events;
        std::vector<std::shared_ptr<NativeRetailSoundBankJobTarget>> targets;
        auto target = [&](std::string_view name) {
            auto value = NativeRetailSoundBankJobTarget::create(
                [&, name](const RetailSoundBankJobCompletion& completion) {
                    events.push_back({name, completion});
                    std::printf("bank=%.*s status=%u handle=%d bank_error=%d preparation_error=%d\n",
                        int(name.size()), name.data(), unsigned(completion.status), bool(completion.bank),
                        int(completion.bank_failure.kind), int(completion.preparation_failure.kind));
                }, failure);
            require(bool(value), "required observation callback");
            return value;
        };
        for (auto name : names) {
            targets.push_back(target(name));
            require(jobs->submit(name, targets.back(), failure) == RetailSoundBankSubmission::queued,
                "real startup bank is queued");
            auto pending = targets.back()->observe(failure);
            require(pending && !pending->bank && !pending->callback_delivered &&
                pending->status == RetailSoundBankJobStatus::pending, "pending is never successful completion");
        }
        require(jobs->queued_count() == names.size() && events.empty(), "all actual requests begin pending");
        for (std::size_t i = 0; i < names.size(); ++i) {
            require(jobs->run_one(failure), "real startup native file/parse/preparation work finishes");
            require(events.size() == i + 1 && events[i].name == names[i] &&
                events[i].completion.status == RetailSoundBankJobStatus::completed &&
                events[i].completion.bank && jobs->queued_count() == names.size() - i - 1,
                "each actual startup bank must prepare and publish its own native identity in source order");
        }
        for (const auto& event : events)
            require(jobs->unload(*event.completion.bank, failure), "actual prepared startup bank retires");

        auto absent = target("__MKAR_no_such_bank__");
        require(jobs->submit("__MKAR_no_such_bank__", absent, failure) == RetailSoundBankSubmission::queued &&
            jobs->run_one(failure), "actual missing-bank request reaches completion");
        require(events.back().completion.status == RetailSoundBankJobStatus::failed &&
            !events.back().completion.bank &&
            events.back().completion.bank_failure.kind == AudioBanksError::archive_failed &&
            events.back().completion.bank_failure.archive.kind == ArchiveError::absent_bank,
            "missing actual bank cannot default to an existing asset");

        const auto before = events.size();
        auto active = target("announcr"), queued = target("shell");
        require(jobs->submit("announcr", active, failure) == RetailSoundBankSubmission::queued &&
            jobs->submit("shell", queued, failure) == RetailSoundBankSubmission::queued,
            "actual cancellation batch queued");
        require(jobs->cancel_path_from_317960("shell", failure) == 1 &&
            events.size() == before + 1 &&
            events.back().completion.status == RetailSoundBankJobStatus::cancelled_queued &&
            !events.back().completion.bank, "queued real pathname cancellation completes before unlink");
        require(jobs->cancel_path_from_317960("announcr", failure) == 1 &&
            events.size() == before + 1, "head pathname cancellation stays pending until work retirement");
        require(jobs->run_one(failure) && events.size() == before + 2 &&
            events.back().completion.status == RetailSoundBankJobStatus::cancelled_after_active &&
            !events.back().completion.bank && jobs->queued_count() == 0,
            "active real bank is loaded/prepared/unloaded before cancelled completion");
        std::puts("PASS_RETAIL_SOUND_BANK_JOBS_REAL_FILES; no Main/game/playback/physical endpoint claim");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL_RETAIL_SOUND_BANK_JOBS_REAL_FILES: %s\n", error.what());
        return 1;
    }
}
