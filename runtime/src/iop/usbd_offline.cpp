#include "usbd_offline.hpp"

#include "iop.hpp"

#include <cstdint>
#include <cstdio>
#include <utility>

namespace ps2::iop::usbd_offline {

namespace {

constexpr std::uint16_t kUsbdVersion = 0x0102u;
constexpr std::int32_t kUsbOk = 0;
constexpr std::int32_t kUsbBadDevice = 0x101;
constexpr std::int32_t kUsbBadPipe = 0x102;
constexpr std::int32_t kUsbBadDriver = 0x104;
constexpr std::int32_t kUsbBusy = 0x121;

constexpr std::uint32_t kDriverNext = 0x00u;
constexpr std::uint32_t kDriverPrev = 0x04u;
constexpr std::uint32_t kDriverName = 0x08u;
constexpr std::uint32_t kDriverReserved1 = 0x18u;
constexpr std::uint32_t kDriverReserved2 = 0x1Cu;
constexpr std::uint32_t kDriverGp = 0x2Cu;
constexpr std::uint32_t kDriverSize = 0x30u;
constexpr std::uint32_t kMaximumDriverWalk = 256u;

std::uint32_t g_driver_head = 0;
std::uint32_t g_driver_tail = 0;
std::uint32_t g_auto_loader = 0;

bool valid_iop_range(std::uint32_t address, std::uint64_t size) {
    if (size == 0) return true;
    const std::uint32_t physical = phys(address);
    return physical < kRamSize && size <= static_cast<std::uint64_t>(kRamSize - physical);
}

void set_result(Cpu &cpu, std::int32_t result) {
    cpu.r[2] = static_cast<std::uint32_t>(result);
}

bool valid_driver(std::uint32_t address) {
    if (address == 0 || !valid_iop_range(address, kDriverSize)) return false;
    return read32(address + kDriverName) != 0 &&
           read32(address + kDriverReserved1) == 0 &&
           read32(address + kDriverReserved2) == 0;
}

bool registered_driver(std::uint32_t target) {
    std::uint32_t current = g_driver_head;
    for (std::uint32_t walked = 0; current != 0 && walked < kMaximumDriverWalk; ++walked) {
        if (!valid_iop_range(current, kDriverSize)) return false;
        if (current == target) return true;
        current = read32(current + kDriverNext);
    }
    return false;
}

void register_ldd(Cpu &cpu) {
    const std::uint32_t driver = cpu.r[4];
    if (!valid_driver(driver)) {
        set_result(cpu, kUsbBadDriver);
        return;
    }
    if (read32(driver + kDriverNext) != 0 || read32(driver + kDriverPrev) != 0 ||
        registered_driver(driver)) {
        set_result(cpu, registered_driver(driver) && read32(driver + kDriverNext) == 0 &&
                       read32(driver + kDriverPrev) == 0 ? kUsbBusy : kUsbBadDriver);
        return;
    }
    if (g_driver_tail != 0 && !valid_iop_range(g_driver_tail, kDriverSize)) {
        set_result(cpu, kUsbBadDriver);
        return;
    }

    write32(driver + kDriverGp, cpu.r[28]);
    write32(driver + kDriverPrev, g_driver_tail);
    if (g_driver_tail != 0) write32(g_driver_tail + kDriverNext, driver);
    else g_driver_head = driver;
    write32(driver + kDriverNext, 0);
    g_driver_tail = driver;
    set_result(cpu, kUsbOk);
}

void unregister_ldd(Cpu &cpu) {
    const std::uint32_t driver = cpu.r[4];
    if (driver == 0 || !valid_iop_range(driver, kDriverSize)) {
        set_result(cpu, kUsbBadDriver);
        return;
    }

    std::uint32_t current = g_driver_head;
    for (std::uint32_t walked = 0; current != 0 && walked < kMaximumDriverWalk; ++walked) {
        if (!valid_iop_range(current, kDriverSize)) {
            set_result(cpu, kUsbBadDriver);
            return;
        }
        if (current == driver) {
            const std::uint32_t next = read32(driver + kDriverNext);
            const std::uint32_t previous = read32(driver + kDriverPrev);
            if ((next != 0 && !valid_iop_range(next, kDriverSize)) ||
                (previous != 0 && !valid_iop_range(previous, kDriverSize))) {
                set_result(cpu, kUsbBadDriver);
                return;
            }
            if (next != 0) {
                write32(next + kDriverPrev, previous);
            } else {
                g_driver_tail = previous;
            }
            if (previous != 0) {
                write32(previous + kDriverNext, next);
            } else {
                g_driver_head = next;
            }
            // PS2SDK leaves the removed driver's link fields intact.
            set_result(cpu, kUsbOk);
            return;
        }
        current = read32(current + kDriverNext);
    }
    set_result(cpu, kUsbBadDriver);
}

void scan_static_descriptor(Cpu &cpu) {
    // The offline service has no device tree, so no descriptor can be found.
    cpu.r[2] = 0;
}

void get_device_location(Cpu &cpu) {
    set_result(cpu, kUsbBadDevice);
}

void set_private_data(Cpu &cpu) {
    set_result(cpu, kUsbBadDevice);
}

void get_private_data(Cpu &cpu) {
    cpu.r[2] = 0;
}

void open_pipe(Cpu &cpu) {
    // sceUsbdOpenPipe returns -1 when the requested device is absent.
    set_result(cpu, -1);
}

void close_pipe(Cpu &cpu) {
    set_result(cpu, kUsbBadPipe);
}

void transfer_pipe(Cpu &cpu) {
    set_result(cpu, kUsbBadPipe);
}

void open_pipe_aligned(Cpu &cpu) {
    set_result(cpu, -1);
}

void register_autoloader(Cpu &cpu) {
    const std::uint32_t driver = cpu.r[4];
    if (!valid_driver(driver)) {
        set_result(cpu, kUsbBadDriver);
        return;
    }
    if (g_auto_loader != 0) {
        set_result(cpu, kUsbBusy);
        return;
    }
    // The SDK records the module GP on the autoloader object but does not link
    // it into the ordinary LDD list.
    write32(driver + kDriverGp, cpu.r[28]);
    g_auto_loader = driver;
    set_result(cpu, kUsbOk);
}

void unregister_autoloader(Cpu &cpu) {
    g_auto_loader = 0;
    set_result(cpu, kUsbOk);
}

void change_thread_priority(Cpu &cpu) {
    // No HCD or callback worker threads exist in the offline host service.
    // The request therefore has no thread state to change in this mode.
    (void)cpu.r[4];
    (void)cpu.r[5];
    set_result(cpu, kUsbOk);
}

void get_report_descriptor(Cpu &cpu) {
    // The SDK reports BADDEV before touching output pointers when the device
    // identifier is absent; no USB devices are exposed by this replacement.
    set_result(cpu, kUsbBadDevice);
}

void multi_isochronous_transfer(Cpu &cpu) {
    set_result(cpu, kUsbBadPipe);
}

} // namespace

void reset() {
    g_driver_head = 0;
    g_driver_tail = 0;
    g_auto_loader = 0;
}

kernel::NativeIopModule make_module() {
    kernel::NativeIopModule module;
    module.name = "USBD";
    module.aliases = {"USB_driver"};
    module.start = [](const std::string &) {
        std::fprintf(stderr, "[iop-native] USBD offline replacement active; no USB device is attached\n");
        return true;
    };

    kernel::NativeIopLibrary library;
    library.name = "usbd";
    library.version = kUsbdVersion;
    library.exports = {
        {4, register_ldd},
        {5, unregister_ldd},
        {6, scan_static_descriptor},
        {7, set_private_data},
        {8, get_private_data},
        {9, open_pipe},
        {10, close_pipe},
        {11, transfer_pipe},
        {12, open_pipe_aligned},
        {13, get_device_location},
        {14, register_autoloader},
        {15, unregister_autoloader},
        {16, change_thread_priority},
        {17, get_report_descriptor},
        {18, multi_isochronous_transfer},
    };
    module.libraries.push_back(std::move(library));
    return module;
}

} // namespace ps2::iop::usbd_offline
