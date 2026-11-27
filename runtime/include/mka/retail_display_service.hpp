#pragma once
#include <array>
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
    stale_device,
    unsupported_target,
    invalid_clear,
    native_operation_failed,
    target_owner
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

// Identity of the actual host draw owner, never a guest pointer or numeric token.
// A camera record supplies this identity; the display borrows it weakly.
class NativeDisplayDrawOwner {
public:
    virtual ~NativeDisplayDrawOwner() = default;
protected:
    NativeDisplayDrawOwner() = default;
};

// Abstract interface for the production host display service.
// Defines context ownership, viewport scaling (logical 1280x720 primary Xbox target,
// named 640x448 PS2 reference mode vs host target), and swap/vsync presentation contracts.
class NativeProductionDisplayService {
public:
    // Xbox 720p Primary Logical Target (Recovered from default.xbe 0x002695f0)
    static constexpr std::uint32_t LOGICAL_WIDTH = 1280;
    static constexpr std::uint32_t LOGICAL_HEIGHT = 720;

    // PS2 Reference Logical Dimensions (Named test case / GS frame buffer)
    static constexpr std::uint32_t PS2_LOGICAL_WIDTH = 640;
    static constexpr std::uint32_t PS2_LOGICAL_HEIGHT = 448;

    virtual ~NativeProductionDisplayService() noexcept = default;

    // Context Ownership Lifecycle
    virtual bool acquire_context(DisplayServiceFailure& failure) noexcept = 0;
    virtual bool release_context(DisplayServiceFailure& failure) noexcept = 0;
    virtual bool is_context_current() const noexcept = 0;
    virtual void* native_display_handle() const noexcept = 0;
    virtual std::uint64_t destination_identity() const noexcept = 0;

    // Native default-window color/depth storage. Viewport coordinates are
    // physical pixels with a lower-left origin; no game-coordinate conversion
    // or depth convention is inferred here. No draw/read framebuffer aliasing.
    virtual bool begin_render_target(const std::shared_ptr<const NativeDisplayDrawOwner>&,
        DisplayViewportRect, bool require_depth, DisplayServiceFailure&) noexcept = 0;
    virtual bool clear_render_target(const NativeDisplayDrawOwner&,
        const std::optional<std::array<float,4>>& color, std::optional<float> depth,
        DisplayServiceFailure&) noexcept = 0;
    virtual bool end_render_target(const NativeDisplayDrawOwner&, DisplayServiceFailure&) noexcept = 0;
    virtual bool owns_render_target(const NativeDisplayDrawOwner&) const noexcept = 0;
    // Host recovery only after the real draw owner has expired. Ordinary begin
    // never steals an active/abandoned pass or guesses that cleanup completed.
    virtual bool restore_abandoned_render_target(DisplayServiceFailure&) noexcept = 0;

    // Logical Dimensions & Viewport & Aspect Ratio Management
    virtual std::uint32_t logical_width() const noexcept = 0;
    virtual std::uint32_t logical_height() const noexcept = 0;
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
    virtual void record_diagnostic_readback(std::uint32_t count = 1) noexcept = 0;
    virtual void record_frame_readback(std::uint32_t count = 1) noexcept = 0;
    virtual void reset_telemetry() noexcept = 0;
};

// Offscreen production display service implementation.
// Meets all presentation requirements for headless / offscreen render targets
// without creating a visible window, audible endpoint, or owner launch.
// Strictly avoids reusing the test-fixture HiddenGlContext.
class NativeOffscreenDisplayService final : public NativeProductionDisplayService {
public:
    static std::shared_ptr<NativeOffscreenDisplayService> create(
        std::uint32_t width, std::uint32_t height, DisplayServiceFailure& failure,
        std::uint32_t logical_w = LOGICAL_WIDTH, std::uint32_t logical_h = LOGICAL_HEIGHT);

    ~NativeOffscreenDisplayService() noexcept override;

    bool acquire_context(DisplayServiceFailure& failure) noexcept override;
    bool release_context(DisplayServiceFailure& failure) noexcept override;
    bool is_context_current() const noexcept override;
    void* native_display_handle() const noexcept override;
    std::uint64_t destination_identity() const noexcept override;
    bool begin_render_target(const std::shared_ptr<const NativeDisplayDrawOwner>&,
        DisplayViewportRect, bool require_depth, DisplayServiceFailure&) noexcept override;
    bool clear_render_target(const NativeDisplayDrawOwner&,
        const std::optional<std::array<float,4>>&, std::optional<float>, DisplayServiceFailure&) noexcept override;
    bool end_render_target(const NativeDisplayDrawOwner&, DisplayServiceFailure&) noexcept override;
    bool owns_render_target(const NativeDisplayDrawOwner&) const noexcept override;
    bool restore_abandoned_render_target(DisplayServiceFailure&) noexcept override;

    std::uint32_t logical_width() const noexcept override;
    std::uint32_t logical_height() const noexcept override;
    DisplayViewportRect compute_viewport(std::uint32_t target_w, std::uint32_t target_h,
        DisplayAspectMode aspect_mode) const noexcept override;

    bool swap_buffers(DisplayServiceFailure& failure) noexcept override;
    bool set_vsync(bool enable, DisplayServiceFailure& failure) noexcept override;

    std::uint32_t target_width() const noexcept override;
    std::uint32_t target_height() const noexcept override;
    DisplayTelemetry telemetry() const noexcept override;
    void record_gl_draw(std::uint32_t count = 1) noexcept override;
    void record_diagnostic_readback(std::uint32_t count = 1) noexcept override;
    void record_frame_readback(std::uint32_t count = 1) noexcept override;
    void reset_telemetry() noexcept override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    explicit NativeOffscreenDisplayService(std::unique_ptr<Impl> impl) noexcept;
};

} // namespace mka::retail
