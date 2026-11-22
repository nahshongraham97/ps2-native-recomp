#pragma once

#include "mka/retail_system_art_cpu.hpp"
#include "mka/native_gl_texture.hpp"
#include <thread>

namespace mka::retail {
class NativeRetailGameSession;
struct NativeRetailMainState;
struct StartupFailure;

enum class RetailLoadedArtError {
    none, wrong_thread, session, package, read, cpu, allocation, native_exception,
    texture, sampler, foreign_device, busy, stale_resource, invalid_state
};
struct RetailLoadedArtFailure {
    RetailLoadedArtError kind{RetailLoadedArtError::none};
    const char* operation{""};
    std::size_t ordinal{};
    RetailPackageFailure package;
    content::NativeFileReadFailure read;
    RetailSystemArtCpuFailure cpu;
    graphics::NativeTextureFailure texture;
};
enum class RetailLoadedArtPhase { empty, reading, converting, converted, failed, retiring, retired };
enum class RetailLoadedArtStep { pending, complete };
struct RetailLoadedArtObservation {
    RetailLoadedArtPhase phase{};
    std::int32_t language{};
    std::size_t converted_textures{}, directory_records{};
    bool pending_job_retained{}, font_zero_cached{}, font_zero_has_metrics{};
    std::size_t texture_lookups{}, metrics_lookups{};
};

// Retail texture metadata, not chip registers. Unsupported source sampler
// modes fail rather than being assigned a host default.
std::optional<graphics::NativeTextureSampler> native_sampler_from_retail_texture(
    std::uint32_t, RetailLoadedArtFailure&) noexcept;
constexpr std::uint32_t font_zero_sampler_from_16fc70(std::uint32_t previous) noexcept {
    return (previous & 0xffff0000u) | 0x3301u;
}

class NativeRetailSectionTexture final {
public:
    std::size_t ordinal() const noexcept { return ordinal_; }
    std::optional<std::uint32_t> sampler_metadata(RetailLoadedArtFailure&) const noexcept;
    std::shared_ptr<const graphics::NativeGlTexture> image(RetailLoadedArtFailure&) const noexcept;
    const FontAtlas* atlas(RetailLoadedArtFailure&) const noexcept;
private:
    friend class NativeRetailLoadedSystemArt;
    NativeRetailSectionTexture(std::weak_ptr<NativeRetailGameSession>,
        std::shared_ptr<const NativeRetailSystemArtCpuSection>,
        std::size_t, std::size_t, std::uint32_t);
    std::shared_ptr<const NativeRetailSystemArtCpuSection> cpu_;
    std::weak_ptr<NativeRetailGameSession> session_;
    std::size_t ordinal_, texture_index_;
    std::uint32_t sampler_metadata_;
    std::shared_ptr<const graphics::NativeGlTexture> image_;
    bool live_{};
    const std::thread::id owner_thread_{std::this_thread::get_id()};
};
struct RetailSelectedFontZero {
    // An engaged slot with null texture is the source's successful empty
    // cache-slot return. A texture with null MET is also a distinct valid result.
    std::shared_ptr<NativeRetailSectionTexture> texture;
    std::optional<text::NativeFontMetrics> metrics;
};

// Persistent actual section/job/GPU owner. CPU parsing alone never publishes
// converted. Every reached type2/3 image is created before pending-job release;
// only then can source lookup/cache consumers observe the converted section.
class NativeRetailLoadedSystemArt final {
public:
    static std::shared_ptr<NativeRetailLoadedSystemArt> for_session(
        const std::shared_ptr<NativeRetailGameSession>&, RetailLoadedArtFailure&) noexcept;
    ~NativeRetailLoadedSystemArt();
    NativeRetailLoadedSystemArt(const NativeRetailLoadedSystemArt&) = delete;
    NativeRetailLoadedSystemArt& operator=(const NativeRetailLoadedSystemArt&) = delete;

    bool queue_from_19b530(NativeRetailPackage&, std::int32_t language, RetailLoadedArtFailure&) noexcept;
    std::optional<RetailLoadedArtStep> advance_from_19aa30(
        const std::shared_ptr<graphics::NativeGlTextureUpload>&, RetailLoadedArtFailure&);
    std::optional<RetailLoadedArtStep> retire_from_19cfa0(RetailLoadedArtFailure&) noexcept;
    std::optional<RetailLoadedArtObservation> observe(RetailLoadedArtFailure&) const noexcept;
    std::optional<RetailSelectedFontZero> configure_zero_from_16fc70(RetailLoadedArtFailure&);
    std::optional<std::shared_ptr<NativeRetailSectionTexture>> find_texture_from_300b60(
        std::string_view, RetailLoadedArtFailure&) const noexcept;
    std::optional<std::span<const std::byte>> find_data_from_300440(
        std::string_view, std::uint32_t& matched_length, RetailLoadedArtFailure&) const noexcept;
private:
    explicit NativeRetailLoadedSystemArt(std::weak_ptr<NativeRetailGameSession>) noexcept;
    bool access(RetailLoadedArtFailure&) const noexcept;
    bool thread(RetailLoadedArtFailure&) const noexcept;
    std::optional<RetailLoadedArtStep> retire(RetailLoadedArtFailure&) noexcept;
    void seal(RetailLoadedArtFailure&) noexcept;
    std::weak_ptr<NativeRetailGameSession> session_;
    const std::thread::id owner_thread_{std::this_thread::get_id()};
    RetailLoadedArtPhase phase_{RetailLoadedArtPhase::empty};
    std::int32_t language_{};
    std::unique_ptr<content::NativeFileReadJob> job_;
    std::shared_ptr<const NativeRetailSystemArtCpuSection> cpu_;
    std::shared_ptr<graphics::NativeGlTextureUpload> device_;
    std::vector<std::shared_ptr<NativeRetailSectionTexture>> textures_;
    std::size_t next_texture_{}, retire_index_{};
    RetailLoadedArtFailure sealed_failure_;
    RetailSelectedFontZero font_zero_;
    mutable std::size_t texture_lookups_{}, metrics_lookups_{};
};

// Separate permanent_strings_eng.mko descendants of19A630/19A5E0 (the actual
// key at5FCE60). Their native resource owner remains required; texture
// conversion is not a successful substitute for215660/215730.
namespace loaded_art_required {
bool queue_permanent_strings_215660(NativeRetailMainState&, StartupFailure&);
bool complete_permanent_strings_215730(NativeRetailMainState&, StartupFailure&);
}
} // namespace mka::retail
