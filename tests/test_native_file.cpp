#include "mka/native_file.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <winioctl.h>
#endif
using namespace mka::content;
void check(bool ok, const char* message) { if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); } }
int main() {
    const auto parent = std::filesystem::temp_directory_path();
    const auto dir = parent / ("mka-native-file-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    check(std::filesystem::create_directory(dir), "create exclusive temporary directory");
    const auto small = dir / "small.bin", empty = dir / "empty.bin", sparse = dir / "sparse.bin";
    { std::ofstream f(small, std::ios::binary); f << "abcde"; }
    { std::ofstream f(empty, std::ios::binary); }
    NativeFile f; std::error_code ec; std::uint64_t value = 99;
    std::vector<std::uint8_t> out{9}; char bytes[8]{};
    check(!f.read_all(out, ec) && out.empty() && ec, "closed read_all clears stale output");
    check(!f.open(dir / "missing", ec) && ec, "missing file error");
    check(f.open(empty, ec) && f.length(value, ec) && value == 0 && f.read_all(out, ec) && out.empty(), "empty file success");
    check(f.open(small, ec) && f.length(value, ec) && value == 5, "opened length");
    auto r = f.read(bytes, 2); check(r.bytes == 2 && !r.eof && !r.error && bytes[1] == 'b', "normal read");
    check(f.position(value, ec) && value == 2 && f.length(value, ec) && value == 5, "length preserves position");
    check(f.position(value, ec) && value == 2, "position retained");
    r = f.read(bytes, 8); check(r.bytes == 3 && r.eof && !r.error && bytes[2] == 'e', "short EOF distinct from error");
    r = f.read(bytes, 1); check(r.bytes == 0 && r.eof && !r.error, "EOF read");
    check(f.seek(-2, SeekOrigin::end, ec), "negative relative end seek");
    r = f.read(bytes, 2); check(r.bytes == 2 && bytes[0] == 'd' && !r.error, "seek clears EOF");
    check(!f.seek(-1, SeekOrigin::begin, ec) && ec, "reject negative absolute");
    check(!f.seek(INT64_MIN, SeekOrigin::current, ec) && ec, "reject minimum negative overflow");
    check(!f.seek(INT64_MAX, SeekOrigin::current, ec) && ec, "reject positive overflow");
    check(f.seek(10, SeekOrigin::begin, ec), "seek beyond EOF permitted");
    r = f.read(bytes, 1); check(!r.bytes && r.eof && !r.error, "beyond EOF short read");
    check(f.read_all(out, ec) && out.size() == 5 && out[0] == 'a', "read_all resets position");
    r = f.read(nullptr, 1); check(bool(r.error), "invalid buffer error");
    NativeFile moved(std::move(f)); check(!f.is_open() && moved.is_open(), "move transfers ownership");
    f = std::move(moved); check(!moved.is_open() && f.is_open(), "move assignment ownership");
    f.close(); f.close(); check(!f.position(value, ec) && ec && !value, "closed position error clears value");
    check(bool(f.read(bytes, 1).error), "closed read error");
    check(f.open(small, ec) && !f.open(dir / "missing", ec) && !f.is_open(), "failed reopen releases previous handle");
    const std::uint64_t extent = (std::uint64_t(1) << 32) + 37;
#ifdef _WIN32
    HANDLE h = CreateFileW(sparse.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    check(h != INVALID_HANDLE_VALUE, "create sparse host file");
    DWORD returned = 0;
    check(DeviceIoControl(h, FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &returned, nullptr), "mark sparse before extending");
    LARGE_INTEGER end; end.QuadPart = static_cast<LONGLONG>(extent - 1);
    check(SetFilePointerEx(h, end, nullptr, FILE_BEGIN), "64-bit sparse seek");
    const char marker = 'Z'; DWORD written = 0;
    check(WriteFile(h, &marker, 1, &written, nullptr) && written == 1, "write only sparse end marker");
    check(CloseHandle(h), "close sparse writer");
#else
    { std::ofstream writer(sparse, std::ios::binary); writer.seekp(extent - 1); writer.put('Z'); check(bool(writer), "sparse writer"); }
#endif
    check(f.open(sparse, ec) && f.length(value, ec) && value == extent, "length beyond 4 GiB");
    check(f.seek(-2, SeekOrigin::end, ec), "seek near sparse end");
    r = f.read(bytes, 8); check(r.bytes == 2 && r.eof && !r.error && bytes[0] == 0 && bytes[1] == 'Z', "read beyond 4 GiB without huge buffer");
    f.close();
    // Verify the exact newly created directory before any recursive cleanup.
    check(dir.parent_path() == parent && dir.filename().string().find("mka-native-file-") == 0, "cleanup confined to new test directory");
    check(std::filesystem::remove_all(dir) == 4 && !std::filesystem::exists(dir), "temporary files removed");
    std::puts("native file tests passed");
}
