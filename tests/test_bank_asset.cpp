#include "mka/bank_asset.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
using namespace mka::sound;
namespace {
void check(bool ok,const char* message) { if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);} }
void put(std::vector<std::uint8_t>& bytes,std::size_t at,std::uint32_t value,unsigned count=4) {
    for(unsigned i=0;i<count;++i)bytes[at+i]=static_cast<std::uint8_t>(value>>(i*8));
}
std::vector<std::uint8_t> bank_fixture() {
    std::vector<std::uint8_t> b(236);
    put(b,0,11);put(b,4,0x13);put(b,8,2,2);put(b,10,1,2);put(b,12,2,2);
    put(b,16,164);put(b,20,48);put(b,24,68);put(b,28,84);put(b,32,64);
    put(b,64,1,2);put(b,76,2);put(b,84,0x20000002);b[92]=1;b[132]=7;
    for(unsigned i=0;i<2;++i) { const auto at=164+i*36;put(b,at,0xffffffff);put(b,at+8,i?0x30000002:0x20000002);put(b,at+12,22050);put(b,at+16,256);put(b,at+20,36); }
    return b;
}
void payload_file(const std::filesystem::path& path,std::uint8_t nibbles,unsigned planes=1) {
    std::vector<std::uint8_t> b(256+20+planes*16);
    put(b,256+4,0x10000);put(b,256+8,planes);put(b,256+12,22050);put(b,256+16,16);
    for(unsigned i=0;i<planes;++i) { b[276+i*16]=12;b[277+i*16]=7;for(unsigned j=2;j<16;++j)b[276+i*16+j]=nibbles; }
    std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(b.data()),b.size());check(bool(file),"write synthetic backing");
}
BankAssetSource source(std::shared_ptr<SoundArchiveBacking> backing,std::vector<std::uint8_t> bytes=bank_fixture()) {
    return {{1,0,bytes.size(),"fixture.mst"},std::move(bytes),std::move(backing)};
}
std::uint64_t pcm_checksum(const std::vector<std::int16_t>& samples) {
    std::uint64_t value=14695981039346656037ull;
    for(auto sample:samples) { const auto word=static_cast<std::uint16_t>(sample);value=(value^(word&255))*1099511628211ull;value=(value^(word>>8))*1099511628211ull; }
    return value;
}
}
int main(int argc,char** argv) {
    const auto parent=std::filesystem::temp_directory_path();
    const auto dir=parent/("mka-bank-asset-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    check(std::filesystem::create_directory(dir),"exclusive temporary directory");
    payload_file(dir/"a.bin",0x11);payload_file(dir/"b.bin",0x22);payload_file(dir/"two.bin",0x11,2);payload_file(dir/"bad.bin",0x11);
    { std::fstream bad(dir/"bad.bin",std::ios::binary|std::ios::in|std::ios::out);bad.seekp(276);bad.put(0x50);check(bool(bad),"write malformed codec block"); }
    ArchiveFailure archive_error;BankAssetFailure failure;
    auto a=SoundArchiveBacking::open(dir/"a.bin",archive_error);auto b=SoundArchiveBacking::open(dir/"b.bin",archive_error);
    check(a && b,"open two actual backing identities");
    auto bank=BankAsset::parse(source(a),failure);check(bank && bank->origin().name=="fixture.mst","owned bank with provenance");
    const auto original=bank->bank().raw_bytes();
    auto result=bank->read_sample(0,0,failure);check(result && result->command.resource_bits()==0x20000002 && result->resource.index==0 && result->payload.encoded_payload().size==16,"exact selector command resource payload chain");
    check(bank->bank().raw_bytes()==original,"serialized commands and bank bytes unchanged");
    AdpcmHistory history{123,-456};auto mono=bank->decode_mono(0,0,history,failure);
    check(mono && mono->pcm.samples.size()==28 && mono->pcm.samples[0]==1 && mono->pcm.block_flags[0]==7 && mono->source.payload.rate_value()==22050,"explicit history mono decode with opaque rate and flags");
    auto other=BankAsset::parse(source(b),failure);history={};auto other_mono=other->decode_mono(0,0,history,failure);
    check(other_mono && other_mono->pcm.samples[0]==2,"new backing B cannot redirect retained A");
    auto moved=std::move(*bank);
    check(!bank->read_sample(0,0,failure) && failure.kind==BankAssetError::no_backing,"moved source safely closed");
    bank.reset();a.reset();b.reset();other.reset();history={};
    { auto copy=moved;copy=copy;copy=std::move(copy);AdpcmHistory copy_history{};check(copy.decode_mono(0,0,copy_history,failure)->pcm.samples[0]==1,"copy and self assignments retain original backing"); }
    check(moved.decode_mono(0,0,history,failure)->pcm.samples[0]==1 && result->payload.encoded_payload().data[2]==0x11,"bank and returned payload lifetimes independent of source owners");
    check(!moved.read_sample(-1,0,failure) && failure.kind==BankAssetError::absent_selector,"invalid selector absent");
    check(!moved.read_sample(1,0,failure) && failure.kind==BankAssetError::absent_program,"absent selector mapping");
    check(!moved.read_sample(0,2,failure) && failure.kind==BankAssetError::command_out_of_bounds,"command bounds");
    history={123,-456};check(!moved.decode_mono(0,1,history,failure) && failure.kind==BankAssetError::unsupported_command_kind && history.s1==123 && history.s2==-456,"unsupported kind preserves history");
    auto backing=SoundArchiveBacking::open(dir/"a.bin",archive_error);
    auto bytes=bank_fixture();put(bytes,84,0x30000002);auto full=BankAsset::parse(source(backing,bytes),failure);
    check(full->read_sample(0,0,failure)->resource.index==1,"same low16 keys retain full32 identity");
    put(bytes,84,0x40000002);auto missing=BankAsset::parse(source(backing,bytes),failure);
    check(!missing->read_sample(0,0,failure) && failure.kind==BankAssetError::absent_resource,"missing key never defaults");
    bytes=bank_fixture();put(bytes,84,0xffffffff);put(bytes,172,0);put(bytes,208,0xffffffff);
    auto null_key=BankAsset::parse(source(backing,bytes),failure);
    check(!null_key->read_sample(0,0,failure) && failure.kind==BankAssetError::payload_failed && failure.payload.kind==PayloadError::unsupported_key_lookup_route,"serialized minus1 relocates to exact zero, never rawFFFFFFFF resource");
    bytes=bank_fixture();put(bytes,4,3);put(bytes,40,236);put(bytes,172,0);put(bytes,208,2);bytes.insert(bytes.end(),{'a',0,'b',0});
    auto named=BankAsset::parse(source(backing,bytes),failure);check(named && !named->read_sample(0,0,failure) && failure.kind==BankAssetError::unsupported_named_command_route,"named command route explicit unsupported");
    bytes=bank_fixture();put(bytes,196,1);auto stream=BankAsset::parse(source(backing,bytes),failure);
    check(!stream->read_sample(0,0,failure) && failure.payload.kind==PayloadError::unsupported_alternate_streaming,"alternate streaming rejected");
    auto two_backing=SoundArchiveBacking::open(dir/"two.bin",archive_error);bytes=bank_fixture();put(bytes,184,52);
    auto two=BankAsset::parse(source(two_backing,bytes),failure);history={123,-456};
    check(!two->decode_mono(0,0,history,failure) && failure.kind==BankAssetError::unsupported_plane_count && history.s1==123 && history.s2==-456,"count2 not guessed or split");
    auto bad_backing=SoundArchiveBacking::open(dir/"bad.bin",archive_error);
    auto bad_codec=BankAsset::parse(source(bad_backing),failure);history={123,-456};
    check(!bad_codec->decode_mono(0,0,history,failure) && failure.kind==BankAssetError::decode_failed && failure.codec==AdpcmError::unsupported_filter && history.s1==123 && history.s2==-456,"codec failure publishes no history");
    check(!BankAsset::parse(source(nullptr),failure) && failure.kind==BankAssetError::no_backing,"backing required");
    check(!BankAsset::parse(source(backing,{1,2}),failure) && failure.kind==BankAssetError::malformed_bank,"malformed bank typed error");
    if(argc==2) {
        auto archive=SoundArchive::open(argv[1],archive_error);check(bool(archive),"open explicit real MS2");
        auto raw=archive->read_bank("announcr.msb",archive_error);check(bool(raw),"actual pathname selects bank");
        auto real=BankAsset::parse(std::move(*raw),failure);archive.reset();check(bool(real),"bank retains archive backing after index destruction");
        history={};auto decoded=real->decode_mono(0,0,history,failure);
        check(decoded && decoded->source.command.resource_bits()==0x20000002 && decoded->source.payload.encoded_payload().size==19840 && decoded->pcm.samples.size()==34720 && pcm_checksum(decoded->pcm.samples)==0x036bb89707b3cbbdull && history.s1==28672 && history.s2==28672,"real name selector resource payload PCM golden and history");
        std::printf("Real bank chain: %s resource%08X payload%zu PCM%zu\n",real->origin().name.c_str(),decoded->source.command.resource_bits(),decoded->source.payload.encoded_payload().size,decoded->pcm.samples.size()*2);
    }
    // Destroy retained handles before removing only this fresh temporary tree.
    full.reset();missing.reset();null_key.reset();named.reset();stream.reset();two.reset();two_backing.reset();bad_codec.reset();bad_backing.reset();backing.reset();
    // moved still retains a.bin; Windows read handle must close before cleanup.
    { auto release=std::move(moved); }
    check(dir.parent_path()==parent && dir.filename().string().find("mka-bank-asset-")==0,"cleanup confined to new directory");
    check(std::filesystem::remove_all(dir)==5 && !std::filesystem::exists(dir),"temporary fixtures removed");
    std::puts("bank asset tests passed");
}
