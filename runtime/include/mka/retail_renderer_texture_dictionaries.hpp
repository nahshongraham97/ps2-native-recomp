#pragma once
#include "mka/retail_renderer_core.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mka::retail {

class NativeRetailRendererTextureDictionaries;
struct NativeRetailTextureDictionaryRecord;

enum class RetailRendererTextureDictionaryError {
    none,
    owner,
    wrong_thread,
    retired,
    foreign_owner,
    invalid_handle,
    retired_dictionary,
    reentrant,
    allocator,
    allocation,
    after_effect_unknown,
    native_exception
};

struct RetailRendererTextureDictionaryFailure {
    RetailRendererTextureDictionaryError kind{RetailRendererTextureDictionaryError::none};
    const char* operation{""};
    std::uint32_t source_code{0};
    RetailRendererAllocatorFailure allocator{};
};

class NativeRetailTextureDictionaryHandle final {
public:
    NativeRetailTextureDictionaryHandle() noexcept = default;
    bool empty() const noexcept { return !assigned_; }
    bool same_identity(const NativeRetailTextureDictionaryHandle& x) const noexcept {
        return assigned_ && x.assigned_ && !record_.owner_before(x.record_) && !x.record_.owner_before(record_);
    }

private:
    friend class NativeRetailRendererTextureDictionaries;
    NativeRetailTextureDictionaryHandle(
        std::weak_ptr<NativeRetailRendererTextureDictionaries> o,
        std::weak_ptr<NativeRetailTextureDictionaryRecord> r)
        : owner_(std::move(o)), record_(std::move(r)), assigned_(true) {}
    std::weak_ptr<NativeRetailRendererTextureDictionaries> owner_;
    std::weak_ptr<NativeRetailTextureDictionaryRecord> record_;
    bool assigned_{false};
};

struct RetailRendererTextureDictionaryDescription {
    std::string name;
};

struct RetailRendererTextureDictionaryObservation {
    std::string name;
    bool is_current{false};
    bool destroying{false};
};

// Plugin 0409 scope strictly recovered from Xbox 0x002c9840/0x002c9860/0x002c9907/0x002c9890/0x002c9bf0
// and PS2 0x001a4a80/0x001a4ac0/0x001a7690/0x001a75d0:
// - FreeList-backed 96-byte dictionary container allocation & circular list membership
// - Module open/close reference counting
// - Global current dictionary pointer tracking
// Note: downstream texture containment (add_texture, find_texture, locale-dependent comparisons)
// is unverified from the plugin registration contract and explicitly omitted.
class NativeRetailRendererTextureDictionaries final
    : public std::enable_shared_from_this<NativeRetailRendererTextureDictionaries> {
public:
    ~NativeRetailRendererTextureDictionaries();
    NativeRetailRendererTextureDictionaries(const NativeRetailRendererTextureDictionaries&) = delete;
    NativeRetailRendererTextureDictionaries& operator=(const NativeRetailRendererTextureDictionaries&) = delete;
    NativeRetailRendererTextureDictionaries(NativeRetailRendererTextureDictionaries&&) = delete;
    NativeRetailRendererTextureDictionaries& operator=(NativeRetailRendererTextureDictionaries&&) = delete;

    static std::shared_ptr<NativeRetailRendererTextureDictionaries> create(
        NativeRetailRendererCore& core,
        RetailRendererTextureDictionaryFailure& f) noexcept;

    std::optional<NativeRetailTextureDictionaryHandle> create_dictionary(
        RetailRendererTextureDictionaryDescription desc,
        RetailRendererTextureDictionaryFailure& f) noexcept;

    bool destroy_dictionary(
        NativeRetailTextureDictionaryHandle handle,
        RetailRendererTextureDictionaryFailure& f) noexcept;

    bool set_current_dictionary(
        NativeRetailTextureDictionaryHandle handle,
        RetailRendererTextureDictionaryFailure& f) noexcept;

    NativeRetailTextureDictionaryHandle current_dictionary() const noexcept;

    std::optional<RetailRendererTextureDictionaryObservation> observe(
        NativeRetailTextureDictionaryHandle dict_handle,
        RetailRendererTextureDictionaryFailure& f) const noexcept;

    std::size_t active_count() const noexcept;

    void retire() noexcept;

private:
    friend class NativeRetailRendererCore;
    NativeRetailRendererTextureDictionaries(
        std::weak_ptr<NativeRetailGameSession> session,
        std::shared_ptr<NativeRetailRendererAllocatorService> allocator);

    bool access(RetailRendererTextureDictionaryFailure& f) const noexcept;
    std::shared_ptr<NativeRetailTextureDictionaryRecord> validate(
        NativeRetailTextureDictionaryHandle handle,
        RetailRendererTextureDictionaryFailure& f) const noexcept;

    std::weak_ptr<NativeRetailGameSession> session_;
    std::shared_ptr<NativeRetailRendererAllocatorService> allocator_;

    std::vector<std::shared_ptr<NativeRetailTextureDictionaryRecord>> records_;
    std::shared_ptr<NativeRetailTextureDictionaryRecord> current_record_;
    NativeRetailTextureDictionaryHandle current_handle_;

    bool retired_{false};
    bool busy_{false};
    std::size_t destroy_cursor_{0};
    const std::thread::id thread_{std::this_thread::get_id()};
};

namespace renderer_required {
bool construct_core_plugin_0409(NativeRetailRendererCore&, RetailRendererCoreFailure&);
void destroy_core_plugin_0409(NativeRetailRendererCore&) noexcept;
}

} // namespace mka::retail
