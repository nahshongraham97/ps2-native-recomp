#include "mka/retail_profile_resource_ids.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_game_session.hpp"
#include "native_retail_startup.hpp"
#include "../runtime/src/native/retail_main_control.hpp"

#include <cstdio>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>

using namespace mka::retail;
namespace {
void check(bool okay,const char* label) {
    if(!okay) throw std::runtime_error(label);
}
mka::content::NativeContentVolume open_volume(const std::filesystem::path& root) {
    mka::content::ContentVolumeFailure f;
    auto volume=mka::content::NativeContentVolume::open(root,f);
    check(bool(volume),"actual native content volume");
    return std::move(*volume);
}
struct Fixture {
    NativeRetailStartupState startup;
    mka::input::ControllerInput samples;
    std::unique_ptr<NativeRetailMainState> main;
    NativeRetailProfileResourceIds* ids{};
    explicit Fixture(const std::filesystem::path& root)
        : startup(open_volume(root),{}) {
        StartupFailure f;
        check(initialize_native_retail_game_session(startup,f),
              "real retained game Session");
        main=std::make_unique<NativeRetailMainState>(startup,samples);
        RetailGameSessionFailure sf;
        ids=startup.game_session->profile_resource_ids(sf);
        check(ids&&sf.kind==RetailGameSessionError::none,
              "ID owner borrowed from actual game Session");
    }
};
enum class Effect { ordinary, append_same_id, transfer, ordinary_throw, named_failure };
Effect effect=Effect::ordinary;
NativeRetailProfileResourceIds* callback_owner{};
std::size_t effect_calls{};
}

// Test-only definition of the deliberately unresolved production effect.
namespace mka::retail {
bool retire_matching_retail_secret_camera_from_446f80(
    NativeRetailMainState& main,std::uint32_t input_id,
    RetailProfileResourceIdsFailure& f) {
    ++effect_calls;
    if(effect==Effect::append_same_id) {
        effect=Effect::ordinary;
        RetailProfileResourceIdsFailure nested_failure;
        const auto nested=callback_owner->record_from_446f80(
            main,input_id,nested_failure);
        check(nested && *nested==RetailProfileResourceIdOutcome::appended,
              "source callback's nested same-ID append");
    } else if(effect==Effect::transfer) {
        throw detail::NativeMainReentry{&main,0x1234};
    } else if(effect==Effect::ordinary_throw) {
        throw std::runtime_error("test actor cleanup failure");
    } else if(effect==Effect::named_failure) {
        f={RetailProfileResourceIdsError::secret_camera_effect,
           "test required actor cleanup refusal"};
        return false;
    }
    f={};
    return true;
}
}

int main(int argc,char** argv) {
    try {
        check(argc==2,"requires an existing content-volume directory");
        const auto root=std::filesystem::u8path(argv[1]);
        Fixture normal(root);
        callback_owner=normal.ids;
        RetailProfileResourceIdsFailure f;

        auto result=normal.ids->record_from_446f80(*normal.main,7,f);
        check(result&&*result==RetailProfileResourceIdOutcome::appended&&effect_calls==1,
              "source first ID invokes actor effect then appends");
        result=normal.ids->record_from_446f80(*normal.main,7,f);
        check(result&&*result==RetailProfileResourceIdOutcome::duplicate&&effect_calls==1,
              "entry duplicate skips actor search and append");
        result=normal.ids->record_from_446f80(*normal.main,300,f);
        check(result&&*result==RetailProfileResourceIdOutcome::outside_byte_identity&&
              effect_calls==2,"wide source ID runs effect but skips byte append");

        effect=Effect::append_same_id;
        result=normal.ids->record_from_446f80(*normal.main,11,f);
        check(result&&*result==RetailProfileResourceIdOutcome::appended&&effect_calls==4,
              "outer call does not repeat duplicate test after effect");
        auto observation=normal.ids->observe(f);
        check(observation&&observation->active.size()==3&&
              observation->active[0]==7&&observation->active[1]==11&&
              observation->active[2]==11&&observation->staging==observation->active&&
              observation->active_count==3&&observation->staging_count==3,
              "nested callback publishes two same-ID bytes in source order");

        bool foreign_refused=false;
        std::thread foreign([&] {
            RetailProfileResourceIdsFailure foreign_failure;
            auto foreign_result=normal.ids->record_from_446f80(
                *normal.main,20,foreign_failure);
            foreign_refused=!foreign_result&&
                foreign_failure.kind==RetailProfileResourceIdsError::wrong_thread;
        });
        foreign.join();
        check(foreign_refused&&effect_calls==4,
              "foreign thread refused before Main or actor effect");

        Fixture other(root);
        result=normal.ids->record_from_446f80(*other.main,20,f);
        check(!result&&f.kind==RetailProfileResourceIdsError::foreign_main&&
              effect_calls==4,"foreign Main Session refused before actor effect");
        result=normal.ids->record_from_446f80(*normal.main,20,f);
        check(result&&*result==RetailProfileResourceIdOutcome::appended,
              "foreign Main refusal did not poison valid Session");

        // The original 60-byte region is not a native storage ceiling.
        for(std::uint32_t id=0;id<=70;++id) {
            result=normal.ids->record_from_446f80(*normal.main,id,f);
            check(result&&f.kind==RetailProfileResourceIdsError::none,
                  "dynamic native ID storage beyond original region");
        }
        observation=normal.ids->observe(f);
        check(observation&&observation->active_count>60&&
              observation->active==observation->staging,
              "native ID collection grows beyond source backing region");

        Fixture transfer(root);
        callback_owner=transfer.ids;
        effect=Effect::transfer;
        bool transferred=false;
        try { (void)transfer.ids->record_from_446f80(*transfer.main,99,f); }
        catch(const detail::NativeMainReentry& transition) {
            transferred=transition.owner==transfer.main.get()&&
                        transition.value==0x1234;
        }
        check(transferred,"source main control transfer propagates");
        effect=Effect::ordinary;
        result=transfer.ids->record_from_446f80(*transfer.main,99,f);
        check(result&&*result==RetailProfileResourceIdOutcome::appended,
              "legitimate transfer does not poison same Session owner");

        Fixture failure(root);
        callback_owner=failure.ids;
        effect=Effect::ordinary_throw;
        result=failure.ids->record_from_446f80(*failure.main,100,f);
        check(!result&&f.kind==RetailProfileResourceIdsError::native_exception,
              "ordinary after-effect exception is named");
        observation=failure.ids->observe(f);
        check(observation&&observation->after_effect_unknown&&
              observation->active.empty()&&observation->staging.empty(),
              "failed actor effect retains source-zero-born IDs and poison");
        const auto calls_before=effect_calls;
        result=failure.ids->record_from_446f80(*failure.main,101,f);
        check(!result&&f.kind==RetailProfileResourceIdsError::after_effect_unknown&&
              effect_calls==calls_before,"after-effect failure cannot replay cleanup");

        Fixture named(root);
        callback_owner=named.ids;
        effect=Effect::named_failure;
        result=named.ids->record_from_446f80(*named.main,1,f);
        check(!result&&f.kind==RetailProfileResourceIdsError::secret_camera_effect,
              "required actor owner named refusal propagates");

        Fixture retired(root);
        callback_owner=retired.ids;
        effect=Effect::ordinary;
        retired.main->lifetime->retire();
        const auto before_retired=effect_calls;
        result=retired.ids->record_from_446f80(*retired.main,1,f);
        check(!result&&f.kind==RetailProfileResourceIdsError::retired_main&&
              effect_calls==before_retired,
              "retired Main refuses before Session actor effect");
        return 0;
    } catch(const std::exception& error) {
        std::fprintf(stderr,"retail profile resource IDs: %s\n",error.what());
        return 1;
    }
}
