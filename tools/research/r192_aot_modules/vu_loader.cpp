// Sealed, offline-generated VU1 code. No runtime translator or interpreter.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include "vu_loader.hpp"
#include "ps2/vu_ops.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef R192_VU_MANIFEST_SHA256
#error Compile the exact offline package seal
#endif
#ifndef R192_VU_SOURCE_STAMP
#error Compile the exact source and ABI stamp
#endif

namespace mka::aot {
namespace {
[[noreturn]] void fail(const std::string& why) {
    throw std::runtime_error("[sealed-vu-aot] " + why);
}
void nt(NTSTATUS result) { if(result < 0) fail("SHA-256 operation failed"); }
class Hash {
    BCRYPT_ALG_HANDLE algorithm{};
    BCRYPT_HASH_HANDLE hash{};
    std::vector<UCHAR> object;
public:
    Hash() {
        nt(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0));
        DWORD bytes{},received{};
        try {
            nt(BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&bytes),sizeof bytes,&received,0));
            object.resize(bytes);
            nt(BCryptCreateHash(algorithm,&hash,object.data(),bytes,nullptr,0,0));
        } catch(...) { BCryptCloseAlgorithmProvider(algorithm,0); throw; }
    }
    ~Hash() { if(hash)BCryptDestroyHash(hash); if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0); }
    void add(const void* p,ULONG bytes) { nt(BCryptHashData(hash,reinterpret_cast<PUCHAR>(const_cast<void*>(p)),bytes,0)); }
    std::string finish() {
        std::array<UCHAR,32> bytes{};nt(BCryptFinishHash(hash,bytes.data(),bytes.size(),0));
        std::string out;
        for(auto b:bytes) { out+="0123456789abcdef"[b>>4];out+="0123456789abcdef"[b&15]; }
        return out;
    }
};
struct File {
    HANDLE value=INVALID_HANDLE_VALUE;
    explicit File(const std::filesystem::path& p) {
        value=CreateFileW(p.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(value==INVALID_HANDLE_VALUE)fail("cannot lock package file: "+p.filename().string());
        BY_HANDLE_FILE_INFORMATION info{};
        if(!GetFileInformationByHandle(value,&info)||(info.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY))) {
            CloseHandle(value);value=INVALID_HANDLE_VALUE;fail("package file is not regular");
        }
    }
    File(const File&)=delete;
    ~File(){if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
    std::string digest(std::string* text=nullptr) {
        Hash hash;std::array<unsigned char,65536> buffer{};DWORD count{};
        for(;;) {
            if(!ReadFile(value,buffer.data(),buffer.size(),&count,nullptr))fail("package read failed");
            if(!count)break;
            hash.add(buffer.data(),count);
            if(text){if(text->size()+count>1024*1024)fail("manifest size exceeds bound");text->append(reinterpret_cast<char*>(buffer.data()),count);}
        }
        return hash.finish();
    }
};
struct Library {
    File locked;
    HMODULE module{};
    explicit Library(const std::filesystem::path& p):locked(p){}
    ~Library(){if(module)FreeLibrary(module);}
};
bool digest(const std::string& value) {
    return value.size()==64&&value.find_first_not_of("0123456789abcdef")==std::string::npos;
}
void mapped(HMODULE library,const void* p,std::size_t bytes,bool executable=false) {
    auto cursor=reinterpret_cast<std::uintptr_t>(p);
    if(!p||!bytes||bytes>std::numeric_limits<std::uintptr_t>::max()-cursor)fail("invalid metadata span");
    const auto end=cursor+bytes;
    while(cursor<end) {
        MEMORY_BASIC_INFORMATION info{};
        if(!VirtualQuery(reinterpret_cast<void*>(cursor),&info,sizeof info)||info.AllocationBase!=library||
           info.State!=MEM_COMMIT||(info.Protect&(PAGE_NOACCESS|PAGE_GUARD)))fail("metadata outside sealed DLL");
        if(executable&&!(info.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))fail("nonexecutable module function");
        const auto next=reinterpret_cast<std::uintptr_t>(info.BaseAddress)+info.RegionSize;
        if(next<=cursor)fail("invalid DLL page bounds");
        cursor=std::min(end,next);
    }
}
void chain(int unit,std::uint32_t pc) {
    if(unit!=1)fail("invalid compiled VU1 chain unit");
    // Frozen v8 VU1 dispatch selects a registered native function or halts.
    // It does not fall through to the VU0 interpreter on this unit. Preserve
    // the dispatcher's original PC masking, including wrapped VI targets.
    ps2::vu::interpret(1,pc);
}
ps2::vu::LiveHost host{chain,ps2::vu::kick,ps2::vu::top,ps2::vu::itop};
struct Resident {
    std::vector<std::unique_ptr<Library>> libraries;
    std::vector<ps2::vu::Program> programs;
};
Resident*& resident(){static Resident* result=nullptr;return result;}
}

void initialize_precompiled_vu_modules(const std::filesystem::path& path) {
    if(resident())fail("initializer called twice");
    const auto directory=std::filesystem::canonical(path);
    File manifest(directory/"manifest.txt");std::string text;
    if(!digest(R192_VU_MANIFEST_SHA256)||manifest.digest(&text)!=R192_VU_MANIFEST_SHA256)fail("manifest seal mismatch");
    std::istringstream input(text);std::string line,schema,stamp_text,extra;
    std::uint64_t regs{},host_bytes{},modules{},unique{};
    if(!std::getline(input,line))fail("missing manifest header");
    std::istringstream header(line);
    if(!(header>>schema>>stamp_text>>regs>>host_bytes>>modules>>unique)||header>>extra||schema!="R192_AOT_VU_V1")fail("invalid manifest header");
    if(stamp_text.empty()||stamp_text.size()>16||stamp_text.find_first_not_of("0123456789abcdefABCDEF")!=std::string::npos)fail("invalid source stamp");
    const auto stamp=std::stoull(stamp_text,nullptr,16);
    if(!stamp||stamp!=static_cast<std::uint64_t>(R192_VU_SOURCE_STAMP))fail("source stamp differs from compiled seal");
    if(regs!=sizeof(ps2::Vu0Regs)||host_bytes!=sizeof(ps2::vu::LiveHost))fail("manifest ABI mismatch");
    if(!modules||modules>2048||!unique||unique>4194304)fail("manifest denominator outside bounds");
    struct Row {std::string hash,key_hash,name;std::uint32_t count{};};
    std::vector<Row> rows;std::set<std::string> names;
    while(std::getline(input,line)) {
        Row row;std::istringstream fields(line);
        if(!(fields>>row.hash>>row.key_hash>>row.count>>row.name)||fields>>extra||!digest(row.hash)||!digest(row.key_hash)||
           !row.count||row.count>2048||row.name.size()<5||row.name.size()>160||!row.name.ends_with(".dll")||
           row.name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_.-")!=std::string::npos||row.name.find("..")!=std::string::npos)fail("invalid manifest row");
        if(!names.insert(row.name).second||(!rows.empty()&&rows.back().name>=row.name))fail("duplicate or unordered DLL names");
        rows.push_back(std::move(row));
    }
    if(rows.size()!=modules)fail("DLL denominator mismatch");
    auto pending=std::make_unique<Resident>();std::map<std::uint64_t,ps2::vu::ProgramFn> merged;
    for(const auto& row:rows) {
        auto library=std::make_unique<Library>(directory/row.name);
        if(library->locked.digest()!=row.hash)fail("DLL seal mismatch: "+row.name);
        library->module=LoadLibraryExW((directory/row.name).c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!library->module)fail("cannot load sealed native DLL: "+row.name);
        auto symbol=[&](const char* name){auto p=GetProcAddress(library->module,name);mapped(library->module,reinterpret_cast<void*>(p),1,true);return p;};
        using Stamp=std::uint64_t(*)();using Size=unsigned(*)();
        using Init=const ps2::vu::Program*(*)(const ps2::vu::LiveHost*,unsigned*);
        const auto get_stamp=reinterpret_cast<Stamp>(symbol("mka_vu_aot_stamp"));
        const auto get_regs=reinterpret_cast<Size>(symbol("mka_vu_aot_regs_size"));
        const auto get_host=reinterpret_cast<Size>(symbol("mka_vu_aot_host_size"));
        const auto init=reinterpret_cast<Init>(symbol("mka_vu_aot_init"));
        if(get_stamp()!=stamp||get_regs()!=sizeof(ps2::Vu0Regs)||get_host()!=sizeof host)fail("DLL source/ABI mismatch: "+row.name);
        unsigned count{};const auto* table=init(&host,&count);
        if(count!=row.count)fail("DLL entry count mismatch: "+row.name);
        mapped(library->module,table,std::size_t(count)*sizeof *table);Hash keys;
        for(unsigned i=0;i<count;++i) {
            if(i&&table[i-1].key>=table[i].key)fail("unordered or duplicate module keys");
            mapped(library->module,reinterpret_cast<void*>(table[i].function),1,true);
            std::array<unsigned char,8> key{};
            for(unsigned b=0;b<8;++b)key[b]=static_cast<unsigned char>(table[i].key>>(8*b));
            keys.add(key.data(),key.size());merged.emplace(table[i].key,table[i].function);
        }
        if(keys.finish()!=row.key_hash)fail("DLL key-set digest mismatch: "+row.name);
        pending->libraries.push_back(std::move(library));
    }
    if(merged.size()!=unique)fail("unique key denominator mismatch");
    pending->programs.reserve(merged.size());
    for(const auto& [key,function]:merged)pending->programs.push_back({key,function});
    // No runtime registration before all files, ABI, tables and functions pass.
    // Ownership survives even if the reference registration allocates/throws.
    resident()=pending.release();
    ps2::vu::register_programs(resident()->programs.data(),resident()->programs.size());
    std::fprintf(stderr,"[sealed-vu-aot] registered %zu native entries from %zu offline DLLs; no runtime compiler\n",resident()->programs.size(),resident()->libraries.size());
}
}
