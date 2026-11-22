#include "mka/retail_package.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"

#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace mka::retail {
namespace {
constexpr std::string_view source_request = "/game/art";
constexpr std::string_view retail_prefix = "\\ps2dvd\\art\\";

RetailPackageError content_error(content::ContentVolumeError kind) noexcept {
    using C = content::ContentVolumeError;
    if (kind == C::allocation_failed) return RetailPackageError::allocation;
    if (kind == C::host_capacity) return RetailPackageError::host_capacity;
    if (kind == C::native_exception) return RetailPackageError::native_exception;
    if (kind == C::length_failed) return RetailPackageError::source_length;
    if (kind == C::seek_failed) return RetailPackageError::source_seek;
    if (kind == C::read_failed) return RetailPackageError::source_read;
    return RetailPackageError::source_open;
}
}

std::optional<std::string> NativeRetailPackage::key(std::string_view raw, RetailPackageFailure& f) {
    if (raw.empty() || raw.find('\0') != raw.npos) {
        f.kind = RetailPackageError::invalid_name;
        f.operation = "retail package member name";
        return {};
    }
    std::string result;
    result.reserve(raw.size());
    for (unsigned char ch : raw) {
        if (ch > 0x7f || ch < 0x20 || ch == 0x7f) {
            f.kind = RetailPackageError::invalid_name;
            f.operation = "retail package member ASCII name";
            return {};
        }
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<unsigned char>(ch + ('a' - 'A'));
        result.push_back(static_cast<char>(ch));
    }
    if (result.starts_with(retail_prefix)) result.erase(0, retail_prefix.size());
    if (result.empty() || result == "." || result == ".." ||
        result.find_first_of("/\\:") != result.npos) {
        f.kind = RetailPackageError::invalid_name;
        f.operation = "native loose asset basename";
        return {};
    }
    return result;
}

bool NativeRetailPackage::exact_read(Asset& asset, std::uint64_t offset,
    void* dst, std::size_t count, RetailPackageFailure& f) {
    if (offset > asset.member.size || count > asset.member.size - offset ||
        offset > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        f.kind = RetailPackageError::invalid_read_range;
        f.operation = "native loose asset bounded read";
        return false;
    }
    if (!asset.file.seek(static_cast<std::int64_t>(offset), content::SeekOrigin::begin, f.content)) {
        f.kind = content_error(f.content.kind);
        f.operation = "native loose asset seek";
        return false;
    }
    auto* bytes = static_cast<std::uint8_t*>(dst);
    std::size_t done = 0;
    while (done < count) {
        const auto r = asset.file.read(bytes + done, count - done, f.content);
        if (r.error || r.bytes == 0) {
            f.kind = RetailPackageError::source_read;
            f.operation = "native loose asset short read";
            return false;
        }
        done += r.bytes;
    }
    return true;
}

bool NativeRetailPackage::build_index(std::vector<content::NativeContentFile>&& files,
    RetailPackageFailure& f) {
    members_.reserve(files.size());
    by_name_.reserve(files.size());
    for (auto& file : files) {
        const std::string name(file.logical_key());
        auto normalized = key(name, f);
        if (!normalized) return false;
        const auto size = file.length(f.content);
        if (!size) {
            f.kind = content_error(f.content.kind);
            f.operation = "native loose asset length";
            return false;
        }
        if (!by_name_.emplace(std::move(*normalized), members_.size()).second) {
            f.kind = RetailPackageError::duplicate_name;
            f.operation = "native loose asset ambiguous filename";
            return false;
        }
        members_.push_back({{name, *size}, std::move(file)});
    }
    return true;
}

std::unique_ptr<NativeRetailPackage> NativeRetailPackage::open(
    const content::NativeContentVolume& volume, const NativeRetailFilePaths& paths,
    RetailPackageFailure& f) noexcept {
    f = {};
    try {
        auto files = paths.open_directory_files(volume, source_request, f.path);
        if (!files) {
            f.content = f.path.content;
            switch (f.path.kind) {
            case RetailFilePathError::content_file: f.kind = content_error(f.content.kind); break;
            case RetailFilePathError::allocation: f.kind = RetailPackageError::allocation; break;
            case RetailFilePathError::host_capacity: f.kind = RetailPackageError::host_capacity; break;
            case RetailFilePathError::native_exception: f.kind = RetailPackageError::native_exception; break;
            default: f.kind = RetailPackageError::source_path; break;
            }
            f.operation = source_request.data();
            return {};
        }
        auto owner = std::unique_ptr<NativeRetailPackage>(new NativeRetailPackage);
        if (!owner->build_index(std::move(*files), f)) return {};
        return owner;
    } catch (const std::bad_alloc&) {
        f.kind = RetailPackageError::allocation;
    } catch (const std::length_error&) {
        f.kind = RetailPackageError::host_capacity;
    } catch (...) {
        f.kind = RetailPackageError::native_exception;
    }
    f.operation = "retail package index construction";
    return {};
}

std::optional<RetailPackageMember> NativeRetailPackage::find(
    std::string_view name, RetailPackageFailure& f) const noexcept {
    f = {};
    try {
        auto normalized = key(name, f);
        if (!normalized) return {};
        const auto it = by_name_.find(*normalized);
        if (it == by_name_.end()) {
            f.kind = RetailPackageError::missing_member;
            f.operation = "retail package member lookup";
            return {};
        }
        return members_[it->second].member;
    } catch (const std::bad_alloc&) {
        f.kind = RetailPackageError::allocation;
    } catch (const std::length_error&) {
        f.kind = RetailPackageError::host_capacity;
    } catch (...) {
        f.kind = RetailPackageError::native_exception;
    }
    f.operation = "retail package member lookup";
    return {};
}

std::optional<std::size_t> NativeRetailPackage::read(std::string_view name,
    std::uint64_t relative_offset, std::span<std::uint8_t> out,
    RetailPackageFailure& f) noexcept {
    if (owner_thread_ != std::this_thread::get_id()) {
        f = {};
        f.kind = RetailPackageError::wrong_thread;
        f.operation = "retail package cursor owner thread";
        return {};
    }
    try {
        const auto member = find(name, f);
        if (!member) return {};
        if (relative_offset > member->size) {
            f.kind = RetailPackageError::invalid_read_range;
            f.operation = "retail package member-relative read";
            return {};
        }
        const auto n = static_cast<std::size_t>(std::min<std::uint64_t>(
            member->size - relative_offset, out.size()));
        if (n) {
            auto normalized = key(name, f);
            if (!normalized || !exact_read(members_[by_name_.at(*normalized)], relative_offset, out.data(), n, f)) return {};
        }
        return n;
    } catch (const std::bad_alloc&) {
        f.kind = RetailPackageError::allocation;
    } catch (const std::length_error&) {
        f.kind = RetailPackageError::host_capacity;
    } catch (...) {
        f.kind = RetailPackageError::native_exception;
    }
    f.operation = "retail package member read";
    return {};
}

std::unique_ptr<content::NativeFileReadJob> NativeRetailPackage::begin_read(std::string_view name,
    std::uint64_t offset,std::size_t count,RetailPackageFailure& f,content::NativeFileReadFailure& read) noexcept {
    f={};read={};if(owner_thread_!=std::this_thread::get_id()){f.kind=RetailPackageError::wrong_thread;f.operation="retail package asynchronous read owner thread";return {};}
    try{
        const auto member=find(name,f);if(!member)return {};
        if(offset>member->size){f.kind=RetailPackageError::invalid_read_range;f.operation="retail package asynchronous member range";return {};}
        const auto n=std::size_t(std::min<std::uint64_t>(count,member->size-offset));
        const auto normalized=key(name,f);if(!normalized)return {};
        auto job=content::NativeFileReadJob::begin(members_[by_name_.at(*normalized)].file,offset,n,read);
        if(!job){f.kind=RetailPackageError::source_read;f.operation="actual native asynchronous member read";}
        return job;
    }catch(const std::bad_alloc&){f.kind=RetailPackageError::allocation;}
     catch(const std::length_error&){f.kind=RetailPackageError::host_capacity;}
     catch(...){f.kind=RetailPackageError::native_exception;}
    f.operation="retail package asynchronous member read";return {};
}

namespace main_binding {
bool initialize_retail_package_stage(NativeRetailMainState& main, StartupFailure& failure) {
    failure = {};
    if (main.package) {
        failure.kind = StartupError::already_initialized;
        failure.operation = "retail package owner exists";
        return false;
    }
    if (!main.file_paths) {
        failure.kind = StartupError::native_exception;
        failure.operation = "retail package native file-path service missing";
        return false;
    }
    RetailPackageFailure source;
    auto owner = NativeRetailPackage::open(main.startup.content, *main.file_paths, source);
    if (!owner) {
        failure.content = source.content;
        failure.operation = source.operation;
        switch (source.kind) {
        case RetailPackageError::allocation: failure.kind = StartupError::allocation; break;
        case RetailPackageError::host_capacity: failure.kind = StartupError::host_capacity; break;
        case RetailPackageError::source_path:
        case RetailPackageError::source_open:
        case RetailPackageError::source_read:
        case RetailPackageError::source_seek:
        case RetailPackageError::source_length: failure.kind = StartupError::content; break;
        default: failure.kind = StartupError::native_exception; break;
        }
        return false;
    }
    main.package = std::move(owner);
    return true;
}
} // namespace main_binding

} // namespace mka::retail
