#include "mka/stream_source.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <future>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <winioctl.h>
#include <bcrypt.h>
#endif
using namespace mka::sound;
namespace {
constexpr std::size_t G = 32768;
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void put(ResourceLookup& r, std::size_t at, std::uint32_t value) {
    for (unsigned i=0;i<4;++i) r.bytes[at+i]=static_cast<std::uint8_t>(value>>(8*i));
}
std::uint32_t word(const ResourceLookup& r,std::size_t at) {
    return std::uint32_t(r.bytes[at]) | (std::uint32_t(r.bytes[at+1])<<8) |
        (std::uint32_t(r.bytes[at+2])<<16) | (std::uint32_t(r.bytes[at+3])<<24);
}
ResourceLookup resource(std::uint32_t offset=0, std::uint32_t length=3*G,
                        std::uint32_t stride=48, std::uint32_t tail=16,
                        std::uint32_t key=0x12340001) {
    ResourceLookup r; r.status=ResourceLookupStatus::found; r.key_mode=ResourceKeyMode::numeric; r.index=17;
    put(r,0,0xffffffff); put(r,4,0xdeadbeef); put(r,8,key); put(r,12,0);
    put(r,16,offset); put(r,20,length); put(r,24,stride); put(r,28,tail); put(r,32,0x81);
    return r;
}
std::string hex(const std::uint8_t* p, std::size_t n) {
    constexpr char chars[]="0123456789abcdef"; std::string s; s.reserve(n*2);
    for (std::size_t i=0;i<n;++i) { s+=chars[p[i]>>4]; s+=chars[p[i]&15]; } return s;
}
std::string quoted(const std::string& s) {
    std::string out="\"";
    for (unsigned char c:s) {
        if (c=='"' || c=='\\') { out+='\\'; out+=char(c); }
        else if (c<32) { const std::uint8_t b=c; out+="\\u00"+hex(&b,1); }
        else out+=char(c);
    }
    return out+'"';
}
std::string sha256(PayloadBytes bytes) {
#ifdef _WIN32
    BCRYPT_ALG_HANDLE alg=nullptr; BCRYPT_HASH_HANDLE hash=nullptr;
    std::vector<std::uint8_t> object; // Outlives BCryptDestroyHash in Cleanup.
    check(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,"BCrypt SHA256 provider");
    struct Cleanup { BCRYPT_ALG_HANDLE& a; BCRYPT_HASH_HANDLE& h;
        ~Cleanup(){if(h) BCryptDestroyHash(h); if(a) BCryptCloseAlgorithmProvider(a,0);} } cleanup{alg,hash};
    DWORD size=0, returned=0;
    check(BCryptGetProperty(alg,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),sizeof(size),&returned,0)>=0,
          "BCrypt object length");
    object.resize(size); std::array<std::uint8_t,32> result{};
    check(BCryptCreateHash(alg,&hash,object.data(),size,nullptr,0,0)>=0,"BCrypt create hash");
    check(bytes.size<=std::numeric_limits<ULONG>::max(),"test hash input length");
    check(BCryptHashData(hash,const_cast<PUCHAR>(bytes.data),static_cast<ULONG>(bytes.size),0)>=0,"BCrypt hash bytes");
    check(BCryptFinishHash(hash,result.data(),static_cast<ULONG>(result.size()),0)>=0,"BCrypt finish hash");
    return hex(result.data(),result.size());
#else
    (void)bytes; throw std::runtime_error("batch-real SHA256 requires Windows BCrypt; no substitute hash");
#endif
}
void write_file(const std::filesystem::path& p, const std::vector<std::uint8_t>& bytes) {
    std::ofstream f(p,std::ios::binary); f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    check(bool(f),"write owned fixture"); f.close(); check(!f.fail(),"close owned fixture");
}
struct Temp {
    std::filesystem::path parent,dir;
    Temp() {
        parent=std::filesystem::weakly_canonical(std::filesystem::temp_directory_path());
        dir=parent/("mka-stream-source-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        check(std::filesystem::create_directory(dir),"create exclusive owned fixture directory");
    }
    void cleanup() {
        if (dir.empty()) return;
        check(dir.parent_path()==parent && dir.filename().string().rfind("mka-stream-source-",0)==0,
              "exact owned cleanup boundary");
        check(std::filesystem::weakly_canonical(dir)==dir,"cleanup canonical boundary");
        for (const auto& e:std::filesystem::recursive_directory_iterator(dir)) {
            check(!e.is_symlink(),"no foreign symlink in fixture cleanup");
#ifdef _WIN32
            const auto attributes=GetFileAttributesW(e.path().c_str());
            check(attributes!=INVALID_FILE_ATTRIBUTES && !(attributes&FILE_ATTRIBUTE_REPARSE_POINT),"no reparse cleanup escape");
#endif
        }
        std::filesystem::remove_all(dir); check(!std::filesystem::exists(dir),"owned fixture cleanup verified"); dir.clear();
    }
    ~Temp(){try{cleanup();}catch(const std::exception& e){std::fprintf(stderr,"fixture cleanup failed: %s\n",e.what());}}
};
void compare(const StreamSourceGroup& g, const ResourceLookup& r, const std::vector<std::uint8_t>& bytes,
             std::size_t ordinal, std::size_t stride) {
    auto all=g.raw_group(),a=g.range0(),b=g.range1(); const auto m=g.metadata();
    check(all.size==G && a.size==stride && b.size==stride,"full immutable group and S/S spans");
    check(std::equal(all.data,all.data+G,bytes.data()+ordinal*G),"unmodified serialized group bytes");
    check(a.data==all.data && b.data==all.data+stride,"adjacent per-group span bases");
    check(m.resource.bytes==r.bytes && m.resource.index==r.index && m.resource.key_mode==r.key_mode &&
          m.resource.status==r.status && m.ordinal==ordinal,"raw36 and provenance preserved");
    check(m.key==word(r,8) && m.rate_value==word(r,12) && m.offset==word(r,16) &&
          m.length==word(r,20) && m.stride==stride && m.tail_control==word(r,28) &&
          m.group_count==word(r,20)/G,"all immutable raw group metadata");
}
void reject(const StreamSource& s, ResourceLookup r, std::uint64_t ordinal, StreamError expected) {
    StreamFailure f; auto g=s.read_group(r,ordinal,f);
    check(!g && f.kind==expected,"exact typed rejection with no result");
}
void sparse_fixture(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes, std::uint64_t offset) {
#ifdef _WIN32
    HANDLE h=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    check(h!=INVALID_HANDLE_VALUE,"create owned sparse fixture");
    struct Close {HANDLE h; ~Close(){CloseHandle(h);}} close{h}; DWORD done=0;
    check(DeviceIoControl(h,FSCTL_SET_SPARSE,nullptr,0,nullptr,0,&done,nullptr),"mark sparse before extending");
    LARGE_INTEGER at; at.QuadPart=static_cast<LONGLONG>(offset);
    check(SetFilePointerEx(h,at,nullptr,FILE_BEGIN),"64-bit sparse fixture seek");
    check(WriteFile(h,bytes.data(),static_cast<DWORD>(bytes.size()),&done,nullptr) && done==bytes.size(),"write only sparse groups");
#else
    std::ofstream f(path,std::ios::binary); f.seekp(offset); f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
    check(bool(f),"sparse fixture write");
#endif
}
void truncate_opened(const std::filesystem::path& path) {
#ifdef _WIN32
    HANDLE h=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                         nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    check(h!=INVALID_HANDLE_VALUE,"open actual fixture for post-open truncation");
    struct Close {HANDLE h; ~Close(){CloseHandle(h);}} close{h}; LARGE_INTEGER at; at.QuadPart=G;
    check(SetFilePointerEx(h,at,nullptr,FILE_BEGIN) && SetEndOfFile(h),"actual post-open truncation");
#else
    std::filesystem::resize_file(path,G);
#endif
}
int synthetic() {
    Temp temp;
    {
        std::vector<std::uint8_t> bytes(3*G),other(3*G);
        for(std::size_t i=0;i<bytes.size();++i){bytes[i]=static_cast<std::uint8_t>((i/G)*53+i*7+23);other[i]=bytes[i]^0xa5;}
        const auto a_path=temp.dir/"A.ms4",b_path=temp.dir/"B.ms4",cut_path=temp.dir/"cut.ms4";
        write_file(a_path,bytes);write_file(b_path,other);write_file(cut_path,bytes);
        StreamFailure f; check(!StreamSource::open_associated_ms4(temp.dir/"absent",f) && f.kind==StreamError::backing_failure,
                               "missing actual source file fails");
        auto opened=StreamSource::open_associated_ms4(a_path,f);check(opened.has_value() && f.kind==StreamError::none,"open A");
        StreamSource a=*opened; auto b=StreamSource::open_associated_ms4(b_path,f);check(b.has_value(),"open B");
        auto r=resource();
        for(std::size_t n=0;n<3;++n){auto g=a.read_group(r,n,f);check(g.has_value(),"first middle last group");compare(*g,r,bytes,n,48);}
        for(auto t:{0u,16u,0xffffffffu}) {put(r,28,t);auto g=a.read_group(r,2,f);check(g.has_value(),"opaque T final group");
            compare(*g,r,bytes,2,48);check(g->metadata().tail_control==t,"T preserved without trim/mutation");}
        for(auto key:{0u,1u,0x12340001u,0xabcd0001u}) {put(r,8,key);auto g=a.read_group(r,0,f);
            check(g && g->metadata().key==key,"full numeric key including zero/non-normal bit");}
        r=resource();auto invalid=r;invalid.status=ResourceLookupStatus::absent;reject(a,invalid,0,StreamError::resource_not_found);
        for(auto mode:{ResourceKeyMode::unknown,ResourceKeyMode::name}){invalid=r;invalid.key_mode=mode;put(invalid,8,0x20000000);reject(a,invalid,0,StreamError::unsupported_key_mode);}
        invalid=r;put(invalid,32,0);reject(a,invalid,0,StreamError::not_alternate_resource);
        for(auto stride:{0u,16385u,0xffffffffu}){invalid=r;put(invalid,24,stride);reject(a,invalid,0,StreamError::invalid_stride);}
        for(auto stride:{1u,3u,16384u}){auto raw=r;put(raw,24,stride);auto g=a.read_group(raw,0,f);check(g.has_value(),"safe raw stride");compare(*g,raw,bytes,0,stride);}
        invalid=r;put(invalid,20,0);reject(a,invalid,0,StreamError::unsupported_empty_extent);
        for(auto length:{1u,32767u,32769u,65535u,65537u}){invalid=r;put(invalid,20,length);reject(a,invalid,0,StreamError::unsupported_partial_extent);}
        invalid=r;put(invalid,16,3*G+1);reject(a,invalid,0,StreamError::extent_out_of_bounds);
        invalid=r;put(invalid,16,1);reject(a,invalid,0,StreamError::extent_out_of_bounds);
        reject(a,r,3,StreamError::group_out_of_bounds);reject(a,r,UINT64_MAX,StreamError::group_out_of_bounds);
        auto one=resource(0,G,48,0);auto only=a.read_group(one,0,f);check(only.has_value(),"one complete final group");compare(*only,one,bytes,0,48);
        auto ag=a.read_group(r,0,f),bg=b->read_group(r,0,f);check(ag && bg,"two explicit associations");
        compare(*ag,r,bytes,0,48);compare(*bg,r,other,0,48);
        // Explicit B association is valid raw I/O, not authenticated as A's bank.
        check(ag->associated_path()==a_path && bg->associated_path()==b_path,"association labels retained");
        auto source_copy=a; auto moved=std::move(source_copy);reject(source_copy,r,0,StreamError::closed);
        check(source_copy.backing_size()==0 && source_copy.associated_path().empty(),"moved source empty");
        moved=std::move(moved);source_copy=moved;check(source_copy.read_group(r,0,f).has_value(),"source selfmove/copy assignment");
        auto copy=*ag;auto group_moved=std::move(copy);check(copy.raw_group().size==0 && copy.range0().size==0 && copy.metadata().group_count==0,"moved group empty");
        group_moved=std::move(group_moved);compare(group_moved,r,bytes,0,48);copy=group_moved;compare(copy,r,bytes,0,48);
        std::vector<std::future<bool>> jobs;
        for(unsigned n=0;n<12;++n)jobs.push_back(std::async(std::launch::async,[a,r,n,&bytes]{StreamFailure e;auto g=a.read_group(r,n%3,e);return g && e.kind==StreamError::none && std::equal(g->raw_group().data,g->raw_group().data+G,bytes.data()+(n%3)*G);}));
        for(auto& job:jobs)check(job.get(),"concurrent shared-backing reads exact");
        std::optional<StreamSourceGroup> retained;
        {auto isolated=StreamSource::open_associated_ms4(a_path,f);retained=isolated->read_group(r,1,f);check(retained.has_value(),"owned result");}
        compare(*retained,r,bytes,1,48);
        auto cut=StreamSource::open_associated_ms4(cut_path,f);check(cut.has_value(),"open truncation source");truncate_opened(cut_path);
        auto failed=cut->read_group(r,2,f);check(!failed && f.kind==StreamError::backing_failure &&
            f.backing.kind==ArchiveError::entry_out_of_bounds,"actual post-open truncated extent fails without zero result");
        auto after=a.read_group(r,0,f);check(after && f.kind==StreamError::none && f.backing.kind==ArchiveError::none,"success clears old failure");
        const auto sparse=temp.dir/"sparse.ms4";const std::uint64_t offset=0xfffff000ull;
        std::vector<std::uint8_t> sparse_bytes(bytes.begin(),bytes.begin()+2*G);sparse_fixture(sparse,sparse_bytes,offset);
        auto wide=StreamSource::open_associated_ms4(sparse,f);check(wide && wide->backing_size()==offset+2*G && wide->backing_size()>UINT32_MAX,"retained host backing beyond4GiB");
        auto wr=resource(static_cast<std::uint32_t>(offset),2*G,48,0);auto wg=wide->read_group(wr,1,f);check(wg.has_value(),"group above4GiB");
        compare(*wg,wr,sparse_bytes,1,48);check(wg->metadata().offset==offset && wg->metadata().length==2*G,"64-bit extent arithmetic");
    }
    temp.cleanup();std::puts("[stream-synthetic-positive] raw ownership/modes/extents/truncation/sparse/concurrency passed; temporary cleanup verified");return 0;
}
template<class T> T number(const std::string& s,int base) {
    T value{};auto end=std::from_chars(s.data(),s.data()+s.size(),value,base);
    check(!s.empty() && end.ec==std::errc{} && end.ptr==s.data()+s.size(),"strict manifest integer");return value;
}
int batch(const char* ms2,const char* ms4,const char* manifest) {
    ArchiveFailure af;auto archive=SoundArchive::open(ms2,af);check(archive.has_value(),"open explicit real MS2");
    StreamFailure sf;auto stream=StreamSource::open_associated_ms4(ms4,sf);check(stream.has_value(),"open explicit associated real MS4");
    std::ifstream in(manifest);check(bool(in),"open explicit TSV manifest");
    struct Bank {ArchiveRecord origin;SoundBank parsed;};std::map<std::string,Bank> banks;std::set<std::string> seen;
    std::string line;std::size_t rows=0;
    while(std::getline(in,line)) {
        if(!line.empty() && line.back()=='\r')line.pop_back();if(line.empty() || line[0]=='#')continue;
        check(seen.insert(line).second,"duplicate manifest row");const auto a=line.find('\t'),b=line.find('\t',a==std::string::npos?0:a+1);
        check(a!=std::string::npos && b!=std::string::npos && line.find('\t',b+1)==std::string::npos,"manifest requires bank_name TAB eighthexkey TAB decimalordinal");
        auto name=line.substr(0,a),key_text=line.substr(a+1,b-a-1);check(!name.empty() && key_text.size()==8,"bank name and full eighthex key required");
        const auto key=number<std::uint32_t>(key_text,16);const auto ordinal=number<std::uint64_t>(line.substr(b+1),10);
        auto it=banks.find(name);
        if(it==banks.end()) {auto raw=archive->read_bank(name,af);check(raw.has_value(),"real bank name lookup");BankError error;
            auto parsed=SoundBank::parse(std::move(raw->bytes),error);check(parsed.has_value(),"actual named type11 bank parse");
            it=banks.emplace(name,Bank{raw->origin,std::move(*parsed)}).first;}
        const auto resource=it->second.parsed.lookup_resource(key);check(resource.status==ResourceLookupStatus::found,"exact fullkey resolves in actual owning bank");
        auto g=stream->read_group(resource,ordinal,sf);check(g.has_value(),"read actual alternate raw group");const auto m=g->metadata();
        check(m.key==key && m.ordinal==ordinal && m.resource.bytes==resource.bytes && m.resource.index==resource.index &&
            m.resource.key_mode==resource.key_mode && m.resource.status==resource.status,"real raw metadata/provenance exact");
        const auto raw=g->raw_group(),p0=g->range0(),p1=g->range1();check(raw.size==G && p0.size==m.stride && p1.size==m.stride,"real fullfinal S/S shape");
        const auto& origin=it->second.origin;
        std::cout<<"{\"bank_request\":"<<quoted(name)<<",\"bank_name\":"<<quoted(origin.name)<<",\"origin_key\":"<<origin.key
            <<",\"origin_offset\":"<<origin.offset<<",\"origin_size\":"<<origin.size<<",\"resource_index\":"<<resource.index
            <<",\"key\":"<<m.key<<",\"key_hex\":"<<quoted(key_text)<<",\"ordinal\":"<<ordinal<<",\"offset\":"<<m.offset
            <<",\"length\":"<<m.length<<",\"stride\":"<<m.stride<<",\"tail_raw\":"<<m.tail_control<<",\"rate_raw\":"<<m.rate_value
            <<",\"group_count\":"<<m.group_count<<",\"raw_resource_hex\":"<<quoted(hex(resource.bytes.data(),resource.bytes.size()))
            <<",\"associated_ms4\":"<<quoted(g->associated_path().string())<<",\"raw_bytes\":"<<raw.size
            <<",\"range0_bytes\":"<<p0.size<<",\"range1_bytes\":"<<p1.size<<",\"raw_group_sha256\":"<<quoted(sha256(raw))
            <<",\"range0_sha256\":"<<quoted(sha256(p0))<<",\"range1_sha256\":"<<quoted(sha256(p1))<<"}\n";
        check(bool(std::cout),"batch output write");++rows;
    }
    check(in.eof() && rows>0,"complete nonempty manifest read");std::cout.flush();check(bool(std::cout),"flush batch output");
    std::fprintf(stderr,"[stream-batch-positive] rows=%zu actual bank lookups=%zu; raw SHA256 only\n",rows,banks.size());return 0;
}
}
int main(int argc,char** argv) {
    try {
        if(argc==2 && std::string(argv[1])=="synthetic")return synthetic();
        if(argc==5 && std::string(argv[1])=="batch-real")return batch(argv[2],argv[3],argv[4]);
        std::fprintf(stderr,"usage: test_stream_source synthetic | batch-real <MS2> <associated-MS4> <bank-key-ordinal.tsv>\n");return 2;
    } catch(const std::exception& e){std::fprintf(stderr,"[stream-fixture-fail] %s\n",e.what());return 1;}
}
