#include "mka/font_geometry.hpp"
#include "mka/movie_subtitles.hpp"
#include "native_test_sha256.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
using namespace mka::text;
using Bytes=std::vector<std::uint8_t>;
static void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
static FontGlyphGeometry geom(const FontGlyphPlacement& p,const FontGeometryConfig& c){FontGeometryFailure f;auto q=geometry_font_glyph(p,c,f);if(!q)throw std::runtime_error(font_geometry_error_name(f.kind));return *q;}
static void synthetic(){
 FontGlyphPlacement p{7,'A',10,20,{2.5f,3.5f,.25f,1.25f,.1f,.2f,.3f,.4f,9}};
 FontGeometryConfig c{100,80,100,80,256,128,FontVerticalScale::one};
 auto q=geom(p,c);
 // Literal source steps: baseX=2008; baseY=trunc(2028)=2028.
 // x=trunc(2008.25,2010.75)-1998; y=trunc(2026.75,2030.25)-2008.
 check(q.x0==10&&q.x1==12&&q.y0==18&&q.y1==22,"fractional metric literal-step rectangle");
 check(q.u0_sixteenths==409&&q.u1_sixteenths==819&&q.v0_sixteenths==614&&q.v1_sixteenths==819,"UV float32 multiplication then truncation");
 check(q.source_offset==7&&q.source_character=='A',"placement identity retained");
 p={0,'A',0,-1,{2,2,0,0,0,1,0,1,1}};c={10,10,10,10,256,128,FontVerticalScale::fifteen_sixteenths};
 q=geom(p,c);
 // trunc(-.9375+2048-5)=2042; minus offset2043 gives-1.
 // Cancelling the origin before truncation would incorrectly produce0.
 check(q.y0==-1&&q.y1==0,"negative fractional baseline must retain origin through conversion");
 p.baseline=1;p.metrics.bearing_y=.5f;
 q=geom(p,c);
 // Baseline truncation first gives2043, then trunc(2043-.46875)=2042.
 // One combined truncation of2043.46875 would incorrectly give2043.
 check(q.y0==-1,"baseline conversion precedes bearing addition");
 p={0,'A',9000,6000,{2,3,0,0,0,1,0,1,1}};c={10000,7000,10000,7000,8192,4096,FontVerticalScale::one};
 q=geom(p,c);check(q.x0==9000&&q.x1==9002&&q.y0==6000&&q.y1==6003,"native geometry beyond source coordinate window without wrapping");
 check(q.u1_sixteenths==131072&&q.v1_sixteenths==65536,"UV quantization is not sixteen-bit wrapping");
 c.logical_screen_width=8000;c.logical_screen_height=5000;
 q=geom(p,c);check(q.x0==10000&&q.y0==7000,"logical screen and draw canvas are separate inputs");
 // No hidden allocation/lifetime: returned value does not refer to placement.
 auto retained=q;p.metrics.width=999;check(retained.x1==10002,"geometry value lifetime");
 FontGeometryFailure f;
 const auto rejects=[&](FontGlyphPlacement pp,FontGeometryConfig cc,FontGeometryError expected){auto r=geometry_font_glyph(pp,cc,f);check(!r&&f.kind==expected&&f.source_offset==pp.source_offset,"named geometry rejection");};
 auto bad=c;bad.atlas_width=0;rejects(p,bad,FontGeometryError::invalid_extent);
 bad=c;bad.draw_canvas_height=0;rejects(p,bad,FontGeometryError::invalid_extent);
 bad=c;bad.vertical_scale=static_cast<FontVerticalScale>(99);rejects(p,bad,FontGeometryError::unsupported_scale);
 auto pp=p;pp.source_character=32;rejects(pp,c,FontGeometryError::invalid_placement);
 pp=p;pp.metrics.v1=std::numeric_limits<float>::quiet_NaN();rejects(pp,c,FontGeometryError::nonfinite_metrics);
 pp=p;pp.pen_x=INT64_MAX;rejects(pp,c,FontGeometryError::host_overflow);
 pp=p;pp.baseline=INT64_MAX;rejects(pp,c,FontGeometryError::host_overflow);
 pp=p;pp.metrics.u1=std::numeric_limits<float>::max();rejects(pp,c,FontGeometryError::host_overflow);
 pp=p;pp.metrics.bearing_y=std::numeric_limits<float>::max();rejects(pp,c,FontGeometryError::host_overflow);
 // Finite signed UVs are data. They do not select a wrap/clamp policy.
 pp=p;pp.metrics.u0=-.25f;bad=c;bad.atlas_width=256;
 q=geom(pp,bad);check(q.u0_sixteenths==-1024,"negative UV retained without unsigned transport reinterpretation");
}
static std::string sha(const Bytes& b){NativeTestSha256 h;h.add(b.data(),b.size());return h.finish();}
static Bytes read(std::ifstream& f,std::uint64_t at,std::size_t n){Bytes b(n);f.seekg(static_cast<std::streamoff>(at));f.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(n));check(std::size_t(f.gcount())==n,"explicit source range");return b;}
static void real(const char* metrics_path,const char* title){
 std::ifstream mf(metrics_path,std::ios::binary);check(bool(mf),"open metrics entry");Bytes raw((std::istreambuf_iterator<char>(mf)),{});
 check(sha(raw)=="b91b6ee00f3bed46a1e89471c247ea659b88af927d7bd9576311b57fd0cfd497","actual metrics SHA");
 FontMetricsFailure me;auto metrics=NativeFontMetrics::parse(std::move(raw),me);check(bool(metrics),"actual metrics parse");
 std::ifstream f(title,std::ios::binary);check(bool(f),"open title fixture");auto tree=read(f,0x1812,2030);auto continuation=read(f,0x2012,359);tree.insert(tree.end(),continuation.begin(),continuation.end());
 check(sha(tree)=="bbe9842b4fdb0763b9393467376d54ec83fc199e52b8251566a2a60d229c4147","actual CRITAGS root SHA");
 mka::video::MovieSubtitleFailure se;auto document=mka::video::NativeMovieSubtitles::parse(std::move(tree),se);check(bool(document),"subtitle root parse");
 auto cursor=document->cursor(0,0,se);check(bool(cursor),"explicit subtitle cursor");
 std::vector<FontGlyphGeometry> retained;
 for(unsigned event=0;event<7;++event){
  // Deliberately supplied offline test clock, not an actual movie clock policy.
  auto query=document->query(*cursor,{7000+3000*event,1000},se);check(query&&query->active,"actual event query");
  auto caption=mka::video::extract_movie_subtitle_language(*query,mka::video::MovieSubtitleLanguage::english,se);check(bool(caption),"actual English extraction");
  check(sha(*caption)=="c06c6ba74476448119b8b1c25bbb776d7be2a9b9806819a8fb8bd20837ff7bd2","actual caption identity");
  FontLayoutFailure le;auto layout=layout_font_bytes(*metrics,*caption,{144,48,320,410,18},le);check(layout&&layout->glyphs.size()==16,"actual owned source layout");
  FontGeometryConfig c{640,448,640,448,256,128,FontVerticalScale::one};
  auto first=geom(layout->glyphs[0],c);
  check(first.source_character=='P'&&first.x0==184&&first.x1==199&&first.y0==430&&first.y1==444,"actual P source-derived one-scale rectangle");
  check(first.u0_sixteenths==720&&first.u1_sixteenths==960&&first.v0_sixteenths==1056&&first.v1_sixteenths==1280,"actual P source UV quantization");
  c.vertical_scale=FontVerticalScale::fifteen_sixteenths;first=geom(layout->glyphs[0],c);
  check(first.x0==184&&first.x1==199&&first.y0==403&&first.y1==416,"actual P source-derived scaled rectangle");
  c.vertical_scale=FontVerticalScale::one;c.draw_canvas_width=1000;c.draw_canvas_height=700;first=geom(layout->glyphs[0],c);
  check(first.x0==364&&first.y0==556&&first.x1==379&&first.y1==570,"actual P differing explicit canvas");
  for(const auto& placement:layout->glyphs)retained.push_back(geom(placement,c));
 }
 document.reset();cursor.reset();metrics.reset();check(retained.size()==112&&retained[0].x0==364,"geometry survives subtitle/layout/font owners");
}
int main(int argc,char** argv){try{synthetic();if(argc==3)real(argv[1],argv[2]);else check(argc==1,"usage: test_font_geometry [metrics-entry TITLEN.SFD]");std::cout<<"PASS native font geometry data; no coverage/sampling/composition\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
