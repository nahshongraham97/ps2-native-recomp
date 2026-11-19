#include "mka/font_layout.hpp"
#include "mka/movie_subtitles.hpp"
#include "native_test_sha256.hpp"
#include <array>
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
using namespace mka::text;
using Bytes=std::vector<std::uint8_t>;
static void check(bool b,const char* d){if(!b)throw std::runtime_error(d);}
static void put(Bytes& b,std::size_t p,float v){auto w=std::bit_cast<std::uint32_t>(v);for(unsigned i=0;i<4;++i)b[p+i]=std::uint8_t(w>>(8*i));}
static NativeFontMetrics parsed(Bytes b){FontMetricsFailure f;auto m=NativeFontMetrics::parse(std::move(b),f);check(bool(m),"metrics parse");return std::move(*m);}
static NativeFontMetrics metrics(float a=3,float bb=4,float c=5,float space=2){
 Bytes b(52+224*36);put(b,48,space);
 for(unsigned i=0;i<224;++i){for(unsigned j=0;j<9;++j)put(b,52+36*i+4*j,float(j+1));put(b,52+36*i+32,1);}
 put(b,52+('A'-32)*36+32,a);put(b,52+('B'-32)*36+32,bb);put(b,52+('C'-32)*36+32,c);return parsed(std::move(b));
}
static std::span<const std::uint8_t> text(std::string_view s){return {reinterpret_cast<const std::uint8_t*>(s.data()),s.size()};}
static FontLayout layout(const NativeFontMetrics& m,std::string_view s,FontLayoutConfig c){
 FontLayoutFailure f;auto r=layout_font_bytes(m,text(s),c,f);
 if(!r)throw std::runtime_error(std::string(font_layout_error_name(f.kind))+" at "+std::to_string(f.source_offset));return std::move(*r);
}
static void synthetic(){
 auto m=metrics();FontLayoutConfig c{144,48,10,100,18};
 auto r=layout(m,"AB",c);check(r.lines.size()==1&&r.lines[0].selected_width==7&&r.glyphs.size()==2,"end-before-scan-budget retains current line");
 check(r.glyphs[0].pen_x==7&&r.glyphs[1].pen_x==10&&r.glyphs[1].baseline==100,"ordinary pen placement");
 check(r.glyphs[0].metrics.width==1&&r.glyphs[0].metrics.v1==8,"raw metrics copied");
 c.per_line_scan_budget=4;r=layout(m,"A A BC",c);
 check(r.lines.size()==2&&r.lines[0].selected_bytes==3&&r.lines[0].scanned_bytes==4&&r.lines[0].selected_width==8,"full scan selects last space");
 check(r.lines[1].source_offset==3&&r.lines[1].selected_bytes==3&&r.lines[1].selected_width==11,"break space not skipped");
 check(r.glyphs[2].source_offset==4&&r.glyphs[2].pen_x==7&&r.glyphs[2].baseline==118,"leading break space advances next line pen");
 c.per_line_scan_budget=48;r=layout(m,"\\n\\nAB",c);
 check(r.lines.size()==3&&r.lines[0].selected_bytes==0&&r.lines[1].selected_bytes==0&&r.glyphs[0].baseline==136,"literal newlines produce empty lines");
 check(r.consumed_bytes==6&&r.lines[2].source_offset==4,"literal newlines consume two source bytes");
 r=layout(m,"\\n\\n\\n\\nAB",c);
 check(r.lines.size()==5&&r.glyphs[0].baseline==172,"total byte budget is not an implicit three-line cap");
 auto carry=c;carry.per_line_scan_budget=4;r=layout(m,"A A\\nBBBBBBBBB",carry);
 check(r.lines.size()==4&&r.lines[1].selected_bytes==3&&r.lines[2].selected_bytes==3&&r.lines[1].selected_width==0&&r.lines[2].selected_width==0,"prior break count persists through full unbroken later scans");
 check(r.lines[1].pen_x==10&&r.lines[3].selected_width==12&&r.lines[3].pen_x==4&&r.consumed_bytes==14,"literal fallback differs from modern wrapping and final partial scan");
 auto fractional=metrics(1.75f,-2.75f,.75f,2.25f);
 r=layout(fractional,"AB",c);check(r.lines[0].selected_width==-1&&r.glyphs[0].pen_x==10&&r.glyphs[1].pen_x==11,"negative half rounds toward zero and each advance truncates");
 c.center=-10;r=layout(fractional,"CC",c);
 check(r.lines[0].selected_width==0&&r.glyphs[0].pen_x==-10&&r.glyphs[1].pen_x==-9,"float32 step truncation uses actual negative pen");
 c={144,48,0,0,1};r=layout(m,std::string(47,'A'),c);check(r.consumed_bytes==47&&r.lines[0].selected_bytes==47,"47-byte unbroken end succeeds");
 FontLayoutFailure f;
 const auto rejects=[&](const NativeFontMetrics& fm,std::string_view s,FontLayoutConfig cfg,FontLayoutError expected,std::size_t at){auto x=layout_font_bytes(fm,text(s),cfg,f);check(!x&&f.kind==expected&&f.source_offset==at,"named transactional failure");};
 rejects(m,std::string(48,'A'),c,FontLayoutError::nonprogress,0);
 rejects(m,"A\nB",c,FontLayoutError::unsupported_control,1);
 rejects(m,"A\tB",c,FontLayoutError::unsupported_control,1);
 // A valid selected first line does not publish a partial result on later failure.
 rejects(m,"A\\nB\t",c,FontLayoutError::unsupported_control,4);
 auto empty=parsed(Bytes(52));rejects(empty,"A",c,FontLayoutError::missing_glyph,0);
 auto moved=std::move(empty);rejects(empty,"A",c,FontLayoutError::invalid_metrics,0);
 auto huge=metrics(std::numeric_limits<float>::max());rejects(huge,"A",c,FontLayoutError::host_overflow,0);
 auto overflow=c;overflow.center=INT64_MAX;rejects(fractional,"BB",overflow,FontLayoutError::host_overflow,0);
 overflow=c;overflow.baseline=INT64_MAX;rejects(m,"A",overflow,FontLayoutError::host_overflow,1);
 auto zero=c;zero.per_line_scan_budget=0;rejects(m,"A",zero,FontLayoutError::nonprogress,0);
 zero=c;zero.total_byte_budget=0;r=layout(m,"A",zero);check(r.lines.empty()&&r.consumed_bytes==0,"zero total budget returns empty");
 c.total_byte_budget=1;r=layout(m,"AB",c);check(r.consumed_bytes==1&&r.glyphs.size()==1,"current line ends at total budget");
 auto retained=r;auto moved_result=std::move(r);check(retained.glyphs[0].metrics.advance==3&&moved_result.glyphs[0].pen_x==-1,"owned copied metrics/placements");
}
static std::string sha(const Bytes& b){NativeTestSha256 h;h.add(b.data(),b.size());return h.finish();}
static Bytes read(std::ifstream& f,std::uint64_t at,std::size_t n){Bytes b(n);f.seekg(static_cast<std::streamoff>(at));f.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(n));check(std::size_t(f.gcount())==n,"read explicit fixture range");return b;}
static void real(const char* metrics_entry,const char* title){
 std::ifstream mf(metrics_entry,std::ios::binary);check(bool(mf),"open explicit metrics entry");Bytes raw((std::istreambuf_iterator<char>(mf)),{});
 check(sha(raw)=="b91b6ee00f3bed46a1e89471c247ea659b88af927d7bd9576311b57fd0cfd497","actual metrics SHA");auto m=parsed(std::move(raw));
 std::ifstream f(title,std::ios::binary);check(bool(f),"open explicit TITLEN fixture");
 // Test-only independently pinned metadata ranges, not production discovery.
 auto tree=read(f,0x1812,2030);auto tail=read(f,0x2012,359);tree.insert(tree.end(),tail.begin(),tail.end());
 check(sha(tree)=="bbe9842b4fdb0763b9393467376d54ec83fc199e52b8251566a2a60d229c4147","actual title CRITAGS SHA");
 mka::video::MovieSubtitleFailure subtitle_error;
 auto document=mka::video::NativeMovieSubtitles::parse(std::move(tree),subtitle_error);
 check(bool(document),"actual owned subtitle tree parses");
 auto cursor=document->cursor(0,0,subtitle_error);check(bool(cursor),"explicit test channel/offset");
 std::vector<FontLayout> retained;
 const std::array<std::int64_t,16> pens={184,200,217,231,248,271,288,303,319,336,357,374,391,406,421,438};
 const std::array languages={mka::video::MovieSubtitleLanguage::english,
  mka::video::MovieSubtitleLanguage::french,mka::video::MovieSubtitleLanguage::german,
  mka::video::MovieSubtitleLanguage::spanish,mka::video::MovieSubtitleLanguage::italian};
 for(unsigned event=0;event<7;++event){
  // Explicit synthetic clock: no actual player/PTS/master-clock selection.
  auto query=document->query(*cursor,{7000+3000*event,1000},subtitle_error);
  check(query&&query->active,"actual event selected through native query");
  for(auto language:languages){
   auto caption=mka::video::extract_movie_subtitle_language(*query,language,subtitle_error);
   check(bool(caption),"actual source language extraction");
   FontLayoutFailure fail;auto out=layout_font_bytes(m,*caption,{144,48,320,410,18},fail);
   check(out&&out->lines.size()==3&&!out->glyphs.empty()&&out->consumed_bytes==caption->size(),"all35 actual language bodies reach complete owned layout");
   if(language==mka::video::MovieSubtitleLanguage::english){
    check(sha(*caption)=="c06c6ba74476448119b8b1c25bbb776d7be2a9b9806819a8fb8bd20837ff7bd2","pinned actual ENG caption body");
    check(out->glyphs.size()==16&&out->lines[2].selected_width==272,"actual ENG literal-newline/width source expectations");
    for(unsigned i=0;i<pens.size();++i)check(out->glyphs[i].pen_x==pens[i]&&out->glyphs[i].baseline==446,"actual ENG logical pen golden");
   }
   retained.push_back(std::move(*out));
  }
 }
 document.reset();cursor.reset();m=metrics();
 check(retained.size()==35,"all actual language placements retained after original owners retire");
 for(unsigned i=0;i<7;++i)check(retained[i*5].glyphs[0].pen_x==184&&retained[i*5].glyphs[0].baseline==446&&retained[i*5].glyphs[0].metrics.advance==16,"owned original glyph metrics and positions survive document/cursor/font teardown");
 std::cout<<"actual title layout events=7 languages=5 retained=35 english_glyphs=16 width=272 baseline=446\n";
}
int main(int argc,char** argv){try{synthetic();if(argc==3)real(argv[1],argv[2]);else check(argc==1,"usage: test_font_layout [metrics-entry TITLEN.SFD]");std::cout<<"PASS native logical font layout; no screen/region/composition\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
