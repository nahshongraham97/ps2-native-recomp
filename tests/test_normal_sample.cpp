#include "mka/normal_sample.hpp"
#include <cstdio>
#include <algorithm>
#include <iterator>
#include <cstdlib>
#include <fstream>
using namespace mka::sound;
namespace {
void check(bool v,const char* s){if(!v){std::fprintf(stderr,"FAIL: %s\n",s);std::exit(1);}}
void put(std::vector<std::uint8_t>& b,std::size_t a,std::uint32_t v){for(unsigned i=0;i<4;++i)b[a+i]=v>>(8*i);}
void save(const std::filesystem::path& p,const std::uint8_t* b,std::size_t n){std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(b),n);check(bool(f),"write owned diagnostics");}
SamplePayload fixture(const std::filesystem::path& dir,const char* name,std::uint32_t plane,std::uint32_t window=65536,unsigned count=2,bool invalidB=false){
 std::vector<std::uint8_t>b(256+20+std::size_t(plane)*count);put(b,260,window);put(b,264,count);put(b,268,22050);put(b,272,plane);
 for(std::size_t a=276;a+16<=b.size();a+=16){b[a]=12;b[a+1]=7;for(unsigned i=2;i<16;++i)b[a+i]=0x11;}
 if(invalidB)b[276+16]=0x50;
 auto p=dir/name;save(p,b.data(),b.size());ResourceLookup r;r.status=ResourceLookupStatus::found;r.key_mode=ResourceKeyMode::numeric;
 std::vector<std::uint8_t>raw(36);put(raw,8,0x20000002);put(raw,12,22050);put(raw,16,256);put(raw,20,20+plane*count);std::copy(raw.begin(),raw.end(),r.bytes.begin());
 PayloadFailure f;auto payload=SamplePayload::read(p,r,f);check(bool(payload),"read synthetic valid payload");return std::move(*payload);
}
const std::uint32_t resources[][9]={
    {0x0,0x0,0x200000c6,0x5622,0x5dbff4,0x10434,0x2090,0x2060,0x0},
    {0x0,0x0,0x20000625,0x5622,0x1877110,0x9214,0x2480,0x0,0x0},
    {0x0,0x0,0x20000102,0x5622,0x6e44a8,0xaa14,0x2a80,0x0,0x0},
    {0x0,0x0,0x2000012a,0x5622,0x78cb94,0x13fd4,0x2800,0x27e0,0x0},
    {0x0,0x0,0x2000012b,0x5622,0x7a0b68,0x145f4,0x28c0,0x28b0,0x0},
    {0x0,0x0,0x2000012c,0x5622,0x7b515c,0x14854,0x2910,0x28f0,0x0},
    {0x0,0x0,0x2000012d,0x5622,0x7c99b0,0x15854,0x2b10,0x2af0,0x0},
    {0x0,0x0,0x20000156,0x5622,0x878300,0xaa14,0x2a80,0x0,0x0},
    {0x0,0x0,0x20000162,0x5622,0x8aa360,0xf2f4,0x3cc0,0x3cb0,0x0},
    {0x0,0x0,0x20000187,0x5622,0x9433f4,0xa854,0x2a10,0x0,0x0},
    {0x0,0x0,0x200001c0,0x5622,0xab14e4,0xaa14,0x2a80,0x0,0x0},
    {0x0,0x0,0x20000602,0x7d00,0x1716328,0x65b4,0x1970,0x1960,0x0},
    {0x0,0x0,0x20000625,0x5622,0x1877110,0x9214,0x2480,0x0,0x0},
    {0x0,0x0,0x2000188e,0x7d00,0x416b568,0x181f4,0x3040,0x3030,0x0},
    {0x0,0x0,0x200018c2,0x5622,0x41c1e30,0x10c54,0x2190,0x2170,0x0},
    {0x0,0x0,0x200018c5,0x7d00,0x41d4788,0x14434,0x2890,0x2860,0x0},
    {0x0,0x0,0x200018c6,0x7d00,0x41e8bbc,0xe034,0x3810,0x3800,0x0},
    {0x0,0x0,0x200018cc,0x5622,0x4205340,0x9cf4,0x2740,0x2730,0x0},
    {0x0,0x0,0x200018f3,0x7d00,0x4293c60,0xe334,0x38d0,0x38c0,0x0},
    {0x0,0x0,0x200018fd,0x5622,0x42c4d28,0x15e14,0x2bc0,0x0,0x0},
    {0x0,0x0,0x2000195c,0x5622,0x443b97c,0x1d2b4,0x3a60,0x3a30,0x0},
    {0x0,0x0,0x20001962,0x5622,0x446aac0,0xd854,0x3610,0x0,0x0},
    {0x0,0x0,0x20001963,0x5622,0x4478314,0xc614,0x3180,0x0,0x0},
    {0x0,0x0,0x20001966,0x5622,0x448ca1c,0x9cf4,0x2740,0x2730,0x0},
    {0x0,0x0,0x2000195c,0x5622,0x443b97c,0x1d2b4,0x3a60,0x3a30,0x0},
    {0x0,0x0,0x2000195c,0x5622,0x443b97c,0x1d2b4,0x3a60,0x3a30,0x0},
    {0x0,0x0,0x2000195c,0x5622,0x443b97c,0x1d2b4,0x3a60,0x3a30,0x0},
    {0x0,0x0,0x20001a50,0x5622,0x49b213c,0x9cf4,0x2740,0x2730,0x0},
    {0x0,0x0,0x20001a9e,0x7d00,0x4a9b270,0x181f4,0x3040,0x3030,0x0},
    {0x0,0x0,0x2000188e,0x7d00,0x416b568,0x181f4,0x3040,0x3030,0x0},
    {0x0,0x0,0x20001fe6,0x5622,0x59b7be4,0x6274,0x18a0,0x1890,0x0},
    {0x0,0x0,0x20002056,0x7d00,0x5b8a8d8,0xf9f74,0x3e80,0x3e30,0x0},
    {0x0,0x0,0x20002074,0x5622,0x5cedc0c,0xc6f4,0x31c0,0x31b0,0x0},
    {0x0,0x0,0x20002075,0x5622,0x5cfa300,0xc594,0x3160,0x0,0x0},
    {0x0,0x0,0x20002076,0x5622,0x5d06894,0xd654,0x3590,0x0,0x0},
    {0x0,0x0,0x20002084,0x7d00,0x5d30770,0x16434,0x2c90,0x2c60,0x0},
    {0x0,0x0,0x20002085,0x7d00,0x5d46ba4,0x16554,0x2cb0,0x2c90,0x0},
    {0x0,0x0,0x20002086,0x7d00,0x5d5d0f8,0x2cd34,0x3bd0,0x3b80,0x0},
    {0x0,0x0,0x20002087,0x7d00,0x5d89e2c,0x16894,0x2d10,0x0,0x0},
    {0x0,0x0,0x20002088,0x5622,0x5da06c0,0x13c54,0x2790,0x2770,0x0},
};
}
int main(int argc,char** argv){
 if(argc!=3){std::fprintf(stderr,"usage: test_normal_sample <pinnedMS2> <NEW diagnostics directory>\n");return 2;}
 const std::filesystem::path out(argv[2]);check(std::filesystem::create_directory(out),"exclusive new diagnostics directory");
 NormalSampleFailure failure;NormalCount2Head head;
 auto short_sample=fixture(out,"short.raw",64);auto old=head;
 check(!convert_normal_count2(short_sample,head,failure)&&failure.kind==NormalSampleError::unknown_initial_head&&!head.known,"unknownshorthead never zero-filled");
 head.known=true;head.bytes.fill(0x22);head.bytes[0]=12;head.bytes[1]=3;auto encoded=convert_normal_count2(short_sample,head,failure);
 check(encoded&&encoded->planes[0].size()==64&&std::equal(head.bytes.begin(),head.bytes.end(),encoded->planes[1].begin())&&encoded->planes[1][16]==12&&encoded->planes[1][17]==7,"explicithead and shiftedoddblock preserved");
 auto wrong=fixture(out,"wrong-window.raw",64,32768);check(!convert_normal_count2(wrong,head,failure)&&failure.kind==NormalSampleError::unsupported_window_encoding,"unprovedwindow is unsupportedformat");
 auto mono=fixture(out,"mono.raw",64,65536,1);check(!convert_normal_count2(mono,head,failure)&&failure.kind==NormalSampleError::unsupported_count,"wrongcount explicit");
 auto odd=fixture(out,"odd.raw",65);check(!convert_normal_count2(odd,head,failure)&&failure.kind==NormalSampleError::misaligned_plane_size,"partialencodedblock explicit");
 auto large=fixture(out,"large.raw",(2u<<20)+16);NormalCount2Head large_head;auto big=convert_normal_count2(large,large_head,failure);check(big&&big->planes[0].size()==(2u<<20)+16&&big->planes[1].size()==(2u<<20)+16,"wholehostplanes exceedolddeviceRAM");
 auto bad=fixture(out,"bad-second.raw",64,65536,2,true);std::array<AdpcmHistory,2> history{{{123,-456},{789,-321}}};auto before_history=history;auto before_head=head;
 check(!decode_normal_count2(bad,head,history,failure)&&failure.kind==NormalSampleError::codec_failed&&failure.codec==AdpcmError::unsupported_filter&&
 history[0].s1==before_history[0].s1&&history[0].s2==before_history[0].s2&&history[1].s1==before_history[1].s1&&history[1].s2==before_history[1].s2&&head.bytes==before_head.bytes&&head.known==before_head.known,"secondplane failure aftervalidprefix leavesallstate unchanged");
 mka::content::NativeFile file;std::error_code ec;check(file.open(argv[1],ec),"open exactMS2");std::ofstream report(out/"results.tsv");report<<"case\tplane\tencoded_bytes\tpcm_samples\thistory1\thistory2\n";
 std::size_t chunks=0;
 for(std::size_t n=0;n<std::size(resources);++n){ResourceLookup resource;resource.status=ResourceLookupStatus::found;resource.key_mode=ResourceKeyMode::numeric;
  for(unsigned w=0;w<9;++w)for(unsigned k=0;k<4;++k)resource.bytes[w*4+k]=resources[n][w]>>(k*8);
  PayloadFailure pf;auto payload=SamplePayload::read(file,resource,pf);check(bool(payload),"exactrealcount2payload");NormalCount2Head h;std::array<AdpcmHistory,2>hist{};
  auto decoded=decode_normal_count2(*payload,h,hist,failure);check(bool(decoded),"allrealcount2 optimizedconversion+independentmono decode");chunks+=(payload->bytes_per_plane()+16383)/16384;
  for(unsigned plane=0;plane<2;++plane){const auto stem=std::to_string(n)+"-"+std::to_string(plane);const auto& raw=decoded->encoded.planes[plane];const auto& pcm=decoded->pcm[plane];
   check(raw.size()==payload->bytes_per_plane()&&pcm.samples.size()==raw.size()/16*28&&pcm.block_flags.size()==raw.size()/16,"fulllengthsamples/flags");
   save(out/(stem+".encoded"),raw.data(),raw.size());save(out/(stem+".flags"),pcm.block_flags.data(),pcm.block_flags.size());std::vector<std::uint8_t>bytes;bytes.reserve(pcm.samples.size()*2);
   for(auto s:pcm.samples){auto u=static_cast<std::uint16_t>(s);bytes.push_back(u&255);bytes.push_back(u>>8);}save(out/(stem+".s16le"),bytes.data(),bytes.size());
   report<<n<<'\t'<<plane<<'\t'<<raw.size()<<'\t'<<pcm.samples.size()<<'\t'<<hist[plane].s1<<'\t'<<hist[plane].s2<<'\n';
  }
 }
 check(chunks==138,"all40resources138softwarechunks");check(bool(report),"writefullresulttable");std::printf("PASS40realresources/%zu chunks plusownedlargeplane/unsupported/transactional tests; noaudibleplaybackclaim\n",chunks);
}
