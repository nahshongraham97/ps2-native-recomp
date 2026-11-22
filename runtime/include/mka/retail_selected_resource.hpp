#pragma once

#include "mka/font_atlas.hpp"
#include "mka/font_geometry.hpp"
#include "mka/font_layout.hpp"
#include "mka/font_metrics.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

namespace mka::retail {

struct NativeRetailMainState;
struct StartupFailure;

// Each tier in an arcade ladder: 4 uint32 words (16 bytes).
struct ArcadeLadderTier {
    std::uint32_t opponent_fighter_id{};
    std::uint32_t sub_id{};
    std::uint32_t arena_id{};
    std::uint32_t extra_flags{};
};

// Complete 8-tier arcade ladder (128 bytes).
using ArcadeLadder = std::array<ArcadeLadderTier, 8>;

inline constexpr std::size_t kArcadeLadderCount = 19;
inline constexpr std::size_t kFontTableEntryCount = 31;

// Exact 19 retail arcade ladders from original 0x005e5630 table.
const std::array<ArcadeLadder, kArcadeLadderCount>& retail_arcade_ladders() noexcept;

struct SelectedResourceLadderState {
    std::int32_t progress_word{ -1 }; // 0x0064ed90
    std::uint32_t tier_index{ 0 };    // 0x0064ed8c
    std::uint32_t ladder_index{ 0 };  // 0x0064cc84 (0..18)
    const ArcadeLadder* active_ladder{ nullptr }; // 0x0064ed94
};

struct FontResourceEntry {
    std::string name;
    std::string atlas_name;
    std::string metrics_name;
    std::uint32_t category{ 10 };
    std::shared_ptr<const FontAtlas> atlas;
    std::shared_ptr<const text::NativeFontMetrics> metrics;
    std::uint32_t flags{ 0 }; // original atlas object flags (+0x50): (flags & 0xffff0000) | 0x3301
};

enum class RetailSelectedResourceError {
    none, wrong_thread, not_found, decode_failure, parse_failure, allocation, native_exception
};

struct RetailSelectedResourceFailure {
    RetailSelectedResourceError kind{RetailSelectedResourceError::none};
    const char* operation{""};
};

class NativeRetailSelectedResource final {
public:
    NativeRetailSelectedResource() noexcept;
    ~NativeRetailSelectedResource() = default;
    NativeRetailSelectedResource(const NativeRetailSelectedResource&) = delete;
    NativeRetailSelectedResource& operator=(const NativeRetailSelectedResource&) = delete;

    bool reset_ladder_from_334700(NativeRetailMainState& main, StartupFailure& failure) noexcept;
    bool configure_font_zero_from_16fc70(NativeRetailMainState& main, StartupFailure& failure) noexcept;

    const SelectedResourceLadderState& ladder_state() const noexcept { return ladder_state_; }
    const FontResourceEntry& font_entry(std::size_t index) const;

    // Reuse existing native text layout and geometry with the configured font 0.
    std::optional<text::FontLayout> layout_text(
        std::string_view text, const text::FontLayoutConfig& config,
        text::FontLayoutFailure& failure) const;
    std::optional<text::FontGlyphGeometry> glyph_geometry(
        const text::FontGlyphPlacement& placement, const text::FontGeometryConfig& config,
        text::FontGeometryFailure& failure) const noexcept;

private:
    bool access(StartupFailure& failure, const char* operation) const noexcept;

    SelectedResourceLadderState ladder_state_{};
    std::array<FontResourceEntry, kFontTableEntryCount> font_table_{};
    const std::thread::id owner_thread_{std::this_thread::get_id()};
};

} // namespace mka::retail
