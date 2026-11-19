// Pure semantic request test. No assets, decoder, devices or guest runtime.
#include "mka/movie_catalog.hpp"
#include "mka/movie_subtitles.hpp" // public headers must compose without enum duplication
#include <array>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <utility>
using namespace mka::video;
namespace {
void check(bool condition,const char* label) {if(!condition) throw std::runtime_error(label);}
const char* view_name(MovieViewSelection view) {
    return view == MovieViewSelection::normal ? "normal" : "alternate";
}
RetailMovieRequest resolve(const RetailMovieCatalog& catalog,std::int64_t id,
                          MovieViewSelection view,std::int64_t language=0) {
    MovieCatalogFailure failure;
    auto request=catalog.resolve(id,view,language,failure);
    check(request.has_value() && failure.kind==MovieCatalogError::none,"owned request resolves");
    return std::move(*request);
}
}
int main() try {
    RetailMovieCatalog catalog;
    MovieCatalogFailure failure;
    for(std::int64_t id : std::array<std::int64_t,4>{-1,33,
            std::numeric_limits<std::int64_t>::min(),std::numeric_limits<std::int64_t>::max()}) {
        check(!catalog.resolve(id,MovieViewSelection::normal,0,failure) &&
              failure.kind==MovieCatalogError::invalid_id,"native64 invalid ID fails before lookup");
        check(!catalog.metadata(id,failure) && failure.kind==MovieCatalogError::invalid_id,
              "metadata invalid ID has no default row");
    }
    check(!catalog.resolve(0,static_cast<MovieViewSelection>(99),0,failure) &&
          failure.kind==MovieCatalogError::invalid_view,"unknown typed view fails");
    check(resolve(catalog,0,MovieViewSelection::normal).logical_key=="LOGON.SFD","first exact key");
    check(resolve(catalog,32,MovieViewSelection::alternate).logical_key=="KRYPT/V_MOTOGI.SFD",
          "last row absent alternate intentionally selects its normal variant");
    check(resolve(catalog,7,MovieViewSelection::normal).logical_key=="BLITZN.SFD" &&
          resolve(catalog,7,MovieViewSelection::alternate).logical_key=="BLITZW.SFD" &&
          resolve(catalog,31,MovieViewSelection::alternate).logical_key=="KRYPT/V_FIN.SFD",
          "missing source keys remain exact semantic requests, no asset alias");
    for(std::int64_t code : std::array<std::int64_t,9>{0,1,2,3,4,-1,5,
            std::numeric_limits<std::int64_t>::min(),std::numeric_limits<std::int64_t>::max()}) {
        const char* tag=code==1?"<SPA>":code==2?"<GER>":code==3?"<FRE>":code==4?"<ITA>":"<ENG>";
        const auto expected=code==1?MovieSubtitleLanguage::spanish:code==2?MovieSubtitleLanguage::german:
            code==3?MovieSubtitleLanguage::french:code==4?MovieSubtitleLanguage::italian:MovieSubtitleLanguage::english;
        for(auto view : {MovieViewSelection::normal,MovieViewSelection::alternate}) {
            const auto request=resolve(catalog,1,view,code);
            check(request.subtitle_tag==tag && request.subtitle_language==expected,
                  "source language mapping independent of view/enum ordinals");
        }
    }
    auto retained=[] {
        RetailMovieCatalog temporary;
        MovieCatalogFailure failure;
        auto request=resolve(temporary,0,MovieViewSelection::alternate,1);
        auto metadata=temporary.metadata(0,failure); // own metadata too
        check(metadata.has_value(),"metadata owns source tails");
        check(metadata->opaque_tail_normal=="LOGO6N.SFD" && metadata->opaque_tail_alternate=="LOGO6W.SFD",
              "opaque tail names retained without active selection");
        metadata->normal.logical_key="changed metadata";
        auto moved=std::move(temporary);
        check(resolve(moved,0,MovieViewSelection::normal).logical_key=="LOGON.SFD","catalog move retains immutable data");
        return request;
    }();
    check(retained.logical_key=="LOGOW.SFD" && retained.subtitle_tag=="<SPA>","request survives catalog/metadata destruction");
    auto copied=retained;copied.logical_key="different";copied.subtitle_tag="changed";
    check(retained.logical_key=="LOGOW.SFD" && retained.subtitle_tag=="<SPA>","request copies own strings");
    for(std::int64_t id=0;id<33;++id) for(auto view : {MovieViewSelection::normal,MovieViewSelection::alternate}) {
        const auto request=resolve(catalog,id,view);
        check(request.route==MovieRoute::blocking && request.route_extra==0,"all actual source rows use blocking route");
        check(request.logical_extent.width && request.logical_extent.height,"native64 logical extents positive");
        check(request.requested_view==view && request.subtitle_tag=="<ENG>","explicit requested view/language retained");
        // Compiled source keys/tags contain no JSON escaping characters, as
        // validated in the generated source/catalog. Exactly66 records stdout.
        std::printf("{\"id\":%u,\"requested_view\":\"%s\",\"selected_view\":\"%s\",\"logical_key\":\"%s\",\"logical_width\":%llu,\"logical_height\":%llu,\"route\":\"blocking\",\"route_extra\":%u,\"language\":\"english\",\"subtitle_tag\":\"%s\"}\n",
            request.id,view_name(request.requested_view),view_name(request.selected_view),request.logical_key.c_str(),
            static_cast<unsigned long long>(request.logical_extent.width),static_cast<unsigned long long>(request.logical_extent.height),
            request.route_extra,request.subtitle_tag.c_str());
    }
    check(std::fflush(stdout)==0 && !std::ferror(stdout),"JSONL output write");
    std::fputs("PASS owned retail semantic catalog; asset/factory/player/game binding open\n",stderr);
    return 0;
} catch(const std::exception& error) {
    std::fprintf(stderr,"FAIL movie catalog: %s\n",error.what());return 1;
}
