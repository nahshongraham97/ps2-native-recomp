#include "mka/retail_game_resource_registry.hpp"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string_view>
#include <thread>
#include <vector>

// Explicit CPU fixture values. Production must supply the actual typed game
// objects and parsed records at their original source callpoints.
namespace mka::retail {
class NativeRetailGameInfoResource {};
class NativeRetailHumanBoneTable {};
class NativeRetailFlippedHumanBoneTable {};
class NativeRetailNpcGroundCollisions {};
}

namespace {
void check(bool condition,const char* message) {
    if(!condition) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
}

int main() {
    using namespace mka::retail;
    using Error=RetailGameResourceRegistryError;
    RetailGameResourceRegistryFailure f;
    auto registry=NativeRetailGameResourceRegistry::create(f);
    check(bool(registry)&&f.kind==Error::none,"2173D0 native registry birth");
    const auto initial=registry->observe(f);
    check(initial&&initial->global_names==0&&initial->record_slots==20&&
          initial->active_order.empty()&&initial->free_order.size()==20,
          "source 23-bucket registry and separate twenty-record birth");
    for(std::size_t i=0;i<20;++i)
        check(initial->free_order[i]==i,"source initial free-record link order");

    struct HashCase {std::string_view name;std::uint32_t hash,bucket;};
    constexpr HashCase source_cases[]{
        {"g_game_info",0x0c2899bfu,0},
        {"lores_human_bones",0xf6755063u,7},
        {"flipped_lores_human_bones",0xf1aaf643u,10},
        {"lores_npc_ground_colls",0xffdd6083u,1}
    };
    for(const auto& row:source_cases)
        check(NativeRetailGameResourceRegistry::source_hash(row.name)==row.hash&&
              NativeRetailGameResourceRegistry::source_bucket(row.name)==row.bucket,
              "original four name hashes and buckets");
    check(NativeRetailGameResourceRegistry::source_hash("G_GAME_INFO")==
          source_cases[0].hash,"ASCII uppercase hash fold");
    check(NativeRetailGameResourceRegistry::source_hash("")==0,
          "source empty C-string hash");

    auto game=std::make_shared<NativeRetailGameInfoResource>();
    auto replacement=std::make_shared<NativeRetailGameInfoResource>();
    check(registry->insert_global(std::nullopt,NativeRetailGameResourceValue{game},f)&&
          registry->observe(f)->global_names==0,"source null-name no effect");
    check(registry->insert_global(std::string_view{"g_game_info"},
          NativeRetailGameResourceValue{game},f),"typed global insertion");
    check(registry->insert_global(std::string_view{"G_GAME_INFO"},
          NativeRetailGameResourceValue{replacement},f),"source duplicate update");
    const auto found=registry->lookup_global(std::string_view{"g_Game_Info"},f);
    check(found&&found->original_name=="g_game_info"&&
          std::get<std::shared_ptr<NativeRetailGameInfoResource>>(found->value)==replacement&&
          registry->observe(f)->global_names==1,
          "duplicate retains original spelling and replaces typed value");
    check(!registry->lookup_global(std::nullopt,f)&&f.kind==Error::none,
          "null lookup is a source miss");
    check(registry->insert_global(std::string_view{""},
          NativeRetailGameResourceValue{game},f)&&
          registry->lookup_global(std::string_view{""},f),
          "empty source name is admitted");
    check(registry->insert_global(std::string_view{"abc\0ignored",11},
          NativeRetailGameResourceValue{game},f)&&
          registry->lookup_global(std::string_view{"ABC"},f),
          "C-string truncation and ASCII comparison");
    check(!registry->insert_global(std::string_view{"bad"},
          NativeRetailGameResourceValue{},f)&&f.kind==Error::missing_value,
          "no successful placeholder resource value");

    std::vector<RetailGameResourceRecordHandle> handles;
    for(std::size_t i=0;i<21;++i) {
        RetailStringDataFailure data;
        auto record=registry->activate_loaded_record(
            NativeRetailLoadedResource::prepare_permanent_strings(0,data),f);
        check(record&&record->slot==i,"source free order and native growth after twenty");
        handles.push_back(*record);
    }
    const auto full=registry->observe(f);
    check(full&&full->record_slots==21&&full->free_order.empty()&&
          full->active_order.front()==20&&full->active_order.back()==0,
          "source active head prepend and dynamic host storage");
    auto retained=registry->retained_loaded_record(handles[7],f);
    check(bool(retained),"active record retains real queued native owner");
    check(registry->retire_loaded_record(handles[7],f),"217FC0 record unlink");
    check(!registry->retained_loaded_record(handles[7],f)&&
          f.kind==Error::stale_record,"retired token cannot see reused content");
    RetailStringDataFailure data;
    auto reused=registry->activate_loaded_record(
        NativeRetailLoadedResource::prepare_permanent_strings(0,data),f);
    check(reused&&reused->slot==7&&reused->generation!=handles[7].generation,
          "retired source slot returns to free head with fresh native token");
    auto foreign=NativeRetailGameResourceRegistry::create(f);
    check(foreign&&!foreign->retire_loaded_record(*reused,f)&&
          f.kind==Error::foreign_record,"record identity is registry-bound");

    bool foreign_refused=false;
    std::thread worker([&] {
        RetailGameResourceRegistryFailure other;
        foreign_refused=!registry->lookup_global(std::string_view{"g_game_info"},other)&&
            other.kind==Error::wrong_thread;
    });
    worker.join();
    check(foreign_refused&&registry->lookup_global(std::string_view{"g_game_info"},f),
          "wrong-thread refusal leaves owner usable");
    std::puts("PASS native game resource registry CPU source rules; typed producers and game open");
}
