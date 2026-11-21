#include "mka/retail_observer_list.hpp"
#include "../runtime/src/native/retail_main_control.hpp"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <thread>
#include <utility>
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
    void cleanup_from_12fa00() override {
        ++cleanups;
        if(transfer) throw mka::retail::detail::NativeMainReentry{nullptr,17};
    }
    std::uint32_t cookie{};
    unsigned cleanups{};
    bool transfer{};
};
struct Callback final : mka::retail::NativeRetailObserverCallback {
    void invoke(const std::shared_ptr<mka::retail::NativeRetailObservedObject>& object) override {
        seen.push_back(object->source_cookie());
    }
    std::vector<std::uint32_t> seen;
};
struct GuardLookup final : mka::retail::NativeRetailObserverCallback {
    mka::retail::NativeRetailObserverList& list;
    std::shared_ptr<Object> guarded, following;
    bool saw{};
    GuardLookup(mka::retail::NativeRetailObserverList& source,
        std::shared_ptr<Object> first,std::shared_ptr<Object> second)
        : list(source), guarded(std::move(first)), following(std::move(second)) {}
    void invoke(const std::shared_ptr<mka::retail::NativeRetailObservedObject>& object) override {
        if(object!=guarded) return;
        guarded->cookie=21;
        mka::retail::RetailObserverListFailure failure;
        auto selected=list.first_live_from_130860(failure);
        check(selected && *selected==following && list.observe(failure)->linked==2,
              "130860 saves next and retains a guarded stale link");
        saw=true;
    }
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
    NativeRetailObserverList lookup(pool);
    auto tail=std::make_shared<Object>(10);
    auto head=std::make_shared<Object>(20);
    auto tail_link=lookup.insert_head_from_12f870(tail,f);
    auto head_link=lookup.insert_head_from_12f870(head,f);
    auto selected=lookup.first_live_from_130860(f);
    check(tail_link && head_link && selected && *selected==head &&
          lookup.observe(f)->linked==2,
          "130860 returns first live head without mutating the following link");
    head->cookie=22;
    selected=lookup.first_live_from_130860(f);
    check(selected && *selected==tail && lookup.observe(f)->linked==1 &&
          pool->observe(pf)->active_records==1 && head->cleanups==0,
          "130860 retires stale head before returning saved-next live object");
    tail->cookie=11;
    selected=lookup.first_live_from_130860(f);
    check(selected && !*selected && f.kind==RetailObserverListError::none &&
          lookup.observe(f)->linked==0 && pool->observe(pf)->active_records==0,
          "130860 reports engaged null after exhausting stale links");

    NativeRetailObserverList guarded_list(pool);
    auto following=std::make_shared<Object>(10);
    auto guarded=std::make_shared<Object>(20);
    auto following_link=guarded_list.insert_head_from_12f870(following,f);
    auto guarded_link=guarded_list.insert_head_from_12f870(guarded,f);
    GuardLookup guard_callback{guarded_list,guarded,following};
    check(following_link && guarded_link &&
          guarded_list.visit_from_12f4e0(guard_callback,f) && guard_callback.saw,
          "nested 130860 sees callback-guarded stale head then next live object");
    selected=guarded_list.first_live_from_130860(f);
    check(selected && *selected==following && guarded_list.observe(f)->linked==1 &&
          guarded_list.retire_link_from_12fa00(following_link,f),
          "unguarded later lookup retires the formerly guarded stale link");

    NativeRetailObserverList owned(pool);
    check(!owned.insert_owned_argument_from_12fee0({},f) &&
          f.kind==RetailObserverListError::none && owned.observe(f)->linked==0,
          "12FEE0 null argument is source no-effect, not a synthetic object");
    auto argument=std::make_shared<Object>(5);
    auto argument_link=owned.insert_owned_argument_from_12fee0(argument,f);
    check(argument_link && owned.first_live_from_130860(f) &&
          *owned.first_live_from_130860(f)==argument,
          "12FEE0 owning argument is returned by source first-live lookup");
    argument->cookie=1;
    selected=owned.first_live_from_130860(f);
    check(selected && !*selected && argument->cleanups==1 &&
          pool->observe(pf)->active_records==0,
          "owning stale cookie-one route invokes virtual cleanup before recycle");

    NativeRetailObserverList transfer_list(pool);
    auto interrupted=std::make_shared<Object>(5);
    interrupted->transfer=true;
    auto interrupted_link=transfer_list.insert_owned_argument_from_12fee0(interrupted,f);
    interrupted->cookie=1;
    bool transferred=false;
    try { (void)transfer_list.first_live_from_130860(f); }
    catch(const detail::NativeMainReentry& event) {
        transferred=event.value==17;
    }
    selected=transfer_list.first_live_from_130860(f);
    check(interrupted_link && transferred && selected && !*selected &&
          interrupted->cleanups==1 && transfer_list.observe(f)->pending==1,
          "130860 propagates native main transfer and never replays nested cleanup");
    std::thread foreign([&] {
        RetailObserverListFailure other;
        check(!list.visit_from_12f4e0(callback,other) &&
              other.kind==RetailObserverListError::wrong_thread,
              "foreign thread refuses callback effects");
        check(!list.first_live_from_130860(other) &&
              other.kind==RetailObserverListError::wrong_thread,
              "foreign thread refuses first-live lookup");
    });
    foreign.join();
    std::puts("native retail observer list: PASS");
}
