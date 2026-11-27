#include "mka/retail_scene_archive.hpp"
#include <array>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
#include <new>

namespace mka::retail {
bool RetailSceneResources::empty() const noexcept {
    return rasters.empty() && textures.empty() && materials.empty() && frames.empty() &&
        scene_objects.empty() && !owns_bsp;
}
bool RetailSceneResources::matches(NativeRetailRendererRasters* r, NativeRetailRendererTextures* t,
    NativeRetailRendererMaterials* m, NativeRetailRendererFrames* f, NativeRetailWorld* w) const noexcept {
    return thread == std::this_thread::get_id() &&
        (!r || raster_owner.get() == r) && (!t || texture_owner.get() == t) &&
        (!m || material_owner.get() == m) && (!f || frame_owner.get() == f) && (!w || world_owner.get() == w);
}
RetailSceneResources::~RetailSceneResources() {
    RetailRendererWorldFailure f;
    if (!cleanup(f)) std::fprintf(stderr, "native scene resource cleanup refused: %s\n", f.operation);
}
bool RetailSceneResources::cleanup(RetailRendererWorldFailure& failure) noexcept {
    failure = {};
    if (empty()) return true;
    if (thread != std::this_thread::get_id()) {
        failure.kind = RetailRendererWorldError::wrong_thread;
        failure.operation = "scene resource cleanup requires its owner thread"; return false;
    }
    const auto reject = [&](const char* operation) {
        failure.kind = RetailRendererWorldError::owner; failure.operation = operation; return false;
    };
    try {
        while (!scene_objects.empty()) {
            if (!world_owner) return reject("scene resource cleanup missing world");
            if (!world_owner->remove_scene_object(scene_objects.back(), failure)) return false;
            scene_objects.pop_back();
        }
        if (owns_bsp) {
            if (!world_owner) return reject("scene BSP cleanup missing world");
            world_owner->clear_bsp_sectors(); owns_bsp = false;
        }
        while (!frames.empty()) {
            RetailRendererFrameFailure f;
            if (!frame_owner) return reject("scene frame cleanup missing owner");
            if (!frame_owner->destroy_leaf(frames.back(), f)) return reject(f.operation);
            frames.pop_back();
        }
        while (!materials.empty()) {
            RetailRendererMaterialFailure f;
            if (!material_owner) return reject("scene material cleanup missing owner");
            if (!material_owner->destroy_material(materials.back(), f)) return reject(f.operation);
            materials.pop_back();
        }
        while (!textures.empty()) {
            RetailRendererTextureFailure f;
            if (!texture_owner) return reject("scene texture cleanup missing owner");
            if (!texture_owner->destroy_texture(textures.back(), f)) return reject(f.operation);
            textures.pop_back();
        }
        while (!rasters.empty()) {
            RetailRendererRasterFailure f;
            if (!raster_owner) return reject("scene raster cleanup missing owner");
            if (!raster_owner->destroy_raster(rasters.back(), f)) return reject(f.operation);
            rasters.pop_back();
        }
        return true;
    } catch (...) {
        failure.kind = RetailRendererWorldError::native_exception;
        failure.operation = "scene resource cleanup native exception"; return false;
    }
}

namespace {
std::uint32_t u32(const std::vector<std::uint8_t>& b, std::size_t i) {
    return std::uint32_t(b[i]) | (std::uint32_t(b[i+1]) << 8) |
        (std::uint32_t(b[i+2]) << 16) | (std::uint32_t(b[i+3]) << 24);
}
bool root(const std::vector<std::uint8_t>& b, unsigned role, RetailSceneArchiveFailure& f) {
    if (b.size() >= 4 && u32(b, 0) == 0x53454320) {
        f.kind = RetailSceneArchiveError::unsupported_sec;
        f.operation = "selected archive member is SEC; nested SEC selection is not proved"; return false;
    }
    if (b.size() < 12 || u32(b, 4) != b.size() - 12) {
        f.kind = RetailSceneArchiveError::invalid_rw_root;
        f.operation = "selected archive member requires one complete bounded RW root"; return false;
    }
    const auto id = u32(b, 0);
    if ((role == 0 && id == 0x16) || (role == 1 && id == 0xb) ||
        (role == 2 && (id == 0xf || id == 0x10))) return true;
    f.kind = RetailSceneArchiveError::invalid_rw_root;
    f.operation = "selected archive member RW root does not match its explicit role"; return false;
}
}

std::optional<RetailArchiveSceneLoadResult> load_retail_scene_streams_from_archive(
    const content::NativeContentVolume& volume, const RetailArchiveSceneStreams& config,
    std::uint64_t base_sector_id, std::uint64_t base_object_id,
    NativeRetailRendererRasters& rasters, NativeRetailRendererTextures& textures,
    NativeRetailRendererMaterials& materials, NativeRetailRendererFrames& frames, NativeRetailWorld& world,
    const NativeRetailRendererFrameHandle& parent_frame, RetailSceneArchiveFailure& failure,
    RetailSceneResources& resources) noexcept {
    failure = {};
    if (!resources.empty() || !resources.matches(&rasters, &textures, &materials, &frames, &world)) {
        failure.kind = RetailSceneArchiveError::resource_owner;
        failure.operation = "archive scene requires a fresh collector bound to these actual owners"; return {};
    }
    bool committed = false;
    struct Rollback {
        RetailSceneResources& resources; bool& committed;
        ~Rollback() { if (!committed) { RetailRendererWorldFailure ignored; (void)resources.cleanup(ignored); } }
    } rollback{resources, committed};
    try {
        if (config.archive_resource_key.empty() || (!config.txd && !config.world && !config.geometry) ||
            (config.world && !base_sector_id) || (config.geometry && !base_object_id)) {
            failure.kind = RetailSceneArchiveError::invalid_selection;
            failure.operation = "archive scene requires a resource, explicit members and nonzero selected base IDs"; return {};
        }
        if (!world.admit_render_modules(&materials, &textures, &rasters, failure.renderer)) {
            failure.kind = RetailSceneArchiveError::resource_owner; failure.operation = failure.renderer.operation; return {};
        }
        auto file = volume.open_resource(config.archive_resource_key, failure.content);
        if (!file) { failure.kind = RetailSceneArchiveError::content; failure.operation = "archive scene source file open"; return {}; }
        auto archive = NativeRetailArchive::open(std::move(*file), failure.archive);
        if (!archive) { failure.kind = RetailSceneArchiveError::archive; failure.operation = failure.archive.operation; return {}; }
        std::array<std::vector<std::uint8_t>, 3> bytes;
        const std::array<const std::optional<RetailArchiveMemberSelector>*, 3> selected{&config.txd, &config.world, &config.geometry};
        RetailArchiveSceneLoadResult result;
        for (unsigned role = 0; role < selected.size(); ++role) {
            if (!*selected[role]) continue;
            const auto& selector = **selected[role];
            if (selector.name.has_value() == selector.index.has_value()) {
                failure.kind = RetailSceneArchiveError::invalid_selection;
                failure.operation = "archive member selector requires exactly one full name or ordinal"; return {};
            }
            auto member = selector.name ? archive->find(*selector.name, failure.archive) : archive->at(*selector.index, failure.archive);
            if (!member) { failure.kind = RetailSceneArchiveError::archive; failure.operation = failure.archive.operation; return {}; }
            auto extracted = archive->extract(member->index, failure.archive);
            if (!extracted) { failure.kind = RetailSceneArchiveError::archive; failure.operation = failure.archive.operation; return {}; }
            bytes[role] = std::move(*extracted);
            if (!root(bytes[role], role, failure)) return {};
            if (bytes[role].size() > std::numeric_limits<std::size_t>::max() - result.bytes_read) {
                failure.kind = RetailSceneArchiveError::allocation; failure.operation = "archive scene aggregate byte count overflow"; return {};
            }
            result.bytes_read += bytes[role].size();
        }
        const auto span = [&](unsigned i) { return std::as_bytes(std::span(bytes[i])); };
        if (config.txd) {
            RetailRendererTextureFailure f;
            if (!ingest_rw_tex_dictionary_stream(span(0), rasters, textures, f, &resources)) {
                failure.kind = RetailSceneArchiveError::ingestion; failure.operation = f.operation; return {};
            }
            result.txd_ingested = true;
        }
        if (config.world) {
            if (!ingest_rw_world_bsp_stream(span(1), base_sector_id, materials, textures, world, failure.renderer, &resources)) {
                failure.kind = RetailSceneArchiveError::ingestion; failure.operation = failure.renderer.operation; return {};
            }
            result.world_ingested = true;
        }
        if (config.geometry) {
            if (!ingest_rw_geometry_stream(span(2), base_object_id, frames, materials, textures, world, parent_frame, failure.renderer, &resources)) {
                failure.kind = RetailSceneArchiveError::ingestion; failure.operation = failure.renderer.operation; return {};
            }
            result.geometry_ingested = true;
        }
        committed = true;
        return result;
    } catch (const std::bad_alloc&) {
        failure.kind = RetailSceneArchiveError::allocation; failure.operation = "archive scene allocation";
    } catch (...) {
        failure.kind = RetailSceneArchiveError::native_exception; failure.operation = "archive scene native exception";
    }
    return {};
}
std::optional<RetailDecodedSsfScene> decode_retail_ssf_scene_from_archive(
    const content::NativeContentVolume& volume, const RetailSsfSceneSelection& config,
    RetailSceneArchiveFailure& failure) noexcept {
    failure={};
    try {
        const auto selector_ok=[](const RetailArchiveMemberSelector& s){return s.name.has_value()!=s.index.has_value();};
        if(config.archive_resource_key.empty()||!selector_ok(config.member)||!selector_ok(config.clump_resource)) {
            failure.kind=RetailSceneArchiveError::invalid_selection;failure.operation="SSF scene requires explicit archive/member/clump selectors";return {};
        }
        auto source=volume.open_resource(config.archive_resource_key,failure.content);
        if(!source){failure.kind=RetailSceneArchiveError::content;failure.operation="SSF scene actual source open";return {};}
        auto archive=NativeRetailArchive::open(std::move(*source),failure.archive);
        if(!archive){failure.kind=RetailSceneArchiveError::archive;failure.operation=failure.archive.operation;return {};}
        auto bundle=config.member.name?NativeRetailSsfBundle::open(*archive,*config.member.name,failure.ssf):
            NativeRetailSsfBundle::open(*archive,*config.member.index,failure.ssf);
        if(!bundle){failure.kind=RetailSceneArchiveError::ssf_bundle;failure.operation=failure.ssf.operation;return {};}
        auto section=bundle->open_art_section(config.section_index,failure.ssf);
        if(!section){failure.kind=RetailSceneArchiveError::ssf_bundle;failure.operation=failure.ssf.operation;return {};}
        auto clump=config.clump_resource.name?section->clump(*config.clump_resource.name,failure.ssf):
            section->clump(*config.clump_resource.index,failure.ssf);
        if(!clump){failure.kind=RetailSceneArchiveError::ssf_bundle;failure.operation=failure.ssf.operation;return {};}
        auto geometry=decode_retail_ssf_geometry(clump->rw_bytes(),
            {config.atomic_index,config.morph_index,std::uint64_t(clump->resource().record().payload_offset)+8},failure.geometry);
        if(!geometry){failure.kind=RetailSceneArchiveError::ssf_geometry;failure.operation=failure.geometry.operation;return {};}
        RetailDecodedSsfScene result;result.geometry=std::move(*geometry);result.serialized_resource_bytes=clump->resource().bytes().size();
        result.textures.reserve(config.texture_resource_indices.size());
        for(std::size_t selected=0;selected<config.texture_resource_indices.size();++selected) {
            const auto ordinal=config.texture_resource_indices[selected];const auto allowed=clump->texture_records();
            if(std::find(allowed.begin(),allowed.end(),ordinal)==allowed.end()||
                std::find(config.texture_resource_indices.begin(),config.texture_resource_indices.begin()+selected,ordinal)!=config.texture_resource_indices.begin()+selected) {
                failure.kind=RetailSceneArchiveError::texture_binding;failure.operation="SSF texture ordinal outside source clump range or duplicated";return {};
            }
            auto resource=section->resource(ordinal,failure.ssf);
            if(!resource){failure.kind=RetailSceneArchiveError::ssf_bundle;failure.operation=failure.ssf.operation;return {};}
            const auto type=resource->record().raw_type&0x3fffffffu;
            if(type!=2&&type!=3){failure.kind=RetailSceneArchiveError::texture_binding;failure.operation="SSF selected texture record has wrong source type";return {};}
            auto texture=decode_retail_ssf_texture(resource->bytes(),resource->record().payload_offset,failure.texture);
            if(!texture){failure.kind=RetailSceneArchiveError::ssf_texture;failure.operation=failure.texture.operation;return {};}
            if(std::any_of(result.textures.begin(),result.textures.end(),[&](const auto& t){return t.name==texture->name;})) {
                failure.kind=RetailSceneArchiveError::texture_binding;failure.operation="SSF decoded texture body name is ambiguous";return {};
            }
            if(resource->bytes().size()>std::numeric_limits<std::size_t>::max()-result.serialized_resource_bytes) {
                failure.kind=RetailSceneArchiveError::allocation;failure.operation="SSF decoded byte count overflow";return {};
            }
            result.serialized_resource_bytes+=resource->bytes().size();result.textures.push_back(std::move(*texture));
        }
        for(const auto& material:result.geometry.materials)if(material.textured) {
            if(std::none_of(result.textures.begin(),result.textures.end(),[&](const auto& t){return t.name==material.texture_name;})) {
                failure.kind=RetailSceneArchiveError::texture_binding;failure.operation="SSF material requires exact decoded texture body name";return {};
            }
        }
        for(const auto& texture:result.textures) {
            if(std::none_of(result.geometry.materials.begin(),result.geometry.materials.end(),[&](const auto& m){return m.textured&&m.texture_name==texture.name;})) {
                failure.kind=RetailSceneArchiveError::texture_binding;failure.operation="SSF explicit texture is not referenced by selected geometry";return {};
            }
        }
        return result;
    } catch(const std::bad_alloc&){failure.kind=RetailSceneArchiveError::allocation;failure.operation="SSF scene CPU ownership allocation";}
      catch(...){failure.kind=RetailSceneArchiveError::native_exception;failure.operation="SSF scene CPU native exception";}
    return {};
}

std::optional<RetailDecodedSsfScene> load_retail_ssf_scene_from_archive(
    const content::NativeContentVolume& volume, const RetailSsfSceneSelection& config, std::uint64_t base_object_id,
    NativeRetailRendererRasters& rasters, NativeRetailRendererTextures& textures, NativeRetailRendererMaterials& materials,
    NativeRetailRendererFrames& frames, NativeRetailWorld& world, const NativeRetailRendererFrameHandle& parent_frame,
    RetailSceneArchiveFailure& failure, RetailSceneResources& resources) noexcept {
    failure={};
    if(config.render_policy!=RetailSsfSceneRenderPolicy::diagnostic_unlit_raw_rgba8) {
        failure.kind=RetailSceneArchiveError::render_policy;failure.operation="SSF retail rendering policy remains unbound; explicit diagnostic policy required";return {};
    }
    if(!resources.empty()||!resources.matches(&rasters,&textures,&materials,&frames,&world)||!base_object_id) {
        failure.kind=RetailSceneArchiveError::resource_owner;failure.operation="SSF scene requires fresh matching native owners and nonzero object ID";return {};
    }
    bool committed=false;
    struct Rollback {RetailSceneResources& owner;bool& committed;RetailSceneArchiveFailure& failure;
        ~Rollback(){if(!committed)(void)owner.cleanup(failure.cleanup);}} rollback{resources,committed,failure};
    try {
        if(!world.admit_render_modules(&materials,&textures,&rasters,failure.renderer,&frames)) {
            failure.kind=RetailSceneArchiveError::resource_owner;failure.operation=failure.renderer.operation;return {};
        }
        auto decoded=decode_retail_ssf_scene_from_archive(volume,config,failure);if(!decoded)return {};
        const auto& data=decoded->geometry;
        if(data.atomic_flags!=5||data.atomic_plugin_words[2]||data.atomic_plugin_words[3]||
            data.materials.size()>std::numeric_limits<std::uint64_t>::max()-base_object_id) {
            failure.kind=RetailSceneArchiveError::render_policy;failure.operation="SSF diagnostic atomic policy or object ID extent unsupported";return {};
        }
        constexpr std::array<std::uint32_t,7> plain_material{5,0,0,0xffffffffu,0,0,0};
        for(const auto& m:data.materials)if(m.plugin_words!=plain_material||!m.mask_name.empty()||m.flags) {
            failure.kind=RetailSceneArchiveError::render_policy;failure.operation="SSF diagnostic preview supports only plain version-5 material metadata and no mask";return {};
        }
        for(const auto& t:decoded->textures)if(t.sampler_metadata!=0x1102||!t.width||!t.height||
            t.width>std::uint32_t(std::numeric_limits<std::int32_t>::max())||t.height>std::uint32_t(std::numeric_limits<std::int32_t>::max())||
            std::uint64_t(t.width)*t.height*4!=t.rgba_raw_alpha.size()) {
            failure.kind=RetailSceneArchiveError::render_policy;failure.operation="SSF diagnostic texture extent or source sampler unsupported";return {};
        }
        std::vector<std::size_t> ancestry;
        for(auto i=std::int64_t(data.frame_index);i>=0;i=data.frames[std::size_t(i)].parent)ancestry.push_back(std::size_t(i));
        std::reverse(ancestry.begin(),ancestry.end());
        std::vector<std::vector<RetailWorldTriangleVertex>> groups(data.materials.size());
        for(const auto& triangle:data.triangles)for(const auto index:triangle.vertices) {
            const auto& source=data.vertices[index];RetailWorldTriangleVertex vertex;
            vertex.position=source.position;vertex.normal=source.normal;vertex.u=source.uv[0];vertex.v=source.uv[1];
            for(unsigned c=0;c<4;++c)vertex.color[c]=data.has_colors?source.stored_color[c]/255.0f:1.0f;
            groups[triangle.material].push_back(vertex);
        }
        resources.rasters.reserve(decoded->textures.size());resources.textures.reserve(decoded->textures.size());
        resources.materials.reserve(data.materials.size());resources.frames.reserve(ancestry.size());resources.scene_objects.reserve(groups.size());
        std::vector<NativeRetailTextureHandle> texture_handles;texture_handles.reserve(decoded->textures.size());
        for(const auto& t:decoded->textures) {
            RetailRendererTextureDescription desc{t.name,"",RetailRendererTextureFilter::linear,RetailRendererTextureAddress::wrap,RetailRendererTextureAddress::wrap};
            RetailRendererRasterFailure rf;auto raster=rasters.create_raster({std::int32_t(t.width),std::int32_t(t.height),32,4,4,0x500},rf);
            if(!raster){failure.kind=RetailSceneArchiveError::ingestion;failure.operation=rf.operation;return {};}
            resources.rasters.push_back(*raster);
            auto pixels=rasters.lock_pixels(*raster,rf);
            if(!pixels){failure.kind=RetailSceneArchiveError::ingestion;failure.operation=rf.operation;return {};}
            if(pixels->size()!=t.rgba_raw_alpha.size()) {
                const bool unlocked=rasters.unlock_pixels(*raster,rf);
                failure.kind=RetailSceneArchiveError::ingestion;
                failure.operation=unlocked?"SSF actual raster storage extent":rf.operation;return {};
            }
            std::memcpy(pixels->data(),t.rgba_raw_alpha.data(),pixels->size());
            if(!rasters.unlock_pixels(*raster,rf)){failure.kind=RetailSceneArchiveError::ingestion;failure.operation=rf.operation;return {};}
            RetailRendererTextureFailure tf;auto texture=textures.create_texture(std::move(desc),*raster,tf);
            if(!texture){failure.kind=RetailSceneArchiveError::ingestion;failure.operation=tf.operation;return {};}
            resources.textures.push_back(*texture);texture_handles.push_back(*texture);
        }
        std::vector<NativeRetailMaterialHandle> material_handles;material_handles.reserve(data.materials.size());
        for(const auto& m:data.materials) {
            RetailRendererMaterialFailure mf;auto material=materials.create_material(mf);
            if(!material){failure.kind=RetailSceneArchiveError::ingestion;failure.operation=mf.operation;return {};}
            resources.materials.push_back(*material);material_handles.push_back(*material);
            const auto& c=m.stored_color;
            if(!materials.set_color(*material,{c[0],c[1],c[2],c[3]},mf)||
                !materials.set_surface_properties(*material,{m.surface[0],m.surface[1],m.surface[2]},mf)) {
                failure.kind=RetailSceneArchiveError::ingestion;failure.operation=mf.operation;return {};
            }
            if(m.textured) {
                const auto t=std::find_if(decoded->textures.begin(),decoded->textures.end(),[&](const auto& x){return x.name==m.texture_name;});
                const auto index=std::size_t(t-decoded->textures.begin());
                if(!materials.set_texture(*material,texture_handles[index],mf)){failure.kind=RetailSceneArchiveError::ingestion;failure.operation=mf.operation;return {};}
            }
        }
        NativeRetailRendererFrameHandle selected_frame=parent_frame;
        const RetailRendererAffine identity{{1,0,0},{0,1,0},{0,0,1},{0,0,0}};
        for(const auto index:ancestry) {
            RetailRendererFrameFailure ff;auto frame=frames.create_base_frame(ff);
            if(!frame){failure.kind=RetailSceneArchiveError::ingestion;failure.operation=ff.operation;return {};}
            resources.frames.push_back(*frame);const auto& affine=data.frames[index].modelling;
            const auto properties=affine==identity?NativeRetailRendererMatrixDomain::identity_shortcut_mask:0u;
            if(!frames.set_modelling(*frame,{affine,properties},ff)||(!selected_frame.empty()&&!frames.add_child(selected_frame,*frame,ff))) {
                failure.kind=RetailSceneArchiveError::ingestion;failure.operation=ff.operation;return {};
            }
            selected_frame=*frame;
        }
        for(std::size_t i=0;i<groups.size();++i)if(!groups[i].empty()) {
            RetailWorldSceneObject object;object.id=base_object_id+i;object.frame=selected_frame;
            object.local_center=data.bound_center;object.bounding_radius=data.bound_radius;object.material=material_handles[i];
            object.triangles=std::move(groups[i]);object.diagnostic_unlit_raw_rgba8=true;
            // Explicit opaque native preview; source effect/blend policy is not inferred.
            object.alpha_blended=false;
            if(!world.add_scene_object(std::move(object),failure.renderer)){failure.kind=RetailSceneArchiveError::ingestion;failure.operation=failure.renderer.operation;return {};}
            resources.scene_objects.push_back(base_object_id+i);
        }
        committed=true;return decoded;
    } catch(const std::bad_alloc&){failure.kind=RetailSceneArchiveError::allocation;failure.operation="SSF scene native owner allocation";}
      catch(...){failure.kind=RetailSceneArchiveError::native_exception;failure.operation="SSF scene native owner exception";}
    return {};
}
} // namespace mka::retail
