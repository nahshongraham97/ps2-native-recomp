#include "mka/phase7_camera_operations.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace co = mka::phase7::camera_operations;
namespace {
unsigned assertions = 0;
std::string plant;
void check(bool condition, const char *message) {
    ++assertions;
    if (!condition) { std::fprintf(stderr, "FAIL %s\n", message); std::exit(1); }
}
template<class Function>
void refused(Function function, const char *operation) {
    try { function(); }
    catch (const co::BoundaryRefusal &failure) {
        check(std::string(failure.what()).find(operation) != std::string::npos, "named boundary identity"); return;
    }
    check(false, "missing required boundary accepted");
}
struct Snapshot {
    std::uint32_t projection, near_bits, far_bits, fog_bits, scale, shift, word;
    std::uint8_t type, subtype, flags, private_flags;
    mka::phase7::NativeRwFrame *parent;
    co::FrameAttachmentLink *next, *previous;
    std::uint32_t wx, wy, ix, iy, ox, oy;
    bool sync, begin, end, frame_empty, depth_empty;
    bool operator==(const Snapshot &) const = default;
};
Snapshot observe(const co::Camera &c) {
    return {c.projection,c.near_bits,c.far_bits,c.fog_bits,c.depth_scale_bits,c.depth_shift_bits,c.unclassified_creation_word,
            c.object_type,c.subtype,c.flags,c.private_flags,c.common.parent,c.attachment.next,c.attachment.previous,
            c.common.view_window.x_bits,c.common.view_window.y_bits,c.common.inverse_window.x_bits,c.common.inverse_window.y_bits,
            c.view_offset.x_bits,c.view_offset.y_bits,bool(c.synchronize),bool(c.begin_update),bool(c.end_update),
            c.frame_buffer.empty(),c.depth_buffer.empty()};
}
void projection_tests() {
    for (const auto projection : {std::uint64_t{1},std::uint64_t{2}}) {
        for (const bool attached : {false,true}) for (const bool dirty : {false,true}) {
            co::Camera c{};c.projection=17;c.near_bits=123;c.flags=0xa5;
            mka::phase7::NativeRwFrame root{},frame{},old_head{};
            root.private_flags=dirty?0x82:0x80;frame.private_flags=0x40;frame.root=&root;
            if(attached)c.common.parent=&frame;
            mka::phase7::NativeRwDirtyList list{&old_head,nullptr};
            const auto original=observe(c);unsigned calls=0;
            co::Services services;services.derive_depth=[&](co::Camera &actual){
                ++calls;check(&actual==&c&&c.projection==projection,"projection store before depth");
                if(attached)check(frame.private_flags==0x4c&&(root.private_flags&3)==3,"frame update before depth");
                actual.depth_scale_bits=0x12345678;actual.depth_shift_bits=0x87654321;
            };
            co::ErrorState errors{0,0x80000000u};
            auto *result=co::set_projection(c,projection,list,errors,services);
            if(plant=="--planted-projection")c.projection^=1;
            check(result==&c&&calls==1&&c.projection==projection,"projection result and depth call");
            check(errors.plugin_id==0&&errors.code==0x80000000u,"valid projection preserves error state");
            check(c.depth_scale_bits==0x12345678&&c.depth_shift_bits==0x87654321,"depth boundary effects retained");
            auto expected=original;expected.projection=static_cast<std::uint32_t>(projection);expected.scale=0x12345678;expected.shift=0x87654321;
            check(observe(c)==expected,"projection whole camera preservation");
            if(attached&&!dirty)check(list.head==&root&&root.dirty_next==&old_head&&root.dirty_prev==&list.head&&old_head.dirty_prev==&root.dirty_next,"projection dirty list insertion");
            else check(list.head==&old_head,"projection no spurious list insertion");
        }
    }
    for(const auto bad:{std::uint64_t{0},std::uint64_t{3},std::uint64_t{0xffffffffffffffffULL},std::uint64_t{0x100000001ULL},std::uint64_t{0x100000002ULL}}){
        co::Camera c{};c.projection=17;c.flags=0x9a;const auto prior=observe(c);
        co::Services missing;mka::phase7::NativeRwDirtyList list{};co::ErrorState errors{0,0x80000000u};
        check(co::set_projection(c,bad,list,errors,missing)==nullptr,"invalid projection null result");
        check(observe(c)==prior,"invalid projection whole camera unchanged");
        check(errors.plugin_id==0&&errors.code==0x80000003u,"invalid projection exact high-bit error");
    }
    co::Camera c{};mka::phase7::NativeRwDirtyList list{};co::ErrorState errors{0,0x80000000u};co::Services services;
    refused([&]{co::set_projection(c,2,list,errors,services);},"001A4F00");check(c.projection==2,"missing depth retains prior source store");
    mka::phase7::NativeRwFrame bad_frame{};c.common.parent=&bad_frame;
    refused([&]{co::set_projection(c,1,list,errors,services);},"001A7510");
}
void creation_tests() {
    co::Services none;refused([&]{co::create(none);},"0069F3D4");
    co::Services allocation_failure;unsigned allocations=0;allocation_failure.allocate=[&](auto hint){++allocations;check(hint==0x30005u,"null allocation hint");return static_cast<co::Camera*>(nullptr);};
    check(co::create(allocation_failure)==nullptr&&allocations==1,"allocation failure short-circuits all initialization");
    for(const bool plugin_success:{false,true}){
        std::vector<std::string> events;co::Camera c{};co::FrameAttachmentLink canary{};
        c.attachment={&canary,&canary};c.unclassified_creation_word=0xfeed1234;c.depth_scale_bits=91;c.depth_shift_bits=92;
        co::Services services;
        services.allocate=[&](auto hint){check(hint==0x30005u,"typed allocator source hint");events.push_back("allocate");return &c;};
        services.derive_depth=[&](co::Camera &actual){events.push_back("depth");
            check(&actual==&c&&c.object_type==4&&c.subtype==0&&c.flags==0&&c.private_flags==0,"source object header defaults");
            check(c.projection==1&&c.common.parent==nullptr,"source projection/parent defaults before depth");
            check(c.common.view_window.x_bits==0x3f800000&&c.common.view_window.y_bits==0x3f800000&&c.common.inverse_window.x_bits==0x3f800000&&c.common.inverse_window.y_bits==0x3f800000,"source exact four window constants");
            check(c.near_bits==0x3d4ccccd&&c.far_bits==0x41200000&&c.fog_bits==0x40a00000,"source near far fog constants");
            check(c.view_offset.x_bits==0&&c.view_offset.y_bits==0&&c.frame_buffer.empty()&&c.depth_buffer.empty(),"source zero defaults");
            check(c.unclassified_creation_word==0xfeed1234,"word not cleared until after depth");
            check(c.depth_scale_bits==91&&c.depth_shift_bits==92,"no invented default depth values");
            c.depth_scale_bits=0x11112222;c.depth_shift_bits=0x33334444;c.unclassified_creation_word=0xdecafbad;
        };
        services.initialize_plugins=[&](co::Camera &actual){events.push_back("plugins");
            check(&actual==&c&&c.unclassified_creation_word==0,"clear word precedes plugin constructor");
            check(c.depth_scale_bits==0x11112222&&c.depth_shift_bits==0x33334444,"plugin sees actual depth effects");
            actual.flags=0x77;return plugin_success;
        };
        unsigned callbacks=0;
        services.synchronize=[&](co::Camera &actual){++callbacks;check(&actual==&c,"sync callback this");return &actual;};
        services.begin_update=[&](co::Camera &actual){callbacks+=10;return &actual;};
        services.end_update=[&](co::Camera &actual){callbacks+=100;return &actual;};
        auto *result=co::create(services);
        if(plant=="--planted-create")c.near_bits^=1;
        check(result==&c&&c.flags==0x77&&c.near_bits==0x3d4ccccd,"factory preserves object identity even on plugin false");
        check(events==std::vector<std::string>{"allocate","depth","plugins"},"factory exact effect order");
        check(c.attachment.next==&canary&&c.attachment.previous==&canary,"factory preserves unwritten link fields");
        services={}; // Bound native callbacks retain their own callable lifetime.
        check(c.synchronize(c)==&c&&c.begin_update(c)==&c&&c.end_update(c)==&c&&callbacks==111,"installed native callback identities/lifetime");
    }
    co::Camera c{};co::Services services;services.allocate=[&](auto){return &c;};
    refused([&]{co::create(services);},"001A4F00");
    services.derive_depth=[](co::Camera &actual){actual.depth_scale_bits=4;};
    refused([&]{co::create(services);},"001A3480");
    check(c.projection==1&&c.depth_scale_bits==4&&c.unclassified_creation_word==0,"plugin absence preserves completed prefix");
    refused([&]{c.synchronize(c);},"001A6040");refused([&]{c.begin_update(c);},"001A6100");refused([&]{c.end_update(c);},"001A60B0");
}
void destruction_tests() {
    for(const bool attached:{false,true}){
        co::Camera c{};mka::phase7::NativeRwFrame frame{};co::FrameAttachmentLink before{},after{};
        before.next=&c.attachment;after.previous=&c.attachment;c.attachment={&after,&before};if(attached)c.common.parent=&frame;
        auto original=observe(c);std::vector<std::string> events;co::Services services;
        services.destroy_plugins=[&](co::Camera &actual){check(&actual==&c,"plugin teardown exact camera");check(before.next==&c.attachment&&after.previous==&c.attachment,"plugins run before detach");events.push_back("plugins");};
        services.release=[&](co::Camera &actual){check(&actual==&c,"free exact allocation");
            check(before.next==(attached?&after:&c.attachment)&&after.previous==(attached?&before:&c.attachment),"free follows actual detach");events.push_back("release");};
        auto result=co::destroy(c,services);if(plant=="--planted-destroy")result=0;
        check(result==1&&events==std::vector<std::string>{"plugins","release"},"destroy result and effect order");
        check(observe(c)==original,"detach preserves camera parent and own link fields");
    }
    co::Camera c{};co::Services services;refused([&]{co::destroy(c,services);},"001A3520");
    services.destroy_plugins=[](co::Camera&){};refused([&]{co::destroy(c,services);},"0069F3D8");
    mka::phase7::NativeRwFrame frame{};c.common.parent=&frame;refused([&]{co::destroy(c,services);},"001AA120");
    // Real host allocation/deallocation verifies that destroy never dereferences
    // the camera after the release boundary; no success-looking fake free.
    unsigned deleted=0;services.release=[&](co::Camera &actual){delete &actual;++deleted;};
    auto *owned=new co::Camera{};check(co::destroy(*owned,services)==1&&deleted==1,"real typed host lifetime release");
}
void device_tests() {
    co::ErrorState errors{0,0x80000000u};co::Device absent;
    refused([&]{co::device_request(absent,13,nullptr,nullptr,0,errors);},"delegate");
    co::DeviceInput input;unsigned delegate_calls=0,copy_calls=0;
    for(const auto delegated:{std::uint64_t{0},std::uint64_t{1},std::uint64_t{0x100000000ULL},std::uint64_t{0xffffffffffffffffULL}}){
        for(const auto request:{std::uint64_t{12},std::uint64_t{13},std::uint64_t{14},std::uint64_t{15},std::uint64_t{16},std::uint64_t{17},std::uint64_t{18},std::uint64_t{19},std::uint64_t{0x10000000eULL},std::uint64_t{0xffffffffULL}}){
            for(const auto extra:{std::uint64_t{0},std::uint64_t{1},std::uint64_t{0x100000000ULL}}){
                errors={0,0x80000000u};co::DeviceOutput output{0xa5a5a5a5,"untouched"};auto expected=output;unsigned copies_before=copy_calls;
                co::Device device;
                device.request=[&](auto actual_request,auto *actual_output,const auto *actual_input,auto actual_extra){++delegate_calls;
                    check(actual_request==request&&actual_output==&output&&actual_input==&input&&actual_extra==extra,"device delegate ABI and full-width values");return delegated;};
                device.copy_subsystem_label=[&](auto *actual_output,auto text){++copy_calls;check(actual_output==&output&&text=="Only rendering sub system","source label callback ABI");actual_output->label=text;};
                std::uint64_t expected_result=delegated;
                if(!delegated){
                    const auto low=static_cast<std::uint32_t>(request);
                    if(low==13){expected.scalar=1;expected_result=1;}
                    else if(low==14){expected_result=extra==0;if(expected_result)expected.label="Only rendering sub system";}
                    else if(low==15){expected.scalar=0;expected_result=1;}
                    else if(low==16)expected_result=extra==0;
                    else if(low==17||low==18)expected_result=1;
                }
                auto result=co::device_request(device,request,&output,&input,extra,errors);
                if(plant=="--planted-device")result^=1;
                check(result==expected_result,"device exact return including unnormalized delegate");
                check(output.scalar==expected.scalar&&output.label==expected.label,"device exact output effect or preservation");
                check(copy_calls-copies_before==(!delegated&&static_cast<std::uint32_t>(request)==14&&extra==0?1u:0u),"label conditional exact call count");
                check(errors.plugin_id==(expected_result?0u:1u)&&errors.code==(expected_result?0x80000000u:24u),"device error fallback exact record");
            }
        }
    }
    check(delegate_calls==120&&copy_calls==2,"device branch matrix invocation counts");
    co::Device zero;zero.request=[](auto,auto*,const auto*,auto){return std::uint64_t{0};};errors={0,0x80000000u};
    refused([&]{co::device_request(zero,13,nullptr,nullptr,0,errors);},"001A696C");
    refused([&]{co::device_request(zero,15,nullptr,nullptr,0,errors);},"001A69A8");
    refused([&]{co::device_request(zero,14,nullptr,nullptr,0,errors);},"0069F388");
    check(co::device_request(zero,17,nullptr,nullptr,99,errors)==1,"non-output request accepts unused null destination");
    // Existing errors are sticky; source does not overwrite either category.
    for(auto state:{co::ErrorState{9,0x80000000u},co::ErrorState{0,7},co::ErrorState{7,8}}){
        auto prior=state;co::publish_error(state,1,24);check(state.plugin_id==prior.plugin_id&&state.code==prior.code,"sticky error preserves entire existing state");
    }
}
}
#include "phase7_camera_operation_captures.hpp"
int main(int argc,char **argv){
    std::filesystem::path capture_root;
    if(argc>=3&&std::string(argv[1])=="--captures"){
        capture_root=argv[2];if(argc==4)plant=argv[3];else if(argc!=3)return 2;
    }else{if(argc>2)return 2;if(argc==2)plant=argv[1];}
    const bool capture_plant=plant=="--planted-capture-projection"||plant=="--planted-capture-create"||plant=="--planted-capture-destroy"||plant=="--planted-capture-device";
    if(!plant.empty()&&plant!="--planted-projection"&&plant!="--planted-create"&&plant!="--planted-destroy"&&plant!="--planted-device"&&!capture_plant)return 2;
    if(capture_plant&&capture_root.empty())return 2;
    projection_tests();creation_tests();destruction_tests();device_tests();
    std::printf("PASS_CAMERA_OPERATIONS_CONSTRUCTED assertions=%u functions=4 device_cases=120 production_adoption=0 actual_capture_equivalence=OPEN\n",assertions);
    if(!capture_root.empty()){
        try{camera_capture_tests(capture_root,plant);}
        catch(const std::exception &failure){std::fprintf(stderr,"REFUSE_CAMERA_CAPTURE %s\n",failure.what());return 2;}
    }
}
