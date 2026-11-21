#include "mka/retail_crt.hpp"
#include <chrono>
#include <deque>
#include <limits>
#include <mutex>
#include <new>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>
namespace mka::retail {
struct NativeCrtWorkReceipt::State {mutable std::mutex mutex;NativeCrtWorkObservation observation{NativeCrtWorkState::pending,{}};};
NativeCrtWorkReceipt::NativeCrtWorkReceipt(std::shared_ptr<State> s):state_(std::move(s)){}
NativeCrtWorkObservation NativeCrtWorkReceipt::observe()const noexcept {std::lock_guard lock(state_->mutex);return state_->observation;}
struct NativeRetailCrt::State {
    using Clock=std::chrono::steady_clock;
    struct Work {std::shared_ptr<NativeCrtTaskCapability> task;NativeCrtTaskOperation operation;std::shared_ptr<NativeCrtWorkReceipt::State> receipt;};
    struct Callback {std::shared_ptr<const void> identity;NativeCrtCallback function;std::shared_ptr<const NativeCrtCallbackArgument> argument;};
    mutable std::mutex mutex;
    const std::thread::id thread=std::this_thread::get_id();
    const Clock::time_point origin=Clock::now();Clock::time_point last=origin;
    std::shared_ptr<const void> identity=std::make_shared<int>(0);
    bool finalized=false,closed=false,pumping=false;std::size_t running=0;
    std::deque<Work> queue;std::vector<Callback> callbacks;
    bool owner(RetailCrtFailure& f)const {if(std::this_thread::get_id()!=thread){f.kind=RetailCrtError::wrong_thread;return false;}return true;}
    bool live(RetailCrtFailure& f)const {if(closed){f.kind=RetailCrtError::closed;return false;}return true;}
};
NativeRetailCrt::NativeRetailCrt(std::unique_ptr<State> s):state_(std::move(s)){}
NativeRetailCrt::~NativeRetailCrt(){
    // Dropped work reports failure, never fabricated completion. Work receipt
    // and capability owners outlive the registry lock during destruction.
    for(auto& work:state_->queue){std::lock_guard lock(work.receipt->mutex);work.receipt->observation={NativeCrtWorkState::failed,{RetailCrtError::closed}};}
}
std::unique_ptr<NativeRetailCrt> NativeRetailCrt::create(RetailCrtFailure& f)noexcept {
    f={};try{return std::unique_ptr<NativeRetailCrt>(new NativeRetailCrt(std::make_unique<State>()));}
    catch(const std::bad_alloc&){f.kind=RetailCrtError::allocation;}catch(...){f.kind=RetailCrtError::native_exception;}return {};
}
bool NativeRetailCrt::finalize_pre_main(RetailCrtFailure& f)noexcept {
    f={};std::lock_guard lock(state_->mutex);if(!state_->owner(f)||!state_->live(f))return false;
    if(state_->finalized){f.kind=RetailCrtError::already_finalized;return false;}state_->finalized=true;return true;
}
bool NativeRetailCrt::validate_current_thread(RetailCrtFailure& f) const noexcept {
    f={};std::lock_guard lock(state_->mutex);
    if(!state_->owner(f)||!state_->live(f))return false;
    if(!state_->finalized){f.kind=RetailCrtError::not_finalized;return false;}
    return true;
}
std::optional<std::uint64_t> NativeRetailCrt::now_nanoseconds(RetailCrtFailure& f)noexcept {
    f={};std::lock_guard lock(state_->mutex);if(!state_->live(f))return {};
    const auto now=State::Clock::now();if(now<state_->last){f.kind=RetailCrtError::clock_regressed;return {};}
    const auto elapsed=now-state_->origin;
    // Convert without a lossy floating point intermediate or signed overflow.
    const auto seconds=std::chrono::duration_cast<std::chrono::seconds>(elapsed);
    if(seconds.count()<0||static_cast<std::uintmax_t>(seconds.count())>std::numeric_limits<std::uint64_t>::max()/1000000000u){f.kind=RetailCrtError::clock_overflow;return {};}
    const auto remainder=std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed-seconds).count();
    auto value=static_cast<std::uint64_t>(seconds.count())*1000000000u;
    if(remainder<0||static_cast<std::uint64_t>(remainder)>std::numeric_limits<std::uint64_t>::max()-value){f.kind=RetailCrtError::clock_overflow;return {};}
    state_->last=now;return value+static_cast<std::uint64_t>(remainder);
}
std::shared_ptr<NativeCrtWorkReceipt> NativeRetailCrt::submit(std::shared_ptr<NativeCrtTaskCapability> task,NativeCrtTaskOperation op,RetailCrtFailure& f)noexcept {
    f={};if(!task||(op!=NativeCrtTaskOperation::wake&&op!=NativeCrtTaskOperation::park&&op!=NativeCrtTaskOperation::yield)){f.kind=RetailCrtError::invalid_capability;return {};}
    try{auto receipt=std::make_shared<NativeCrtWorkReceipt::State>();auto result=std::shared_ptr<NativeCrtWorkReceipt>(new NativeCrtWorkReceipt(receipt));
        State::Work work{std::move(task),op,std::move(receipt)};
        {std::lock_guard lock(state_->mutex);if(!state_->live(f))return {};state_->queue.push_back(std::move(work));}return result;
    }catch(const std::bad_alloc&){f.kind=RetailCrtError::allocation;}catch(const std::length_error&){f.kind=RetailCrtError::host_capacity;}catch(...){f.kind=RetailCrtError::native_exception;}return {};
}
std::optional<std::size_t> NativeRetailCrt::pump(std::size_t max_work,RetailCrtFailure& f)noexcept {
    f={};{std::lock_guard lock(state_->mutex);if(!state_->owner(f)||!state_->live(f))return {};if(!state_->finalized){f.kind=RetailCrtError::not_finalized;return {};}if(state_->pumping||state_->running){f.kind=RetailCrtError::work_active;return {};}state_->pumping=true;}
    std::size_t done=0;
    while(done<max_work){
        std::optional<State::Work> work;
        {std::lock_guard lock(state_->mutex);if(state_->queue.empty())break;if(state_->running==std::numeric_limits<std::size_t>::max()){state_->pumping=false;f.kind=RetailCrtError::host_capacity;return {};}work.emplace(std::move(state_->queue.front()));state_->queue.pop_front();++state_->running;}
        {std::lock_guard lock(work->receipt->mutex);work->receipt->observation.state=NativeCrtWorkState::running;}
        RetailCrtFailure result;bool ok=false;
        try{switch(work->operation){case NativeCrtTaskOperation::wake:ok=work->task->wake(result);break;case NativeCrtTaskOperation::park:ok=work->task->park(result);break;case NativeCrtTaskOperation::yield:ok=work->task->yield(result);break;}}
        catch(...){result.kind=RetailCrtError::native_exception;}
        if(!ok&&result.kind==RetailCrtError::none)result.kind=RetailCrtError::callback_failed;
        {std::lock_guard lock(work->receipt->mutex);work->receipt->observation={ok?NativeCrtWorkState::completed:NativeCrtWorkState::failed,result};}
        {std::lock_guard lock(state_->mutex);--state_->running;}
        ++done;
    }
    {std::lock_guard lock(state_->mutex);state_->pumping=false;}return done;
}
std::optional<NativeCrtCallbackToken> NativeRetailCrt::register_callback(NativeCrtCallback function,std::shared_ptr<const NativeCrtCallbackArgument> argument,RetailCrtFailure& f)noexcept {
    f={};if(!function||!argument){f.kind=RetailCrtError::invalid_capability;return {};}
    try{auto identity=std::make_shared<int>(0);State::Callback callback{identity,function,std::move(argument)};
        {std::lock_guard lock(state_->mutex);if(!state_->live(f))return {};state_->callbacks.push_back(std::move(callback));}
        NativeCrtCallbackToken token;token.owner_=state_->identity;token.identity_=std::move(identity);return token;}
    catch(const std::bad_alloc&){f.kind=RetailCrtError::allocation;}catch(const std::length_error&){f.kind=RetailCrtError::host_capacity;}catch(...){f.kind=RetailCrtError::native_exception;}return {};
}
bool NativeRetailCrt::invoke_callback(const NativeCrtCallbackToken& token,RetailCrtFailure& f)noexcept {
    f={};std::optional<State::Callback> callback;
    {std::lock_guard lock(state_->mutex);if(!state_->owner(f)||!state_->live(f))return false;
     if(!state_->finalized){f.kind=RetailCrtError::not_finalized;return false;}
     if(state_->pumping||state_->running){f.kind=RetailCrtError::work_active;return false;}
     if(token.owner_==state_->identity)for(const auto& item:state_->callbacks)if(item.identity==token.identity_){callback=item;break;}
     if(!callback){f.kind=RetailCrtError::unknown_callback;return false;}if(state_->running==std::numeric_limits<std::size_t>::max()){f.kind=RetailCrtError::host_capacity;return false;}++state_->running;}
    bool ok=false;try{ok=callback->function(callback->argument,f);}catch(...){f.kind=RetailCrtError::native_exception;}
    if(!ok&&f.kind==RetailCrtError::none)f.kind=RetailCrtError::callback_failed;
    {std::lock_guard lock(state_->mutex);--state_->running;}return ok;
}
bool NativeRetailCrt::unregister_callback(const NativeCrtCallbackToken& token,RetailCrtFailure& f)noexcept {
    f={};std::optional<State::Callback> retired;
    {std::lock_guard lock(state_->mutex);if(!state_->live(f))return false;if(token.owner_==state_->identity)for(auto i=state_->callbacks.begin();i!=state_->callbacks.end();++i)if(i->identity==token.identity_){retired.emplace(std::move(*i));state_->callbacks.erase(i);return true;}f.kind=RetailCrtError::unknown_callback;}return false;
}
NativeRetailCrtObservation NativeRetailCrt::observe()const noexcept {std::lock_guard lock(state_->mutex);return {state_->finalized,state_->closed,state_->queue.size(),state_->running,state_->callbacks.size()};}
bool NativeRetailCrt::shutdown(RetailCrtFailure& f)noexcept {
    f={};std::lock_guard lock(state_->mutex);if(!state_->owner(f)||!state_->live(f))return false;
    if(state_->pumping||state_->running||!state_->queue.empty()){f.kind=RetailCrtError::work_active;return false;}
    if(!state_->callbacks.empty()){f.kind=RetailCrtError::callbacks_active;return false;}state_->closed=true;return true;
}
} // namespace mka::retail
