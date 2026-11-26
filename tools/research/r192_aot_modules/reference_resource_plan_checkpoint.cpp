// User-scope DPAPI authenticates accidental/external byte alteration, not
// origin against arbitrary code running as the same Windows user. Accepted
// paired witnesses must originate from this trusted committed-Plan exporter.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <wincrypt.h>
#include <bcrypt.h>
#include "reference_resource_plan_checkpoint.hpp"
#include <filesystem>
#include <fstream>
#include <set>

namespace ps2::iop::kernel {
void r192_fresh_before_machine_restore();
void r192_fresh_after_machine_restore(bool complete_machine_success);
}
namespace mka::emitted::resource_plan_checkpoint {
using resource_owner_epochs::CheckpointOwner;
using resource_owner_epochs::SourceProgram;
using namespace reference_resources;
using Bytes=std::vector<std::uint8_t>;
using Digest=std::array<std::uint8_t,32>;
namespace {
constexpr std::size_t limit=16u<<20;
constexpr char domain[]="MKA source Plan checkpoint v1; ELF a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f; Cpu1456";
constexpr char abi[]="v8-little-endian-Cpu1456;cpu.hpp=2eb8c482859337add6480f0a1f0ad79ab908dda6d878d575f84f554ecf81c0fe;state.hpp=1c2cc5173971b9645d213942171cf1a0906b754bd9ada990235f644a634fa3eb";
[[noreturn]]void fail(const char*s){throw std::runtime_error(std::string("native Plan checkpoint: ")+s);}
void need(bool x,const char*s){if(!x)fail(s);}
Digest hash(std::span<const std::uint8_t>data){
    BCRYPT_ALG_HANDLE algorithm{};BCRYPT_HASH_HANDLE h{};
    struct Close{BCRYPT_ALG_HANDLE&a;BCRYPT_HASH_HANDLE&h;~Close(){if(h)BCryptDestroyHash(h);if(a)BCryptCloseAlgorithmProvider(a,0);}}close{algorithm,h};
    auto nt=[](NTSTATUS s){if(s<0)fail("SHA256 service failure");};
    nt(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0));DWORD bytes{},got{};
    nt(BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&bytes),4,&got,0));Bytes object(bytes);
    nt(BCryptCreateHash(algorithm,&h,object.data(),bytes,nullptr,0,0));
    while(!data.empty()){const auto n=ULONG(std::min<std::size_t>(data.size(),1u<<20));nt(BCryptHashData(h,const_cast<PUCHAR>(data.data()),n,0));data=data.subspan(n);}
    Digest result{};nt(BCryptFinishHash(h,result.data(),32,0));return result;
}
Bytes read_file(const std::filesystem::path&p,std::size_t maximum){
    std::error_code e;const auto size=std::filesystem::file_size(p,e);need(!e&&size<=maximum,"file size bound");
    Bytes result(std::size_t(size),0);std::ifstream f(p,std::ios::binary);if(size)f.read(reinterpret_cast<char*>(result.data()),std::streamsize(size));need(bool(f)&&f.peek()==EOF,"file read extent");return result;
}
Digest self_hash(){static const Digest value=[](){std::array<wchar_t,32768>path{};const auto n=GetModuleFileNameW(nullptr,path.data(),DWORD(path.size()));need(n&&n<path.size(),"writer executable path");return hash(read_file(std::filesystem::path(path.data()),512u<<20));}();return value;}
bool hex64(const std::string&s){return s.size()==64&&std::all_of(s.begin(),s.end(),[](unsigned char c){return c>='0'&&c<='9'||c>='a'&&c<='f';});}
std::filesystem::path path(const std::string&s){return std::filesystem::u8path(s);}
std::filesystem::path sidecar(const std::string&s){auto p=path(s);p+=L".native-plans";return p;}
constexpr std::array<std::uint8_t,16>trailer_magic={'M','K','A','P','L','A','N','E','N','V','1',0,0,0,0,0};
struct Envelope{bool paired{};std::size_t payload{};};
Envelope envelope(std::span<const std::uint8_t>state){
    if(state.size()>=32&&std::equal(trailer_magic.begin(),trailer_magic.end(),state.end()-32)){
        unsigned schema{},required{};std::uint64_t bytes{};std::memcpy(&schema,state.data()+state.size()-16,4);std::memcpy(&required,state.data()+state.size()-12,4);std::memcpy(&bytes,state.data()+state.size()-8,8);
        need(schema==1&&required==1&&bytes==state.size()-32,"native state envelope schema or payload bounds");
        need(bytes>=8&&std::memcmp(state.data()+bytes-8,"end\0\0\0\0\0",8)==0,"native state payload end tag");return {true,std::size_t(bytes)};
    }
    // Authentic legacy serializers end with this fixed tag. Reject trailing
    // damaged/unrecognized envelopes instead of treating them as legacy data.
    need(state.size()>=8&&std::memcmp(state.data()+state.size()-8,"end\0\0\0\0\0",8)==0,"legacy end tag or unknown native envelope");return {false,state.size()};
}
void append_envelope(Bytes&state){
    const auto old=envelope(state);need(!old.paired&&state.size()<=(512u<<20)-32,"new state envelope bound");
    const std::uint64_t bytes=state.size();state.insert(state.end(),trailer_magic.begin(),trailer_magic.end());const unsigned one=1;
    for(unsigned i=0;i<2;++i){const auto*p=reinterpret_cast<const std::uint8_t*>(&one);state.insert(state.end(),p,p+4);}const auto*p=reinterpret_cast<const std::uint8_t*>(&bytes);state.insert(state.end(),p,p+8);
}
void new_save_path(const std::string&s){
    const char*configured=r192_family_checkpoint_owned_root();need(configured&&*configured,"configured save root missing");
    const auto root=std::filesystem::weakly_canonical(path(configured));
    const auto target=std::filesystem::weakly_canonical(path(s));
    need(!root.empty()&&!target.empty()&&target!=root,"configured save root missing");
    const auto relative=target.lexically_relative(root);need(!relative.empty()&&!relative.is_absolute()&&*relative.begin()!=L"..","save outside configured save area");
    need(!std::filesystem::exists(target)&&!std::filesystem::exists(sidecar(s)),"preexisting save or sidecar protected");
}
std::span<const std::uint8_t>saved_ram(std::span<const std::uint8_t>state){
    static_assert(sizeof(ps2::Cpu)==1456);constexpr std::size_t offset=8+4+8+sizeof(ps2::Cpu);
    need(state.size()>=offset+ps2::kRamSize,"v8 saved RAM extent");
    need(std::memcmp(state.data(),"PS2STATE",8)==0,"saved state magic");unsigned version{};std::memcpy(&version,state.data()+8,4);
    need(version==8&&std::memcmp(state.data()+12,"ee\0\0\0\0\0\0",8)==0,"saved state version or ee tag");
    return state.subspan(offset,ps2::kRamSize);
}
struct Writer{
    Bytes data;
    void u32(unsigned v){for(unsigned i=0;i<4;++i)data.push_back(std::uint8_t(v>>(8*i)));need(data.size()<=limit,"bundle size bound");}
    void blob(std::span<const std::uint8_t>s){need(s.size()<=limit-data.size(),"bundle size bound");data.insert(data.end(),s.begin(),s.end());}
    void text(const std::string&s){need(s.size()<=1024,"text bound");u32(unsigned(s.size()));blob({reinterpret_cast<const std::uint8_t*>(s.data()),s.size()});}
};
struct Reader{
    std::span<const std::uint8_t>data;
    unsigned u32(){need(data.size()>=4,"truncated bundle");unsigned v{};for(unsigned i=0;i<4;++i)v|=unsigned(data[i])<<(8*i);data=data.subspan(4);return v;}
    Bytes blob(unsigned n){need(n<=data.size(),"truncated bundle bytes");Bytes result(data.begin(),data.begin()+n);data=data.subspan(n);return result;}
    std::string text(){const auto n=u32();need(n<=1024,"text bound");auto raw=blob(n);return {raw.begin(),raw.end()};}
};
void write_input(Writer&w,const construction::CapturedPlanInput&p){
    need(p.original_attributes.size()<=20&&p.original_effects.size()<=13&&p.original_filters.size()<=35,"source input count bound");
    w.u32(unsigned(p.original_attributes.size()));for(auto&a:p.original_attributes){w.u32(a.tag);w.u32(a.stream);w.u32(a.offset);}
    w.u32(unsigned(p.original_effects.size()));for(auto&e:p.original_effects){w.u32(e.id);w.u32(e.destination_tag);w.u32(e.source_tag);for(auto v:e.constant)w.u32(v);w.u32(unsigned(e.remap.upper));w.u32(unsigned(e.remap.lower));w.u32(e.remap.start);w.u32(e.remap.extent);}
    w.u32(unsigned(p.original_filter_count));w.u32(unsigned(p.original_filters.size()));for(auto&f:p.original_filters){w.u32(unsigned(std::int32_t(f.id)));w.u32(f.source_tag);w.u32(f.linked_descriptor);w.u32(f.limit);}
}
auto read_input(Reader&r){
    std::vector<Attribute>a;std::vector<Effect>e;std::vector<construction::RawFilter>f;
    auto n=r.u32();need(n<=20,"attribute count bound");for(unsigned i=0;i<n;++i)a.push_back({r.u32(),r.u32(),r.u32()});
    n=r.u32();need(n<=13,"effect count bound");for(unsigned i=0;i<n;++i){Effect x{};x.id=r.u32();x.destination_tag=r.u32();x.source_tag=r.u32();for(auto&v:x.constant)v=r.u32();x.remap={std::int32_t(r.u32()),std::int32_t(r.u32()),r.u32(),r.u32()};e.push_back(x);}
    const auto declared=std::int32_t(r.u32());n=r.u32();need(n<=35,"filter record count bound");for(unsigned i=0;i<n;++i){const auto id=std::int32_t(r.u32());need(id>=INT16_MIN&&id<=INT16_MAX,"filter ID width");f.push_back({std::int16_t(id),r.u32(),r.u32(),r.u32()});}
    return std::make_shared<const construction::CapturedPlanInput>(construction::capture_plan_input(a,e,f,declared));
}
void validate_saved(std::span<const CheckpointOwner>rows,std::span<const std::uint8_t>ram){
    auto range=[&](std::uint64_t p,std::uint64_t n){return p<=ram.size()&&n<=ram.size()-p;};
    auto word=[&](unsigned p){need(!(p&3)&&range(p,4),"saved RAM word bounds");unsigned v;std::memcpy(&v,ram.data()+p,4);return v;};
    need(rows.size()<=4096,"owner count bound");std::set<unsigned>owners,programs;
    for(const auto&o:rows){need(owners.insert(o.cache).second&&range(o.cache,784)&&o.bytes&&range(o.backing,o.bytes),"owner identity or extent");
        auto inside=[&](std::uint64_t p,std::uint64_t n){return p>=o.backing&&range(p,n)&&p-o.backing<=o.bytes&&n<=o.bytes-(p-o.backing);};
        need(word(o.cache)==0x59fd90&&word(o.cache+4)==o.identity&&o.identity&&!(ram[o.cache+8]&1)&&word(o.cache+60)==o.backing,"saved cache changed");
        need(word(0x59fd90)==0x12d870&&word(0x59fd94)==0x12d860&&word(0x59fd98)==0x12d830&&word(0x59fd9c)==0x210920,"saved original table changed");
        const auto owner=o.cache+64,count=word(owner+524),table=word(owner+528),pool=word(owner+636);
        need(count&&count<=10&&inside(table,4ull*count+928ull*count)&&pool<=table&&inside(pool,table-pool),"saved owner table extent");
        need(!o.programs.empty()&&o.programs.size()<=10,"program count bound");
        for(const auto&p:o.programs){need(p.input&&p.copy_origin_verified&&p.cache==o.cache&&p.bytes&&p.bytes%4==0&&p.bytes<=8000&&programs.insert(p.descriptor).second,"program identity or width");
            bool member=false;for(unsigned i=0;i<count;++i){const auto d=word(table+4*i);need(d==std::uint64_t(table)+4ull*count+928ull*i,"saved descriptor table layout");member|=d==p.descriptor;}
            need(member&&word(p.descriptor+72)==owner&&word(p.descriptor+76)==p.code&&word(p.descriptor+916)==p.data_start_words&&p.code>=pool&&std::uint64_t(p.code)+p.bytes<=table&&inside(p.code,p.bytes),"saved descriptor binding");
            std::vector<Bits>storage(p.bytes/4);std::memcpy(storage.data(),ram.data()+p.code,p.bytes);const auto&i=*p.input;
            const auto equal=resource_word_equality(i.original_attributes,i.original_effects,i.outcome.effective,storage,p.data_start_words,p.bytes/4);need(equal.equal,"saved source code equations");
        }
    }
}
Bytes protect(std::span<const std::uint8_t>raw,bool seal){
    DATA_BLOB in{DWORD(raw.size()),const_cast<BYTE*>(raw.data())},entropy{DWORD(sizeof domain),reinterpret_cast<BYTE*>(const_cast<char*>(domain))},out{};
    const BOOL ok=seal?CryptProtectData(&in,L"MKA native source Plans",&entropy,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&out):CryptUnprotectData(&in,nullptr,&entropy,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&out);
    need(ok,"DPAPI seal rejected");struct Free{BYTE*p;~Free(){LocalFree(p);}}free{out.pbData};need(out.cbData<=limit+(64u<<10),"DPAPI extent bound");return {out.pbData,out.pbData+out.cbData};
}
struct PairData{Digest state_hash{};std::vector<CheckpointOwner>owners;bool paired{};};
thread_local PairData*active{};thread_local bool restoring{},family_started{},iop_started{};
struct SaveData{std::string name;Digest state_hash{};Bytes sealed;unsigned owners{};};
thread_local std::unique_ptr<SaveData>pending_save;
Bytes encode(const PairData&p){
    Writer w;w.text("MKA_SOURCE_PLANS");w.u32(1);w.u32(1456);w.text("a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f");
    w.text(abi);w.blob(p.state_hash);w.blob(self_hash());const std::string source=r192_family_checkpoint_source_id();need(hex64(source),"writer source identity missing");w.text(source);
    w.u32(unsigned(p.owners.size()));for(const auto&o:p.owners){w.u32(o.cache);w.u32(o.identity);w.u32(o.backing);w.u32(o.bytes);w.u32(unsigned(o.programs.size()));for(const auto&q:o.programs){w.u32(q.descriptor);w.u32(q.code);w.u32(q.bytes);w.u32(q.data_start_words);write_input(w,*q.input);}}
    const auto sum=hash(w.data);w.blob(sum);return w.data;
}
PairData decode(std::span<const std::uint8_t>plain,std::span<const std::uint8_t>state){
    need(plain.size()>=32&&plain.size()<=limit,"plain bundle extent");auto payload=plain.first(plain.size()-32);const auto sum=hash(payload);need(std::memcmp(sum.data(),plain.data()+payload.size(),32)==0,"bundle checksum");
    Reader r{payload};need(r.text()=="MKA_SOURCE_PLANS"&&r.u32()==1&&r.u32()==1456,"schema or ABI mismatch");need(r.text()=="a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f","original ELF mismatch");
    need(r.text()==abi,"fixed serializer ABI fingerprint mismatch");
    PairData p;p.paired=true;const auto saved=r.blob(32);p.state_hash=hash(state);need(std::equal(saved.begin(),saved.end(),p.state_hash.begin()),"wrong or stale state pair");
    r.blob(32);need(hex64(r.text()),"writer source provenance malformed");const auto n=r.u32();need(n<=4096,"owner count bound");
    for(unsigned i=0;i<n;++i){CheckpointOwner o;o.cache=r.u32();o.identity=r.u32();o.backing=r.u32();o.bytes=r.u32();const auto m=r.u32();need(m&&m<=10,"program count bound");for(unsigned j=0;j<m;++j){SourceProgram q;q.cache=o.cache;q.descriptor=r.u32();q.code=r.u32();q.bytes=r.u32();q.data_start_words=r.u32();q.copy_origin_verified=true;q.input=read_input(r);o.programs.push_back(std::move(q));}p.owners.push_back(std::move(o));}
    need(r.data.empty(),"trailing bundle bytes");validate_saved(p.owners,saved_ram(state));return p;
}
}

void before_save(const std::string&name){new_save_path(name);pending_save.reset();}
void prepare_save(const std::string&name,std::vector<std::uint8_t>&state){
    // Called AFTER the original family_pending barrier and complete Writer,
    // but BEFORE std::ofstream creates/truncates the new game-state file.
    new_save_path(name);append_envelope(state);PairData p;p.paired=true;p.state_hash=hash(state);p.owners=r192_family_registry().export_committed();
    validate_saved(p.owners,saved_ram(state));auto pending=std::make_unique<SaveData>();pending->name=name;pending->state_hash=p.state_hash;pending->owners=unsigned(p.owners.size());pending->sealed=protect(encode(p),true);
    pending_save=std::move(pending);
}
void saved(const std::string&name,std::span<const std::uint8_t>state){
    // before_save has already protected preexisting paths; now verify that the
    // completed original state file contains exactly the Writer's byte vector.
    need(pending_save&&pending_save->name==name&&pending_save->state_hash==hash(state),"save was not preflighted before state creation");
    need(hash(read_file(path(name),512u<<20))==pending_save->state_hash,"saved file does not match serialized state");const auto&sealed=pending_save->sealed;
    const auto destination=sidecar(name);need(!std::filesystem::exists(destination),"preexisting sidecar protected");auto temporary=destination;temporary+=L".tmp";need(!std::filesystem::exists(temporary),"preexisting checkpoint temporary protected");
    HANDLE h=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);need(h!=INVALID_HANDLE_VALUE,"sidecar temporary creation");
    DWORD wrote{};const bool okay=WriteFile(h,sealed.data(),DWORD(sealed.size()),&wrote,nullptr)&&wrote==sealed.size()&&FlushFileBuffers(h);CloseHandle(h);need(okay,"sidecar write failure");
    need(MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_WRITE_THROUGH),"sidecar atomic publication");
    std::fprintf(stderr,"[native-plan-checkpoint] saved owners=%u paired=1\n",pending_save->owners);pending_save.reset();
}
struct PathLoad::Impl{PairData prepared;};
PathLoad::PathLoad(const std::string&name,std::span<const std::uint8_t>state):impl_(std::make_unique<Impl>()){
    need(!active&&!restoring,"nested state restore");const auto metadata=sidecar(name);const auto shape=envelope(state);
    if(std::filesystem::exists(metadata)){need(shape.paired,"paired Plans require native state envelope");impl_->prepared=decode(protect(read_file(metadata,limit+(64u<<10)),false),state);}
    else{need(!shape.paired,"paired native Plans required but absent");impl_->prepared.state_hash=hash(state);std::fprintf(stderr,"[native-plan-checkpoint] legacy state without paired Plans\n");}
    active=&impl_->prepared;
}
PathLoad::~PathLoad(){if(active==&impl_->prepared)active=nullptr;}
void begin_buffer_restore(std::span<const std::uint8_t>state){
    need(!restoring,"nested progressive state restore");const auto shape=envelope(state);need(!shape.paired||active&&active->paired,"byte-only native state lacks paired preflight");if(active)need(hash(state)==active->state_hash,"state changed after paired preflight");
    restoring=true;family_started=iop_started=false;
    try{r192_family_restore_begin();family_started=true;ps2::iop::kernel::r192_fresh_before_machine_restore();iop_started=true;}
    catch(...){if(family_started)r192_family_restore_end(false);if(iop_started)ps2::iop::kernel::r192_fresh_after_machine_restore(false);restoring=family_started=iop_started=false;throw;}
}
std::span<const std::uint8_t>payload_for_restore(std::span<const std::uint8_t>state){return state.first(envelope(state).payload);}
void end_buffer_restore(bool success){
    if(!restoring)return;
    try{
        if(!success){if(family_started)r192_family_restore_end(false);if(iop_started)ps2::iop::kernel::r192_fresh_after_machine_restore(false);restoring=family_started=iop_started=false;return;}
        r192_family_restore_end(true);if(active&&active->paired)r192_family_registry().import_paired_checkpoint(active->owners);
        ps2::iop::kernel::r192_fresh_after_machine_restore(true);restoring=family_started=iop_started=false;
        std::fprintf(stderr,"[native-plan-checkpoint] restored owners=%zu paired=%u new_host_epochs=1\n",active?active->owners.size():0,unsigned(active&&active->paired));
    }catch(...){if(family_started){r192_family_restore_begin();r192_family_restore_end(false);}if(iop_started)ps2::iop::kernel::r192_fresh_after_machine_restore(false);restoring=family_started=iop_started=false;throw;}
}
}
