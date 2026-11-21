#include "mka/retail_main.hpp"
#include "mka/native_user_paths.hpp"
#include "native_retail_startup.hpp"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <vector>

#define NOMINMAX
#include <windows.h>

namespace {
void require(bool value, const char* message) {
    if (!value) { std::cerr << message << '\n'; std::abort(); }
}
}

int main() {
    using namespace mka::retail;
    const auto temp = std::filesystem::temp_directory_path();
    std::filesystem::path root;
    for (unsigned i = 0; ; ++i) {
        root = temp / ("mka-save-startup-owned-" + std::to_string(GetCurrentProcessId()) +
            "-" + std::to_string(GetTickCount64()) + "-" + std::to_string(i));
        std::error_code error;
        if (std::filesystem::create_directory(root, error)) break;
        require(i < 100, "create exclusively owned startup fixture");
    }
    std::filesystem::path application, saves;
    {
        // This is a test-owned native root, not an environment/correctness
        // flag. Production readiness resolves the Windows known folder.
        mka::platform::NativeUserPaths paths;
        mka::platform::UserPathFailure path_failure;
        require(mka::platform::NativeUserPaths::from_local_app_data(root, paths, path_failure),
            "native default directory policy on owned test root");
        application = paths.application();
        saves = application / "saves";
        require(application == std::filesystem::canonical(root) / "MortalKombatArmageddon" &&
            !std::filesystem::exists(saves), "policy does not create directories early");
        mka::content::ContentVolumeFailure content_failure;
        auto content = mka::content::NativeContentVolume::open(root, content_failure);
        require(bool(content), "native read-only test content root");
        NativeRetailStartupState startup(std::move(*content), {});
        mka::input::ControllerInput samples;
        NativeRetailMainState main(startup, samples);
        mka::save::SaveFailure save_failure;
        auto storage = mka::save::NativeSaveStore::open_or_create(saves, save_failure);
        require(bool(storage), "real writable native save capability");
        main.saved_profile_storage = std::move(storage);
        main.saved_profile_service_word = 97;
        main.post_overlay_state = 4;
        StartupFailure failure;
        require(main_binding::initialize_retail_saved_profile_storage(main, failure) &&
            failure.kind == StartupError::none && main.saved_profile_service_word == 0 &&
            main.post_overlay_state == 4, "actual Main readiness retains service and clears only source word");
        const std::array<std::uint8_t, 4> payload{0, 21, 0xff, 0};
        require(main.saved_profile_storage->replace("ProfileFixture", payload, save_failure),
            "Main owns real storage rather than a readiness flag");
        main.saved_profile_service_word = 39;
        require(main_binding::initialize_retail_saved_profile_storage(main, failure) &&
            main.saved_profile_service_word == 0, "reentry validates same storage");
        std::vector<std::uint8_t> got;
        require(main.saved_profile_storage->read("ProfileFixture", got, save_failure) &&
            got == std::vector<std::uint8_t>(payload.begin(), payload.end()),
            "reinitialization preserves saved bytes");
        auto retained = std::move(*main.saved_profile_storage);
        main.saved_profile_service_word = 17;
        require(!main_binding::initialize_retail_saved_profile_storage(main, failure) &&
            failure.kind == StartupError::saved_profile_storage &&
            failure.saved_profile_storage.kind == mka::save::SaveError::moved_store &&
            main.saved_profile_service_word == 17 && main.post_overlay_state == 4,
            "invalid retained service refuses before source/profile mutation without reopening a fallback");
        main.saved_profile_storage.emplace(std::move(retained));
        require(main_binding::initialize_retail_saved_profile_storage(main, failure) &&
            main.saved_profile_service_word == 0, "explicit owner restoration is validated");
    }
    {
        mka::save::SaveFailure failure;
        auto reopened = mka::save::NativeSaveStore::open_or_create(saves, failure);
        std::vector<std::uint8_t> bytes;
        require(reopened && reopened->read("ProfileFixture", bytes, failure) &&
            bytes == std::vector<std::uint8_t>({0, 21, 0xff, 0}),
            "actual saved file outlives Main storage owner");
        require(reopened->erase("ProfileFixture", failure), "delete only owned fixture payload");
    }
    require(std::filesystem::remove(saves) && std::filesystem::remove(application) &&
        std::filesystem::remove(root), "retire only exclusively owned empty fixture directories");
    std::cout << "PASS Main native save readiness and retained file lifetime; full profile records remain required\n";
}
