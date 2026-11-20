// Actual native GL upload/presentation fixture. Reads existing decoded planes
// and original lookup bytes as data; no game, decoder rerun or GS packets.
#include "mka/movie_gpu.hpp"
#include "mka/movie_gl_device.hpp"
#include <algorithm>
#include <cmath>
#include <climits>
#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>
#endif
namespace {
using namespace mka::video;
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
std::vector<std::uint8_t> bytes(const std::filesystem::path& p){std::ifstream in(p,std::ios::binary);check(bool(in),"open explicit data fixture");return {(std::istreambuf_iterator<char>(in)),{}};}
#ifdef _WIN32
struct HiddenContext {
    HWND window=nullptr;HDC dc=nullptr;HGLRC context=nullptr;
    HiddenContext(){
        WNDCLASSW wc{};wc.style=CS_OWNDC;wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"NativeMovieGlOwnedTest";
        check(RegisterClassW(&wc)||GetLastError()==ERROR_CLASS_ALREADY_EXISTS,"fixture window class");
        window=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP|WS_DISABLED,0,0,1103,601,nullptr,nullptr,wc.hInstance,nullptr);check(window!=nullptr,"hidden fixture window");
        dc=GetDC(window);check(dc!=nullptr,"fixture DC");PIXELFORMATDESCRIPTOR p{};p.nSize=sizeof(p);p.nVersion=1;p.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;p.iPixelType=PFD_TYPE_RGBA;p.cColorBits=32;p.cAlphaBits=8;p.iLayerType=PFD_MAIN_PLANE;
        int format=ChoosePixelFormat(dc,&p);check(format&&SetPixelFormat(dc,format,&p),"fixture pixel format");context=wglCreateContext(dc);check(context&&wglMakeCurrent(dc,context),"fixture owns current WGL context");
        check(!IsWindowVisible(window),"test never shows a window");
    }
    ~HiddenContext(){if(wglGetCurrentContext()==context)wglMakeCurrent(nullptr,nullptr);if(context)wglDeleteContext(context);if(dc)ReleaseDC(window,dc);if(window)DestroyWindow(window);}
};
struct RetirementScope {
    std::shared_ptr<NativeMovieGpuRetirementOwner> owner;
    RetirementScope() { MovieGpuError error; owner=NativeMovieGpuRetirementOwner::create(error); check(owner&&error==MovieGpuError::none,"explicit host retirement owner"); }
    ~RetirementScope() {
        for(unsigned i=0;i<5000;++i){
            auto result=owner->poll();
            if(result.state==MovieFenceState::complete)return;
            if(result.state==MovieFenceState::failed){result=owner->retry_failed();if(result.state==MovieFenceState::complete)return;}
            Sleep(1);
        }
        std::fputs("FAIL physical fixture retained GPU retirement work\n",stderr);std::abort();
    }
};
void shutdown_complete(const std::shared_ptr<NativeMovieGlDevice>& device) {
    for(unsigned i=0;i<5000;++i){const auto error=device->shutdown();if(error==MovieGpuError::none)return;
        check(error==MovieGpuError::pending,"actual native shutdown failure");Sleep(1);}
    throw std::runtime_error("physical fixture native shutdown pending");
}
void completed(const std::shared_ptr<NativeMovieGpuBackend>& backend,const std::shared_ptr<MovieGpuFence>& fence){
    check(bool(fence),"actual submitted GPU fence");MovieFenceResult status;
    for(unsigned i=0;i<5000;++i){status=backend->poll(fence);if(status.state!=MovieFenceState::pending)break;Sleep(1);}
    check(status.state==MovieFenceState::complete&&status.error==MovieGpuError::none,"actual GLsync reaches completion");
}
void picture(const NativeMoviePresentPlan& plan,const MovieHostLayout& layout,const MovieRgbaFrame& expected){
    const auto rect=movie_destination(plan,layout);check(bool(rect),"explicit movie destination");
    using Bind=void(APIENTRY*)(GLenum,GLuint);const auto bind=reinterpret_cast<Bind>(wglGetProcAddress("glBindFramebuffer"));check(bind!=nullptr,"fixture GL entry");
    GLint old_read=0;glGetIntegerv(0x8caa,&old_read);bind(0x8ca8,0);glReadBuffer(GL_BACK);
    glPixelStorei(GL_PACK_ALIGNMENT,1);glPixelStorei(GL_PACK_ROW_LENGTH,0);glPixelStorei(GL_PACK_SKIP_ROWS,0);glPixelStorei(GL_PACK_SKIP_PIXELS,0);
    std::vector<std::uint8_t> actual(std::size_t(layout.window_width)*layout.window_height*4);
    glReadPixels(0,0,layout.window_width,layout.window_height,GL_RGBA,GL_UNSIGNED_BYTE,actual.data());check(glGetError()==GL_NO_ERROR,"native movie consumer pixels");
    for(unsigned y=0;y<layout.window_height;++y)for(unsigned x=0;x<layout.window_width;++x){
        std::array<unsigned,4> color{0,0,0,255};
        if(x>=rect->x&&x<rect->x+rect->width&&y>=rect->y&&y<rect->y+rect->height){
            const double sx=(double(x-rect->x)+.5)*expected.width/rect->width-.5;
            const double sy=(double(y-rect->y)+.5)*expected.height/rect->height-.5;
            if(plan.filter==MovieFilter::nearest){
                const auto ix=std::min(expected.width-1,unsigned(std::floor(sx+.5))),iy=std::min(expected.height-1,unsigned(std::floor(sy+.5)));
                for(unsigned c=0;c<4;++c)color[c]=expected.rgba[(std::size_t(iy)*expected.width+ix)*4+c];
            }else{
                const int ix=int(std::floor(sx)),iy=int(std::floor(sy));const double fx=sx-ix,fy=sy-iy;
                for(unsigned c=0;c<4;++c){double value=0;
                    for(int j=0;j<2;++j)for(int i=0;i<2;++i){const auto px=std::clamp(ix+i,0,int(expected.width)-1),py=std::clamp(iy+j,0,int(expected.height)-1);
                        value+=expected.rgba[(std::size_t(py)*expected.width+px)*4+c]*(i?fx:1-fx)*(j?fy:1-fy);}
                    color[c]=unsigned(std::round(value));}
            }
        }
        const auto at=(std::size_t(layout.window_height-1-y)*layout.window_width+x)*4;
        for(unsigned c=0;c<4;++c)if(std::abs(int(actual[at+c])-int(color[c]))>(plan.filter==MovieFilter::linear?1:0)){
            std::fprintf(stderr,"movie pixel %u,%u channel%u actual%u expected%u\n",x,y,c,actual[at+c],color[c]);throw std::runtime_error("every native RGBA pixel matches independent sampling/alpha reference");}
    }
    bind(0x8ca8,old_read);
}
#endif
}
int main(int argc,char** argv){try{
    check(argc==3,"usage: test_movie_gl_device original_MOVIER.ovl existing_decoded_planes_directory");
#ifdef _WIN32
    using namespace mka::video;HiddenContext host;RetirementScope retirement_scope;auto retirement=retirement_scope.owner;
    std::printf("native-movie-gl driver: %s | %s | %s\n",glGetString(GL_VENDOR),glGetString(GL_RENDERER),glGetString(GL_VERSION));
    glViewport(3,5,73,91);GLint viewport[4]{};glGetIntegerv(GL_VIEWPORT,viewport);
    MovieGpuError attach_error;GLint alpha=0;glGetIntegerv(GL_ALPHA_BITS,&alpha);check(alpha>=8,"fixture requires real8-bit destination alpha");
    check(!NativeMovieGlDevice::attach_current_context(nullptr,1,retirement,attach_error)&&attach_error==MovieGpuError::unavailable,"missing host association fails");
    check(!NativeMovieGlDevice::attach_current_context(host.window,0,retirement,attach_error)&&attach_error==MovieGpuError::unavailable,"missing explicit destination fails");
    auto backend=NativeMovieGlDevice::attach_current_context(host.window,0x200000003ull,retirement,attach_error);check(bool(backend)&&attach_error==MovieGpuError::none,"pure native movie device without runtime/packet IDs");
    GLint unchanged[4]{};glGetIntegerv(GL_VIEWPORT,unchanged);check(std::equal(viewport,viewport+4,unchanged),"attach does not install fixed legacy viewport");
    const auto overlay=bytes(std::filesystem::u8path(argv[1]));check(overlay.size()>=0x77dd8+6144,"verified original helper lookup extent");
    MovieColorError color_error;const auto table=NativeMovieColorTable::decode(MovieColorRole::helper_default,overlay.data()+0x77dd8,6144,color_error);check(bool(table),"SHA-verified original conversion role");
    const auto directory=std::filesystem::u8path(argv[2]);MovieVideoFrame video;video.width=640;video.height=448;
    for(unsigned i=0;i<3;++i)video.planes[i]=bytes(directory/("LOGON.SFD.first.plane"+std::to_string(i)));
    video.source_pts={0,1,90000,true};video.timestamp=video.source_pts;
    video.codec_timing={{30000,1001},{30,1},{60000,1001},0x123456u};
    Mpeg2PictureTiming synthetic_picture;synthetic_picture.packet_picture_offset=(1ull<<34)+37;
    synthetic_picture.temporal_reference=981;synthetic_picture.coding_type=3;
    video.codec_timing.picture=synthetic_picture;
    MoviePictureSource synthetic_source;synthetic_source.packet_es_begin=(1ull<<36)+11;
    synthetic_source.picture_es_byte=synthetic_source.packet_es_begin+synthetic_picture.packet_picture_offset;
    synthetic_source.pes={0xe0,(1ull<<37)+2,(1ull<<37)+20,(1ull<<37)+2030,(1ull<<35)+1,(1ull<<35)+2011,(1ull<<33)-1,123456};
    synthetic_source.quarter_pts=((1ull<<33)-1)>>2;video.codec_timing.picture_source=synthetic_source;
    const auto expected_timing=video.codec_timing; // Synthetic ownership sentinel, not an actual source/selected rate.
    // Extracted plane files contain no SAR; preserve unknown rather than
    // attributing an invented aspect to the original source.
    auto converted=convert_movie_yuv420(video,*table,color_error);check(bool(converted),"actual decoded logo -> native converted owned RGBA");
    check(converted->conversion_role==MovieColorRole::helper_default,"verified converter role is retained");
    const auto expected=*converted;
    using Gen=void(APIENTRY*)(GLsizei,GLuint*);using Bind=void(APIENTRY*)(GLenum,GLuint);
    using Data=void(APIENTRY*)(GLenum,std::ptrdiff_t,const void*,GLenum);using Delete=void(APIENTRY*)(GLsizei,const GLuint*);
    const auto gen=reinterpret_cast<Gen>(wglGetProcAddress("glGenBuffers"));const auto erase=reinterpret_cast<Delete>(wglGetProcAddress("glDeleteBuffers"));
    const auto bind=reinterpret_cast<Bind>(wglGetProcAddress("glBindBuffer"));const auto data=reinterpret_cast<Data>(wglGetProcAddress("glBufferData"));
    check(gen&&erase&&bind&&data,"actual PBO ownership test entries");GLuint pbo=0;gen(1,&pbo);check(pbo!=0,"test PBO");bind(0x88ec,pbo);data(0x88ec,16,nullptr,0x88e0);
    glPixelStorei(GL_UNPACK_ALIGNMENT,8);glPixelStorei(GL_UNPACK_ROW_LENGTH,37);glPixelStorei(GL_UNPACK_SKIP_ROWS,3);glPixelStorei(GL_UNPACK_SKIP_PIXELS,2);glPixelStorei(GL_UNPACK_SWAP_BYTES,GL_TRUE);
    auto uploaded=backend->upload(*converted);check(uploaded.error==MovieGpuError::none&&uploaded.frame,"actual RGBA upload clears PBO/unpack interpretation");
    GLint value=0;glGetIntegerv(0x88ef,&value);check(value==int(pbo),"caller PBO binding restored");
    glGetIntegerv(GL_UNPACK_ALIGNMENT,&value);check(value==8,"unpack alignment restored");glGetIntegerv(GL_UNPACK_ROW_LENGTH,&value);check(value==37,"unpack row length restored");
    glGetIntegerv(GL_UNPACK_SKIP_ROWS,&value);check(value==3,"unpack skip rows restored");glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&value);check(value==2,"unpack skip pixels restored");
    glGetIntegerv(GL_UNPACK_SWAP_BYTES,&value);check(value==GL_TRUE,"unpack byte order restored");bind(0x88ec,0);erase(1,&pbo);
    check(uploaded.frame->metadata().source_pts.value==0&&uploaded.frame->metadata().timestamp.denominator==90000,"GPU frame retains timing without scheduling it");
    video.codec_timing={};check(uploaded.frame->metadata().codec_timing==expected_timing,"all independent rate, GOP, picture offset and PES provenance values survive source mutation");
    converted->rgba.clear();video.planes={};converted.reset(); // GPU resource has no CPU data dependency.
    RECT client{};check(GetClientRect(WindowFromDC(wglGetCurrentDC()),&client)!=0,"actual destination size");
    MovieHostLayout layout{0x200000003ull,unsigned(client.right),unsigned(client.bottom),MovieAspectMode::explicit_display_aspect,640,448,MovieWindowPolicy::centered_fit};
    NativeMoviePresentPlan plan{uploaded.frame,{0,0,640,448},MovieFilter::nearest,MovieAlpha::replace_rgba8};
    auto submission=backend->present(plan,layout);check(submission.error==MovieGpuError::none,"native movie present without GS packet or resource identity");completed(backend,submission.fence);picture(plan,layout,expected);backend->retire(std::move(submission.fence));
    auto invalid=plan;invalid.sample.width=639;check(backend->present(invalid,layout).error==MovieGpuError::invalid_plan,"unsupported crop is not silently executed");
    invalid=plan;invalid.alpha=MovieAlpha::unspecified;check(backend->present(invalid,layout).error==MovieGpuError::unsupported_effect,"unknown alpha cannot silently use GS scaling");
    auto unknown_sar=layout;unknown_sar.aspect=MovieAspectMode::source_sample_aspect;unknown_sar.display_aspect_numerator=unknown_sar.display_aspect_denominator=0;
    check(backend->present(plan,unknown_sar).error==MovieGpuError::invalid_layout,"missing source SAR requires explicit host policy");
    auto other=NativeMovieGlDevice::attach_current_context(host.window,layout.destination,retirement,attach_error);check(bool(other),"distinct native session");check(other->present(plan,layout).error==MovieGpuError::stale_resource,"foreign-session source is not aliased");
    MovieGpuError foreign=MovieGpuError::none;std::thread thread([&]{foreign=backend->upload(expected).error;});thread.join();check(foreign==MovieGpuError::wrong_thread,"foreign-thread upload does not steal context");
    MovieRgbaFrame large;large.width=2053;large.height=769;large.conversion_role=MovieColorRole::fresh_preset21;
    large.source_sample_aspect_numerator=9;large.source_sample_aspect_denominator=8;large.rgba.resize(std::size_t(large.width)*large.height*4);
    for(unsigned y=0;y<large.height;++y)for(unsigned x=0;x<large.width;++x){const auto at=(std::size_t(y)*large.width+x)*4;large.rgba[at]=x%256;large.rgba[at+1]=y%256;large.rgba[at+2]=(x/17+y/13)%256;large.rgba[at+3]=(x+y)%256;}
    // Synthetic bytes exercise capacity/native alpha, not a claim that this
    // arbitrary pattern was generated by the original lookup or retail movie.
    auto dynamic=backend->upload(large);check(dynamic.error==MovieGpuError::none&&dynamic.frame->width()==2053&&dynamic.frame->height()==769,"native dimensions exceed legacy1024x512 scene bounds");
    NativeMoviePresentPlan dynamic_plan{dynamic.frame,{0,0,2053,769},MovieFilter::nearest,MovieAlpha::replace_rgba8};
    layout.aspect=MovieAspectMode::source_sample_aspect;layout.display_aspect_numerator=layout.display_aspect_denominator=0;
    const auto destination=movie_destination(dynamic_plan,layout);check(bool(destination),"explicit positive source SAR");
    const double aspect=2053.*9/(769.*8),fit=std::min(double(layout.window_width)/aspect,double(layout.window_height));
    check(destination->width==unsigned(std::round(aspect*fit))&&destination->height==unsigned(std::round(fit)),"source SAR exact rational layout matches independent ratio");
    submission=backend->present(dynamic_plan,layout);check(submission.error==MovieGpuError::none,"present large native source using actual aspect");completed(backend,submission.fence);picture(dynamic_plan,layout,large);backend->retire(std::move(submission.fence));
    dynamic_plan.filter=MovieFilter::linear;submission=backend->present(dynamic_plan,layout);check(submission.error==MovieGpuError::none,"explicit linear native sampler");completed(backend,submission.fence);picture(dynamic_plan,layout,large);backend->retire(std::move(submission.fence));
    auto malformed=large;malformed.rgba.pop_back();check(backend->upload(malformed).error==MovieGpuError::invalid_frame,"truncated native bytes reject");malformed=large;malformed.conversion_role=MovieColorRole::unspecified;check(backend->upload(malformed).error==MovieGpuError::invalid_frame,"missing conversion role rejects");
    GLint limit=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&limit);check(limit>0&&limit<INT_MAX,"driver dimensional limit");MovieRgbaFrame oversized;oversized.width=unsigned(limit)+1;oversized.height=1;oversized.conversion_role=MovieColorRole::helper_default;oversized.rgba.resize(std::size_t(oversized.width)*4);
    check(backend->upload(oversized).error==MovieGpuError::host_capacity,"actual driver capacity named before texture allocation");
    // The fence itself retains the source after all caller frame/plan handles
    // are released, including when the backend is then destroyed.
    submission=backend->present(dynamic_plan,layout);check(submission.error==MovieGpuError::none,"retained-lifetime submission");
    std::weak_ptr<const MovieGpuFrame> weak=dynamic.frame;dynamic.frame.reset();dynamic_plan.frame.reset();check(!weak.expired(),"in-flight fence pins actual source");completed(backend,submission.fence);backend->retire(std::move(submission.fence));
    MovieGpuError wrong_shutdown=MovieGpuError::none;std::thread shutdown_thread([&]{wrong_shutdown=backend->shutdown();});shutdown_thread.join();
    check(wrong_shutdown==MovieGpuError::wrong_thread,"foreign shutdown revokes but retains owner cleanup");
    shutdown_complete(backend);check(backend->shutdown()==MovieGpuError::none,"explicit owner cleanup and idempotence");
    check(wglGetCurrentContext()==host.context&&wglGetCurrentDC()==host.dc&&IsWindow(host.window),"borrowed host context/window never detached or deleted");
    check(backend->upload(expected).error==MovieGpuError::stale_resource,"shutdown revokes session without resurrecting resource");
    check(other->present(plan,layout).error==MovieGpuError::stale_resource,"foreign-session lease rejects after other device shutdown");shutdown_complete(other);check(retirement->poll().state==MovieFenceState::complete,"all explicit native sessions retired");
    std::puts("PASS actual decoded-logo converted pixels, dynamic movieRGBA upload/filter/SAR/alpha/fences/ownership; no game/lifecycle/A-V-sync acceptance");return 0;
#else
    throw std::runtime_error("movie GPU fixture requires actual Windows GL");
#endif
}catch(const std::exception& e){std::fprintf(stderr,"FAIL native movie GPU: %s\n",e.what());return 1;}}
