// Real texture-only offscreen WGL test. Never ShowWindow/SwapBuffers/draw/present.
#include "mka/native_gl_texture.hpp"
#include "mka/movie_gl_device.hpp"
#include "mka/retail_system_art_cpu.hpp"
#include "native_test_sha256.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <thread>
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>
using namespace mka::video;using namespace mka::graphics;using namespace mka::retail;
extern "C" int SDL_InitSubSystem(unsigned){std::abort();}
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
struct HiddenContext {
    HWND window=nullptr;HDC dc=nullptr;HGLRC context=nullptr;
    HiddenContext(){
        WNDCLASSW wc{};wc.style=CS_OWNDC;wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"NativeSystemArtTextureTest";
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

std::string sha(const std::vector<std::uint8_t>&b){NativeTestSha256 h;h.add(b.data(),b.size());return h.finish();}
int main(int argc,char**argv)try{
 check(argc==3,"actual loose asset root and unique readback directory");auto output=std::filesystem::path(argv[2]);check(!std::filesystem::exists(output),"unique readback output");std::filesystem::create_directories(output);std::ofstream manifest(output/"images.tsv");check(bool(manifest),"readback manifest");
 mka::content::ContentVolumeFailure vf;auto volume=mka::content::NativeContentVolume::open(argv[1],vf);check(bool(volume),"actual volume");RetailFilePathFailure pf;auto paths=NativeRetailFilePaths::create(pf);RetailPackageFailure bf;auto package=NativeRetailPackage::open(*volume,*paths,bf);check(bool(package),"actual loose package");
 HiddenContext host;RetirementScope retirement;MovieGpuError error;
 auto device=NativeMovieGlDevice::attach_current_context(host.window,1,retirement.owner,error);check(device&&error==MovieGpuError::none,"actual existing context texture owner");auto foreign=NativeMovieGlDevice::attach_current_context(host.window,2,retirement.owner,error);check(bool(foreign),"independent actual context session");
 std::printf("GPU vendor=%s renderer=%s version=%s\n",glGetString(GL_VENDOR),glGetString(GL_RENDERER),glGetString(GL_VERSION));
 GLint max=0,units=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&max);glGetIntegerv(0x84e2,&units);std::printf("capabilities max_texture=%d texture_units=%d\n",max,units);check(max>0,"queried host capacity");
 const NativeTextureSampler initial{NativeTextureFilter::nearest,NativeTextureAddress::repeat,NativeTextureAddress::repeat};
 const NativeTextureSampler changed{NativeTextureFilter::linear,NativeTextureAddress::clamp_to_edge,NativeTextureAddress::repeat};
 using GenBuffers=void(APIENTRY*)(GLsizei,GLuint*);using BindBuffer=void(APIENTRY*)(GLenum,GLuint);using BufferData=void(APIENTRY*)(GLenum,std::ptrdiff_t,const void*,GLenum);using DeleteBuffers=void(APIENTRY*)(GLsizei,const GLuint*);
 auto gen=reinterpret_cast<GenBuffers>(wglGetProcAddress("glGenBuffers"));auto bind=reinterpret_cast<BindBuffer>(wglGetProcAddress("glBindBuffer"));auto data=reinterpret_cast<BufferData>(wglGetProcAddress("glBufferData"));auto erase=reinterpret_cast<DeleteBuffers>(wglGetProcAddress("glDeleteBuffers"));check(gen&&bind&&data&&erase,"actual native buffer API");GLuint pbo=0;gen(1,&pbo);bind(0x88ec,pbo);std::array<std::uint8_t,16> poison{};data(0x88ec,16,poison.data(),0x88e4);check(glGetError()==GL_NO_ERROR,"actual incompatible caller PBO fixture");
 GLuint sentinel=0;glGenTextures(1,&sentinel);glBindTexture(GL_TEXTURE_2D,sentinel);glPixelStorei(GL_UNPACK_ALIGNMENT,8);GLint before=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&before);
 std::vector<std::shared_ptr<const NativeGlTexture>> images;std::vector<GLuint> names;unsigned total=0;
 for(int language=0;language<5;++language){RetailSystemArtCpuFailure cf;auto cpu=prepare_system_art_cpu_section(*package,language,cf);check(bool(cpu),"actual system-art CPU owner");
  for(const auto& source:cpu->textures()){
   const auto& pixels=source.data.rgba_raw_alpha();NativeTextureRgbaImage input{source.data.width(),source.data.height(),pixels};auto made=device->upload_rgba_image(input,initial);check(made.texture&&made.failure.kind==NativeTextureError::none,"actual image GPU allocation/upload");check(made.texture->width()==input.width&&made.texture->height()==input.height&&made.texture->identity()!=0,"actual image capability extent/identity");NativeTextureFailure tf;auto name=made.texture->gl_object(tf);check(name&&glIsTexture(*name),"actual GL-created resource, no fixture token");
   GLint bound=0,unpack=0;glGetIntegerv(GL_TEXTURE_BINDING_2D,&bound);glGetIntegerv(GL_UNPACK_ALIGNMENT,&unpack);GLint unpack_buffer=0;glGetIntegerv(0x88ef,&unpack_buffer);check(bound==before&&unpack==8&&unpack_buffer==GLint(pbo),"native upload resets then restores caller PBO/binding/unpack state");
   glBindTexture(GL_TEXTURE_2D,*name);GLint w=0,h=0;glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_WIDTH,&w);glGetTexLevelParameteriv(GL_TEXTURE_2D,0,GL_TEXTURE_HEIGHT,&h);check(w==int(input.width)&&h==int(input.height),"queried actual native texture storage");std::vector<std::uint8_t> readback(pixels.size());glPixelStorei(GL_PACK_ALIGNMENT,1);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,readback.data());check(glGetError()==GL_NO_ERROR&&readback==pixels,"every actual GPU byte matches source-decoded stored RGBA/rawalpha");
   const auto filename=std::to_string(language)+"-"+std::to_string(source.ordinal)+".rgba";std::ofstream file(output/filename,std::ios::binary);file.write(reinterpret_cast<const char*>(readback.data()),std::streamsize(readback.size()));check(bool(file),"exact actual readback bytes saved");manifest<<filename<<'\t'<<input.width<<'\t'<<input.height<<'\t'<<sha(readback)<<'\n';
   glBindTexture(GL_TEXTURE_2D,sentinel);check(device->set_image_sampler(made.texture,changed,tf),"actual native sampler update");glGetIntegerv(GL_TEXTURE_BINDING_2D,&bound);check(bound==before,"sampler restores caller binding");glBindTexture(GL_TEXTURE_2D,*name);GLint filter=0,u=0,v=0;glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,&filter);glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,&u);glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,&v);check(filter==GL_LINEAR&&u==0x812f&&v==GL_REPEAT,"actual driver sampler values");glBindTexture(GL_TEXTURE_2D,sentinel);
   names.push_back(*name);images.push_back(std::move(made.texture));++total;
  }
 }
 check(total==105,"all actual type2/type3 textures uploaded");NativeTextureFailure tf;
 check(!foreign->set_image_sampler(images.front(),changed,tf)&&tf.kind==NativeTextureError::stale_resource,"foreign native session rejects actual capability");
 check(!device->set_image_sampler(images.front(),{},tf)&&tf.kind==NativeTextureError::invalid_sampler,"no implicit sampler policy");
 bool wrong=false;std::thread thread([&]{NativeTextureFailure f;wrong=!images.front()->gl_object(f)&&f.kind==NativeTextureError::wrong_thread;});thread.join();check(wrong,"actual image capability rejects foreign thread before GL use");
 std::array<std::uint8_t,4> one{};check(device->upload_rgba_image({1,1,one},{}).failure.kind==NativeTextureError::invalid_sampler,"unknown sampler rejects");check(device->upload_rgba_image({0,1,one},initial).failure.kind==NativeTextureError::invalid_image,"invalid dimensions reject");check(device->upload_rgba_image({2,1,one},initial).failure.kind==NativeTextureError::invalid_image,"truncated bytes reject");
 std::vector<std::uint8_t> too_wide(std::size_t(max+1)*4);check(device->upload_rgba_image({unsigned(max+1),1,too_wide},initial).failure.kind==NativeTextureError::host_capacity,"queried host capacity rejection");
 glBindTexture(0xffffffff,0);check(device->upload_rgba_image({1,1,one},initial).failure.kind==NativeTextureError::upload_failed,"incoming GL failure is named before successful allocation");
 auto drain=device->begin_context_drain();check(drain.receipt&&drain.result.state!=MovieFenceState::failed,"actual image context-tail fence");
 check(device->upload_rgba_image({1,1,one},initial).failure.kind==NativeTextureError::admission_closed,"drain seals actual image uploads");check(!images.front()->gl_object(tf)&&tf.kind==NativeTextureError::admission_closed,"drain seals image native-object admission");
 MovieFenceResult drained;for(unsigned i=0;i<5000;++i){drained=device->poll_context_drain(drain.receipt);if(drained.state!=MovieFenceState::pending)break;Sleep(1);}check(drained.state==MovieFenceState::complete&&device->resume_after_drain(drain.receipt)==MovieGpuError::none,"actual image context completion and source caller-authorized resume");check(bool(images.front()->gl_object(tf)),"resumed live native image");
 check(!foreign->revoke_image(images.front(),tf)&&tf.kind==NativeTextureError::stale_resource,"foreign revoke rejects");
 check(device->revoke_image(images.front(),tf)&&!glIsTexture(names.front()),"section-style revoke deletes actual storage while cache retains capability");
 check(!images.front()->gl_object(tf)&&tf.kind==NativeTextureError::stale_resource,"retained cache capability revoked");
 check(!device->set_image_sampler(images.front(),changed,tf)&&tf.kind==NativeTextureError::stale_resource,"revoked image rejects sampler");
 check(device->revoke_image(images.front(),tf)&&bool(images.back()->gl_object(tf)),"idempotent revoke preserves other images");
 shutdown_complete(device);for(auto n:names)check(!glIsTexture(n),"actual owner-current fence retirement deletes native texture objects");check(!images.front()->gl_object(tf)&&tf.kind==NativeTextureError::stale_resource,"retirement revokes retained capability");check(device->upload_rgba_image({1,1,one},initial).failure.kind==NativeTextureError::stale_resource,"revoked device never fabricates texture success");shutdown_complete(foreign);check(retirement.owner->poll().state==MovieFenceState::complete,"all native retirement work completed");
 check(wglGetCurrentContext()==host.context&&wglGetCurrentDC()==host.dc&&!IsWindowVisible(host.window),"borrowed context remains current and never visible");bind(0x88ec,0);erase(1,&pbo);glDeleteTextures(1,&sentinel);check(glGetError()==GL_NO_ERROR,"test actual PBO/sentinel cleanup");manifest.close();check(bool(manifest),"readback manifest closed");
 std::printf("PASS105 real GLimage uploads/exact byte readbacks/sampler/limits/context/thread/retirement; actual_gpu_context_opened=true visible_window=false presentation=false audio_endpoint=false; no source sampler mapping/cache/whole3011D0/16FC70/game acceptance\n");return 0;
}catch(const std::exception&e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
