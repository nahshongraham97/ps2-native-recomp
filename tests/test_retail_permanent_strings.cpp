#include "mka/retail_permanent_strings.hpp"
#include "mka/retail_loaded_system_art.hpp"
#include "mka/retail_game_info_resources.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"
#include "native_test_sha256.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <new>
#include <stdexcept>
#include <thread>
#define NOMINMAX
#include <windows.h>
using namespace mka::retail;
extern "C" int SDL_InitSubSystem(unsigned){std::abort();}
namespace {thread_local int allocation_cut=-1;}
void* operator new(std::size_t n){
    if(allocation_cut>=0&&allocation_cut--==0)throw std::bad_alloc();
    if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();
}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::string sha(std::span<const std::uint8_t> b){NativeTestSha256 h;h.add(b.data(),b.size());return h.finish();}
struct Expected {std::size_t length{};std::string hash;};
using ExpectedLanguage=std::array<Expected,78>;
std::array<ExpectedLanguage,5> expectations(const char* path){
    std::ifstream f(path);check(bool(f),"independent original expectations");
    std::array<ExpectedLanguage,5> result;
    unsigned language=0,index=0,count=0;std::size_t length=0;std::string digest;
    while(f>>language>>index>>length>>digest){
        check(language<5&&index<78,"bounded expectation row");
        result[language][index]={length,digest};++count;
    }
    check(count==390,"all independent original strings");return result;
}
std::vector<std::uint8_t> original_english(const std::filesystem::path& root){
    std::ifstream f(root/"art/gameart.ssf",std::ios::binary);
    f.seekg(0x1e5800);std::vector<std::uint8_t> b(1760);
    f.read(reinterpret_cast<char*>(b.data()),b.size());check(std::size_t(f.gcount())==b.size(),"original English range");
    check(sha(b)=="a8e31af662530f0b5c63db3d8f9feb5ca3502c29010fa3889300d6c67b3bbe24","original English payload hash");return b;
}
void put(std::vector<std::uint8_t>& b,std::size_t at,std::uint32_t v){for(unsigned i=0;i<4;++i)b[at+i]=std::uint8_t(v>>(8*i));}
struct Game {
    mka::input::ControllerInput input;
    std::unique_ptr<NativeRetailStartupState> startup;
    std::unique_ptr<NativeRetailMainState> main;
    explicit Game(const std::filesystem::path& root){
        mka::content::ContentVolumeFailure cf;
        auto volume=mka::content::NativeContentVolume::open(root,cf);check(bool(volume),"actual content volume");
        startup=std::make_unique<NativeRetailStartupState>(std::move(*volume),std::vector<std::string>{});
        StartupFailure f;check(initialize_native_retail_game_session(*startup,f),"actual persistent Session");
        main=std::make_unique<NativeRetailMainState>(*startup,input);
        RetailFilePathFailure pf;main->file_paths=NativeRetailFilePaths::create(pf);
        RetailPackageFailure package;
        main->package=NativeRetailPackage::open(startup->content,*main->file_paths,package);
        check(bool(main->package),"actual package");
        RetailGameResourceRegistryFailure rf;main->resource_registry=NativeRetailGameResourceRegistry::create(rf);
        check(bool(main->resource_registry),"actual source resource registry");
    }
    void close(){
        if(!main->permanent_strings)return;
        RetailPermanentStringFailure f;const auto deadline=GetTickCount64()+10000;
        for(;;){
            const auto result=main->permanent_strings->shutdown(f);
            check(bool(result),"actual record/job cleanup");
            if(*result==RetailPermanentStringStep::complete)return;
            check(GetTickCount64()<deadline,"bounded cancellation");Sleep(1);
        }
    }
};
}
int main(int argc,char** argv)try{
    check(argc==4,"loose root, original expectations TSV, exclusive fixture root");
    const auto expected=expectations(argv[2]);
    const auto root=std::filesystem::path(argv[3]);
    check(!std::filesystem::exists(root),"unique fixtures only");std::filesystem::create_directories(root);
    StartupFailure sf;RetailPermanentStringFailure f;RetailGameResourceRegistryFailure rf;RetailStringDataFailure df;
    for(unsigned language=0;language<5;++language){
        Game g(argv[1]);g.main->post_overlay_state=language;
        check(loaded_art_required::queue_permanent_strings_215660(*g.main,sf),"real215660 producer");
        const auto queued=g.main->permanent_strings->observe(f);
        check(queued&&queued->record_published&&queued->job_retained&&!queued->ready,"real queued record precedes parsing");
        const auto first=*queued->record;
        check(!g.main->resource_registry->lookup_string_table_from_215ed0("string_list",rf)&&rf.kind==RetailGameResourceRegistryError::none,"queued record is not parsed success");
        g.main->post_overlay_state=0xffffffffu;
        check(loaded_art_required::queue_permanent_strings_215660(*g.main,sf),"existing tag bypasses invalid locale selection");
        check(g.main->permanent_strings->observe(f)->record->generation==first.generation,"repeat request keeps same source association");
        check(loaded_art_required::complete_permanent_strings_215730(*g.main,sf),"real215730 linked data producer");
        const auto ready=g.main->permanent_strings->observe(f);
        check(ready&&ready->ready&&!ready->job_retained,"actual job release before ready");
        auto binding=g.main->resource_registry->lookup_string_table_from_215ed0("string_list",rf);
        check(bool(binding),"actual loaded export lookup");
        check(!g.main->resource_registry->lookup_string_table_from_215ed0("STRING_LIST",rf)&&rf.kind==RetailGameResourceRegistryError::none,"loaded names use source exact comparison");
        check(g.main->resource_registry->lookup_string_table_from_215ed0(std::string_view("string_list\0ignored",19),rf).has_value(),"source C-string boundary");
        check(!g.main->resource_registry->lookup_string_table_from_215ed0(std::nullopt,rf)&&rf.kind==RetailGameResourceRegistryError::none,"null source lookup miss");
        for(unsigned i=0;i<78;++i){
            const auto value=binding->value(i,df);
            check(value&&!value->is_null()&&value->bytes().size()==expected[language][i].length&&
                  sha(value->bytes())==expected[language][i].hash,"all390 exact original text values");
        }
        check(!binding->value(78,df)&&df.kind==RetailStringDataError::invalid_index,"upper source array boundary");
        check(!binding->value(UINT64_MAX,df)&&df.kind==RetailStringDataError::invalid_index,"negative-as-wide index does not wrap");
        auto value=binding->value(0,df);auto moved=std::move(*value);
        check(sha(moved.bytes())==expected[language][0].hash&&sha(value->bytes())==expected[language][0].hash,"immutable native value move retains actual bytes");
        bool wrong=false;
        std::thread worker([&]{RetailGameResourceRegistryFailure other;wrong=!g.main->resource_registry->lookup_string_table_from_215ed0("string_list",other)&&other.kind==RetailGameResourceRegistryError::wrong_thread;});
        worker.join();check(wrong,"wrong-thread lookup refuses before record access");
        g.close();
        check(!binding->value(0,df)&&df.kind==RetailStringDataError::retired,"retirement invalidates retained typed binding");
        check(sha(moved.bytes())==expected[language][0].hash,"already acquired immutable text value survives resource cleanup");
        check(!g.main->resource_registry->retained_loaded_record(first,rf)&&rf.kind==RetailGameResourceRegistryError::stale_record,"actual source record retired");
        g.main->post_overlay_state=language;
        check(loaded_art_required::queue_permanent_strings_215660(*g.main,sf),"explicit retirement allows new real request");
        const auto second=*g.main->permanent_strings->observe(f)->record;
        check(second.slot==first.slot&&second.generation!=first.generation,"real free-head reuse has new native generation");
        check(loaded_art_required::complete_permanent_strings_215730(*g.main,sf),"reloaded source association");
        auto escaped=g.main->resource_registry->lookup_string_table_from_215ed0("string_list",rf);
        g.main.reset();
        check(!escaped->value(0,df)&&df.kind==RetailStringDataError::retired,"Main cleanup retires record before registry/lifetime destruction");
    }
    const auto original=original_english(argv[1]);
    auto rejects=[&](std::vector<std::uint8_t> bytes,RetailStringDataError expected_error){
        auto image=NativeRetailPermanentStringImage::decode(std::move(bytes),df);
        check(!image&&df.kind==expected_error,"named original-byte mutation refusal");
    };
    {auto b=original;put(b,0,1);rejects(std::move(b),RetailStringDataError::unsupported_functions);}
    {auto b=original;put(b,4,4);rejects(std::move(b),RetailStringDataError::unsupported_tail);}
    {auto b=original;put(b,36,0);rejects(std::move(b),RetailStringDataError::malformed_name);}
    {auto b=original;put(b,40,0);rejects(std::move(b),RetailStringDataError::unsupported_variables);}
    {auto b=original;put(b,52,0x40000000u);rejects(std::move(b),RetailStringDataError::truncated);}
    {auto b=original;put(b,44,0xffffffffu);rejects(std::move(b),RetailStringDataError::malformed_reference);}
    {auto b=original;for(std::size_t i=0x170;i<0x17c;++i)b[i]='X';rejects(std::move(b),RetailStringDataError::malformed_name);}
    {auto b=original;b[0x58c+4]=2;rejects(std::move(b),RetailStringDataError::unsupported_type);}
    {auto b=original;b[0x58c+5]=6;rejects(std::move(b),RetailStringDataError::unsupported_type);}
    {auto b=original;put(b,0x594+4,79);rejects(std::move(b),RetailStringDataError::malformed_reference);}
    {auto b=original;put(b,56,0xffffffffu);rejects(std::move(b),RetailStringDataError::malformed_string);}
    {auto b=original;put(b,0x594+4,0);auto image=NativeRetailPermanentStringImage::decode(std::move(b),df);auto value=image->value(0,df);check(value&&value->is_null(),"source zero reference is null, not an invented string");}
    {auto b=original;b[0x17c]=0;auto image=NativeRetailPermanentStringImage::decode(std::move(b),df);auto value=image->value(0,df);check(value&&!value->is_null()&&value->bytes().empty(),"nonnull empty source string differs from null");}
    {auto b=original;put(b,32,0xffffffffu);auto image=NativeRetailPermanentStringImage::decode(std::move(b),df);check(image&&image->size()==78,"2170E0 ignores overwritten serialized pointer placeholder");}
    {auto b=original;put(b,48,0);auto image=NativeRetailPermanentStringImage::decode(std::move(b),df);check(image&&image->size()==0,"verified zero-element data case");}
    unsigned data_cuts=0;
    for(int cut=0;cut<32;++cut){
        auto bytes=original;allocation_cut=cut;
        auto image=NativeRetailPermanentStringImage::decode(std::move(bytes),df);allocation_cut=-1;
        if(image)break;
        check(df.kind==RetailStringDataError::allocation,"decoder allocation failure retained");++data_cuts;
    }
    check(data_cuts>=4&&data_cuts<32,"native decoder allocation cuts");
    unsigned queue_cuts=0;
    for(int cut=0;cut<32;++cut){
        Game g(argv[1]);allocation_cut=cut;
        const bool result=loaded_art_required::queue_permanent_strings_215660(*g.main,sf);allocation_cut=-1;
        if(result){g.close();break;}
        check(sf.kind==StartupError::game_records,"producer allocation failures named at required boundary");
        ++queue_cuts;g.close();
    }
    check(queue_cuts>=6&&queue_cuts<32,"real record/job producer allocation cuts");
    {
        const auto empty=root/"missing";std::filesystem::create_directories(empty/"art");Game g(empty);
        check(!loaded_art_required::queue_permanent_strings_215660(*g.main,sf),"missing source refuses without PAK/English fallback");
        const auto failed=g.main->permanent_strings->observe(f);
        check(failed&&failed->failed&&failed->record_published&&!failed->job_retained,"real published prefix survives IO admission failure");
        g.close();check(g.main->resource_registry->observe(rf)->active_order.empty(),"failed queued prefix released");
    }
    {
        Game g(argv[1]);g.main->post_overlay_state=5;
        check(!loaded_art_required::queue_permanent_strings_215660(*g.main,sf),"unproved locale refuses before record publication");
        check(g.main->resource_registry->observe(rf)->active_order.empty(),"failed descriptor leaves registry untouched");
        g.main->post_overlay_state=0;check(loaded_art_required::complete_permanent_strings_215730(*g.main,sf),"complete performs actual request when tag absent");
        RetailGameInfoResourceFailure info_failure;
        auto info=NativeRetailGameInfoResources::create(g.startup->game_session,info_failure);
        check(info&&g.main->resource_registry->insert_global("STRING_LIST",NativeRetailGameResourceValue{info->game_info(info_failure)},rf),"actual typed global collision");
        check(!g.main->resource_registry->lookup_string_table_from_215ed0("string_list",rf)&&rf.kind==RetailGameResourceRegistryError::type_mismatch,"native global precedence is not replaced by loaded export");
        g.close();
    }
    {
        Game g(argv[1]);check(loaded_art_required::queue_permanent_strings_215660(*g.main,sf),"pending lifetime request");
        g.main.reset(); // Destructor cancels and observes terminal IO before freeing its record.
    }
    {
        Game g(argv[1]);check(loaded_art_required::queue_permanent_strings_215660(*g.main,sf),"queued original file identity");
        g.main->package.reset();g.main->file_paths.reset();
        check(loaded_art_required::complete_permanent_strings_215730(*g.main,sf),"actual job owns file identity after package retirement");
        const auto binding=g.main->resource_registry->lookup_string_table_from_215ed0("string_list",rf);
        const auto value=binding->value(0,df);
        check(value&&sha(value->bytes())==expected[0][0].hash,"post-package actual text identity");g.close();
    }
    {
        Game g(argv[1]);check(loaded_art_required::queue_permanent_strings_215660(*g.main,sf),"escaped producer pending IO");
        const auto retained_retired_witness=g.main->lifetime;
        auto escaped=std::move(g.main->permanent_strings);
        g.main.reset();
        check(!retained_retired_witness->active_on_owner_thread(),"retained witness does not retain Main");
        check(!escaped->observe(f)&&f.kind==RetailPermanentStringError::main_lifetime,"escaped producer refuses destroyed Main");
        const auto deadline=GetTickCount64()+10000;
        for(;;){
            const auto result=escaped->shutdown(f);
            check(bool(result),"escaped native IO cleanup never dereferences retired Main");
            if(*result==RetailPermanentStringStep::complete)break;
            check(GetTickCount64()<deadline,"escaped cancellation terminal");Sleep(1);
        }
    }
    std::printf("PASS real215660/215730 worker/registry/lookup/lifetime, all390 original strings, null/empty/malformed and%u data/%u producer allocation cuts; GPU=false window=false audio=false; game acceptance OPEN\n",data_cuts,queue_cuts);
    return 0;
}catch(const std::exception& e){allocation_cut=-1;std::fprintf(stderr,"FAIL permanent strings: %s\n",e.what());return 1;}
