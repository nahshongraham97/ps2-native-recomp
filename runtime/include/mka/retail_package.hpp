#pragma once

#include "mka/content_volume.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace mka::retail {

enum class RetailPackageError {
    none, source_open, source_read, source_seek, source_length,
    invalid_header, invalid_index, invalid_name, invalid_entry_range,
    missing_member, invalid_read_range, wrong_thread, allocation, host_capacity, native_exception
};
struct RetailPackageFailure {
    RetailPackageError kind{RetailPackageError::none};
    const char* operation{""};
    content::ContentVolumeFailure content{};
};
struct RetailPackageMember {
    std::string name;
    std::uint64_t offset{};
    std::uint64_t size{};
    std::uint32_t source_index{};
};

// Original 219050/2193E0 package owner. The archive handle and its index
// remain owned together; no guest address, disc sector or fixed file buffer.
class NativeRetailPackage final {
public:
    static std::unique_ptr<NativeRetailPackage> open(
        const content::NativeContentVolume&, RetailPackageFailure&) noexcept;
    NativeRetailPackage(const NativeRetailPackage&)=delete;
    NativeRetailPackage& operator=(const NativeRetailPackage&)=delete;

    std::size_t member_count() const noexcept { return members_.size(); }
    // Indexed names compare with ASCII case folding only. Slashes and
    // backslashes remain distinct, as in the original member lookup.
    std::optional<RetailPackageMember> find(std::string_view, RetailPackageFailure&) const noexcept;
    // Read at a member-relative byte offset. The request is clipped at the
    // member end, matching the source member stream read rather than crossing
    // into the next archive object. A failed seek/read never reports success.
    std::optional<std::size_t> read(std::string_view, std::uint64_t,
        std::span<std::uint8_t>, RetailPackageFailure&) noexcept;

private:
    explicit NativeRetailPackage(content::NativeContentFile&& file)
        : file_(std::move(file)) {}
    bool build_index(RetailPackageFailure&);
    bool exact_read(std::uint64_t, void*, std::size_t, RetailPackageFailure&);
    static std::optional<std::string> key(std::string_view, RetailPackageFailure&);

    content::NativeContentFile file_;
    const std::thread::id owner_thread_{std::this_thread::get_id()};
    std::uint64_t file_size_{};
    std::vector<RetailPackageMember> members_;
    std::unordered_map<std::string, std::size_t> by_name_;
};

} // namespace mka::retail
