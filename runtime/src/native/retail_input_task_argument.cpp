#include "mka/retail_input_task_argument.hpp"

#include <new>
#include <stdexcept>

namespace mka::retail {
namespace {
bool fail(RetailInputTaskArgumentFailure& f,
    RetailInputTaskArgumentError kind, const char* operation) noexcept {
    f.kind = kind;
    f.operation = operation;
    return false;
}
bool known_kind(RetailInputTaskArgumentKind kind) noexcept {
    return kind == RetailInputTaskArgumentKind::task_2065 ||
           kind == RetailInputTaskArgumentKind::task_2066 ||
           kind == RetailInputTaskArgumentKind::task_208b;
}
bool known_role(RetailInputRole role) noexcept {
    return static_cast<std::uint8_t>(role) < 4;
}
}

NativeRetailInputTaskArgument::NativeRetailInputTaskArgument(
    RetailInputTaskArgumentKind kind, std::uint32_t cookie) noexcept
    : kind_(kind), source_cookie_(cookie) {}

std::shared_ptr<NativeRetailInputTaskArgument>
NativeRetailInputTaskArgument::create(RetailInputTaskArgumentKind kind,
    std::uint32_t source_cookie, RetailInputTaskArgumentFailure& f) noexcept {
    f = {};
    if (!known_kind(kind)) {
        fail(f, RetailInputTaskArgumentError::invalid_kind,
             "59FE40 unsupported source task argument kind");
        return {};
    }
    if (source_cookie == 0) {
        fail(f, RetailInputTaskArgumentError::invalid_cookie,
             "130D00 real nonzero source argument cookie missing");
        return {};
    }
    try {
        return std::shared_ptr<NativeRetailInputTaskArgument>(
            new NativeRetailInputTaskArgument(kind, source_cookie));
    } catch (const std::bad_alloc&) {
        fail(f, RetailInputTaskArgumentError::allocation,
             "130D00 native input argument allocation");
    } catch (...) {
        fail(f, RetailInputTaskArgumentError::native_exception,
             "130D00 native input argument construction");
    }
    return {};
}

bool NativeRetailInputTaskArgument::access(
    RetailInputTaskArgumentFailure& f) const noexcept {
    f = {};
    if (std::this_thread::get_id() == owner_thread_) return true;
    return fail(f, RetailInputTaskArgumentError::wrong_thread,
                "59FE40 argument owner thread");
}
bool NativeRetailInputTaskArgument::live_access(
    RetailInputTaskArgumentFailure& f) const noexcept {
    if (!access(f)) return false;
    if (!retired_) return true;
    return fail(f, RetailInputTaskArgumentError::retired,
                "59FE40 source argument already released");
}
bool NativeRetailInputTaskArgument::store_role_from_source(
    RetailInputRole role, RetailInputTaskArgumentFailure& f) noexcept {
    if (!live_access(f)) return false;
    if (!known_role(role))
        return fail(f, RetailInputTaskArgumentError::invalid_role,
                    "59FE40 source role identity outside four game roles");
    role_ = role;
    return true;
}
bool NativeRetailInputTaskArgument::store_restore_analog_inhibited_from_source(
    bool value, RetailInputTaskArgumentFailure& f) noexcept {
    if (!live_access(f)) return false;
    restore_analog_inhibited_ = value;
    return true;
}
bool NativeRetailInputTaskArgument::store_control_word_for_208b_from_source(
    std::uint32_t word, RetailInputTaskArgumentFailure& f) noexcept {
    if (!live_access(f)) return false;
    if (kind_ != RetailInputTaskArgumentKind::task_208b)
        return fail(f, RetailInputTaskArgumentError::incompatible_field,
                    "source control word exists only for 208B argument");
    source_control_word_208b_ = word;
    return true;
}

std::shared_ptr<NativeRetailInputTaskArgument>
NativeRetailInputTaskArgument::self_from_12d860(
    RetailInputTaskArgumentFailure& f) noexcept {
    if (!live_access(f)) return {};
    auto self = weak_from_this().lock();
    if (self) return self;
    fail(f, RetailInputTaskArgumentError::retired,
         "12D860 source argument owner no longer retained");
    return {};
}

bool NativeRetailInputTaskArgument::retire_from_12df60(
    RetailInputTaskArgumentFailure& f) noexcept {
    if (!live_access(f)) return false;
    // 12DF60 zeros +4 before 130CF0 releases the source allocation.
    source_cookie_.store(0, std::memory_order_release);
    retired_ = true;
    role_.reset();
    restore_analog_inhibited_.reset();
    source_control_word_208b_.reset();
    return true;
}

std::optional<RetailInputTaskArgumentObservation>
NativeRetailInputTaskArgument::observe(
    RetailInputTaskArgumentFailure& f) const noexcept {
    if (!access(f)) return {};
    return RetailInputTaskArgumentObservation{
        kind_, source_cookie(), role_, restore_analog_inhibited_,
        source_control_word_208b_, retired_};
}

std::uint32_t NativeRetailInputTaskArgument::source_cookie() const noexcept {
    return source_cookie_.load(std::memory_order_acquire);
}

void NativeRetailInputTaskArgument::cleanup_from_12fa00() {
    RetailInputTaskArgumentFailure f;
    if (!retire_from_12df60(f))
        throw std::logic_error(f.operation);
}

} // namespace mka::retail
