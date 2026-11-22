#include "mka/retail_sound_metadata.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"
#include <bit>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <thread>
extern "C" int SDL_InitSubSystem(unsigned) { std::abort(); }
using namespace mka::retail;
namespace { void check(bool okay,const char* what) {if(!okay) throw std::runtime_error(what);} }
int main(int argc,char** argv) try {
    check(argc==3,"requires original content root and original-derived metadata corpus");
    RetailSoundMetadataFailure f;
    check(!decode_retail_semantic_sound_metadata(0xffffffffu,f)&&f.kind==RetailSoundMetadataError::invalid_semantic_id,"all-ones sentinel source refusal");
    std::ifstream corpus(argv[2]);std::string line;std::getline(corpus,line);
    unsigned i,s,p,c,g,priority,scalar;int output;unsigned rows{};
    while(corpus>>i>>s>>p>>c>>g>>priority>>output>>scalar) {
        const auto x=decode_retail_semantic_sound_metadata(i,f);
        check(x&&x->sound_index==i&&x->bank_index==0&&x->selector==s&&x->percent==p&&x->category==c&&x->gain_index==g&&x->priority==priority&&x->output==output&&x->category_scalar_bits==scalar,"independent original data field corpus");++rows;
    }
    check(rows==9656,"full9656 original sound rows checked");
    for(const auto id:{0x015725b7u,0x015825b7u,0x015725b8u,0xffff0001u,0x0000ffffu,0xfffffffeu}) {
        const auto x=decode_retail_semantic_sound_metadata(id,f);
        const unsigned expected_bank=(id>>16)<344?id>>16:0;
        const unsigned expected_sound=(id&65535)<9656?id&65535:0;
        check(x&&x->semantic_id==id&&x->bank_index==expected_bank&&x->sound_index==expected_sound,"bank and sound indices independently normalize");
    }
    mka::content::ContentVolumeFailure cf;
    auto volume=mka::content::NativeContentVolume::open(std::filesystem::path(argv[1]),cf);
    check(bool(volume),"real content volume");
    NativeRetailStartupState startup(std::move(*volume),{});StartupFailure sf;
    check(initialize_native_retail_game_session(startup,sf),"real retained GameSession");
    mka::input::ControllerInput input;NativeRetailMainState main(startup,input);
    check(!resolve_retail_semantic_sound_operands(main,0,f)&&f.kind==RetailSoundMetadataError::missing_catalog,"missing catalog is not bank presence");
    check(main_binding::initialize_retail_current_profile_preferences(main,sf),"actual295660 preference producer");
    for(unsigned n=0;n<6;++n) check(read_retail_sound_gain_operand_bits(*startup.game_session,n,f)==(n==5?0x3f800000u:0x3f400000u),"five default levels and sixth word_14 alias");
    check(!read_retail_sound_gain_operand_bits(*startup.game_session,6,f)&&f.kind==RetailSoundMetadataError::invalid_gain_index,"sixth boundary is exclusive");
    RetailFilePathFailure pf;main.file_paths=NativeRetailFilePaths::create(pf);check(bool(main.file_paths),"actual native path owner");
    RetailSoundCatalogFailure catalog_failure;
    main.sound_bank_catalog=NativeRetailSoundBankCatalog::open(startup.content,*main.file_paths,main.lifetime,catalog_failure);
    check(bool(main.sound_bank_catalog),"existing actual catalog adopts one archive pair");
    check(!resolve_retail_semantic_sound_operands(main,0,f)&&f.kind==RetailSoundMetadataError::bank_not_present,"unloaded bank refuses without fake handle");
    check(main.sound_bank_catalog->select_mode_one(catalog_failure),"actual catalog requests");
    check(!resolve_retail_semantic_sound_operands(main,0,f)&&f.kind==RetailSoundMetadataError::bank_not_present,"pending bank has no published handle");
    unsigned callbacks{};
    for(;;) {const auto pending=main.sound_bank_catalog->pending(catalog_failure);check(bool(pending),"actual catalog pending observation");if(!*pending) break;check(main.sound_bank_catalog->pump_one(catalog_failure),"actual bank worker/callback");++callbacks;}
    check(callbacks==4,"four existing mode-one callback deliveries");
    auto shared=main.sound_bank_catalog->share_bank_session(catalog_failure);check(bool(shared),"same existing catalog Session");
    RetailGameSessionFailure gf;auto* values=startup.game_session->game_records_values(gf);check(values!=nullptr,"actual current preference values");
    const std::uint32_t operand_bits[]{0x80000000u,0x3f800000u,0x3f400000u,0x7fc12345u,0x3dcccccd};
    for(unsigned n=0;n<5;++n) values->profile_preferences.levels[n]=std::bit_cast<float>(operand_bits[n]);
    values->profile_preferences.word_14=0xff812345u;
    check(read_retail_sound_gain_operand_bits(*startup.game_session,5,f)==0xff812345u,"sixth raw operand bits are not numeric integer conversion");
    std::uint32_t selected{};bool found=false;
    for(unsigned n=0;n<9656;++n) {
        auto x=resolve_retail_semantic_sound_operands(main,n,f);check(bool(x),"loaded bank metadata resolution");
        const auto row=main.sound_bank_catalog->observe(0,catalog_failure);
        check(row&&row->bank&&x->bank.same_identity(*row->bank)&&x->current_gain_bits==operand_bits[x->metadata.gain_index],"actual callback identity and current preference operands");
        if(x->metadata.gain_index==3&&!found) {selected=n;found=true;}
    }
    check(found,"pinned table reaches live gain3");
    values->profile_preferences.levels[3]=std::bit_cast<float>(0x3e800000u);
    const auto changed=resolve_retail_semantic_sound_operands(main,selected,f);
    check(changed&&changed->current_gain_bits==0x3e800000u,"next resolution observes same live owner mutation");
    for(const unsigned bank:{0u,1u,2u,8u}) {
        const auto x=resolve_retail_semantic_sound_operands(main,bank<<16,f);const auto row=main.sound_bank_catalog->observe(bank,catalog_failure);
        mka::sound::AudioBanksFailure bf;
        check(x&&row&&row->bank&&x->bank.same_identity(*row->bank)&&shared->load_state(x->bank,bf).has_value(),"all real mode-one callback handles resolve in same Session");
    }
    check(!resolve_retail_semantic_sound_operands(main,3u<<16,f)&&f.kind==RetailSoundMetadataError::bank_not_present,"other catalog bank remains unloaded");
    const auto normalized=resolve_retail_semantic_sound_operands(main,0x01580001u,f);const auto zero=resolve_retail_semantic_sound_operands(main,1,f);
    check(normalized&&zero&&normalized->bank.same_identity(zero->bank),"normalized bank resolves actual bank0 identity");
    const auto retained_session=startup.game_session;startup.game_session.reset();
    check(!resolve_retail_semantic_sound_operands(main,0,f)&&f.kind==RetailSoundMetadataError::missing_session,"missing canonical game owner");startup.game_session=retained_session;
    bool refused=false;std::thread other([&]{RetailSoundMetadataFailure local;refused=!resolve_retail_semantic_sound_operands(main,0,local)&&local.kind==RetailSoundMetadataError::main_lifetime&&!read_retail_sound_gain_operand_bits(*retained_session,0,local)&&local.kind==RetailSoundMetadataError::game_session;});other.join();check(refused,"actual Main and Session wrong-thread refusal");
    check(main.sound_bank_catalog->shutdown(catalog_failure),"real catalog handle retirement");
    check(!resolve_retail_semantic_sound_operands(main,0,f)&&f.kind==RetailSoundMetadataError::catalog,"closed actual catalog refusal");
    main.lifetime->retire();check(!resolve_retail_semantic_sound_operands(main,0,f)&&f.kind==RetailSoundMetadataError::main_lifetime,"retired Main lifetime refusal");
    std::puts("PASS sound metadata only:9656 original rows,4 real callback handles,current Session operands,normalization,thread/lifetime; arithmetic/playback OPEN");return 0;
} catch(const std::exception& e) {std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
