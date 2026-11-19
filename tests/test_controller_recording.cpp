#include "ps2/system.hpp"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"recording check %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char** argv) {
    if(argc!=3){std::fprintf(stderr,"usage: test_controller_recording v1|v2|bad <NEW directory>\n");return 2;}
    const std::string mode(argv[1]);
    const bool bad=mode=="bad"||mode=="bad-row"||mode=="bad-range"||mode=="bad-buttons"||
        mode=="bad-version"||mode=="bad-port"||mode=="bad-connection"||mode=="bad-negative";
    CHECK(mode=="v1"||mode=="v2"||bad);
    const auto directory=std::filesystem::path(argv[2]);
    CHECK(std::filesystem::create_directory(directory));
    auto held=mka::input::digital_state(0x4100,true,{{17,29,73,201}},73,0);
    if(mode!="v1")held.pressure[14]=37;
    const auto released=mka::input::digital_state(0,true);
    const auto file=directory/"inputs.txt";
    {
        std::ofstream out(file);
        out<<"ps2-inputs "<<(mode=="v1"?1:mode=="bad-version"?20:2)<<"\nstart power-on frame 0\n";
        if(mode=="bad-row")out<<"p1 1 4100";
        else out<<(mode=="bad-port"?"p3":"p1")<<" 1 "<<(mode=="bad-buttons"?"10000":"4100")
            <<" "<<(mode=="bad-range"?"256":mode=="bad-negative"?"-1":"17")
            <<" 29 73 201 73 0 "<<(mode=="bad-connection"?"2":"1");
        if(mode!="v1"&&mode!="bad")out<<" "<<mka::input::encode_pressure(held.pressure);
        if(mode=="bad")out<<" 0000";
        out<<"\np1 3 0000 128 128 128 128 0 0 1";
        if(mode!="v1")out<<" "<<mka::input::encode_pressure(released.pressure);
        out<<"\ne 5\n";
        CHECK(bool(out));
    }
    ps2::g_options.replay_path=file.string();
    if(bad){CHECK(!ps2::replay_begin()&&ps2::replay_status()==5);return 0;}
    CHECK(ps2::replay_begin());
    ps2::publish_controller_frame(1);
    auto first=mka::input::controllers().sample(0);
    CHECK(first&&first->state.buttons==held.buttons&&first->state.axes==held.axes&&first->state.pressure==held.pressure);
    // A live host update cannot erase a replay hold on a frame with no event.
    ps2::set_pad_state(mka::input::digital_state(0x8000,true));
    ps2::publish_controller_frame(2);
    auto middle=mka::input::controllers().sample(0);
    CHECK(middle&&middle->sequence==first->sequence&&middle->state.pressure==held.pressure);
    ps2::publish_controller_frame(3);
    auto last=mka::input::controllers().sample(0);
    CHECK(last&&last->state.buttons==0&&last->state.pressure==released.pressure);
    std::puts("recorded complete pressures, legacy data decode, hold and release PASS");
}
