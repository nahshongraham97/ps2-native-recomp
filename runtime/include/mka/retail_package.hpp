#pragma once

#include "mka/content_volume.hpp"
#include "mka/retail_file_paths.hpp"
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
    none, source_path, source_open, source_read, source_seek, source_length,
    invalid_name, duplicate_name,
    missing_member, invalid_read_range, wrong_thread, allocation, host_capacity, native_exception
};
struct RetailPackageFailure {
    RetailPackageError kind{RetailPackageError::none};
    const char* operation{""};
    content::ContentVolumeFailure content{};
    RetailFilePathFailure path{};
};
struct RetailPackageMember {
    std::string name;
    std::uint64_t size{};
};

// Native replacement for the original 219050/2193E0 named asset provider.
// Owns loose files under /game/art. MKDA.PAK is never opened or searched.
class NativeRetailPackage final {
public:
    static std::unique_ptr<NativeRetailPackage> open(
        const content::NativeContentVolume&, const NativeRetailFilePaths&, RetailPackageFailure&) noexcept;
    NativeRetailPackage(const NativeRetailPackage&)=delete;
    NativeRetailPackage& operator=(const NativeRetailPackage&)=delete;

    std::size_t member_count() const noexcept { return members_.size(); }
    // Native basenames compare with ASCII case folding. The verified original
    // \\ps2dvd\\art\\ prefix is decoded only as an incoming asset identifier.
    std::optional<RetailPackageMember> find(std::string_view, RetailPackageFailure&) const noexcept;
    // Read at a member-relative byte offset. The request is clipped at the
    // file end. A failed seek/read never reports success.
    std::optional<std::size_t> read(std::string_view, std::uint64_t,
        std::span<std::uint8_t>, RetailPackageFailure&) noexcept;

private:
    NativeRetailPackage() = default;
    struct Asset {
        RetailPackageMember member;
        content::NativeContentFile file;
    };
    bool build_index(std::vector<content::NativeContentFile>&&, RetailPackageFailure&);
    bool exact_read(Asset&, std::uint64_t, void*, std::size_t, RetailPackageFailure&);
    static std::optional<std::string> key(std::string_view, RetailPackageFailure&);

    const std::thread::id owner_thread_{std::this_thread::get_id()};
    std::vector<Asset> members_;
    std::unordered_map<std::string, std::size_t> by_name_;
};

} // namespace mka::retail
