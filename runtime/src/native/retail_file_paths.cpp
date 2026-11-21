#include "mka/retail_file_paths.hpp"

#include <algorithm>
#include <array>
#include <new>
#include <stdexcept>
#include <utility>

namespace mka::retail {
namespace {

constexpr std::array<std::pair<std::string_view, std::string_view>, 8> source_aliases{{
    {"game", "/cdrom"},
    {"art", "/game/art"},
    {"sounds", "/game/sndsps2"},
    {"logs", "/game/logs"},
    {"movies", "/game/movieps2"},
    {"kryptmovies", "/game/movieps2/krypt"},
    {"anims", "/game/anims"},
    {"mko", "/game/mko"},
}};

unsigned char fold(unsigned char c) noexcept {
    return c >= 'A' && c <= 'Z' ? static_cast<unsigned char>(c + ('a' - 'A')) : c;
}

int compare_alias(std::string_view a, std::string_view b) noexcept {
    const auto count = std::min(a.size(), b.size());
    for (std::size_t i = 0; i < count; ++i) {
        const auto x = fold(static_cast<unsigned char>(a[i]));
        const auto y = fold(static_cast<unsigned char>(b[i]));
        if (x != y) return x < y ? -1 : 1;
    }
    return a.size() == b.size() ? 0 : (a.size() < b.size() ? -1 : 1);
}

} // namespace

std::optional<std::string> NativeRetailFilePaths::resolve(
    std::string_view path, RetailFilePathFailure& failure) const {
    if (path.empty() || path.front() != '/' || path.find('\0') != path.npos) {
        failure.kind = RetailFilePathError::invalid_source_path;
        return {};
    }
    path.remove_prefix(1);
    const auto separator = path.find('/');
    const auto name = path.substr(0, separator);
    if (name.empty()) {
        failure.kind = RetailFilePathError::unknown_alias;
        return {};
    }
    const auto tail = separator == path.npos ? std::string_view{} : path.substr(separator + 1);

    // 32C970 installs a `cdrom` provider before 2DF470 adds its eight aliases.
    // Its native destination is the already opened extracted content volume.
    std::string prefix;
    if (compare_alias(name, "cdrom") != 0) {
        const auto found = std::lower_bound(aliases_.begin(), aliases_.end(), name,
            [](const Alias& alias, std::string_view wanted) {
                return compare_alias(alias.name, wanted) < 0;
            });
        if (found == aliases_.end() || compare_alias(found->name, name) != 0) {
            failure.kind = RetailFilePathError::unknown_alias;
            return {};
        }
        prefix = found->volume_prefix;
    }

    if (!tail.empty()) {
        if (!prefix.empty()) prefix.push_back('/');
        prefix.append(tail);
    }
    return prefix;
}

bool NativeRetailFilePaths::add_source_alias(
    std::string_view name, std::string_view target, RetailFilePathFailure& failure) {
    if (name.empty()) {
        failure.kind = RetailFilePathError::invalid_source_path;
        return false;
    }
    // Retail 328120 resolves the target before attempting sorted insertion.
    auto prefix = resolve(target, failure);
    if (!prefix) return false;
    const auto found = std::lower_bound(aliases_.begin(), aliases_.end(), name,
        [](const Alias& alias, std::string_view wanted) {
            return compare_alias(alias.name, wanted) < 0;
        });
    if ((found != aliases_.end() && compare_alias(found->name, name) == 0) ||
        compare_alias(name, "cdrom") == 0) {
        failure.kind = RetailFilePathError::duplicate_alias;
        return false;
    }
    aliases_.insert(found, Alias{std::string(name), std::move(*prefix)});
    return true;
}

std::unique_ptr<NativeRetailFilePaths> NativeRetailFilePaths::create(
    RetailFilePathFailure& failure) noexcept {
    failure = {};
    try {
        auto result = std::unique_ptr<NativeRetailFilePaths>(new NativeRetailFilePaths);
        for (const auto& [name, target] : source_aliases)
            if (!result->add_source_alias(name, target, failure)) return {};
        return result;
    } catch (const std::bad_alloc&) {
        failure.kind = RetailFilePathError::allocation;
    } catch (const std::length_error&) {
        failure.kind = RetailFilePathError::host_capacity;
    } catch (...) {
        failure.kind = RetailFilePathError::native_exception;
    }
    return {};
}

std::optional<content::NativeContentFile> NativeRetailFilePaths::open(
    const content::NativeContentVolume& volume, std::string_view retail_path,
    RetailFilePathFailure& failure) const noexcept {
    failure = {};
    try {
        auto key = resolve(retail_path, failure);
        if (!key) return {};
        if (key->empty()) {
            failure.kind = RetailFilePathError::empty_file_key;
            return {};
        }
        auto file = volume.open_resource(*key, failure.content);
        if (!file) failure.kind = RetailFilePathError::content_file;
        return file;
    } catch (const std::bad_alloc&) {
        failure.kind = RetailFilePathError::allocation;
    } catch (const std::length_error&) {
        failure.kind = RetailFilePathError::host_capacity;
    } catch (...) {
        failure.kind = RetailFilePathError::native_exception;
    }
    return {};
}

} // namespace mka::retail
