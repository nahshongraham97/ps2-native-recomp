#include "mka/retail_dispatch_owners.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>
#include <stdexcept>
using namespace mka::retail;
namespace {
bool fail_allocation = false;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
struct Callback final : NativeRetailPrefixCallback {
    void invoke(const NativeRetailDispatchInvocation&) override { ++calls; }
    unsigned calls{};
};
struct Metadata final : NativeRetailDispatchDescriptorData {};
struct Watch {
    NativeRetailPrefixBinding* target{};
    NativeRetailPrefixCallback* callback{};
    const NativeRetailDispatchDescriptorData* metadata{};
    std::byte prefix{};
    bool seen{}, coherent{};
};
struct WatchedCallback final : NativeRetailPrefixCallback {
    explicit WatchedCallback(Watch& value) : watch(value) {}
    void invoke(const NativeRetailDispatchInvocation&) override {}
    ~WatchedCallback() {
        watch.seen = true;
        watch.coherent = watch.target && watch.target->callback().get() == watch.callback &&
            watch.target->copied_descriptor_data().get() == watch.metadata &&
            watch.target->prefix_bytes().size() == 1 && watch.target->prefix_bytes()[0] == watch.prefix;
    }
    Watch& watch;
};
}
void* operator new(std::size_t n) {
    if (fail_allocation) throw std::bad_alloc();
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
int main() { try {
    RetailDispatchOwnerFailure f;
    fail_allocation = true;
    auto failed = NativeRetailPrefixDispatchDescriptor::create(f);
    fail_allocation = false;
    check(!failed && f.kind == RetailDispatchOwnerError::allocation, "factory reports allocation failure");
    auto descriptor = NativeRetailPrefixDispatchDescriptor::create(f);
    check(descriptor && !descriptor->configured() && !descriptor->snapshot(f) &&
        f.kind == RetailDispatchOwnerError::unbound_callback, "unbound constructor cannot masquerade as registered callback");
    descriptor->source_default_operation();
    auto callback = std::make_shared<Callback>();
    auto metadata = std::make_shared<Metadata>();
    std::array prefix{std::byte{0x65}};
    check(descriptor->configure(callback, prefix, metadata, f), "actual configured ownership");
    auto first = descriptor->snapshot(f);
    check(first && first->matches_prefix(prefix) && !first->matches_prefix({}), "prefix length respected");
    prefix[0] = std::byte{0x23};
    check(first->prefix_bytes()[0] == std::byte{0x65}, "snapshot owns immutable bytes");
    check(!descriptor->configure({}, prefix, {}, f) && f.kind == RetailDispatchOwnerError::unbound_callback,
        "invalid replacement leaves original live");
    fail_allocation = true;
    auto no_snapshot = descriptor->snapshot(f);
    fail_allocation = false;
    check(!no_snapshot && f.kind == RetailDispatchOwnerError::allocation && descriptor->configured(),
        "snapshot failure leaves actual descriptor intact");
    descriptor.reset(); first.reset(); callback.reset(); metadata.reset();
    descriptor = NativeRetailPrefixDispatchDescriptor::create(f);
    auto first_callback = std::make_shared<Callback>();
    auto second_callback = std::make_shared<Callback>();
    metadata = std::make_shared<Metadata>();
    const std::array a{std::byte{0x65}}, b{std::byte{0x23}};
    check(descriptor->configure(first_callback, a, metadata, f), "initial binding");
    first = descriptor->snapshot(f);
    fail_allocation = true;
    const bool configured = descriptor->configure(second_callback, b, {}, f);
    fail_allocation = false;
    check(!configured && f.kind == RetailDispatchOwnerError::allocation, "fallible prefix copy reports failure");
    auto unchanged = descriptor->snapshot(f);
    check(unchanged && unchanged->callback() == first_callback && unchanged->matches_prefix(a) &&
        unchanged->copied_descriptor_data() == metadata, "failed configure commits no field");
    check(descriptor->configure(second_callback, b, {}, f), "real replacement");
    auto replacement = descriptor->snapshot(f);
    bool threw = false;
    fail_allocation = true;
    try { *replacement = *first; } catch (const std::bad_alloc&) { threw = true; }
    fail_allocation = false;
    check(threw && replacement->callback() == second_callback && replacement->matches_prefix(b) &&
        !replacement->copied_descriptor_data(), "failed copy assignment cannot mix binding identities");
    *replacement = *first;
    *replacement = *replacement;
    *replacement = std::move(*replacement);
    check(replacement->callback() == first_callback && replacement->matches_prefix(a), "self assignments preserve identity");
    std::weak_ptr<Callback> lifetime = first_callback;
    first_callback.reset(); descriptor.reset(); first.reset(); unchanged.reset();
    check(!lifetime.expired(), "independent copied registration retains callback and metadata");
    replacement.reset(); check(lifetime.expired(), "last registration releases exact callback");
    Watch watch;
    auto watched = NativeRetailPrefixDispatchDescriptor::create(f);
    check(watched->configure(std::make_shared<WatchedCallback>(watch), b, {}, f), "watched old binding");
    auto target = watched->snapshot(f); watched.reset();
    auto fresh = NativeRetailPrefixDispatchDescriptor::create(f);
    check(fresh->configure(second_callback, a, metadata, f), "new coherent binding");
    auto source = fresh->snapshot(f);
    watch.target = &*target; watch.callback = second_callback.get();
    watch.metadata = metadata.get(); watch.prefix = a[0];
    *target = std::move(*source);
    check(watch.seen && watch.coherent, "old callback deleter observes all new fields committed");
    std::array<std::byte, 64> large{};
    check(fresh->configure(second_callback, large, {}, f) && fresh->snapshot(f)->matches_prefix(large),
        "serialized prefix size is not a native storage ceiling");
    check(fresh->configure(second_callback, {}, {}, f) && fresh->snapshot(f)->matches_prefix({}), "configured empty prefix");
    check(second_callback->calls == 0, "ownership fixtures do not invent decoded message invocation");
    std::puts("PASS native prefix copy/failure/lifetime checks; no message parser, game or endpoint");
    return 0;
} catch (const std::exception& e) {
    fail_allocation = false; std::fprintf(stderr, "FAIL prefix: %s\n", e.what()); return 1;
}}
