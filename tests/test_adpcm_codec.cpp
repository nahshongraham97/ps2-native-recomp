#include "mka/adpcm_codec.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
using namespace mka::sound;
void check(bool ok,const char* s) { if(!ok) { std::fprintf(stderr,"FAIL: %s\n",s); std::exit(1); } }
std::uint64_t checksum(const std::vector<std::uint8_t>& bytes) {
    std::uint64_t value=14695981039346656037ull;
    for(auto byte:bytes)value=(value^byte)*1099511628211ull;
    return value;
}
int main(int argc,char** argv) {
    std::array<std::uint8_t,16> b{}; AdpcmHistory h; AdpcmBlock out;
    b[0]=12; b[1]=0xff;
    for(int i=0;i<14;++i)b[i+2]=static_cast<std::uint8_t>(((2*i+1)&15)*16+((2*i)&15));
    check(decode_adpcm_block(b.data(),16,h,out)==AdpcmError::none && out.flags==255,"explicit block and opaque flags");
    for(int i=0;i<28;++i)check(out.samples[i]==((i&15)<8?(i&15):(i&15)-16),"all signed nibbles, low first");
    const int first[]={0,94,139,110,144},second[]={0,88,169,83,181};
    for(int filter=1;filter<=4;++filter) {
        b.fill(0);b[0]=static_cast<std::uint8_t>((filter<<4)|12);h={100,50};
        check(decode_adpcm_block(b.data(),16,h,out)==AdpcmError::none && out.samples[0]==first[filter] && out.samples[1]==second[filter],"hand-derived predictor coefficients and rounding");
    }
    b.fill(0);b[0]=0x1c;h={-1,0};decode_adpcm_block(b.data(),16,h,out);
    check(out.samples[0]==-1,"negative rounding is floor after positive bias");
    b[0]=0x20;h={32767,-32768};decode_adpcm_block(b.data(),16,h,out);
    check(out.samples[0]==32767,"positive saturation");
    h={-32768,32767};decode_adpcm_block(b.data(),16,h,out);check(out.samples[0]==-32768,"negative saturation");
    b.fill(0);b[0]=0x1c;h={100,0};AdpcmBlock firstblock;decode_adpcm_block(b.data(),16,h,firstblock);auto saved=h;
    AdpcmBlock next;decode_adpcm_block(b.data(),16,h,next);check(next.samples[0]==(int(saved.s1)*60+32)/64,"history continues between blocks");
    std::vector<std::uint8_t> joined(b.begin(),b.end());joined.insert(joined.end(),b.begin(),b.end());
    AdpcmHistory whole{100,0};AdpcmError error;auto all=decode_adpcm_mono(joined.data(),joined.size(),whole,error);
    check(all && std::equal(firstblock.samples.begin(),firstblock.samples.end(),all->samples.begin()) && std::equal(next.samples.begin(),next.samples.end(),all->samples.begin()+28) && whole.s1==h.s1,"bulk continuity");
    for(auto bad:{0x50,0x0d}) {
        b[0]=static_cast<std::uint8_t>(bad);h={123,-456};out.samples.fill(77);out.flags=99;
        check(decode_adpcm_block(b.data(),16,h,out)==(bad==0x50?AdpcmError::unsupported_filter:AdpcmError::unsupported_shift) && h.s1==123 && h.s2==-456 && out.samples[0]==77 && out.flags==99,"unsupported block atomic failure");
        joined[16]=static_cast<std::uint8_t>(bad);h={123,-456};
        check(!decode_adpcm_mono(joined.data(),joined.size(),h,error) && h.s1==123 && h.s2==-456,"valid prefix invalid suffix publishes neither PCM nor history");
    }
    check(decode_adpcm_block(b.data(),15,h,out)==AdpcmError::truncated_block && !decode_adpcm_mono(joined.data(),31,h,error),"truncation explicit");
    check(decode_adpcm_block(nullptr,16,h,out)==AdpcmError::null_input,"null input explicit");
    auto empty=decode_adpcm_mono(nullptr,0,h,error);check(empty && empty->samples.empty() && h.s1==123,"empty input no fabricated samples");
    if(argc==3) {
        std::ifstream f(argv[1],std::ios::binary);check(bool(f),"open explicit MS2");f.seekg(0x2219c);
        std::vector<std::uint8_t> raw(0x4d80);f.read(reinterpret_cast<char*>(raw.data()),raw.size());check(f.gcount()==static_cast<std::streamsize>(raw.size()),"exact pinned payload");
        check(checksum(raw)==0x3167477d50802c26ull,"pinned payload byte checksum");
        h={};auto pcm=decode_adpcm_mono(raw.data(),raw.size(),h,error);check(pcm && pcm->samples.size()==34720 && pcm->block_flags.size()==1240,"pinned mono output shape");
        check(h.s1==28672 && h.s2==28672,"pinned final predictor history");
        std::vector<std::uint8_t> serialized;serialized.reserve(pcm->samples.size()*2);
        for(auto value:pcm->samples) { const auto bits=static_cast<std::uint16_t>(value);serialized.push_back(static_cast<std::uint8_t>(bits));serialized.push_back(static_cast<std::uint8_t>(bits>>8)); }
        check(checksum(serialized)==0x036bb89707b3cbbdull,"pinned full decoded PCM checksum");
        std::ofstream target(argv[2],std::ios::binary);target.write(reinterpret_cast<const char*>(serialized.data()),serialized.size());check(bool(target),"write explicit offline PCM artifact");
        std::printf("Pinned mono: %zu samples, %zu flags, history %d/%d\n",pcm->samples.size(),pcm->block_flags.size(),h.s1,h.s2);
    }
    std::puts("adpcm codec tests passed");
}
