// Actual GL resource/consumer test only. No game boot/device reset/reference
// rasterizer. One GS integration case seeds/checks CPU sentinels to prove bypass.
#include "gs/native_gl_backend.hpp"
#include "gs/gif.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>
#endif
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
constexpr std::uint32_t base=12352; // Opaque packet resource identity; not a host address.
ps2::gs::State state(std::uint32_t image=base,std::uint32_t frame=0) {
    ps2::gs::State s{};s.prim=6u|(1u<<4)|(1u<<8);s.prmodecont=1;s.colclamp=1;
    s.ctx[0].frame=frame|(1ull<<16);s.ctx[0].zbuf=1ull<<32;
    s.ctx[0].scissor=(1023ull<<16)|(511ull<<48);
    s.ctx[0].tex0=image|(1ull<<14)|(1ull<<26)|(1ull<<30)|(1ull<<34)|(1ull<<35);
    s.ctx[0].clamp=5;return s; // Exact CT32, 2x2, DECAL/TCC, clamp-to-edge.
}
std::array<ps2::gs::Vertex,2> vertices() {
    std::array<ps2::gs::Vertex,2> v{};
    for(auto& p:v){p.r=p.g=p.b=p.a=128;p.q=1;}
    v[0].x=v[0].y=8*16;v[1].x=v[1].y=10*16;
    v[1].u=v[1].v=2*16;return v;
}
#ifdef _WIN32
using BindFramebuffer=void(APIENTRY*)(GLenum,GLuint);
ps2::gs::DisplayRegs display_for(std::uint32_t image,unsigned width,unsigned height) {
    ps2::gs::DisplayRegs d{};d.pmode=1;d.dispfb[0]=(image/32u)|(1ull<<9);
    d.display[0]=(std::uint64_t(width-1)<<32)|(std::uint64_t(height-1)<<44);return d;
}
std::vector<std::uint32_t> capture(std::uint32_t image,unsigned width,unsigned height) {
    std::vector<std::uint32_t> pixels;std::uint32_t w=0,h=0;
    check(ps2::gs::native_gl::read_picture(display_for(image,width,height),pixels,w,h),"capture exact raw copy destination");
    check(w==width && h==height,"copy capture shape");return pixels;
}
void pixel(int x,int y,std::array<unsigned char,4> expected) {
    std::array<unsigned char,4> actual{};
    glReadPixels(x,y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,actual.data());
    if(actual!=expected){std::fprintf(stderr,"pixel %d,%d actual=%u,%u,%u,%u expected=%u,%u,%u,%u\n",x,y,
        actual[0],actual[1],actual[2],actual[3],expected[0],expected[1],expected[2],expected[3]);throw std::runtime_error("actual GPU sampled pixel mismatch");}
    check(glGetError()==GL_NO_ERROR,"GL pixel read has no error");
}
int retail_final(const char* path){
    using namespace ps2::gs;
    // Captured bytes, not an authored reconstruction. External runner pins
    // stateSHA; every chain byte is checked here against the captured320 bytes.
    static constexpr std::uint64_t words[40]={0x70000013,0,0x1000000000008012,0xe,
        0xa0d0000,0x4c,0,0x18,0x7fff00007fff0000,0x40,0x30000,0x47,
        0x10a0000e7,0x4e,1,0x45,0x6fc009fc00a,8,0xaa8028b60,6,0x61,0x14,
        0x16,0,0x3f80000000000000,1,0x3a00000039c00000,2,0,5,
        0x3ee040003f201800,2,0x1c003400,5,0x720000006c00,0x18,0xa005b,0x4c,0xa0000e7,0x4e};
    std::ifstream file(path,std::ios::binary);check(bool(file),"explicit preserved state opens read-only");
    std::array<char,20> header{};file.read(header.data(),header.size());
    check(std::string(header.data(),8)=="PS2STATE"&&static_cast<unsigned char>(header[8])==8&&header[9]==0&&header[10]==0&&header[11]==0,
        "pinned original version8 state layout");
    check(std::string(header.data()+12,2)=="ee","saved EE tag before original1456-byte Cpu");
    std::array<std::uint8_t,320> chain{};
    for(unsigned address:{0x6a3fc0u,0x6a4100u,0x6a4240u,0x6a4380u}){
        file.seekg(1476ull+address);file.read(reinterpret_cast<char*>(chain.data()),chain.size());check(bool(file),"read full original constructed chain");
        check(std::memcmp(chain.data(),words,sizeof(words))==0,"all320 original chain bytes match captured fixture, including explicitPRIM16");
    }
    // Both explicitly pinned version8 files use this exact serialized State
    // ABI and section offset, corroborated by backup render_state ordering.
    // Reject another layout rather than searching for a plausible byte tag.
    static_assert(sizeof(State)==2536 && offsetof(State,count)==484);
    std::array<char,8> gs_tag{};State preceding{};
    file.seekg(33620275);file.read(gs_tag.data(),gs_tag.size());
    check(gs_tag==std::array<char,8>{{'g','s',0,0,0,0,0,0}},"pinned saved GS section at33620275");
    file.read(reinterpret_cast<char*>(&preceding),sizeof(preceding));
    check(bool(file)&&preceding.prim==0x154&&preceding.prmode==0&&preceding.prmodecont==1&&preceding.colclamp==1&&
        preceding.scanmsk==0&&preceding.pabe==0&&preceding.count==2,"exact captured preceding GSState variant");
    std::fprintf(stderr,"[retail-final-source] %s EEoffset1476 chainSHA256=bac2162f3cbb400d66edceca030dc30d0471c12559ed2a07045a94237f11b7db\n",path);
    check(native_gl::enabled(),"native GL for captured final chain");
    const auto tex0=words[18],clamp=words[16];
    const unsigned source_base=tex0&16383u,source_bw=(tex0>>14)&63u;
    const unsigned source_width=((clamp>>14)&1023u)+1,source_height=((clamp>>34)&1023u)+1;
    auto source_state=state(base,source_base/32);source_state.prim&=~(1u<<4);
    source_state.ctx[0].frame=(source_base/32)|(std::uint64_t(source_bw)<<16);
    auto v=vertices();check(native_gl::submit(source_state,v.data(),2),"create exact offscreen source named by originalTEX0");
    const auto color=[](unsigned x,unsigned y){return 0xff000000u|(x&255u)|((y&255u)<<8)|(((x/8)^(y/8))&255u)<<16;};
    std::vector<std::uint32_t> source(source_width*source_height);
    for(unsigned y=0;y<source_height;++y)for(unsigned x=0;x<source_width;++x)source[y*source_width+x]=color(x,y)&0x80ffffffu;
    check(native_gl::upload_image(PSMCT32,source_base,source_bw,0,0,source_width,source_height,
        reinterpret_cast<const std::uint8_t*>(source.data()),source.size()*4),"known native GPU offscreen pattern, no CPU VRAM");
    g_state=preceding; // Original PRIM16 AD write resets the saved strip queue; no invented reset/device clock.
    ps2::gif::transfer(3,chain.data()+16,19); // Strip original DMAtag; retain exact GIFtag+18 AD writes.
    DisplayRegs output{};output.pmode=1;output.dispfb[0]=13ull<<9|std::uint64_t(PSMCT16S)<<15;
    output.display[0]=831ull<<32|447ull<<44;
    std::vector<std::uint32_t> pixels;unsigned w=0,h=0;
    check(native_gl::read_picture(output,pixels,w,h)&&w==832&&h==448,"exact original destination0/16S output is captured after GIF replay");
    unsigned mismatches=0;double max_error=0;
    for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){
        // Independent bilinear arithmetic with original saved ST bias and
        // existing native sprite phase(-.49X,0Y). This verifies the new view
        // without changing user-verified sampling; hardwarephase parityOPEN.
        const double fx=std::clamp(.375+(x+.01)*640.0/832.0-.5,0.0,639.0);
        const double fy=std::clamp(.5+(y+.5)*448.0/448.0-.5,0.0,447.0);
        const unsigned x0=unsigned(fx),y0=unsigned(fy),x1=std::min(x0+1,639u),y1=std::min(y0+1,447u);
        const double ax=fx-x0,ay=fy-y0;
        for(unsigned component=0;component<3;++component){
            const auto c=[&](unsigned xx,unsigned yy){return double((color(xx,yy)>>(component*8))&255);};
            const double expected=(c(x0,y0)*(1-ax)+c(x1,y0)*ax)*(1-ay)+(c(x0,y1)*(1-ax)+c(x1,y1)*ax)*ay;
            const double error=std::abs(double((pixels[y*w+x]>>(component*8))&255)-expected);
            max_error=std::max(max_error,error);if(error>1.1)++mismatches;
        }
    }
    std::fprintf(stderr,"[retail-final-rgb] pixels=%u mismatchedComponents=%u maxError=%g (nativeRGBA8;16Squantization/ditherparityOPEN)\n",w*h,mismatches,max_error);
    check(!mismatches,"captured actual chain resampling RGB agrees with independent bilinear/ST/clamp reference");
    check(native_gl::present_frame(output),"exact destination resource presents without source guessing");
    native_gl::shutdown();return 0;
}
#endif
}
int main(int argc,char** argv) {
    try {
#ifdef _WIN32
        if(argc==3&&std::string(argv[1])=="--retail-final")return retail_final(argv[2]);
        check(argc==1,"usage: test_native_image_upload [--retail-final preserved.state]");
        using namespace ps2::gs;
        check(native_gl::enabled(),"actual native GL initialization");
        const auto* renderer=glGetString(GL_RENDERER);check(renderer!=nullptr,"actual GL renderer");
        std::fprintf(stderr,"[image-test-driver] %s\n",renderer);
        const std::array<std::uint8_t,16> rgba={255,0,0,64, 0,255,0,127, 0,0,255,128, 255,255,255,255};
        auto s=state();auto v=vertices();
        auto missing=state(16000);
        write_pixel(PSMCT32,16000,1,0,0,0xffcc77aau);
        check(!native_gl::submit(missing,v.data(),2),"CPU-only texture must reject instead of importing emulated VRAM");
        check(native_gl::upload_image(PSMCT32,base,1,0,0,2,1,rgba.data(),8),"upload before any texture binding");
        check(!native_gl::submit(s,v.data(),2),"undefined second source row must reject, not zero-fill");
        check(native_gl::upload_image(PSMCT32,base,1,0,1,2,1,rgba.data()+8,8),"grow source and preserve first GPU row");
        check(native_gl::submit(s,v.data(),2),"consume exact pending GPU texture view");
        // Scene storage is bottom-up; the two uploaded logical rows remain top-down.
        pixel(8,512-9,{255,0,0,128});pixel(9,512-9,{0,255,0,254});
        pixel(8,512-10,{0,0,255,255});pixel(9,512-10,{255,255,255,255});
        const std::array<std::uint8_t,4> yellow={255,255,0,64};
        check(native_gl::upload_image(PSMCT32,base,1,1,0,1,1,yellow.data(),4),"partial GPU-owned update");
        check(native_gl::submit(s,v.data(),2),"updated view is rebuilt from GPU source");pixel(9,512-9,{255,255,0,128});
        check(!native_gl::upload_image(PSMT8,base,1,0,0,1,1,yellow.data(),4),"indexed transfer reinterpretation rejected");
        check(!native_gl::upload_image(PSMCT32,base,2,0,0,1,1,yellow.data(),4),"same-identity stride alias rejected");
        check(!native_gl::upload_image(PSMCT32,base,1,0,0,1,1,yellow.data(),3),"inexact byte count rejected");
        check(!native_gl::upload_image(PSMCT32,base,1,64,0,1,1,yellow.data(),4),"rectangle outside declared stride rejected");
        auto indexed=s;indexed.ctx[0].tex0|=std::uint64_t(PSMT8)<<20;
        check(!native_gl::submit(indexed,v.data(),2),"indexed view cannot fall back to CPU decoder");
        // Existing exact framebuffer destination is updated natively; sample its
        // logical top-left source into a different native FBO using GPU copy.
        check(native_gl::upload_image(PSMCT32,0,1,0,0,2,2,rgba.data(),rgba.size()),"existing FBO GPU upload");
        auto alias=state(0,64);check(native_gl::submit(alias,v.data(),2),"exact CT32 framebuffer texture alias");
        pixel(8,512-9,{255,0,0,128});pixel(9,512-9,{0,255,0,254});
        pixel(8,512-10,{0,0,255,255});pixel(9,512-10,{255,255,255,255});
        // Real GS transfer parser: 3x2 pixels cross two IMAGE chunks. Column3
        // was actually uploaded beforehand so the declared 4x2 TEX view is defined.
        constexpr std::uint32_t packet_base=14848, sentinel=0x31415926;
        std::array<std::uint32_t,8> prefill;prefill.fill(0x800080ffu);
        check(native_gl::upload_image(PSMCT32,packet_base,1,0,0,4,2,
            reinterpret_cast<const std::uint8_t*>(prefill.data()),sizeof(prefill)),"prefill native GS integration texture");
        for(unsigned y=0;y<2;++y)for(unsigned x=0;x<4;++x)write_pixel(PSMCT32,packet_base,1,x,y,sentinel);
        write(0x50u,(std::uint64_t(packet_base)<<32)|(1ull<<48));write(0x51u,0);
        write(0x52u,3ull|(2ull<<32));write(0x53u,0);
        const std::array<std::uint32_t,8> packet={0x800000ff,0x8000ff00,0x80ff0000,
            0x80ffffff,0x8000ffff,0x80ffff00,0xdeadbeef,0xbad0cafe};
        write_image(reinterpret_cast<const std::uint8_t*>(packet.data()),1);
        write_image(reinterpret_cast<const std::uint8_t*>(packet.data()+4),1);
        for(unsigned y=0;y<2;++y)for(unsigned x=0;x<4;++x)
            check(read_pixel(PSMCT32,packet_base,1,x,y)==sentinel,"native split IMAGE must not write CPU VRAM");
        auto packet_state=state(packet_base);packet_state.ctx[0].tex0 &= ~(15ull<<26);packet_state.ctx[0].tex0 |= 2ull<<26;
        auto packet_vertices=v;packet_vertices[1].x=12*16;packet_vertices[1].u=4*16;
        check(native_gl::submit(packet_state,packet_vertices.data(),2),"consume actual split GS IMAGE pixels");
        pixel(8,512-9,{255,0,0,255});pixel(9,512-9,{0,255,0,255});pixel(10,512-9,{0,0,255,255});
        pixel(8,512-10,{255,255,255,255});pixel(9,512-10,{255,255,0,255});pixel(10,512-10,{0,255,255,255});
        pixel(11,512-9,{255,128,0,255});pixel(11,512-10,{255,128,0,255});
        // Actual DISPLAY/DISPFB selection of an uploaded-only resource, not a
        // guessed movie base or the active framebuffer's unrelated contents.
        DisplayRegs display{};display.pmode=1;display.dispfb[0]=(base/32u)|(1ull<<9);
        display.display[0]=(1ull<<32)|(1ull<<44);
        check(native_gl::present_frame(display),"descriptor selects native uploaded-only display resource");
        const auto bind=reinterpret_cast<BindFramebuffer>(wglGetProcAddress("glBindFramebuffer"));check(bind!=nullptr,"FBO inspection entry point");
        bind(0x8D40,0);glReadBuffer(GL_BACK);
        RECT rect{};check(GetClientRect(WindowFromDC(wglGetCurrentDC()),&rect),"actual hidden output extent");
        const int side=std::min(rect.right,rect.bottom),left=(rect.right-side)/2,top=(rect.bottom-side)/2;
        // RGB scanout is exact; native-window backbuffer alpha is not a source-format contract.
        std::array<unsigned char,4> window{};glReadPixels(left+side/4,rect.bottom-1-(top+side/4),1,1,GL_RGBA,GL_UNSIGNED_BYTE,window.data());
        check(window[0]==255 && window[1]==0 && window[2]==0,"actual presented source pixel from DISPLAY identity");
        check(glGetError()==GL_NO_ERROR,"native display GL operation succeeds");
        std::vector<std::uint32_t> picture;std::uint32_t picture_width=0,picture_height=0;
        check(native_gl::read_picture(display,picture,picture_width,picture_height),"capture exact uploaded-only DISPLAY source");
        const std::vector<std::uint32_t> raw_picture={0x400000ff,0x4000ffff,0x80ff0000,0xffffffff};
        check(picture_width==2 && picture_height==2 && picture==raw_picture,
              "GPU source capture preserves original raw alpha, RGB and row order");
        write_privileged(0x0000,1);write_privileged(0x0070,(base/32u)|(2ull<<9));
        write_privileged(0x0080,display.display[0]);
        check(ps2::gs::display(display,picture,picture_width,picture_height) && picture==raw_picture && picture_width==2 && picture_height==2,
              "GS display uses explicit descriptor despite different global DISPFB");
        check(copy_frame(display,picture,picture_width,picture_height) && picture==raw_picture,
              "GS copy_frame uses exact explicit native picture");
        // DIR changes traversal, never orientation. Independent primary local
        // PCSX2 Move advances source and destination coordinates together.
        for(unsigned direction=0;direction<4;++direction) {
            const std::uint32_t destination=13056+direction*64;
            check(native_gl::copy_image(PSMCT32,base,1,0,0,PSMCT32,destination,1,0,0,2,2,direction),"raw GPU copy in each traversal direction");
            check(capture(destination,2,2)==raw_picture,"raw copies preserve bytes/alpha and do not mirror");
        }
        check(!native_gl::upload_image(PSMCT32,base+1,1,0,0,1,1,yellow.data(),4),"interior upload alias rejects");
        auto interior=state(base+1);check(!native_gl::submit(interior,v.data(),2),"interior texture alias cannot decode CPU VRAM");
        check(!native_gl::copy_image(PSMCT32,base+1,1,0,0,PSMCT32,13440,1,0,0,1,1),"interior copy source alias rejects");
        check(!native_gl::copy_image(PSMCT32,base,1,0,0,PSMCT32,base+1,1,0,0,1,1),"interior copy destination alias rejects");
        constexpr std::uint32_t overlap_base=13632;
        const std::array<std::uint32_t,8> overlap={0x11010203,0x22040506,0x33070809,0x440a0b0c,
            0x550d0e0f,0x66101112,0x77131415,0x88161718};
        check(native_gl::upload_image(PSMCT32,overlap_base,1,0,0,4,2,
            reinterpret_cast<const std::uint8_t*>(overlap.data()),sizeof(overlap)),"overlap source is actually uploaded");
        check(native_gl::copy_image(PSMCT32,overlap_base,1,0,0,PSMCT32,overlap_base,1,1,0,3,2,3),"same-resource overlap uses GPU snapshot");
        const std::vector<std::uint32_t> overlapped={overlap[0],overlap[0],overlap[1],overlap[2],overlap[4],overlap[4],overlap[5],overlap[6]};
        check(capture(overlap_base,4,2)==overlapped,"overlap is exact snapshot copy, not in-place corruption or mirror");
        constexpr std::uint32_t holes=13824;
        check(native_gl::upload_image(PSMCT32,holes,1,0,0,2,1,rgba.data(),8),"partial copy source upload");
        check(!native_gl::copy_image(PSMCT32,holes,1,0,0,PSMCT32,13952,1,0,0,2,2),"copy rejects undefined source row");
        check(!native_gl::copy_image(PSMCT32,base,1,0,0,PSMCT32,13952,1,0,0,2,2,4),"unknown direction rejects");
        check(!native_gl::copy_image(PSMCT32,0,1,0,0,PSMCT32,14080,1,0,0,2,2),"FBO to raw cannot reconstruct original alpha");
        // Exact FBO representation survives FBO-to-FBO copy; raw-to-FBO
        // conversion happens once, on GPU, and preserves the CT32 alpha rule.
        check(native_gl::copy_image(PSMCT32,0,1,0,0,PSMCT32,64*32,1,0,0,2,2),"FBO to FBO native copy");
        auto copied_frame=state(64*32,128);check(native_gl::submit(copied_frame,v.data(),2),"sample copied FBO");
        pixel(8,512-9,{255,0,0,128});pixel(9,512-9,{0,255,0,254});
        check(native_gl::copy_image(PSMCT32,base,1,0,0,PSMCT32,0,1,0,0,2,2),"raw to existing FBO GPU conversion");
        auto converted_frame=state(0,128);check(native_gl::submit(converted_frame,v.data(),2),"sample raw-to-FBO copy");
        pixel(8,512-9,{255,0,0,128});pixel(9,512-9,{255,255,0,128});
        auto region_vertices=v;region_vertices[1].x=region_vertices[1].y=12*16;
        region_vertices[1].u=region_vertices[1].v=4*16;
        const auto region_state=[&](unsigned image){auto r=state(image,128);
            r.ctx[0].tex0=(r.ctx[0].tex0&~((15ull<<26)|(15ull<<30)))|(2ull<<26)|(2ull<<30);
            r.ctx[0].clamp=10ull|(1ull<<14)|(1ull<<34);return r;};
        auto raw_region=region_state(base);
        check(native_gl::submit(raw_region,region_vertices.data(),2),"raw GPU region clamp covers virtual4x4 using only defined2x2 footprint");
        pixel(8,512-9,{255,0,0,128});pixel(11,512-9,{255,255,0,128});pixel(8,512-12,{0,0,255,255});pixel(11,512-12,{255,255,255,255});
        auto fbo_region=region_state(0);
        check(native_gl::submit(fbo_region,region_vertices.data(),2),"FBO region clamp flips logicalY once and preserves existing alpha");
        pixel(8,512-9,{255,0,0,128});pixel(11,512-9,{255,255,0,128});pixel(8,512-12,{0,0,255,255});
        raw_region.ctx[0].clamp|=1ull<<4;
        check(native_gl::submit(raw_region,region_vertices.data(),2),"nonzero MINU clamps left virtual columns to defined source edge");
        pixel(8,512-9,{255,255,0,128});pixel(8,512-12,{255,255,255,255});
        raw_region=region_state(base);raw_region.ctx[0].clamp=(raw_region.ctx[0].clamp&~(1023ull<<14))|(2ull<<14);
        check(!native_gl::submit(raw_region,region_vertices.data(),2),"undefined pixel inside required clamp footprint cannot be zero-filled");
        // DISPLAY owns selection even when unrelated active/movie-numbered
        // targets were written later. No newest-writer or FBP280 pinning.
        auto blue_vertices=v;for(auto& p:blue_vertices){p.r=p.g=0;p.b=p.a=255;}
        for(unsigned frame:{280u,300u}){auto blue=state(base,frame);blue.prim&=~(1u<<4);
            check(native_gl::submit(blue,blue_vertices.data(),2),"later unrelated scene framebuffer draw");}
        auto exact_scene=display_for(0,2,2);
        check(native_gl::read_picture(exact_scene,picture,picture_width,picture_height)&&picture==
            std::vector<std::uint32_t>({0x800000ff,0x8000ffff,0xffff0000,0xffffffff}),
            "capture selects actual DISPLAY FBO despite newer active and movie-numbered targets");
        check(native_gl::present_frame(exact_scene),"present selects exact DISPLAY scene framebuffer");
        bind(0x8ca8,0);glReadBuffer(GL_BACK);
        glReadPixels(left+side/4,rect.bottom-1-(top+side/4),1,1,GL_RGBA,GL_UNSIGNED_BYTE,window.data());
        check(window[0]==255&&window[1]==0&&window[2]==0,"window shows requested red FBO instead of newer blue targets");
        auto offset_scene=exact_scene;offset_scene.dispfb[0]|=1ull<<32;offset_scene.display[0]=0;
        check(native_gl::read_picture(offset_scene,picture,picture_width,picture_height)&&picture_width==1&&picture_height==1&&picture[0]==0x8000ffff,
            "FBO capture honors explicit DISPFB DBX instead of resetting it to active target origin");
        auto absent_scene=display_for(511*32,2,2);picture={0xdeadbeef};picture_width=picture_height=1;
        check(!native_gl::read_picture(absent_scene,picture,picture_width,picture_height)&&picture.empty()&&!picture_width&&!picture_height,
            "unowned DISPLAY target fails and clears previous pixels");
        check(!native_gl::present_frame(absent_scene),"unowned DISPLAY cannot select another surface or allocate fake picture");
        // Deliberately distinct read/draw bindings catch restoration to only
        // GL_FRAMEBUFFER_BINDING (which aliases DRAW, not READ).
        using Gen=void(APIENTRY*)(GLsizei,GLuint*);using Attach=void(APIENTRY*)(GLenum,GLenum,GLenum,GLuint,GLint);
        using Delete=void(APIENTRY*)(GLsizei,const GLuint*);
        const auto gen=reinterpret_cast<Gen>(wglGetProcAddress("glGenFramebuffers"));
        const auto attach=reinterpret_cast<Attach>(wglGetProcAddress("glFramebufferTexture2D"));
        const auto del=reinterpret_cast<Delete>(wglGetProcAddress("glDeleteFramebuffers"));
        check(gen && attach && del,"native copy binding fixture entry points");
        GLuint test_fbo[2]{},test_texture[2]{};gen(2,test_fbo);glGenTextures(2,test_texture);
        for(unsigned i=0;i<2;++i){bind(0x8d40,test_fbo[i]);glBindTexture(GL_TEXTURE_2D,test_texture[i]);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
            attach(0x8d40,0x8ce0,GL_TEXTURE_2D,test_texture[i],0);}
        bind(0x8ca8,test_fbo[0]);bind(0x8ca9,test_fbo[1]);
        glEnable(GL_SCISSOR_TEST);glScissor(0,0,0,0);
        check(native_gl::copy_image(PSMCT32,base,1,0,0,PSMCT32,13440,1,0,0,2,2),"copy while read/draw bindings differ");
        check(glIsEnabled(GL_SCISSOR_TEST)==GL_TRUE,"copy restores enabled scissor despite bypassing it for resource copy");
        GLint read_binding=0,draw_binding=0;glGetIntegerv(0x8caa,&read_binding);glGetIntegerv(0x8ca6,&draw_binding);
        check(read_binding==GLint(test_fbo[0]) && draw_binding==GLint(test_fbo[1]),"copy restores both framebuffer owners exactly");
        check(native_gl::upload_image(PSMCT32,base,1,0,2,2,1,rgba.data(),8),"growth preserves separate framebuffer bindings");
        glGetIntegerv(0x8caa,&read_binding);glGetIntegerv(0x8ca6,&draw_binding);
        check(read_binding==GLint(test_fbo[0]) && draw_binding==GLint(test_fbo[1]),"growth helper restores read/draw independently");
        glDisable(GL_SCISSOR_TEST);bind(0x8d40,0);del(2,test_fbo);glDeleteTextures(2,test_texture);
        check(capture(13440,2,2)==raw_picture,"copy after independent binding restoration remains exact");
        // Indexed views read only GPU-owned raw bytes and the effective GPU
        // palette. Golden byte selectors follow primary PCSX2 columnTable8:
        // (0,0)=byte0, (8,0)=byte2, (0,2)=byte1 of their CT32 word.
        constexpr std::uint32_t indices_base=15360,palette_base=15424;
        std::vector<std::uint8_t> indices(64*32*4);
        for(std::size_t i=0;i<indices.size();i+=4){indices[i]=1;indices[i+1]=2;indices[i+2]=3;indices[i+3]=164;}
        check(native_gl::upload_image(PSMCT32,indices_base,1,0,0,64,32,indices.data(),indices.size()),"CT32 serialized indexed source remains raw GPU bytes");
        auto indexed_state=state(indices_base);indexed_state.ctx[0].tex0=indices_base|(2ull<<14)|(std::uint64_t(PSMT8)<<20)|
            (4ull<<26)|(2ull<<30)|(1ull<<34)|(1ull<<35)|(std::uint64_t(palette_base)<<37)|(1ull<<61);
        auto indexed_vertices=v;indexed_vertices[1].x=24*16;indexed_vertices[1].y=12*16;
        indexed_vertices[1].u=16*16;indexed_vertices[1].v=4*16;
        std::fill(std::begin(indexed_state.clut),std::end(indexed_state.clut),0xffffffffu);
        check(!native_gl::submit(indexed_state,indexed_vertices.data(),2),"CPU CLUT sentinel cannot replace missing native palette");
        std::array<std::uint32_t,256> palette_pixels{};
        for(unsigned i=0;i<256;++i){const unsigned j=(i&~24u)|((i&8u)<<1)|((i&16u)>>1);
            palette_pixels[j]=i|((255-i)<<8)|((i^85u)<<16)|(64u<<24);}
        check(native_gl::upload_image(PSMCT32,palette_base,1,0,0,16,16,
            reinterpret_cast<const std::uint8_t*>(palette_pixels.data()),sizeof(palette_pixels)),"actual GPU CSM1 palette source upload");
        check(native_gl::load_clut(indexed_state.ctx[0].tex0,0),"effective CSM1 CPSM32 CSA0 native load");
        check(native_gl::submit(indexed_state,indexed_vertices.data(),2),"PSMT8 view consumes GPU byte/palette shader");
        pixel(8,512-9,{1,254,84,128});pixel(16,512-9,{3,252,86,128});pixel(8,512-11,{2,253,87,128});
        for(unsigned mode:{unsigned(PSMT8H),unsigned(PSMT4HL),unsigned(PSMT4HH)}){
            auto high=state(indices_base);high.ctx[0].tex0|=std::uint64_t(mode)<<20;
            const unsigned index=mode==PSMT8H?164:mode==PSMT4HL?4:10;
            check(native_gl::submit(high,v.data(),2),"high-byte/nibble native indexed view");
            pixel(8,512-9,{static_cast<unsigned char>(index),static_cast<unsigned char>(255-index),static_cast<unsigned char>(index^85),128});
        }
        const std::array<std::uint8_t,4> replacement_palette={240,10,20,32};
        check(native_gl::upload_image(PSMCT32,palette_base,1,1,0,1,1,replacement_palette.data(),4),"palette source changes without implicit CLD load");
        check(native_gl::submit(indexed_state,indexed_vertices.data(),2),"retained palette remains usable before effective load");
        pixel(8,512-9,{1,254,84,128});
        check(native_gl::load_clut(indexed_state.ctx[0].tex0,0),"subsequent effective palette load updates GPU snapshot");
        check(native_gl::submit(indexed_state,indexed_vertices.data(),2),"palette generation invalidates indexed view cache");
        pixel(8,512-9,{240,10,20,64});
        const std::array<std::uint8_t,4> replacement_indices={7,2,3,164};
        check(native_gl::upload_image(PSMCT32,indices_base,1,0,0,1,1,replacement_indices.data(),4),"index raw byte update");
        check(native_gl::submit(indexed_state,indexed_vertices.data(),2),"index generation invalidates indexed view cache");
        pixel(8,512-9,{7,248,82,128});
        auto four=indexed_state;four.ctx[0].tex0=(four.ctx[0].tex0&~((63ull<<20)|(15ull<<26)))|(std::uint64_t(PSMT4)<<20)|(5ull<<26);
        auto four_vertices=indexed_vertices;four_vertices[1].x=40*16;four_vertices[1].u=32*16;
        check(native_gl::submit(four,four_vertices.data(),2),"ordinary PSMT4 nibble permutation consumes raw GPU bytes");
        pixel(8,512-9,{7,248,82,128});pixel(16,512-9,{2,253,87,128});
        pixel(24,512-9,{3,252,86,128});pixel(32,512-9,{4,251,81,128});
        four.ctx[0].tex0|=3ull<<56;
        check(native_gl::load_clut(four.ctx[0].tex0,0),"CSM1 four-bit palette writes selected CSA bank only");
        check(native_gl::submit(four,four_vertices.data(),2),"four-bit CSA bank selected during GPU lookup");
        pixel(8,512-9,{7,248,82,128});pixel(9,512-9,{240,10,20,64});
        const std::array<std::uint8_t,4> unloaded_palette={3,4,5,6};
        check(native_gl::upload_image(PSMCT32,palette_base,1,1,0,1,1,unloaded_palette.data(),4),"change low source bank before partial high CSA load");
        auto offset_eight=indexed_state;offset_eight.ctx[0].tex0|=15ull<<56;
        check(native_gl::load_clut(offset_eight.ctx[0].tex0,0),"CSM1 eight-bit load starts at CSA bank");
        check(native_gl::submit(offset_eight,indexed_vertices.data(),2),"eight-bit palette read clamps bank at240 instead of wrapping");
        pixel(8,512-9,{247,8,162,128});
        auto high_offset=state(indices_base);high_offset.ctx[0].tex0|=(std::uint64_t(PSMT8H)<<20)|(31ull<<56);
        check(native_gl::submit(high_offset,v.data(),2),"CPSM32 ignores high CSA bit and clamps index bank");
        pixel(8,512-9,{244,11,161,128});
        check(native_gl::submit(indexed_state,indexed_vertices.data(),2),"low palette banks survive later partial CSM1 load");
        pixel(9,512-9,{240,10,20,64});
        auto unsupported=indexed_state;unsupported.ctx[0].tex0|=3ull<<51;
        check(!native_gl::load_clut(unsupported.ctx[0].tex0,0),"unknown palette format rejects explicitly");
        constexpr std::uint32_t palette_holes=15616,csm2_base=15680;
        check(native_gl::upload_image(PSMCT32,palette_holes,1,0,0,16,1,
            reinterpret_cast<const std::uint8_t*>(palette_pixels.data()),64),"partial native palette upload");
        const auto hole_tex=(indexed_state.ctx[0].tex0&~(16383ull<<37))|(std::uint64_t(palette_holes)<<37);
        check(!native_gl::load_clut(hole_tex,0),"undefined palette entries fail before replacement");
        std::array<std::uint32_t,256> csm2_pixels{};
        for(unsigned i=0;i<256;++i)csm2_pixels[i]=i|((255-i)<<8)|((i^85u)<<16)|(64u<<24);
        check(native_gl::upload_image(PSMCT32,csm2_base,4,0,0,256,1,
            reinterpret_cast<const std::uint8_t*>(csm2_pixels.data()),sizeof(csm2_pixels)),"linear CSM2 GPU source");
        const auto csm2_tex=(indexed_state.ctx[0].tex0&~(16383ull<<37))|(std::uint64_t(csm2_base)<<37)|(1ull<<55);
        check(!native_gl::load_clut(csm2_tex|(1ull<<56),4),"CSM2 nonzero-CSA eight-bit storage remains explicitly unsupported");
        check(!native_gl::load_clut(csm2_tex,1),"CSM2 mismatched source stride rejects");
        check(native_gl::load_clut(csm2_tex,4),"CSM2 CBW/COU/COV native palette load");
        check(native_gl::submit(indexed_state,indexed_vertices.data(),2),"CSM2 palette usable without CPU CLUT state");
        pixel(8,512-9,{7,248,82,128});
        check(!native_gl::submit(four,four_vertices.data(),2),"cross-CSM four-bit reinterpretation remains explicitly unsupported");
        // CPSM16 CSM1 physical halfword positions independently selected from
        // the primary shuffle: logical1→2,2→8,3→10,4→16,7→26,164→176.
        // Raw bytes stay on GPU; TEXA expansion is deliberately deferred.
        constexpr std::uint32_t palette16_base=16000;
        std::vector<std::uint8_t> palette16_source(64*32*4);
        // upload_image accepts tightly packed LOGICAL CT32 rows, not physical
        // swizzled memory. Primary columnTable32 maps physical halfwords
        // 2/3→(1,0),8→(2,0),10→(3,0),16→(4,0),26→(7,0),176→(12,2).
        const auto half=[&](unsigned x,unsigned y,unsigned high,unsigned value){const auto at=(y*64+x)*4+high*2;
            palette16_source[at]=value&255;palette16_source[at+1]=value>>8;};
        half(1,0,0,0x001f);half(1,0,1,0xfc00);half(2,0,0,0x83e0);half(3,0,0,0x7c00);
        half(4,0,0,0x8000);half(7,0,0,0x001f);half(12,2,0,0xffff);
        check(native_gl::upload_image(PSMCT32,palette16_base,1,0,0,64,32,palette16_source.data(),palette16_source.size()),"raw serialized CPSM16 source lives in GPU IMAGE resource");
        const auto source16_capture=capture(palette16_base,64,32);
        check(source16_capture[1]==0xfc00001fu && source16_capture[7]==0x001fu && source16_capture[2*64+12]==0xffffu,
              "GPU raw source readback proves physical palette halfword coordinates and unchanged bytes");
        auto palette16_state=indexed_state;
        palette16_state.ctx[0].tex0=(palette16_state.ctx[0].tex0&~((16383ull<<37)|(15ull<<51)))|
            (std::uint64_t(palette16_base)<<37)|(std::uint64_t(PSMCT16)<<51);
        palette16_state.texa=32ull|(96ull<<32);
        check(!native_gl::submit(palette16_state,indexed_vertices.data(),2),"CPSM reinterpretation cannot use stale palette without effective load");
        check(native_gl::load_clut(palette16_state.ctx[0].tex0,0),"CPSM16 CSM1 palette loaded from actual GPU words");
        check(native_gl::submit(palette16_state,indexed_vertices.data(),2),"CPSM16 palette expands RGB5 and TEXA on GPU");
        pixel(8,512-9,{248,0,0,64});pixel(8,512-11,{0,248,0,192});pixel(16,512-9,{0,0,248,64});
        const std::array<std::uint8_t,4> odd_half_index={17,2,3,164};
        check(native_gl::upload_image(PSMCT32,indices_base,1,0,0,1,1,odd_half_index.data(),4),"select palette physical high halfword");
        check(native_gl::submit(palette16_state,indexed_vertices.data(),2),"CPSM16 odd-half byte selection is GPU-only");
        pixel(8,512-9,{0,0,248,192});
        check(native_gl::upload_image(PSMCT32,indices_base,1,0,0,1,1,replacement_indices.data(),4),"restore source index after high-half assertion");
        auto high16=state(indices_base);high16.ctx[0].tex0|=(std::uint64_t(PSMT8H)<<20)|(std::uint64_t(PSMCT16)<<51);
        high16.texa=palette16_state.texa;
        check(native_gl::submit(high16,v.data(),2),"CPSM16 high-byte source index selects palette word");
        pixel(8,512-9,{248,248,248,192});
        auto four16=state(indices_base);four16.ctx[0].tex0|=(std::uint64_t(PSMT4HL)<<20)|(std::uint64_t(PSMCT16)<<51);
        four16.texa=palette16_state.texa|(1ull<<15);
        check(native_gl::submit(four16,v.data(),2),"CPSM16 AEM does not erase black with bit15 set");
        pixel(8,512-9,{0,0,0,192});
        four16.ctx[0].tex0=(four16.ctx[0].tex0&~(63ull<<20))|(std::uint64_t(PSMT4HH)<<20);
        check(native_gl::submit(four16,v.data(),2),"CPSM16 AEM zero palette word becomes transparent");
        pixel(8,512-9,{0,0,0,0});
        four16.texa=palette16_state.texa;
        check(native_gl::submit(four16,v.data(),2),"same raw palette word obeys TEXA change without reload");
        pixel(8,512-9,{0,0,0,64});
        palette16_state.texa=5ull|(128ull<<32);
        check(native_gl::submit(palette16_state,indexed_vertices.data(),2),"TEXA generation changes expansion but preserves raw palette");
        pixel(8,512-9,{248,0,0,10});pixel(8,512-11,{0,248,0,255});
        auto unsupported16=palette16_state;unsupported16.ctx[0].tex0|=1ull<<56;
        check(!native_gl::load_clut(unsupported16.ctx[0].tex0,0),"CPSM16 nonzero CSA remains explicitly unsupported");
        unsupported16=palette16_state;unsupported16.ctx[0].tex0|=1ull<<55;
        check(!native_gl::load_clut(unsupported16.ctx[0].tex0,0),"CPSM16 CSM2 remains explicitly unsupported");
        check(!native_gl::submit(indexed_state,indexed_vertices.data(),2),"CPSM32 view cannot read a CPSM16 load as independent old snapshot");
        palette16_state.ctx[0].tex0=(palette16_state.ctx[0].tex0&~(15ull<<51))|(std::uint64_t(PSMCT16S)<<51);
        check(native_gl::load_clut(palette16_state.ctx[0].tex0,0),"CPSM16S CSM1 uses primary identical BlockPtr origin and shuffle");
        check(native_gl::submit(palette16_state,indexed_vertices.data(),2),"CPSM16S original encoded palette bytes expand natively");
        pixel(8,512-9,{248,0,0,10});
        // Full-size layout regression: explicit logical640x448 inside an
        //832-column native resource. Distinct padding must never influence
        // source identity, viewport dimensions or centered destination math.
        constexpr unsigned canvas_fbp=129,scan_width=832,canvas_width=640,canvas_height=448;
        auto canvas_state=state(base,canvas_fbp);canvas_state.ctx[0].frame=canvas_fbp|(13ull<<16);canvas_state.prim&=~(1u<<4);
        check(native_gl::submit(canvas_state,v.data(),2),"create exact owned full-frame presentation resource");
        std::vector<std::uint32_t> canvas(scan_width*canvas_height);
        const auto canvas_color=[](unsigned x,unsigned y){return 0xff000000u|(x&255u)|((y&255u)<<8)|(((x/64+y/32)&255u)<<16);};
        for(unsigned y=0;y<canvas_height;++y)for(unsigned x=0;x<scan_width;++x)
            canvas[y*scan_width+x]=(x<canvas_width?canvas_color(x,y):0xffff00ffu)&0x80ffffffu;
        check(native_gl::upload_image(PSMCT32,canvas_fbp*32,13,0,0,scan_width,canvas_height,
            reinterpret_cast<const std::uint8_t*>(canvas.data()),canvas.size()*4),"full832x448 GPU source with192 distinct padding columns");
        auto canvas_display=display_for(canvas_fbp*32,scan_width,canvas_height);canvas_display.dispfb[0]=canvas_fbp|(13ull<<9);
        check(native_gl::read_picture(canvas_display,picture,picture_width,picture_height)&&picture_width==scan_width&&picture_height==canvas_height,
            "unconfigured resource retains exact DISPLAY extent rather than guessed content bounds");
        const native_gl::PresentationLayout canvas_layout{scan_width,canvas_height,0,0,canvas_width,canvas_height};
        check(native_gl::configure_presentation_layout(canvas_fbp,13,PSMCT32,canvas_layout),"bind explicit source-proven native logical viewport to exact resource");
        check(!native_gl::configure_presentation_layout(511,13,PSMCT32,canvas_layout),"layout cannot create or alias an unowned framebuffer");
        check(!native_gl::configure_presentation_layout(canvas_fbp,10,PSMCT32,canvas_layout),"layout does not authorize a different stride");
        auto unrelated=state(base,300);unrelated.prim&=~(1u<<4);unrelated.ctx[0].scissor=(1ull<<16)|(1ull<<48);
        check(native_gl::submit(unrelated,v.data(),2),"unrelated small primitive scissor cannot redefine selected logical canvas");
        check(native_gl::read_picture(canvas_display,picture,picture_width,picture_height)&&picture_width==canvas_width&&picture_height==canvas_height,
            "explicit resource layout selects640x448 independent of unrelated active scissor");
        for(unsigned y=0;y<canvas_height;++y)for(unsigned x=0;x<canvas_width;++x)
            check(picture[y*canvas_width+x]==canvas_color(x,y),"every full-frame GPU capture pixel matches logical canvas excluding padding");
        check(native_gl::present_frame(canvas_display),"present explicit full-size logical viewport");
        bind(0x8ca8,0);glReadBuffer(GL_BACK);
        const unsigned ow=rect.right-rect.left,oh=rect.bottom-rect.top;
        std::vector<std::uint8_t> output(std::size_t(ow)*oh*4);glReadPixels(0,0,ow,oh,GL_RGBA,GL_UNSIGNED_BYTE,output.data());
        check(glGetError()==GL_NO_ERROR,"full native viewport window readback succeeds");
        const double canvas_fit=std::min(double(ow)/canvas_width,double(oh)/canvas_height);
        const double canvas_scale=canvas_fit>=1?std::floor(canvas_fit):canvas_fit;
        const int cw=std::max(1,int(std::round(canvas_width*canvas_scale))),ch=std::max(1,int(std::round(canvas_height*canvas_scale)));
        const int cx=(int(ow)-cw)/2,cy=(int(oh)-ch)/2;
        for(unsigned wy=0;wy<oh;++wy)for(unsigned wx=0;wx<ow;++wx){
            std::uint32_t expected=0;
            if(int(wx)>=cx&&int(wx)<cx+cw&&int(wy)>=cy&&int(wy)<cy+ch){
                const auto sx=std::min(canvas_width-1,unsigned((double(int(wx)-cx)+0.5)*canvas_width/cw));
                const auto sy=std::min(canvas_height-1,unsigned((double(cy+ch-1-int(wy))+0.5)*canvas_height/ch));
                expected=canvas_color(sx,sy);
            }
            const auto at=(std::size_t(wy)*ow+wx)*4;
            check(output[at]==(expected&255)&&output[at+1]==((expected>>8)&255)&&output[at+2]==((expected>>16)&255),
                "every window RGB pixel matches centered nearest logical viewport and black borders");
        }
        auto mismatched_scan=canvas_display;mismatched_scan.display[0]=(831ull<<32)|(446ull<<44);
        check(!native_gl::read_picture(mismatched_scan,picture,picture_width,picture_height)&&picture.empty(),
            "typed layout rejects an unconfigured scan extent rather than silently recropping");
        display.dispfb[0]=(base/32u)|(2ull<<9);
        check(!native_gl::present_frame(display),"display stride alias rejects");
        check(!native_gl::read_picture(display,picture,picture_width,picture_height) && picture.empty() && !picture_width && !picture_height,
              "unsupported source capture fails without unrelated FBO/stale pixels");
        picture={0xdeadbeef};picture_width=picture_height=1;
        check(!ps2::gs::display(display,picture,picture_width,picture_height) && picture.empty() && !picture_width && !picture_height,
              "GS display clears stale successful capture on explicit descriptor failure");
        picture={0xdeadbeef};picture_width=picture_height=1;
        check(!copy_frame(display,picture,picture_width,picture_height) && picture.empty() && !picture_width && !picture_height,
              "GS copy_frame clears stale picture on failed native descriptor");
        native_gl::shutdown();
        std::puts("[native-image-positive] uploads, copies, indexed GPU palettes and explicit DISPLAY pixels passed");return 0;
#else
        throw std::runtime_error("actual native IMAGE fixture requires Windows GL");
#endif
    }catch(const std::exception& e){std::fprintf(stderr,"[native-image-fail] %s\n",e.what());ps2::gs::native_gl::shutdown();return 1;}
}
