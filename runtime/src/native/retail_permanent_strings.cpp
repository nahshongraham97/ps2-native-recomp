#include "mka/retail_permanent_strings.hpp"
#include "mka/retail_loaded_system_art.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"
#include <bit>
#include <cstdio>
#include <new>
#include <thread>

namespace mka::retail {
namespace {
bool fail(RetailPermanentStringFailure& f,RetailPermanentStringError kind,const char* operation) noexcept {
    f.kind=kind;f.operation=operation;return false;
}
bool report(StartupFailure& f,const RetailPermanentStringFailure& detail) noexcept {
    f.kind=StartupError::game_records;f.operation=detail.operation;return false;
}
}
NativeRetailPermanentStrings::NativeRetailPermanentStrings(NativeRetailMainState& main) noexcept
    :main_(&main),lifetime_(main.lifetime),registry_(main.resource_registry.get()) {}
std::unique_ptr<NativeRetailPermanentStrings> NativeRetailPermanentStrings::create(
    NativeRetailMainState& main,RetailPermanentStringFailure& f) noexcept {
    f={};
    if(!main.lifetime||!main.lifetime->active_on_owner_thread()) {
        fail(f,RetailPermanentStringError::main_lifetime,"permanent strings actual Main lifetime/thread");return {};
    }
    if(!main.resource_registry||!main.package) {
        fail(f,RetailPermanentStringError::registry,"215660 actual resource registry/package");return {};
    }
    try{return std::unique_ptr<NativeRetailPermanentStrings>(new NativeRetailPermanentStrings(main));}
    catch(const std::bad_alloc&){fail(f,RetailPermanentStringError::allocation,"native permanent-string producer");}
    catch(...){fail(f,RetailPermanentStringError::native_exception,"native permanent-string producer");}
    return {};
}
bool NativeRetailPermanentStrings::access(RetailPermanentStringFailure& f) const noexcept {
    f={};
    const auto life=lifetime_.lock();
    if(!life||!life->active_on_owner_thread())
        return fail(f,RetailPermanentStringError::main_lifetime,"permanent strings actual Main lifetime/thread");
    if(main_->resource_registry.get()!=registry_)
        return fail(f,RetailPermanentStringError::foreign_owner,"permanent strings cannot replace actual registry owner");
    return true;
}
void NativeRetailPermanentStrings::seal(RetailPermanentStringFailure& f) noexcept {
    failed_=true;failed_kind_=f.kind;failed_operation_=f.operation;
    if(module_)module_->phase_=RetailLoadedStringPhase::failed;
}
bool NativeRetailPermanentStrings::queue_from_215660(
    std::int32_t language,RetailPermanentStringFailure& f) noexcept {
    if(!access(f))return false;
    if(failed_)return fail(f,failed_kind_,failed_operation_);
    if(retiring_)return fail(f,RetailPermanentStringError::invalid_state,"permanent-string retirement in progress");
    const auto found=registry_->find_loaded_tag(1,f.registry);
    if(f.registry.kind!=RetailGameResourceRegistryError::none)
        return fail(f,RetailPermanentStringError::registry,"215660 actual tag lookup");
    if(found) {
        if(!record_||found->registry_identity!=record_->registry_identity||
           found->slot!=record_->slot||found->generation!=record_->generation)
            return fail(f,RetailPermanentStringError::foreign_owner,"215660 tag1 belongs to another actual producer");
        return true; // Existing tag precedes descriptor/locale selection.
    }
    if(module_||record_)return fail(f,RetailPermanentStringError::invalid_state,"permanent strings prior partial association");
    auto module=NativeRetailLoadedResource::prepare_permanent_strings(language,f.data);
    if(!module)return fail(f,RetailPermanentStringError::data,f.data.operation);
    module->main_=main_->lifetime;
    const auto handle=registry_->activate_loaded_record(module,f.registry);
    if(!handle)return fail(f,RetailPermanentStringError::registry,"215180 real queued record publication");
    // Source215180 publishes the zero/metadata record BEFORE starting IO.
    // Retain that exact prefix if native IO admission fails.
    module_=std::move(module);record_=handle;
    if(!main_->package) {
        fail(f,RetailPermanentStringError::package,"215180 actual package retired before read");seal(f);return false;
    }
    job_=main_->package->begin_read("gameart.ssf",module_->descriptor_.offset,
        module_->descriptor_.length,f.package,f.read);
    if(!job_) {
        fail(f,RetailPermanentStringError::package,"215180 actual native permanent-string read");seal(f);return false;
    }
    module_->phase_=RetailLoadedStringPhase::reading;
    module_->tag_=1; //215710/2157E0 after actual request publication.
    return true;
}
std::optional<RetailPermanentStringStep> NativeRetailPermanentStrings::advance_from_215730(
    RetailPermanentStringFailure& f) noexcept {
    if(!access(f))return {};
    if(failed_){fail(f,failed_kind_,failed_operation_);return {};}
    if(retiring_){fail(f,RetailPermanentStringError::invalid_state,"permanent strings retiring");return {};}
    if(!module_) {
        if(!queue_from_215660(std::bit_cast<std::int32_t>(main_->post_overlay_state),f))return {};
    }
    if(module_->phase_==RetailLoadedStringPhase::ready)return RetailPermanentStringStep::complete;
    if(!job_) {
        fail(f,RetailPermanentStringError::invalid_state,"215730 queued record has no native job");seal(f);return {};
    }
    try {
        const auto status=job_->poll(f.read);
        if(!status){fail(f,RetailPermanentStringError::read,"215730 actual worker observation");seal(f);return {};}
        if(*status==content::NativeFileReadStatus::pending)return RetailPermanentStringStep::pending;
        if(*status!=content::NativeFileReadStatus::complete) {
            fail(f,RetailPermanentStringError::read,"215730 worker failed/cancelled");seal(f);return {};
        }
        const auto bytes=job_->bytes(f.read);
        if(!bytes){fail(f,RetailPermanentStringError::read,"2170E0 actual completed bytes");seal(f);return {};}
        auto image=NativeRetailPermanentStringImage::decode(
            std::vector<std::uint8_t>(bytes->begin(),bytes->end()),f.data);
        if(!image){fail(f,RetailPermanentStringError::data,f.data.operation);seal(f);return {};}
        module_->image_=std::move(image);
        //215120: finish2170E0, release actual3DE190 job, then converted.
        job_.reset();
        module_->phase_=RetailLoadedStringPhase::ready;
        return RetailPermanentStringStep::complete;
    } catch(const std::bad_alloc&){fail(f,RetailPermanentStringError::allocation,"native permanent-string byte owner");}
    catch(...){fail(f,RetailPermanentStringError::native_exception,"native permanent-string completion");}
    seal(f);return {};
}
std::optional<RetailPermanentStringStep> NativeRetailPermanentStrings::shutdown(
    RetailPermanentStringFailure& f) noexcept {
    f={};
    if(std::this_thread::get_id()!=owner_thread_) {
        fail(f,RetailPermanentStringError::main_lifetime,"permanent-string cleanup owner thread");return {};
    }
    retiring_=true;
    if(module_)module_->phase_=RetailLoadedStringPhase::retired;
    if(job_) {
        if(!job_->cancel(f.read)){fail(f,RetailPermanentStringError::read,"215010 native read cancellation");return {};}
        const auto status=job_->poll(f.read);
        if(!status){fail(f,RetailPermanentStringError::read,"215010 native cancellation completion");return {};}
        if(*status==content::NativeFileReadStatus::pending)return RetailPermanentStringStep::pending;
        job_.reset();
    }
    if(module_) {module_->image_.reset();module_->tag_=-1;}
    if(record_) {
        // Main calls this before retiring its lifetime or destroying registry.
        // A final escaped producer can still free native buffers after Main is
        // gone; the already destroyed registry must not be dereferenced.
        if(const auto life=lifetime_.lock();life&&life->active_on_owner_thread()) {
            if(main_->resource_registry.get()!=registry_) {
                fail(f,RetailPermanentStringError::foreign_owner,"215010 actual registry replaced before cleanup");return {};
            }
            if(!registry_->retire_loaded_record(*record_,f.registry)) {
                fail(f,RetailPermanentStringError::registry,"217FC0 actual record retirement");return {};
            }
        }
        record_.reset();
    }
    module_.reset();failed_=false;retiring_=false;
    return RetailPermanentStringStep::complete;
}
std::optional<RetailPermanentStringObservation> NativeRetailPermanentStrings::observe(
    RetailPermanentStringFailure& f) const noexcept {
    if(!access(f))return {};
    return RetailPermanentStringObservation{bool(record_),bool(job_),
        module_&&module_->phase_==RetailLoadedStringPhase::ready,failed_,
        module_?module_->source_name():std::string_view{},record_};
}
NativeRetailPermanentStrings::~NativeRetailPermanentStrings() {
    RetailPermanentStringFailure f;
    for(;;) {
        const auto result=shutdown(f);
        if(!result){std::fprintf(stderr,"native permanent-string cleanup failed: %s\n",f.operation);std::terminate();}
        if(*result==RetailPermanentStringStep::complete)break;
        std::this_thread::yield();
    }
}
namespace loaded_art_required {
bool queue_permanent_strings_215660(NativeRetailMainState& main,StartupFailure& f) {
    f={};RetailPermanentStringFailure sf;
    if(!main.permanent_strings) {
        auto owner=NativeRetailPermanentStrings::create(main,sf);
        if(!owner)return report(f,sf);
        main.permanent_strings=std::move(owner);
    }
    if(!main.permanent_strings->queue_from_215660(
        std::bit_cast<std::int32_t>(main.post_overlay_state),sf))return report(f,sf);
    return true;
}
bool complete_permanent_strings_215730(NativeRetailMainState& main,StartupFailure& f) {
    f={};
    if(!main.permanent_strings&&!queue_permanent_strings_215660(main,f))return false;
    RetailPermanentStringFailure sf;
    for(;;) {
        const auto result=main.permanent_strings->advance_from_215730(sf);
        if(!result)return report(f,sf);
        if(*result==RetailPermanentStringStep::complete)return true;
        // Actual Main startup, before task creation. Other task-yield callers
        // need their own native continuation; no guest scheduler is installed.
        std::this_thread::yield();
    }
}
}
} // namespace mka::retail
