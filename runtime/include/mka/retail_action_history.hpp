#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace mka::retail {

// Source 163210 owns four 30-entry windows. The kind names follow 162E70's
// actual source destinations, not the host's controller roles.
enum class RetailActionJournalKind : std::size_t {
    first_ordinary,
    first_directional,
    second_ordinary,
    second_directional
};

struct RetailActionJournalRecord {
    std::int32_t action{};       // source +00
    std::int32_t frame{};        // source +04
    std::string label;           // owned native value for source +08 string
    std::uint32_t context{};     // source +0c, untouched by 163210
    std::uint32_t canonical{};   // source +10, untouched by 163210
};

struct RetailActionJournal {
    std::array<RetailActionJournalRecord, 30> records{};
    std::int32_t cursor{-1};     // source image data, not reset by 163210
};

struct RetailRecentActionHistory {
    std::array<std::int32_t, 5> canonical{};
    std::int32_t cursor{};       // source BSS, reset to zero by 163210
};

class NativeRetailActionHistory {
public:
    NativeRetailActionHistory() noexcept = default;

    // Mirrors only the stores of 163210. In particular, later record fields
    // and four journal cursors retain their current values on every reset.
    void reset() noexcept;

    // Native producers may update these owned records when their original
    // call ordering, actor context and callback effects have been bound.
    std::array<RetailActionJournal, 4> journals{};
    std::array<RetailRecentActionHistory, 2> recent{};
};

} // namespace mka::retail
