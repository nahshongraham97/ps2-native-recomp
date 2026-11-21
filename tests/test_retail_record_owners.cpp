#include "mka/retail_record_owners.hpp"
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <new>
#include <stdexcept>
#include <thread>
#include <vector>
using namespace mka::retail;
namespace {
int allocation_budget = -1;
void check(bool value, const char* text) { if (!value) throw std::runtime_error(text); }
}
namespace mka::retail {
// Test-only request backing. No retail async operation or callback is faked.
class NativeRetailNamedRecordRequest final {
public:
    std::weak_ptr<NativeRetailRecordOwners> owner;
    std::size_t expected_occupied{};
    bool* release_observed{};
    ~NativeRetailNamedRecordRequest() {
        if (!release_observed) return;
        RetailRecordOwnerFailure f;
        auto live = owner.lock();
        const auto state = live ? live->observe(f) : std::nullopt;
        *release_observed = state && state->occupied == expected_occupied;
    }
};
}
void* operator new(std::size_t n) {
    if (allocation_budget == 0) throw std::bad_alloc();
    if (allocation_budget > 0) --allocation_budget;
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
int main() {
    std::set_terminate([] { std::fputs("FAIL record: pending fixture ownership during unwind\n", stderr); std::fflush(stderr); std::_Exit(3); });
    try {
        RetailRecordOwnerFailure f;
        unsigned factory_cuts = 0;
        for (int budget = 0; budget < 32; ++budget) {
            allocation_budget = budget;
            auto candidate = NativeRetailRecordOwners::create(f);
            allocation_budget = -1;
            if (candidate) break;
            check(f.kind == RetailRecordOwnerError::allocation ||
                (f.kind == RetailRecordOwnerError::descriptor && f.descriptor.kind == RetailDispatchOwnerError::allocation),
                "factory failure preserves direct or nested allocation diagnosis");
            ++factory_cuts;
        }
        check(factory_cuts >= 6 && factory_cuts < 32, "all actual constructor allocation cuts reached before success");
        auto owners = NativeRetailRecordOwners::create(f);
        auto foreign = NativeRetailRecordOwners::create(f);
        check(owners && foreign && owners->observe(f)->workspaces == 30 && owners->observe(f)->occupied == 0,
            "actual original constructor count and empty occupancy");
        check(&owners->first_prefix_descriptor() != &owners->second_prefix_descriptor() &&
            !owners->first_prefix_descriptor().configured() && !owners->second_prefix_descriptor().configured(),
            "two independent unbound prefix owners");
        check(make_retail_named_record_key("", 1, f) == "/n//r/1" &&
            make_retail_named_record_key("profile", INT_MIN, f) == "/n/profile/r/-2147483648" &&
            make_retail_named_record_key("profile", INT_MAX, f) == "/n/profile/r/2147483647", "exact signed service keys");
        check(!make_retail_named_record_key(std::string_view("a\0b", 3), 1, f) &&
            f.kind == RetailRecordOwnerError::invalid_name, "sized embedded NUL is malformed");
        auto request = std::make_shared<NativeRetailNamedRecordRequest>();
        check(!owners->retain_request({}, f) && f.kind == RetailRecordOwnerError::invalid_request, "null request refused");
        allocation_budget = 0;
        auto no_ticket = owners->retain_request(request, f);
        allocation_budget = -1;
        check(!no_ticket && f.kind == RetailRecordOwnerError::allocation && owners->observe(f)->occupied == 0,
            "failed occurrence allocation publishes no request");
        auto ticket = owners->retain_request(request, f);
        check(ticket.has_value(), "real backing retained");
        check(!owners->retain_request(request, f) && f.kind == RetailRecordOwnerError::invalid_request,
            "same in-flight operation cannot occupy another workspace");
        check(!foreign->release_after_callback_ack(*ticket, f) && f.kind == RetailRecordOwnerError::foreign_ticket,
            "foreign owner cannot consume acknowledgment");
        bool observed = false;
        request->owner = owners; request->release_observed = &observed;
        std::weak_ptr<NativeRetailNamedRecordRequest> weak = request; request.reset();
        check(!weak.expired(), "ticket does not replace actual operation ownership");
        check(owners->release_after_callback_ack(*ticket, f) && weak.expired() && observed,
            "slot is vacant before final request destructor reenters observation");
        auto reused = owners->retain_request(std::make_shared<NativeRetailNamedRecordRequest>(), f);
        check(reused && !owners->release_after_callback_ack(*ticket, f) && f.kind == RetailRecordOwnerError::stale_ticket &&
            owners->observe(f)->occupied == 1, "old acknowledgment cannot release a reused workspace");
        check(owners->release_after_callback_ack(*reused, f), "actual current acknowledgment retires current owner");
        std::vector<NativeRetailRecordTicket> tickets; tickets.reserve(96);
        for (unsigned i = 0; i < 30; ++i) {
            auto next = owners->retain_request(std::make_shared<NativeRetailNamedRecordRequest>(), f);
            check(next.has_value(), "all original workspaces own distinct requests"); tickets.push_back(*next);
        }
        auto extra_request = std::make_shared<NativeRetailNamedRecordRequest>();
        allocation_budget = 1; // Occurrence succeeds; vector growth allocation fails.
        auto no_growth = owners->retain_request(extra_request, f);
        allocation_budget = -1;
        check(!no_growth && f.kind == RetailRecordOwnerError::allocation && owners->observe(f)->workspaces == 30 &&
            owners->observe(f)->occupied == 30, "growth failure leaves prior workspaces and owners unchanged");
        for (unsigned i = 30; i < 96; ++i) {
            auto next = owners->retain_request(std::make_shared<NativeRetailNamedRecordRequest>(), f);
            check(next.has_value(), "host ownership grows beyond original initialized count"); tickets.push_back(*next);
        }
        check(owners->observe(f)->occupied == 96, "no original thirty-request hardware ceiling");
        bool wrong_thread = false;
        std::thread reader([&] {
            RetailRecordOwnerFailure other;
            wrong_thread = !owners->observe(other) && other.kind == RetailRecordOwnerError::wrong_thread &&
                !owners->release_after_callback_ack(tickets[0], other) && other.kind == RetailRecordOwnerError::wrong_thread;
        }); reader.join(); check(wrong_thread, "foreign thread cannot observe or release live work");
        for (const auto& next : tickets) check(owners->release_after_callback_ack(next, f), "drain all actual fixture backing");
        check(owners->observe(f)->occupied == 0, "native owner safely retires only after all work acknowledged");
        std::puts("PASS native named-record ownership/failure/growth checks; no profile provider, game or endpoint");
        return 0;
    } catch (const std::exception& e) {
        allocation_budget = -1; std::fprintf(stderr, "FAIL record: %s\n", e.what()); return 1;
    }
}
