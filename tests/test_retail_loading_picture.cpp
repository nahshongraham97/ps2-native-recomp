#include "mka/retail_platform.hpp"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
namespace {
void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
}
int main(int argc,char** argv){try{
    check(argc==3,"original content root and exclusive fixture root");
    const auto original_root=std::filesystem::u8path(argv[1]);
    const auto fixture=std::filesystem::u8path(argv[2]);
    check(std::filesystem::create_directory(fixture),"exclusive fixture directory");
    mka::content::ContentVolumeFailure cf;
    auto volume=mka::content::NativeContentVolume::open(original_root,cf);
    check(bool(volume),"actual retained native volume");
    mka::retail::RetailPlatformFailure f;
    auto picture=mka::retail::load_retail_loading_picture(*volume,f);
    check(picture&&picture->origin==mka::retail::RetailLoadingOrigin::asset,"actual loading asset is not a source failure pattern");
    check(picture->width==256&&picture->height==128&&picture->canvas_width==640&&picture->canvas_height==224&&
        picture->x==192&&picture->y==48&&picture->rgb.size()==98304,"original dimensions placement and stored channel extent");
    std::ifstream input(original_root/"LOADING.RAW",std::ios::binary);
    const std::vector<std::uint8_t> original((std::istreambuf_iterator<char>(input)),{});
    check(!input.bad()&&original.size()==98304&&original==picture->rgb,"every actual stored RGB byte retained in order");
    volume.reset();
    check(original==picture->rgb,"image owns its bytes after volume destruction");
    auto empty=mka::content::NativeContentVolume::open(fixture,cf);check(bool(empty),"fixture volume");
    const auto missing=mka::retail::load_retail_loading_picture(*empty,f);
    check(missing&&missing->origin==mka::retail::RetailLoadingOrigin::source_missing_file_pattern&&
        missing->rgb.size()==98304&&std::all_of(missing->rgb.begin(),missing->rgb.end(),[](auto b){return b==0x80;}),
        "source missing-file pattern is explicit; no invented alpha or asset readiness");
    {
        std::ofstream out(fixture/"LOADING.RAW",std::ios::binary);
        out.write(reinterpret_cast<const char*>(original.data()),98303);out.close();check(bool(out),"short fixture");
    }
    check(!mka::retail::load_retail_loading_picture(*empty,f)&&f.kind==mka::retail::RetailPlatformError::invalid_picture,
        "malformed RAW extent rejected before any presentation");
    {
        std::ofstream out(fixture/"LOADING.RAW",std::ios::binary|std::ios::app);out.put('\0');out.put('\0');out.close();check(bool(out),"long fixture");
    }
    check(!mka::retail::load_retail_loading_picture(*empty,f)&&f.kind==mka::retail::RetailPlatformError::invalid_picture,
        "unexpected extra stored data rejected");
    std::puts("PASS native production loading ingress: actual RAW bytes, ownership and error paths; no graphics/device/game");
    return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL native loading: %s\n",e.what());return 1;}}
