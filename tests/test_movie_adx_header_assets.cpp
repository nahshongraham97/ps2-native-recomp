#include "mka/movie_adx_header.hpp"
#include <array>
#include <charconv>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <string>
using namespace mka::video;
namespace {
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
std::int64_t number(std::string_view s){std::int64_t n;auto r=std::from_chars(s.data(),s.data()+s.size(),n);check(!s.empty()&&r.ec==std::errc{}&&r.ptr==s.data()+s.size()&&std::to_string(n)==s,"strict canonical signed decimal");return n;}
void json_string(const std::string& text){std::putchar('"');for(unsigned char c:text){if(c=='"'||c=='\\'){std::putchar('\\');std::putchar(c);}else if(c<32||c>=127)std::printf("\\u%04x",unsigned(c));else std::putchar(c);}std::putchar('"');}
struct Row{std::string key,file;std::array<std::int64_t,8> fields;};
}
int main(int argc,char** argv){try{
    check(argc==3,"usage: test_native_movie_adx_header_assets header_fixture_dir expected.tsv");
    std::ifstream manifest(std::filesystem::u8path(argv[2]),std::ios::binary);check(bool(manifest),"explicit manifest open");
    std::vector<Row> rows;std::set<std::string> keys,names;std::string line;
    while(std::getline(manifest,line)){
        if(!line.empty()&&line.back()=='\r')line.pop_back();std::array<std::string_view,10> parts;std::string_view remaining(line);
        for(std::size_t i=0;i<parts.size();++i){auto tab=remaining.find('\t');if(i==9){check(tab==remaining.npos,"extra manifest field");parts[i]=remaining;}else{check(tab!=remaining.npos,"missing manifest field");parts[i]=remaining.substr(0,tab);remaining.remove_prefix(tab+1);}}
        check(!parts[0].empty()&&parts[0].find('\0')==parts[0].npos&&keys.emplace(parts[0]).second,"unique nonempty source key");
        check(parts[1].size()==14&&parts[1].substr(0,7)=="header-"&&parts[1].substr(10)==".bin"&&parts[1][7]>='0'&&parts[1][7]<='9'&&parts[1][8]>='0'&&parts[1][8]<='9'&&parts[1][9]>='0'&&parts[1][9]<='9'&&names.emplace(parts[1]).second,"unique exact header-NNN.bin filename");
        Row row{std::string(parts[0]),std::string(parts[1]),{}};for(std::size_t i=0;i<8;++i)row.fields[i]=number(parts[i+2]);
        check(row.fields[0]>=38&&row.fields[0]<=65539&&row.fields[1]>0&&row.fields[1]<=UINT32_MAX&&row.fields[2]>0&&row.fields[2]<=UINT32_MAX&&row.fields[3]>0&&row.fields[3]<=65535,"serialized header scalar ranges");for(std::size_t i=4;i<8;++i)check(row.fields[i]>=-32768&&row.fields[i]<=32767,"signed16 history expected");rows.push_back(std::move(row));
    }
    check(manifest.eof()&&!manifest.bad()&&rows.size()==29,"complete exact29-row manifest");
    std::vector<MovieAdxHeader> held;held.reserve(29);
    for(const auto& row:rows){
        std::ifstream input(std::filesystem::u8path(argv[1])/std::filesystem::u8path(row.file),std::ios::binary);check(bool(input),"explicit header fixture open");std::vector<std::uint8_t> raw((std::istreambuf_iterator<char>(input)),{});check(!input.bad()&&raw.size()==static_cast<std::size_t>(row.fields[0]),"exact fixture header extent");
        MovieAdxHeaderFailure f;auto parsed=parse_movie_adx_header(raw,f);check(parsed&&f.kind==MovieAdxHeaderError::none,"actual real-header parser");
        check(parsed->raw_header==raw&&parsed->data_start==static_cast<std::uint64_t>(row.fields[0])&&parsed->sample_rate==static_cast<std::uint64_t>(row.fields[1])&&parsed->declared_samples==static_cast<std::uint64_t>(row.fields[2])&&parsed->cutoff==row.fields[3],"every real scalar and owned raw header");
        check(parsed->encoding==3&&parsed->block_bytes==18&&parsed->sample_bits==4&&parsed->source_planes==2&&parsed->version==4&&parsed->flags==0,"known actual serialized profile fields");
        check(parsed->histories[0].first==row.fields[4]&&parsed->histories[0].second==row.fields[5]&&parsed->histories[1].first==row.fields[6]&&parsed->histories[1].second==row.fields[7],"all signed final plane histories");
        raw.insert(raw.end(),{0x12,0x34,0x56});auto extra=parse_movie_adx_header(raw,f);check(extra&&extra->raw_header==parsed->raw_header&&extra->data_start==parsed->data_start,"payload excluded from owned header");held.push_back(std::move(*parsed));
    }
    // All temporary source vectors/streams are gone before output inspection.
    for(std::size_t i=0;i<held.size();++i){const auto& h=held[i];std::printf("{\"key\":");json_string(rows[i].key);std::printf(",\"data_start\":%llu,\"encoding\":%u,\"block_bytes\":%u,\"sample_bits\":%u,\"source_planes\":%u,\"sample_rate\":%llu,\"declared_samples\":%llu,\"cutoff\":%u,\"version\":%u,\"flags\":%u,\"histories\":[[%d,%d],[%d,%d]],\"raw_header_hex\":\"",(unsigned long long)h.data_start,unsigned(h.encoding),unsigned(h.block_bytes),unsigned(h.sample_bits),unsigned(h.source_planes),(unsigned long long)h.sample_rate,(unsigned long long)h.declared_samples,unsigned(h.cutoff),unsigned(h.version),unsigned(h.flags),int(h.histories[0].first),int(h.histories[0].second),int(h.histories[1].first),int(h.histories[1].second));for(auto byte:h.raw_header)std::printf("%02x",unsigned(byte));std::puts("\"}");}
    std::puts("PASS native ADX header real29 source fixtures; no coefficient/packet/mode admission");check(!std::fflush(stdout)&&!std::ferror(stdout),"complete real header output");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL ADX header assets %s\n",e.what());return 1;}}
