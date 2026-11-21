#include "mka/native_file.hpp"
#include "mka/sample_payload.hpp"
#include "mka/sound_archive.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <vector>

namespace {
unsigned fail_on_read{};
bool invalidate_before_restore{};
unsigned injected_reads{};
const auto read_error = std::make_error_code(std::errc::permission_denied);
const auto restore_error = std::make_error_code(std::errc::bad_file_descriptor);

void check(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "FAIL %s\n", message);
        std::exit(1);
    }
}
void word(std::vector<std::uint8_t>& data, std::size_t at, std::uint32_t value) {
    for (unsigned i = 0; i != 4; ++i) data[at + i] = static_cast<std::uint8_t>(value >> (8 * i));
}
} // namespace

// Link-only injection of a native host-file error. Production libraries keep
// their ordinary NativeFile calls, with no injected transport or success path.
mka::content::FileRead real_read(mka::content::NativeFile*, void*, std::size_t)
    asm("__real__ZN3mka7content10NativeFile4readEPvy");
mka::content::FileRead wrapped_read(mka::content::NativeFile*, void*, std::size_t)
    asm("__wrap__ZN3mka7content10NativeFile4readEPvy");
mka::content::FileRead wrapped_read(
    mka::content::NativeFile* file, void* output, std::size_t count) {
    if (fail_on_read != 0 && --fail_on_read == 0) {
        ++injected_reads;
        if (invalidate_before_restore) file->close();
        return {0, false, read_error};
    }
    return real_read(file, output, count);
}

int main(int argc, char** argv) {
    using namespace mka::sound;
    using namespace mka::content;
    check(argc == 2, "requires new disposable directory");
    const std::filesystem::path directory(argv[1]);
    check(std::filesystem::create_directory(directory), "exclusive fixture directory");
    const auto path = directory / "sample.bin";
    std::vector<std::uint8_t> bytes(256);
    word(bytes, 64 + 8, 1);
    word(bytes, 64 + 12, 22050);
    word(bytes, 64 + 16, 8);
    {
        std::ofstream output(path, std::ios::binary);
        output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        check(bool(output), "write owned fixture");
    }
    ArchiveFailure archive_failure;
    auto backing = SoundArchiveBacking::open(path, archive_failure);
    check(bool(backing), "actual native opened backing");
    fail_on_read = 1;
    const auto failed_read = backing->read_bytes(8, 4, archive_failure);
    check(!failed_read && archive_failure.kind == ArchiveError::io_error &&
          injected_reads == 1,
          "injected read error reaches actual archive read");
    ArchiveFailure cursor_failure;
    check(backing->opened_position(cursor_failure) == 0,
          "actual native restore returns from failed-read offset8 to saved cursor0");
    check(archive_failure.host_error == read_error,
          "archive read error survives successful native cursor restoration");
    check(backing->opened_position(archive_failure) == 0, "archive cursor restored after failed read");
    fail_on_read = 1;
    invalidate_before_restore = true;
    check(!backing->read_bytes(8, 4, archive_failure) &&
          archive_failure.kind == ArchiveError::cursor_restore_failed &&
          archive_failure.host_error == restore_error,
          "failed archive restoration supersedes the read error by name");
    invalidate_before_restore = false;

    NativeFile file;
    std::error_code error;
    check(file.open(path, error), "ordinary native sample file");
    ResourceLookup resource;
    resource.status = ResourceLookupStatus::found;
    resource.key_mode = ResourceKeyMode::numeric;
    const auto store_resource_word = [&](unsigned at, std::uint32_t value) {
        for (unsigned i = 0; i != 4; ++i)
            resource.bytes[at + i] = static_cast<std::uint8_t>(value >> (8 * i));
    };
    store_resource_word(8, 0x20000002u);
    store_resource_word(12, 22050);
    store_resource_word(16, 64);
    store_resource_word(20, 28);
    for (const unsigned read_index : {1u, 2u}) {
        check(file.seek(37, SeekOrigin::begin, error), "known saved sample cursor");
        fail_on_read = read_index;
        PayloadFailure payload_failure;
        check(!SamplePayload::read(file, resource, payload_failure) &&
              payload_failure.kind == PayloadError::io_error && payload_failure.host_error == read_error,
              "sample header/payload preserves actual NativeFile read error");
        std::uint64_t position{};
        check(file.position(position, error) && position == 37, "sample cursor restored after read failure");
    }
    check(file.seek(37, SeekOrigin::begin, error), "saved cursor before failed restoration");
    fail_on_read = 1;
    invalidate_before_restore = true;
    PayloadFailure payload_failure;
    check(!SamplePayload::read(file, resource, payload_failure) &&
          payload_failure.kind == PayloadError::cursor_restore_failed && payload_failure.host_error == restore_error,
          "sample restoration error remains distinct from original read error");
    invalidate_before_restore = false;
    ContentVolumeFailure content_failure;
    auto volume = NativeContentVolume::open(directory, content_failure);
    check(bool(volume), "actual retained content volume");
    auto content_file = volume->open_resource("sample.bin", content_failure);
    check(content_file && content_file->seek(41, SeekOrigin::begin, content_failure),
          "actual opened content capability with saved cursor");
    auto content_backing = SoundArchiveBacking::adopt(std::move(*content_file),
        AudioResourceDomain::create(archive_failure.resource), archive_failure);
    check(bool(content_backing), "adopt actual native content file");
    fail_on_read = 1;
    check(!content_backing->read_bytes(8, 4, archive_failure) &&
          archive_failure.kind == ArchiveError::io_error && archive_failure.host_error == read_error &&
          archive_failure.content.kind == ContentVolumeError::read_failed,
          "adopted content backing preserves named native read error");
    check(content_backing->opened_position(cursor_failure) == 41, "adopted content cursor restored");
    auto content_sample = volume->open_resource("sample.bin", content_failure);
    check(content_sample && content_sample->seek(41, SeekOrigin::begin, content_failure),
          "opened content sample with saved cursor");
    for (const unsigned read_index : {1u, 2u}) {
        fail_on_read = read_index;
        check(!SamplePayload::read(*content_sample, resource, payload_failure) &&
              payload_failure.kind == PayloadError::io_error && payload_failure.host_error == read_error &&
              payload_failure.content.kind == ContentVolumeError::read_failed &&
              content_sample->position(content_failure) == 41,
              "opened-content sample header/payload errors retain details and cursor");
    }
    std::puts("PASS_NATIVE_SOUND_FILE_ERRORS");
    return 0;
}
