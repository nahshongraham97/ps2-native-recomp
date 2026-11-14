#include "kernel.hpp"
#include "sif.hpp"

#include "mka/content_vfs.hpp"
#include "ps2/memory.hpp"
#include "ps2/state.hpp"
#include "ps2/system.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <functional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ps2::iop::kernel {

namespace {

constexpr std::uint32_t kCdServerInit = 0x80000592u;
constexpr std::uint32_t kCdServerS = 0x80000593u;
constexpr std::uint32_t kCdServerN = 0x80000595u;
constexpr std::uint32_t kCdServerD = 0x80000596u;
constexpr std::uint32_t kCdServerSearch = 0x80000597u;
constexpr std::uint32_t kCdServerReadyA = 0x8000059Au;
constexpr std::uint32_t kCdServerReadyB = 0x8000059Cu;

// One SIF read moves whole sectors through main memory. These caps keep a bad
// count from becoming a multi-gigabyte host allocation; a request past them is
// refused and logged instead of being silently truncated.
constexpr std::uint32_t kMaxEeReadSectors = 1024;   // 2 MiB of the 32 MiB EE RAM
constexpr std::uint32_t kMaxIopReadSectors = 512;   // 1 MiB, half of IOP RAM

struct ReadState {
    bool busy = false;
    std::uint32_t callback = 0;
    std::uint32_t error = 0;
    std::vector<int> sync_waiters;
};

// The IOP library stream (sceCdSt*). A read takes the next sectors of the
// volume and the ring buffer always counts as full.
struct StreamState {
    std::uint32_t buffer_sectors = 0;
    std::uint32_t bank_sectors = 0;
    std::uint32_t buffer_address = 0;
    std::uint32_t position = 0;
    bool active = false;
};

// The EE-facing libcdvd stream (RPC 0x80000595 method 9).
struct FsvStreamState {
    bool active = false;
    std::uint32_t position = 0;
};

ReadState g_cd;
StreamState g_st;
FsvStreamState g_fsv;

std::set<std::pair<std::uint32_t, std::uint32_t>> g_logged_methods;
std::set<std::pair<std::uint32_t, std::uint32_t>> g_unknown_methods;
bool g_logged_no_volume = false;

std::uint32_t ee32(std::uint32_t a) {
    return load<std::uint32_t>(ps2::g_ram + (a & (ps2::kRamSize - 1)));
}
void set_ee32(std::uint32_t a, std::uint32_t v) {
    store<std::uint32_t>(ps2::g_ram + (a & (ps2::kRamSize - 1)), v);
}

std::string ee_string(std::uint32_t address, std::size_t limit) {
    const std::uint32_t at = address & (ps2::kRamSize - 1);
    const std::size_t available = ps2::kRamSize - at;
    const std::size_t count = std::min(limit, available);
    const char *text = reinterpret_cast<const char *>(ps2::g_ram + at);
    std::size_t length = 0;
    while (length < count && text[length] != '\0') ++length;
    return std::string(text, length);
}

std::uint8_t bcd(int v) { return static_cast<std::uint8_t>(((v / 10) << 4) | (v % 10)); }

void log_method(std::uint32_t sid, std::uint32_t method) {
    if (g_logged_methods.emplace(sid, method).second)
        std::fprintf(stderr, "[cdvd-native] sid 0x%08X method 0x%X\n", sid, method);
}

void log_unknown(std::uint32_t sid, std::uint32_t method) {
    if (g_unknown_methods.emplace(sid, method).second)
        std::fprintf(stderr, "[cdvd-native-stop] sid 0x%08X method 0x%X has no native implementation\n", sid, method);
}

bool volume_available() {
    if (mka::content::volume_ready()) return true;
    if (!g_logged_no_volume) {
        std::fprintf(stderr,
                     "[cdvd-native-stop] no sector-addressed volume is indexed; CDVD reads cannot be served\n");
        g_logged_no_volume = true;
    }
    return false;
}

std::uint64_t read_cycles(std::uint32_t sectors) {
    return usec_to_cycles(400) + static_cast<std::uint64_t>(sectors) * usec_to_cycles(250);
}

std::uint64_t fsv_cycles(std::uint32_t sectors) {
    return usec_to_cycles(400) + static_cast<std::uint64_t>(sectors) * usec_to_cycles(200);
}

// Copy sectors from the volume into EE main memory.
void read_to_ee(std::uint32_t sector, std::uint32_t count, std::uint32_t destination) {
    std::vector<std::uint8_t> data(static_cast<std::size_t>(count) * mka::content::kSectorSize);
    if (!mka::content::read_sectors(sector, count, data.data())) {
        if (!volume_available()) return;
        std::fprintf(stderr, "[cdvd-native] read of %u sectors at %u runs past the volume; the tail is zero\n", count,
                     sector);
    }
    const std::uint32_t at = destination & (ps2::kRamSize - 1);
    const std::size_t bytes = std::min<std::size_t>(data.size(), ps2::kRamSize - at);
    if (bytes) std::memcpy(ps2::g_ram + at, data.data(), bytes);
    ps2::watch_span(at, static_cast<std::uint32_t>(bytes), "cdvd:read");
}

// Copy sectors from the volume into IOP memory.
void read_to_iop(std::uint32_t sector, std::uint32_t count, std::uint32_t destination) {
    std::vector<std::uint8_t> data(static_cast<std::size_t>(count) * mka::content::kSectorSize);
    if (!mka::content::read_sectors(sector, count, data.data()) && !volume_available()) return;
    for (std::size_t i = 0; i < data.size(); ++i) write8(destination + static_cast<std::uint32_t>(i), data[i]);
}

void finish_read() {
    g_cd.busy = false;
    for (int id : g_cd.sync_waiters)
        if (Thread *t = thread(id)) wake(*t, 0);
    if (!g_cd.sync_waiters.empty()) reschedule();
    g_cd.sync_waiters.clear();
    if (g_cd.callback) call_guest(g_cd.callback, 1);
}

std::uint32_t arg(const Cpu &c, int i) {
    return i < 4 ? c.r[4 + i] : read32(c.r[29] + 16u + static_cast<std::uint32_t>(i - 4) * 4u);
}

void def(const char *library, std::uint32_t index, const char *name, std::function<void(Cpu &)> body) {
    register_hle(library, index, [library, name, body = std::move(body)](Cpu &c) {
        trace_hle(library, name, c);
        body(c);
    });
}

bool start_read(std::uint32_t lsn, std::uint32_t sectors, std::uint32_t buffer, std::uint32_t mode, bool ee_memory);

bool start_read(std::uint32_t lsn, std::uint32_t sectors, std::uint32_t buffer, std::uint32_t, bool) {
    if (g_cd.busy) return false;
    if (sectors > kMaxIopReadSectors) {
        std::fprintf(stderr, "[cdvd-native-stop] sceCdRead asks for %u sectors (limit %u)\n", sectors, kMaxIopReadSectors);
        return false;
    }
    g_cd.busy = true;
    g_cd.error = 0;
    schedule(now() + read_cycles(sectors), Job{kJobCdRead, lsn, sectors, buffer});
    return true;
}

} // namespace

void install_cdvd_native() {
    g_cd = ReadState{};
    g_st = StreamState{};
    g_fsv = FsvStreamState{};
    g_logged_methods.clear();
    g_unknown_methods.clear();
    g_logged_no_volume = false;

    std::fprintf(stderr,
                 "[cdvd-native] transitional: the guest libcdvd SIF endpoint and the IOP cdvdman library are served "
                 "from the native content volume; no disc image, CDVD device or guest CDVD module is used\n");
    if (!volume_available())
        std::fprintf(stderr, "[cdvd-native-stop] content volume missing at install time\n");

    // ---- asynchronous reads -------------------------------------------------
    register_job(kJobCdRead, [](const Job &j) {
        read_to_iop(j.a, j.b, j.c);
        finish_read();
    });
    register_job(kJobFsvRead, [](const Job &j) {
        read_to_ee(j.a, j.b, j.c);
        sif::complete_call(kCdServerN);
    });
    register_job(kJobFsvComplete, [](const Job &) { sif::complete_call(kCdServerN); });
    register_job(kJobFsvIopRead, [](const Job &j) {
        read_to_iop(j.a, j.b, j.c);
        sif::complete_call(kCdServerN);
    });

    // ---- IOP library replacements (cdvdman v1.1) ----------------------------
    register_hle_library("cdvdman", 0x0101);
    def("cdvdman", 4, "sceCdInit", [](Cpu &c) { c.r[2] = 1; });
    def("cdvdman", 5, "sceCdStandby", [](Cpu &c) { c.r[2] = 1; });
    def("cdvdman", 6, "sceCdRead", [](Cpu &c) {
        if (g_trace_hle) std::fprintf(stderr, "[cdvd-native] sceCdRead lsn %u count %u to 0x%08X\n", arg(c, 0), arg(c, 1), arg(c, 2));
        c.r[2] = start_read(arg(c, 0), arg(c, 1), arg(c, 2), arg(c, 3), false) ? 1u : 0u;
    });
    def("cdvdman", 7, "sceCdSeek", [](Cpu &c) { c.r[2] = 1; });
    def("cdvdman", 8, "sceCdGetError", [](Cpu &c) { c.r[2] = g_cd.error; });
    def("cdvdman", 10, "sceCdSearchFile", [](Cpu &c) {
        const std::string name = ram_string(arg(c, 1), 256);
        const auto entry = mka::content::find(name);
        if (!entry || entry->directory) {
            c.r[2] = 0;
            return;
        }
        const std::uint32_t p = arg(c, 0);
        write32(p, entry->sector);
        write32(p + 4, entry->size);
        const std::size_t slash = name.find_last_of("\\/");
        const std::string base = name.substr(slash == std::string::npos ? 0 : slash + 1);
        for (std::uint32_t i = 0; i < 16; ++i) write8(p + 8 + i, i < base.size() ? static_cast<std::uint8_t>(base[i]) : 0);
        c.r[2] = 1;
    });
    def("cdvdman", 11, "sceCdSync", [](Cpu &c) {
        if (arg(c, 0) == 1 || arg(c, 0) == 17) {
            c.r[2] = g_cd.busy ? 1u : 0u;
            return;
        }
        c.r[2] = 0;
        if (!g_cd.busy || !current()) return;
        g_cd.sync_waiters.push_back(current()->id);
        block(c, kWaitHle, 0);
    });
    def("cdvdman", 12, "sceCdGetDiskType", [](Cpu &c) { c.r[2] = 0x14; });
    def("cdvdman", 13, "sceCdDiskReady", [](Cpu &c) { c.r[2] = 2; });
    def("cdvdman", 14, "sceCdTrayReq", [](Cpu &c) {
        if (arg(c, 1)) write32(arg(c, 1), 0);
        c.r[2] = 1;
    });
    def("cdvdman", 15, "sceCdStop", [](Cpu &c) { c.r[2] = 1; });
    def("cdvdman", 22, "sceCdRI", [](Cpu &c) {
        for (std::uint32_t i = 0; i < 8; ++i) write8(arg(c, 0) + i, 0);
        if (arg(c, 1)) write32(arg(c, 1), 0);
        c.r[2] = 1;
    });
    def("cdvdman", 24, "sceCdReadClock", [](Cpu &c) {
        const std::time_t t = static_cast<std::time_t>(rtc_seconds());
        const std::tm *tm = std::gmtime(&t);
        const std::uint32_t p = arg(c, 0);
        write8(p + 0, 0);
        write8(p + 1, bcd(tm->tm_sec));
        write8(p + 2, bcd(tm->tm_min));
        write8(p + 3, bcd((tm->tm_hour + 9) % 24));
        write8(p + 4, 0);
        write8(p + 5, bcd(tm->tm_mday));
        write8(p + 6, bcd(tm->tm_mon + 1));
        write8(p + 7, bcd(tm->tm_year % 100));
        c.r[2] = 1;
    });
    def("cdvdman", 28, "sceCdStatus", [](Cpu &c) { c.r[2] = g_cd.busy ? 0x06u : 0x0Au; });
    def("cdvdman", 37, "sceCdCallback", [](Cpu &c) {
        c.r[2] = g_cd.callback;
        g_cd.callback = arg(c, 0);
    });
    def("cdvdman", 38, "sceCdPause", [](Cpu &c) { c.r[2] = 1; });
    def("cdvdman", 39, "sceCdBreak", [](Cpu &c) { c.r[2] = 1; });
    def("cdvdman", 56, "sceCdStInit", [](Cpu &c) {
        g_st = StreamState{};
        g_st.buffer_sectors = arg(c, 0);
        g_st.bank_sectors = arg(c, 1);
        g_st.buffer_address = arg(c, 2);
        c.r[2] = 1;
    });
    def("cdvdman", 57, "sceCdStRead", [](Cpu &c) {
        const std::uint32_t sectors = arg(c, 0), dst = arg(c, 1), error = arg(c, 3);
        if (error) write32(error, 0);
        if (!g_st.active || sectors == 0 || (dst & 0x1FFFFFFFu) >= kRamSize) {
            c.r[2] = 0;
            return;
        }
        if (sectors > kMaxIopReadSectors) {
            std::fprintf(stderr, "[cdvd-native-stop] sceCdStRead asks for %u sectors (limit %u)\n", sectors, kMaxIopReadSectors);
            if (error) write32(error, 1);
            c.r[2] = 0;
            return;
        }
        read_to_iop(g_st.position, sectors, dst);
        g_st.position += sectors;
        c.r[2] = sectors;
    });
    def("cdvdman", 58, "sceCdStSeek", [](Cpu &c) {
        g_st.position = arg(c, 0);
        c.r[2] = 1;
    });
    def("cdvdman", 59, "sceCdStStart", [](Cpu &c) {
        g_st.position = arg(c, 0);
        g_st.active = true;
        c.r[2] = 1;
    });
    def("cdvdman", 60, "sceCdStStat", [](Cpu &c) { c.r[2] = g_st.active ? g_st.buffer_sectors : 0u; });
    def("cdvdman", 61, "sceCdStStop", [](Cpu &c) {
        g_st.active = false;
        c.r[2] = 1;
    });
    def("cdvdman", 74, "sceCdPowerOff", [](Cpu &c) { c.r[2] = 1; });
    def("cdvdman", 75, "sceCdMmode", [](Cpu &c) { c.r[2] = 1; });
    def("cdvdman", 78, "sceCdPOffCallback", [](Cpu &c) { c.r[2] = 0; });
    def("cdvdman", 83, "sceCdReadDvdDualInfo", [](Cpu &c) {
        write32(arg(c, 0), mka::content::layer1_start() ? 1u : 0u);
        write32(arg(c, 1), mka::content::layer1_start());
        c.r[2] = 1;
    });
    def("cdvdman", 84, "sceCdLayerSearchFile", [](Cpu &c) {
        const std::string name = ram_string(arg(c, 1), 256);
        const auto entry = mka::content::find(name);
        if (!entry || entry->directory) {
            c.r[2] = 0;
            return;
        }
        write32(arg(c, 0), entry->sector);
        write32(arg(c, 0) + 4, entry->size);
        c.r[2] = 1;
    });

    // ---- EE-facing libcdvd RPC servers -------------------------------------
    sif::register_hle_server(kCdServerInit,
        [](std::uint32_t method, std::uint32_t, std::uint32_t, std::uint32_t ret, std::uint32_t ret_size) {
            log_method(kCdServerInit, method);
            const std::uint32_t bytes = std::min<std::uint32_t>(ret_size, 16u);
            for (std::uint32_t i = 0; i < bytes; ++i) ps2::g_ram[(ret + i) & (ps2::kRamSize - 1)] = 0;
            if (bytes >= 16u) set_ee32(ret + 12, 0xFF);  // sceCdInit: 0xFF = initialized, no version negotiation
            return true;
        });
    sif::register_hle_server(kCdServerS,
        [](std::uint32_t method, std::uint32_t, std::uint32_t, std::uint32_t ret, std::uint32_t ret_size) {
            log_method(kCdServerS, method);
            switch (method) {
            case 0x01: {
                if (ret_size < 12) return true;
                const std::time_t t = static_cast<std::time_t>(rtc_seconds());
                const std::tm *tm = std::gmtime(&t);
                set_ee32(ret, 1);
                std::uint8_t *clock = ps2::g_ram + ((ret + 4) & (ps2::kRamSize - 1));
                clock[0] = 0;
                clock[1] = bcd(tm->tm_sec);
                clock[2] = bcd(tm->tm_min);
                clock[3] = bcd((tm->tm_hour + 9) % 24);
                clock[4] = 0;
                clock[5] = bcd(tm->tm_mday);
                clock[6] = bcd(tm->tm_mon + 1);
                clock[7] = bcd(tm->tm_year % 100);
                break;
            }
            case 0x03:
                if (ret_size >= 4) set_ee32(ret, 0x14);
                break;
            case 0x04:
                if (ret_size >= 4) set_ee32(ret, 0);
                break;
            case 0x05:
                if (ret_size >= 8) {
                    set_ee32(ret, 1);
                    set_ee32(ret + 4, 0);
                }
                break;
            case 0x0C:
                if (ret_size >= 4) set_ee32(ret, g_fsv.active ? 0x12u : 0x0Au);
                break;
            case 0x16:
            case 0x22:
                if (ret_size >= 4) set_ee32(ret, 1);
                break;
            case 0x27:
                if (ret_size >= 4) set_ee32(ret, 1);
                if (ret_size >= 8) set_ee32(ret + 4, mka::content::layer1_start() ? 1u : 0u);
                if (ret_size >= 12) set_ee32(ret + 8, mka::content::layer1_start());
                break;
            default:
                log_unknown(kCdServerS, method);
                if (ret_size >= 4) set_ee32(ret, 0);
                break;
            }
            return true;
        });
    sif::register_hle_server(kCdServerN,
        [](std::uint32_t method, std::uint32_t args, std::uint32_t, std::uint32_t ret, std::uint32_t ret_size) {
            log_method(kCdServerN, method);
            const std::uint32_t sector = ee32(args), count = ee32(args + 4), dst = ee32(args + 8);
            switch (method) {
            case 0x01:
                if (ret_size >= 4) set_ee32(ret, 0);
                if (count > kMaxEeReadSectors) {
                    std::fprintf(stderr, "[cdvd-native-stop] SIF read asks for %u sectors (limit %u)\n", count,
                                 kMaxEeReadSectors);
                    if (ret_size >= 4) set_ee32(ret, 1);
                    return true;
                }
                schedule(now() + fsv_cycles(count), Job{kJobFsvRead, sector, count, dst});
                return false;
            case 0x04:
                if (ret_size >= 4) set_ee32(ret, 1);
                return true;
            case 0x05:
                return true;
            case 0x08:
                schedule(now() + fsv_cycles(0), Job{kJobFsvComplete});
                return false;
            case 0x09: {
                const std::uint32_t command = ee32(args + 12);
                switch (command) {
                case 1:
                    g_fsv.position = sector;
                    g_fsv.active = true;
                    set_ee32(ret, 1);
                    return true;
                case 2: {
                    if (count > kMaxEeReadSectors) {
                        std::fprintf(stderr, "[cdvd-native-stop] SIF stream read asks for %u sectors (limit %u)\n", count,
                                     kMaxEeReadSectors);
                        set_ee32(ret, 0);
                        return true;
                    }
                    const std::uint32_t position = g_fsv.position;
                    g_fsv.position += count;
                    set_ee32(ret, count);
                    schedule(now() + fsv_cycles(count), Job{kJobFsvRead, position, count, dst});
                    return false;
                }
                case 3:
                    g_fsv.active = false;
                    set_ee32(ret, 1);
                    return true;
                case 4:
                case 9:
                    g_fsv.position = sector;
                    set_ee32(ret, 1);
                    return true;
                case 5:
                    set_ee32(ret, 1);
                    return true;
                case 6:
                    set_ee32(ret, sector);
                    return true;
                default:
                    std::fprintf(stderr, "[cdvd-native-stop] stream command %u has no native implementation\n", command);
                    set_ee32(ret, 0);
                    return true;
                }
            }
            case 0x0D:
                if (ret_size >= 4) set_ee32(ret, 0);
                if (count > kMaxIopReadSectors) {
                    std::fprintf(stderr, "[cdvd-native-stop] SIF IOP read asks for %u sectors (limit %u)\n", count,
                                 kMaxIopReadSectors);
                    if (ret_size >= 4) set_ee32(ret, 1);
                    return true;
                }
                schedule(now() + fsv_cycles(count), Job{kJobFsvIopRead, sector, count, dst});
                return false;
            case 0x0E:
                if (ret_size >= 4) set_ee32(ret, 2);
                return true;
            default:
                log_unknown(kCdServerN, method);
                if (ret_size >= 4) set_ee32(ret, 0);
                return true;
            }
        });
    sif::register_hle_server(kCdServerD,
        [](std::uint32_t method, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) {
            log_method(kCdServerD, method);
            return true;
        });
    sif::register_hle_server(kCdServerSearch,
        [](std::uint32_t method, std::uint32_t args, std::uint32_t args_size, std::uint32_t ret, std::uint32_t) {
            log_method(kCdServerSearch, method);
            const std::uint32_t offset = args_size == 0x124u ? 0x20u : 0x24u;
            if (args_size <= offset) {
                std::fprintf(stderr, "[cdvd-native-stop] search request of %u bytes has no path field\n", args_size);
                set_ee32(ret, 0);
                return true;
            }
            const std::string path = ee_string(args + offset, 256);
            const auto entry = mka::content::find(path);
            if (!entry || entry->directory) {
                static std::set<std::string> missing;
                if (missing.insert(path).second)
                    std::fprintf(stderr, "[cdvd-native] search %s -> missing\n", path.c_str());
                set_ee32(ret, 0);
                return true;
            }
            set_ee32(args, entry->sector);
            set_ee32(args + 4, entry->size);
            static std::set<std::string> found;
            if (found.insert(path).second)
                std::fprintf(stderr, "[cdvd-native] search %s -> sector %u size %u\n", path.c_str(), entry->sector,
                             entry->size);
            set_ee32(ret, 1);
            return true;
        });
    const auto ready = [](std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t ret, std::uint32_t ret_size) {
        if (ret_size >= 4) set_ee32(ret, 2);
        return true;
    };
    sif::register_hle_server(kCdServerReadyA, ready);
    sif::register_hle_server(kCdServerReadyB, ready);
}

void cdvd_state(ps2::state::Stream &s) {
    s.tag("cdvd");
    s.pod(g_cd.busy);
    s.pod(g_cd.callback);
    s.pod(g_cd.error);
    s.vec(g_cd.sync_waiters);
    if (s.version() >= 6) s.pod(g_st);
}

void cdvdfsv_state(ps2::state::Stream &s) {
    s.tag("cdvdfsv");
    s.pod(g_fsv.active);
    s.pod(g_fsv.position);
}

} // namespace ps2::iop::kernel
