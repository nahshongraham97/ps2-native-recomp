#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace mka::save {

enum class SaveError {
    none, unsupported_host, invalid_root, root_changed, invalid_name,
    unsafe_name, name_alias, missing, not_regular_file, enumerate_failed,
    open_failed, read_failed, write_failed, flush_failed, replace_failed,
    delete_failed, after_effect, allocation_failed, host_capacity,
    moved_store, native_exception
};
struct SaveFailure {
    SaveError kind = SaveError::none;
    std::error_code host;
};

// Native profile bytes. The retail payload schema and game profile defaults
// are supplied by their own source-checked owner; this service never parses
// or invents them. Names are single UTF-8 file components with exact spelling.
class NativeSaveStore final {
public:
    NativeSaveStore(const NativeSaveStore&) = delete;
    NativeSaveStore& operator=(const NativeSaveStore&) = delete;
    NativeSaveStore(NativeSaveStore&&) noexcept;
    NativeSaveStore& operator=(NativeSaveStore&&) noexcept;
    ~NativeSaveStore() noexcept;

    static std::optional<NativeSaveStore> open_or_create(
        const std::filesystem::path& absolute_directory, SaveFailure&) noexcept;
    bool validate(SaveFailure&) const noexcept;
    bool list(std::vector<std::string>& names, SaveFailure&) const noexcept;
    bool read(std::string_view name, std::vector<std::uint8_t>& bytes,
        SaveFailure&) const noexcept;
    bool replace(std::string_view name, std::span<const std::uint8_t> bytes,
        SaveFailure&) const noexcept;
    bool erase(std::string_view name, SaveFailure&) const noexcept;

private:
    struct State;
    explicit NativeSaveStore(std::unique_ptr<State> state) noexcept;
    std::unique_ptr<State> state_;
};

} // namespace mka::save
