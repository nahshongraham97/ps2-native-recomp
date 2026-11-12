#include "mwfilefr.hpp"

#include "iop.hpp"
#include "kernel.hpp"
#include "sif.hpp"

#include "ps2/disc.hpp"
#include "ps2/memory.hpp"
#include "ps2/state.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace ps2::iop::kernel {

namespace {

constexpr std::uint32_t kMwfilefrSid = 0x000F0001u;
constexpr std::uint32_t kMwfilefrCompletionSid = 0x000F0002u;
constexpr std::uint32_t kMwfilefrEeCallbackSid = 0x000F1002u;
constexpr std::uint32_t kSifRpcClientBytes = 40u;
constexpr std::uint32_t kOpenPathBytes = 0x200u;
constexpr std::uint32_t kOpenRequestBytes = 12u + kOpenPathBytes;
constexpr std::uint32_t kReadRequestBytes = 7u * sizeof(std::uint32_t);
constexpr std::uint32_t kWorkItemBytes = 0x4Cu;
constexpr std::uint32_t kFileHandleBytes = 0x20u;
constexpr std::uint32_t kCallbackSendBytes = 0x40u;
constexpr std::uint32_t kCallbackReplyBytes = sizeof(std::uint32_t);
constexpr std::uint32_t kNoCompletionId = 0xFFFFFFFFu;
constexpr std::uint32_t kMaxCompletionRecords = 4096u;
constexpr std::uint32_t kIrqCallbackAddress = 0x00000C78u;
constexpr std::uint32_t kCallbackRpcMethod = 100u;
constexpr std::uint32_t kOpenErrorNotFound = static_cast<std::uint32_t>(-2);
constexpr std::uint32_t kOpenErrorInvalidHandle = static_cast<std::uint32_t>(-9);
constexpr std::uint32_t kOpenErrorRead = static_cast<std::uint32_t>(-5);

enum class WorkKind : std::uint16_t { Open = 0, Close = 2, ReadStaged = 4, ReadDirect = 5 };

struct CompletionRecord {
    std::uint32_t job_id = 0;
    std::uint32_t bytes_transferred = 0;
    std::uint32_t error_code = 0;
};
static_assert(sizeof(CompletionRecord) == 12u);

struct NativeFile {
    std::string guest_path;
    std::string host_path;
    std::uint32_t handle = 0;
    std::uint64_t size = 0;
    std::uint32_t block_size = 1;
};

struct WorkItem {
    std::uint32_t address = 0;
    std::uint32_t path_buffer = 0;
    std::uint32_t file_handle = 0;
    std::uint32_t destination = 0;
    std::uint32_t offset = 0;
    std::uint32_t requested_size = 0;
    std::uint32_t bytes_transferred = 0;
    std::uint32_t error_code = 0;
    std::uint32_t callback_cookie = 0;
    std::uint32_t completion_id = kNoCompletionId;
    std::uint32_t open_flags = 0;
    std::uint8_t mode = 0;
    WorkKind kind = WorkKind::Open;
    bool cancelled = false;
    bool completed = false;
    bool callback_queued = false;
    bool callback_finished = false;
    bool release_requested = false;
    std::string guest_path;
};

std::map<std::uint32_t, NativeFile> g_files;
std::map<std::uint32_t, WorkItem> g_work_items;
std::vector<std::uint32_t> g_pending_callbacks;
std::uint32_t g_callback_inflight = 0;
std::uint32_t g_callback_send_buffer = 0;
std::uint32_t g_callback_reply_buffer = 0;
bool g_callback_poll_scheduled = false;
bool g_service_ready = false;
bool g_service_stopping = false;
std::uint8_t g_config_mode = 0;
std::uint32_t g_config_timeout = 0;
std::uint32_t g_config_value = 0;
std::mutex g_completion_mutex;
std::vector<CompletionRecord> g_completion_records;
std::uint32_t g_ee_rpc_client = 0;
bool g_bind_reply_waiting = false;
bool g_bind_poll_scheduled = false;
std::uint64_t g_bind_poll_count = 0;

std::uint32_t ee_physical(std::uint32_t address) { return address & (ps2::kRamSize - 1u); }

bool valid_ee_range(std::uint32_t address, std::uint32_t size) {
    if (size == 0) return true;
    const std::uint32_t physical = ee_physical(address);
    return physical < ps2::kRamSize && size <= ps2::kRamSize - physical;
}

bool valid_iop_range(std::uint32_t address, std::uint32_t size) {
    if (size == 0) return true;
    const std::uint32_t physical = ps2::iop::phys(address) & (ps2::iop::kRamSize - 1u);
    return physical < ps2::iop::kRamSize && size <= ps2::iop::kRamSize - physical;
}

std::uint32_t load_ee32(std::uint32_t address) {
    std::uint32_t value = 0;
    std::memcpy(&value, ps2::g_ram + ee_physical(address), sizeof(value));
    return value;
}

void store_ee32(std::uint32_t address, std::uint32_t value) {
    std::memcpy(ps2::g_ram + ee_physical(address), &value, sizeof(value));
}

std::uint16_t load_le16(const std::uint8_t *bytes) {
    return static_cast<std::uint16_t>(bytes[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[1]) << 8);
}

bool initialize_completion_records(std::uint32_t capacity) {
    if (capacity == 0 || capacity > kMaxCompletionRecords) return false;
    std::lock_guard<std::mutex> lock(g_completion_mutex);
    g_completion_records.assign(capacity, CompletionRecord{});
    return true;
}

bool begin_completion(std::uint32_t job_id) {
    if (job_id == kNoCompletionId) return true;
    std::lock_guard<std::mutex> lock(g_completion_mutex);
    if (g_completion_records.empty()) return false;
    auto &record = g_completion_records[job_id % g_completion_records.size()];
    record = CompletionRecord{job_id, 0u, 0u};
    return true;
}

bool set_completion_progress(std::uint32_t job_id, std::uint32_t bytes_transferred,
                             std::uint32_t error_code = 0u) {
    if (job_id == kNoCompletionId) return true;
    std::lock_guard<std::mutex> lock(g_completion_mutex);
    if (g_completion_records.empty()) return false;
    // FUN_000034D4 writes word 1; the worker error path writes word 2.
    auto &record = g_completion_records[job_id % g_completion_records.size()];
    record.bytes_transferred = bytes_transferred;
    if (error_code) record.error_code = error_code;
    return true;
}

void poll_ee_rpc_bind();

void bind_poll_job(const Job &) { poll_ee_rpc_bind(); }

void schedule_ee_rpc_bind_poll() {
    if (g_bind_poll_scheduled) return;
    g_bind_poll_scheduled = true;
    schedule(now() + usec_to_cycles(2000u), Job{kJobMwfilefrBindPoll});
}

bool ensure_ee_rpc_client() {
    if (g_ee_rpc_client) return true;
    g_ee_rpc_client = alloc(kSifRpcClientBytes);
    if (!g_ee_rpc_client) return false;
    std::memset(ps2::iop::ram_ptr(g_ee_rpc_client), 0, kSifRpcClientBytes);
    return true;
}

void poll_ee_rpc_bind() {
    g_bind_poll_scheduled = false;
    if (!g_bind_reply_waiting || !g_ee_rpc_client) return;

    ++g_bind_poll_count;
    if (sif::native_rpc_bound(g_ee_rpc_client)) {
        g_bind_reply_waiting = false;
        g_service_ready = true;
        std::fprintf(stderr,
                     "[mwfilefr-native] bound EE callback SID 0x%08X after %llu polls "
                     "(EE buffer 0x%08X, server data 0x%08X)\n",
                     kMwfilefrEeCallbackSid,
                     static_cast<unsigned long long>(g_bind_poll_count),
                     ps2::iop::read32(g_ee_rpc_client + 20u),
                     ps2::iop::read32(g_ee_rpc_client + 36u));
        g_bind_poll_count = 0;
        sif::complete_call(kMwfilefrSid);
        return;
    }

    // The retail method-1 path calls blocking sceSifBindRpc in a retry loop.
    // This host-native path leaves the HLE request pending and retries only
    // after the previous bind packet has completed, without running IOP code.
    if (!sif::native_rpc_pending(g_ee_rpc_client)) {
        if (!sif::native_bind_rpc_nowait(g_ee_rpc_client, kMwfilefrEeCallbackSid))
            std::fprintf(stderr, "[mwfilefr-native] SIF bind packet could not be queued; retrying\n");
    }
    schedule_ee_rpc_bind_poll();
}

bool snapshot_completion_dma(std::vector<std::uint8_t> &bytes, std::uint32_t &record_count) {
    std::lock_guard<std::mutex> lock(g_completion_mutex);
    if (g_completion_records.empty()) return false;

    record_count = static_cast<std::uint32_t>(g_completion_records.size());
    const std::size_t record_bytes = g_completion_records.size() * sizeof(CompletionRecord);
    const std::size_t dma_bytes = (record_bytes + 0x3Fu) & ~std::size_t(0x3Fu);
    bytes.assign(dma_bytes, 0u);
    std::memcpy(bytes.data(), g_completion_records.data(), record_bytes);
    return true;
}

bool reject(std::uint32_t method, const char *reason) {
    std::fprintf(stderr, "[mwfilefr-native-stop] SID 0x%08X method %u: %s\n", kMwfilefrSid, method, reason);
    if (native_strict()) {
        std::fflush(stderr);
        std::abort();
    }
    return false;
}

std::optional<std::string> resolve_host_file(const std::string &guest_path) {
    if (const auto overlay = disc::native_vfs_path(guest_path)) return overlay;
    return disc::extracted_file_path(guest_path);
}

void write_iop16(std::uint32_t address, std::uint16_t value) {
    std::memcpy(ps2::iop::ram_ptr(address), &value, sizeof(value));
}

void write_iop32(std::uint32_t address, std::uint32_t value) {
    std::memcpy(ps2::iop::ram_ptr(address), &value, sizeof(value));
}

void write_work_item(const WorkItem &work) {
    write_iop32(work.address + 0x08u, work.bytes_transferred);
    write_iop16(work.address + 0x0Cu, work.cancelled ? 4u : 0u);
    write_iop16(work.address + 0x0Eu, static_cast<std::uint16_t>(work.kind));
    write_iop32(work.address + 0x10u, work.file_handle);
    write_iop32(work.address + 0x14u, work.destination);
    write_iop32(work.address + 0x18u,
                (work.kind == WorkKind::ReadDirect || work.kind == WorkKind::ReadStaged)
                    ? work.mode : work.requested_size);
    write_iop32(work.address + 0x1Cu, work.error_code);
    write_iop32(work.address + 0x20u, work.bytes_transferred);
    write_iop32(work.address + 0x24u, work.requested_size - std::min(work.requested_size, work.bytes_transferred));
    write_iop32(work.address + 0x2Cu, work.offset);
    write_iop32(work.address + 0x3Cu, kIrqCallbackAddress);
    write_iop32(work.address + 0x40u, work.callback_cookie);
    write_iop32(work.address + 0x44u, work.completion_id);
}

void schedule_callback_poll();

void release_work_item(WorkItem &work) {
    if (work.path_buffer) {
        free(work.path_buffer);
        work.path_buffer = 0;
    }
    const std::uint32_t address = work.address;
    g_work_items.erase(address);
    free(address);
}

void queue_work_callback(WorkItem &work) {
    if (work.callback_queued) return;
    work.callback_queued = true;
    g_pending_callbacks.push_back(work.address);
    schedule_callback_poll();
}

void finish_work(WorkItem &work) {
    work.completed = true;
    write_work_item(work);
    if (!set_completion_progress(work.completion_id, work.bytes_transferred, work.error_code)) {
        work.error_code = kOpenErrorRead;
        write_iop32(work.address + 0x1Cu, work.error_code);
    }
    queue_work_callback(work);
}

void process_open(WorkItem &work) {
    if ((work.open_flags & 0x22u) == 0x22u) {
        work.error_code = static_cast<std::uint32_t>(-5);
        finish_work(work);
        return;
    }
    const auto host_path = resolve_host_file(work.guest_path);
    if (!host_path) {
        work.error_code = kOpenErrorNotFound;
        finish_work(work);
        return;
    }

    std::error_code ec;
    const std::uint64_t size = std::filesystem::file_size(*host_path, ec);
    if (ec || !std::filesystem::is_regular_file(*host_path, ec) || ec) {
        work.error_code = kOpenErrorNotFound;
        finish_work(work);
        return;
    }

    const std::uint32_t handle = alloc(kFileHandleBytes);
    if (!handle) {
        work.error_code = kOpenErrorRead;
        finish_work(work);
        return;
    }
    std::memset(ps2::iop::ram_ptr(handle), 0, kFileHandleBytes);
    write_iop32(handle + 4u, static_cast<std::uint32_t>(std::min<std::uint64_t>(size, 0xFFFFFFFFu)));
    write_iop16(handle + 0x18u, 1u);
    g_files.emplace(handle, NativeFile{work.guest_path, *host_path, handle, size, 1u});
    work.file_handle = handle;
    work.requested_size = static_cast<std::uint32_t>(std::min<std::uint64_t>(size, 0xFFFFFFFFu));
    work.destination = work.requested_size;
    if (work.path_buffer) {
        free(work.path_buffer);
        work.path_buffer = 0;
    }
    std::fprintf(stderr, "[mwfilefr-native] opened %s (%llu bytes) at IOP handle 0x%08X -> %s\n",
                 work.guest_path.c_str(), static_cast<unsigned long long>(size), handle, host_path->c_str());
    finish_work(work);
}

void process_close(WorkItem &work) {
    auto file = g_files.find(work.file_handle);
    if (file == g_files.end()) {
        work.error_code = kOpenErrorInvalidHandle;
    } else {
        const std::uint32_t handle = file->first;
        g_files.erase(file);
        free(handle);
        work.file_handle = 0;
    }
    finish_work(work);
}

void process_read(WorkItem &work) {
    const auto file = g_files.find(work.file_handle);
    if (file == g_files.end()) {
        work.error_code = kOpenErrorInvalidHandle;
        finish_work(work);
        return;
    }
    if (!valid_ee_range(work.destination, work.requested_size)) {
        work.error_code = kOpenErrorRead;
        finish_work(work);
        return;
    }

    work.bytes_transferred = 0;
    while (work.bytes_transferred < work.requested_size) {
        const std::uint64_t file_offset = static_cast<std::uint64_t>(work.offset) + work.bytes_transferred;
        if (file_offset >= file->second.size) break;
        const std::uint64_t available = file->second.size - file_offset;
        const std::uint32_t remaining = work.requested_size - work.bytes_transferred;
        const std::uint32_t wanted = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(remaining, std::min<std::uint64_t>(available, 0x8000u)));
        std::ifstream stream(file->second.host_path, std::ios::binary);
        if (!stream) {
            work.error_code = kOpenErrorRead;
            finish_work(work);
            return;
        }
        stream.seekg(static_cast<std::streamoff>(file_offset), std::ios::beg);
        if (!stream) {
            work.error_code = kOpenErrorRead;
            finish_work(work);
            return;
        }
        std::uint8_t *out = ps2::g_ram + ee_physical(work.destination) + work.bytes_transferred;
        stream.read(reinterpret_cast<char *>(out), static_cast<std::streamsize>(wanted));
        if (stream.bad()) {
            work.error_code = kOpenErrorRead;
            finish_work(work);
            return;
        }
        const std::streamsize got = stream.gcount();
        if (got <= 0) break;
        work.bytes_transferred += static_cast<std::uint32_t>(got);
        if (static_cast<std::uint32_t>(got) < wanted) break;
    }
    finish_work(work);
    std::fprintf(stderr, "[mwfilefr-native] read IOP handle 0x%08X offset 0x%08X requested 0x%X -> %u bytes at EE 0x%08X\n",
                 work.file_handle, work.offset, work.requested_size, work.bytes_transferred, work.destination);
}

void work_job(const Job &job) {
    const auto found = g_work_items.find(job.a);
    if (found == g_work_items.end()) return;
    WorkItem &work = found->second;
    if (work.completed) return;
    if (work.cancelled || g_service_stopping) {
        work.error_code = kOpenErrorRead;
        finish_work(work);
        return;
    }

    switch (work.kind) {
    case WorkKind::Open: process_open(work); break;
    case WorkKind::Close: process_close(work); break;
    case WorkKind::ReadStaged:
    case WorkKind::ReadDirect: process_read(work); break;
    }
}

void callback_poll_job(const Job &) {
    g_callback_poll_scheduled = false;
    if (g_callback_inflight) {
        if (sif::native_rpc_pending(g_ee_rpc_client)) {
            schedule_callback_poll();
            return;
        }
        const std::uint32_t finished = g_callback_inflight;
        g_callback_inflight = 0;
        auto it = g_work_items.find(finished);
        if (it != g_work_items.end()) {
            it->second.callback_finished = true;
            if (it->second.release_requested) release_work_item(it->second);
        }
    }

    while (!g_pending_callbacks.empty() && !g_work_items.count(g_pending_callbacks.front()))
        g_pending_callbacks.erase(g_pending_callbacks.begin());
    if (g_pending_callbacks.empty()) return;
    if (!g_ee_rpc_client || !sif::native_rpc_bound(g_ee_rpc_client)) {
        schedule_callback_poll();
        return;
    }

    if (!g_callback_send_buffer) g_callback_send_buffer = alloc(kCallbackSendBytes);
    if (!g_callback_reply_buffer) g_callback_reply_buffer = alloc(kCallbackReplyBytes);
    if (!g_callback_send_buffer || !g_callback_reply_buffer) {
        schedule_callback_poll();
        return;
    }

    const std::uint32_t address = g_pending_callbacks.front();
    g_pending_callbacks.erase(g_pending_callbacks.begin());
    const auto it = g_work_items.find(address);
    if (it == g_work_items.end()) {
        schedule_callback_poll();
        return;
    }
    WorkItem &work = it->second;
    std::uint8_t *send = ps2::iop::ram_ptr(g_callback_send_buffer);
    std::memset(send, 0, kCallbackSendBytes);
    write_iop32(g_callback_send_buffer, work.callback_cookie);
    std::memcpy(send + 4u, ps2::iop::ram_ptr(work.address + 8u), 24u);
    std::memset(ps2::iop::ram_ptr(g_callback_reply_buffer), 0, kCallbackReplyBytes);
    if (!sif::native_call_rpc_nowait(g_ee_rpc_client, kCallbackRpcMethod,
                                     g_callback_send_buffer, kCallbackSendBytes,
                                     g_callback_reply_buffer, kCallbackReplyBytes)) {
        g_pending_callbacks.insert(g_pending_callbacks.begin(), address);
        schedule_callback_poll();
        return;
    }
    g_callback_inflight = address;
    schedule_callback_poll();
}

void schedule_work(WorkItem &work) {
    schedule(now() + usec_to_cycles(1u), Job{kJobMwfilefrWork, work.address});
}

void schedule_callback_poll() {
    if (g_callback_poll_scheduled) return;
    g_callback_poll_scheduled = true;
    schedule(now() + usec_to_cycles(1000u), Job{kJobMwfilefrCallbackPoll});
}

bool valid_reply(std::uint32_t method, std::uint32_t ret, std::uint32_t ret_size) {
    return ret_size == 0u || (ret_size == 4u && valid_ee_range(ret, 4u)) ||
           (reject(method, "reply buffer must be empty or one EE word"), false);
}

void clear_native_service() {
    for (const auto &[address, file] : g_files) {
        (void)file;
        free(address);
    }
    g_files.clear();
    for (auto &[address, work] : g_work_items) {
        if (work.path_buffer) free(work.path_buffer);
        free(address);
    }
    g_work_items.clear();
    g_pending_callbacks.clear();
    g_callback_inflight = 0;
    g_callback_poll_scheduled = false;
    g_service_ready = false;
    g_service_stopping = false;
    g_bind_reply_waiting = false;
    g_bind_poll_scheduled = false;
    g_bind_poll_count = 0;
    g_config_mode = 0;
    g_config_timeout = 0;
    g_config_value = 0;
    std::lock_guard<std::mutex> lock(g_completion_mutex);
    g_completion_records.clear();
}

std::uint32_t create_work(WorkItem work) {
    work.address = alloc(kWorkItemBytes);
    if (!work.address) return 0;
    std::memset(ps2::iop::ram_ptr(work.address), 0, kWorkItemBytes);
    write_iop16(work.address + 0x0Eu, static_cast<std::uint16_t>(work.kind));
    const std::uint32_t address = work.address;
    g_work_items.emplace(address, std::move(work));
    return address;
}

bool mwfilefr_rpc(std::uint32_t method, std::uint32_t args, std::uint32_t args_size,
                  std::uint32_t ret, std::uint32_t ret_size) {
    if (args_size && !valid_ee_range(args, args_size)) return reject(method, "request buffer outside EE RAM");
    if (ret_size && !valid_ee_range(ret, ret_size)) return reject(method, "reply buffer outside EE RAM");
    const std::uint8_t *request = ps2::g_ram + ee_physical(args);

    if (method == 1u) {
        if (args_size != 20u || !valid_reply(method, ret, ret_size))
            return reject(method, "unexpected initialization request size");
        const std::uint32_t capacity = load_le16(request + 4u);
        clear_native_service();
        if (!initialize_completion_records(capacity))
            return reject(method, "invalid or unsupported completion-table capacity");
        if (ret_size == 4u) store_ee32(ret, 0u);
        if (!ensure_ee_rpc_client()) return reject(method, "could not allocate native EE RPC client state");
        if (sif::native_rpc_bound(g_ee_rpc_client)) {
            g_bind_reply_waiting = false;
            g_service_ready = true;
            return true;
        }
        if (!g_bind_reply_waiting) {
            g_bind_reply_waiting = true;
            g_bind_poll_count = 0;
            std::fprintf(stderr,
                         "[mwfilefr-native] waiting for EE callback SID 0x%08X via nonblocking SIF bind\n",
                         kMwfilefrEeCallbackSid);
        }
        schedule_ee_rpc_bind_poll();
        return false;
    }

    if (method == 2u) {
        if (args_size != 0u || !valid_reply(method, ret, ret_size))
            return reject(method, "unexpected shutdown request size");
        g_service_stopping = true;
        clear_native_service();
        if (ret_size == 4u) store_ee32(ret, 0u);
        return true;
    }

    if (method == 4u) {
        if (args_size != 4u || !valid_reply(method, ret, ret_size))
            return reject(method, "unexpected work-item release request size");
        const std::uint32_t address = load_ee32(args);
        const auto it = g_work_items.find(address);
        if (it != g_work_items.end()) {
            it->second.release_requested = true;
            if (it->second.callback_finished) release_work_item(it->second);
        }
        if (ret_size == 4u) store_ee32(ret, 0u);
        return true;
    }

    if (method == 5u || method == 6u || method == 7u || method == 9u) {
        if (!g_service_ready) return reject(method, "file request arrived before native service initialization");
        if (!valid_reply(method, ret, ret_size) || ret_size != 4u)
            return reject(method, "asynchronous request must return one IOP work-item address");
        if (method == 5u && args_size != kOpenRequestBytes)
            return reject(method, "open request must contain the full fixed path buffer");
        if (method == 6u && args_size != 12u)
            return reject(method, "close request has an unexpected size");
        if ((method == 7u || method == 9u) && args_size != kReadRequestBytes)
            return reject(method, "read request has an unexpected size");

        WorkItem work;
        if (method == 5u) {
            work.kind = WorkKind::Open;
            work.callback_cookie = load_ee32(args);
            work.open_flags = load_ee32(args + 4u);
            work.completion_id = load_ee32(args + 8u);
            const char *path = reinterpret_cast<const char *>(request + 12u);
            const void *end = std::memchr(path, 0, kOpenPathBytes);
            if (!end) return reject(method, "open path is not NUL-terminated within 512 bytes");
            work.guest_path.assign(path, static_cast<const char *>(end) - path);
            work.path_buffer = alloc(kOpenPathBytes);
            if (!work.path_buffer) return reject(method, "could not allocate native open path buffer");
            std::memcpy(ps2::iop::ram_ptr(work.path_buffer), path, kOpenPathBytes);
            ps2::iop::ram_ptr(work.path_buffer)[kOpenPathBytes - 1u] = 0;
        } else if (method == 6u) {
            work.kind = WorkKind::Close;
            work.file_handle = load_ee32(args);
            work.callback_cookie = load_ee32(args + 4u);
            work.completion_id = load_ee32(args + 8u);
        } else {
            work.kind = method == 7u ? WorkKind::ReadStaged : WorkKind::ReadDirect;
            work.file_handle = load_ee32(args);
            work.offset = load_ee32(args + 4u);
            work.destination = load_ee32(args + 8u);
            work.requested_size = load_ee32(args + 12u);
            work.mode = request[20u];
            work.callback_cookie = load_ee32(args + 16u);
            work.completion_id = load_ee32(args + 24u);
            if (!valid_ee_range(work.destination, work.requested_size))
                return reject(method, "read destination outside EE RAM");
        }
        if (!begin_completion(work.completion_id)) {
            if (work.path_buffer) free(work.path_buffer);
            return reject(method, "file request arrived before completion-table initialization");
        }
        const std::uint32_t path_buffer = work.path_buffer;
        const std::uint32_t address = create_work(std::move(work));
        if (!address) {
            if (path_buffer) free(path_buffer);
            return reject(method, "could not allocate native work item");
        }
        store_ee32(ret, address);
        schedule_work(g_work_items.at(address));
        return true;
    }

    if (method == 8u) {
        if (args_size != 4u || !valid_reply(method, ret, ret_size))
            return reject(method, "unexpected cancellation request size");
        const std::uint32_t address = load_ee32(args);
        const auto it = g_work_items.find(address);
        if (it != g_work_items.end()) {
            it->second.cancelled = true;
            const std::uint16_t flags = static_cast<std::uint16_t>(
                ps2::iop::read32(address + 0x0Cu) | 4u);
            write_iop16(address + 0x0Cu, flags);
        }
        if (ret_size == 4u) store_ee32(ret, it == g_work_items.end() ? 0xFFFFFFFBu : 0u);
        return true;
    }

    if (method == 11u) {
        if (args_size != 12u || !valid_reply(method, ret, ret_size))
            return reject(method, "unexpected configuration request size");
        g_config_mode = request[0];
        g_config_timeout = std::min(load_ee32(args + 4u), 100u);
        g_config_value = load_ee32(args + 8u);
        if (ret_size == 4u) store_ee32(ret, 0u);
        return true;
    }

    if (method == 12u) {
        if (args_size != 0u || !valid_reply(method, ret, ret_size))
            return reject(method, "unexpected no-op request size");
        if (ret_size == 4u) store_ee32(ret, 0u);
        return true;
    }

    return reject(method, "unobserved MWFILEFR RPC method has no native implementation");
}

bool mwfilefr_completion_rpc(std::uint32_t method, std::uint32_t args, std::uint32_t args_size,
                             std::uint32_t ret, std::uint32_t ret_size) {
    if (args_size && !valid_ee_range(args, args_size))
        return reject(method, "completion request buffer outside EE RAM");
    if (ret_size && !valid_ee_range(ret, ret_size))
        return reject(method, "completion reply buffer outside EE RAM");
    if (method != 200u) return reject(method, "unobserved completion-server RPC method");
    if (args_size != sizeof(std::uint32_t))
        return reject(method, "method 200 request omits its DMA destination word");
    // FUN_00000A08 passes the first request word to FUN_000003E4 as the EE DMA
    // destination. FUN_00003454 snapshots the full persistent capacity*12-byte
    // record table; FUN_000003E4 DMA-copies its 64-byte-aligned size on every
    // poll. The IRX does not clear or consume the table after this copy.
    if (ret_size != 0u)
        return reject(method, "nonzero method 200 reply size is not yet characterized");

    const std::uint32_t destination = load_ee32(args);
    std::vector<std::uint8_t> dma_bytes;
    std::uint32_t record_count = 0;
    if (!snapshot_completion_dma(dma_bytes, record_count))
        return reject(method, "completion poll arrived before completion-table handshake");
    if (dma_bytes.size() > std::numeric_limits<std::uint32_t>::max() ||
        !valid_ee_range(destination, static_cast<std::uint32_t>(dma_bytes.size())))
        return reject(method, "completion DMA destination outside EE RAM");

    std::memcpy(ps2::g_ram + ee_physical(destination), dma_bytes.data(), dma_bytes.size());
    static std::uint64_t polls = 0;
    ++polls;
    if (polls == 1u || polls % 1000u == 0u)
        std::fprintf(stderr,
                     "[mwfilefr-native] completion DMA poll %llu: %u records (%zu bytes) to EE 0x%08X\n",
                     static_cast<unsigned long long>(polls), record_count, dma_bytes.size(), destination);
    return true;
}

} // namespace

void register_native_mwfilefr_job() {
    register_job(kJobMwfilefrBindPoll, bind_poll_job);
    register_job(kJobMwfilefrWork, work_job);
    register_job(kJobMwfilefrCallbackPoll, callback_poll_job);
}

void mwfilefr_state(ps2::state::Stream &s) {
    if (s.version() < 10u) {
        if (s.loading()) {
            clear_native_service();
            g_ee_rpc_client = 0;
            g_bind_reply_waiting = false;
            g_bind_poll_scheduled = false;
            g_bind_poll_count = 0;
        }
        return;
    }

    // Version 10 contained the first experiment's synchronous fake tokens.
    // Consume that exact layout when reading an old state, then discard its
    // handles because those token values never named native allocations.
    if (s.version() == 10u) {
        std::uint32_t legacy_next_file_token = 0;
        s.pod(legacy_next_file_token);
        s.pod(g_ee_rpc_client);
        s.pod(g_bind_reply_waiting);
        s.pod(g_bind_poll_scheduled);
        s.pod(g_bind_poll_count);
        s.map(g_files, [&s](NativeFile &file) {
            s.str(file.guest_path);
            s.str(file.host_path);
            s.pod(file.size);
        });
        {
            std::lock_guard<std::mutex> lock(g_completion_mutex);
            s.vec(g_completion_records, [&s](CompletionRecord &record) {
                s.pod(record.job_id);
                s.pod(record.bytes_transferred);
                s.pod(record.error_code);
            });
        }
        if (s.loading()) clear_native_service();
        return;
    }

    s.tag("mwfile");
    s.pod(g_ee_rpc_client);
    s.pod(g_bind_reply_waiting);
    s.pod(g_bind_poll_scheduled);
    s.pod(g_bind_poll_count);
    s.pod(g_callback_inflight);
    s.pod(g_callback_send_buffer);
    s.pod(g_callback_reply_buffer);
    s.pod(g_callback_poll_scheduled);
    s.pod(g_service_ready);
    s.pod(g_service_stopping);
    s.pod(g_config_mode);
    s.pod(g_config_timeout);
    s.pod(g_config_value);
    s.map(g_files, [&s](NativeFile &file) {
        s.str(file.guest_path);
        s.str(file.host_path);
        s.pod(file.handle);
        s.pod(file.size);
        s.pod(file.block_size);
    });
    s.map(g_work_items, [&s](WorkItem &work) {
        s.pod(work.address);
        s.pod(work.path_buffer);
        s.pod(work.file_handle);
        s.pod(work.destination);
        s.pod(work.offset);
        s.pod(work.requested_size);
        s.pod(work.bytes_transferred);
        s.pod(work.error_code);
        s.pod(work.callback_cookie);
        s.pod(work.completion_id);
        s.pod(work.open_flags);
        s.pod(work.mode);
        s.pod(work.kind);
        s.pod(work.cancelled);
        s.pod(work.completed);
        s.pod(work.callback_queued);
        s.pod(work.callback_finished);
        s.pod(work.release_requested);
        s.str(work.guest_path);
    });
    s.vec(g_pending_callbacks);
    {
        std::lock_guard<std::mutex> lock(g_completion_mutex);
        s.vec(g_completion_records, [&s](CompletionRecord &record) {
            s.pod(record.job_id);
            s.pod(record.bytes_transferred);
            s.pod(record.error_code);
        });
    }
    if (s.loading()) {
        if (g_ee_rpc_client && !valid_iop_range(g_ee_rpc_client, kSifRpcClientBytes))
            throw std::runtime_error("MWFILEFR state contains an invalid native SIF client address");
        if (g_callback_send_buffer && !valid_iop_range(g_callback_send_buffer, kCallbackSendBytes))
            throw std::runtime_error("MWFILEFR state contains an invalid callback send buffer");
        if (g_callback_reply_buffer && !valid_iop_range(g_callback_reply_buffer, kCallbackReplyBytes))
            throw std::runtime_error("MWFILEFR state contains an invalid callback reply buffer");
        for (const auto &[address, file] : g_files) {
            if (!valid_iop_range(address, kFileHandleBytes) || address != file.handle)
                throw std::runtime_error("MWFILEFR state contains an invalid native file handle");
        }
        for (const auto &[address, work] : g_work_items) {
            if (!valid_iop_range(address, kWorkItemBytes) || address != work.address ||
                (work.path_buffer && !valid_iop_range(work.path_buffer, kOpenPathBytes)))
                throw std::runtime_error("MWFILEFR state contains an invalid native work item");
        }
        if (g_callback_inflight && !g_work_items.count(g_callback_inflight))
            throw std::runtime_error("MWFILEFR state callback references a missing work item");
    }
}

void install_native_mwfilefr_service() {
    g_files.clear();
    g_work_items.clear();
    g_pending_callbacks.clear();
    g_callback_inflight = 0;
    g_callback_send_buffer = 0;
    g_callback_reply_buffer = 0;
    g_callback_poll_scheduled = false;
    g_service_ready = false;
    g_service_stopping = false;
    g_config_mode = 0;
    g_config_timeout = 0;
    g_config_value = 0;
    {
        std::lock_guard<std::mutex> lock(g_completion_mutex);
        g_completion_records.clear();
    }
    g_ee_rpc_client = 0;
    g_bind_reply_waiting = false;
    g_bind_poll_scheduled = false;
    g_bind_poll_count = 0;
    sif::register_hle_override(kMwfilefrSid, mwfilefr_rpc);
    sif::register_hle_override(kMwfilefrCompletionSid, mwfilefr_completion_rpc);
    std::fprintf(stderr,
                 "[iop-native] installed MWFILEFR C++ SIF services for SIDs 0x%08X and 0x%08X "
                 "(async native filesystem open/close/read, work-item lifecycle, and EE completion callbacks)\n",
                 kMwfilefrSid, kMwfilefrCompletionSid);
}

} // namespace ps2::iop::kernel
