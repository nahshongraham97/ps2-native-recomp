#pragma once
#include "mka/content_volume.hpp"
#include "mka/controller_input.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace mka::retail {
enum class RetailPlatformError {
    none, allocation, native_exception, wrong_thread, closed, missing_crt,
    content, invalid_picture, settings, initialization, window, context,
    graphics, input, unknown_device, invalid_assignment, publication
};
struct RetailPlatformFailure {
    RetailPlatformError kind=RetailPlatformError::none;
    const char* operation="";
    std::string detail;
    content::ContentVolumeFailure content;
};
enum class RetailLoadingOrigin { asset, source_missing_file_pattern };
struct RetailLoadingPicture {
    // Actual source picture data, not a renderer/VRAM or an RGBA conversion.
    std::uint32_t width=256,height=128,canvas_width=640,canvas_height=224;
    std::uint32_t x=192,y=48;
    RetailLoadingOrigin origin=RetailLoadingOrigin::asset;
    std::vector<std::uint8_t> rgb;
};
std::optional<RetailLoadingPicture> load_retail_loading_picture(
    const content::NativeContentVolume&,RetailPlatformFailure&) noexcept;

struct RetailHostController {std::int32_t instance;std::string name;};
// Native window/event/controller/GL ownership for the connected retail entry.
// No original module, kernel, bus, framebuffer register or device is recreated.
// Real endpoints are opened only by create(), never by static initialization.
class NativeRetailPlatform final {
public:
    static std::unique_ptr<NativeRetailPlatform> create(
        const content::NativeContentVolume&,RetailPlatformFailure&) noexcept;
    ~NativeRetailPlatform();
    NativeRetailPlatform(const NativeRetailPlatform&)=delete;
    NativeRetailPlatform& operator=(const NativeRetailPlatform&)=delete;
    input::ControllerInput& controller_samples() noexcept;
    bool poll_events_and_input(RetailPlatformFailure&) noexcept;
    std::optional<std::vector<RetailHostController>> controllers(RetailPlatformFailure&) const noexcept;
    // Game role ownership explicitly chooses players. Device arrival never
    // assigns a retail player or invents the game's role association.
    bool assign_keyboard(std::uint32_t player,RetailPlatformFailure&) noexcept;
    bool assign_controller(std::int32_t instance,std::uint32_t player,RetailPlatformFailure&) noexcept;
    bool remove_keyboard_assignment(RetailPlatformFailure&) noexcept;
    bool remove_controller_assignment(std::int32_t instance,RetailPlatformFailure&) noexcept;
    bool close_requested() const noexcept;
    bool prepare_frame(RetailPlatformFailure&) noexcept;
    bool present(RetailPlatformFailure&) noexcept;
    bool drain(RetailPlatformFailure&) noexcept;
    bool shutdown(RetailPlatformFailure&) noexcept;
private:
    struct State;std::unique_ptr<State> state_;
    explicit NativeRetailPlatform(std::unique_ptr<State>);
};
} // namespace mka::retail
