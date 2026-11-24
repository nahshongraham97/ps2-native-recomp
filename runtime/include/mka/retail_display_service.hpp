#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>

namespace mka::retail {

enum class DisplayAspectMode : std::uint32_t {
    standard_4_3 = 0,   // 4:3 standard aspect ratio (retail default, 640x448)
    widescreen_16_9 = 1 // 16:9 widescreen presentation mode
};

enum class DisplayServiceError : std::uint32_t {
    none = 0,
    context_creation_failed,
    context_activation_failed,
    wrong_thread,
    invalid_dimensions,
    swap_failed,
    vsync_unsupported_offscreen,
    resource_busy,
    stale_device
};

struct DisplayServiceFailure {
    DisplayServiceError error = DisplayServiceError::none;
    const char* message = "";
};

struct DisplayViewportRect {
    std::uint32_t x = 0;
    std::uint32_t y = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

struct DisplayTelemetry {
    std::uint32_t gl_draw_calls = 0;
    std::uint32_t cpu_composited_pixels = 0;
    std::uint32_t frame_readbacks = 0;
    std::uint32_t diagnostic_readbacks = 0;
    std::uint32_t fallback_pixels = 0;
};

// Abstract interface for the production host display service.
// Defines context ownership, viewport scaling (logical 640x448 vs host target),
// and swap/vsync presentation contracts.
class NativeProductionDisplayService {
public:
    static constexpr std::uint32_t LOGICAL_WIDTH = 640;
    static constexpr std::uint32_t LOGICAL_HEIGHT = 448;

    virtual ~NativeProductionDisplayService() noexcept = default;

    // Context Ownership Lifecycle
    virtual bool acquire_context(DisplayServiceFailure& failure) noexcept = 0;
    virtual bool release_context(DisplayServiceFailure& failure) noexcept = 0;
    virtual bool is_context_current() const noexcept = 0;
    virtual void* native_display_handle() const noexcept = 0;
    virtual std::uint64_t destination_identity() const noexcept = 0;

    // Viewport & Aspect Ratio Management (Letterbox / Pillarbox centering)
    virtual DisplayViewportRect compute_viewport(std::uint32_t target_w, std::uint32_t target_h,
        DisplayAspectMode aspect_mode) const noexcept = 0;

    // Swap and Vsync Semantics
    // Note: On physical window targets, swap_buffers performs hardware swapchain presentation
    // (SwapBuffers/eglSwapBuffers) and vsync controls monitor refresh sync (wglSwapIntervalEXT).
    // On offscreen targets, swap_buffers completes the offscreen FBO fence and rotates the virtual target.
    virtual bool swap_buffers(DisplayServiceFailure& failure) noexcept = 0;
    virtual bool set_vsync(bool enable, DisplayServiceFailure& failure) noexcept = 0;

    // Target Dimensions & Telemetry
    virtual std::uint32_t target_width() const noexcept = 0;
    virtual std::uint32_t target_height() const noexcept = 0;
    virtual DisplayTelemetry telemetry() const noexcept = 0;
    virtual void record_gl_draw(std::uint32_t count = 1) noexcept = 0;
    virtual void reset_telemetry() noexcept = 0;
};

// Offscreen production display service implementation.
// Meets all presentation requirements for headless / offscreen render targets
// without creating a visible window, audible endpoint, or owner launch.
// Strictly avoids reusing the test-fixture HiddenGlContext.
class NativeOffscreenDisplayService final : public NativeProductionDisplayService {
public:
    static std::shared_ptr<NativeOffscreenDisplayService> create(
        std::uint32_t width, std::uint32_t height, DisplayServiceFailure& failure);

    ~NativeOffscreenDisplayService() noexcept override;

    bool acquire_context(DisplayServiceFailure& failure) noexcept override;
    bool release_context(DisplayServiceFailure& failure) noexcept override;
    bool is_context_current() const noexcept override;
    void* native_display_handle() const noexcept override;
    std::uint64_t destination_identity() const noexcept override;

    DisplayViewportRect compute_viewport(std::uint32_t target_w, std::uint32_t target_h,
        DisplayAspectMode aspect_mode) const noexcept override;

    bool swap_buffers(DisplayServiceFailure& failure) noexcept override;
    bool set_vsync(bool enable, DisplayServiceFailure& failure) noexcept override;

    std::uint32_t target_width() const noexcept override;
    std::uint32_t target_height() const noexcept override;
    DisplayTelemetry telemetry() const noexcept override;
    void record_gl_draw(std::uint32_t count = 1) noexcept override;
    void reset_telemetry() noexcept override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    explicit NativeOffscreenDisplayService(std::unique_ptr<Impl> impl) noexcept;
};

} // namespace mka::retail
