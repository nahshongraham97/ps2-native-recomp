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
struct Texture {GLuint object=0;bool revoked=false;};
bool image_error(const char* operation);
struct GlNativeFence final : MovieNativeFence { Sync sync = nullptr; };
struct GlFencePort final : NativeMovieFencePort {
    HGLRC context = nullptr; HDC dc = nullptr; std::thread::id owner;
    Sync(APIENTRY* fence)(GLenum, GLbitfield) = nullptr;
    GLenum(APIENTRY* wait)(Sync, GLbitfield, std::uint64_t) = nullptr;
    void(APIENTRY* erase)(Sync) = nullptr;
    MovieGpuError current() const noexcept override {
        return std::this_thread::get_id() == owner && wglGetCurrentContext() == context && wglGetCurrentDC() == dc
            ? MovieGpuError::none : MovieGpuError::wrong_thread;
    }
    MovieNativeFencePlacement insert_after_commands() override {
        if (auto error = current(); error != MovieGpuError::none) return {error, {}};
        if (image_error("native completion incoming state")) return {MovieGpuError::fence_failed, {}};
        auto placed = std::make_shared<GlNativeFence>(); // Allocate BEFORE native submission.
        placed->sync = fence(0x9117, 0); glFlush();
        const bool bad = image_error("native context completion fence");
        return {placed->sync && !bad ? MovieGpuError::none : MovieGpuError::fence_failed, std::move(placed)};
    }
    MovieFenceResult poll(const std::shared_ptr<MovieNativeFence>& opaque) noexcept override {
        if (auto error = current(); error != MovieGpuError::none) return {MovieFenceState::failed, error};
        auto native = std::dynamic_pointer_cast<GlNativeFence>(opaque);
        if (!native || !native->sync) return {MovieFenceState::failed, MovieGpuError::stale_resource};
        if (image_error("native completion poll incoming state")) return {MovieFenceState::failed, MovieGpuError::fence_failed};
        const auto status = wait(native->sync, 0, 0);
        if (image_error("native completion poll")) return {MovieFenceState::failed, MovieGpuError::fence_failed};
        if (status == 0x911a || status == 0x911c) return {MovieFenceState::complete, MovieGpuError::none};
        if (status == 0x911b) return {MovieFenceState::pending, MovieGpuError::none};
        return {MovieFenceState::failed, MovieGpuError::fence_failed};
    }
    MovieGpuError release(std::shared_ptr<MovieNativeFence>& opaque) noexcept override {
        if (auto error = current(); error != MovieGpuError::none) return error;
        auto native = std::dynamic_pointer_cast<GlNativeFence>(opaque);
        if (!native) return MovieGpuError::stale_resource;
        if (image_error("native completion release incoming state")) return MovieGpuError::fence_failed;
        if (native->sync) {
            erase(native->sync);
            if (image_error("native completion release")) return MovieGpuError::fence_failed;
            native->sync = nullptr;
        }
        opaque.reset(); return MovieGpuError::none;
    }
};
struct Session : NativeMovieGpuRetirementTask {
    HGLRC context=nullptr;HDC dc=nullptr;HWND window=nullptr;std::thread::id owner;
    std::atomic<bool> revoked{false};bool cleaned=false;std::uint64_t destination=0,identity=0;
    // A failed deletion leaves the identity of the retained GL name unknown.
    // Do not retry deletion or resume this Session without host-context recovery.
    bool image_delete_outcome_unknown=false;
    Sync(APIENTRY* fence)(GLenum,GLbitfield)=nullptr;
    GLenum(APIENTRY* wait)(Sync,GLbitfield,std::uint64_t)=nullptr;
    void(APIENTRY* erase)(Sync)=nullptr;
    void(APIENTRY* bind_framebuffer)(GLenum,GLuint)=nullptr;
    void(APIENTRY* active)(GLenum)=nullptr;
    void(APIENTRY* use)(GLuint)=nullptr;
    std::vector<std::shared_ptr<Texture>> textures;
    std::shared_ptr<NativeMovieGpuWorkOwner> completion;
    MovieFenceResult poll_retirement() noexcept override;
    MovieFenceResult retry_retirement() noexcept override;
};
bool image_error(const char* operation){auto error=glGetError();if(error==GL_NO_ERROR)return false;std::fprintf(stderr,"[native-movie-gl] %s GL error=0x%X\n",operation,unsigned(error));return true;}
bool collect_video(Session& s){
    if (s.completion->collect() != MovieGpuError::none) return false;
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
    std::weak_ptr<Session> session;std::shared_ptr<Texture> texture;
    video::MovieGpuMetadata info;std::uint32_t w=0,h=0;std::uint64_t id=0;
    std::uint64_t identity()const noexcept override{return id;}
    std::uint32_t width()const noexcept override{return w;}
    std::uint32_t height()const noexcept override{return h;}
    const video::MovieGpuMetadata& metadata()const noexcept override{return info;}
};
struct GlMovieFence final:video::MovieGpuFence {
    std::shared_ptr<Session> session;std::shared_ptr<MovieGpuWorkReceipt> work;
    GlMovieFence(video::NativeMoviePresentPlan p,std::shared_ptr<Session> s)
        :MovieGpuFence(std::move(p)),session(std::move(s)){}
    ~GlMovieFence(){if(work)work->retire();}
};
video::MovieGpuError movie_current(const Session& s){
    if(s.revoked.load(std::memory_order_acquire))return video::MovieGpuError::stale_resource;
    if(std::this_thread::get_id()!=s.owner||wglGetCurrentContext()!=s.context||wglGetCurrentDC()!=s.dc)return video::MovieGpuError::wrong_thread;
    if(s.image_delete_outcome_unknown)return video::MovieGpuError::fence_failed;
    return video::MovieGpuError::none;
}
graphics::NativeTextureError texture_error(MovieGpuError e)noexcept{
 using T=graphics::NativeTextureError;switch(e){
 case MovieGpuError::none:return T::none;case MovieGpuError::wrong_thread:return T::wrong_thread;
 case MovieGpuError::stale_resource:return T::stale_resource;case MovieGpuError::host_capacity:return T::host_capacity;
 case MovieGpuError::admission_closed:return T::admission_closed;case MovieGpuError::upload_failed:return T::upload_failed;
 case MovieGpuError::invalid_frame:return T::invalid_image;default:return T::completion_failed;}
}
struct ImageLease final:graphics::NativeGlTexture {
 std::weak_ptr<Session> session;std::shared_ptr<Texture> texture;std::uint32_t w{},h{};std::uint64_t id{};
 std::uint64_t identity()const noexcept override{return id;}
 std::uint32_t width()const noexcept override{return w;}
 std::uint32_t height()const noexcept override{return h;}
 std::optional<std::uint32_t> gl_object(graphics::NativeTextureFailure& f)const noexcept override{
  f={};auto owner=session.lock();if(!owner){f={graphics::NativeTextureError::stale_resource,"native GL image context retired"};return {};}
  if(auto e=movie_current(*owner);e!=MovieGpuError::none){f={texture_error(e),"native GL image owner/context admission"};return {};}
  if(auto e=owner->completion->admission();e!=MovieGpuError::none){f={texture_error(e),"native GL image command admission"};return {};}
  if(!texture||texture->revoked||!texture->object){f={graphics::NativeTextureError::stale_resource,"native GL image released"};return {};}
  return texture->object;
 }
};
bool valid_sampler(graphics::NativeTextureSampler p)noexcept{
 using F=graphics::NativeTextureFilter;using A=graphics::NativeTextureAddress;
 return (p.filter==F::nearest||p.filter==F::linear)&&(p.u==A::repeat||p.u==A::clamp_to_edge)&&(p.v==A::repeat||p.v==A::clamp_to_edge);
}
void image_sampler(graphics::NativeTextureSampler p){
 using F=graphics::NativeTextureFilter;using A=graphics::NativeTextureAddress;
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,p.filter==F::nearest?GL_NEAREST:GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,p.filter==F::nearest?GL_NEAREST:GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,p.u==A::repeat?GL_REPEAT:GL_CLAMP_TO_EDGE);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,p.v==A::repeat?GL_REPEAT:GL_CLAMP_TO_EDGE);
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
// Common actual GL allocation path for movie and ordinary image resources.
// Caller-visible lease allocation precedes this function's command admission.
MovieGpuError validate_image_upload(Session& session,std::uint32_t width,std::uint32_t height,std::span<const std::uint8_t> rgba){
 using E=MovieGpuError;
 if(auto e=movie_current(session);e!=E::none)return e;
 if(auto e=session.completion->admission();e!=E::none)return e;
 if(!width||!height||!rgba.data())return E::invalid_frame;
 if(std::size_t(width)>std::size_t(-1)/4/height)return E::host_capacity;
 if(rgba.size()!=std::size_t(width)*height*4)return E::invalid_frame;
 if(!collect_video(session))return E::fence_failed;
 if(image_error("native image upload incoming GL state"))return E::upload_failed;
 GLint limit=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&limit);
 // Movie collection and incoming-error precedence are preserved even when a
 // representable input dimension exceeds the native GL signed dimension limit.
 if(limit<=0||width>unsigned(limit)||height>unsigned(limit)||width>INT_MAX||height>INT_MAX)return E::host_capacity;
 return E::none;
}
MovieGpuError store_image(Session& session,MovieBindBuffer bind,std::uint32_t width,std::uint32_t height,
 std::span<const std::uint8_t> rgba,graphics::NativeTextureSampler sampler,std::shared_ptr<Texture>& result){
 using E=MovieGpuError;
 auto texture=std::make_shared<Texture>();session.textures.push_back(texture);
 if(auto e=session.completion->commands_issued();e!=E::none)return e;
 bool ok=false;{
  VideoGlState state(session);MovieUnpackState unpack(bind);
  glGenTextures(1,&texture->object);glBindTexture(GL_TEXTURE_2D,texture->object);image_sampler(sampler);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,GLsizei(width),GLsizei(height),0,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
  ok=texture->object&&!image_error("upload native RGBA8 image");
 }
 if(!ok||image_error("restore native image upload state"))return E::upload_failed;
 result=std::move(texture);return E::none;
}
MovieFenceResult Session::poll_retirement() noexcept {
    revoked.store(true,std::memory_order_release);
    if(std::this_thread::get_id()!=owner||wglGetCurrentContext()!=context||wglGetCurrentDC()!=dc)
        return {MovieFenceState::failed,MovieGpuError::wrong_thread};
    if(cleaned)return {MovieFenceState::complete,MovieGpuError::none};
    if(image_delete_outcome_unknown)return {MovieFenceState::failed,MovieGpuError::fence_failed};
    auto tail=completion->begin_drain();
    if(tail.result.state==MovieFenceState::failed)return tail.result;
    auto done=completion->poll_drain(tail.receipt);
    if(done.state!=MovieFenceState::complete)return done;
    if(image_error("native movie cleanup incoming state"))return {MovieFenceState::failed,MovieGpuError::fence_failed};
    for(auto& texture:textures)if(texture->object){
        glDeleteTextures(1,&texture->object);
        if(image_error("native movie cleanup texture"))return {MovieFenceState::failed,MovieGpuError::fence_failed};
        texture->object=0;
    }
    textures.clear();cleaned=true;return {MovieFenceState::complete,MovieGpuError::none};
}
MovieFenceResult Session::retry_retirement() noexcept {
    if(std::this_thread::get_id()!=owner||wglGetCurrentContext()!=context||wglGetCurrentDC()!=dc)
        return {MovieFenceState::failed,MovieGpuError::wrong_thread};
    if(cleaned)return {MovieFenceState::complete,MovieGpuError::none};
    if(image_delete_outcome_unknown)return {MovieFenceState::failed,MovieGpuError::fence_failed};
    auto tail=completion->begin_drain();
    if(tail.result.state==MovieFenceState::failed && tail.receipt) {
        auto retry=completion->retry_drain(tail.receipt);
        if(retry.result.state==MovieFenceState::failed)return retry.result;
    }
    return poll_retirement();
}
class GlMovieBackend final:public video::NativeMovieGlDevice {
public:
    GlMovieBackend(std::shared_ptr<Session> s,MovieBindBuffer b):session_(std::move(s)),bind_(b){}
    MovieGpuRetirementRegistration retirement;
    video::MovieGpuUpload upload(const video::MovieRgbaFrame& f)override{
        using E=video::MovieGpuError;
        if(auto e=movie_current(*session_);e!=E::none)return {e,{}};
        if(auto e=session_->completion->admission();e!=E::none)return {e,{}};
        if(auto e=video::validate_movie_upload(f);e!=E::none)return {e,{}};
        if(auto e=validate_image_upload(*session_,f.width,f.height,f.rgba);e!=E::none)return {e,{}};
        try{
            auto lease=std::make_shared<MovieLease>();std::shared_ptr<Texture> texture;
            const graphics::NativeTextureSampler sampler{graphics::NativeTextureFilter::nearest,
                graphics::NativeTextureAddress::clamp_to_edge,graphics::NativeTextureAddress::clamp_to_edge};
            if(auto e=store_image(*session_,bind_,f.width,f.height,f.rgba,sampler,texture);e!=E::none)return {e,{}};
            if(session_->identity==UINT64_MAX)return {E::host_capacity,{}};auto id=session_->identity++;
            lease->session=session_;lease->texture=std::move(texture);lease->id=id+1;lease->w=f.width;lease->h=f.height;
            lease->info={f.conversion_role,f.source_pts,f.timestamp,f.source_sample_aspect_numerator,f.source_sample_aspect_denominator,
                f.source_color_range,f.source_color_space,f.source_color_primaries,f.source_color_transfer,f.codec_timing};
            return {E::none,std::move(lease)};
        }catch(const std::bad_alloc&){return {E::host_capacity,{}};}catch(const std::length_error&){return {E::host_capacity,{}};}
    }
    graphics::NativeTextureUpload upload_rgba_image(const graphics::NativeTextureRgbaImage& input,graphics::NativeTextureSampler sampler)override{
        using T=graphics::NativeTextureError;
        if(auto e=movie_current(*session_);e!=MovieGpuError::none)return {{texture_error(e),"native image owner/context admission"},{}};
        if(auto e=session_->completion->admission();e!=MovieGpuError::none)return {{texture_error(e),"native image command admission"},{}};
        if(!valid_sampler(sampler))return {{T::invalid_sampler,"explicit native image sampler required"},{}};
        if(auto e=validate_image_upload(*session_,input.width,input.height,input.bytes);e!=MovieGpuError::none)
            return {{texture_error(e),"actual native GL image validation"},{}};
        try{
            auto lease=std::make_shared<ImageLease>();std::shared_ptr<Texture> texture;
            if(auto e=store_image(*session_,bind_,input.width,input.height,input.bytes,sampler,texture);e!=MovieGpuError::none)
                return {{texture_error(e),"actual native GL image upload"},{}};
            if(session_->identity==UINT64_MAX)return {{T::host_capacity,"native GL image identity capacity"},{}};
            lease->session=session_;lease->texture=std::move(texture);lease->w=input.width;lease->h=input.height;lease->id=++session_->identity;
            return {{},std::move(lease)};
        }catch(const std::bad_alloc&){return {{T::allocation,"native GL image ownership allocation"},{}};}
         catch(const std::length_error&){return {{T::host_capacity,"native GL image ownership capacity"},{}};}
    }
    bool revoke_image(const std::shared_ptr<const graphics::NativeGlTexture>& image,
        graphics::NativeTextureFailure& failure)override{
        using T=graphics::NativeTextureError;failure={};
        if(auto e=movie_current(*session_);e!=MovieGpuError::none){failure={texture_error(e),"native GL image revoke context"};return false;}
        if(auto e=session_->completion->admission();e!=MovieGpuError::none){failure={texture_error(e),"native GL image revoke admission"};return false;}
        const auto lease=std::dynamic_pointer_cast<const ImageLease>(image);
        if(!lease||lease->session.lock()!=session_||!lease->texture){failure={T::stale_resource,"foreign native GL image revoke"};return false;}
        auto& texture=*lease->texture;
        if(!texture.object)return true;
        if(image_error("native image revoke incoming state")){failure={T::completion_failed,"native GL image revoke incoming error"};return false;}
        if(auto e=session_->completion->commands_issued();e!=MovieGpuError::none){failure={texture_error(e),"native GL image revoke command admission"};return false;}
        texture.revoked=true;
        // Do not restore a deleted texture binding: binding its old name would
        // recreate storage. Native GL retains queued uses until driver completion.
        glDeleteTextures(1,&texture.object);
        if(image_error("native image revoke delete")){
            session_->image_delete_outcome_unknown=true;
            failure={T::completion_failed,"native GL delete outcome unknown; required host context recovery"};
            return false;
        }
        texture.object=0;return true;
    }
    bool set_image_sampler(const std::shared_ptr<const graphics::NativeGlTexture>& image,
        graphics::NativeTextureSampler sampler,graphics::NativeTextureFailure& failure)override{
        using T=graphics::NativeTextureError;failure={};
        if(auto e=movie_current(*session_);e!=MovieGpuError::none){failure={texture_error(e),"native GL sampler context"};return false;}
        if(auto e=session_->completion->admission();e!=MovieGpuError::none){failure={texture_error(e),"native GL sampler admission"};return false;}
        if(!valid_sampler(sampler)){failure={T::invalid_sampler,"explicit native image sampler required"};return false;}
        const auto lease=std::dynamic_pointer_cast<const ImageLease>(image);
        if(!lease||lease->session.lock()!=session_||!lease->texture||lease->texture->revoked||!lease->texture->object){failure={T::stale_resource,"foreign or retired native GL image"};return false;}
        if(image_error("native image sampler incoming state")){failure={T::sampler_failed,"native GL sampler incoming error"};return false;}
        if(auto e=session_->completion->commands_issued();e!=MovieGpuError::none){failure={texture_error(e),"native GL sampler command admission"};return false;}
        {VideoGlState state(*session_);glBindTexture(GL_TEXTURE_2D,lease->texture->object);image_sampler(sampler);}
        if(image_error("native image sampler update/state restore")){failure={T::sampler_failed,"native GL sampler possible partial update"};return false;}return true;
    }
    video::MovieGpuSubmission present(const video::NativeMoviePresentPlan& p,const video::MovieHostLayout& l)override{
        using E=video::MovieGpuError;
        if(auto e=movie_current(*session_);e!=E::none)return {e,{}};
        if(auto e=session_->completion->admission();e!=E::none)return {e,{}};
        if(auto e=video::validate_movie_present(p);e!=E::none)return {e,{}};
        auto lease=std::dynamic_pointer_cast<const MovieLease>(p.frame);
        if(!lease||lease->session.lock()!=session_||!lease->texture->object)return {E::stale_resource,{}};
        const auto dest=video::movie_destination(p,l);RECT client{};
        if(!dest||l.destination!=session_->destination||!GetClientRect(session_->window,&client)||client.right!=l.window_width||client.bottom!=l.window_height)return {E::invalid_layout,{}};
        if(!collect_video(*session_))return {E::fence_failed,{}};
        if(image_error("movie presentation incoming GL state"))return {E::present_failed,{}};
        try{
            // All caller-visible wrapper allocation precedes work admission.
            auto fence=std::make_shared<GlMovieFence>(p,session_);
            auto admitted=session_->completion->admit(lease);
            if(admitted.error!=E::none)return {admitted.error,{}};
            fence->work=std::move(admitted.receipt);bool ok=false;
            {
                VideoGlState state(*session_);
                session_->bind_framebuffer(GL_DRAW_FRAMEBUFFER,0);glDrawBuffer(GL_BACK);
                // Ordinary native RGBA8 UNORM replacement, no second transfer
                // curve or device-alpha reinterpretation. Host owns SwapBuffers.
                glDisable(0x8db9); // GL_FRAMEBUFFER_SRGB, supported by sync-capable context
                glDisable(GL_SCISSOR_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
                video_quad(lease->texture->object,dest->width,dest->height,0,0,1,1,false,dest->x,l.window_height-dest->y-dest->height,p.filter==video::MovieFilter::linear);
                ok=!image_error("present native movie frame");

            }
            if(image_error("restore native movie presentation state"))ok=false;
            const auto submitted=session_->completion->finish_submission(fence->work,ok?E::none:E::present_failed);
            return {submitted,std::move(fence)};
        }catch(const std::bad_alloc&){return {E::host_capacity,{}};}catch(const std::length_error&){return {E::host_capacity,{}};}
    }
    video::MovieFenceResult poll(const std::shared_ptr<video::MovieGpuFence>& f)override{
        using E=video::MovieGpuError;using S=video::MovieFenceState;
        if(auto e=movie_current(*session_);e!=E::none)return {S::failed,e};
        const auto fence=std::dynamic_pointer_cast<GlMovieFence>(f);
        if(!fence||fence->session!=session_||!fence->work)return {S::failed,E::stale_resource};
        return session_->completion->poll(fence->work);
    }
    void retire(std::shared_ptr<video::MovieGpuFence> f)noexcept override{f.reset();}
    ~GlMovieBackend(){
        session_->revoked.store(true,std::memory_order_release);
        if(retirement)retirement.retire(); // Registered before any GPU operation; no allocation here.
    }
    MovieGpuDrain begin_context_drain() noexcept override {
        if(std::this_thread::get_id()!=session_->owner||wglGetCurrentContext()!=session_->context||wglGetCurrentDC()!=session_->dc)
            return {{MovieFenceState::failed,MovieGpuError::wrong_thread},{}};
        if(session_->cleaned)return {{MovieFenceState::failed,MovieGpuError::stale_resource},{}};
        if(session_->image_delete_outcome_unknown)return {{MovieFenceState::failed,MovieGpuError::fence_failed},{}};
        return session_->completion->begin_drain();
    }
    MovieFenceResult poll_context_drain(const std::shared_ptr<MovieGpuDrainReceipt>& receipt) noexcept override {
        if(std::this_thread::get_id()!=session_->owner||wglGetCurrentContext()!=session_->context||wglGetCurrentDC()!=session_->dc)
            return {MovieFenceState::failed,MovieGpuError::wrong_thread};
        if(session_->cleaned)return {MovieFenceState::failed,MovieGpuError::stale_resource};
        if(session_->image_delete_outcome_unknown)return {MovieFenceState::failed,MovieGpuError::fence_failed};
        return session_->completion->poll_drain(receipt);
    }
    MovieGpuDrain retry_context_drain(const std::shared_ptr<MovieGpuDrainReceipt>& receipt) noexcept override {
        if(std::this_thread::get_id()!=session_->owner||wglGetCurrentContext()!=session_->context||wglGetCurrentDC()!=session_->dc)
            return {{MovieFenceState::failed,MovieGpuError::wrong_thread},{}};
        if(session_->cleaned)return {{MovieFenceState::failed,MovieGpuError::stale_resource},{}};
        if(session_->image_delete_outcome_unknown)return {{MovieFenceState::failed,MovieGpuError::fence_failed},{}};
        return session_->completion->retry_drain(receipt);
    }
    MovieGpuError resume_after_drain(const std::shared_ptr<MovieGpuDrainReceipt>& receipt) noexcept override {
        if(session_->revoked.load(std::memory_order_acquire))return MovieGpuError::stale_resource;
        if(std::this_thread::get_id()!=session_->owner||wglGetCurrentContext()!=session_->context||wglGetCurrentDC()!=session_->dc)
            return MovieGpuError::wrong_thread;
        if(session_->image_delete_outcome_unknown)return MovieGpuError::fence_failed;
        return session_->completion->resume(receipt);
    }
    video::MovieGpuError shutdown()noexcept override{
        session_->revoked.store(true,std::memory_order_release);
        const auto result=session_->poll_retirement();
        if(result.state==MovieFenceState::pending)return MovieGpuError::pending;
        if(result.state==MovieFenceState::failed)return result.error;
        if(retirement)retirement.retire();
        return MovieGpuError::none;
    }
private:std::shared_ptr<Session> session_;MovieBindBuffer bind_;
};
} // namespace
#endif
std::shared_ptr<NativeMovieGlDevice> NativeMovieGlDevice::attach_current_context(void* host_window,std::uint64_t destination,
    const std::shared_ptr<NativeMovieGpuRetirementOwner>& retirement,MovieGpuError& error){
    error=MovieGpuError::unavailable;
#ifdef _WIN32
    auto window=static_cast<HWND>(host_window);auto dc=wglGetCurrentDC();auto context=wglGetCurrentContext();RECT client{};
    if(!retirement||!window||!destination||!dc||!context||WindowFromDC(dc)!=window||!GetClientRect(window,&client)||client.right<=0||client.bottom<=0)return {};
    try{auto s=std::make_shared<Session>();s->window=window;s->dc=dc;s->context=context;s->destination=destination;s->owner=std::this_thread::get_id();
        s->fence=load<decltype(s->fence)>("glFenceSync");s->wait=load<decltype(s->wait)>("glClientWaitSync");s->erase=load<decltype(s->erase)>("glDeleteSync");
        s->bind_framebuffer=load<decltype(s->bind_framebuffer)>("glBindFramebuffer");s->active=load<decltype(s->active)>("glActiveTexture");s->use=load<decltype(s->use)>("glUseProgram");
        auto bind=load<MovieBindBuffer>("glBindBuffer");
        if(!s->fence||!s->wait||!s->erase||!s->bind_framebuffer||!s->active||!s->use||!bind)return {};
        auto port=std::make_shared<GlFencePort>();port->context=context;port->dc=dc;port->owner=s->owner;
        port->fence=s->fence;port->wait=s->wait;port->erase=s->erase;
        s->completion=NativeMovieGpuWorkOwner::create(std::move(port),error);if(!s->completion)return {};
        // Queries/loading and pre-registration only; no fence/draw/window mutation.
        auto device=std::make_shared<GlMovieBackend>(s,bind);
        auto registration=retirement->register_task(std::move(s),error);
        if(!registration)return {};
        device->retirement=MovieGpuRetirementRegistration(std::move(registration));
        error=MovieGpuError::none;return device;
    }catch(const std::bad_alloc&){error=MovieGpuError::host_capacity;}catch(const std::length_error&){error=MovieGpuError::host_capacity;}
#else
    (void)host_window;(void)destination;(void)retirement;
#endif
    return {};
}
} // namespace mka::video
