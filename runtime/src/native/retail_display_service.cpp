#include "mka/retail_display_service.hpp"
#include <atomic>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#undef near
#undef far
#include <GL/gl.h>
#endif

namespace mka::retail {

namespace {
std::atomic<std::uint64_t> next_display_identity{0x300000001ull};
std::uint64_t display_identity() noexcept {
    auto value=next_display_identity.load(std::memory_order_relaxed);
    while(value!=std::numeric_limits<std::uint64_t>::max()) {
        if(next_display_identity.compare_exchange_weak(value,value+1,std::memory_order_relaxed))return value;
    }
    return 0;
}
#ifdef _WIN32
template<class T>T draw_proc(const char* name) noexcept {
    auto p=wglGetProcAddress(name);
    if(!p||p==reinterpret_cast<PROC>(1)||p==reinterpret_cast<PROC>(2)||
       p==reinterpret_cast<PROC>(3)||p==reinterpret_cast<PROC>(-1))return nullptr;
    return reinterpret_cast<T>(p);
}
#endif
}

struct NativeOffscreenDisplayService::Impl {
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::uint32_t logical_width_ = NativeProductionDisplayService::LOGICAL_WIDTH;
    std::uint32_t logical_height_ = NativeProductionDisplayService::LOGICAL_HEIGHT;
    const std::thread::id owner_thread_{std::this_thread::get_id()};
    DisplayTelemetry telemetry_{};
    std::uint64_t destination_identity_ = display_identity();
    bool draw_active_{false},draw_ready_{false},draw_has_depth_{false};
    std::weak_ptr<const NativeDisplayDrawOwner> draw_owner_;
    DisplayViewportRect draw_viewport_;

#ifdef _WIN32
    HWND window_ = nullptr;
    HDC dc_ = nullptr;
    HGLRC context_ = nullptr;
    void(APIENTRY* bind_framebuffer_)(GLenum,GLuint)=nullptr;
    GLenum(APIENTRY* framebuffer_status_)(GLenum)=nullptr;
    void(APIENTRY* color_mask_)(GLuint,GLboolean,GLboolean,GLboolean,GLboolean)=nullptr;
    void(APIENTRY* viewport_)(GLuint,GLfloat,GLfloat,GLfloat,GLfloat)=nullptr;
    void(APIENTRY* scissor_)(GLuint,GLint,GLint,GLsizei,GLsizei)=nullptr;
    void(APIENTRY* enable_)(GLenum,GLuint)=nullptr;
    void(APIENTRY* disable_)(GLenum,GLuint)=nullptr;
    GLboolean(APIENTRY* enabled_)(GLenum,GLuint)=nullptr;
    void(APIENTRY* get_boolean_)(GLenum,GLuint,GLboolean*)=nullptr;
    void(APIENTRY* get_integer_)(GLenum,GLuint,GLint*)=nullptr;
    void(APIENTRY* get_float_)(GLenum,GLuint,GLfloat*)=nullptr;
    void(APIENTRY* draw_buffers_)(GLsizei,const GLenum*)=nullptr;
    GLint prior_draw_framebuffer_{};
    std::vector<GLenum> prior_default_draw_buffers_;
    GLfloat prior_viewport_[4]{};
    bool default_buffer_snapshot_{false};

    bool current(DisplayServiceFailure& f) const noexcept {
        if(owner_thread_!=std::this_thread::get_id()) {
            f={DisplayServiceError::wrong_thread,"native display creator thread required"};return false;
        }
        if(!context_||!dc_||wglGetCurrentContext()!=context_||wglGetCurrentDC()!=dc_) {
            f={DisplayServiceError::context_activation_failed,"actual native display context is not current"};return false;
        }
        return true;
    }
    bool restore_draw(DisplayServiceFailure& f) noexcept {
        if(!current(f))return false;
        if(glGetError()!=GL_NO_ERROR) {
            f={DisplayServiceError::native_operation_failed,"incoming GPU error before native target restoration"};return false;
        }
        if(default_buffer_snapshot_) {
            bind_framebuffer_(0x8CA9 /*DRAW_FRAMEBUFFER*/,0);
            auto count=prior_default_draw_buffers_.size();
            while(count>1&&prior_default_draw_buffers_[count-1]==GL_NONE)--count;
            if(count==1)glDrawBuffer(prior_default_draw_buffers_[0]);
            else draw_buffers_(static_cast<GLsizei>(count),prior_default_draw_buffers_.data());
        }
        bind_framebuffer_(0x8CA9,static_cast<GLuint>(prior_draw_framebuffer_));
        viewport_(0,prior_viewport_[0],prior_viewport_[1],prior_viewport_[2],prior_viewport_[3]);
        if(glGetError()!=GL_NO_ERROR) {
            f={DisplayServiceError::native_operation_failed,"native target restoration failed; retain host pass for recovery"};return false;
        }
        draw_active_=false;draw_ready_=false;draw_owner_.reset();default_buffer_snapshot_=false;return true;
    }
#endif

    Impl(std::uint32_t width, std::uint32_t height, std::uint32_t logical_w, std::uint32_t logical_h)
        : width_(width), height_(height), logical_width_(logical_w), logical_height_(logical_h) {}


    ~Impl() {
#ifdef _WIN32
        if (wglGetCurrentContext() == context_) {
            wglMakeCurrent(nullptr, nullptr);
        }
        if (context_) {
            wglDeleteContext(context_);
            context_ = nullptr;
        }
        if (dc_) {
            ReleaseDC(window_, dc_);
            dc_ = nullptr;
        }
        if (window_) {
            DestroyWindow(window_);
            window_ = nullptr;
        }
#endif
    }
};

std::shared_ptr<NativeOffscreenDisplayService> NativeOffscreenDisplayService::create(
    std::uint32_t width, std::uint32_t height, DisplayServiceFailure& failure,
    std::uint32_t logical_w, std::uint32_t logical_h) {
    failure = {};
    if (width == 0 || height == 0 || logical_w == 0 || logical_h == 0) {
        failure = {DisplayServiceError::invalid_dimensions, "Target and logical display dimensions must be non-zero"};
        return nullptr;
    }
    if(width>static_cast<std::uint32_t>(std::numeric_limits<int>::max())||
       height>static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
        failure={DisplayServiceError::invalid_dimensions,"native window dimensions exceed signed host extent"};return nullptr;
    }

    try {
        auto impl = std::make_unique<Impl>(width, height, logical_w, logical_h);
        if(!impl->destination_identity_) {
            failure={DisplayServiceError::resource_busy,"native display identity capacity exhausted"};return nullptr;
        }

#ifdef _WIN32
        WNDCLASSW wc{};
        wc.style = CS_OWNDC;
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"MkaRetailOffscreenDisplayServiceClass";
        RegisterClassW(&wc);

        // Created disabled and offscreen without visible taskbar presence
        impl->window_ = CreateWindowExW(0, wc.lpszClassName, L"",
            WS_POPUP | WS_DISABLED, 0, 0, width, height, nullptr, nullptr, wc.hInstance, nullptr);
        if (!impl->window_) {
            failure = {DisplayServiceError::context_creation_failed, "Failed to create offscreen host window handle"};
            return nullptr;
        }
        RECT client{};
        if(!GetClientRect(impl->window_,&client)||client.right!=static_cast<LONG>(width)||
           client.bottom!=static_cast<LONG>(height)) {
            failure={DisplayServiceError::invalid_dimensions,"native drawable extent differs from requested physical extent"};return nullptr;
        }

        impl->dc_ = GetDC(impl->window_);
        if (!impl->dc_) {
            failure = {DisplayServiceError::context_creation_failed, "Failed to acquire device context for offscreen target"};
            return nullptr;
        }

        PIXELFORMATDESCRIPTOR pfd{};
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cAlphaBits = 8;
        pfd.cDepthBits = 24;
        pfd.iLayerType = PFD_MAIN_PLANE;

        int format = ChoosePixelFormat(impl->dc_, &pfd);
        if (!format || !SetPixelFormat(impl->dc_, format, &pfd)) {
            failure = {DisplayServiceError::context_creation_failed, "Failed to select matching double-buffered pixel format"};
            return nullptr;
        }

        impl->context_ = wglCreateContext(impl->dc_);
        if (!impl->context_) {
            failure = {DisplayServiceError::context_creation_failed, "Failed to create OpenGL context for offscreen display service"};
            return nullptr;
        }
#endif
        return std::shared_ptr<NativeOffscreenDisplayService>(new NativeOffscreenDisplayService(std::move(impl)));
    } catch (const std::exception& e) {
        failure = {DisplayServiceError::context_creation_failed, e.what()};
        return nullptr;
    }
}

NativeOffscreenDisplayService::NativeOffscreenDisplayService(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}

NativeOffscreenDisplayService::~NativeOffscreenDisplayService() noexcept = default;

bool NativeOffscreenDisplayService::acquire_context(DisplayServiceFailure& failure) noexcept {
    failure = {};
#ifdef _WIN32
    if(impl_&&impl_->owner_thread_!=std::this_thread::get_id()) {
        failure={DisplayServiceError::wrong_thread,"native display cannot migrate from its creator thread"};return false;
    }
    if (!impl_ || !impl_->context_ || !impl_->dc_) {
        failure = {DisplayServiceError::stale_device, "Display service is in an uninitialized or stale state"};
        return false;
    }
    if (!wglMakeCurrent(impl_->dc_, impl_->context_)) {
        failure = {DisplayServiceError::context_activation_failed, "wglMakeCurrent failed to bind offscreen context"};
        return false;
    }
    return true;
#else
    failure = {DisplayServiceError::context_activation_failed, "Offscreen display service requires Windows OpenGL"};
    return false;
#endif
}

bool NativeOffscreenDisplayService::release_context(DisplayServiceFailure& failure) noexcept {
    failure = {};
#ifdef _WIN32
    if (!impl_) {failure={DisplayServiceError::stale_device,"missing native display"};return false;}
    if (impl_->owner_thread_ != std::this_thread::get_id()) {
        failure = {DisplayServiceError::wrong_thread, "Cannot release context from a non-owning thread"};
        return false;
    }
    if(impl_->draw_active_) {
        failure={DisplayServiceError::resource_busy,"end or explicitly restore native draw pass before releasing context"};return false;
    }
    if (wglGetCurrentContext() == impl_->context_) {
        if(!wglMakeCurrent(nullptr, nullptr)) {
            failure={DisplayServiceError::context_activation_failed,"native context release failed"};return false;
        }
    }
    return true;
#else
    return true;
#endif
}

bool NativeOffscreenDisplayService::is_context_current() const noexcept {
#ifdef _WIN32
    return impl_ && impl_->owner_thread_==std::this_thread::get_id() && impl_->context_ &&
        wglGetCurrentContext()==impl_->context_ && wglGetCurrentDC()==impl_->dc_;
#else
    return false;
#endif
}

void* NativeOffscreenDisplayService::native_display_handle() const noexcept {
#ifdef _WIN32
    return impl_ ? impl_->window_ : nullptr;
#else
    return nullptr;
#endif
}

std::uint64_t NativeOffscreenDisplayService::destination_identity() const noexcept {
    return impl_ ? impl_->destination_identity_ : 0;
}

bool NativeOffscreenDisplayService::begin_render_target(
    const std::shared_ptr<const NativeDisplayDrawOwner>& owner, DisplayViewportRect viewport,
    bool require_depth, DisplayServiceFailure& f) noexcept {
    f={};
#ifdef _WIN32
    if(!impl_){f={DisplayServiceError::stale_device,"native display missing"};return false;}
    auto& s=*impl_;
    if(!s.current(f))return false;
    if(!owner){f={DisplayServiceError::target_owner,"live native draw owner required"};return false;}
    if(s.draw_active_){f={DisplayServiceError::resource_busy,"native target already owned; explicit end/recovery required"};return false;}
    if(!viewport.width||!viewport.height||viewport.x>s.width_||viewport.y>s.height_||
       viewport.width>s.width_-viewport.x||viewport.height>s.height_-viewport.y) {
        f={DisplayServiceError::invalid_dimensions,"physical lower-left viewport lies outside native target"};return false;
    }
    s.bind_framebuffer_=draw_proc<decltype(s.bind_framebuffer_)>("glBindFramebuffer");
    s.framebuffer_status_=draw_proc<decltype(s.framebuffer_status_)>("glCheckFramebufferStatus");
    s.color_mask_=draw_proc<decltype(s.color_mask_)>("glColorMaski");
    s.viewport_=draw_proc<decltype(s.viewport_)>("glViewportIndexedf");
    s.scissor_=draw_proc<decltype(s.scissor_)>("glScissorIndexed");
    s.enable_=draw_proc<decltype(s.enable_)>("glEnablei");
    s.disable_=draw_proc<decltype(s.disable_)>("glDisablei");
    s.enabled_=draw_proc<decltype(s.enabled_)>("glIsEnabledi");
    s.get_boolean_=draw_proc<decltype(s.get_boolean_)>("glGetBooleani_v");
    s.get_integer_=draw_proc<decltype(s.get_integer_)>("glGetIntegeri_v");
    s.get_float_=draw_proc<decltype(s.get_float_)>("glGetFloati_v");
    s.draw_buffers_=draw_proc<decltype(s.draw_buffers_)>("glDrawBuffers");
    if(!s.bind_framebuffer_||!s.framebuffer_status_||!s.color_mask_||!s.viewport_||!s.scissor_||
       !s.enable_||!s.disable_||!s.enabled_||!s.get_boolean_||!s.get_integer_||!s.get_float_||!s.draw_buffers_) {
        f={DisplayServiceError::unsupported_target,"native framebuffer/indexed state operations unavailable"};return false;
    }
    if(glGetError()!=GL_NO_ERROR) {
        f={DisplayServiceError::native_operation_failed,"incoming GPU error before target begin"};return false;
    }
    GLint draw_count{};glGetIntegerv(0x8824 /*MAX_DRAW_BUFFERS*/,&draw_count);
    if(glGetError()!=GL_NO_ERROR||draw_count<=0) {
        f={DisplayServiceError::unsupported_target,"actual native draw-buffer count unavailable"};return false;
    }
    try{s.prior_default_draw_buffers_.assign(static_cast<std::size_t>(draw_count),GL_NONE);}
    catch(...){f={DisplayServiceError::resource_busy,"native draw-state ownership allocation failed before GPU mutation"};return false;}
    glGetIntegerv(0x8CA6 /*DRAW_FRAMEBUFFER_BINDING*/,&s.prior_draw_framebuffer_);
    s.get_float_(GL_VIEWPORT,0,s.prior_viewport_);
    if(glGetError()!=GL_NO_ERROR) {
        f={DisplayServiceError::native_operation_failed,"native target state snapshot failed"};return false;
    }
    // Retain an actual owner BEFORE the first GL mutation. A failed snapshot
    // or rollback must not leave changed target state without a recovery path.
    s.draw_active_=true;s.draw_ready_=false;s.draw_has_depth_=false;
    s.draw_owner_=owner;s.draw_viewport_=viewport;s.default_buffer_snapshot_=false;
    s.bind_framebuffer_(0x8CA9 /*DRAW_FRAMEBUFFER*/,0);
    for(GLint i=0;i<draw_count;++i) {
        GLint value{};glGetIntegerv(0x8825 /*DRAW_BUFFER0*/+i,&value);
        s.prior_default_draw_buffers_[static_cast<std::size_t>(i)]=static_cast<GLenum>(value);
    }
    if(glGetError()!=GL_NO_ERROR) {
        DisplayServiceFailure cleanup;
        if(!s.restore_draw(cleanup)){f=cleanup;return false;}
        f={DisplayServiceError::native_operation_failed,"native default target snapshot failed"};return false;
    }
    // From here the complete state needed by restoration is captured. The
    // pass stays owned on a restoration failure; no successful no-op fallback.
    s.default_buffer_snapshot_=true;
    GLint depth{},red{},green{},blue{};GLboolean doubled{};
    glGetIntegerv(GL_DEPTH_BITS,&depth);glGetIntegerv(GL_RED_BITS,&red);
    glGetIntegerv(GL_GREEN_BITS,&green);glGetIntegerv(GL_BLUE_BITS,&blue);
    glGetBooleanv(GL_DOUBLEBUFFER,&doubled);
    const auto status=s.framebuffer_status_(0x8CA9);
    const bool query_failed=glGetError()!=GL_NO_ERROR;
    if(query_failed||status!=0x8CD5 /*COMPLETE*/||!doubled||red<=0||green<=0||blue<=0||(require_depth&&depth<=0)) {
        DisplayServiceFailure cleanup;
        if(!s.restore_draw(cleanup)){f=cleanup;return false;}
        f={DisplayServiceError::unsupported_target,"actual native color/depth/default framebuffer capability refused"};return false;
    }
    s.draw_has_depth_=require_depth;
    glDrawBuffer(GL_BACK);
    s.viewport_(0,viewport.x,viewport.y,viewport.width,viewport.height);
    GLfloat actual_viewport[4]{};s.get_float_(GL_VIEWPORT,0,actual_viewport);
    if(glGetError()!=GL_NO_ERROR||actual_viewport[0]!=static_cast<float>(viewport.x)||
       actual_viewport[1]!=static_cast<float>(viewport.y)||actual_viewport[2]!=static_cast<float>(viewport.width)||
       actual_viewport[3]!=static_cast<float>(viewport.height)) {
        DisplayServiceFailure cleanup;
        if(!s.restore_draw(cleanup)){f=cleanup;return false;}
        f={DisplayServiceError::native_operation_failed,"native target begin failed"};return false;
    }
    s.draw_ready_=true;return true;
#else
    (void)owner;(void)viewport;(void)require_depth;
    f={DisplayServiceError::unsupported_target,"Windows native draw target required"};return false;
#endif
}

bool NativeOffscreenDisplayService::clear_render_target(const NativeDisplayDrawOwner& owner,
    const std::optional<std::array<float,4>>& color, std::optional<float> depth,
    DisplayServiceFailure& f) noexcept {
    f={};
#ifdef _WIN32
    if(!impl_){f={DisplayServiceError::stale_device,"native display missing"};return false;}
    auto& s=*impl_;if(!s.current(f))return false;
    if(!s.draw_active_||!s.draw_ready_||s.draw_owner_.lock().get()!=&owner) {
        f={DisplayServiceError::target_owner,"clear requires the actual active draw owner"};return false;
    }
    if(color)for(float x:*color)if(!std::isfinite(x)) {
        f={DisplayServiceError::invalid_clear,"finite native clear color required"};return false;
    }
    if(depth&&(!s.draw_has_depth_||!std::isfinite(*depth)||*depth<0||*depth>1)) {
        f={DisplayServiceError::invalid_clear,"explicit normalized depth value and admitted depth target required"};return false;
    }
    if(glGetError()!=GL_NO_ERROR) {
        f={DisplayServiceError::native_operation_failed,"incoming GPU error before native clear"};return false;
    }
    GLint target{},buffer{};glGetIntegerv(0x8CA6,&target);glGetIntegerv(GL_DRAW_BUFFER,&buffer);
    bool extra_target=false;
    for(std::size_t i=1;i<s.prior_default_draw_buffers_.size();++i) {
        GLint value{};glGetIntegerv(0x8825+static_cast<GLenum>(i),&value);extra_target|=value!=GL_NONE;
    }
    if(glGetError()!=GL_NO_ERROR||target!=0||buffer!=GL_BACK||extra_target) {
        f={DisplayServiceError::target_owner,"native draw target changed outside its active owner"};return false;
    }
    if(!color&&!depth)return true; // Explicit empty native clear, not a draw.
    GLfloat prior_color[4]{};GLdouble prior_depth{};GLboolean color_mask[4]{},depth_mask{};
    GLint scissor[4]{};const GLboolean scissor_enabled=s.enabled_(GL_SCISSOR_TEST,0),dither=glIsEnabled(GL_DITHER);
    const GLboolean discard=glIsEnabled(0x8C89 /*RASTERIZER_DISCARD*/),srgb=glIsEnabled(0x8DB9 /*FRAMEBUFFER_SRGB*/);
    glGetFloatv(GL_COLOR_CLEAR_VALUE,prior_color);glGetDoublev(GL_DEPTH_CLEAR_VALUE,&prior_depth);
    s.get_boolean_(GL_COLOR_WRITEMASK,0,color_mask);glGetBooleanv(GL_DEPTH_WRITEMASK,&depth_mask);
    s.get_integer_(GL_SCISSOR_BOX,0,scissor);
    if(glGetError()!=GL_NO_ERROR) {
        f={DisplayServiceError::native_operation_failed,"native clear state snapshot failed"};return false;
    }
    s.enable_(GL_SCISSOR_TEST,0);s.scissor_(0,s.draw_viewport_.x,s.draw_viewport_.y,s.draw_viewport_.width,s.draw_viewport_.height);
    glDisable(GL_DITHER);glDisable(0x8C89);glDisable(0x8DB9);GLbitfield mask{};
    if(color) {s.color_mask_(0,GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glClearColor((*color)[0],(*color)[1],(*color)[2],(*color)[3]);mask|=GL_COLOR_BUFFER_BIT;}
    if(depth) {glDepthMask(GL_TRUE);glClearDepth(*depth);mask|=GL_DEPTH_BUFFER_BIT;}
    glClear(mask);
    const bool clear_failed=glGetError()!=GL_NO_ERROR;
    glClearColor(prior_color[0],prior_color[1],prior_color[2],prior_color[3]);glClearDepth(prior_depth);
    s.color_mask_(0,color_mask[0],color_mask[1],color_mask[2],color_mask[3]);glDepthMask(depth_mask);
    s.scissor_(0,scissor[0],scissor[1],scissor[2],scissor[3]);
    if(scissor_enabled)s.enable_(GL_SCISSOR_TEST,0);else s.disable_(GL_SCISSOR_TEST,0);
    if(dither)glEnable(GL_DITHER);else glDisable(GL_DITHER);
    if(discard)glEnable(0x8C89);else glDisable(0x8C89);
    if(srgb)glEnable(0x8DB9);else glDisable(0x8DB9);
    const bool restore_failed=glGetError()!=GL_NO_ERROR;
    if(clear_failed||restore_failed) {
        f={DisplayServiceError::native_operation_failed,"native clear/restoration failed after command submission"};return false;
    }
    return true;
#else
    (void)owner;(void)color;(void)depth;
    f={DisplayServiceError::unsupported_target,"Windows native draw target required"};return false;
#endif
}

bool NativeOffscreenDisplayService::end_render_target(const NativeDisplayDrawOwner& owner,
    DisplayServiceFailure& f) noexcept {
    f={};
#ifdef _WIN32
    if(!impl_){f={DisplayServiceError::stale_device,"native display missing"};return false;}
    if(!impl_->current(f))return false;
    if(!impl_->draw_active_||impl_->draw_owner_.lock().get()!=&owner) {
        f={DisplayServiceError::target_owner,"end requires the actual active native draw owner"};return false;
    }
    return impl_->restore_draw(f);
#else
    (void)owner;f={DisplayServiceError::unsupported_target,"Windows native draw target required"};return false;
#endif
}

bool NativeOffscreenDisplayService::restore_abandoned_render_target(DisplayServiceFailure& f) noexcept {
    f={};
#ifdef _WIN32
    if(!impl_){f={DisplayServiceError::stale_device,"native display missing"};return false;}
    if(!impl_->current(f))return false;
    if(!impl_->draw_active_)return true;
    if(!impl_->draw_owner_.expired()) {
        f={DisplayServiceError::resource_busy,"live native draw owner must end its own pass"};return false;
    }
    return impl_->restore_draw(f);
#else
    f={DisplayServiceError::unsupported_target,"Windows native draw target required"};return false;
#endif
}

bool NativeOffscreenDisplayService::owns_render_target(const NativeDisplayDrawOwner& owner) const noexcept {
    return impl_&&impl_->owner_thread_==std::this_thread::get_id()&&impl_->draw_active_&&
        impl_->draw_owner_.lock().get()==&owner;
}

std::uint32_t NativeOffscreenDisplayService::logical_width() const noexcept {
    return impl_ ? impl_->logical_width_ : LOGICAL_WIDTH;
}

std::uint32_t NativeOffscreenDisplayService::logical_height() const noexcept {
    return impl_ ? impl_->logical_height_ : LOGICAL_HEIGHT;
}

DisplayViewportRect NativeOffscreenDisplayService::compute_viewport(
    std::uint32_t target_w, std::uint32_t target_h, DisplayAspectMode aspect_mode) const noexcept {
    // If target matches 16:9 Xbox 720p logical resolution and 16:9 aspect requested
    if (target_w == LOGICAL_WIDTH && target_h == LOGICAL_HEIGHT && aspect_mode == DisplayAspectMode::widescreen_16_9) {
        return DisplayViewportRect{0, 0, LOGICAL_WIDTH, LOGICAL_HEIGHT};
    }
    // If target matches PS2 640x448 (anamorphic 4:3 NTSC framebuffer) and 4:3 aspect requested
    if (target_w == PS2_LOGICAL_WIDTH && target_h == PS2_LOGICAL_HEIGHT && aspect_mode == DisplayAspectMode::standard_4_3) {
        return DisplayViewportRect{0, 0, PS2_LOGICAL_WIDTH, PS2_LOGICAL_HEIGHT};
    }
    double aspect = (aspect_mode == DisplayAspectMode::widescreen_16_9) ? (16.0 / 9.0) : (4.0 / 3.0);
    double target_aspect = (target_h > 0) ? (double(target_w) / double(target_h)) : aspect;
    DisplayViewportRect rect{};
    if (std::abs(target_aspect - aspect) < 0.001) {
        return DisplayViewportRect{0, 0, target_w, target_h};
    }
    if (target_aspect > aspect) {
        // Target is wider than aspect ratio -> Pillarbox (vertical fit, center horizontally)
        rect.height = target_h;
        rect.width = static_cast<std::uint32_t>(std::round(double(target_h) * aspect));
        rect.x = (target_w > rect.width) ? ((target_w - rect.width) / 2) : 0;
        rect.y = 0;
    } else {
        // Target is taller than aspect ratio -> Letterbox (horizontal fit, center vertically)
        rect.width = target_w;
        rect.height = static_cast<std::uint32_t>(std::round(double(target_w) / aspect));
        rect.x = 0;
        rect.y = (target_h > rect.height) ? ((target_h - rect.height) / 2) : 0;
    }
    return rect;
}

bool NativeOffscreenDisplayService::swap_buffers(DisplayServiceFailure& failure) noexcept {
    failure = {};
    if (!is_context_current()) {
        failure = {DisplayServiceError::wrong_thread, "swap_buffers requires active context on current thread"};
        return false;
    }
#ifdef _WIN32
    // On physical window targets: SwapBuffers(impl_->dc_) executes physical double-buffer swap.
    // On offscreen targets: Virtual frame boundary is committed via glFlush/fence sync without physical monitor swap.
    glFlush();
    return true;
#else
    return true;
#endif
}

bool NativeOffscreenDisplayService::set_vsync(bool /*enable*/, DisplayServiceFailure& failure) noexcept {
    // Physical monitor vsync (wglSwapIntervalEXT) requires a desktop swapchain and monitor raster beam.
    // Offscreen render targets operate under virtual vsync (deterministic unlocked simulation clock).
    failure = {DisplayServiceError::vsync_unsupported_offscreen,
        "Physical vsync is exclusive to desktop swapchain presentation; offscreen targets use virtual vsync"};
    return false;
}

std::uint32_t NativeOffscreenDisplayService::target_width() const noexcept {
    return impl_ ? impl_->width_ : 0;
}

std::uint32_t NativeOffscreenDisplayService::target_height() const noexcept {
    return impl_ ? impl_->height_ : 0;
}

DisplayTelemetry NativeOffscreenDisplayService::telemetry() const noexcept {
    return impl_ ? impl_->telemetry_ : DisplayTelemetry{};
}

void NativeOffscreenDisplayService::record_gl_draw(std::uint32_t count) noexcept {
    if (impl_) {
        impl_->telemetry_.gl_draw_calls += count;
    }
}

void NativeOffscreenDisplayService::record_diagnostic_readback(std::uint32_t count) noexcept {
    if (impl_) {
        impl_->telemetry_.diagnostic_readbacks += count;
    }
}

void NativeOffscreenDisplayService::record_frame_readback(std::uint32_t count) noexcept {
    if (impl_) {
        impl_->telemetry_.frame_readbacks += count;
    }
}

void NativeOffscreenDisplayService::reset_telemetry() noexcept {
    if (impl_) {
        impl_->telemetry_ = {};
    }
}

} // namespace mka::retail
