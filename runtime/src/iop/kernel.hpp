#pragma once

#include "iop.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace ps2::state {
class Stream;
}

namespace ps2::iop::kernel {

void state(ps2::state::Stream &s);
void libs_state(ps2::state::Stream &s);
void cdvd_state(ps2::state::Stream &s);
void cdvdfsv_state(ps2::state::Stream &s);

enum Status { kRun = 1, kReady = 2, kWait = 4, kSuspend = 8, kWaitSuspend = 12, kDormant = 16 };
enum WaitKind { kWaitNone, kWaitSleep, kWaitDelay, kWaitSema, kWaitEvent, kWaitMbx, kWaitFpl, kWaitVblank, kWaitRpc, kWaitHle, kWaitVpl };

struct Thread {
    int id{};
    int status{kDormant};
    int priority{};
    int init_priority{};
    std::uint32_t entry{}, stack{}, stack_size{}, gp{}, attr{}, option{};
    int wakeups{};
    WaitKind wait{kWaitNone};
    int wait_id{};
    std::uint64_t order{};
    Cpu context{};
    std::uint32_t rpc_queue{};
    std::uint32_t rpc_server{};
    std::uint32_t event_bits{}, event_mode{}, event_result{};
    std::uint32_t message_out{};
    bool suspended{};
    std::function<void()> on_exit;
};

void fault(const char *reason);
std::uint32_t fetch_special(std::uint32_t pc);
void syscall(Cpu &c, std::uint32_t pc);
void set_status(Cpu &c, std::uint32_t value);
bool switch_pending();

std::uint64_t now();
std::uint64_t usec_to_cycles(std::uint64_t usec);
void schedule(std::uint64_t at, std::function<void()> action);

enum JobKind : std::uint32_t {
    kJobNone,
    kJobHardTimer,
    kJobAlarm,
    kJobDeleteThread,
    kJobDelayWake,
    kJobSpuTick,
    kJobDmaDone,
    kJobCdRead,
    kJobFsvRead,
    kJobFsvComplete,
    kJobFsvIopRead,
    kJobSifDmaCallback,
    kJobSifReply,
    kJobMwfilefrBindPoll,
    kJobMwfilefrWork,
    kJobMwfilefrCallbackPoll,
    kJobCount
};
struct Job {
    std::uint32_t kind = kJobNone;
    std::uint32_t a = 0, b = 0, c = 0, d = 0;
};
using JobHandler = void (*)(const Job &);
void register_job(std::uint32_t kind, JobHandler handler);
void schedule(std::uint64_t at, const Job &job);

Thread *current();
Thread *thread(int id);
int create_thread(std::uint32_t entry, std::uint32_t stack_size, int priority, std::uint32_t gp, std::uint32_t attr, std::uint32_t option);
void start_thread(int id, std::uint32_t a0, std::uint32_t a1 = 0);
bool delete_thread(int id);
void ready(Thread &t);
void block(Cpu &c, WaitKind kind, int id);
void wake(Thread &t, std::uint32_t result);
void reschedule();
bool in_interrupt();
std::uint32_t call_guest(std::uint32_t address, std::uint32_t a0 = 0, std::uint32_t a1 = 0, std::uint32_t a2 = 0, std::uint32_t a3 = 0);

std::uint32_t alloc(std::uint32_t size, int mode = 0, std::uint32_t at = 0);
void free(std::uint32_t address);
std::uint32_t free_total();
std::uint32_t free_max();
std::uint32_t kernel_alloc(std::uint32_t size);
std::uint32_t hle_stub(const HleFunction &function);

void register_library(const std::string &name, std::uint32_t table);
void register_hle_library(const std::string &name, std::uint32_t version);
bool library_exists(const std::string &name);

// Resolve the registry key for a module load. Buffer-loaded IRXs are named by
// their IOPMOD header, not by the SIF loader's placeholder path ("buffer").
bool resolve_native_module_name(const std::vector<std::uint8_t> &data, const std::string &path, std::string &name);

struct Module {
    int id;
    std::string name;
    std::uint32_t base, size, entry, gp;
};
const std::map<int, Module> &modules();

// A native IOP module replaces a selected guest module with host C++: start is
// a host-side activation callback and exports are direct host functions. These
// callbacks are not IOP HLE opcodes or guest-memory jump stubs. An offline IRX
// to C++ generator is not present yet; current coverage is per registered
// service, and direct import dispatch is only exercised by synthetic tests.
using NativeIopFunction = std::function<void(Cpu &)>;
using NativeIopModuleStart = std::function<bool(const std::string &)>;
struct NativeIopExport {
    std::uint32_t ordinal{};
    NativeIopFunction function;
};
struct NativeIopLibrary {
    std::string name;
    std::uint16_t version{};
    std::vector<NativeIopExport> exports;
};
struct NativeIopModule {
    std::string name;
    // Filename and IOPMOD-header spellings that resolve to this canonical name.
    std::vector<std::string> aliases;
    NativeIopModuleStart start;
    std::vector<NativeIopLibrary> libraries;
};
void register_native_module(NativeIopModule module);
// Direct entry used by AOT-generated IOP C++ callers. In the mixed diagnostic
// runtime, link_imports can also route a matching guest import stub through an
// HLE trampoline to this native export. That path still requires guest IOP
// execution and is rejected by native-strict mode; no production IOP AOT caller
// exists yet. In native-strict mode, an unresolved direct import stops.
bool invoke_native_import(const std::string &library, std::uint16_t version, std::uint32_t ordinal, Cpu &c);
// Installs built-in host-native module/service descriptors. Called during
// IOP kernel boot; this does not load or execute guest IRX instructions.
void install_native_modules();

// Legacy callback seam retained for non-strict diagnostics. Strict native
// module loading intentionally does not accept callback-only registrations.
using HleModuleStart = std::function<void()>;
void register_hle_module(const std::string &name, HleModuleStart start);
// Opt-in fail-fast check for routes that must not load or execute guest IRX code.
bool native_strict();

void register_interrupt(unsigned line, std::uint32_t handler, std::uint32_t arg, std::uint32_t mode);
void release_interrupt(unsigned line);
void enable_interrupt(unsigned line, bool enabled);
void interrupt_state(bool suspended);
bool interrupts_suspended();
void deliver_interrupts();

void install_libraries();
void vblank_edge(bool start);
void install_sif();
void install_cdvdfsv();
void install_cdvd();
void install_io();

void trace_hle(const char *library, const char *function, const Cpu &c);
extern bool g_trace_hle;
extern bool g_trace_modules;

}
