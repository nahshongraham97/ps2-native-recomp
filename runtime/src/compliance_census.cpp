#include "compliance_census.hpp"

#include <algorithm>
#include <atomic>
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

std::atomic<std::uint64_t> g_spu2_key_ons{0};
std::atomic<std::uint64_t> g_spu2_key_offs{0};
std::atomic<std::uint64_t> g_spu2_dma_writes{0};
std::atomic<std::uint64_t> g_spu2_dma_write_bytes{0};
std::atomic<std::uint64_t> g_spu2_dma_reads{0};
std::atomic<std::uint64_t> g_spu2_dma_read_bytes{0};
std::atomic<std::int32_t>  g_spu2_peak_sample_val{0};

std::atomic<std::uint64_t> g_cdvd_sector_reads{0};

std::mutex g_mutex;
std::map<std::pair<std::uint32_t, std::uint32_t>, std::pair<RpcKind, std::uint64_t>> g_sif_rpcs;
std::map<std::uint32_t, std::pair<std::uint64_t, std::uint64_t>> g_gs_transfers_by_base; // <applied, skipped>
std::map<std::uint32_t, std::uint64_t> g_gs_primitives_by_target;
std::map<std::uint32_t, std::pair<std::uint64_t, std::uint64_t>> g_card_rpcs_by_method; // <ok, fail>

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
    (void)pc;
    g_ee_live_translations.fetch_add(1, std::memory_order_relaxed);
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
    g_cdvd_sector_reads.fetch_add(sectors, std::memory_order_relaxed);
}

void report() {
    static std::atomic<bool> reported{false};
    if (reported.exchange(true)) return;
    std::lock_guard<std::mutex> lock(g_mutex);

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
    std::fprintf(stderr, "[census] SIF  : RPCs native = %llu guest = %llu unknown = %llu\n",
                 static_cast<unsigned long long>(sif_nat),
                 static_cast<unsigned long long>(sif_gst),
                 static_cast<unsigned long long>(sif_unk));

    if (sif_gst > 0) {
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
    std::fprintf(stderr, "[census] GS   : transfers applied = %llu skipped = %llu; primitives = %llu\n",
                 static_cast<unsigned long long>(gs_app),
                 static_cast<unsigned long long>(gs_skp),
                 static_cast<unsigned long long>(gs_prm));
    if (!g_gs_transfers_by_base.empty()) {
        std::fprintf(stderr, "[census] GS   : transfers by base:");
        for (const auto &[base, counts] : g_gs_transfers_by_base) {
            std::fprintf(stderr, " base=%u(fbp=%u):app=%llu,skp=%llu",
                         base, base / 32u,
                         static_cast<unsigned long long>(counts.first),
                         static_cast<unsigned long long>(counts.second));
        }
        std::fprintf(stderr, "\n");
    }

    const std::uint64_t kon = g_spu2_key_ons.load(std::memory_order_relaxed);
    const std::uint64_t koff = g_spu2_key_offs.load(std::memory_order_relaxed);
    const std::uint64_t dma_w = g_spu2_dma_writes.load(std::memory_order_relaxed);
    const std::uint64_t dma_wb = g_spu2_dma_write_bytes.load(std::memory_order_relaxed);
    const std::uint64_t dma_r = g_spu2_dma_reads.load(std::memory_order_relaxed);
    const std::int32_t peak = g_spu2_peak_sample_val.load(std::memory_order_relaxed);
    std::fprintf(stderr, "[census] SPU2 : key-ons = %llu key-offs = %llu DMA writes = %llu (%.1f MB) reads = %llu peak sample = %d\n",
                 static_cast<unsigned long long>(kon),
                 static_cast<unsigned long long>(koff),
                 static_cast<unsigned long long>(dma_w),
                 static_cast<double>(dma_wb) / (1024.0 * 1024.0),
                 static_cast<unsigned long long>(dma_r),
                 peak);

    if (!g_card_rpcs_by_method.empty()) {
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
    std::fprintf(stderr, "[census] CDVD : sector reads = %llu                   (%s)\n",
                 static_cast<unsigned long long>(cdvd_reads), cdvd_reads == 0 ? "PASS" : "must be 0");

    // Verdict summary
    int violations = 0;
    std::string viol_list;
    if (ee_live > 0) { violations++; viol_list += "EE live translation, "; }
    if (vu0_fb + vu1_fb > 0) { violations++; viol_list += "VU interpreter fallback, "; }
    if (iop_insns > 0) { violations++; viol_list += "IOP guest execution, "; }
    if (sif_gst > 0) { violations++; viol_list += "SIF guest RPCs, "; }
    if (cdvd_reads > 0) { violations++; viol_list += "CDVD sector reads, "; }

    if (violations > 0) {
        if (!viol_list.empty()) viol_list.resize(viol_list.size() - 2); // trim trailing comma
        std::fprintf(stderr, "[census] VERDICT: %d mandate violations remain: %s\n", violations, viol_list.c_str());
    } else {
        std::fprintf(stderr, "[census] VERDICT: ALL MANDATE CRITERIA SATISFIED (zero guest emulation on accepted route)\n");
    }
    std::fprintf(stderr, "=================================================\n\n");
    std::fflush(stderr);
}

} // namespace ps2::census
