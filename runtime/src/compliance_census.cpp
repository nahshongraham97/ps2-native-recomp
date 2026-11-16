#include "compliance_census.hpp"

#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "iop/iop.hpp"
#include "iop/kernel.hpp"

namespace ps2::census {

namespace {

std::atomic<std::uint64_t> g_iop_instructions{0};
std::atomic<std::uint64_t> g_ee_live_translations{0};
std::atomic<std::uint64_t> g_vu0_fallbacks{0};
std::atomic<std::uint64_t> g_vu1_fallbacks{0};

std::atomic<std::uint64_t> g_sif_native{0};
std::atomic<std::uint64_t> g_sif_guest{0};
std::atomic<std::uint64_t> g_sif_unknown{0};

std::atomic<std::uint64_t> g_gs_transfers_applied{0};
std::atomic<std::uint64_t> g_gs_transfers_skipped{0};
std::atomic<std::uint64_t> g_gs_primitives{0};
std::atomic<std::uint64_t> g_movie_blit_ran{0};
std::atomic<std::uint64_t> g_movie_blit_skipped{0};

std::atomic<std::uint64_t> g_spu2_key_ons{0};
std::atomic<std::uint64_t> g_spu2_key_offs{0};
std::atomic<std::uint64_t> g_spu2_dma_writes{0};
std::atomic<std::uint64_t> g_spu2_dma_write_bytes{0};
std::atomic<std::uint64_t> g_spu2_dma_reads{0};
std::atomic<std::uint64_t> g_spu2_dma_read_bytes{0};
std::atomic<std::uint64_t> g_spu2_device_ops{0};
std::atomic<std::int32_t>  g_spu2_peak_sample_val{0};
std::atomic<std::uint64_t> g_audio_native_frames_mixed{0};
std::atomic<std::uint64_t> g_audio_native_blocks_consumed{0};
std::atomic<std::uint64_t> g_audio_native_voices_keyed{0};
std::atomic<std::uint64_t> g_audio_native_voice_frames_rendered{0};
std::atomic<std::uint64_t> g_audio_native_adpcm_blocks_decoded{0};

std::atomic<std::uint64_t> g_cdvd_sector_reads{0};
std::atomic<std::uint64_t> g_content_volume_reads{0};

std::mutex g_mutex;
std::map<std::pair<std::uint32_t, std::uint32_t>, std::pair<RpcKind, std::uint64_t>> g_sif_rpcs;
std::map<std::uint32_t, std::pair<std::uint64_t, std::uint64_t>> g_gs_transfers_by_base; // <applied, skipped>
std::map<std::uint32_t, std::uint64_t> g_gs_primitives_by_target;
std::map<std::uint32_t, std::pair<std::uint64_t, std::uint64_t>> g_card_rpcs_by_method; // <ok, fail>
std::map<const char *, std::uint64_t> g_gs_packet_rejected;                              // rejecting site -> dropped packets

constexpr std::size_t kMaxSamples = 4096;
std::uint32_t g_iop_samples[kMaxSamples]{};
std::atomic<std::size_t> g_iop_sample_count{0};

} // namespace

void iop_guest_instructions(std::uint64_t count) {
    g_iop_instructions.fetch_add(count, std::memory_order_relaxed);
}

void iop_guest_sample(std::uint32_t pc) {
    const std::size_t idx = g_iop_sample_count.fetch_add(1, std::memory_order_relaxed);
    if (idx < kMaxSamples) {
        g_iop_samples[idx] = pc;
    }
}

void ee_live_translation(std::uint32_t pc) {
    g_ee_live_translations.fetch_add(1, std::memory_order_relaxed);
    // The census counts these (the accepted route must run only captured code),
    // but the counter did not say where. Live translation happens twice per run
    // at most, so the PC is worth logging.
    std::fprintf(stderr, "[ee-live] translating EE code outside the captured windows at 0x%08X\n", pc);
    std::fflush(stderr);
}

void vu_interpreter_fallback(int unit, std::uint32_t pc) {
    (void)pc;
    if (unit == 0) {
        g_vu0_fallbacks.fetch_add(1, std::memory_order_relaxed);
    } else {
        g_vu1_fallbacks.fetch_add(1, std::memory_order_relaxed);
    }
}

void sif_rpc(std::uint32_t sid, std::uint32_t method, RpcKind kind) {
    if (kind == RpcKind::Native) g_sif_native.fetch_add(1, std::memory_order_relaxed);
    else if (kind == RpcKind::Guest) g_sif_guest.fetch_add(1, std::memory_order_relaxed);
    else g_sif_unknown.fetch_add(1, std::memory_order_relaxed);

    std::lock_guard<std::mutex> lock(g_mutex);
    auto &entry = g_sif_rpcs[{sid, method}];
    entry.first = kind;
    entry.second++;
}

void gs_transfer(std::uint32_t base, std::uint32_t pixels, bool applied) {
    (void)pixels;
    if (applied) g_gs_transfers_applied.fetch_add(1, std::memory_order_relaxed);
    else g_gs_transfers_skipped.fetch_add(1, std::memory_order_relaxed);

    std::lock_guard<std::mutex> lock(g_mutex);
    auto &entry = g_gs_transfers_by_base[base];
    if (applied) entry.first++;
    else entry.second++;
}

void gs_primitive(std::uint32_t fbp) {
    g_gs_primitives.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(g_mutex);
    g_gs_primitives_by_target[fbp]++;
}

void movie_present(bool blit_ran, bool source_existed) {
    if (!source_existed) return;
    if (blit_ran) g_movie_blit_ran.fetch_add(1, std::memory_order_relaxed);
    else g_movie_blit_skipped.fetch_add(1, std::memory_order_relaxed);
}

void spu2_key_on(int core, int voice) {
    (void)core; (void)voice;
    g_spu2_key_ons.fetch_add(1, std::memory_order_relaxed);
}

void spu2_key_off(int core, int voice) {
    (void)core; (void)voice;
    g_spu2_key_offs.fetch_add(1, std::memory_order_relaxed);
}

void spu2_dma(int core, std::uint32_t bytes, bool to_spu) {
    (void)core;
    if (to_spu) {
        g_spu2_dma_writes.fetch_add(1, std::memory_order_relaxed);
        g_spu2_dma_write_bytes.fetch_add(bytes, std::memory_order_relaxed);
    } else {
        g_spu2_dma_reads.fetch_add(1, std::memory_order_relaxed);
        g_spu2_dma_read_bytes.fetch_add(bytes, std::memory_order_relaxed);
    }
}

void spu2_device_op() {
    g_spu2_device_ops.fetch_add(1, std::memory_order_relaxed);
}

void gs_packet_rejected(const char *why) {
    std::lock_guard<std::mutex> lock(g_mutex);
    ++g_gs_packet_rejected[why];
}

void audio_native_frame_mixed() {
    g_audio_native_frames_mixed.fetch_add(1, std::memory_order_relaxed);
}

void audio_native_block_consumed() {
    g_audio_native_blocks_consumed.fetch_add(1, std::memory_order_relaxed);
}

void audio_native_voice_keyed_on(int core, int voice) {
    (void)core; (void)voice;
    g_audio_native_voices_keyed.fetch_add(1, std::memory_order_relaxed);
}

void audio_native_voice_frame_rendered(std::uint64_t count) {
    g_audio_native_voice_frames_rendered.fetch_add(count, std::memory_order_relaxed);
}

void audio_native_adpcm_block_decoded() {
    g_audio_native_adpcm_blocks_decoded.fetch_add(1, std::memory_order_relaxed);
}

void spu2_peak_sample(std::int16_t left, std::int16_t right) {
    const std::int32_t max_val = std::max(std::abs(static_cast<std::int32_t>(left)),
                                          std::abs(static_cast<std::int32_t>(right)));
    std::int32_t current = g_spu2_peak_sample_val.load(std::memory_order_relaxed);
    while (max_val > current && !g_spu2_peak_sample_val.compare_exchange_weak(current, max_val, std::memory_order_relaxed)) {
    }
}

void card_rpc(std::uint32_t method, bool ok) {
    std::lock_guard<std::mutex> lock(g_mutex);
    auto &entry = g_card_rpcs_by_method[method];
    if (ok) entry.first++;
    else entry.second++;
}

void cdvd_sector_read(std::uint32_t sectors) {
    // A read that a disc device performed. The native content volume serves
    // the game's data from the host's files, so it does not come through here.
    g_cdvd_sector_reads.fetch_add(sectors, std::memory_order_relaxed);
}

void content_volume_sector_read(std::uint32_t sectors) {
    // A read the native content volume served (host files, no disc image and
    // no CDVD device).
    g_content_volume_reads.fetch_add(sectors, std::memory_order_relaxed);
}

static void (*g_exit_flush_hook)();

void set_exit_flush_hook(void (*hook)()) { g_exit_flush_hook = hook; }

void report() {
    static std::atomic<bool> reported{false};
    if (reported.exchange(true)) return;
    std::unique_lock<std::mutex> lock(g_mutex, std::try_to_lock);
    const bool locked = lock.owns_lock();

    std::fprintf(stderr, "\n[census] ===== accepted-route PS2 API census =====\n");

    const std::uint64_t ee_live = g_ee_live_translations.load(std::memory_order_relaxed);
    std::fprintf(stderr, "[census] EE   : live guest translations = %llu        (%s)\n",
                 static_cast<unsigned long long>(ee_live), ee_live == 0 ? "PASS" : "must be 0");

    const std::uint64_t vu0_fb = g_vu0_fallbacks.load(std::memory_order_relaxed);
    const std::uint64_t vu1_fb = g_vu1_fallbacks.load(std::memory_order_relaxed);
    std::fprintf(stderr, "[census] VU   : interpreter fallbacks = %llu (vu0 %llu, vu1 %llu) (%s)\n",
                 static_cast<unsigned long long>(vu0_fb + vu1_fb),
                 static_cast<unsigned long long>(vu0_fb),
                 static_cast<unsigned long long>(vu1_fb),
                 (vu0_fb + vu1_fb == 0) ? "PASS" : "must be 0");

    const std::uint64_t iop_insns = g_iop_instructions.load(std::memory_order_relaxed);
    std::fprintf(stderr, "[census] IOP  : guest instructions = %llu       (%s)\n",
                 static_cast<unsigned long long>(iop_insns), iop_insns == 0 ? "PASS" : "must be 0");

    // IOP module attribution
    const auto &modules = iop::kernel::modules();
    const std::size_t total_samples = std::min(g_iop_sample_count.load(std::memory_order_relaxed), kMaxSamples);
    if (total_samples > 0) {
        std::map<std::string, std::size_t> module_hits;
        std::size_t unknown_hits = 0;
        for (std::size_t i = 0; i < total_samples; ++i) {
            const std::uint32_t phys_pc = iop::phys(g_iop_samples[i]);
            std::string mod_name;
            for (const auto &[id, mod] : modules) {
                if (phys_pc >= mod.base && phys_pc < mod.base + mod.size) {
                    mod_name = mod.name;
                    break;
                }
            }
            if (!mod_name.empty()) module_hits[mod_name]++;
            else unknown_hits++;
        }

        std::fprintf(stderr, "[census] IOP  : guest execution by module (sampled):");
        for (const auto &[name, hits] : module_hits) {
            const double pct = (static_cast<double>(hits) / total_samples) * 100.0;
            std::fprintf(stderr, " %s %.1f%%", name.c_str(), pct);
        }
        if (unknown_hits > 0) {
            const double pct = (static_cast<double>(unknown_hits) / total_samples) * 100.0;
            std::fprintf(stderr, " unknown %.1f%%", pct);
        }
        std::fprintf(stderr, "\n");
    }

    if (!modules.empty()) {
        std::fprintf(stderr, "[census] IOP  : guest modules loaded:");
        for (const auto &[id, mod] : modules) {
            std::fprintf(stderr, " %s 0x%08X", mod.name.c_str(), mod.base);
        }
        std::fprintf(stderr, "\n");
    }

    const std::uint64_t sif_nat = g_sif_native.load(std::memory_order_relaxed);
    const std::uint64_t sif_gst = g_sif_guest.load(std::memory_order_relaxed);
    const std::uint64_t sif_unk = g_sif_unknown.load(std::memory_order_relaxed);
    std::fprintf(stderr, "[census] SIF  : RPCs dispatched natively = %llu, through guest-registered servers = %llu, unknown = %llu\n",
                 static_cast<unsigned long long>(sif_nat),
                 static_cast<unsigned long long>(sif_gst),
                 static_cast<unsigned long long>(sif_unk));

    if (locked && sif_gst > 0) {
        std::fprintf(stderr, "[census] SIF  : guest-handled RPCs:");
        for (const auto &[key, val] : g_sif_rpcs) {
            if (val.first == RpcKind::Guest) {
                std::fprintf(stderr, " sid 0x%08X fn %u x%llu", key.first, key.second,
                             static_cast<unsigned long long>(val.second));
            }
        }
        std::fprintf(stderr, "\n");
    }

    const std::uint64_t gs_app = g_gs_transfers_applied.load(std::memory_order_relaxed);
    const std::uint64_t gs_skp = g_gs_transfers_skipped.load(std::memory_order_relaxed);
    const std::uint64_t gs_prm = g_gs_primitives.load(std::memory_order_relaxed);
    std::fprintf(stderr, "[census] GS   : transfers reached the native surface = %llu, stopped in emulated VRAM = %llu; primitives = %llu\n",
                 static_cast<unsigned long long>(gs_app),
                 static_cast<unsigned long long>(gs_skp),
                 static_cast<unsigned long long>(gs_prm));

    const std::uint64_t blit_ok = g_movie_blit_ran.load(std::memory_order_relaxed);
    const std::uint64_t blit_skip = g_movie_blit_skipped.load(std::memory_order_relaxed);
    if (blit_ok > 0 || blit_skip > 0) {
        std::fprintf(stderr, "[census] MOVIE : presents with blit = %llu, blit skipped = %llu\n",
                     static_cast<unsigned long long>(blit_ok),
                     static_cast<unsigned long long>(blit_skip));
    }

    const std::uint64_t kon = g_spu2_key_ons.load(std::memory_order_relaxed);
    const std::uint64_t koff = g_spu2_key_offs.load(std::memory_order_relaxed);
    const std::uint64_t dma_w = g_spu2_dma_writes.load(std::memory_order_relaxed);
    const std::uint64_t dma_wb = g_spu2_dma_write_bytes.load(std::memory_order_relaxed);
    const std::uint64_t dma_r = g_spu2_dma_reads.load(std::memory_order_relaxed);
    const std::uint64_t spu2_ops = g_spu2_device_ops.load(std::memory_order_relaxed);
    const std::int32_t peak = g_spu2_peak_sample_val.load(std::memory_order_relaxed);
    std::fprintf(stderr, "[census] SPU2 : chip-model ops at the device boundary = %llu; key-ons = %llu key-offs = %llu DMA writes = %llu (%.1f MB) reads = %llu peak sample = %d\n",
                 static_cast<unsigned long long>(spu2_ops),
                 static_cast<unsigned long long>(kon),
                 static_cast<unsigned long long>(koff),
                 static_cast<unsigned long long>(dma_w),
                 static_cast<double>(dma_wb) / (1024.0 * 1024.0),
                 static_cast<unsigned long long>(dma_r),
                 peak);

    const std::uint64_t nat_audio_frames = g_audio_native_frames_mixed.load(std::memory_order_relaxed);
    const std::uint64_t nat_audio_blocks = g_audio_native_blocks_consumed.load(std::memory_order_relaxed);
    const std::uint64_t nat_voices_keyed = g_audio_native_voices_keyed.load(std::memory_order_relaxed);
    const std::uint64_t nat_voice_frames = g_audio_native_voice_frames_rendered.load(std::memory_order_relaxed);
    const std::uint64_t nat_adpcm_blocks = g_audio_native_adpcm_blocks_decoded.load(std::memory_order_relaxed);
    std::fprintf(stderr, "[census] AUDIO: native frames mixed = %llu, blocks consumed = %llu; voices keyed = %llu, voice frames = %llu, ADPCM blocks = %llu\n",
                 static_cast<unsigned long long>(nat_audio_frames),
                 static_cast<unsigned long long>(nat_audio_blocks),
                 static_cast<unsigned long long>(nat_voices_keyed),
                 static_cast<unsigned long long>(nat_voice_frames),
                 static_cast<unsigned long long>(nat_adpcm_blocks));

    if (locked && !g_card_rpcs_by_method.empty()) {
        std::fprintf(stderr, "[census] CARD : RPCs by method:");
        for (const auto &[method, counts] : g_card_rpcs_by_method) {
            std::fprintf(stderr, " 0x%02X ok=%llu fail=%llu",
                         method,
                         static_cast<unsigned long long>(counts.first),
                         static_cast<unsigned long long>(counts.second));
        }
        std::fprintf(stderr, "\n");
    }

    const std::uint64_t cdvd_reads = g_cdvd_sector_reads.load(std::memory_order_relaxed);
    const std::uint64_t volume_reads = g_content_volume_reads.load(std::memory_order_relaxed);
    std::fprintf(stderr, "[census] CDVD : device sector reads = %llu                   (%s); native content volume reads = %llu\n",
                 static_cast<unsigned long long>(cdvd_reads),
                 cdvd_reads == 0 ? "no CDVD device on route" : "gate violation: CDVD device on route",
                 static_cast<unsigned long long>(volume_reads));

    // What the native GL path refused to draw. A rejection is logged once per site, so the log
    // alone hides the scale; these counts are the size of the rendering gap (dropped packets).
    if (!g_gs_packet_rejected.empty()) {
        std::fprintf(stderr, "[census] GS   : packets dropped by the native GL path (not drawn):");
        for (const auto &[why, n] : g_gs_packet_rejected)
            std::fprintf(stderr, " [%s]=%llu", why, static_cast<unsigned long long>(n));
        std::fprintf(stderr, "\n");
        std::fflush(stderr);
    }

    // The acceptance gate, stated where every run record reads it: no interpreter and no emulated
    // PS2 device on the accepted route. Interpreter absence alone is not the gate - the emulated
    // GS/VRAM and SPU2 chip are devices, and they are violations until they leave the route.
    std::fprintf(stderr,
                 "[census] GATE  : emulated PS2 devices on the accepted route (each must reach 0): "
                 "GS-stopped-in-VRAM=%llu SPU2-chip-model-ops=%llu SIF-guest-registered=%llu CDVD-device-reads=%llu\n",
                 static_cast<unsigned long long>(gs_skp),
                 static_cast<unsigned long long>(spu2_ops),
                 static_cast<unsigned long long>(sif_gst),
                 static_cast<unsigned long long>(cdvd_reads));

    // Verdict summary
    int violations = 0;
    std::string viol_list;
    if (ee_live > 0) { violations++; viol_list += "EE live translation, "; }
    if (vu0_fb + vu1_fb > 0) { violations++; viol_list += "VU interpreter fallback, "; }
    if (iop_insns > 0) { violations++; viol_list += "IOP guest execution, "; }
    if (sif_gst > 0) { violations++; viol_list += "SIF RPCs through guest-registered servers, "; }
    if (cdvd_reads > 0) { violations++; viol_list += "CDVD device reads, "; }
    if (gs_skp > 0) { violations++; viol_list += "GS transfers stopped in emulated VRAM, "; }
    if (spu2_ops > 0) { violations++; viol_list += "SPU2 chip-model operations, "; }

    if (violations > 0) {
        if (!viol_list.empty()) viol_list.resize(viol_list.size() - 2); // trim trailing comma
        std::fprintf(stderr, "[census] VERDICT: %d acceptance-gate violations remain: %s\n", violations, viol_list.c_str());
    } else {
        std::fprintf(stderr, "[census] VERDICT: ACCEPTANCE GATE MET (no interpreter and no emulated PS2 device on the accepted route)\n");
    }
    std::fprintf(stderr, "=================================================\n");
    std::fflush(stderr);

    // Verbose transfer breakdown printed after the verdict. Kept complete: the table is small
    // (~1.5 KB) and it is the detail that identifies which bases a run actually wrote, which is
    // what the movie diagnosis needed. Truncation is a flush problem, not a size problem.
    if (locked && !g_gs_transfers_by_base.empty()) {
        std::fprintf(stderr, "[census] GS   : transfers by base:");
        for (const auto &[base, counts] : g_gs_transfers_by_base) {
            std::fprintf(stderr, " base=%u(fbp=%u):app=%llu,skp=%llu",
                         base, base / 32u,
                         static_cast<unsigned long long>(counts.first),
                         static_cast<unsigned long long>(counts.second));
        }
        std::fprintf(stderr, "\n");
        std::fflush(stderr);
    }
    if (g_exit_flush_hook) g_exit_flush_hook();
}

namespace {

struct CensusShutdownHook {
    CensusShutdownHook() {
        std::atexit([]() {
            report();
        });
        std::signal(SIGABRT, [](int sig) {
            report();
            std::signal(sig, SIG_DFL);
            std::raise(sig);
        });
        std::signal(SIGTERM, [](int sig) {
            report();
            std::signal(sig, SIG_DFL);
            std::raise(sig);
        });
    }
};

static CensusShutdownHook s_census_hook;

} // namespace

} // namespace ps2::census
