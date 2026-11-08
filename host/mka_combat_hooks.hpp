#pragma once

#include "../../../../include/armageddon/retail_pad_event.hpp"
#include "../../../../include/armageddon/retail_health.hpp"
#include "../../../../include/armageddon/stage_hazards.hpp"
#include "../../../../include/armageddon/video_stream_registry.hpp"
#include "../../../../include/armageddon/audio_sink_registry.hpp"
#include "../../../../include/armageddon/camera_registry.hpp"
#include "../../../../include/armageddon/special_move_state.hpp"
#include "../../../../include/armageddon/fatality_sequencer.hpp"
#include "../../../../include/armageddon/projectile_manager.hpp"
#include "../../../../include/armageddon/breaker_parry_engine.hpp"
#include "../../../../include/armageddon/air_combat_engine.hpp"
#include "../../../../include/armageddon/aggressor_weapon_matrix.hpp"
#include "../../../../include/armageddon/stage_transition_engine.hpp"
#include "ps2/native_hook.hpp"
#include "ps2/generated.hpp"
#include "ps2/system.hpp"
#include "ps2/memory.hpp"
#include "ps2/cpu.hpp"
#include "ps2/os.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>

namespace mka::combat {

// Interception Activation State
inline bool g_combat_interception_active = false;

inline bool is_interception_enabled() {
    if (g_combat_interception_active) return true;
    const std::string env = ps2::os::env("MKA_INTERCEPT_COMBAT");
    if (!env.empty() && env != "0" && env != "off") {
        g_combat_interception_active = true;
        return true;
    }
    return false;
}

// Native Phase 2 Combo History Tracker (outside RDRAM)
inline armageddon::retail::ComboHistoryTracker g_combo_tracker;

// Native Phase 3 Subsystem Managers
inline armageddon::StageHazardManager g_stage_hazard_manager;
inline armageddon::VideoStreamRegistry g_video_stream_registry;

// Native Phase 4 Audio Sink Registry (outside RDRAM)
inline armageddon::audio::AudioSinkRegistry g_audio_registry;

// Native Phase 5 Director's Cut Camera Registry (outside RDRAM)
inline armageddon::camera::CameraRegistry g_camera_registry;

// Native Phase 6 Special Move & Stance Registry (outside RDRAM)
inline armageddon::special_moves::SpecialMoveRegistry g_stance_registry;

// Native Phase 7 Kreate-A-Fatality Sequencer (outside RDRAM)
inline armageddon::fatality::FatalitySequencer g_fatality_sequencer;

// Native Phase 8 Projectile Manager (outside RDRAM)
inline armageddon::projectile::ProjectileManager g_projectile_manager;

// Native Phase 9 Defense Matrix Registry (outside RDRAM)
inline armageddon::defense::DefenseMatrixRegistry g_defense_matrix;

// Native Phase 10 Air Combat Manager (outside RDRAM)
inline armageddon::air_combat::AirCombatManager g_air_combat_manager;

// Native Phase 11 Aggressor & Weapon Awakening Matrix (outside RDRAM)
inline armageddon::aggressor::AggressorMatrixRegistry g_aggressor_matrix;

// Native Phase 12 Stage Transition & Multi-Tier Engine (outside RDRAM)
inline armageddon::stage_transition::StageTransitionManager g_stage_transition_manager;

inline void process_audio(const std::int16_t *samples, std::size_t frames) {
    if (!is_interception_enabled()) return;
    g_audio_registry.on_audio_samples(samples, frames);
}

// Portable C++17 bit reinterpretation primitives
inline float from_bits(std::uint32_t bits) {
    float result = 0.0f;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

inline std::uint32_t to_bits(float value) {
    std::uint32_t result = 0;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

// Guest PC Constants: Cross-Unit Boundaries
constexpr std::uint32_t kPadScannerGuestPc   = 0x001637D0u; // From unit_0008 (entry 328u in unit_0018)
constexpr std::uint32_t kHealthScalarGuestPc = 0x00197BC0u; // From unit_0041 (Hit Dispatcher, RA 0x00207728)
constexpr std::uint32_t kVideoStreamGuestPc  = 0x00181F90u; // Video stream dispatcher (entry 126u in unit_0020)

// RDRAM Layout (Strict Authentic 30-entry Ring & 5-entry Recent Map)
constexpr std::uint32_t kRingBufferAddr       = 0x0065ed40u;
constexpr std::uint32_t kRecentRingAddr       = 0x0065e600u;
constexpr std::uint32_t kWriteIndexAddr       = 0x0064c3acu;
constexpr std::uint32_t kRecentIndexAddr      = 0x0064e0ecu;
constexpr std::uint32_t kLastTickAddr         = 0x0064e0e4u;

constexpr std::uint32_t kSubZeroHealthAddr   = 0x00660180u;
constexpr std::uint32_t kModeWordAddr        = 0x0064dea4u;
constexpr std::uint32_t kGatekeeperFlagsAddr = 0x00660080u;
constexpr std::uint32_t kPlayer2ObjPtr       = 0x006601a4u;

// 1. Health Scalar & Stage Hazard Interception (Linker Wrapped unit_0025 @ entry 204u)
inline void handle_subzero_health(ps2::Cpu &c, std::uint32_t depth,
                                  void (*real_unit_0025)(ps2::Cpu&, std::uint32_t, std::uint32_t)) {
    const std::uint32_t mode        = ps2::ld32(kModeWordAddr);
    const std::uint8_t flags_660080 = static_cast<std::uint8_t>(ps2::ld32(kGatekeeperFlagsAddr));
    const std::uint8_t flags_660084 = static_cast<std::uint8_t>(ps2::ld32(kGatekeeperFlagsAddr + 4));

    const float delta   = from_bits(c.f[12]);
    const float current = from_bits(ps2::ld32(kSubZeroHealthAddr));

    const float combo_mult = g_combo_tracker.get_damage_multiplier();
    const float effective_delta = delta * combo_mult;

    // Gatekeeper: Non-standard / knockout modes (health depletion <= 0) delegate explicitly to authentic unit_0025
    static std::uint64_t s_health_calls = 0;
    const bool is_target_combat = (mode < 2 && (flags_660080 & 4u) != 0 && (flags_660084 & 4u) == 0 &&
                                   std::isfinite(current) && std::isfinite(effective_delta) &&
                                   effective_delta < 0.0f && current > 0.0f && (current + effective_delta) > 0.0f);

    if (++s_health_calls <= 25 || is_target_combat) {
        std::fprintf(stderr, "[mka-health] invocation #%llu: mode=%u flags=0x%02X/0x%02X cur=%.4f delta=%.4f (mult=%.1fx) (%s)\n",
                     static_cast<unsigned long long>(s_health_calls), mode, flags_660080, flags_660084,
                     current, effective_delta, combo_mult, is_target_combat ? "intercepting" : "delegating");
    }

    if (!is_target_combat) {
        g_audio_registry.evaluate_combat_audio_state(0.0f, effective_delta, false, true);
        g_camera_registry.on_combat_hit(effective_delta, true, false);
        g_camera_registry.advance_frame();
        g_fatality_sequencer.start_fatality(0u);
        real_unit_0025(c, 204u, depth);
        return;
    }

    try {
        // Phase 9 Defense Matrix: Parry Evaluation (Down + Block)
        float active_delta = effective_delta;
        float mitigated_dmg = 0.0f;
        const bool parried = (effective_delta < 0.0f &&
                              g_defense_matrix.evaluate_incoming_hit(-effective_delta, 0u, mitigated_dmg));
        if (parried) {
            std::fprintf(stderr, "[mka-defense] PARRY DEFLECT! Incoming strike of %.4ff mitigated to 0.0f (attacker stagger=18f)!\n",
                         -effective_delta);
            active_delta = 0.0f;
        }

        // Phase 11 Aggressor Light Armor Absorption
        float armor_absorbed_dmg = 0.0f;
        const bool armor_absorbed = (!parried && active_delta < 0.0f &&
                                     g_aggressor_matrix.try_absorb_damage(-active_delta, armor_absorbed_dmg, 0u));
        if (armor_absorbed) {
            std::fprintf(stderr, "[mka-aggressor] LIGHT ARMOR ABSORPTION! Incoming strike of %.4ff reduced to %.4ff (armor remaining=%u)\n",
                         -active_delta, armor_absorbed_dmg, g_aggressor_matrix.armor_hits_remaining());
            active_delta = -armor_absorbed_dmg;
        }

        // Standard Hit Path: Bit-cast delta and commit with IEEE-754 integrity
        const float new_health = armageddon::retail::apply_observed_hit_to_health(
            current, active_delta, mode, flags_660080, flags_660084);

        ps2::st32(kSubZeroHealthAddr, to_bits(new_health)); // Preserves 0x3f6d288d

        const std::uint32_t p2 = ps2::ld32(kPlayer2ObjPtr);
        if (p2 != 0) {
            const float old_acc = from_bits(ps2::ld32(p2 + 0x3dc));
            ps2::st32(p2 + 0x3dc, to_bits(old_acc - active_delta));

            // Stage Hazard Collision Check (Position at p2 + 0x50, 0x54, 0x58)
            const float p2_x = from_bits(ps2::ld32(p2 + 0x50));
            const float p2_y = from_bits(ps2::ld32(p2 + 0x54));
            const float p2_z = from_bits(ps2::ld32(p2 + 0x58));

            auto hazard_hit = g_stage_hazard_manager.check_point_collision(p2_x, p2_y, p2_z);
            if (hazard_hit.triggered) {
                std::fprintf(stderr, "[mka-hazard] Triggered %s at (%.1f, %.1f, %.1f) (damage=%.1f, lethal=%d)\n",
                             hazard_hit.hazard_name.c_str(), p2_x, p2_y, p2_z,
                             hazard_hit.total_damage, hazard_hit.is_lethal);

                if (hazard_hit.is_lethal) {
                    std::fprintf(stderr, "[mka-hazard] Lethal deathtrap fatality [%s]! Delegating to authentic unit_0025\n",
                                 hazard_hit.fatality_cinematic_name.c_str());
                    g_audio_registry.evaluate_combat_audio_state(0.0f, effective_delta, false, true);
                    g_camera_registry.on_combat_hit(effective_delta, true, false);
                    g_camera_registry.advance_frame();
                    g_fatality_sequencer.start_fatality(0u);
                    real_unit_0025(c, 204u, depth);
                    return;
                }
            }

            // Phase 8 Projectile Collision Evaluation (Target at p2 + 0x50, 0x54, 0x58)
            auto proj_hit = g_projectile_manager.update_and_collide(p2_x, p2_y, p2_z, 0u);
            if (proj_hit == armageddon::projectile::ProjectileHitResult::DirectHit) {
                std::fprintf(stderr, "[mka-proj] %s collided with Player 2! (freeze=%u frames, dmg=%.2f)\n",
                             armageddon::projectile::projectile_type_name(g_projectile_manager.last_hit_type()),
                             g_projectile_manager.target_frozen_ticks(), g_projectile_manager.last_hit_damage());
                g_camera_registry.on_combat_hit(-g_projectile_manager.last_hit_damage(), false, false);
                g_camera_registry.advance_frame();
            }

            // Phase 10 Aerial Kinematics Synchronization (Y at p2 + 0x54)
            if (g_air_combat_manager.p2().is_airborne()) {
                ps2::st32(p2 + 0x54, to_bits(g_air_combat_manager.p2().y));
            }

            // Phase 12 Stage Transition & Multi-Tier Breach Evaluation
            const bool wall_breached = g_stage_transition_manager.evaluate_wall_impact(p2_x, 1.0f);
            if (wall_breached) {
                std::fprintf(stderr, "[mka-stage] ARENA PERIMETER BREACHED at X=%.2f! Transitioning to Freefall Tier 2\n", p2_x);
                g_stage_transition_manager.trigger_tier_fall(1u, p2_y);
                g_camera_registry.on_combat_hit(-0.25f, false, true);
                g_camera_registry.advance_frame();
            } else if (g_stage_transition_manager.is_in_transition()) {
                const bool landed_tier2 = g_stage_transition_manager.step_fall_physics();
                if (landed_tier2) {
                    bool is_fatal = false;
                    const float impact_dmg = g_stage_transition_manager.calculate_impact_damage(current, is_fatal);
                    std::fprintf(stderr, "[mka-stage] TIER 2 IMPACT LANDING! Impact dmg=%.2ff (lethal=%d, new Tier=2)\n",
                                 impact_dmg, is_fatal ? 1 : 0);
                    if (is_fatal) {
                        std::fprintf(stderr, "[mka-stage] Lethal tier drop fatality! Delegating to authentic unit_0025\n");
                        g_audio_registry.evaluate_combat_audio_state(0.0f, -impact_dmg, false, true);
                        g_camera_registry.on_combat_hit(-impact_dmg, true, false);
                        g_camera_registry.advance_frame();
                        g_fatality_sequencer.start_fatality(0u);
                        real_unit_0025(c, 204u, depth);
                        return;
                    }
                    g_camera_registry.on_combat_hit(-impact_dmg, false, true);
                    g_camera_registry.advance_frame();
                }
            }
        }

        const bool is_combo = g_combo_tracker.detect_hapkido_string();
        if (parried) {
            g_audio_registry.evaluate_combat_audio_state(new_health, 0.0f, false, false);
            g_camera_registry.on_combat_hit(0.0f, false, true);
            g_camera_registry.advance_frame();
            g_defense_matrix.advance_frame();
            g_air_combat_manager.advance_frame(0u);
            g_aggressor_matrix.advance_frame();
        } else {
            g_audio_registry.evaluate_combat_audio_state(new_health, active_delta, is_combo, false);
            g_camera_registry.on_combat_hit(active_delta, false, is_combo);
            g_camera_registry.advance_frame();
            g_defense_matrix.advance_frame();
            g_air_combat_manager.advance_frame(0u);
            g_aggressor_matrix.advance_frame();
        }

        std::fprintf(stderr, "[health] delta %.4ff (mult=%.1fx, parried=%d) applied to current %.4ff -> new %.4ff (mode=%u)\n",
                     active_delta, combo_mult, parried ? 1 : 0, current, new_health, mode);
        std::fprintf(stderr, "[mka-camera] Hit-stop: %s (dilation=%.2f, shake=%u, fov=%.1f)\n",
                     armageddon::camera::hit_stop_state_name(g_camera_registry.hit_stop().state),
                     g_camera_registry.hit_stop().time_dilation(),
                     static_cast<unsigned>(g_camera_registry.shake().active_tier),
                     g_camera_registry.matrix_buffer().latest().fov);

        c.pc = c.r[31].w[0]; // Clean unwind to caller RA (0x00207728)
    } catch (const std::exception &e) {
        std::fprintf(stderr, "[mka-health] delegated on exception (%s)\n", e.what());
        real_unit_0025(c, 204u, depth);
    }
}

// 2. PAD Event Interception Implementation (Linker Wrapped unit_0018 @ entry 309u)
inline void handle_pad_event(ps2::Cpu &c, std::uint32_t depth,
                             void (*real_unit_0018)(ps2::Cpu&, std::uint32_t, std::uint32_t)) {
    const std::uint32_t channel       = c.r[19].w[0]; // channel loop index (0 = P1)
    const std::uint32_t button_code   = c.r[5].w[0];  // button index (0..15)
    const std::uint32_t tick          = c.r[6].w[0];
    const std::uint32_t label_addr    = c.r[7].w[0];
    const std::uint32_t mapping_index = c.r[8].w[0];
    const std::uint32_t ret_addr      = c.r[31].w[0];

    // Gatekeeper: Intercept Player 1 combat cluster:
    // 4 (Triangle), 5 (Circle), 6 (Cross), 7 (Square), 9 (R2 Block), 10 (L1 Style Change)
    // Also intercept directional buttons (0..3) if a Kreate-A-Fatality sequence is active
    static std::uint64_t s_pad_events = 0;
    static std::uint32_t s_last_dir_code = 0xFFFFFFFFu;
    static std::uint32_t s_last_dir_tick = 0u;
    if (channel == 0 && button_code <= 3u) {
        s_last_dir_code = button_code;
        s_last_dir_tick = tick;
    }

    const bool is_target_combat = (channel == 0 &&
        (button_code == 4u || button_code == 5u || button_code == 6u || button_code == 7u ||
         button_code == 9u || button_code == 10u ||
         (g_fatality_sequencer.is_active() && button_code <= 3u)));

    if (++s_pad_events <= 25 || is_target_combat) {
        const auto btn_enum = armageddon::retail::to_combat_button(button_code);
        std::fprintf(stderr, "[mka-pad] event #%llu: ch=%u btn=%u [%s] (target=%s)\n",
                     static_cast<unsigned long long>(s_pad_events), channel, button_code,
                     armageddon::retail::combat_button_name(btn_enum),
                     is_target_combat ? "COMBAT_INTERCEPT" : "delegate");
    }

    if (!is_target_combat) {
        real_unit_0018(c, 309u, depth);
        return;
    }

    try {
        // Native Path: Reconstruct channel state and commit atomically
        armageddon::retail::PadEventChannel0 pad_ch{};
        pad_ch.write_index     = ps2::ld32(kWriteIndexAddr);
        pad_ch.recent_index    = ps2::ld32(kRecentIndexAddr);
        pad_ch.last_event_tick = ps2::ld32(kLastTickAddr);

        if (pad_ch.write_index >= pad_ch.records.size() ||
            pad_ch.recent_index >= pad_ch.recent_mappings.size()) {
            real_unit_0018(c, 309u, depth);
            return;
        }

        for (std::size_t i = 0; i < pad_ch.records.size(); ++i) {
            const std::uint32_t at = kRingBufferAddr + static_cast<std::uint32_t>(i * 20);
            pad_ch.records[i] = {
                ps2::ld32(at), ps2::ld32(at + 4), ps2::ld32(at + 8), ps2::ld32(at + 12), ps2::ld32(at + 16)
            };
        }
        for (std::size_t i = 0; i < pad_ch.recent_mappings.size(); ++i) {
            pad_ch.recent_mappings[i] = ps2::ld32(kRecentRingAddr + static_cast<std::uint32_t>(i * 4));
        }

        const std::uint32_t prev_index = pad_ch.write_index;
        armageddon::retail::append_pad_event(pad_ch, button_code, tick, label_addr, 0u, mapping_index);
        g_combo_tracker.record(button_code, tick);

        if (g_combo_tracker.detect_hapkido_string()) {
            std::fprintf(stderr, "[mka-combo] DETECTED HAPKIDO STRING (Square -> Triangle -> Cross) at tick 0x%04X!\n", tick);
            g_audio_registry.evaluate_combat_audio_state(1.0f, 0.0f, true, false);
            g_camera_registry.on_combat_hit(-0.10f, false, true);
            g_camera_registry.advance_frame();
            std::fprintf(stderr, "[mka-camera] Combo string camera pulse: FOV=%.1f, shake=%u\n",
                         g_camera_registry.matrix_buffer().latest().fov,
                         static_cast<unsigned>(g_camera_registry.shake().active_tier));
        }

        // Phase 6 Stance & Special Move Processing
        if (button_code == 10u) { // L1 Style Change / Stance Switch
            const bool switched = g_stance_registry.on_button_press(button_code, tick);
            if (switched) {
                std::fprintf(stderr, "[mka-stance] Switched stance to %s (L1 at tick 0x%04X, cooldown=8f)\n",
                             armageddon::special_moves::stance_name(g_stance_registry.current_stance()), tick);
            }
        }

        // Phase 7 Kreate-A-Fatality Processing
        if (g_fatality_sequencer.is_active()) {
            const bool matched = g_fatality_sequencer.on_pad_event(button_code, tick);
            if (matched && g_fatality_sequencer.phase() == armageddon::fatality::FatalityPhase::ExecutingMove) {
                std::fprintf(stderr, "[mka-fatality] Executed %s (Tier %u, Score=%u)\n",
                             armageddon::fatality::fatality_move_name(g_fatality_sequencer.active_move()),
                             g_fatality_sequencer.current_tier(), g_fatality_sequencer.total_score());
                g_camera_registry.on_combat_hit(-0.15f, false, true);
                g_camera_registry.advance_frame();
            } else if (matched && g_fatality_sequencer.phase() == armageddon::fatality::FatalityPhase::FinalFinisherExecuting) {
                std::fprintf(stderr, "[mka-fatality] FINAL FINISHER: %s! (Score=%u)\n",
                             armageddon::fatality::fatality_move_name(g_fatality_sequencer.active_move()),
                             g_fatality_sequencer.total_score());
                g_audio_registry.evaluate_combat_audio_state(0.0f, -0.30f, false, true);
                g_camera_registry.on_combat_hit(-0.30f, true, false);
                g_camera_registry.advance_frame();
            }
        }

        // Phase 8 Projectile Spawning on Special Inputs
        // Cross (button 6): spawn Freeze Ball
        // Square (button 7) in Shotokan stance: spawn stationary Ice Clone trap
        if (button_code == 6u) {
            g_projectile_manager.spawn_freeze_ball(armageddon::projectile::Vec3::create(0.0f, 1.2f, 0.0f), 1.0f);
            std::fprintf(stderr, "[mka-proj] Spawned Freeze Ball at tick 0x%04X (active=%zu)\n",
                         tick, g_projectile_manager.active_count());
        } else if (button_code == 7u && g_stance_registry.current_stance() == armageddon::special_moves::StanceType::Shotokan) {
            g_projectile_manager.spawn_ice_clone(armageddon::projectile::Vec3::create(0.0f, 0.8f, 0.0f));
            std::fprintf(stderr, "[mka-proj] Spawned Ice Clone trap at tick 0x%04X (active=%zu)\n",
                         tick, g_projectile_manager.active_count());
        }

        // Phase 9 Defense Matrix Processing (R2 Block = button 9)
        if (button_code == 9u) {
            const bool recent_dir = (tick >= s_last_dir_tick && (tick - s_last_dir_tick) <= 30u);
            if (recent_dir && s_last_dir_code == 2u) { // Down + R2 -> Parry
                const bool opened = g_defense_matrix.try_parry_input(tick);
                if (opened) {
                    std::fprintf(stderr, "[mka-defense] Opened 4-frame Parry Window at tick 0x%04X (Down + R2)\n", tick);
                }
            } else if (recent_dir && s_last_dir_code == 1u) { // Forward + R2 -> Breaker
                const bool broken = g_defense_matrix.try_combo_breaker(tick, true, true);
                if (broken) {
                    std::fprintf(stderr, "[mka-defense] COMBO BREAKER TRIGGERED at tick 0x%04X! Charges remaining=%u\n",
                                 tick, g_defense_matrix.breaker().charges_remaining);
                    g_combo_tracker.clear();
                    const std::uint32_t p2 = ps2::ld32(kPlayer2ObjPtr);
                    if (p2 != 0) {
                        float p2_x = from_bits(ps2::ld32(p2 + 0x50));
                        p2_x += g_defense_matrix.breaker().knockback_impulse_x;
                        ps2::st32(p2 + 0x50, to_bits(p2_x));
                        std::fprintf(stderr, "[mka-defense] Applied Breaker knockback (+%.1fm) -> new P2 X=%.2f\n",
                                     g_defense_matrix.breaker().knockback_impulse_x, p2_x);
                        if (g_stage_transition_manager.evaluate_wall_impact(p2_x, 1.5f, tick)) {
                            std::fprintf(stderr, "[mka-stage] BREAKER KNOCKBACK SHATTERED PERIMETER BARRIER! (P2 X=%.2f)\n", p2_x);
                            g_stage_transition_manager.trigger_tier_fall(1u, 0.0f, tick);
                        }
                    }
                    g_camera_registry.on_combat_hit(-0.05f, false, true);
                    g_camera_registry.advance_frame();
                }
            }
        }

        // Phase 10 Air Combat Launcher & Jump Pursuit Processing
        const bool recent_up = (tick >= s_last_dir_tick && (tick - s_last_dir_tick) <= 30u && s_last_dir_code == 0u);
        if (button_code == 7u && recent_up) { // Up + Square -> Launcher Strike
            g_air_combat_manager.on_launcher_strike(tick);
            std::fprintf(stderr, "[mka-air] LAUNCHER STRIKE at tick 0x%04X! P2 launched into air (vy=%.1fm/s)\n",
                         tick, g_air_combat_manager.p2().vy);
            g_camera_registry.on_combat_hit(-0.08f, false, true);
            g_camera_registry.advance_frame();
        } else if (button_code == 6u && recent_up) { // Up + Cross -> Jump Pursuit
            g_air_combat_manager.on_jump_pursuit(tick);
            std::fprintf(stderr, "[mka-air] JUMP PURSUIT at tick 0x%04X! P1 airborne (vy=%.1fm/s)\n",
                         tick, g_air_combat_manager.p1().vy);
        } else if (g_air_combat_manager.p2().is_airborne() &&
                   (button_code == 4u || button_code == 5u || button_code == 7u)) {
            float air_scale = 1.0f;
            if (g_air_combat_manager.on_aerial_combo_hit(tick, air_scale)) {
                std::fprintf(stderr, "[mka-air] AERIAL STRIKE connected at tick 0x%04X! Juggle #%u (scaling=%.0f%%)\n",
                             tick, g_air_combat_manager.p2().juggle_hits, air_scale * 100.0f);
                g_camera_registry.on_combat_hit(-0.05f * air_scale, false, true);
                g_camera_registry.advance_frame();
            }
        }

        // Phase 11 Aggressor Super Meter & Awakening Activation
        if (button_code == 4u || button_code == 5u || button_code == 6u || button_code == 7u) {
            const float gain = (button_code == 4u || button_code == 5u) ? 8.0f : 5.0f;
            const bool reached_full = g_aggressor_matrix.add_meter(gain, tick);
            if (reached_full) {
                std::fprintf(stderr, "[mka-aggressor] METER FULL at tick 0x%04X! Aggressor meter reached 100.0f (Awakening ready via L1)\n", tick);
            }
        }
        if (button_code == 10u && g_aggressor_matrix.is_full()) {
            const bool awakened = g_aggressor_matrix.trigger_awakening(tick);
            if (awakened) {
                const bool in_weapon = (g_stance_registry.current_stance() == armageddon::special_moves::StanceType::KoriBlade);
                const float boost = g_aggressor_matrix.evaluate_offensive_multiplier(in_weapon, tick);
                std::fprintf(stderr, "[mka-aggressor] AWAKENING ACTIVATED at tick 0x%04X! (duration=240f, armor=2 hits, boost=%.2fx, weapon_synergy=%d)\n",
                             tick, boost, in_weapon ? 1 : 0);
                g_camera_registry.on_combat_hit(-0.12f, false, true);
                g_camera_registry.advance_frame();
            }
        }

        ps2::st32(kWriteIndexAddr,  pad_ch.write_index);
        ps2::st32(kRecentIndexAddr, pad_ch.recent_index);
        ps2::st32(kLastTickAddr,    pad_ch.last_event_tick);

        const std::uint32_t write_record_addr = kRingBufferAddr + (pad_ch.write_index * 20);
        const auto &rec = pad_ch.records[pad_ch.write_index];
        ps2::st32(write_record_addr + 0,  rec.code);
        ps2::st32(write_record_addr + 4,  rec.tick);
        ps2::st32(write_record_addr + 8,  rec.label_address);
        ps2::st32(write_record_addr + 12, rec.context);
        ps2::st32(write_record_addr + 16, rec.mapping_index);

        const std::uint32_t recent_entry_addr = kRecentRingAddr + (pad_ch.recent_index * 4);
        ps2::st32(recent_entry_addr, pad_ch.recent_mappings[pad_ch.recent_index]);

        std::fprintf(stderr, "[ring] write_index advanced from %u to %u (tick=0x%04X, button=%u [%s])\n",
                     prev_index, pad_ch.write_index, tick, button_code,
                     armageddon::retail::combat_button_name(armageddon::retail::to_combat_button(button_code)));

        c.pc = ret_addr; // Clean unwind to caller RA (0x001638E8)
    } catch (const std::exception &e) {
        std::fprintf(stderr, "[mka-pad] delegated on exception (%s)\n", e.what());
        real_unit_0018(c, 309u, depth);
    }
}

// 3. Video Stream Interception Implementation (Linker Wrapped unit_0020 @ entry 126u)
inline void handle_video_stream(ps2::Cpu &c, std::uint32_t depth,
                                void (*real_unit_0020)(ps2::Cpu&, std::uint32_t, std::uint32_t)) {
    const std::uint32_t stream_id_raw = c.r[4].w[0];
    const std::uint32_t callback_ra   = c.r[5].w[0];
    const auto stream_id = static_cast<armageddon::VideoStreamId>(stream_id_raw);
    const bool is_known = g_video_stream_registry.has(stream_id);

    static std::uint64_t s_video_calls = 0;
    ++s_video_calls;

    if (is_known) {
        const auto &def = g_video_stream_registry.get(stream_id);
        std::fprintf(stderr, "[mka-video] stream request #%llu: ID=%u (%s, loops=%d, skippable=%d, cb=0x%08X)\n",
                     static_cast<unsigned long long>(s_video_calls), stream_id_raw,
                     def.normal_path.c_str(), def.loops, def.skippable, callback_ra);
    } else {
        std::fprintf(stderr, "[mka-video] unknown stream ID %u (calls=%llu)\n",
                     stream_id_raw, static_cast<unsigned long long>(s_video_calls));
    }

    // Pass through to authentic CRI ADX / SFD video player in unit_0020
    real_unit_0020(c, 126u, depth);
}

// 4. Fallback Dispatch Hooks (for any dynamic guest PC dispatch)
inline void hook_subzero_health(ps2::Cpu &c) {
    const std::uint32_t mode        = ps2::ld32(kModeWordAddr);
    const std::uint8_t flags_660080 = static_cast<std::uint8_t>(ps2::ld32(kGatekeeperFlagsAddr));
    const std::uint8_t flags_660084 = static_cast<std::uint8_t>(ps2::ld32(kGatekeeperFlagsAddr + 4));

    const float delta   = from_bits(c.f[12]);
    const float current = from_bits(ps2::ld32(kSubZeroHealthAddr));

    const float combo_mult = g_combo_tracker.get_damage_multiplier();
    const float effective_delta = delta * combo_mult;

    const bool is_target_combat = (mode < 2 && (flags_660080 & 4u) != 0 && (flags_660084 & 4u) == 0 &&
                                   std::isfinite(current) && std::isfinite(effective_delta) &&
                                   effective_delta < 0.0f && current > 0.0f && (current + effective_delta) > 0.0f);
    if (!is_target_combat) {
        ps2::hook::unregister_hook(kHealthScalarGuestPc);
        ps2::call(c, kHealthScalarGuestPc, 0);
        ps2::hook::register_hook(kHealthScalarGuestPc, hook_subzero_health, "MKA_HealthDeduction");
        return;
    }

    try {
        float active_delta = effective_delta;
        float mitigated_dmg = 0.0f;
        const bool parried = (effective_delta < 0.0f &&
                              g_defense_matrix.evaluate_incoming_hit(-effective_delta, 0u, mitigated_dmg));
        if (parried) {
            std::fprintf(stderr, "[mka-defense] PARRY DEFLECT! Incoming strike of %.4ff mitigated to 0.0f!\n",
                         -effective_delta);
            active_delta = 0.0f;
        }

        const float new_health = armageddon::retail::apply_observed_hit_to_health(
            current, active_delta, mode, flags_660080, flags_660084);

        ps2::st32(kSubZeroHealthAddr, to_bits(new_health));

        const std::uint32_t p2 = ps2::ld32(kPlayer2ObjPtr);
        if (p2 != 0) {
            const float old_acc = from_bits(ps2::ld32(p2 + 0x3dc));
            ps2::st32(p2 + 0x3dc, to_bits(old_acc - active_delta));

            const float p2_x = from_bits(ps2::ld32(p2 + 0x50));
            const float p2_y = from_bits(ps2::ld32(p2 + 0x54));
            const float p2_z = from_bits(ps2::ld32(p2 + 0x58));

            auto hazard_hit = g_stage_hazard_manager.check_point_collision(p2_x, p2_y, p2_z);
            if (hazard_hit.triggered && hazard_hit.is_lethal) {
                ps2::hook::unregister_hook(kHealthScalarGuestPc);
                ps2::call(c, kHealthScalarGuestPc, 0);
                ps2::hook::register_hook(kHealthScalarGuestPc, hook_subzero_health, "MKA_HealthDeduction");
                return;
            }

            auto proj_hit = g_projectile_manager.update_and_collide(p2_x, p2_y, p2_z, 0u);
            if (proj_hit == armageddon::projectile::ProjectileHitResult::DirectHit) {
                std::fprintf(stderr, "[mka-proj] %s collided with Player 2! (freeze=%u frames, dmg=%.2f)\n",
                             armageddon::projectile::projectile_type_name(g_projectile_manager.last_hit_type()),
                             g_projectile_manager.target_frozen_ticks(), g_projectile_manager.last_hit_damage());
            }

            // Phase 10 Aerial Kinematics Synchronization (Y at p2 + 0x54)
            if (g_air_combat_manager.p2().is_airborne()) {
                ps2::st32(p2 + 0x54, to_bits(g_air_combat_manager.p2().y));
            }

            // Phase 12 Stage Transition & Multi-Tier Breach Evaluation
            const bool wall_breached = g_stage_transition_manager.evaluate_wall_impact(p2_x, 1.0f);
            if (wall_breached) {
                std::fprintf(stderr, "[mka-stage] ARENA PERIMETER BREACHED at X=%.2f! Transitioning to Freefall Tier 2\n", p2_x);
                g_stage_transition_manager.trigger_tier_fall(1u, p2_y);
                g_camera_registry.on_combat_hit(-0.25f, false, true);
                g_camera_registry.advance_frame();
            } else if (g_stage_transition_manager.is_in_transition()) {
                const bool landed_tier2 = g_stage_transition_manager.step_fall_physics();
                if (landed_tier2) {
                    bool is_fatal = false;
                    const float impact_dmg = g_stage_transition_manager.calculate_impact_damage(current, is_fatal);
                    std::fprintf(stderr, "[mka-stage] TIER 2 IMPACT LANDING! Impact dmg=%.2ff (lethal=%d, new Tier=2)\n",
                                 impact_dmg, is_fatal ? 1 : 0);
                    if (is_fatal) {
                        std::fprintf(stderr, "[mka-stage] Lethal tier drop fatality! Delegating to authentic unit_0025\n");
                        g_audio_registry.evaluate_combat_audio_state(0.0f, -impact_dmg, false, true);
                        g_camera_registry.on_combat_hit(-impact_dmg, true, false);
                        g_camera_registry.advance_frame();
                        g_fatality_sequencer.start_fatality(0u);
                        ps2::hook::unregister_hook(kHealthScalarGuestPc);
                        ps2::call(c, kHealthScalarGuestPc, 0);
                        ps2::hook::register_hook(kHealthScalarGuestPc, hook_subzero_health, "MKA_HealthDeduction");
                        return;
                    }
                    g_camera_registry.on_combat_hit(-impact_dmg, false, true);
                    g_camera_registry.advance_frame();
                }
            }
        }

        g_air_combat_manager.advance_frame(0u);
        g_audio_registry.evaluate_combat_audio_state(new_health, effective_delta, g_combo_tracker.detect_hapkido_string(), false);

        std::fprintf(stderr, "[health] delta %.4ff (mult=%.1fx) applied to current %.4ff -> new %.4ff (mode=%u)\n",
                     effective_delta, combo_mult, current, new_health, mode);

        c.pc = c.r[31].w[0];
    } catch (...) {
        ps2::hook::unregister_hook(kHealthScalarGuestPc);
        ps2::call(c, kHealthScalarGuestPc, 0);
        ps2::hook::register_hook(kHealthScalarGuestPc, hook_subzero_health, "MKA_HealthDeduction");
    }
}

inline void hook_pad_scanner_event(ps2::Cpu &c) {
    const std::uint32_t channel     = c.r[4].w[0];
    const std::uint32_t button_code = c.r[5].w[0];
    const bool is_target_combat = (channel == 0 &&
        (button_code == 4u || button_code == 5u || button_code == 6u || button_code == 7u ||
         button_code == 9u || button_code == 10u));

    if (!is_target_combat) {
        ps2::hook::unregister_hook(kPadScannerGuestPc);
        ps2::call(c, kPadScannerGuestPc, 0);
        ps2::hook::register_hook(kPadScannerGuestPc, hook_pad_scanner_event, "MKA_PadScanner");
        return;
    }
}

// 5. Registration Trigger
inline void register_mka_combat_hooks() {
    g_combat_interception_active = true;
    ps2::hook::register_hook(kPadScannerGuestPc, hook_pad_scanner_event, "MKA_PadScanner");
    ps2::hook::register_hook(kHealthScalarGuestPc, hook_subzero_health, "MKA_HealthDeduction");
    std::fprintf(stderr, "[mka-hooks] Combat interception armed for guest PCs 0x%08X and 0x%08X (Linker Wrappers Active)\n",
                 kPadScannerGuestPc, kHealthScalarGuestPc);
}

} // namespace mka::combat
