#include "mka/native_file.hpp"
#include <cerrno>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>
#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <io.h>
#endif

namespace mka::content {
namespace {
std::error_code io_error() { return {errno ? errno : EIO, std::generic_category()}; }
#ifdef _WIN32
auto host_tell(std::FILE* f) { return _ftelli64(f); }
int host_seek(std::FILE* f, std::int64_t p, int origin) { return _fseeki64(f, p, origin); }
bool query_handle_path(HANDLE handle,std::filesystem::path& out,std::error_code& error){
    out.clear();error.clear();static_assert(sizeof(wchar_t)==2);
    const auto fail=[&](){const auto code=GetLastError();error={int(code?code:ERROR_GEN_FAILURE),std::system_category()};return false;};
    DWORD capacity=GetFinalPathNameByHandleW(handle,nullptr,0,FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
    if(!capacity)return fail();
    try{
        std::vector<wchar_t> buffer;
        for(;;){
            if(capacity>buffer.max_size()){error=std::make_error_code(std::errc::value_too_large);return false;}
            buffer.resize(capacity);
            const DWORD size=GetFinalPathNameByHandleW(handle,buffer.data(),capacity,FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
            if(!size)return fail();
            if(size>=capacity){
                if(size==MAXDWORD){error=std::make_error_code(std::errc::value_too_large);return false;}
                capacity=size+1;continue;
            }
            if(buffer[size]!=L'\0'){error=std::make_error_code(std::errc::invalid_argument);return false;}
            for(DWORD i=0;i<size;++i){const auto c=std::uint16_t(buffer[i]);
                if(!c){error=std::make_error_code(std::errc::invalid_argument);return false;}
                if(c>=0xd800&&c<=0xdbff){
                    if(i+1>=size||std::uint16_t(buffer[i+1])<0xdc00||std::uint16_t(buffer[i+1])>0xdfff){error=std::make_error_code(std::errc::illegal_byte_sequence);return false;}
                    ++i;
                }else if(c>=0xdc00&&c<=0xdfff){error=std::make_error_code(std::errc::illegal_byte_sequence);return false;}
            }
            // Preserve the OS-returned extended namespace. This path is a
            // handle observation, not a new fopen/canonical lookup.
            std::filesystem::path observed(std::wstring(buffer.data(),size));
            if(!observed.is_absolute()){error=std::make_error_code(std::errc::invalid_argument);return false;}
            out=std::move(observed);return true;
        }
    }catch(const std::bad_alloc&){error=std::make_error_code(std::errc::not_enough_memory);}
     catch(const std::length_error&){error=std::make_error_code(std::errc::value_too_large);}
     catch(const std::filesystem::filesystem_error& e){error=e.code();}
    return false;
}
#else
auto host_tell(std::FILE* f) { return ftello(f); }
int host_seek(std::FILE* f, std::int64_t p, int origin) {
    if (p > std::numeric_limits<off_t>::max()) { errno = EOVERFLOW; return -1; }
    return fseeko(f, static_cast<off_t>(p), origin);
}
#endif
}
NativeFile::~NativeFile() { close(); }
NativeFile::NativeFile(NativeFile&& other) noexcept : file_(std::exchange(other.file_, nullptr)) {}
NativeFile& NativeFile::operator=(NativeFile&& other) noexcept {
    if (this != &other) { close(); file_ = std::exchange(other.file_, nullptr); }
    return *this;
}
void NativeFile::close() noexcept { if (file_) { std::fclose(file_); file_ = nullptr; } }
bool NativeFile::open(const std::filesystem::path& path, std::error_code& error) {
    close(); error.clear(); errno = 0;
#ifdef _WIN32
    file_ = _wfopen(path.c_str(), L"rb");
#else
    file_ = std::fopen(path.c_str(), "rb");
#endif
    if (!file_) { error = io_error(); return false; }
    return true;
}
bool NativeFile::opened_path(std::filesystem::path& out,std::error_code& error)const {
    out.clear();error.clear();
    if(!file_){error=std::make_error_code(std::errc::bad_file_descriptor);return false;}
#ifdef _WIN32
    static_assert(sizeof(wchar_t)==2);
    errno=0;const auto descriptor=_fileno(file_);
    if(descriptor<0){error=io_error();return false;}
    const auto native=_get_osfhandle(descriptor);
    if(native==-1||native==-2){error=io_error();return false;}
    const auto handle=reinterpret_cast<HANDLE>(native);
    return query_handle_path(handle,out,error);
#else
    error=std::make_error_code(std::errc::operation_not_supported);return false;
#endif
}
bool NativeFile::opened_regular_file(bool& out,std::error_code& error)const{
    out=false;error.clear();if(!file_){error=std::make_error_code(std::errc::bad_file_descriptor);return false;}
#ifdef _WIN32
    errno=0;const auto descriptor=_fileno(file_);if(descriptor<0){error=io_error();return false;}
    const auto native=_get_osfhandle(descriptor);if(native==-1||native==-2){error=io_error();return false;}
    auto handle=reinterpret_cast<HANDLE>(native);BY_HANDLE_FILE_INFORMATION info{};
    if(!GetFileInformationByHandle(handle,&info)){error={int(GetLastError()),std::system_category()};return false;}
    SetLastError(NO_ERROR);const auto type=GetFileType(handle);
    if(type==FILE_TYPE_UNKNOWN&&GetLastError()!=NO_ERROR){error={int(GetLastError()),std::system_category()};return false;}
    out=type==FILE_TYPE_DISK&&!(info.dwFileAttributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_DEVICE));return true;
#else
    struct stat info{};if(fstat(fileno(file_),&info)){error=io_error();return false;}out=S_ISREG(info.st_mode);return true;
#endif
}
bool NativeFile::directory_path(const std::filesystem::path& path,std::filesystem::path& out,std::error_code& error){
    out.clear();error.clear();
#ifdef _WIN32
    const auto handle=CreateFileW(path.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr);
    if(handle==INVALID_HANDLE_VALUE){error={int(GetLastError()),std::system_category()};return false;}
    struct Owner {HANDLE handle;~Owner(){CloseHandle(handle);}} owner{handle};
    BY_HANDLE_FILE_INFORMATION info{};
    if(!GetFileInformationByHandle(handle,&info)){error={int(GetLastError()),std::system_category()};return false;}
    if(!(info.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)){error=std::make_error_code(std::errc::not_a_directory);return false;}
    return query_handle_path(handle,out,error);
#else
    (void)path;error=std::make_error_code(std::errc::operation_not_supported);return false;
#endif
}
bool NativeFile::position(std::uint64_t& out, std::error_code& error) {
    out = 0; error.clear();
    if (!file_) { error = std::make_error_code(std::errc::bad_file_descriptor); return false; }
    errno = 0; const auto p = host_tell(file_);
    if (p < 0) { error = io_error(); return false; }
    out = static_cast<std::uint64_t>(p); return true;
}
bool NativeFile::length(std::uint64_t& out, std::error_code& error) {
    out = 0; std::uint64_t saved;
    if (!position(saved, error)) return false;
    errno = 0;
    if (host_seek(file_, 0, SEEK_END)) { error = io_error(); return false; }
    const auto end = host_tell(file_);
    const auto end_error = end < 0 ? io_error() : std::error_code{};
    errno = 0;
    if (host_seek(file_, static_cast<std::int64_t>(saved), SEEK_SET)) { error = io_error(); return false; }
    if (end_error) { error = end_error; return false; }
    out = static_cast<std::uint64_t>(end); return true;
}
bool NativeFile::seek(std::int64_t offset, SeekOrigin origin, std::error_code& error) {
    std::uint64_t base = 0; error.clear();
    if (!file_) { error = std::make_error_code(std::errc::bad_file_descriptor); return false; }
    if (origin == SeekOrigin::current) { if (!position(base, error)) return false; }
    else if (origin == SeekOrigin::end) { if (!length(base, error)) return false; }
    else if (origin != SeekOrigin::begin) { error = std::make_error_code(std::errc::invalid_argument); return false; }
    const auto magnitude = offset < 0 ? std::uint64_t(-(offset + 1)) + 1 : std::uint64_t(offset);
    if ((offset < 0 && magnitude > base) ||
        (offset >= 0 && magnitude > std::uint64_t(INT64_MAX) - base)) {
        error = std::make_error_code(std::errc::invalid_argument); return false;
    }
    const auto target = offset < 0 ? base - magnitude : base + magnitude;
    errno = 0;
    if (host_seek(file_, static_cast<std::int64_t>(target), SEEK_SET)) { error = io_error(); return false; }
    return true;
}
FileRead NativeFile::read(void* out, std::size_t count) {
    FileRead result;
    if (!file_) { result.error = std::make_error_code(std::errc::bad_file_descriptor); return result; }
    if (count && !out) { result.error = std::make_error_code(std::errc::invalid_argument); return result; }
    if (!count) return result;
    errno = 0; result.bytes = std::fread(out, 1, count, file_);
    result.eof = std::feof(file_) != 0;
    if (std::ferror(file_)) result.error = io_error();
    return result;
}
bool NativeFile::read_all(std::vector<std::uint8_t>& out, std::error_code& error) {
    out.clear(); std::uint64_t size;
    if (!length(size, error)) return false;
    if (size > out.max_size()) { error = std::make_error_code(std::errc::value_too_large); return false; }
    std::vector<std::uint8_t> data;
    try { data.resize(static_cast<std::size_t>(size)); }
    catch (const std::bad_alloc&) { error = std::make_error_code(std::errc::not_enough_memory); return false; }
    catch (const std::length_error&) { error = std::make_error_code(std::errc::value_too_large); return false; }
    if (!seek(0, SeekOrigin::begin, error)) return false;
    const auto result = read(data.data(), data.size());
    if (result.error || result.bytes != data.size()) {
        error = result.error ? result.error : std::make_error_code(std::errc::io_error); return false;
    }
    out = std::move(data); error.clear(); return true;
}
} // namespace mka::content
