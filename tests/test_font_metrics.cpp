#include "mka/font_metrics.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
using namespace mka::text;
namespace {
using Bytes = std::vector<std::uint8_t>;
void check(bool b, const char* message) { if (!b) { std::fprintf(stderr,"font-metrics FAIL: %s\n",message); std::exit(1); } }
void set(Bytes& b, std::size_t at, float value) {
    const auto w = std::bit_cast<std::uint32_t>(value);
    for (unsigned i=0;i<4;++i) b[at+i] = std::uint8_t(w>>(i*8));
}
float original(const Bytes& b, std::size_t at) {
    static_assert(std::endian::native == std::endian::little);
    float value; std::memcpy(&value,b.data()+at,4); return value;
}
std::array<float,9> fields(const FontGlyphMetrics& g) {
    return {g.width,g.height,g.bearing_x,g.bearing_y,g.u0,g.u1,g.v0,g.v1,g.advance};
}
std::string sha(const Bytes& b) {
    BCRYPT_ALG_HANDLE alg=nullptr; BCRYPT_HASH_HANDLE hash=nullptr;
    check(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)==0,"SHA provider");
    check(BCryptCreateHash(alg,&hash,nullptr,0,nullptr,0,0)==0,"SHA create");
    check(BCryptHashData(hash,const_cast<PUCHAR>(b.data()),ULONG(b.size()),0)==0,"SHA update");
    std::array<unsigned char,32> out{}; check(BCryptFinishHash(hash,out.data(),ULONG(out.size()),0)==0,"SHA finish");
    BCryptDestroyHash(hash); BCryptCloseAlgorithmProvider(alg,0);
    std::string result; for(auto c:out) { char s[3]; std::snprintf(s,3,"%02x",c); result+=s; } return result;
}
NativeFontMetrics parse(Bytes b) { FontMetricsFailure f; auto m=NativeFontMetrics::parse(std::move(b),f); check(bool(m)&&f.kind==FontMetricsError::none,"parse succeeds"); return std::move(*m); }
void reject(Bytes b,FontMetricsError expected,std::size_t offset) {
    FontMetricsFailure f;auto m=NativeFontMetrics::parse(std::move(b),f);
    check(!m&&f.kind==expected&&f.offset==offset,"exact malformed error and offset");
}
void synthetic() {
    Bytes raw(52+36*2,0);
    // Non-NUL opaque labels and negative/finite metrics are preserved.
    std::fill_n(raw.begin(),32,0xff); set(raw,48,6);
    for(unsigned i=0;i<9;++i) set(raw,52+i*4,float(i)-4.0f);
    set(raw,52+32,7); set(raw,52+36+32,9);
    auto m=parse(raw); check(m.glyph_count()==2&&m.label_bytes()[31]==255,"raw label and dynamic count");
    check(std::equal(m.raw_bytes().begin(),m.raw_bytes().end(),raw.begin()),"all raw bytes retained");
    FontMetricsFailure f; check(m.advance(32,f)==6&&m.glyph(32,f)->advance==7&&m.advance(33,f)==9,"space header advance differs from glyph0");
    check(!m.glyph(31,f)&&f.kind==FontMetricsError::missing_glyph,"control glyph rejected");
    check(!m.glyph(34,f)&&f.kind==FontMetricsError::missing_glyph,"absent glyph rejected");
    check(!m.glyph(UINT64_MAX,f)&&f.kind==FontMetricsError::missing_glyph,"large native index checked without wrap");
    auto retained=m; auto moved=std::move(m);
    check(m.raw_bytes().empty()&&!m.glyph(32,f)&&f.kind==FontMetricsError::moved_metrics,"moved owner rejects");
    check(moved.glyph_count()==2&&retained.advance(32,f)==6,"copy/move lifetime");
    for(std::size_t n=0;n<52;++n) reject(Bytes(n,0),FontMetricsError::truncated_header,n);
    reject(Bytes(53,0),FontMetricsError::partial_glyph_record,52);
    auto bad=raw;set(bad,36,std::numeric_limits<float>::infinity());reject(bad,FontMetricsError::nonfinite_value,36);
    bad=raw;set(bad,52+36+20,std::numeric_limits<float>::quiet_NaN());reject(bad,FontMetricsError::nonfinite_value,108);
    bad=raw;set(bad,52,-std::numeric_limits<float>::infinity());reject(bad,FontMetricsError::nonfinite_value,52);
    auto empty=parse(Bytes(52,0));check(empty.glyph_count()==0&&!empty.glyph(32,f)&&f.kind==FontMetricsError::missing_glyph,"header-only explicit absent glyph");
    // Native storage is not limited to the retail byte-character glyph count.
    Bytes large(52+36*65536,0);set(large,52+36*65535+32,17);
    auto host=parse(std::move(large));check(host.glyph_count()==65536&&host.glyph(65567,f)->advance==17,"native dynamic storage and index");
}
void real(const std::filesystem::path& pak) {
    // Test-only independently pinned member/range. Production reader receives
    // explicit owned metrics bytes, and performs no archive/font selection.
    std::ifstream in(pak,std::ios::binary);check(bool(in),"open exact retail PAK fixture");
    in.seekg(std::streamoff(291459072ULL+0x8b50));Bytes raw(8116);
    in.read(reinterpret_cast<char*>(raw.data()),std::streamsize(raw.size()));check(in.gcount()==std::streamsize(raw.size()),"exact metric byte range");in.close();
    const auto hash=sha(raw);check(hash=="b91b6ee00f3bed46a1e89471c247ea659b88af927d7bd9576311b57fd0cfd497","pinned raw metrics identity");
    auto m=parse(raw);check(m.glyph_count()==224,"actual glyph count");
    FontMetricsFailure f;
    for(std::uint64_t c=32;c<256;++c) {
        auto g=m.glyph(c,f);check(bool(g),"actual glyph present");const auto v=fields(*g);
        for(std::size_t k=0;k<v.size();++k)check(std::bit_cast<std::uint32_t>(v[k])==std::bit_cast<std::uint32_t>(original(raw,52+std::size_t(c-32)*36+k*4)),"all2016 source float bits");
    }
    const auto a=fields(*m.glyph('A',f));const std::array<float,9> expected={15,14,0,16,0,0.05859375f,0.515625f,0.625f,16};
    check(a==expected,"source A field order and values");check(m.advance(32,f)==6&&m.glyph(32,f)->advance==7,"actual space special case");
    for(float v:fields(*m.glyph(255,f)))check(v==0,"source final empty glyph retained");
    check(!m.glyph(256,f)&&f.kind==FontMetricsError::missing_glyph,"actual absent glyph does not substitute");
    std::printf("font-metrics actual glyphs=%zu float_fields=2016 sha256=%s space=6 glyph0_advance=7\n",m.glyph_count(),hash.c_str());
}
}
int main(int argc,char** argv) {
    if(argc!=2){std::fprintf(stderr,"usage: test_native_font_metrics <MKDA.PAK>\n");return 2;}
    synthetic();real(std::filesystem::u8path(argv[1]));
    std::puts("PASS owned native metrics, exact retail fields, malformed/lifetime/dynamic-capacity checks; no atlas/layout/font-selection/render/game acceptance");
}
