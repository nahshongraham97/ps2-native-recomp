#include "mka/retail_middleware_frame.hpp"
#include "mka/retail_middleware_work.hpp"
#include "../runtime/src/native/retail_main_control.hpp"

#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace mka::retail;
namespace {
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
struct PublishingChild final : NativeRetailMiddlewareChild {
    NativeRetailMiddlewareFrame& frame;
    std::vector<unsigned>& calls;
    explicit PublishingChild(NativeRetailMiddlewareFrame& f, std::vector<unsigned>& c)
        : frame(f), calls(c) {}
    std::optional<RetailMiddlewarePollStatus> poll(RetailMiddlewareWorkFailure&) override {
        return RetailMiddlewarePollStatus::transfer_to_parent;
    }
    bool primary_complete(RetailMiddlewareWorkFailure& work) override {
        RetailMiddlewareFrameFailure f;
        const bool ready = frame.install_callback(
            [this](std::uint8_t argument) { calls.push_back(argument); }, f);
        if (!ready) work.kind = RetailMiddlewareWorkError::child_failed;
        return ready;
    }
    std::optional<bool> release_required(RetailMiddlewareWorkFailure&) const override {
        return false;
    }
    std::optional<RetailMiddlewareReleaseResult> release(RetailMiddlewareWorkFailure&) override {
        throw std::runtime_error("release must not run");
    }
};
}

int main() {
    try {
        RetailCrtFailure cf;
        auto crt = NativeRetailCrt::create(cf);
        check(bool(crt) && crt->finalize_pre_main(cf), "actual native CRT finalized");
        RetailMiddlewareFrameFailure f;
        auto frame = NativeRetailMiddlewareFrame::create(*crt, f);
        check(bool(frame), "persistent native frame owner bound to CRT");
        auto first = frame->sample(*crt, f);
        check(first && first->nanoseconds_since_first == 0 &&
              first->nanoseconds_since_previous == 0 &&
              first->milliseconds_since_first == 0,
              "first native sample establishes logical time zero");
        auto second = frame->sample(*crt, f);
        check(second && second->nanoseconds_since_first >= first->nanoseconds_since_first &&
              second->nanoseconds_since_previous <= second->nanoseconds_since_first,
              "later native CRT sample is monotonic without Count wrapping");
        auto age = frame->elapsed_nanoseconds(*first, *second, f);
        check(age && *age == second->nanoseconds_since_first,
              "typed samples compare within the same native time owner");

        auto second_frame = NativeRetailMiddlewareFrame::create(*crt, f);
        check(bool(second_frame), "same CRT can have a separate logical origin");
        auto foreign = second_frame->sample(*crt, f);
        check(foreign && !frame->elapsed_nanoseconds(*first, *foreign, f) &&
              f.kind == RetailMiddlewareFrameError::foreign_sample,
              "foreign logical time mark refused");
        auto other_crt = NativeRetailCrt::create(cf);
        check(bool(other_crt) && other_crt->finalize_pre_main(cf), "second real CRT");
        check(!frame->sample(*other_crt, f) &&
              f.kind == RetailMiddlewareFrameError::wrong_clock,
              "different CRT identity refused");
        bool foreign_thread = false;
        std::thread other([&] {
            RetailMiddlewareFrameFailure tf;
            foreign_thread = !frame->sample(*crt, tf) &&
                tf.kind == RetailMiddlewareFrameError::wrong_thread;
        });
        other.join();
        check(foreign_thread, "frame owner refuses foreign-thread sampling");

        check(NativeRetailMiddlewareFrame::logical_milliseconds(99999999u) == 99 &&
              !NativeRetailMiddlewareFrame::reached_100_milliseconds(99999999u) &&
              NativeRetailMiddlewareFrame::logical_milliseconds(100000000u) == 100 &&
              NativeRetailMiddlewareFrame::reached_100_milliseconds(100000000u),
              "source transport threshold is logical 100 milliseconds");
        constexpr std::uint64_t long_duration = std::uint64_t{1} << 55;
        check(NativeRetailMiddlewareFrame::logical_milliseconds(long_duration) >
                  std::numeric_limits<std::uint32_t>::max() &&
              NativeRetailMiddlewareFrame::reached_100_milliseconds(long_duration),
              "native logical duration exceeds 32-bit console counter range");

        check(frame->has_callback(f) == std::optional<bool>{false} &&
              frame->invoke_after_dispatcher(*crt, 0, f),
              "BSS-born empty optional callback follows null branch");
        check(!frame->install_callback({}, f) &&
              f.kind == RetailMiddlewareFrameError::invalid_callback,
              "empty callable cannot impersonate a source callback");

        RetailMiddlewareWorkFailure work;
        auto group = NativeRetailMiddlewareWorkGroup::create(work);
        auto dispatcher = NativeRetailMiddlewareDispatcher::create(work);
        check(group && dispatcher && group->configure(32, work) &&
              dispatcher->attach(group, work) &&
              dispatcher->validate_attachment(group, work),
              "real middleware group and dispatcher attached");
        std::vector<unsigned> calls;
        auto child = std::make_shared<PublishingChild>(*frame, calls);
        check(bool(group->append(child, work)), "actual child queued");
        check(frame->has_callback(f) == std::optional<bool>{false},
              "callback absent before dispatcher completion");
        check(dispatcher->pump(work) == std::optional<std::size_t>{1},
              "actual completion publishes callback");
        check(frame->invoke_after_dispatcher(*crt, 0, f) &&
              calls == std::vector<unsigned>{0},
              "fresh post-dispatch selection invokes newly published callback");
        check(dispatcher->detach(group, work) && group->shutdown(work) &&
              dispatcher->shutdown(work), "real dispatcher and group retired");

        std::weak_ptr<int> selected_lifetime;
        {
            auto retained = std::make_shared<int>(7);
            selected_lifetime = retained;
            check(frame->install_callback(
                [&, retained](std::uint8_t argument) {
                    calls.push_back(argument);
                    RetailMiddlewareFrameFailure nested_failure;
                    check(frame->install_callback(
                        [&](std::uint8_t nested) { calls.push_back(nested); },
                        nested_failure), "callback publishes replacement");
                    check(!selected_lifetime.expired(),
                          "selected callback remains alive after replacement");
                    check(frame->invoke_after_dispatcher(*crt, 1, nested_failure),
                          "nested invocation reads replacement callback");
                }, f), "first callback installed");
        }
        check(frame->invoke_after_dispatcher(*crt, 0, f) &&
              calls == std::vector<unsigned>{0, 0, 1} &&
              selected_lifetime.expired(),
              "selected callable retained through nested replacement then released");
        check(frame->clear_callback(f) &&
              frame->has_callback(f) == std::optional<bool>{false},
              "native callback slot can return to empty");

        check(frame->install_callback(
            [](std::uint8_t value) { throw detail::NativeMainReentry{nullptr,static_cast<std::uint32_t>(value + 9)}; }, f),
            "typed nonlocal control callback installed");
        bool propagated = false;
        try { (void)frame->invoke_after_dispatcher(*crt, 1, f); }
        catch (const detail::NativeMainReentry& transfer) { propagated = transfer.owner == nullptr && transfer.value == 10; }
        check(propagated, "typed callback control transfer propagates unchanged");
        check(frame->clear_callback(f), "callback retired after control transfer");

        check(frame->install_callback([](std::uint8_t) {
            throw std::runtime_error("observable native callback failure");
        },f),"ordinary failing callback installed");
        check(!frame->invoke_after_dispatcher(*crt,0,f)&&
              f.kind==RetailMiddlewareFrameError::native_exception,
              "ordinary callback exception becomes named failure");
        check(frame->install_callback([](std::uint8_t) {throw std::bad_alloc();},f)&&
              !frame->invoke_after_dispatcher(*crt,0,f)&&
              f.kind==RetailMiddlewareFrameError::allocation,
              "callback allocation exception becomes named failure");
        check(frame->clear_callback(f),"failing callback retired without loss of owner");

        check(crt->shutdown(cf), "real CRT closed");
        check(!frame->sample(*crt, f) && f.kind == RetailMiddlewareFrameError::crt &&
              f.crt.kind == RetailCrtError::closed,
              "closed bound CRT cannot supply another sample");
        std::puts("PASS native middleware frame time and typed callback component");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }
}
