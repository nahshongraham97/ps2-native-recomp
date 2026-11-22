#pragma once
#include "mka/retail_main_lifetime.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace mka::retail {
enum class RetailStringDataError {
    none, language, truncated, unsupported_functions, unsupported_tail,
    unsupported_variables, unsupported_type, malformed_name, malformed_reference,
    malformed_string, invalid_index, allocation, native_exception, wrong_thread,
    lifetime, not_ready, retired
};
struct RetailStringDataFailure {
    RetailStringDataError kind{RetailStringDataError::none};
    const char* operation{""};
    std::size_t offset{};
};
struct RetailPermanentStringDescriptor {
    std::string_view name;
    std::uint64_t offset{};
    std::size_t length{};
};
std::optional<RetailPermanentStringDescriptor> permanent_string_descriptor_from_source(
    std::int32_t,RetailStringDataFailure&) noexcept;

// Immutable original glyph bytes, not transcoded Unicode. This value lease
// owns its bytes; a source null and a nonnull empty string remain distinct.
class NativeRetailStringValue final {
public:
    NativeRetailStringValue(const NativeRetailStringValue&)=default;
    NativeRetailStringValue& operator=(const NativeRetailStringValue&)=default;
    // Immutable value leases may share their storage even after a move.
    NativeRetailStringValue(NativeRetailStringValue&& other) noexcept
        :NativeRetailStringValue(static_cast<const NativeRetailStringValue&>(other)) {}
    NativeRetailStringValue& operator=(NativeRetailStringValue&& other) noexcept {
        return *this=static_cast<const NativeRetailStringValue&>(other);
    }
    bool is_null() const noexcept { return null_; }
    std::span<const std::uint8_t> bytes() const & noexcept;
    std::span<const std::uint8_t> bytes() const && = delete;
private:
    friend class NativeRetailPermanentStringImage;
    NativeRetailStringValue(std::shared_ptr<const std::vector<std::uint8_t>>,
        std::size_t,std::size_t,bool) noexcept;
    std::shared_ptr<const std::vector<std::uint8_t>> storage_;
    std::size_t offset_{},length_{};
    bool null_{};
};
class NativeRetailPermanentStringImage final {
public:
    // Actual2170E0/216D80 data-only MKO. No relocated guest pointer or script
    // instruction survives decoding. Unported shapes fail explicitly.
    static std::shared_ptr<const NativeRetailPermanentStringImage> decode(
        std::vector<std::uint8_t>,RetailStringDataFailure&) noexcept;
    std::string_view export_name() const & noexcept;
    std::string_view export_name() const && = delete;
    std::size_t size() const noexcept { return values_.size(); }
    std::span<const std::uint8_t> serialized() const & noexcept { return *storage_; }
    std::span<const std::uint8_t> serialized() const && = delete;
    std::optional<NativeRetailStringValue> value(std::uint64_t,RetailStringDataFailure&) const noexcept;
private:
    NativeRetailPermanentStringImage()=default;
    struct Range {std::size_t offset{},length{};bool null{};};
    std::shared_ptr<const std::vector<std::uint8_t>> storage_;
    std::size_t export_offset_{},export_length_{};
    std::vector<Range> values_;
};
enum class RetailLoadedStringPhase { prepared, reading, ready, failed, retired };
class NativeRetailPermanentStrings;
class NativeRetailGameResourceRegistry;
class NativeRetailLoadedResource final {
public:
    NativeRetailLoadedResource(const NativeRetailLoadedResource&)=delete;
    NativeRetailLoadedResource& operator=(const NativeRetailLoadedResource&)=delete;
    NativeRetailLoadedResource(NativeRetailLoadedResource&&)=delete;
    NativeRetailLoadedResource& operator=(NativeRetailLoadedResource&&)=delete;
    //215180's actual record exists before parsing. The registry owns this
    // real queued object, not a successful parsed fixture or numeric token.
    static std::shared_ptr<NativeRetailLoadedResource> prepare_permanent_strings(
        std::int32_t,RetailStringDataFailure&) noexcept;
    std::string_view source_name() const noexcept { return descriptor_.name; }
    std::int32_t source_tag() const noexcept { return tag_; }
    RetailLoadedStringPhase phase() const noexcept { return phase_; }
    std::optional<NativeRetailStringValue> value(std::uint64_t,RetailStringDataFailure&) const noexcept;
private:
    friend class NativeRetailPermanentStrings;
    friend class NativeRetailGameResourceRegistry;
    explicit NativeRetailLoadedResource(RetailPermanentStringDescriptor d) noexcept :descriptor_(d) {}
    bool access(RetailStringDataFailure&) const noexcept;
    RetailPermanentStringDescriptor descriptor_;
    std::int32_t tag_{-1};
    RetailLoadedStringPhase phase_{RetailLoadedStringPhase::prepared};
    std::weak_ptr<NativeRetailMainLifetime> main_;
    std::shared_ptr<const NativeRetailPermanentStringImage> image_;
    const std::thread::id owner_thread_{std::this_thread::get_id()};
};
class NativeRetailStringTableBinding final {
public:
    std::optional<NativeRetailStringValue> value(std::uint64_t index,RetailStringDataFailure& f) const noexcept {
        return owner_->value(index,f);
    }
private:
    friend class NativeRetailGameResourceRegistry;
    explicit NativeRetailStringTableBinding(std::shared_ptr<const NativeRetailLoadedResource> p) noexcept
        :owner_(std::move(p)) {}
    std::shared_ptr<const NativeRetailLoadedResource> owner_;
};
} // namespace mka::retail
