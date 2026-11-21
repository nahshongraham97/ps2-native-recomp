#include "mka/retail_main.hpp"
#include "mka/retail_middleware_work.hpp"
#include "native_retail_startup.hpp"

namespace mka::retail {
namespace {
bool fail(StartupFailure& f, const char* operation) noexcept {
    f.kind = StartupError::middleware;
    f.operation = operation;
    return false;
}

bool fail_work(StartupFailure& f, const char* operation,
               const RetailMiddlewareWorkFailure& work) noexcept {
    f.kind = StartupError::middleware;
    f.operation = operation;
    f.middleware = work;
    return false;
}

// The group and dispatcher have strict live-owner destructors. Roll back in
// reverse order; a refused detach leaves both owners in MainState for an
// explicit later drain, rather than dropping live work.
bool retire_empty(NativeRetailMainState& main, StartupFailure& f) noexcept {
    auto& dispatcher = main.middleware_dispatcher;
    auto& group = main.middleware_work_group;
    RetailMiddlewareWorkFailure work;
    // Validation applies even to closed owners: their identity and publication
    // still belong to the creating thread. Observe() itself is read-only.
    if (dispatcher && !dispatcher->validate_current_thread(work))
        return fail_work(f, "retail middleware dispatcher thread", work);
    if (group && !group->validate_current_thread(work))
        return fail_work(f, "retail middleware work group thread", work);
    if (dispatcher && group) {
        const auto parent = dispatcher->observe();
        const auto child = group->observe();
        if (parent.pumping || parent.pending_completions || child.polling || child.children) {
            f.middleware.kind = RetailMiddlewareWorkError::active;
            return fail(f, "retail middleware work remains pending");
        }
        if (child.attached && !dispatcher->detach(group, work))
            return fail_work(f, "retail middleware detach", work);
    }
    if (group && !group->observe().closed && !group->shutdown(work))
        return fail_work(f, "retail middleware work group shutdown", work);
    if (dispatcher && !dispatcher->observe().closed && !dispatcher->shutdown(work))
        return fail_work(f, "retail middleware dispatcher shutdown", work);
    main.file_paths.reset();
    dispatcher.reset();
    return true;
}
} // namespace

namespace main_binding {
bool initialize_retail_middleware_files(NativeRetailMainState& main,
                                         StartupFailure& f) {
    f = {};
    if (!main.middleware_work_group)
        return fail(f, "retail middleware work group missing");
    RetailMiddlewareWorkFailure work;
    if (!main.middleware_work_group->validate_current_thread(work))
        return fail_work(f, "retail middleware work group thread", work);
    if (main.file_paths || main.middleware_dispatcher) {
        f.kind = StartupError::already_initialized;
        f.operation = "retail middleware files already initialized";
        return false;
    }
    const auto initial_group = main.middleware_work_group->observe();
    if (initial_group.closed || initial_group.enabled || initial_group.polling ||
        initial_group.attached || initial_group.children) {
        f.middleware.kind = initial_group.closed ? RetailMiddlewareWorkError::closed :
            RetailMiddlewareWorkError::active;
        return fail(f, "retail middleware source work group not pristine");
    }
    if (!main.startup.crt) {
        f.kind = StartupError::crt;
        f.operation = "retail middleware CRT owner missing";
        return false;
    }
    if (!main.startup.crt->validate_current_thread(f.crt)) {
        f.kind = StartupError::crt;
        f.operation = "retail middleware CRT thread";
        return false;
    }
    const auto crt = main.startup.crt->observe();
    if (!crt.finalized || crt.closed) {
        f.kind = StartupError::crt;
        f.crt.kind = crt.closed ? RetailCrtError::closed : RetailCrtError::not_finalized;
        f.operation = "retail middleware CRT inactive";
        return false;
    }
    if (!main.startup.content.validate(f.content)) {
        f.kind = StartupError::content;
        f.operation = "retail middleware content volume";
        return false;
    }
    std::unique_ptr<NativeRetailMiddlewareFrame> new_frame;
    if (main.startup.middleware_frame) {
        if (!main.startup.middleware_frame->validate(*main.startup.crt, f.middleware_frame)) {
            f.kind = StartupError::middleware_frame;
            f.operation = "retail middleware retained frame owner";
            return false;
        }
    } else {
        new_frame = NativeRetailMiddlewareFrame::create(*main.startup.crt, f.middleware_frame);
        if (!new_frame) {
            f.kind = StartupError::middleware_frame;
            f.operation = "retail middleware native frame construction";
            return false;
        }
    }
    auto dispatcher = NativeRetailMiddlewareDispatcher::create(work);
    if (!dispatcher)
        return fail_work(f, "retail middleware dispatcher construction", work);
    // Publish the root owner before any operation can engage its strict
    // lifetime. The failure path retires empty owners or retains live ones.
    main.middleware_dispatcher = std::move(dispatcher);
    if (!main.middleware_work_group->configure(32, work)) {
        fail_work(f, "retail middleware work group reserve", work);
        StartupFailure cleanup;
        if (!retire_empty(main, cleanup)) f = cleanup;
        return false;
    }
    if (!main.middleware_dispatcher->attach(main.middleware_work_group, work)) {
        fail_work(f, "retail middleware group attachment", work);
        StartupFailure cleanup;
        if (!retire_empty(main, cleanup)) f = cleanup;
        return false;
    }
    if (!main.middleware_dispatcher->validate_attachment(main.middleware_work_group, work)) {
        fail_work(f, "retail middleware group attachment identity", work);
        StartupFailure cleanup;
        if (!retire_empty(main, cleanup)) f = cleanup;
        return false;
    }
    RetailFilePathFailure paths_failure;
    auto paths = NativeRetailFilePaths::create(paths_failure);
    if (!paths) {
        f.operation = "retail middleware source alias construction";
        switch (paths_failure.kind) {
        case RetailFilePathError::allocation: f.kind = StartupError::allocation; break;
        case RetailFilePathError::host_capacity: f.kind = StartupError::host_capacity; break;
        default: f.kind = StartupError::middleware; break;
        }
        StartupFailure cleanup;
        if (!retire_empty(main, cleanup)) f = cleanup;
        return false;
    }
    main.file_paths = std::move(paths);
    if (new_frame) main.startup.middleware_frame = std::move(new_frame);
    return true;
}

bool shutdown_retail_middleware_files(NativeRetailMainState& main,
                                       StartupFailure& f) noexcept {
    f = {};
    return retire_empty(main, f);
}
} // namespace main_binding
} // namespace mka::retail
