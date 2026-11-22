#include "mka/retail_loaded_system_art.hpp"
#include "mka/retail_game_session.hpp"
#include "retail_main_control.hpp"
#include <cstring>
#include <cstdio>
#include <new>

namespace mka::retail {
namespace {
bool fail(RetailLoadedArtFailure& f, RetailLoadedArtError kind, const char* operation) noexcept {
    f.kind=kind; f.operation=operation; return false;
}
}
std::optional<graphics::NativeTextureSampler> native_sampler_from_retail_texture(
    std::uint32_t metadata, RetailLoadedArtFailure& f) noexcept {
    f={};
    using namespace graphics;
    NativeTextureSampler result;
    // Original1CFCA0: source1 leaves TEX1 sampling bits clear; source2 sets60.
    // Original1D3584..35C8: address3 sets the corresponding clamp bit.
    switch(metadata&0xffu) {
    case 1: result.filter=NativeTextureFilter::nearest; break;
    case 2: result.filter=NativeTextureFilter::linear; break;
    default: fail(f,RetailLoadedArtError::sampler,"unimplemented retail texture filter"); return {};
    }
    auto address=[](std::uint32_t mode) {
        return mode==1 ? NativeTextureAddress::repeat :
               mode==3 ? NativeTextureAddress::clamp_to_edge : NativeTextureAddress::unspecified;
    };
    result.u=address((metadata>>8)&15u); result.v=address((metadata>>12)&15u);
    if(result.u==NativeTextureAddress::unspecified||result.v==NativeTextureAddress::unspecified) {
        fail(f,RetailLoadedArtError::sampler,"unimplemented retail texture address policy"); return {};
    }
    return result;
}
NativeRetailSectionTexture::NativeRetailSectionTexture(
    std::weak_ptr<NativeRetailGameSession> session,
    std::shared_ptr<const NativeRetailSystemArtCpuSection> cpu, std::size_t ordinal,
    std::size_t index, std::uint32_t sampler)
    : cpu_(std::move(cpu)), session_(std::move(session)),
      ordinal_(ordinal), texture_index_(index), sampler_metadata_(sampler) {}
std::shared_ptr<const graphics::NativeGlTexture> NativeRetailSectionTexture::image(
    RetailLoadedArtFailure& f) const noexcept {
    f={};
    if(std::this_thread::get_id()!=owner_thread_) {
        fail(f,RetailLoadedArtError::wrong_thread,"section texture owner thread"); return {};
    }
    const auto session=session_.lock(); RetailGameSessionFailure sf;
    if(!session||!session->validate_current_thread(sf)) {
        fail(f,RetailLoadedArtError::session,"section texture Session retired"); return {};
    }
    if(!live_||!image_) {
        fail(f,RetailLoadedArtError::stale_resource,"retired native section texture"); return {};
    }
    if(!image_->gl_object(f.texture)) {
        fail(f,RetailLoadedArtError::texture,"section texture native capability admission"); return {};
    }
    return image_;
}
std::optional<std::uint32_t> NativeRetailSectionTexture::sampler_metadata(
    RetailLoadedArtFailure& f) const noexcept {
    if(!image(f)) return {};
    return sampler_metadata_;
}
const FontAtlas* NativeRetailSectionTexture::atlas(RetailLoadedArtFailure& f) const noexcept {
    if(!image(f)) return nullptr;
    return &cpu_->textures()[texture_index_].data;
}
NativeRetailLoadedSystemArt::NativeRetailLoadedSystemArt(
    std::weak_ptr<NativeRetailGameSession> session) noexcept : session_(std::move(session)) {}
bool NativeRetailLoadedSystemArt::thread(RetailLoadedArtFailure& f) const noexcept {
    return std::this_thread::get_id()==owner_thread_ ||
        fail(f,RetailLoadedArtError::wrong_thread,"native loaded section owner thread");
}
bool NativeRetailLoadedSystemArt::access(RetailLoadedArtFailure& f) const noexcept {
    f={}; if(!thread(f)) return false;
    const auto session=session_.lock(); RetailGameSessionFailure sf;
    return (session&&session->validate_current_thread(sf)) ||
        fail(f,RetailLoadedArtError::session,"native loaded section Session retired");
}
std::shared_ptr<NativeRetailLoadedSystemArt> NativeRetailLoadedSystemArt::for_session(
    const std::shared_ptr<NativeRetailGameSession>& session, RetailLoadedArtFailure& f) noexcept {
    f={}; RetailGameSessionFailure sf;
    if(!session||!session->validate_current_thread(sf)) {
        fail(f,RetailLoadedArtError::session,"actual native Session for loaded sections"); return {};
    }
    if(session->system_art_) return session->system_art_;
    try {
        auto owner=std::shared_ptr<NativeRetailLoadedSystemArt>(new NativeRetailLoadedSystemArt(session));
        session->system_art_=owner; return owner;
    } catch(const std::bad_alloc&) { fail(f,RetailLoadedArtError::allocation,"persistent native loaded-section owner"); }
    catch(...) { fail(f,RetailLoadedArtError::native_exception,"persistent native loaded-section owner"); }
    return {};
}
std::shared_ptr<NativeRetailLoadedSystemArt> NativeRetailGameSession::loaded_system_art(
    RetailGameSessionFailure& f) const noexcept {
    if(!validate_current_thread(f)) return {};
    return system_art_;
}
void NativeRetailLoadedSystemArt::seal(RetailLoadedArtFailure& f) noexcept {
    phase_=RetailLoadedArtPhase::failed;
    // Keep the original caller's full decoder failure. The sticky failure
    // itself must not allocate while handling an allocation/after-effect error.
    sealed_failure_.kind=f.kind; sealed_failure_.operation=f.operation;
    sealed_failure_.ordinal=f.ordinal; sealed_failure_.texture=f.texture;
    sealed_failure_.read=f.read;
}
bool NativeRetailLoadedSystemArt::queue_from_19b530(
    NativeRetailPackage& package, std::int32_t language, RetailLoadedArtFailure& f) noexcept {
    if(!access(f)) return false;
    const auto descriptor=system_art_descriptor_from_source(language,f.cpu);
    if(!descriptor) return fail(f,RetailLoadedArtError::cpu,"actual selected sysart descriptor");
    if(phase_==RetailLoadedArtPhase::failed) { f=sealed_failure_; return false; }
    if(phase_!=RetailLoadedArtPhase::empty&&phase_!=RetailLoadedArtPhase::retired) {
        if(language_==language&&phase_!=RetailLoadedArtPhase::retiring) return true;
        return fail(f,RetailLoadedArtError::busy,"retire previous native sysart association before replacement");
    }
    auto job=package.begin_read("gameart.ssf",descriptor->offset,
        std::size_t(descriptor->length),f.package,f.read);
    if(!job) return fail(f,RetailLoadedArtError::package,"19B530 actual opened-package asynchronous read");
    // Publication cannot throw after the independent native job starts.
    job_=std::move(job); language_=language; next_texture_=retire_index_=0;
    phase_=RetailLoadedArtPhase::reading; return true;
}
std::optional<RetailLoadedArtStep> NativeRetailLoadedSystemArt::advance_from_19aa30(
    const std::shared_ptr<graphics::NativeGlTextureUpload>& device, RetailLoadedArtFailure& f) {
    if(!access(f)) return {};
    if(phase_==RetailLoadedArtPhase::failed) { f=sealed_failure_; return {}; }
    if(phase_==RetailLoadedArtPhase::converted) return RetailLoadedArtStep::complete;
    if(phase_!=RetailLoadedArtPhase::reading&&phase_!=RetailLoadedArtPhase::converting) {
        fail(f,RetailLoadedArtError::invalid_state,"19AA30 requires queued native section"); return {};
    }
    try {
        if(phase_==RetailLoadedArtPhase::reading) {
            const auto status=job_->poll(f.read);
            if(!status) { fail(f,RetailLoadedArtError::read,"native section read owner"); seal(f); return {}; }
            if(*status==content::NativeFileReadStatus::pending) return RetailLoadedArtStep::pending;
            if(*status!=content::NativeFileReadStatus::complete) {
                fail(f,RetailLoadedArtError::read,"native section read failed or cancelled"); seal(f); return {};
            }
            const auto bytes=job_->bytes(f.read);
            if(!bytes) { fail(f,RetailLoadedArtError::read,"complete native section read bytes"); seal(f); return {}; }
            std::vector<std::byte> serialized(bytes->size());
            std::memcpy(serialized.data(),bytes->data(),bytes->size());
            auto cpu=decode_system_art_cpu_section(std::move(serialized),language_,f.cpu);
            if(!cpu) { fail(f,RetailLoadedArtError::cpu,"3011D0 actual retained section CPU decode"); seal(f); return {}; }
            // Stage every fallible metadata owner before the first GPU effect.
            std::vector<std::shared_ptr<NativeRetailSectionTexture>> staged;
            staged.reserve(cpu->textures().size());
            for(std::size_t i=0;i<cpu->textures().size();++i) {
                const auto& t=cpu->textures()[i];
                staged.push_back(std::shared_ptr<NativeRetailSectionTexture>(
                    new NativeRetailSectionTexture(session_,cpu,t.ordinal,i,t.sampler_metadata)));
            }
            cpu_=std::move(cpu); textures_=std::move(staged);
            phase_=RetailLoadedArtPhase::converting;
        }
        if(!device) { fail(f,RetailLoadedArtError::texture,"actual native GPU image owner required"); seal(f); return {}; }
        if(device_&&device_!=device) {
            fail(f,RetailLoadedArtError::foreign_device,"section conversion cannot switch native GPU owners"); seal(f); return {};
        }
        device_=device;
        for(;next_texture_<textures_.size();++next_texture_) {
            auto& target=*textures_[next_texture_]; f.ordinal=target.ordinal_;
            const auto sampler=native_sampler_from_retail_texture(target.sampler_metadata_,f);
            if(!sampler) { f.ordinal=target.ordinal_; seal(f); return {}; }
            const auto& source=cpu_->textures()[next_texture_].data;
            auto upload=device_->upload_rgba_image(
                {source.width(),source.height(),source.rgba_raw_alpha()},*sampler);
            if(!upload.texture||upload.failure.kind!=graphics::NativeTextureError::none) {
                f.texture=upload.failure; fail(f,RetailLoadedArtError::texture,"3011D0 native texture conversion");
                f.ordinal=target.ordinal_; seal(f); return {};
            }
            target.image_=std::move(upload.texture); target.live_=true;
        }
        // Actual3DE190 releases the completed job BEFORE3014BC converted.
        job_.reset();
        phase_=RetailLoadedArtPhase::converted;
        f={}; return RetailLoadedArtStep::complete;
    } catch(const detail::NativeMainReentry&) { throw; }
    catch(const std::bad_alloc&) { fail(f,RetailLoadedArtError::allocation,"native section conversion metadata"); }
    catch(...) { fail(f,RetailLoadedArtError::native_exception,"native section conversion after-effect failure"); }
    seal(f); return {};
}
std::optional<std::shared_ptr<NativeRetailSectionTexture>>
NativeRetailLoadedSystemArt::find_texture_from_300b60(
    std::string_view name, RetailLoadedArtFailure& f) const noexcept {
    if(!access(f)) return {};
    ++texture_lookups_;
    if(phase_!=RetailLoadedArtPhase::converted) return std::shared_ptr<NativeRetailSectionTexture>{};
    // The admitted sysart source descriptor has one subrecord. Preserve the
    // forward directory scan and first matching name, not texture-vector order.
    for(std::size_t ordinal=0;ordinal<cpu_->directory().records().size();++ordinal) {
        if(cpu_->directory().records()[ordinal].name!=name) continue;
        for(const auto& texture:textures_) if(texture->ordinal_==ordinal) return texture;
        return std::shared_ptr<NativeRetailSectionTexture>{};
    }
    return std::shared_ptr<NativeRetailSectionTexture>{};
}
std::optional<std::span<const std::byte>> NativeRetailLoadedSystemArt::find_data_from_300440(
    std::string_view name, std::uint32_t& length, RetailLoadedArtFailure& f) const noexcept {
    if(!access(f)) return {};
    ++metrics_lookups_;
    // Optional failure is distinct from an engaged empty source-null payload.
    if(phase_!=RetailLoadedArtPhase::converted) return std::span<const std::byte>{};
    for(std::size_t i=0;i<cpu_->directory().records().size();++i) {
        const auto& record=cpu_->directory().records()[i];
        if(record.name!=name) continue;
        length=record.raw_word_08; // Written even for matched null payload.
        if(!record.payload_offset) return std::span<const std::byte>{};
        content::ArtSectionFailure af;
        auto bytes=cpu_->directory().resource_bytes(i,af);
        if(!bytes||length>bytes->size()) {
            fail(f,RetailLoadedArtError::cpu,"300440 retained native record payload"); return {};
        }
        return bytes->first(length);
    }
    return std::span<const std::byte>{};
}
std::optional<RetailSelectedFontZero> NativeRetailLoadedSystemArt::configure_zero_from_16fc70(
    RetailLoadedArtFailure& f) {
    if(!access(f)) return {};
    if(phase_==RetailLoadedArtPhase::failed) { f=sealed_failure_; return {}; }
    if(font_zero_.texture) {
        if(!font_zero_.texture->image(f)) return {};
        return font_zero_;
    }
    const auto texture=find_texture_from_300b60("EDDMONDGRAY14",f);
    if(!texture) return {};
    if(!*texture) return font_zero_; // No MET lookup, sampler or cache effect.
    std::uint32_t length=0;
    const auto data=find_data_from_300440("EDDMONDGRAY14_MET",length,f);
    if(!data) return {};
    std::optional<text::NativeFontMetrics> metrics;
    if(!data->empty()) {
        for(const auto& entry:cpu_->metrics()) {
            if(cpu_->directory().records()[entry.ordinal].name=="EDDMONDGRAY14_MET") {
                metrics=entry.data; break;
            }
        }
        if(!metrics) {
            fail(f,RetailLoadedArtError::cpu,"16FC70 matched MET requires existing native metrics decoder"); return {};
        }
    }
    auto& selected=**texture;
    const auto metadata=font_zero_sampler_from_16fc70(selected.sampler_metadata_);
    const auto sampler=native_sampler_from_retail_texture(metadata,f);
    if(!sampler) return {};
    if(!device_->set_image_sampler(selected.image_,*sampler,f.texture)) {
        fail(f,RetailLoadedArtError::texture,"16FC70 actual native image sampler update");
        seal(f); return {};
    }
    selected.sampler_metadata_=metadata;
    font_zero_.texture=*texture; font_zero_.metrics=std::move(metrics);
    return font_zero_;
}
std::optional<RetailLoadedArtStep> NativeRetailLoadedSystemArt::retire(
    RetailLoadedArtFailure& f) noexcept {
    f={}; if(!thread(f)) return {};
    phase_=RetailLoadedArtPhase::retiring;
    if(job_) {
        if(!job_->cancel(f.read)) { fail(f,RetailLoadedArtError::read,"3DE060 native read cancellation"); return {}; }
        const auto status=job_->poll(f.read);
        if(!status) { fail(f,RetailLoadedArtError::read,"3DE060 native read terminal observation"); return {}; }
        if(*status==content::NativeFileReadStatus::pending) return RetailLoadedArtStep::pending;
        job_.reset();
    }
    for(;retire_index_<textures_.size();++retire_index_) {
        auto& texture=*textures_[retire_index_];
        if(!texture.image_) continue;
        texture.live_=false;
        if(!device_->revoke_image(texture.image_,f.texture)) {
            f.ordinal=texture.ordinal_;
            fail(f,RetailLoadedArtError::texture,"2FFD60 actual section image retirement"); return {};
        }
        texture.image_.reset();
    }
    textures_.clear(); cpu_.reset(); device_.reset();
    // Source cache words are not silently cleared/repopulated by section
    // destruction. Retained typed cache capabilities report stale instead.
    phase_=RetailLoadedArtPhase::retired;
    return RetailLoadedArtStep::complete;
}
std::optional<RetailLoadedArtStep> NativeRetailLoadedSystemArt::retire_from_19cfa0(
    RetailLoadedArtFailure& f) noexcept {
    // Native cleanup remains possible after game Session retirement.
    f={}; if(!thread(f)) return {};
    return retire(f);
}
NativeRetailLoadedSystemArt::~NativeRetailLoadedSystemArt() {
    RetailLoadedArtFailure f;
    // Native finalization retains pending handles/buffers until cancellation
    // reaches a terminal state, even if the game Session has just retired.
    for(;;) {
        const auto done=retire(f);
        if(!done) {
            std::fprintf(stderr,"native loaded-section cleanup failed: %s\n",f.operation); std::terminate();
        }
        if(*done==RetailLoadedArtStep::complete) break;
        std::this_thread::yield();
    }
}
std::optional<RetailLoadedArtObservation> NativeRetailLoadedSystemArt::observe(
    RetailLoadedArtFailure& f) const noexcept {
    if(!access(f)) return {};
    return RetailLoadedArtObservation{phase_,language_,next_texture_,
        cpu_?cpu_->directory().records().size():0,bool(job_),bool(font_zero_.texture),
        bool(font_zero_.metrics),texture_lookups_,metrics_lookups_};
}
} // namespace mka::retail
