#pragma once

#include "mka/stage_a_guest_memory.hpp"

#include <cstdint>
#include <bit>
#include <functional>
#include "mka/retail_action_history.hpp"

namespace mka::phase7 {

struct NativePadSlot {
    std::uint8_t flags = 0;         // guest (0x00660080 + slot*32) + 460
    std::uint32_t active_ptr = 0;   // guest + 468
    std::uint32_t entries_ptr = 0;  // guest + 464
    std::uint32_t button_mask = 0;  // guest + 480
};

/// Stage A native reimplementation of 0x001637D0 (PAD Scanner, unit_0018:328u).
/// Scans the 3 PAD slots at 0x00660080 (stride 32). When a slot is active
/// ((flags & 2) != 0 && (flags & 1) == 0 && active_ptr != 0), scans up to 16
/// button entries (stride 12) and invokes the event callback for matching masks.
template <typename OnMatch>
inline std::uint32_t pad_scan_001637d0(
    mka::stage_a::GuestMemory &mem,
    OnMatch &&on_match,
    std::uint32_t slot_base = 0x00660080u) {
    std::uint32_t matched = 0;
    for (std::uint32_t slot = 0; slot < 3u; ++slot) {
        const std::uint32_t base = slot_base + slot * 32u;
        const std::uint8_t flags = mem.load<std::uint8_t>(base + 460u);
        if ((flags & 2u) == 0u || (flags & 1u) != 0u) {
            continue;
        }
        const std::uint32_t active_ptr = mem.load<std::uint32_t>(base + 468u);
        if (active_ptr == 0u) {
            continue;
        }
        const std::uint32_t entries_ptr = mem.load<std::uint32_t>(base + 464u);
        const std::uint32_t mask = mem.load<std::uint32_t>(base + 480u);
        for (std::uint32_t i = 0; i < 16u; ++i) {
            const std::uint32_t entry_addr = entries_ptr + i * 12u;
            const std::uint32_t entry_mask = mem.load<std::uint32_t>(entry_addr);
            if ((mask & entry_mask) != 0u) {
                on_match(slot, i, entry_addr, active_ptr);
                ++matched;
            }
        }
    }
    return matched;
}


// AOT-derived 00162E70 orchestration. Host-owned history reuses the existing
// native reset owner. The callback bodies remain separately admitted services.
enum class JournalResult { complete, invalid_cursor, context_service_unbound,
                           dispatch_service_unbound, service_refused };
struct NativeJournalState {
    mka::retail::NativeRetailActionHistory& history;
    std::uint32_t source_frame{}, last_action_frame{};
    bool dispatch_enabled{};
    std::array<bool, 2> actor_present{};
};
struct NativeJournalAction {
    bool second{};
    std::uint64_t action{}; // original branches compare full 64-bit a1
    std::uint32_t frame{}, canonical{};
    std::string label;
};
struct NativeJournalServices {
    std::function<bool(bool, std::uint32_t&)> resolve_context;
    std::function<bool()> dispatch;
};
// Observer identifies native fields, never guest addresses. The test bridge
// translates these events; production callers may omit it.
enum class JournalField { last_frame, cursor, action, frame, label, canonical,
                          context, recent_cursor, recent_canonical };
using JournalObserver = std::function<void(JournalField, unsigned, unsigned, std::uint32_t)>;
inline JournalResult record_action_00162e70(NativeJournalState& state,
    const NativeJournalAction& action, const NativeJournalServices& services,
    const JournalObserver& observe = {}) {
    auto event=[&](JournalField f,unsigned group,unsigned slot,std::uint32_t value) {
        if(observe) observe(f,group,slot,value);
    };
    state.last_action_frame=state.source_frame;
    event(JournalField::last_frame,0,0,state.last_action_frame);
    const unsigned owner=action.second?1:0;
    const auto canonical=action.canonical;
    const bool directional=action.action>=12 && action.action<=15;
    const unsigned group=(action.second?2:0)+(directional?1:0);
    auto& journal=state.history.journals[group];
    // ADDIU wraps to 32 bits, then SLTI compares its sign-extended result.
    const auto increment=std::bit_cast<std::uint32_t>(journal.cursor)+1u;
    auto cursor=std::bit_cast<std::int32_t>(increment);
    journal.cursor=cursor; event(JournalField::cursor,group,0,increment);
    if(cursor>=30) { journal.cursor=cursor=0; event(JournalField::cursor,group,0,0); }
    if(cursor<0) return JournalResult::invalid_cursor;
    const unsigned slot=static_cast<unsigned>(cursor);
    auto& record=journal.records[slot];
    record.action=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(action.action));
    event(JournalField::action,group,slot,static_cast<std::uint32_t>(action.action));
    record.frame=std::bit_cast<std::int32_t>(action.frame);
    event(JournalField::frame,group,slot,action.frame);
    record.label=action.label; event(JournalField::label,group,slot,0);
    record.canonical=canonical;
    event(JournalField::canonical,group,slot,canonical);
    if(!directional) {
        std::uint32_t context=0;
        const bool context_needed=state.actor_present[owner];
        if(context_needed) {
            if(!services.resolve_context) return JournalResult::context_service_unbound;
            if(!services.resolve_context(owner!=0,context)) return JournalResult::service_refused;
        }
        // Source reloads the live journal cursor after the context call.
        // A reentrant service may have advanced it; do not cache its slot.
        const auto context_slot=context_needed?journal.cursor:static_cast<std::int32_t>(slot);
        if(context_slot<0 || context_slot>=30)return JournalResult::invalid_cursor;
        journal.records[context_slot].context=context;
        event(JournalField::context,group,static_cast<unsigned>(context_slot),context);
    }
    auto& recent=state.history.recent[owner];
    const auto next=std::bit_cast<std::uint32_t>(recent.cursor)+1u;
    recent.cursor=std::bit_cast<std::int32_t>(next);
    event(JournalField::recent_cursor,owner,0,next);
    if(recent.cursor>=5) { recent.cursor=0; event(JournalField::recent_cursor,owner,0,0); }
    if(recent.cursor<0) return JournalResult::invalid_cursor;
    recent.canonical[recent.cursor]=std::bit_cast<std::int32_t>(canonical);
    event(JournalField::recent_canonical,owner,recent.cursor,canonical);
    if(state.dispatch_enabled) {
        if(!services.dispatch) return JournalResult::dispatch_service_unbound;
        if(!services.dispatch()) return JournalResult::service_refused;
    }
    return JournalResult::complete;
}

} // namespace mka::phase7
