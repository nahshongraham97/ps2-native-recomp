#include "mka/retail_observer_list.hpp"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <thread>
#include <vector>

namespace {
void check(bool yes,const char* label) {
    if(!yes) {
        std::fprintf(stderr,"retail observer list: %s\n",label);
        std::abort();
    }
}
struct Object final : mka::retail::NativeRetailObservedObject {
    explicit Object(std::uint32_t source_cookie) : cookie(source_cookie) {}
    std::uint32_t source_cookie() const noexcept override { return cookie; }
    void cleanup_from_12fa00() override { ++cleanups; }
    std::uint32_t cookie{};
    unsigned cleanups{};
};
struct Callback final : mka::retail::NativeRetailObserverCallback {
    void invoke(const std::shared_ptr<mka::retail::NativeRetailObservedObject>& object) override {
        seen.push_back(object->source_cookie());
    }
    std::vector<std::uint32_t> seen;
};
}

int main() {
    using namespace mka::retail;
    RetailGamePoolFailure pf;
    std::shared_ptr<NativeRetailGamePool> pool(
        NativeRetailGamePool::initialize(2,pf));
    check(bool(pool),"real shared game pool birth");
    NativeRetailObserverList list(pool);
    RetailObserverListFailure f;
    auto first=std::make_shared<Object>(5);
    auto second=std::make_shared<Object>(6);
    auto first_link=list.insert_head_from_12f870(first,f);
    auto second_link=list.insert_head_from_12f870(second,f);
    check(first_link && second_link && pool->observe(pf)->active_records==2,
          "two real observer leases and head insertion");
    Callback callback;
    check(list.visit_from_12f4e0(callback,f) &&
          callback.seen==std::vector<std::uint32_t>({6,5}),
          "source head order and callback invocation");
    first->cookie=7;
    callback.seen.clear();
    check(list.visit_from_12f4e0(callback,f) &&
          callback.seen==std::vector<std::uint32_t>({6}) &&
          list.observe(f)->linked==1 &&
          pool->observe(pf)->active_records==1 && first->cleanups==0,
          "stale cached cookie retires and recycles without cleanup replay");
    check(list.retire_link_from_12fa00(second_link,f) &&
          list.observe(f)->linked==0 && pool->observe(pf)->active_records==0,
          "explicit source retirement recycles final observer lease");
    std::thread foreign([&] {
        RetailObserverListFailure other;
        check(!list.visit_from_12f4e0(callback,other) &&
              other.kind==RetailObserverListError::wrong_thread,
              "foreign thread refuses callback effects");
    });
    foreign.join();
    std::puts("native retail observer list: PASS");
}
