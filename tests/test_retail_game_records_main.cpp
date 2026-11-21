#include "mka/retail_main.hpp"
#include "mka/retail_game_records_required.hpp"
#include "mka/retail_game_info_resources.hpp"
#include "mka/retail_initial_game_tasks.hpp"
#include "mka/retail_profile_main_state.hpp"
#include "mka/retail_text_storage.hpp"
#include "native_retail_startup.hpp"

#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
void check(bool okay,const char* label) {
    if(!okay) throw std::runtime_error(label);
}
void source_call(bool okay,const mka::retail::StartupFailure& failure,const char* label) {
    if(!okay) throw std::runtime_error(std::string(label)+": "+failure.operation+
        " (startup="+std::to_string(static_cast<int>(failure.kind))+
        " records="+std::to_string(static_cast<int>(failure.game_records.kind))+")");
}
}
int main(int argc,char** argv) {
    using namespace mka::retail;
    try {
        check(argc==2,"requires original extracted GAMER module directory");
        mka::content::ContentVolumeFailure cf;
        auto volume=mka::content::NativeContentVolume::open(std::filesystem::u8path(argv[1]),cf);
        check(bool(volume),"actual filesystem content volume");
        NativeRetailStartupState startup(std::move(*volume),{});
        StartupFailure f;
        source_call(initialize_native_retail_game_session(startup,f),f,"actual retained Session");
        source_call(initialize_retail_memory_owners(startup,f),f,"actual memory group");
        mka::input::ControllerInput samples;
        NativeRetailMainState main(startup,samples);
        RetailGameSessionFailure option_failure;
        auto* options=startup.game_session->game_options(option_failure);
        RetailGameOptionsFailure options_failure;
        const std::uint8_t selected_record[]={'4','.'};
        check(options&&options->parse_selected_record_from_3433c0(
                  std::span<const std::uint8_t>{selected_record},options_failure),
              "actual retained source option producer");
        source_call(main_binding::initialize_retail_game_options(main,f),f,"production164130 game options");
        check(options->observe(options_failure)->live_mode==3&&
              options->observe(options_failure)->word_660380==1,
              "Main164130 reads live source mode rather than assuming BSS zero");
        check(options->parse_selected_record_from_3433c0(std::nullopt,options_failure),
              "actual null-record mode reset");
        source_call(main_binding::initialize_retail_game_options(main,f),f,"repeat production164130");
        check(options->observe(options_failure)->live_mode==0&&
              options->observe(options_failure)->word_660380==0,
              "repeat Main option publication follows the same live source owner");
        check(!main_binding::retail_task_exists(main,0x2065,f)&&
              f.kind==StartupError::game_session&&
              f.game.task_domain.kind==RetailTaskDomainError::registry_unborn,
              "Main lookup refuses an unborn registry instead of reporting no task");
        auto selector=main_binding::read_retail_profile_selector(main,f);
        check(bool(selector),"source profile selector");
        source_call(main_binding::reset_and_initialize_retail_profile(main,select_native_class_profile(*selector),f),
                    f,"first real182670");
        source_call(main_binding::initialize_retail_active_overlay(main,f),f,"real GAMER overlay initialization");
        source_call(main_binding::clear_retail_mode_stack(main,f),f,"actual source mode history");
        source_call(main_binding::enter_retail_mode_zero(main,f),f,"actual mode0");
        source_call(main_binding::initialize_retail_game_records(main,f),f,"production1303F0/130350");
        check(main.participants&&main.input_roles&&main.resource_registry&&main.game_info_resources&&
              startup.game_pool&&startup.text_storage,"connected real production owners");
        RetailGameSessionFailure sf;
        auto* domain=startup.game_session->task_domain(sf);
        auto tasks=startup.game_session->initial_game_tasks(sf);
        check(domain&&tasks,"same Session task domain and native bodies");
        RetailTaskDomainFailure df;
        auto* registry=domain->registry(df);
        check(registry!=nullptr,"actual initialized task registry");
        RetailTaskRegistryFailure rf;
        auto state=registry->observe(rf);
        RetailGamePoolFailure pf;
        auto pool= startup.game_pool->observe(pf);
        check(state&&state->ordered.size()==4&&pool&&pool->active_records==4,
              "130350 published four actual task bodies using shared pool");
        const auto present=main_binding::retail_task_exists(main,state->ordered.front().kind,f);
        const auto absent=main_binding::retail_task_exists(main,0x2065,f);
        check(present&&*present&&absent&&!*absent,
              "Main uses the actual populated source registry for live and absent kinds");
        bool foreign_refused=false;
        std::thread foreign([&] {
            StartupFailure wrong_thread;
            foreign_refused=!main_binding::retail_task_exists(main,0x2065,wrong_thread)&&
                wrong_thread.game.kind==RetailGameSessionError::wrong_thread;
        });
        foreign.join();
        check(foreign_refused,"Main task lookup preserves actual Session thread ownership");
        RetailGameResourceRegistryFailure gf;
        const auto globals=main.resource_registry->observe(gf);
        check(globals&&globals->global_names==4,"185EE0 registered four actual native values");
        check(domain->dispatch_first_root_zero_flags_from_12e4e0(df),
              "first source-root dispatch executes four bodies on actual empty heads");
        RetailInitialGameTaskFailure tf;
        const auto camera=tasks->observe_camera(tf);
        check(camera&&camera->flags==0x20003u,"actual127120 BSS no-actor source path executed");
        auto text_owner=startup.text_storage;
        RetailTextStorageFailure text_failure;
        auto allocation=text_owner->allocate_text(77,text_failure);
        check(allocation&&text_owner->retire_text(*allocation,0,text_failure)==RetailStorageReleaseRoute::deferred,
              "actual installed native text operations");
        for(unsigned frame=0;frame<4;++frame)
            source_call(main_binding::retire_retail_deferred_storage(main,f),f,"production12D930 frame retirement");
        check(text_owner->observe(text_failure)->pending==0&&startup.game_pool->observe(pf)->active_records==4,
              "production frame frees text backing and shared observer record only");
        check(domain->cancel_first_root_from_12f030(df)&&startup.game_pool->observe(pf)->active_records==0,
              "real task cleanup before repeated source initialization");
        source_call(main_binding::reset_retail_game_records_130350(main,f),f,"production130350 after explicit source task cleanup");
        check(startup.text_storage==text_owner&&startup.game_session->initial_game_tasks(sf)==tasks&&
              startup.game_pool->observe(pf)->active_records==4,
              "repeat preserves actual text/camera ownership and recreates four source tasks");
        registry=domain->registry(df);
        check(registry!=nullptr,"same-session replacement root after source reset");
        state=registry->observe(rf);
        check(state&&state->ordered.size()==4,"reinitialized actual task batch");
        // Original12ED10 writes cached identity1 before12FA00; identity1 makes
        // the real cleanup condition true, exercising that effectful branch.
        auto stale=state->ordered.front().object;
        const auto stale_kind=state->ordered.front().kind;
        check(registry->store_object_cookie_from_source(stale,1,rf),"actual stale identity source write");
        const auto retired=main_binding::retail_task_exists(main,stale_kind,f);
        state=registry->observe(rf);
        check(retired&&!*retired&&state&&state->ordered.size()==3&&
              startup.game_pool->observe(pf)->active_records==3,
              "production Main lookup retires the stale body and shared link before returning not found");
        check(domain->cancel_first_root_from_12f030(df)&&startup.game_pool->observe(pf)->active_records==0,
              "second source task batch retires normally");
        std::puts("PASS_NATIVE_RETAIL_GAME_RECORDS_MAIN: actual1303F0/130350, four source bodies, effectful Main task lookup, text/frame retirement, explicit-cleanup repeat; full1861B0/game path remains open");
    } catch(const std::exception& error) {
        std::fprintf(stderr,"FAIL connected game-record main: %s\n",error.what());
        return 1;
    }
}
