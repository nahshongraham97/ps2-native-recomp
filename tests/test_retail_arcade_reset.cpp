#include "mka/retail_arcade_ladder.hpp"
#include "mka/retail_initial_reset_required.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <thread>
extern "C" int SDL_InitSubSystem(unsigned) { std::abort(); }
using namespace mka::retail;
namespace {
void check(bool value, const char* why) { if (!value) throw std::runtime_error(why); }
struct RngCase {std::uint32_t seed, selected, next;};
// Independent original RNG reference vectors, with source185C30(19) scaling.
// The firsttwo draws do not reach a twist after any seed in this short probe.
constexpr std::array<RngCase, 4> cases{{
    {0u,5u,4267234555u}, {3735928559u,6u,2369570450u},
    {1u,0u,1728641009u}, {1985229328u,13u,818682336u}}};
void check_data(const char* path) {
    std::ifstream file(path); std::string line;
    check(bool(std::getline(file,line)) && line=="ladder\ttier\tword0\tword1\tword2\tword3", "source data corpus header");
    const auto& all=retail_arcade_ladders_from_source();
    for (unsigned ladder=0;ladder<all.size();++ladder) for (unsigned tier=0;tier<all[ladder].size();++tier) {
        check(bool(std::getline(file,line)), "source row exists"); std::istringstream row(line);
        unsigned l=99,t=99; std::uint64_t a=0,b=0,c=0,d=0; row>>l>>t>>a>>b>>c>>d;
        check(bool(row) && l==ladder && t==tier, "source row order");
        check(all[ladder][tier].words==std::array<std::uint32_t,4>{static_cast<std::uint32_t>(a),static_cast<std::uint32_t>(b),static_cast<std::uint32_t>(c),static_cast<std::uint32_t>(d)}, "all152 actual original ladder rows");
    }
    check(!std::getline(file,line), "no extra source data row");
}
}
int main(int argc,char** argv) try {
    check(argc==3,"requires existing content directory and original ladder corpus"); check_data(argv[2]);
    mka::content::ContentVolumeFailure vf;
    auto volume=mka::content::NativeContentVolume::open(std::filesystem::path(argv[1]),vf); check(bool(volume),"native volume");
    NativeRetailStartupState startup(std::move(*volume),{}); StartupFailure f;
    check(initialize_native_retail_game_session(startup,f),"actual persistent Session");
    const auto session=startup.game_session; mka::input::ControllerInput input; NativeRetailMainState main(startup,input);
    RetailGameSessionFailure sf; auto* selection=session->arcade_ladder_values(sf);
    auto* records=session->game_records_values(sf); auto* random=session->random(sf); RetailRandomFailure rf;
    check(selection && selection->progress==0 && selection->tier==0 && selection->selected_index==0xffffffffu && !selection->active_ladder,"actual initialized-data/BSS births");
    const auto& all=retail_arcade_ladders_from_source();
    for (const auto& expected:cases) {
        check(random->seed(expected.seed,rf),"same actual RNG seed fixture");
        selection->progress=71;selection->tier=7;selection->selected_index=0xffff1234u;selection->active_ladder=&all[18];
        records->word_6602e8=0xa5a5a5a5u;
        check(initial_reset_required::reset_selected_resource_334700(main,f),"real334700 reset");
        check(selection->progress==-1 && selection->tier==0 && selection->selected_index==expected.selected && selection->active_ladder==&all[expected.selected] && records->word_6602e8==0,"source full-field stores and actual pointer publication");
        check(random->next(rf)==expected.next,"exactlyone draw from same retained Session RNG");
    }
    auto fill=[&]{selection->progress=29;selection->tier=6;selection->selected_index=3;selection->active_ladder=&all[3];records->word_6602e8=91;check(random->seed(1,rf),"guard probe seed");};
    auto unchanged=[&]{return selection->progress==29 && selection->tier==6 && selection->selected_index==3 && selection->active_ladder==&all[3] && records->word_6602e8==91;};
    fill();bool refused=false;std::thread worker([&]{StartupFailure bad;refused=!initial_reset_required::reset_selected_resource_334700(main,bad)&&bad.kind==StartupError::game_session;});worker.join();
    check(refused&&unchanged()&&random->next(rf)==4194449u,"wrong thread before store or RNG draw");
    fill();startup.game_session.reset();check(!initial_reset_required::reset_selected_resource_334700(main,f)&&unchanged()&&random->next(rf)==4194449u,"missing canonical Session refuses without touching retained owner");startup.game_session=session;
    fill();main.lifetime->retire();check(!initial_reset_required::reset_selected_resource_334700(main,f)&&unchanged()&&random->next(rf)==4194449u,"retired Main before stores or RNG");
    NativeRetailMainState next(startup,input);check(next.startup.game_session->arcade_ladder_values(sf)==selection&&unchanged(),"real ladder values survive Main recreation");
    check(initial_reset_required::reset_selected_resource_334700(next,f)&&selection->progress==-1,"new Main continues same live native ladder owner");
    std::puts("PASS real334700 arcade reset:152 original rows,exact same-Session RNG draw,shared fields,initialized births,thread/Main/Session lifetime;arcade progression not implemented");return 0;
} catch(const std::exception& error) {std::fprintf(stderr,"FAIL %s\n",error.what());return 1;}
