#include "mka/retail_input_task_argument.hpp"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>
#include <stdexcept>
#include <thread>

using namespace mka::retail;
namespace {
std::atomic<bool> fail_next_allocation{false};
void check(bool ok, const char* what) {
    if (!ok) throw std::runtime_error(what);
}
RetailInputRole role(std::uint8_t identity) {
    return static_cast<RetailInputRole>(identity);
}
}
void* operator new(std::size_t size) {
    if (fail_next_allocation.exchange(false)) throw std::bad_alloc();
    if (auto* p = std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

int main() { try {
    RetailInputTaskArgumentFailure f;
    constexpr auto pad0 = RetailInputTaskArgumentKind::task_2065;
    constexpr auto pad1 = RetailInputTaskArgumentKind::task_2066;
    constexpr auto start = RetailInputTaskArgumentKind::task_208b;
    check(!NativeRetailInputTaskArgument::create(
            static_cast<RetailInputTaskArgumentKind>(0x2067), 0x30, f) &&
          f.kind == RetailInputTaskArgumentError::invalid_kind,
          "only three source task-argument kinds are admitted");
    check(!NativeRetailInputTaskArgument::create(pad0, 0, f) &&
          f.kind == RetailInputTaskArgumentError::invalid_cookie,
          "source 130D00 cookie must come from a real nonzero producer");
    fail_next_allocation = true;
    check(!NativeRetailInputTaskArgument::create(pad0, 0x30, f) &&
          f.kind == RetailInputTaskArgumentError::allocation,
          "argument allocation failure is named before publication");
    check(!fail_next_allocation.load(), "allocation fault was exercised");

    // The supplied nonzero cookies stand for actual 130D00 producer results;
    // this component never increments or derives them itself.
    auto a = NativeRetailInputTaskArgument::create(pad0, 0x30, f);
    auto b = NativeRetailInputTaskArgument::create(pad1, 0x31, f);
    auto c = NativeRetailInputTaskArgument::create(start, 0xf0000000u, f);
    check(a && b && c && f.kind == RetailInputTaskArgumentError::none,
          "three source identities create concrete observed objects");
    auto birth = a->observe(f);
    check(birth && !birth->role && !birth->restore_analog_inhibited &&
          !birth->source_control_word_208b && birth->source_cookie == 0x30 &&
          !birth->retired,
          "source allocation does not invent not-yet-written argument fields");
    check(!a->store_role_from_source(role(4), f) &&
          f.kind == RetailInputTaskArgumentError::invalid_role &&
          !a->observe(f)->role,
          "invalid role identity refuses before write");
    check(a->store_role_from_source(role(0), f) &&
          b->store_role_from_source(role(1), f) &&
          c->store_role_from_source(role(3), f),
          "source role writes retain four-role domain identities");
    check(a->store_restore_analog_inhibited_from_source(true, f) &&
          b->store_restore_analog_inhibited_from_source(false, f) &&
          c->store_restore_analog_inhibited_from_source(true, f),
          "common +0C field records the live bit6 value");
    check(!a->store_control_word_for_208b_from_source(0x12345678u, f) &&
          f.kind == RetailInputTaskArgumentError::incompatible_field &&
          !a->observe(f)->source_control_word_208b,
          "source +10 field is absent from 2065/2066 arguments");
    check(!c->observe(f)->source_control_word_208b &&
          c->store_control_word_for_208b_from_source(0x12345678u, f) &&
          c->observe(f)->source_control_word_208b == 0x12345678u,
          "208B control word appears only after source selection-bit branch");
    check(a->observe(f)->role == role(0) &&
          a->observe(f)->restore_analog_inhibited == true &&
          b->observe(f)->role == role(1) &&
          b->observe(f)->restore_analog_inhibited == false &&
          c->observe(f)->role == role(3) &&
          c->observe(f)->restore_analog_inhibited == true,
          "kind-specific values remain independent");
    auto self = a->self_from_12d860(f);
    check(self.get() == a.get(), "12D860 virtual +4 returns the same object");

    bool foreign_refused = false;
    std::thread foreign([&] {
        RetailInputTaskArgumentFailure other;
        const auto write = a->store_restore_analog_inhibited_from_source(false, other);
        const auto same = a->self_from_12d860(other);
        const auto observation = a->observe(other);
        const auto retired = a->retire_from_12df60(other);
        foreign_refused = !write && !same && !observation && !retired &&
            other.kind == RetailInputTaskArgumentError::wrong_thread;
    });
    foreign.join();
    check(foreign_refused && a->source_cookie() == 0x30 &&
          a->observe(f)->restore_analog_inhibited == true,
          "foreign calls cannot change source cookie or retained fields");

    std::weak_ptr<NativeRetailInputTaskArgument> weak = a;
    std::shared_ptr<NativeRetailObservedObject> observed = a;
    observed->cleanup_from_12fa00();
    check(a->source_cookie() == 0 && a->observe(f)->retired &&
          !a->observe(f)->role && !a->observe(f)->restore_analog_inhibited &&
          !a->self_from_12d860(f) &&
          f.kind == RetailInputTaskArgumentError::retired,
          "12DF60 clears cookie before the owned argument is released");
    check(!a->retire_from_12df60(f) &&
          f.kind == RetailInputTaskArgumentError::retired &&
          !a->store_role_from_source(role(0), f),
          "retired native argument cannot be revived or cleaned twice");
    observed.reset();
    self.reset();
    a.reset();
    check(weak.expired(), "native shared ownership releases argument lifetime");
    check(b->retire_from_12df60(f) && c->retire_from_12df60(f) &&
          b->source_cookie() == 0 && c->source_cookie() == 0,
          "both remaining source kinds invalidate identity on cleanup");
    std::puts("PASS native retail input task argument; real task bodies open");
    return 0;
} catch (const std::exception& e) {
    std::fprintf(stderr, "FAIL native retail input task argument: %s\n", e.what());
    return 1;
} }
