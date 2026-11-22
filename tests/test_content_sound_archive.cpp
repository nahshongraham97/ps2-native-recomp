#include "mka/audio_banks.hpp"
#include "mka/content_volume.hpp"
#include "mka/stream_source.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace mka::sound;
using namespace mka::content;
namespace {
void check(bool value, const char* why) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}
void word(std::vector<std::uint8_t>& bytes, std::size_t at, std::uint32_t value, unsigned width = 4) {
    for (unsigned i = 0; i < width; ++i) bytes[at + i] = static_cast<std::uint8_t>(value >> (i * 8));
}
void write(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    check(bool(out), "exclusive fixture write");
}
std::vector<std::uint8_t> archive_bytes() {
    // One ordinary type2 named bank and one numeric normal sample extent.
    std::vector<std::uint8_t> archive(1024);
    word(archive, 0, 2); word(archive, 4, 2); word(archive, 8, 34);
    word(archive, 16, static_cast<std::uint32_t>(archive.size()));
    word(archive, 24, 1); word(archive, 28, 128); word(archive, 32, 158);
    const char names[] = "\0good.mst";
    std::copy_n(reinterpret_cast<const std::uint8_t*>(names), sizeof(names), archive.begin() + 36);
    std::vector<std::uint8_t> bank(158);
    word(bank, 0, 11); word(bank, 4, 0x13);
    word(bank, 8, 1, 2); word(bank, 10, 1, 2); word(bank, 12, 1, 2);
    word(bank, 16, 48); word(bank, 20, 84); word(bank, 24, 100);
    word(bank, 28, 116); word(bank, 32, 156);
    word(bank, 48, 0xdeadbeef); word(bank, 52, 0xdeadbeef);
    word(bank, 56, 0x20000002); word(bank, 60, 22050);
    word(bank, 64, 512); word(bank, 68, 28);
    word(bank, 84 + 12, 0); word(bank, 100 + 8, 1);
    word(bank, 116, 0x20000002); bank[124] = 1;
    bank[126] = 0xfd; bank[127] = 0xff;
    word(bank, 156, 1, 2);
    std::copy(bank.begin(), bank.end(), archive.begin() + 128);
    word(archive, 512 + 8, 1); word(archive, 512 + 12, 22050);
    word(archive, 512 + 16, 8);
    for (unsigned i = 0; i < 8; ++i) archive[512 + 20 + i] = static_cast<std::uint8_t>(i + 3);
    return archive;
}
ResourceLookup sample_resource() {
    ResourceLookup resource;
    resource.status = ResourceLookupStatus::found;
    resource.key_mode = ResourceKeyMode::numeric;
    for (unsigned i = 0; i < 4; ++i) {
        resource.bytes[8 + i] = static_cast<std::uint8_t>(0x20000002u >> (8 * i));
        resource.bytes[12 + i] = static_cast<std::uint8_t>(22050u >> (8 * i));
        resource.bytes[16 + i] = static_cast<std::uint8_t>(512u >> (8 * i));
        resource.bytes[20 + i] = static_cast<std::uint8_t>(28u >> (8 * i));
    }
    return resource;
}
}

int main(int argc, char** argv) {
    check(argc == 2, "usage: test_content_sound_archive <new disposable directory>");
    const std::filesystem::path dir(argv[1]);
    check(std::filesystem::create_directory(dir), "exclusive disposable directory");
    const auto root = dir / "root";
    check(std::filesystem::create_directory(root), "exclusive content root");
    const auto ms2 = root / "asset.ms2";
    const auto ms4 = root / "explicit.ms4";
    write(ms2, archive_bytes());
    std::vector<std::uint8_t> stream(0x8000);
    for (std::size_t i = 0; i < stream.size(); ++i) stream[i] = static_cast<std::uint8_t>(i);
    write(ms4, stream);
    const auto other_root = dir / "other-root";
    check(std::filesystem::create_directory(other_root), "exclusive second content root");
    write(other_root / "separate.ms4", stream);
    ContentVolumeFailure content_failure;
    auto volume = NativeContentVolume::open(root, content_failure);
#ifndef _WIN32
    check(!volume && content_failure.kind == ContentVolumeError::unsupported_host,
          "host capability refusal is named");
    std::puts("PASS_CONTENT_SOUND_ARCHIVE_UNSUPPORTED_HOST");
    return 0;
#else
    check(bool(volume), "real retained content volume");
    auto opened = volume->open_resource("asset.ms2", content_failure);
    auto sibling = volume->open_resource("asset.ms2", content_failure);
    check(opened && sibling && opened->same_volume(*sibling), "retained same volume identity");
    check(opened->seek(77, SeekOrigin::begin, content_failure), "initial cursor");
    ArchiveFailure archive_failure;
    auto backing = SoundArchiveBacking::adopt(std::move(*opened),
        AudioResourceDomain::create(archive_failure.resource), archive_failure);
    check(backing && !opened->valid() && backing->opened_position(archive_failure) == 77,
          "adopt exact opened handle and cursor");
    auto header = backing->read_bytes(0, 12, archive_failure);
    check(header && (*header)[0] == 2 && backing->opened_position(archive_failure) == 77,
          "shared parser read restores adopted cursor");
    PayloadFailure payload_failure;
    auto payload = backing->read_payload(sample_resource(), payload_failure);
    check(payload && payload->encoded_payload().size == 8 &&
          payload->encoded_payload().data[0] == 3 && backing->opened_position(archive_failure) == 77,
          "same sample parser reads content-backed payload and restores cursor");
    auto index_file = volume->open_resource("asset.ms2", content_failure);
    check(index_file && index_file->seek(49, SeekOrigin::begin, content_failure), "index cursor");
    auto archive = SoundArchive::adopt(std::move(*index_file), archive_failure);
    check(archive && archive->named_entry_count() == 1, "content archive directory parser");
    auto source = archive->read_bank("GOOD.MSB", archive_failure);
    check(source && source->bytes.size() == 158 &&
          source->backing->opened_position(archive_failure) == 49,
          "original lookup and bank copy preserve opened cursor");
    auto path_archive = SoundArchive::open(ms2, archive_failure);
    check(path_archive && path_archive->read_bank("good", archive_failure)->bytes == source->bytes,
          "adopted and standalone path banks have identical source bytes");
    auto bad = volume->open_resource("asset.ms2", content_failure);
    auto moved_bad = std::move(*bad);
    check(!SoundArchive::adopt(std::move(*bad), archive_failure) &&
          archive_failure.kind == ArchiveError::invalid_content_file &&
          archive_failure.content.kind == ContentVolumeError::moved_file,
          "moved content file is a named error");
    auto stream_file = volume->open_resource("explicit.ms4", content_failure);
    auto domain = AudioResourceDomain::create(archive_failure.resource);
    StreamFailure stream_failure;
    auto stream_source = StreamSource::adopt_associated_ms4(std::move(*stream_file), domain, stream_failure);
    check(stream_source && stream_source->resource_domain() == domain &&
          stream_source->associated_path() == std::filesystem::path("explicit.ms4"),
          "explicit associated MS4 retains content and resource domain");
    ResourceLookup stream_lookup;
    stream_lookup.status = ResourceLookupStatus::found;
    stream_lookup.key_mode = ResourceKeyMode::numeric;
    stream_lookup.bytes[0x20] = 1;
    stream_lookup.bytes[0x14] = 0x00; stream_lookup.bytes[0x15] = 0x80;
    stream_lookup.bytes[0x18] = 64;
    auto group = stream_source->read_group(stream_lookup, 0, stream_failure);
    check(group && group->raw_group().size == 0x8000 && group->range0().size == 64 &&
          group->range1().data[0] == stream[64], "content MS4 group uses existing range parser");
    auto session_file = volume->open_resource("asset.ms2", content_failure);
    check(bool(session_file), "retained content MS2 for bank session");
    auto other_volume = NativeContentVolume::open(other_root, content_failure);
    check(bool(other_volume), "independent explicit MS4 volume");
    auto invalid_ms2 = volume->open_resource("asset.ms2", content_failure);
    auto still_owned_ms2 = std::move(*invalid_ms2);
    std::optional<NativeContentFile> no_ms4;
    AudioBanksFailure banks_failure;
    check(!AudioBankSession::adopt(std::move(*invalid_ms2), std::move(no_ms4), banks_failure) &&
          banks_failure.kind == AudioBanksError::archive_failed &&
          banks_failure.archive.kind == ArchiveError::invalid_content_file,
          "moved content MS2 is rejected by session without a path fallback");
    auto ms4_file = other_volume->open_resource("separate.ms4", content_failure);
    check(ms4_file && !session_file->same_volume(*ms4_file),
          "explicit association need not invent same-volume requirement");
    auto invalid_ms4 = other_volume->open_resource("separate.ms4", content_failure);
    auto retained_ms4 = std::move(*invalid_ms4);
    auto temporary_ms2 = volume->open_resource("asset.ms2", content_failure);
    std::optional<NativeContentFile> invalid_association(std::move(*invalid_ms4));
    check(!AudioBankSession::adopt(std::move(*temporary_ms2), std::move(invalid_association), banks_failure) &&
          banks_failure.kind == AudioBanksError::stream_read_failed &&
          banks_failure.stream.backing.kind == ArchiveError::invalid_content_file,
          "moved explicit MS4 is refused before consuming valid MS2");
    check(temporary_ms2->valid(), "invalid association leaves MS2 ownership with caller");
    std::optional<NativeContentFile> associated(std::move(*ms4_file));
    auto session = AudioBankSession::adopt(std::move(*session_file), std::move(associated), banks_failure);
    check(bool(session), "native bank session adopts both opened files");
    auto bank_handle = session->load_bank("good", banks_failure);
    check(bool(bank_handle), "original bank parser accepts adopted named entry");
    auto bank_sample = session->read_normal(*bank_handle, 0, 0, banks_failure);
    check(bank_sample && bank_sample->payload.encoded_payload().size == 8 &&
          bank_sample->payload.encoded_payload().data[0] == 3,
          "bank session reads normal payload from retained content file");
    volume.reset();
    other_volume.reset();
    check(source->backing->read_bytes(128, 3, archive_failure).has_value() &&
          group->raw_group().data[1] == stream[1], "bank and stream retain content after public volume release");
    write(root / "bad.ms2", {1,2,3});
    auto retained_volume = NativeContentVolume::open(root, content_failure);
    auto malformed = retained_volume->open_resource("bad.ms2", content_failure);
    check(!SoundArchive::adopt(std::move(*malformed), archive_failure) &&
          archive_failure.kind == ArchiveError::truncated_header,
          "malformed adopted archive has original named parse error");
    std::puts("PASS_CONTENT_SOUND_ARCHIVE");
    return 0;
#endif
}
