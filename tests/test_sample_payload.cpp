#include "mka/sample_payload.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>

namespace {
using namespace mka::sound;
void check(bool ok,const char* message) { if (!ok) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); } }
void put(std::vector<std::uint8_t>& b,std::size_t at,std::uint32_t value) {
    for (unsigned i=0;i<4;++i) b[at+i]=static_cast<std::uint8_t>(value>>(8*i));
}
ResourceLookup resource(std::uint32_t offset,std::uint32_t size) {
    ResourceLookup r; r.status=ResourceLookupStatus::found; r.key_mode=ResourceKeyMode::numeric;
    auto putr=[&](std::size_t at,std::uint32_t value) { for(unsigned i=0;i<4;++i) r.bytes[at+i]=static_cast<std::uint8_t>(value>>(8*i)); };
    putr(8,0x20000002); putr(12,22050); putr(16,offset); putr(20,size);
    return r;
}
void field(ResourceLookup& r,std::size_t at,std::uint32_t value) {
    for(unsigned i=0;i<4;++i) r.bytes[at+i]=static_cast<std::uint8_t>(value>>(8*i));
}
std::vector<std::uint8_t> extent(std::uint32_t count=1,std::uint32_t stride=0x4d80) {
    std::vector<std::uint8_t> b(20+std::size_t(count)*stride);
    // Exact header words for pinned real normal resource0 when defaults used.
    put(b,0,0); put(b,4,0x10000); put(b,8,count); put(b,12,22050); put(b,16,stride);
    for(std::size_t i=20;i<b.size();++i) b[i]=static_cast<std::uint8_t>(i*13);
    return b;
}
void write(const std::filesystem::path& path,std::uint64_t offset,const std::vector<std::uint8_t>& bytes) {
    std::ofstream f(path,std::ios::binary|std::ios::trunc);
    f.seekp(static_cast<std::streamoff>(offset));
    f.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    check(bool(f),"write explicit backing fixture");
}
}
int main(int argc,char** argv) {
    using namespace mka::content;
    const auto parent=std::filesystem::temp_directory_path();
    const auto dir=parent/("mka-sample-payload-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    check(std::filesystem::create_directory(dir),"exclusive fixture directory");
    const auto path=dir/"backing.bin";
    auto bytes=extent(); auto r=resource(64,static_cast<std::uint32_t>(bytes.size()));
    write(path,64,bytes);
    NativeFile file; std::error_code ec; PayloadFailure failure;
    check(file.open(path,ec) && file.seek(7,SeekOrigin::begin,ec),"open explicit file and set caller cursor");
    auto sample=SamplePayload::read(file,r,failure);
    std::uint64_t cursor=0;
    check(sample && failure.kind==PayloadError::none && sample->raw_bytes()==bytes,"exact header/payload bytes");
    check(file.position(cursor,ec) && cursor==7,"success restores caller cursor");
    check(sample->plane_count()==1 && sample->rate_value()==22050 && sample->bytes_per_plane()==0x4d80,"proven raw count/rate/size metadata");
    auto raw=sample->encoded_payload();
    check(raw.size==0x4d80 && std::equal(raw.data,raw.data+raw.size,bytes.begin()+20),"single raw payload view without plane splitting");
    auto moved=std::move(*sample);
    check(sample->raw_bytes().empty() && sample->encoded_payload().size==0 && sample->plane_count()==0,"moved source safe and empty");
    check(moved.raw_bytes()==bytes,"moved owner exact bytes");
    auto copy=moved; moved=std::move(moved);
    check(copy.raw_bytes()==bytes && moved.raw_bytes()==bytes,"copy and self move retain owned bytes");
    auto reject=[&](ResourceLookup bad,PayloadError error) {
        check(!SamplePayload::read(file,bad,failure) && failure.kind==error,"named rejected resource error");
        check(file.position(cursor,ec) && cursor==7,"failure restores caller cursor");
    };
    auto bad=r; bad.status=ResourceLookupStatus::absent; reject(bad,PayloadError::resource_not_found);
    bad=r; bad.key_mode=ResourceKeyMode::unknown; reject(bad,PayloadError::unsupported_key_mode);
    bad=r; bad.key_mode=ResourceKeyMode::name; reject(bad,PayloadError::unsupported_key_mode);
    bad=r; field(bad,32,1); field(bad,16,0xffffffffu); reject(bad,PayloadError::unsupported_alternate_streaming);
    bad=r; field(bad,8,2); reject(bad,PayloadError::unsupported_key_lookup_route);
    bad=r; field(bad,16,0); reject(bad,PayloadError::unsupported_key_lookup_route);
    bad=r; field(bad,16,0xffffffffu); field(bad,20,0xffffffffu); reject(bad,PayloadError::extent_out_of_bounds);
    bad=r; field(bad,20,19); reject(bad,PayloadError::truncated_header);
    bad=r; field(bad,20,static_cast<std::uint32_t>(bytes.size()-1)); reject(bad,PayloadError::inconsistent_extent);
    file.close();
    for (const auto pair : {std::pair<unsigned,PayloadError>{8,PayloadError::unsupported_zero_count},{12,PayloadError::unsupported_zero_rate},{16,PayloadError::unsupported_zero_plane_size}}) {
        auto b=bytes; put(b,pair.first,0); write(path,64,b);
        check(!SamplePayload::read(path,r,failure) && failure.kind==pair.second,"zero metadata unsupported, no default guessed");
    }
    for(auto count:{0xffffffffu,0x8000u,0x10001u}) {
        auto b=bytes; put(b,8,count); write(path,64,b);
        check(!SamplePayload::read(path,r,failure) && failure.kind==PayloadError::unsupported_count_encoding,"signed/truncated count encoding unsupported");
    }
    auto changed=bytes; put(changed,12,32000); write(path,64,changed);
    check(!SamplePayload::read(path,r,failure) && failure.kind==PayloadError::unsupported_rate_mismatch,"unproved mismatched raw rate not normalized");
    changed=bytes; put(changed,16,0xffffffffu); write(path,64,changed);
    check(!SamplePayload::read(path,r,failure) && failure.kind==PayloadError::inconsistent_extent,"extreme product rejected without multiplication wrap");
    changed=extent(2,32); put(changed,0,0xdeadbeef); put(changed,4,0x12345678); write(path,64,changed);
    auto two=SamplePayload::read(path,resource(64,static_cast<std::uint32_t>(changed.size())),failure);
    check(two && two->raw_bytes()==changed && two->plane_count()==2 && two->encoded_payload().size==64,"count2 raw bytes preserved with no interleaving assumption");
    auto large=extent(1,2*1024*1024+64); const std::uint32_t far=32*1024*1024+256;
    write(path,far,large);
    auto far_sample=SamplePayload::read(path,resource(far,static_cast<std::uint32_t>(large.size())),failure);
    check(far_sample && far_sample->raw_bytes()==large,"actual raw read beyond32MiB with payload exceeding2MiB");
    check(!SamplePayload::read(dir/"missing",r,failure) && failure.kind==PayloadError::io_error,"missing explicit backing path fails");
    check(!SamplePayload::read(file,r,failure) && failure.kind==PayloadError::io_error,"closed explicit handle fails");
    if(argc==2) {
        NativeFile actual;
        check(actual.open(argv[1],ec),"open explicit pinned MS2");
        auto real=SamplePayload::read(actual,resource(0x22188,0x4d94),failure);
        check(real && real->header()==copy.header() && real->plane_count()==1 && real->bytes_per_plane()==0x4d80,"exact pinned real normal resource header");
        std::vector<std::uint8_t> reference(0x4d94);
        check(actual.seek(0x22188,SeekOrigin::begin,ec) && actual.read(reference.data(),reference.size()).bytes==reference.size() && real->raw_bytes()==reference,"exact real absolute backing extent, no bankbase addition");
        std::printf("Pinned normal extent: offset0x22188 size%zu raw payload%zu\n",real->raw_bytes().size(),real->encoded_payload().size);
    }
    check(dir.parent_path()==parent && dir.filename().string().find("mka-sample-payload-")==0,"cleanup only exclusive fixture root");
    check(std::filesystem::remove_all(dir)==2 && !std::filesystem::exists(dir),"fixture cleanup");
    std::puts("sample payload tests passed");
}
