#include "native_startup_codegen.hpp"
#include <array>
#include <iterator>
#include <new>
namespace recomp { namespace {
constexpr const char* identity="a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f";
constexpr NativeClosureWord required_words[]={
    {0x00100008u,0x70000c28u},
    {0x0010000cu,0x70001428u},
    {0x00100010u,0x70001c28u},
    {0x00100014u,0x70002428u},
    {0x00100018u,0x70002c28u},
    {0x0010001cu,0x70003428u},
    {0x00100020u,0x70003c28u},
    {0x00100024u,0x70004428u},
    {0x00100028u,0x70004c28u},
    {0x0010002cu,0x70005428u},
    {0x00100030u,0x70005c28u},
    {0x00100034u,0x70006428u},
    {0x00100038u,0x70006c28u},
    {0x0010003cu,0x70007428u},
    {0x00100040u,0x70007c28u},
    {0x00100044u,0x70008428u},
    {0x00100048u,0x70008c28u},
    {0x0010004cu,0x70009428u},
    {0x00100050u,0x70009c28u},
    {0x00100054u,0x7000a428u},
    {0x00100058u,0x7000ac28u},
    {0x0010005cu,0x7000b428u},
    {0x00100060u,0x7000bc28u},
    {0x00100064u,0x7000c428u},
    {0x00100068u,0x7000cc28u},
    {0x0010006cu,0x7000e428u},
    {0x00100070u,0x7000ec28u},
    {0x00100074u,0x7000f428u},
    {0x00100078u,0x7000fc28u},
    {0x0010007cu,0x00000011u},
    {0x00100080u,0x70000011u},
    {0x00100084u,0x00000013u},
    {0x00100088u,0x70000013u},
    {0x0010008cu,0x04190000u},
    {0x00100090u,0x44800000u},
    {0x00100094u,0x44800800u},
    {0x00100098u,0x44801000u},
    {0x0010009cu,0x44801800u},
    {0x001000a0u,0x44802000u},
    {0x001000a4u,0x44802800u},
    {0x001000a8u,0x44803000u},
    {0x001000acu,0x44803800u},
    {0x001000b0u,0x44804000u},
    {0x001000b4u,0x44804800u},
    {0x001000b8u,0x44805000u},
    {0x001000bcu,0x44805800u},
    {0x001000c0u,0x44806000u},
    {0x001000c4u,0x44806800u},
    {0x001000c8u,0x44807000u},
    {0x001000ccu,0x44807800u},
    {0x001000d0u,0x44808000u},
    {0x001000d4u,0x44808800u},
    {0x001000d8u,0x44809000u},
    {0x001000dcu,0x44809800u},
    {0x001000e0u,0x4480a000u},
    {0x001000e4u,0x4480a800u},
    {0x001000e8u,0x4480b000u},
    {0x001000ecu,0x4480b800u},
    {0x001000f0u,0x4480c000u},
    {0x001000f4u,0x4480c800u},
    {0x001000f8u,0x4480d000u},
    {0x001000fcu,0x4480d800u},
    {0x00100100u,0x4480e000u},
    {0x00100104u,0x4480e800u},
    {0x00100108u,0x4480f000u},
    {0x0010010cu,0x4480f800u},
    {0x00100110u,0x46010018u},
    {0x00100114u,0x0000040fu},
    {0x00100118u,0x44c0f800u},
    {0x0010011cu,0x3c020065u},
    {0x00100120u,0x3c03009cu},
    {0x00100124u,0x2442de80u},
    {0x00100128u,0x24634a00u},
    {0x0010012cu,0x3044000fu},
    {0x00100130u,0x10800006u},
    {0x00100134u,0x00000000u},
    {0x00100138u,0xa0400000u},
    {0x0010013cu,0x24420001u},
    {0x00100140u,0x00000000u},
    {0x00100144u,0x0804004bu},
    {0x00100148u,0x00000000u},
    {0x0010014cu,0x3c04ffffu},
    {0x00100150u,0x3484fff0u},
    {0x00100154u,0x00642024u},
    {0x00100158u,0x10440007u},
    {0x0010015cu,0x00000000u},
    {0x00100160u,0x7c400000u},
    {0x00100164u,0x24420010u},
    {0x00100168u,0x00000000u},
    {0x0010016cu,0x00000000u},
    {0x00100170u,0x08040056u},
    {0x00100174u,0x00000000u},
    {0x00100178u,0x10430007u},
    {0x0010017cu,0x00000000u},
    {0x00100180u,0xa0400000u},
    {0x00100184u,0x24420001u},
    {0x00100188u,0x00000000u},
    {0x0010018cu,0x00000000u},
    {0x00100190u,0x0804005eu},
    {0x00100194u,0x00000000u},
    {0x00100198u,0x3c040065u},
    {0x0010019cu,0x3c0501ffu},
    {0x001001a0u,0x3c060001u},
    {0x001001a4u,0x3c07006au},
    {0x001001a8u,0x3c080010u},
    {0x001001acu,0x248440f0u},
    {0x001001b0u,0x24a50000u},
    {0x001001b4u,0x24c60000u},
    {0x001001b8u,0x24e7ca80u},
    {0x001001bcu,0x25080220u},
    {0x001001c0u,0x0080e025u},
    {0x001001c4u,0x2403003cu},
    {0x001001c8u,0x0000000cu},
    {0x001001ccu,0x0040e825u},
    {0x001001d0u,0x3c04009cu},
    {0x001001d4u,0x3c050000u},
    {0x001001d8u,0x24844a00u},
    {0x001001dcu,0x24a5ffffu},
    {0x001001e0u,0x2403003du},
    {0x001001e4u,0x0000000cu},
    {0x001001e8u,0x0c045b36u},
    {0x001001ecu,0x00000000u},
    {0x001001f0u,0x0c043298u},
    {0x001001f4u,0x00002025u},
    {0x001001f8u,0x42000038u},
    {0x001001fcu,0x3c02006au},
    {0x00100200u,0x2442ca80u},
    {0x00100204u,0x8c440000u},
    {0x00100208u,0x0c0489d0u},
    {0x0010020cu,0x24450004u},
    {0x00100210u,0x080763aau},
    {0x00100214u,0x00402025u},
    {0x00100218u,0x08045c36u},
    {0x0010021cu,0x00000000u},
    {0x00100220u,0x24030023u},
    {0x00100224u,0x0000000cu},
    {0x00116cd8u,0x27bdfff0u},
    {0x00116cdcu,0xffbf0000u},
    {0x00116ce0u,0x0c045aa2u},
    {0x00116ce4u,0x00000000u},
    {0x00116ce8u,0x0c045af2u},
    {0x00116cecu,0x00000000u},
    {0x00116cf0u,0x0c045c6eu},
    {0x00116cf4u,0x00000000u},
    {0x00116cf8u,0x0c045cccu},
    {0x00116cfcu,0x24040002u},
    {0x00116d00u,0x0c045d5cu},
    {0x00116d04u,0x00000000u},
    {0x00116d08u,0x0c04356eu},
    {0x00116d0cu,0x00000000u},
    {0x00116d10u,0x0c045b80u},
    {0x00116d14u,0x00000000u},
    {0x00116d18u,0x0c0458beu},
    {0x00116d1cu,0x00000000u},
    {0x00116d20u,0xdfbf0000u},
    {0x00116d24u,0x08046294u},
    {0x00116d28u,0x27bd0010u},
    {0x0010ca60u,0x24030064u},
    {0x0010ca64u,0x0000000cu},
    {0x0010ca68u,0x03e00008u},
    {0x0010ca6cu,0x00000000u},
    {0x00122740u,0x27bdffc0u},
    {0x00122744u,0xffbf0010u},
    {0x00122748u,0x7fb00000u},
    {0x0012274cu,0x0c088634u},
    {0x00122750u,0x00a0802du},
    {0x00122754u,0x0c0a5148u},
    {0x00122758u,0x00000000u},
    {0x0012275cu,0x0c0f7700u},
    {0x00122760u,0x8e040000u},
    {0x00122764u,0x0c0cd0f8u},
    {0x002218d0u,0x3c030060u},
    {0x002218d4u,0xdc64e750u},
    {0x002218d8u,0x3c03006bu},
    {0x002218dcu,0x03e00008u},
    {0x002218e0u,0xfc64a760u},
    {0x003ddc00u,0x27bdffe0u},
    {0x003ddc04u,0x0080282du},
    {0x003ddc08u,0x3c04007bu},
    {0x003ddc0cu,0xffbf0010u},
    {0x003ddc10u,0x24847e00u},
    {0x003ddc14u,0x0c077b16u},
    {0x003ddc18u,0x7fb00000u},
    {0x003ddc1cu,0x3c03007bu},
    {0x003ddc20u,0x3c06007bu},
    {0x003ddc24u,0x80657e00u},
    {0x003ddc28u,0x10a0000fu},
    {0x003ddc2cu,0x24c67e00u},
    {0x003ddc30u,0x2403005cu},
    {0x003ddc34u,0x2404002fu},
    {0x003ddc38u,0x00052e3cu},
    {0x003ddc3cu,0x00052e3fu},
    {0x003ddc40u,0x10a40003u},
    {0x003ddc44u,0x00000000u},
    {0x003ddc48u,0x14a30003u},
    {0x003ddc4cu,0x00000000u},
    {0x003ddc50u,0x00c0802du},
    {0x003ddc54u,0x00000000u},
    {0x003ddc58u,0x24c60001u},
    {0x003ddc5cu,0x80c50000u},
    {0x003ddc60u,0x54a0fff6u},
    {0x003ddc64u,0x00052e3cu},
    {0x003ddc68u,0xa2000001u},
    {0x003ddc6cu,0xdfbf0010u},
    {0x003ddc70u,0x7bb00000u},
    {0x003ddc74u,0x03e00008u},
    {0x003ddc78u,0x27bd0020u},
    {0x005fe750u,0x6f726463u},
    {0x005fe754u,0x003a306du}
};
constexpr NativeStartupStep steps[]={NativeStartupStep::crt_prerequisites,NativeStartupStep::thread_contract,
    NativeStartupStep::content_prefix,NativeStartupStep::platform_initialization,
    NativeStartupStep::boot_directory,NativeStartupStep::main_continuation};
}
NativeStartupManifest native_startup_manifest(){
    return {identity,{std::begin(required_words),std::end(required_words)},{std::begin(steps),std::end(steps)}};
}
std::optional<NativeStartupSources> emit_native_startup_closure(std::span<const std::uint8_t> bytes,
    const NativeStartupManifest& manifest,NativeClosureFailure& failure)noexcept {
    failure={};try {
        if(manifest.original_elf_sha256!=identity){failure={NativeClosureError::source_identity,"native startup manifest source identity"};return {};}
        if(manifest.words!=std::vector<NativeClosureWord>(std::begin(required_words),std::end(required_words))){failure={NativeClosureError::source_word,"native startup exact ordered word evidence"};return {};}
        if(manifest.steps!=std::vector<NativeStartupStep>(std::begin(steps),std::end(steps))){failure={NativeClosureError::recipe,"native startup ordered CRT and main obligations"};return {};}
        if(!verify_native_closure_source(bytes,manifest.original_elf_sha256,manifest.words,failure))return {};
        return NativeStartupSources{R"STARTUP(#pragma once
#include "mka/content_volume.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace mka::retail {
enum class StartupError {none,missing_boot_argument,malformed_boot_argument,missing_directory_separator,content,allocation,host_capacity,native_exception};
struct StartupFailure {StartupError kind=StartupError::none;const char* operation="";content::ContentVolumeFailure content;};
// Native-owned replacements for the original startup arguments and writable
// content/path state. Host executable location, boot-resource string and volume
// are separate inputs; no guest argument block, pointer table or BSS is retained.
struct NativeRetailStartupState {
    content::NativeContentVolume content;
    std::vector<std::string> boot_arguments;
    std::string resource_prefix,boot_directory;
    NativeRetailStartupState(content::NativeContentVolume volume,std::vector<std::string> arguments)
        :content(std::move(volume)),boot_arguments(std::move(arguments)){}
};
bool initialize_retail_resource_prefix(NativeRetailStartupState&,StartupFailure&) noexcept;
bool derive_retail_boot_directory(NativeRetailStartupState&,StartupFailure&) noexcept;
// Ordinary native file operation bound to the same actual startup volume.
// This is a platform destination, not an IRX loader or a disc API.
std::optional<content::NativeContentFile> open_retail_resource(const NativeRetailStartupState&,
    std::string_view,StartupFailure&) noexcept;
// Required native implementations. There are intentionally no successful
// defaults or weak definitions. The full entry cannot link until these original
// CRT/platform/remaining-game obligations have real implementations.
bool initialize_native_retail_crt(NativeRetailStartupState&,StartupFailure&);
bool configure_native_retail_thread(NativeRetailStartupState&,StartupFailure&);
bool initialize_native_retail_platform(NativeRetailStartupState&,StartupFailure&);
std::optional<std::int64_t> continue_native_retail_main(NativeRetailStartupState&,StartupFailure&);
std::optional<std::int64_t> run_native_retail(NativeRetailStartupState&,StartupFailure&) noexcept;
} // namespace mka::retail
)STARTUP",R"STARTUP(#include "native_retail_startup.hpp"
#include <new>
#include <stdexcept>
namespace mka::retail {
bool initialize_retail_resource_prefix(NativeRetailStartupState& state,StartupFailure& f)noexcept {
    f={};try {state.resource_prefix="cdrom0:";return true;}
    catch(const std::bad_alloc&){f={StartupError::allocation,"retail resource prefix allocation"};}
    catch(...){f={StartupError::native_exception,"retail resource prefix"};}return false;
}
bool derive_retail_boot_directory(NativeRetailStartupState& state,StartupFailure& f)noexcept {
    f={};
    if(state.boot_arguments.empty()){f={StartupError::missing_boot_argument,"retail argv0 missing"};return false;}
    const auto& boot=state.boot_arguments.front();
    if(boot.empty()||boot.find('\0')!=std::string::npos){f={StartupError::malformed_boot_argument,"retail argv0 extent"};return false;}
    const auto last=boot.find_last_of("/\\");
    if(last==std::string::npos){f={StartupError::missing_directory_separator,"retail argv0 directory separator"};return false;}
    try {auto directory=boot.substr(0,last+1);state.boot_directory=std::move(directory);return true;}
    catch(const std::bad_alloc&){f={StartupError::allocation,"retail boot directory allocation"};}
    catch(const std::length_error&){f={StartupError::host_capacity,"retail boot directory host capacity"};}
    catch(...){f={StartupError::native_exception,"retail boot directory"};}return false;
}
std::optional<content::NativeContentFile> open_retail_resource(const NativeRetailStartupState& state,
    std::string_view name,StartupFailure& f)noexcept {
    f={};try {auto result=state.content.open_resource(name,f.content);
        if(!result)f.kind=StartupError::content;return result;
    }catch(const std::bad_alloc&){f={StartupError::allocation,"retail content open allocation"};}
    catch(...){f={StartupError::native_exception,"retail content open"};}return {};
}
} // namespace mka::retail
)STARTUP",R"STARTUP(#include "native_retail_startup.hpp"
#include <new>
namespace mka::retail {
std::optional<std::int64_t> run_native_retail(NativeRetailStartupState& state,StartupFailure& f)noexcept {
    f={};try {
        // Original CRT calls precede main. Their required native definitions
        // cannot be replaced by entering the middle of the retail main.
        if(!initialize_native_retail_crt(state,f))return {};
        if(!configure_native_retail_thread(state,f))return {};
        if(!initialize_retail_resource_prefix(state,f))return {};
        if(!initialize_native_retail_platform(state,f))return {};
        if(!derive_retail_boot_directory(state,f))return {};
        return continue_native_retail_main(state,f);
    }catch(const std::bad_alloc&){f={StartupError::allocation,"native retail startup allocation"};}
    catch(...){f={StartupError::native_exception,"native retail startup"};}return {};
}
} // namespace mka::retail
)STARTUP"};
    }catch(const std::bad_alloc&){failure={NativeClosureError::allocation,"native startup source emission allocation"};}
    catch(...){failure={NativeClosureError::recipe,"native startup source emission"};}return {};
}
} // namespace recomp
