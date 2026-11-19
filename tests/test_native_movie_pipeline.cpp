// Actual native file -> codec -> retail color arithmetic -> OpenGL integration.
// Hidden back-buffer comparison only: no game, swap, A/V clock or playback loop.
#include "mka/movie_decode.hpp"
#include "mka/movie_color.hpp"
#include "mka/movie_gl_device.hpp"
#include "native_test_sha256.hpp"
#include "data/movie_decode_oracle.hpp"
#include <GL/gl.h>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <optional>
#include <stdexcept>

namespace {
using namespace mka::video;
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
struct HiddenContext {
    HWND window=nullptr;HDC dc=nullptr;HGLRC context=nullptr;
    HiddenContext(){
        WNDCLASSW wc{};wc.style=CS_OWNDC;wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"NativeMoviePipelineTest";
        check(RegisterClassW(&wc)||GetLastError()==ERROR_CLASS_ALREADY_EXISTS,"hidden window class");
        window=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP|WS_DISABLED,0,0,640,448,nullptr,nullptr,wc.hInstance,nullptr);check(window!=nullptr,"hidden window");
        dc=GetDC(window);check(dc!=nullptr,"host DC");PIXELFORMATDESCRIPTOR p{};p.nSize=sizeof(p);p.nVersion=1;p.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;p.iPixelType=PFD_TYPE_RGBA;p.cColorBits=32;p.cAlphaBits=8;p.iLayerType=PFD_MAIN_PLANE;
        const int format=ChoosePixelFormat(dc,&p);check(format&&SetPixelFormat(dc,format,&p),"host pixel format");context=wglCreateContext(dc);check(context&&wglMakeCurrent(dc,context),"owned current context");
        check(!IsWindowVisible(window),"no game or visible window");GLint alpha=0;glGetIntegerv(GL_ALPHA_BITS,&alpha);check(alpha>=8,"exact RGBA8 test surface");
    }
    ~HiddenContext(){if(wglGetCurrentContext()==context)wglMakeCurrent(nullptr,nullptr);if(context)wglDeleteContext(context);if(dc)ReleaseDC(window,dc);if(window)DestroyWindow(window);}
};
struct DeviceOwner {
    std::shared_ptr<NativeMovieGlDevice> device;
    ~DeviceOwner(){if(device&&device->shutdown()!=MovieGpuError::none)std::abort();}
};
std::vector<std::uint8_t> bytes(const std::filesystem::path& path){std::ifstream f(path,std::ios::binary);check(bool(f),"open original color data");return {(std::istreambuf_iterator<char>(f)),{}};}
bool equal_time(const MovieTimestamp& a,const MovieTimestamp& b){return a.value==b.value&&a.numerator==b.numerator&&a.denominator==b.denominator&&a.present==b.present;}
std::string present(const std::shared_ptr<NativeMovieGlDevice>& device,const MovieVideoFrame& input,const NativeMovieColorTable& table){
    MovieColorError color_error;auto converted=convert_movie_yuv420(input,table,color_error);check(converted&&color_error==MovieColorError::none,"real decoded frame converted with explicit verified role");
    auto expected=converted->rgba;auto uploaded=device->upload(*converted);check(uploaded.error==MovieGpuError::none&&uploaded.frame,"real converted bytes uploaded");
    const auto& metadata=uploaded.frame->metadata();
    check(equal_time(metadata.source_pts,input.source_pts)&&equal_time(metadata.timestamp,input.timestamp)&&metadata.conversion_role==table.role(),"raw/best timestamp and explicit color role survive GPU handoff");
    check(metadata.sample_aspect_numerator==input.sample_aspect_numerator&&metadata.sample_aspect_denominator==input.sample_aspect_denominator,"source SAR survives GPU handoff");
    check(metadata.color_range==input.color_range&&metadata.color_space==input.color_space&&metadata.color_primaries==input.color_primaries&&metadata.color_transfer==input.color_transfer,"all source color metadata survives GPU handoff");
    converted.reset(); // Neither codec planes nor converted CPU bytes back the GPU resource.
    NativeMoviePresentPlan plan{uploaded.frame,{0,0,input.width,input.height},MovieFilter::nearest,MovieAlpha::replace_rgba8};
    // Deliberate 1:1 fixture policy, not an inferred retail aspect/filter choice.
    MovieHostLayout layout{0x13579bdf2468ace0ull,640,448,MovieAspectMode::explicit_display_aspect,640,448,MovieWindowPolicy::centered_fit};
    const auto destination=movie_destination(plan,layout);check(destination&&destination->x==0&&destination->y==0&&destination->width==640&&destination->height==448,"exact explicit fixture pixel mapping");
    auto submitted=device->present(plan,layout);check(submitted.error==MovieGpuError::none&&submitted.fence,"native GPU submission retains source");
    MovieFenceResult state;
    for(unsigned i=0;i<5000;++i){state=device->poll(submitted.fence);if(state.state!=MovieFenceState::pending)break;Sleep(1);}
    check(state.state==MovieFenceState::complete&&state.error==MovieGpuError::none,"actual GPU completion");
    // Readback exists solely in this acceptance fixture, never in the device.
    using Bind=void(APIENTRY*)(GLenum,GLuint);auto bind=reinterpret_cast<Bind>(wglGetProcAddress("glBindFramebuffer"));check(bind!=nullptr,"test readback entry");
    bind(0x8ca8,0);glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);glPixelStorei(GL_PACK_ROW_LENGTH,0);glPixelStorei(GL_PACK_SKIP_ROWS,0);glPixelStorei(GL_PACK_SKIP_PIXELS,0);
    std::vector<std::uint8_t> bottom_up(expected.size()),top_down(expected.size());glReadPixels(0,0,640,448,GL_RGBA,GL_UNSIGNED_BYTE,bottom_up.data());check(glGetError()==GL_NO_ERROR,"test-only GPU readback");
    for(std::size_t y=0;y<448;++y)std::copy_n(bottom_up.data()+(447-y)*640*4,640*4,top_down.data()+y*640*4);
    check(top_down==expected,"every actual first/last movie RGBA pixel survives complete native pipeline");
    NativeTestSha256 sha;sha.add(top_down.data(),top_down.size());device->retire(std::move(submitted.fence));return sha.finish();
}
}
int main(int argc,char** argv){try{
    check(argc==4,"usage: test_native_movie_pipeline native_library_directory MOVIEPS2_directory original_MOVIER.ovl");
    const auto libs=std::filesystem::u8path(argv[1]),assets=std::filesystem::u8path(argv[2]);const auto overlay=bytes(std::filesystem::u8path(argv[3]));
    check(overlay.size()>=0x795d8+6144,"original color table extents");MovieColorError color_error;
    auto first_table=NativeMovieColorTable::decode(MovieColorRole::helper_default,overlay.data()+0x77dd8,6144,color_error);check(bool(first_table),"first explicit table identity");
    auto last_table=NativeMovieColorTable::decode(MovieColorRole::fresh_preset21,overlay.data()+0x795d8,6144,color_error);check(bool(last_table),"last explicit table identity");
    HiddenContext host;MovieGpuError gpu_error;DeviceOwner device{NativeMovieGlDevice::attach_current_context(host.window,0x13579bdf2468ace0ull,gpu_error)};check(device.device&&gpu_error==MovieGpuError::none,"independent native GPU service");
    std::printf("native pipeline GL: %s | %s | %s\n",glGetString(GL_VENDOR),glGetString(GL_RENDERER),glGetString(GL_VERSION));
    for(const auto& expected:native_movie_oracle::assets){
        MovieAssetFailure asset_error;auto catalog=NativeMovieAssetStore::create(assets,{{expected.name,expected.name}},asset_error);check(bool(catalog),"explicit exact asset binding");
        auto asset=catalog->open(expected.name,asset_error);check(asset&&asset->seek(19,mka::content::SeekOrigin::begin,asset_error),"owned inspected file handoff");
        MovieDecodeFailure failure;auto decoder=NativeMovieDecoder::open(libs,std::move(*asset),failure);check(bool(decoder),"same owned asset native decoder");catalog.reset();asset.reset();
        std::optional<MovieVideoFrame> first,last;NativeTestSha256 yuv,pcm;std::uint64_t videos=0,audios=0,samples=0;
        for(;;){auto result=decoder->next(failure);check(result.status!=MovieReadStatus::error,"complete movie decode");if(result.status==MovieReadStatus::end)break;
            if(auto* frame=std::get_if<MovieVideoFrame>(&result.frame)){
                for(const auto& plane:frame->planes)yuv.add(plane.data(),plane.size());++videos;if(!first)first=*frame;last=std::move(*frame);
            }else{const auto& audio=std::get<MovieAudioFrame>(result.frame);check(!audio.channels.empty(),"actual owned audio channels");for(const auto& channel:audio.channels)pcm.add(channel.data(),channel.size()*sizeof(std::int16_t));++audios;samples+=audio.channels[0].size();}
        }
        check(videos==expected.video.frames&&audios==expected.audio.frames&&samples==expected.audio.samples,"full original video/audio drain counts before GPU comparison");
        check(yuv.finish()==expected.video.sha256&&pcm.finish()==expected.audio.sha256,"every decoded byte retains independent native oracle identity");
        decoder.reset(); // GPU handoff uses owned frames AFTER file/codec/DLL teardown.
        check(first&&last&&first->width==640&&first->height==448&&last->width==640&&last->height==448,"pinned original frame shape");
        const auto first_sha=present(device.device,*first,*first_table),last_sha=present(device.device,*last,*last_table);
        std::printf("[native-pipeline] %s videos=%llu audio_frames=%llu samples_per_channel=%llu first_rgba_sha256=%s last_rgba_sha256=%s\n",expected.name,(unsigned long long)videos,(unsigned long long)audios,(unsigned long long)samples,first_sha.c_str(),last_sha.c_str());
    }
    std::puts("PASS three full owned native movie decodes and six exact first/last GPU frames; no game/audio-device/A-V-sync/retail-aspect-role-selection acceptance");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL native movie pipeline: %s\n",e.what());return 1;}}
