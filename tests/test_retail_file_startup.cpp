#include "mka/retail_main.hpp"
#include "mka/retail_middleware_work.hpp"
#include "native_retail_startup.hpp"

#include <array>
#include <cstddef>
#include <limits>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <new>
#include <stdexcept>
#include <thread>

using namespace mka::retail;
namespace {
std::ptrdiff_t allocation_cut = -1;
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
struct Fixture {
    std::filesystem::path root, file;
    bool owned = false;
    explicit Fixture(const std::filesystem::path& disposable_parent) {
        // An exclusive child is never reused or removed recursively.
        for (unsigned n = 0; n < 1000; ++n) {
            const auto candidate = disposable_parent /
                ("retail-file-startup-" + std::to_string(n));
            if (std::filesystem::create_directory(candidate)) {
                root = candidate;
                owned = true;
                break;
            }
        }
        check(owned, "exclusive fixture directory");
        file = root / "mkda.pak";
        std::ofstream out(file, std::ios::binary);
        check(bool(out), "fixture file open");
        out.write("ABCD", 4);
        check(bool(out), "fixture file write");
    }
    ~Fixture() {
        if (!owned) return;
        std::error_code ignored;
        std::filesystem::remove(file, ignored);
        std::filesystem::remove(root, ignored);
    }
};
struct MainFixture {
    NativeRetailStartupState startup;
    mka::input::ControllerInput samples;
    NativeRetailMainState main;
    explicit MainFixture(const std::filesystem::path& root)
        : startup(open_volume(root), {}), main(startup, samples) {}
    static mka::content::NativeContentVolume open_volume(const std::filesystem::path& root) {
        mka::content::ContentVolumeFailure f;
        auto volume = mka::content::NativeContentVolume::open(root, f);
        check(bool(volume), "native volume opened");
        return std::move(*volume);
    }
    void ready_crt_and_group() {
        RetailCrtFailure cf;
        startup.crt = NativeRetailCrt::create(cf);
        check(bool(startup.crt) && startup.crt->finalize_pre_main(cf), "real CRT finalized");
        StartupFailure f;
        check(main_binding::construct_retail_middleware_work_group(main, f), "source constructor group");
    }
};
struct Child final : NativeRetailMiddlewareChild {
    unsigned polls = 0, primary = 0, releases = 0;
    std::optional<RetailMiddlewarePollStatus> poll(RetailMiddlewareWorkFailure&) override {
        ++polls; return RetailMiddlewarePollStatus::transfer_to_parent;
    }
    bool primary_complete(RetailMiddlewareWorkFailure&) override { ++primary; return true; }
    std::optional<bool> release_required(RetailMiddlewareWorkFailure&) const override { return true; }
    std::optional<RetailMiddlewareReleaseResult> release(RetailMiddlewareWorkFailure&) override {
        ++releases; return RetailMiddlewareReleaseResult::released;
    }
};
}
void* operator new(std::size_t n) {
    if (allocation_cut == 0) { allocation_cut = -1; throw std::bad_alloc(); }
    if (allocation_cut > 0) --allocation_cut;
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

int main(int argc, char** argv) {
    try {
        check(argc == 2, "explicit disposable fixture parent required");
        Fixture fixture(argv[1]);
        {
            MainFixture env(fixture.root);
            StartupFailure f;
            check(!main_binding::initialize_retail_middleware_files(env.main, f) &&
                  f.kind == StartupError::middleware && !env.main.file_paths,
                  "missing source work group refused");
            env.ready_crt_and_group();
            auto original_group = env.main.middleware_work_group;
            check(main_binding::initialize_retail_middleware_files(env.main, f) &&
                  env.main.middleware_work_group == original_group &&
                  env.main.middleware_dispatcher && env.main.file_paths && env.startup.middleware_frame,
                  "source group attached before aliases and persistent native frame publication");
            auto group = env.main.middleware_work_group->observe();
            auto dispatcher = env.main.middleware_dispatcher->observe();
            check(group.enabled && group.attached && !group.closed &&
                  dispatcher.groups == 1 && !dispatcher.closed,
                  "same group identity attached to root dispatcher");
            check(!main_binding::initialize_retail_middleware_files(env.main, f) &&
                  f.kind == StartupError::already_initialized,
                  "duplicate file startup refused");
            RetailFilePathFailure pf;
            auto file = env.main.file_paths->open(env.startup.content, "/game/mkda.pak", pf);
            check(bool(file), "actual game alias opens native file");
            std::array<char, 4> data{};
            mka::content::ContentVolumeFailure vf;
            auto bytes = file->read(data.data(), data.size(), vf);
            check(bytes.bytes == 4 && data == std::array<char, 4>{'A','B','C','D'},
                  "actual native file reads bytes");
            check(file->seek(1, mka::content::SeekOrigin::begin, vf), "native seek");
            bytes = file->read(data.data(), 1, vf);
            check(bytes.bytes == 1 && data[0] == 'B', "native reread after seek");
            RetailMiddlewareWorkFailure wf;
            check(env.main.middleware_dispatcher->pump(wf) == 0,
                  "actual attached empty dispatcher pump");
            auto child = std::make_shared<Child>();
            auto ticket = env.main.middleware_work_group->append(child, wf);
            check(bool(ticket), "real child queued in same attached group");
            check(!main_binding::shutdown_retail_middleware_files(env.main, f) &&
                  f.kind == StartupError::middleware && env.main.file_paths &&
                  env.main.middleware_dispatcher && env.main.middleware_work_group->observe().children == 1,
                  "pending child blocks shutdown without dropping owners");
            check(env.main.middleware_dispatcher->pump(wf) == 1 &&
                  child->polls == 1 && child->primary == 1 && child->releases == 1,
                  "actual child completion and release drained");
            check(main_binding::shutdown_retail_middleware_files(env.main, f) && !env.main.file_paths &&
                  !env.main.middleware_dispatcher && env.main.middleware_work_group->observe().closed,
                  "explicit empty teardown detaches and closes owners");
            const auto closed_group = env.main.middleware_work_group;
            bool refused_closed_foreign_teardown = false;
            std::thread closed_other([&] {
                StartupFailure foreign_failure;
                refused_closed_foreign_teardown =
                    !main_binding::shutdown_retail_middleware_files(env.main, foreign_failure) &&
                    foreign_failure.kind == StartupError::middleware &&
                    foreign_failure.middleware.kind == RetailMiddlewareWorkError::wrong_thread &&
                    env.main.middleware_work_group == closed_group &&
                    !env.main.middleware_dispatcher && !env.main.file_paths;
            });
            closed_other.join();
            check(refused_closed_foreign_teardown,
                  "closed owner still rejects foreign-thread teardown without mutation");
            check(main_binding::shutdown_retail_middleware_files(env.main, f), "shutdown idempotence");
        }
        {
            MainFixture env(fixture.root);
            env.ready_crt_and_group();
            StartupFailure f;
            bool refused_on_foreign_thread = false;
            std::thread other([&] {
                refused_on_foreign_thread = !main_binding::initialize_retail_middleware_files(env.main, f) &&
                    f.kind == StartupError::middleware &&
                    f.middleware.kind == RetailMiddlewareWorkError::wrong_thread &&
                    !env.main.file_paths && !env.main.middleware_dispatcher;
            });
            other.join();
            check(refused_on_foreign_thread, "foreign thread refused before publication");
            check(main_binding::initialize_retail_middleware_files(env.main, f),
                  "same owner remains usable after wrong thread");
            check(main_binding::shutdown_retail_middleware_files(env.main, f), "thread fixture teardown");
        }
        {
            MainFixture env(fixture.root);
            env.ready_crt_and_group();
            StartupFailure f;
            RetailCrtFailure cf;
            check(env.startup.crt->shutdown(cf), "CRT closed before file startup");
            check(!main_binding::initialize_retail_middleware_files(env.main, f) &&
                  f.kind == StartupError::crt && !env.main.middleware_dispatcher && !env.main.file_paths,
                  "closed CRT refused without publication");
        }
        {
            MainFixture env(fixture.root);
            env.ready_crt_and_group();
            auto retained_volume = std::move(env.startup.content);
            StartupFailure f;
            check(!main_binding::initialize_retail_middleware_files(env.main, f) &&
                  f.kind == StartupError::content &&
                  f.content.kind == mka::content::ContentVolumeError::moved_volume &&
                  !env.main.middleware_dispatcher && !env.main.file_paths,
                  "moved source volume refused without publication");
        }
        {
            MainFixture env(fixture.root);
            env.ready_crt_and_group();
            RetailMiddlewareWorkFailure wf;
            auto foreign = NativeRetailMiddlewareDispatcher::create(wf);
            check(bool(foreign) && env.main.middleware_work_group->configure(32, wf) &&
                  foreign->attach(env.main.middleware_work_group, wf),
                  "foreign dispatcher owns source group");
            StartupFailure f;
            check(!main_binding::initialize_retail_middleware_files(env.main, f) &&
                  f.kind == StartupError::middleware && !env.main.middleware_dispatcher &&
                  !env.main.file_paths && env.main.middleware_work_group->observe().attached,
                  "foreign attachment refused and retained");
            check(foreign->detach(env.main.middleware_work_group, wf) &&
                  foreign->shutdown(wf), "foreign dispatcher retired explicitly");
            check(main_binding::shutdown_retail_middleware_files(env.main, f), "foreign fixture group retired");
        }
        // Every allocation that startup can reach must either publish all
        // three owners or leave no live dispatcher/group attachment behind.
        std::ptrdiff_t fault_cuts = 0;
        for (;;) {
            MainFixture env(fixture.root);
            env.ready_crt_and_group();
            StartupFailure f;
            allocation_cut = fault_cuts;
            const bool ready = main_binding::initialize_retail_middleware_files(env.main, f);
            allocation_cut = -1;
            if (ready) {
                check(env.main.file_paths && env.main.middleware_dispatcher && env.startup.middleware_frame,
                      "successful fault cut publishes complete startup and frame owner");
            } else {
                check(!env.main.file_paths && !env.main.middleware_dispatcher && !env.startup.middleware_frame &&
                      !env.main.middleware_work_group->observe().attached,
                      "failed fault cut rolls back publication and attachment");
            }
            check(main_binding::shutdown_retail_middleware_files(env.main, f), "fault cut teardown");
            if (ready) break; // Every preceding index forced its real allocation failure.
            check(fault_cuts != std::numeric_limits<std::ptrdiff_t>::max(),
                  "host fault index remains representable");
            ++fault_cuts;
        }
        {
            MainFixture env(fixture.root);
            env.ready_crt_and_group();
            StartupFailure f;
            check(main_binding::initialize_retail_middleware_files(env.main, f), "growth fixture real startup");
            RetailFilePathFailure paths_failure;
            std::vector<mka::content::NativeContentFile> files;
            for (unsigned i = 0; i < 12; ++i) {
                auto file = env.main.file_paths->open(env.startup.content, "/game/mkda.pak", paths_failure);
                check(bool(file), "native file ownership exceeds original four-file reservation");
                files.push_back(std::move(*file));
            }
            RetailMiddlewareWorkFailure work;
            std::vector<std::shared_ptr<Child>> children;
            for (unsigned i = 0; i < 65; ++i) {
                auto child = std::make_shared<Child>();
                check(bool(env.main.middleware_work_group->append(child, work)),
                      "native group grows beyond source32 reserve without a console capacity");
                children.push_back(std::move(child));
            }
            check(env.main.middleware_dispatcher->pump(work) == children.size(),
                  "same attached dispatcher completes all actual native children");
            for (const auto& child : children)
                check(child->polls == 1 && child->primary == 1 && child->releases == 1,
                      "every grown native child completed and released exactly once");
            check(main_binding::shutdown_retail_middleware_files(env.main, f),
                  "grown native work and alias owners retired");
            for (auto& file : files) {
                char byte{};
                auto read = file.read(&byte, 1, paths_failure.content);
                check(read.bytes == 1 && !read.error && byte == 'A',
                      "opened native files survive service retirement without provider fallback");
            }
        }
        std::printf("PASS native middleware file startup: %td allocation cuts, 12 native files, 65 native children; source frame body covered separately; callback producers/game remain open\n", fault_cuts);
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
