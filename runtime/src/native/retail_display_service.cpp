#include "mka/retail_display_service.hpp"
#include <cmath>
#include <stdexcept>
#include <thread>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#undef near
#undef far
#include <GL/gl.h>
#endif

namespace mka::retail {

struct NativeOffscreenDisplayService::Impl {
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::uint32_t logical_width_ = NativeProductionDisplayService::LOGICAL_WIDTH;
    std::uint32_t logical_height_ = NativeProductionDisplayService::LOGICAL_HEIGHT;
    std::thread::id owner_thread_{};
    DisplayTelemetry telemetry_{};
    std::uint64_t destination_identity_ = 0x300000001ull;

#ifdef _WIN32
    HWND window_ = nullptr;
    HDC dc_ = nullptr;
    HGLRC context_ = nullptr;
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

    try {
        auto impl = std::make_unique<Impl>(width, height, logical_w, logical_h);

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
    if (!impl_ || !impl_->context_ || !impl_->dc_) {
        failure = {DisplayServiceError::stale_device, "Display service is in an uninitialized or stale state"};
        return false;
    }
    if (!wglMakeCurrent(impl_->dc_, impl_->context_)) {
        failure = {DisplayServiceError::context_activation_failed, "wglMakeCurrent failed to bind offscreen context"};
        return false;
    }
    impl_->owner_thread_ = std::this_thread::get_id();
    return true;
#else
    failure = {DisplayServiceError::context_activation_failed, "Offscreen display service requires Windows OpenGL"};
    return false;
#endif
}

bool NativeOffscreenDisplayService::release_context(DisplayServiceFailure& failure) noexcept {
    failure = {};
#ifdef _WIN32
    if (impl_->owner_thread_ != std::this_thread::get_id() && impl_->owner_thread_ != std::thread::id{}) {
        failure = {DisplayServiceError::wrong_thread, "Cannot release context from a non-owning thread"};
        return false;
    }
    if (wglGetCurrentContext() == impl_->context_) {
        wglMakeCurrent(nullptr, nullptr);
    }
    impl_->owner_thread_ = {};
    return true;
#else
    return true;
#endif
}

bool NativeOffscreenDisplayService::is_context_current() const noexcept {
#ifdef _WIN32
    return impl_ && impl_->context_ && (wglGetCurrentContext() == impl_->context_);
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
