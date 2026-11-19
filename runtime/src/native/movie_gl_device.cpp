#include "mka/movie_gl_device.hpp"
#include <atomic>
#include <thread>
#include <climits>
#include <cstdio>
#include <new>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_FOG_COORDINATE_SOURCE  // OpenGL 1.4 constants
#define GL_FOG_COORDINATE_SOURCE 0x8450
#define GL_FOG_COORDINATE 0x8451
#endif
#ifndef GL_CONSTANT_ALPHA  // OpenGL 1.4 blend factors
#define GL_CONSTANT_ALPHA 0x8003
#define GL_ONE_MINUS_CONSTANT_ALPHA 0x8004
#endif
#ifndef GL_COMBINE
#define GL_COMBINE 0x8570
#define GL_COMBINE_RGB 0x8571
#define GL_COMBINE_ALPHA 0x8572
#define GL_RGB_SCALE 0x8573
#define GL_ALPHA_SCALE 0x0D1C
#define GL_SOURCE0_RGB 0x8580
#define GL_SOURCE1_RGB 0x8581
#define GL_SOURCE0_ALPHA 0x8588
#define GL_SOURCE1_ALPHA 0x8589
#define GL_OPERAND0_RGB 0x8590
#define GL_OPERAND1_RGB 0x8591
#define GL_OPERAND0_ALPHA 0x8598
#define GL_OPERAND1_ALPHA 0x8599
#define GL_PRIMARY_COLOR 0x8577
#endif
#ifndef GL_ALPHA_SCALE
#define GL_ALPHA_SCALE 0x0D1C
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_MAX_TEXTURE_UNITS 0x84E2
#endif
#ifndef GL_ACTIVE_TEXTURE
#define GL_ACTIVE_TEXTURE 0x84E0
#endif
#ifndef GL_PREVIOUS
#define GL_PREVIOUS 0x8578
#endif
#ifndef GL_ONE_MINUS_DST_ALPHA
#define GL_ONE_MINUS_DST_ALPHA 0x0305
#endif
#ifndef GL_FUNC_ADD
#define GL_FUNC_ADD 0x8006
#define GL_FUNC_SUBTRACT 0x800A
#define GL_FUNC_REVERSE_SUBTRACT 0x800B
#endif
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_READ_FRAMEBUFFER_BINDING 0x8CAA
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_RENDERBUFFER 0x8D41
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#endif
#ifndef GL_DRAW_FRAMEBUFFER_BINDING
#define GL_DRAW_FRAMEBUFFER_BINDING 0x8CA6
#endif
#ifndef GL_CURRENT_PROGRAM
#define GL_CURRENT_PROGRAM 0x8B8D
#endif

#endif
namespace mka::video {
#ifdef _WIN32
namespace {
namespace video=mka::video;
template<class T>T load(const char* name){auto p=wglGetProcAddress(name);if(!p||p==reinterpret_cast<PROC>(1)||p==reinterpret_cast<PROC>(2)||p==reinterpret_cast<PROC>(3)||p==reinterpret_cast<PROC>(-1))return nullptr;return reinterpret_cast<T>(p);}
using Sync=void*;
struct Texture {GLuint object=0;};
struct Work {Sync sync=nullptr;std::shared_ptr<const VideoFrameResource> source;std::atomic<bool> retired{false};};
struct Session {
    HGLRC context=nullptr;HDC dc=nullptr;HWND window=nullptr;std::thread::id owner;
    std::atomic<bool> revoked{false};bool cleaned=false;std::uint64_t destination=0,identity=0;
    Sync(APIENTRY* fence)(GLenum,GLbitfield)=nullptr;
    GLenum(APIENTRY* wait)(Sync,GLbitfield,std::uint64_t)=nullptr;
    void(APIENTRY* erase)(Sync)=nullptr;
    void(APIENTRY* bind_framebuffer)(GLenum,GLuint)=nullptr;
    void(APIENTRY* active)(GLenum)=nullptr;
    void(APIENTRY* use)(GLuint)=nullptr;
    std::vector<std::shared_ptr<Texture>> textures;std::vector<std::shared_ptr<Work>> work;
};
bool image_error(const char* operation){auto error=glGetError();if(error==GL_NO_ERROR)return false;std::fprintf(stderr,"[native-movie-gl] %s GL error=0x%X\n",operation,unsigned(error));return true;}
bool collect_video(Session& s){
    for(auto i=s.work.begin();i!=s.work.end();){auto& w=**i;if(w.retired.load(std::memory_order_acquire)){
        if(!w.sync)return false;const auto status=s.wait(w.sync,0,0);
        if(status==0x911a||status==0x911c){s.erase(w.sync);w.sync=nullptr;w.source.reset();i=s.work.erase(i);continue;}
        if(status!=0x911b)return false;}++i;}
    for(auto i=s.textures.begin();i!=s.textures.end();){if(i->use_count()==1){if((*i)->object)glDeleteTextures(1,&(*i)->object);i=s.textures.erase(i);}else ++i;}
    return true;
}
struct VideoGlState {
    GLint read=0,draw=0,program=0,active=GL_TEXTURE0,mode=GL_MODELVIEW;
    using UseProgram=void(APIENTRY*)(GLuint);
    Session& session;
    UseProgram use;
    explicit VideoGlState(Session& s):session(s),use(s.use){
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);
        glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_ACTIVE_TEXTURE,&active);glGetIntegerv(GL_MATRIX_MODE,&mode);
        glPushAttrib(GL_ALL_ATTRIB_BITS);if(use)use(0);
        GLint units=0;glGetIntegerv(GL_MAX_TEXTURE_UNITS,&units);
        for(GLint i=units-1;i>=1;--i){session.active(GL_TEXTURE0+i);glDisable(GL_TEXTURE_2D);}
        session.active(GL_TEXTURE0);
        glMatrixMode(GL_TEXTURE);glPushMatrix();glLoadIdentity();
        glMatrixMode(GL_PROJECTION);glPushMatrix();glMatrixMode(GL_MODELVIEW);glPushMatrix();
    }
    ~VideoGlState(){
        if(session.active)session.active(GL_TEXTURE0);
        glMatrixMode(GL_TEXTURE);glPopMatrix();
        glMatrixMode(GL_MODELVIEW);glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();
        session.bind_framebuffer(GL_READ_FRAMEBUFFER,read);session.bind_framebuffer(GL_DRAW_FRAMEBUFFER,draw);
        glPopAttrib();if(use)use(program);if(session.active)session.active(active);glMatrixMode(mode);
    }
};
void video_quad(GLuint texture,unsigned width,unsigned height,float u0,float v0,float u1,float v1,bool final_pass,unsigned x=0,unsigned y=0,bool linear=false){
    glViewport(x,y,width,height);glDisable(GL_SCISSOR_TEST);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_ALPHA_TEST);
    glDisable(GL_FOG);glDisable(GL_CULL_FACE);glDisable(GL_STENCIL_TEST);glDisable(GL_DITHER);glDisable(GL_COLOR_LOGIC_OP);
    glDisable(GL_LIGHTING);glDisable(GL_TEXTURE_GEN_S);glDisable(GL_TEXTURE_GEN_T);glDisable(GL_TEXTURE_GEN_R);glDisable(GL_TEXTURE_GEN_Q);
    for(unsigned i=0;i<6;++i)glDisable(GL_CLIP_PLANE0+i);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glDepthMask(GL_FALSE);
    glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
    glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,width,height,0,-1,1);
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,(final_pass||linear)?GL_LINEAR:GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,(final_pass||linear)?GL_LINEAR:GL_NEAREST);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_COMBINE);
    glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_RGB,GL_REPLACE);glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE0_RGB,GL_TEXTURE);
    glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND0_RGB,GL_SRC_COLOR);
    glTexEnvi(GL_TEXTURE_ENV,GL_RGB_SCALE,1);glTexEnvi(GL_TEXTURE_ENV,GL_ALPHA_SCALE,1);
    glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_ALPHA,GL_REPLACE);
    glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE0_ALPHA,final_pass?GL_PRIMARY_COLOR:GL_TEXTURE);
    glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND0_ALPHA,GL_SRC_ALPHA);glColor4f(1,1,1,0);
    glBegin(GL_QUADS);
    glTexCoord2f(u0,v0);glVertex2i(0,0);glTexCoord2f(u1,v0);glVertex2i(width,0);
    glTexCoord2f(u1,v1);glVertex2i(width,height);glTexCoord2f(u0,v1);glVertex2i(0,height);glEnd();
}
struct MovieLease final:video::MovieGpuFrame {
    std::shared_ptr<Session> session;std::shared_ptr<Texture> texture;
    video::MovieGpuMetadata info;std::uint32_t w=0,h=0;std::uint64_t id=0;
    std::uint64_t identity()const noexcept override{return id;}
    std::uint32_t width()const noexcept override{return w;}
    std::uint32_t height()const noexcept override{return h;}
    const video::MovieGpuMetadata& metadata()const noexcept override{return info;}
};
struct GlMovieFence final:video::MovieGpuFence {
    std::shared_ptr<Session> session;std::shared_ptr<Work> work;
    GlMovieFence(video::NativeMoviePresentPlan p,std::shared_ptr<Session> s,std::shared_ptr<Work> w)
        :MovieGpuFence(std::move(p)),session(std::move(s)),work(std::move(w)){}
    ~GlMovieFence(){work->retired.store(true,std::memory_order_release);}
};
video::MovieGpuError movie_current(const Session& s){
    if(s.revoked.load(std::memory_order_acquire))return video::MovieGpuError::stale_resource;
    if(std::this_thread::get_id()!=s.owner||wglGetCurrentContext()!=s.context||wglGetCurrentDC()!=s.dc)return video::MovieGpuError::wrong_thread;
    return video::MovieGpuError::none;
}
using MovieBindBuffer=void(APIENTRY*)(GLenum,GLuint);
// Client pixels must not be reinterpreted as an offset into a caller's PBO.
struct MovieUnpackState {
    MovieBindBuffer bind;GLint buffer=0,alignment=0,row=0,rows=0,pixels=0,swap=0,lsb=0;
    explicit MovieUnpackState(MovieBindBuffer b):bind(b){
        glGetIntegerv(0x88ef,&buffer);glGetIntegerv(GL_UNPACK_ALIGNMENT,&alignment);
        glGetIntegerv(GL_UNPACK_ROW_LENGTH,&row);glGetIntegerv(GL_UNPACK_SKIP_ROWS,&rows);glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&pixels);
        glGetIntegerv(GL_UNPACK_SWAP_BYTES,&swap);glGetIntegerv(GL_UNPACK_LSB_FIRST,&lsb);
        bind(0x88ec,0);glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);
        glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
        glPixelStorei(GL_UNPACK_SWAP_BYTES,GL_FALSE);glPixelStorei(GL_UNPACK_LSB_FIRST,GL_FALSE);
    }
    ~MovieUnpackState(){
        bind(0x88ec,buffer);glPixelStorei(GL_UNPACK_ALIGNMENT,alignment);glPixelStorei(GL_UNPACK_ROW_LENGTH,row);
        glPixelStorei(GL_UNPACK_SKIP_ROWS,rows);glPixelStorei(GL_UNPACK_SKIP_PIXELS,pixels);
        glPixelStorei(GL_UNPACK_SWAP_BYTES,swap);glPixelStorei(GL_UNPACK_LSB_FIRST,lsb);
    }
};
class GlMovieBackend final:public video::NativeMovieGlDevice {
public:
    GlMovieBackend(std::shared_ptr<Session> s,MovieBindBuffer b):session_(std::move(s)),bind_(b){}
    video::MovieGpuUpload upload(const video::MovieRgbaFrame& f)override{
        using E=video::MovieGpuError;
        if(auto e=movie_current(*session_);e!=E::none)return {e,{}};
        if(auto e=video::validate_movie_upload(f);e!=E::none)return {e,{}};
        if(!collect_video(*session_))return {E::fence_failed,{}};
        if(image_error("movie upload incoming GL state"))return {E::upload_failed,{}};
        GLint limit=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&limit);
        if(limit<=0||f.width>unsigned(limit)||f.height>unsigned(limit)||f.width>INT_MAX||f.height>INT_MAX)return {E::host_capacity,{}};
        try{
            auto lease=std::make_shared<MovieLease>();auto texture=std::make_shared<Texture>();session_->textures.push_back(texture);
            bool ok=false;
            {
                VideoGlState state(*session_);MovieUnpackState unpack(bind_);
                glGenTextures(1,&texture->object);glBindTexture(GL_TEXTURE_2D,texture->object);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
                glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,GLsizei(f.width),GLsizei(f.height),0,GL_RGBA,GL_UNSIGNED_BYTE,f.rgba.data());
                ok=texture->object&&!image_error("upload native movie RGBA8");
            }
            if(!ok||image_error("restore native movie upload state"))return {E::upload_failed,{}};
            if(session_->identity==UINT64_MAX)return {E::host_capacity,{}};auto id=session_->identity++;
            lease->session=session_;lease->texture=std::move(texture);lease->id=id+1;lease->w=f.width;lease->h=f.height;
            lease->info={f.conversion_role,f.source_pts,f.timestamp,f.source_sample_aspect_numerator,f.source_sample_aspect_denominator,
                f.source_color_range,f.source_color_space,f.source_color_primaries,f.source_color_transfer};
            return {E::none,std::move(lease)};
        }catch(const std::bad_alloc&){return {E::host_capacity,{}};}catch(const std::length_error&){return {E::host_capacity,{}};}
    }
    video::MovieGpuSubmission present(const video::NativeMoviePresentPlan& p,const video::MovieHostLayout& l)override{
        using E=video::MovieGpuError;
        if(auto e=movie_current(*session_);e!=E::none)return {e,{}};
        if(auto e=video::validate_movie_present(p);e!=E::none)return {e,{}};
        auto lease=std::dynamic_pointer_cast<const MovieLease>(p.frame);
        if(!lease||lease->session!=session_||!lease->texture->object)return {E::stale_resource,{}};
        const auto dest=video::movie_destination(p,l);RECT client{};
        if(!dest||l.destination!=session_->destination||!GetClientRect(session_->window,&client)||client.right!=l.window_width||client.bottom!=l.window_height)return {E::invalid_layout,{}};
        if(!collect_video(*session_))return {E::fence_failed,{}};
        if(image_error("movie presentation incoming GL state"))return {E::present_failed,{}};
        try{
            auto work=std::make_shared<Work>();auto fence=std::make_shared<GlMovieFence>(p,session_,work);
            session_->work.push_back(work);work->source=lease;bool ok=false;
            {
                VideoGlState state(*session_);
                session_->bind_framebuffer(GL_DRAW_FRAMEBUFFER,0);glDrawBuffer(GL_BACK);
                // Ordinary native RGBA8 UNORM replacement, no second transfer
                // curve or device-alpha reinterpretation. Host owns SwapBuffers.
                glDisable(0x8db9); // GL_FRAMEBUFFER_SRGB, supported by sync-capable context
                glDisable(GL_SCISSOR_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
                video_quad(lease->texture->object,dest->width,dest->height,0,0,1,1,false,dest->x,l.window_height-dest->y-dest->height,p.filter==video::MovieFilter::linear);
                ok=!image_error("present native movie frame");
                work->sync=session_->fence(0x9117,0);glFlush();ok=ok&&work->sync&&!image_error("native movie completion fence");
            }
            if(image_error("restore native movie presentation state"))ok=false;
            return {ok?E::none:E::present_failed,std::move(fence)};
        }catch(const std::bad_alloc&){return {E::host_capacity,{}};}catch(const std::length_error&){return {E::host_capacity,{}};}
    }
    video::MovieFenceResult poll(const std::shared_ptr<video::MovieGpuFence>& f)override{
        using E=video::MovieGpuError;using S=video::MovieFenceState;
        if(auto e=movie_current(*session_);e!=E::none)return {S::failed,e};
        const auto fence=std::dynamic_pointer_cast<GlMovieFence>(f);
        if(!fence||fence->session!=session_||!fence->work->sync)return {S::failed,E::stale_resource};
        if(image_error("movie fence incoming GL state"))return {S::failed,E::fence_failed};
        const auto status=session_->wait(fence->work->sync,0,0);
        if(image_error("poll native movie completion"))return {S::failed,E::fence_failed};
        if(status==0x911a||status==0x911c)return {S::complete,E::none};if(status==0x911b)return {S::pending,E::none};
        return {S::failed,E::fence_failed};
    }
    void retire(std::shared_ptr<video::MovieGpuFence> f)noexcept override{f.reset();}
    ~GlMovieBackend(){
        if(!session_->cleaned&&shutdown()!=video::MovieGpuError::none)
            std::fprintf(stderr,"[native-movie-gl] explicit owner-current shutdown required; caller context still owns unreleased GL objects\n");
    }
    video::MovieGpuError shutdown()noexcept override{
        session_->revoked.store(true,std::memory_order_release);
        if(session_->cleaned)return video::MovieGpuError::none;
        if(std::this_thread::get_id()!=session_->owner||wglGetCurrentContext()!=session_->context||wglGetCurrentDC()!=session_->dc)return video::MovieGpuError::wrong_thread;
        // Deleting a GLsync/texture after queued commands is ordered by GL;
        // no invented completion or caller-context destruction is necessary.
        for(auto& w:session_->work){if(w->sync)session_->erase(w->sync);w->sync=nullptr;w->source.reset();}
        session_->work.clear();for(auto& t:session_->textures){if(t->object)glDeleteTextures(1,&t->object);t->object=0;}
        session_->textures.clear();session_->cleaned=true;
        return glGetError()==GL_NO_ERROR?video::MovieGpuError::none:video::MovieGpuError::fence_failed;
    }
private:std::shared_ptr<Session> session_;MovieBindBuffer bind_;
};
} // namespace
#endif
std::shared_ptr<NativeMovieGlDevice> NativeMovieGlDevice::attach_current_context(void* host_window,std::uint64_t destination,MovieGpuError& error){
    error=MovieGpuError::unavailable;
#ifdef _WIN32
    auto window=static_cast<HWND>(host_window);auto dc=wglGetCurrentDC();auto context=wglGetCurrentContext();RECT client{};
    if(!window||!destination||!dc||!context||WindowFromDC(dc)!=window||!GetClientRect(window,&client)||client.right<=0||client.bottom<=0)return {};
    try{auto s=std::make_shared<Session>();s->window=window;s->dc=dc;s->context=context;s->destination=destination;s->owner=std::this_thread::get_id();
        s->fence=load<decltype(s->fence)>("glFenceSync");s->wait=load<decltype(s->wait)>("glClientWaitSync");s->erase=load<decltype(s->erase)>("glDeleteSync");
        s->bind_framebuffer=load<decltype(s->bind_framebuffer)>("glBindFramebuffer");s->active=load<decltype(s->active)>("glActiveTexture");s->use=load<decltype(s->use)>("glUseProgram");
        auto bind=load<MovieBindBuffer>("glBindBuffer");
        if(!s->fence||!s->wait||!s->erase||!s->bind_framebuffer||!s->active||!s->use||!bind)return {};
        // Queries/function loading only: no framebuffer, viewport, clear or
        // fixed geometry state is installed during attachment.
        auto device=std::make_shared<GlMovieBackend>(std::move(s),bind);error=MovieGpuError::none;return device;
    }catch(const std::bad_alloc&){error=MovieGpuError::host_capacity;}catch(const std::length_error&){error=MovieGpuError::host_capacity;}
#else
    (void)host_window;(void)destination;
#endif
    return {};
}
} // namespace mka::video
