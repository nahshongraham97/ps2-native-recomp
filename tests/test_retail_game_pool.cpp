#include "mka/retail_game_pool.hpp"

#include <cstdio>
#include <cstdlib>
#include <thread>
#include <utility>
#include <vector>

namespace {
void check(bool yes, const char* label) {
    if (!yes) {
        std::fprintf(stderr, "retail game pool: %s\n", label);
        std::abort();
    }
}
}

int main() {
    using namespace mka::retail;
    RetailGamePoolFailure failure;
    auto pool = NativeRetailGamePool::initialize(3, failure);
    check(bool(pool), "source configured initial record reserve");
    auto born = pool->observe(failure);
    check(born && born->configured_initial_records == 3 &&
              born->native_records == 3 && born->available_records == 3 &&
              born->active_records == 0 && born->first_free_ordinal == 2,
          "130030 prepends initial free nodes in source order");
    std::vector<NativeRetailGamePoolLink> links;
    for (std::size_t ordinal : {2u, 1u, 0u, 3u}) {
        auto lease = pool->acquire_task_link(0x55667788, failure);
        check(lease && lease->native_ordinal() == ordinal &&
                  lease->cached_object_identity() == 0x55667788 &&
                  lease->kind() == RetailGamePoolLinkKind::task_12ffe0,
              "source free order then native dynamic growth");
        links.push_back(std::move(*lease));
    }
    check(pool->observe(failure)->native_records == 4 &&
              pool->observe(failure)->active_records == 4,
          "source initial count is not a host allocation ceiling");
    check(!pool->reset_from_12da70(2, failure) &&
              failure.kind == RetailGamePoolError::active_links,
          "live links cannot disappear on repeat birth");
    std::thread foreign([&] {
        RetailGamePoolFailure other;
        check(!pool->acquire_task_link(9, other) &&
                  other.kind == RetailGamePoolError::wrong_thread,
              "foreign native acquisition refused");
    });
    foreign.join();
    check(links[0].release(failure), "explicit source link recycle");
    auto observer = pool->acquire_observer_link(0x01020304, failure);
    check(observer && observer->native_ordinal() == 2 &&
              observer->kind() == RetailGamePoolLinkKind::observer_12ff70,
          "recycled head reused with observer flag semantics");
    links.clear();
    check(pool->observe(failure)->active_records == 1,
          "retained observer is the only live link");
    pool.reset();
    check(observer->release(failure),
          "real link remains owned after public pool retirement");
    std::puts("native retail game pool: PASS");
}
