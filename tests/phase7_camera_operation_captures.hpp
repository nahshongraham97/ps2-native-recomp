#pragma once
#include "phase7_track_b_capture.hpp"
#include <memory>

namespace {
// Test-only instruction-derived numeric boundary. Native production numerics
// remain unchanged; status and accumulator comparison are explicit scope.
void source_depth(co::Camera &camera, std::uint32_t device_near, std::uint32_t device_far,
                  std::uint32_t &status, std::uint32_t &accumulator) {
    namespace fp = ps2::fp::ee;
    const auto far_value = camera.projection == 2 ? camera.far_bits : fp::div(status,0x3f800000,camera.far_bits);
    const auto near_value = camera.projection == 2 ? camera.near_bits : fp::div(status,0x3f800000,camera.near_bits);
    const auto margin=fp::mul(status,0x38d1b717,fp::sub(status,device_far,device_near));
    const auto high=fp::sub(status,device_far,margin);
    const auto low=fp::add(status,device_near,margin);
    const auto span=fp::sub(status,high,low);
    const auto sum=fp::add(status,high,low);
    camera.depth_scale_bits=fp::div(status,span,fp::sub(status,far_value,near_value));
    const auto clip_sum=fp::add(status,far_value,near_value);
    accumulator=fp::add(status,0,sum);
    camera.depth_shift_bits=fp::mul(status,0x3f000000,fp::msub(status,accumulator,camera.depth_scale_bits,clip_sum));
}
struct SourceBoundPlugins {
    co::CameraCallback saved_begin,saved_end,saved_sync;
    std::uint32_t capacity{},count{},world_word{},clip_words[3]{};
    std::unique_ptr<unsigned char> native_sector_storage;
    bool world_wrappers{};
};
unsigned camera_capture_tests(const std::filesystem::path &directory, const std::string &mutation) {
    unsigned counts[4]{};
    for (const auto &path:track_b::files(directory,"cap_001a")) {
        auto capture=track_b::load(path);
        if(capture.pc!=0x1a6410&&capture.pc!=0x1a6540&&capture.pc!=0x1a65a0&&capture.pc!=0x1a68f0)continue;
        check(capture.after.pc==capture.before.r[31].w[0]&&std::memcmp(&capture.before.r[29],&capture.after.r[29],sizeof(ps2::Reg128))==0,"actual complete ordinary return and full SP");
        for(unsigned reg=16;reg<=23;++reg)check(std::memcmp(&capture.before.r[reg],&capture.after.r[reg],sizeof(ps2::Reg128))==0,"actual full callee-saved register preservation");
        check(capture.scratch==capture.exit_scratch,"actual scratchpad preservation");
        const auto entry=[&](std::uint32_t address){return track_b::read(capture.ram,address);};
        const auto expected=[&](std::uint32_t address){return track_b::read(capture.exit_ram,address);};
        const auto byte=[&](const auto &ram,std::uint32_t address){if(address>=ram.size())throw std::runtime_error("capture byte bounds");return std::to_integer<std::uint8_t>(ram[address]);};
        const auto input_camera=capture.before.r[4].w[0];
        const auto gp=capture.before.r[28].w[0];
        auto status=capture.before.fcr31;auto accumulator=capture.before.facc;
        co::ErrorState errors{entry(0x69f290+entry(gp-0x5c80)),entry(0x69f294+entry(gp-0x5c80))};
        mka::phase7::NativeRwDirtyList list{};
        mka::phase7::NativeRwFrame frame{},root{},head{};
        co::Services services;
        services.derive_depth=[&](co::Camera &actual){source_depth(actual,entry(0x69f2a8),entry(0x69f2ac),status,accumulator);};
        const auto lift=[&](co::Camera &camera,std::uint32_t base){
            camera.object_type=byte(capture.ram,base);camera.subtype=byte(capture.ram,base+1);camera.flags=byte(capture.ram,base+2);camera.private_flags=byte(capture.ram,base+3);
            camera.projection=entry(base+20);camera.common.view_window={entry(base+104),entry(base+108)};
            camera.common.inverse_window={entry(base+112),entry(base+116)};camera.view_offset={entry(base+120),entry(base+124)};
            camera.near_bits=entry(base+128);camera.far_bits=entry(base+132);camera.fog_bits=entry(base+136);
            camera.depth_scale_bits=entry(base+140);camera.depth_shift_bits=entry(base+144);camera.unclassified_creation_word=entry(base+44);
            // Frame/raster payloads are independently owned. Only exact fields
            // observed by these operations are lifted; no exit-state loading.
        };
        const auto compare=[&](const Snapshot &camera,std::uint32_t base){
            check(camera.projection==expected(base+20)&&camera.word==expected(base+44),"actual projection and initialized word");
            check(camera.type==byte(capture.exit_ram,base)&&camera.subtype==byte(capture.exit_ram,base+1)&&camera.flags==byte(capture.exit_ram,base+2)&&camera.private_flags==byte(capture.exit_ram,base+3),"actual whole logical object header");
            check(camera.wx==expected(base+104)&&camera.wy==expected(base+108)&&camera.ix==expected(base+112)&&camera.iy==expected(base+116)&&camera.ox==expected(base+120)&&camera.oy==expected(base+124),"actual view-window inverse and offset fields");
            check(camera.near_bits==expected(base+128)&&camera.far_bits==expected(base+132)&&camera.fog_bits==expected(base+136)&&camera.scale==expected(base+140)&&camera.shift==expected(base+144),"actual clip and independently derived depth fields");
        };
        if(capture.pc==0x1a6410){
            co::Camera camera{};lift(camera,input_camera);const auto parent=entry(input_camera+4);
            std::uint32_t root_address=0,head_address=0;std::uint8_t root_flags=0;
            if(parent){
                root_address=entry(parent+160);root_flags=byte(capture.ram,root_address+3);
                root.private_flags=root_flags;frame.private_flags=byte(capture.ram,parent+3);frame.root=root_address==parent?&frame:&root;
                if(root_address==parent)frame.private_flags=root_flags;
                camera.common.parent=&frame;head_address=entry(0x69f34c);list.head=&head;
            }
            auto *result=co::set_projection(camera,capture.before.r[5].d[0],list,errors,services);
            if(mutation=="--planted-capture-projection")camera.projection^=1;
            check(result==&camera&&capture.after.r[2].w[0]==input_camera,"actual projection return identity");
            compare(observe(camera),input_camera);
            check(status==capture.after.fcr31&&accumulator==capture.after.facc,"actual depth status and accumulator");
            if(parent){
                check(frame.private_flags==byte(capture.exit_ram,parent+3)&&frame.root->private_flags==byte(capture.exit_ram,root_address+3),"actual frame root/self flags");
                if(!(root_flags&3))check(list.head==frame.root&&frame.root->dirty_next==&head&&expected(root_address+8)==head_address&&expected(root_address+12)==0x69f34c&&expected(head_address+4)==root_address+8&&expected(0x69f34c)==root_address+8,"actual dirty graph insertion");
                else check(list.head==&head&&expected(root_address+8)==entry(root_address+8)&&expected(root_address+12)==entry(root_address+12)&&expected(head_address+4)==entry(head_address+4)&&expected(0x69f34c)==head_address,"actual dirty graph preservation");
            }
            ++counts[0];
        }else if(capture.pc==0x1a65a0||capture.pc==0x1a6540){
            // Admit only the independently source-joined three-plugin cohort.
            const std::uint32_t constructors[]={0x1a3120,0x1cdfd0,0x1c9300};
            const std::uint32_t destructors[]={0x1a3130,0x1ce080,0x1c9320};
            auto node=entry(0x5a3640);unsigned plugin_count=0;
            while(node&&plugin_count<3){check(entry(node+32)==constructors[plugin_count]&&entry(node+36)==destructors[plugin_count],"actual source-bound plugin callback identities");node=entry(node+48);++plugin_count;}
            check(!node&&plugin_count==3,"actual exact admitted plugin cohort");
            check(entry(0x69f3d4)==0x1a1180&&entry(0x69f3d8)==0x1a1340,"actual source allocator callback identities");
            const auto world_offset=entry(gp-0x59a8),clip_offset=entry(gp-0x5a00);
            SourceBoundPlugins plugins;std::unique_ptr<co::Camera> owner;
            services.synchronize=[](co::Camera&)->co::Camera*{throw co::BoundaryRefusal("001A6040 synchronization body not executed by create");};
            services.begin_update=[](co::Camera&)->co::Camera*{throw co::BoundaryRefusal("001A6100 begin body not executed by create");};
            services.end_update=[](co::Camera&)->co::Camera*{throw co::BoundaryRefusal("001A60B0 end body not executed by create");};
            services.allocate=[&](auto hint){check(hint==0x30005,"actual native allocation hint");owner=std::make_unique<co::Camera>();return owner.get();};
            services.initialize_plugins=[&](co::Camera &camera){
                plugins.capacity=0;plugins.count=0;plugins.world_word=0;
                plugins.saved_begin=camera.begin_update;plugins.saved_end=camera.end_update;plugins.saved_sync=camera.synchronize;
                camera.begin_update=[](co::Camera&)->co::Camera*{throw co::BoundaryRefusal("001CDEE0 world begin body not invoked");};
                camera.end_update=[](co::Camera&)->co::Camera*{throw co::BoundaryRefusal("001CDF20 world end body not invoked");};
                camera.synchronize=[](co::Camera&)->co::Camera*{throw co::BoundaryRefusal("001CDF40 world sync body not invoked");};
                plugins.world_wrappers=true;for(auto &word:plugins.clip_words)word=0;
                return true; // All three proved selected constructors return this.
            };
            if(capture.pc==0x1a65a0){
                auto *camera=co::create(services);if(mutation=="--planted-capture-create")camera->near_bits^=1;
                const auto base=capture.after.r[2].w[0];check(camera==owner.get()&&base!=0,"actual normalized fresh allocation identity");
                compare(observe(*camera),base);
                check(status==capture.after.fcr31&&accumulator==capture.after.facc,"actual creation numeric status");
                check(expected(base+4)==0&&expected(base+96)==0&&expected(base+100)==0,"actual creation null parent and rasters");
                check(plugins.world_wrappers&&expected(base+16)==0x1cdf40&&expected(base+24)==0x1cdee0&&expected(base+28)==0x1cdf20,"actual world callback installation");
                check(expected(base+world_offset)==0&&expected(base+world_offset+4)==plugins.capacity&&expected(base+world_offset+8)==plugins.count&&expected(base+world_offset+12)==plugins.world_word,"actual world plugin initial owner state");
                check(expected(base+world_offset+16)==0x1a6100&&expected(base+world_offset+20)==0x1a60b0&&expected(base+world_offset+24)==0x1a6040,"actual saved base callbacks");
                for(unsigned n=0;n<3;++n)check(expected(base+clip_offset+n*4)==plugins.clip_words[n],"actual clipping plugin zeros");
                ++counts[2];
            }else{
                owner=std::make_unique<co::Camera>();lift(*owner,input_camera);
                const auto parent=entry(input_camera+4);if(parent)owner->common.parent=&frame;
                co::FrameAttachmentLink before{},after{};const auto next=entry(input_camera+8),previous=entry(input_camera+12);
                // Same-neighbor sentinel aliases are represented by one native node.
                auto *previous_node=&before;auto *next_node=next==previous?&before:&after;
                owner->attachment={next_node,previous_node};previous_node->next=&owner->attachment;next_node->previous=&owner->attachment;
                plugins.capacity=entry(input_camera+world_offset+4);plugins.count=entry(input_camera+world_offset+8);
                plugins.world_word=entry(input_camera+world_offset+12);
                if(entry(input_camera+world_offset))plugins.native_sector_storage=std::make_unique<unsigned char>();
                const auto saved_begin=entry(input_camera+world_offset+16),saved_end=entry(input_camera+world_offset+20),saved_sync=entry(input_camera+world_offset+24);
                check(saved_begin==0x1a6100&&saved_end==0x1a60b0&&saved_sync==0x1a6040,"actual source saved callback restoration cohort");
                services.destroy_plugins=[&](co::Camera &camera){
                    if(plugins.native_sector_storage){check(entry(0x69f3c8)==0x2209a0,"actual plugin storage release callback");plugins.native_sector_storage.reset();}
                    plugins.capacity=0;plugins.count=0;
                    camera.begin_update=services.begin_update;camera.end_update=services.end_update;camera.synchronize=services.synchronize;
                };
                Snapshot released{};bool release_seen=false;
                services.release=[&](co::Camera &camera){released=observe(camera);release_seen=true;if(mutation!="--planted-capture-destroy")owner.reset();};
                const auto result=co::destroy(*owner,services);
                check(result==capture.after.r[2].d[0]&&release_seen&&!owner&&!plugins.native_sector_storage,"actual normalized camera/plugin lifetime release");
                compare(released,input_camera);
                check(expected(input_camera+world_offset)==0&&expected(input_camera+world_offset+4)==plugins.capacity&&expected(input_camera+world_offset+8)==plugins.count&&expected(input_camera+world_offset+12)==plugins.world_word,"actual destruction plugin storage state");
                check(expected(input_camera+4)==entry(input_camera+4)&&expected(input_camera+8)==entry(input_camera+8)&&expected(input_camera+12)==entry(input_camera+12),"actual destruction preserves parent and own links");
                for(unsigned n=0;n<3;++n)check(expected(input_camera+clip_offset+n*4)==entry(input_camera+clip_offset+n*4),"actual destruction preserves clipping plugin words");
                check(expected(input_camera+16)==saved_sync&&expected(input_camera+24)==saved_begin&&expected(input_camera+28)==saved_end,"actual destruction callback restoration");
                if(parent)check(previous_node->next==next_node&&next_node->previous==previous_node&&expected(previous)==next&&expected(next+4)==previous,"actual frame attachment unlink");
                ++counts[1];
            }
        }else{
            // Independently joined 1B9CF0 request10 branch 1B9EA4..1B9EB0.
            // This is the actual observed delegate path, not a guessed generic
            // callback or exit-state return copied into a success stub.
            check(entry(input_camera+4)==0x1b9cf0&&capture.before.r[5].d[0]==10,"actual admitted device callback and branch");
            const auto destination=capture.before.r[6].w[0];co::DeviceOutput output{entry(destination),""};
            co::Device device;device.request=[&](auto request,auto *actual_output,const auto*,auto){
                if(request!=10||!actual_output)throw co::BoundaryRefusal("1B9CF0 unproved device branch");
                actual_output->scalar=entry(gp-0x5a78);return std::uint64_t{1};
            };
            auto result=co::device_request(device,10,&output,nullptr,capture.before.r[8].d[0],errors);
            if(mutation=="--planted-capture-device")output.scalar^=1;
            check(result==capture.after.r[2].d[0]&&output.scalar==expected(destination),"actual delegated device result/output");
            ++counts[3];
        }
    }
    check(counts[0]&&counts[1]&&counts[2]&&counts[3],"all four actual camera operation captures required");
    std::printf("PASS_CAMERA_ACTUAL_TYPED_COMPOSITION projection=%u destroy=%u create=%u device=%u full_function_equivalence=OPEN\n",counts[0],counts[1],counts[2],counts[3]);
    return counts[0]+counts[1]+counts[2]+counts[3];
}
}
