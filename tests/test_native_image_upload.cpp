// Actual GL resource/consumer test only. No game boot/device reset/reference
// rasterizer. One GS integration case seeds/checks CPU sentinels to prove bypass.
#include "gs/native_gl_backend.hpp"
#include <array>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
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
#endif
}
int main() {
    try {
#ifdef _WIN32
        using namespace ps2::gs;
        check(native_gl::enabled(),"actual native GL initialization");
        const auto* renderer=glGetString(GL_RENDERER);check(renderer!=nullptr,"actual GL renderer");
        std::fprintf(stderr,"[image-test-driver] %s\n",renderer);
        const std::array<std::uint8_t,16> rgba={255,0,0,64, 0,255,0,127, 0,0,255,128, 255,255,255,255};
        auto s=state();auto v=vertices();
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
        auto unsupported=indexed_state;unsupported.ctx[0].tex0|=2ull<<51;
        check(!native_gl::load_clut(unsupported.ctx[0].tex0,0),"CPSM16 palette rejects explicitly");
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
