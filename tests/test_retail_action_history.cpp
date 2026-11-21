#include "mka/retail_action_history.hpp"

#include <cstdint>
#include <iostream>

int main() {
    mka::retail::NativeRetailActionHistory history;
    for (const auto& journal : history.journals) {
        if (journal.cursor != -1) return 1;
        for (const auto& record : journal.records)
            if (record.action != 0 || record.frame != 0 || !record.label.empty() ||
                record.context != 0 || record.canonical != 0) return 2;
    }
    for (auto& journal : history.journals) {
        journal.cursor = 17;
        for (auto& record : journal.records) {
            record.action = 12;
            record.frame = 42;
            record.label = "source action";
            record.context = 0x12345678u;
            record.canonical = 0x87654321u;
        }
    }
    for (auto& recent : history.recent) {
        recent.cursor = 4;
        recent.canonical.fill(23);
    }
    history.reset();
    for (const auto& journal : history.journals) {
        if (journal.cursor != 17) return 3;
        for (const auto& record : journal.records)
            if (record.action != -1 || record.frame != -1 || !record.label.empty() ||
                record.context != 0x12345678u || record.canonical != 0x87654321u) return 4;
    }
    for (const auto& recent : history.recent) {
        if (recent.cursor != 0) return 5;
        for (auto canonical : recent.canonical) if (canonical != -1) return 6;
    }
    history.journals[0].records[29].context = 7;
    history.recent[1].cursor = 3;
    history.reset();
    if (history.journals[0].records[29].context != 7 ||
        history.recent[1].cursor != 0) return 7;
    std::cout << "PASS native retail action journal selective reset; producer/game open\n";
    return 0;
}
