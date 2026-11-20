#include "mka/audio_bank_service.hpp"
#include <atomic>
#include <limits>
#include <list>
#include <mutex>
#include <new>
#include <stdexcept>
#include <unordered_map>
namespace mka::sound {
struct AudioBankService::State {
    explicit State(AudioBankProtocol&& p):protocol(std::move(p)){}
    enum class TokenState { reserved, completion_pending, live, unloaded, unloading, unknown };
    struct TokenRecord { TokenState state=TokenState::reserved; std::size_t program_work=0; };
    struct Completion { bool ready=false; BankLoadCompletion value; };
    struct Job { AudioBankCommand request; BankWireToken identity; std::list<Completion>::iterator completion; };
    AudioBankProtocol protocol;
    mutable std::mutex mutex;
    std::list<Completion> completions;
    std::unordered_map<std::uint32_t,TokenRecord> tokens;
    std::optional<Job> pending;
    bool busy=false, worker=false, closed=false;
    std::size_t operations=0;
    struct Activity {
        std::shared_ptr<State> state;
        std::uint32_t reserved_token = 0;
        ~Activity() {
            if (!state) return;
            std::lock_guard lock(state->mutex);
            if (reserved_token) {
                auto token = state->tokens.find(reserved_token);
                if (token != state->tokens.end() && token->second.state == TokenState::reserved) state->tokens.erase(token);
            }
            --state->operations;
        }
    };
    BankLoadOutcome execute(const AudioBankCommand& request,BankWireToken token){
        BankLoadOutcome result;
        auto load=protocol.load_unprepared(request,result.protocol_failure);
        if(!load)return result;
        auto prepared=protocol.prepare_loaded(load->handle,result.preparation,result.preparation_failure,result.protocol_failure);
        if(!prepared)return result; // Session invalidated the failed candidate.
        if(!protocol.bind_unprepared(token,load->handle,result.protocol_failure)){
            AudioProtocolFailure cleanup;protocol.discard_unprepared(load->handle,cleanup);
            return result;
        }
        result.bank=token;
        return result;
    }
    bool usable(BankWireToken token,BankServiceFailure& f)const{
        auto i=tokens.find(token.value);
        if(i==tokens.end()){f.kind=BankServiceError::unknown_bank_token;return false;}
        if(i->second.state==TokenState::unloaded){f.kind=BankServiceError::unloaded_bank_token;return false;}
        if(i->second.state==TokenState::unloading){f.kind=BankServiceError::bank_unloading;return false;}
        if(i->second.state==TokenState::unknown){f.kind=BankServiceError::bank_state_unknown;return false;}
        if(i->second.state!=TokenState::live){f.kind=BankServiceError::unpublished_bank_token;return false;}
        return true;
    }
};
struct BankProgramWork::Impl {
    Impl(std::shared_ptr<AudioBankService::State> owner,const AudioBankCommand& command)
        :service(std::move(owner)),request(command){}
    const std::shared_ptr<AudioBankService::State> service;
    const AudioBankCommand request;
    // Written only before public admission; never reset by concurrent methods.
    std::shared_ptr<PreparedProgramWork> work;
    std::mutex reservation_mutex;
    std::atomic<bool> retained=false;
    void release_reservation() {
        std::lock_guard own_lock(reservation_mutex);
        if(!retained.load(std::memory_order_acquire))return;
        std::lock_guard service_lock(service->mutex);
        auto& token=service->tokens.at(request.bank_token.value);
        --token.program_work;--service->operations;
        retained.store(false,std::memory_order_release);
    }
    ~Impl(){work.reset();release_reservation();}
};
BankProgramWork::~BankProgramWork()=default;
const AudioBankCommand& BankProgramWork::request()const noexcept{return state_->request;}
bool BankProgramWork::build(PreparationFailure& f){return state_->work->build(f);}
ProgramWorkCancellation BankProgramWork::cancel(){return state_->work->cancel();}
std::shared_ptr<const PreparedProgram> BankProgramWork::take_result(PreparationFailure& f){return state_->work->take_result(f);}
bool BankProgramWork::retire(PreparationFailure& f){
    if(!state_->work->retire(f))return false;
    state_->release_reservation();return true;
}
BankProgramWorkObservation BankProgramWork::observe()const {
    const bool retained=state_->retained.load(std::memory_order_acquire);
    return {state_->work->observe(),retained};
}
std::shared_ptr<BankProgramWork> AudioBankService::admit_deferred_program(
    const AudioBankCommand& request,PreparationFailure& preparation,BankServiceFailure& failure){
    preparation={};failure={};
    if(!state_){failure.kind=BankServiceError::moved_service;return {};}
    if(request.opcode!=AudioBankOpcode::play&&request.opcode!=AudioBankOpcode::play_extended){failure.kind=BankServiceError::wrong_command;return {};}
    try {
        // Copy immutable inputs and allocate keeper before touching counters.
        auto job=std::make_shared<BankProgramWork::Impl>(state_,request);
        {std::lock_guard lock(state_->mutex);
         if(state_->closed){failure.kind=BankServiceError::closed;return {};}
         if(!state_->usable(request.bank_token,failure))return {};
         auto& token=state_->tokens.at(request.bank_token.value);
         const auto max=std::numeric_limits<std::size_t>::max();
         if(state_->operations==max||token.program_work==max){failure.kind=BankServiceError::host_capacity;return {};}
         ++state_->operations;++token.program_work;job->retained.store(true,std::memory_order_release);}
        job->work=state_->protocol.admit_deferred_normal(job->request,preparation,failure.protocol);
        if(!job->work){failure.kind=BankServiceError::protocol_failed;return {};}
        return std::shared_ptr<BankProgramWork>(new BankProgramWork(std::move(job)));
    }catch(const std::bad_alloc&){failure.kind=BankServiceError::allocation_failed;}
    catch(const std::length_error&){failure.kind=BankServiceError::host_capacity;}
    catch(...){failure.kind=BankServiceError::native_exception;}
    return {};
}
std::shared_ptr<AudioResourceDomain> AudioBankService::resource_domain() const noexcept {
    return state_ ? state_->protocol.resource_domain() : nullptr;
}
std::optional<AudioBankService> AudioBankService::create(AudioBankProtocol&& protocol,BankServiceFailure& failure){
    failure={};try{return AudioBankService(std::make_shared<State>(std::move(protocol)));}
    catch(const std::bad_alloc&){failure.kind=BankServiceError::allocation_failed;}
    catch(const std::length_error&){failure.kind=BankServiceError::host_capacity;}return std::nullopt;
}
std::optional<BankLoadOutcome> AudioBankService::load_sync(
    const AudioBankCommand& request,BankWireToken token,BankServiceFailure& failure){
    failure={};if(!state_){failure.kind=BankServiceError::moved_service;return std::nullopt;}
    if(request.opcode!=AudioBankOpcode::load_sync){failure.kind=BankServiceError::wrong_command;return std::nullopt;}
    if(!token.value){failure.kind=BankServiceError::unsupported_null_bank_token;return std::nullopt;}
    try{
        State::Activity activity;
        {std::lock_guard<std::mutex> lock(state_->mutex);
         if(state_->closed){failure.kind=BankServiceError::closed;return std::nullopt;}
         if(state_->busy){failure.kind=BankServiceError::sync_during_async_unsupported;return std::nullopt;}
         if(!state_->tokens.emplace(token.value,State::TokenRecord{}).second){failure.kind=BankServiceError::duplicate_bank_token;return std::nullopt;}
         ++state_->operations; activity.state=state_; activity.reserved_token=token.value;}
        BankLoadOutcome result;
        result=state_->execute(request,token);
        {std::lock_guard<std::mutex> lock(state_->mutex);
         if(result.bank.value)state_->tokens.at(token.value).state=State::TokenState::live;
         else state_->tokens.erase(token.value);
         activity.reserved_token=0; // Finalize under the same lock; never erase a newer retry.
        }
        return result;
    }catch(const std::bad_alloc&){failure.kind=BankServiceError::allocation_failed;}
     catch(const std::length_error&){failure.kind=BankServiceError::host_capacity;}
     catch(...){failure.kind=BankServiceError::native_exception;}
    return std::nullopt;
}
std::optional<AsyncBankSubmission> AudioBankService::submit_async(
    const AudioBankCommand& request,BankWireToken token,BankServiceFailure& failure){
    failure={};if(!state_){failure.kind=BankServiceError::moved_service;return std::nullopt;}
    if(request.opcode!=AudioBankOpcode::load_async){failure.kind=BankServiceError::wrong_command;return std::nullopt;}
    std::lock_guard<std::mutex> lock(state_->mutex);
    if(state_->closed){failure.kind=BankServiceError::closed;return std::nullopt;}
    try{
        if(state_->busy){
            // Source2A97C..98C -> completion helper: actual status2/error7.
            State::Completion c;c.ready=true;c.value.callback_token=request.callback_token;c.value.words={2,7};
            state_->completions.push_back(std::move(c));return AsyncBankSubmission::completed_busy;
        }
        if(!token.value){failure.kind=BankServiceError::unsupported_null_bank_token;return std::nullopt;}
        if(state_->tokens.count(token.value)){failure.kind=BankServiceError::duplicate_bank_token;return std::nullopt;}
        State::Job job{request,token,{}}; // Allocate request copy before admission.
        state_->completions.emplace_back();auto node=std::prev(state_->completions.end());
        try{state_->tokens.emplace(token.value,State::TokenRecord{});}
        catch(...){state_->completions.erase(node);throw;}
        job.completion=node;node->value.callback_token=request.callback_token;
        state_->pending=std::move(job);state_->busy=true;
        // Source pending status1 is internal; it is NOT sent by2B9C8.
        return AsyncBankSubmission::queued;
    }catch(const std::bad_alloc&){failure.kind=BankServiceError::allocation_failed;}
     catch(const std::length_error&){failure.kind=BankServiceError::host_capacity;}return std::nullopt;
}
bool AudioBankService::run_one(BankServiceFailure& failure){
    failure={};if(!state_){failure.kind=BankServiceError::moved_service;return false;}
    std::optional<State::Job> job;
    State::Activity activity;
    {std::lock_guard<std::mutex> lock(state_->mutex);
     if(state_->closed){failure.kind=BankServiceError::closed;return false;}
     if(state_->worker){failure.kind=BankServiceError::worker_active;return false;}
     if(!state_->pending){failure.kind=BankServiceError::no_pending_work;return false;}
     job=std::move(state_->pending);state_->pending.reset();state_->worker=true;
     ++state_->operations; activity.state=state_;}
    BankLoadOutcome outcome;
    try { outcome=state_->execute(job->request,job->identity); }
    catch(const std::bad_alloc&) { outcome.native_failure=BankServiceError::allocation_failed; }
    catch(const std::length_error&) { outcome.native_failure=BankServiceError::host_capacity; }
    catch(...) { outcome.native_failure=BankServiceError::native_exception; }
    const auto native_failure=outcome.native_failure;
    {std::lock_guard<std::mutex> lock(state_->mutex);
     auto& c=*job->completion;c.value.words=outcome.bank.value?std::array<std::uint32_t,2>{0,outcome.bank.value}:std::array<std::uint32_t,2>{2,1};
     if(outcome.bank.value)state_->tokens.at(job->identity.value).state=State::TokenState::completion_pending;
     else state_->tokens.erase(job->identity.value);
     c.value.outcome=std::move(outcome);
     // Original2A91C clearsbusy in the JAL delay slot BEFORE invoking completion.
     state_->busy=false;state_->worker=false;c.ready=true;}
    failure.kind=native_failure;
    return native_failure==BankServiceError::none;
}
std::optional<BankLoadCompletion> AudioBankService::poll_completion(BankServiceFailure& failure){
    failure={};if(!state_){failure.kind=BankServiceError::moved_service;return std::nullopt;}
    std::lock_guard<std::mutex> lock(state_->mutex);
    if(state_->closed){failure.kind=BankServiceError::closed;return std::nullopt;}
    for(auto i=state_->completions.begin();i!=state_->completions.end();++i)if(i->ready){
        auto result=std::move(i->value);state_->completions.erase(i);
        if(result.words[0]==0)state_->tokens.at(result.words[1]).state=State::TokenState::live;
        return result;
    }
    return std::nullopt;
}
std::optional<AudioResource> AudioBankService::resolve_play_kind1(
    const AudioBankCommand& request,std::size_t index,BankServiceFailure& failure)const{
    failure={};if(!state_){failure.kind=BankServiceError::moved_service;return std::nullopt;}
    State::Activity activity;
    {std::lock_guard<std::mutex> lock(state_->mutex);
     if(state_->closed){failure.kind=BankServiceError::closed;return std::nullopt;}
     if(!state_->usable(request.bank_token,failure))return std::nullopt;
     ++state_->operations; activity.state=state_;}
    auto result=state_->protocol.resolve_play_kind1(request,index,failure.protocol);
    if(!result)failure.kind=BankServiceError::protocol_failed;
    return result;
}
bool AudioBankService::unload(const AudioBankCommand& request,BankServiceFailure& failure){
    failure={};if(!state_){failure.kind=BankServiceError::moved_service;return false;}
    if(request.opcode!=AudioBankOpcode::unload){failure.kind=BankServiceError::wrong_command;return false;}
    State::Activity activity;
    {std::lock_guard<std::mutex> lock(state_->mutex);
     if(state_->closed){failure.kind=BankServiceError::closed;return false;}
     if(!state_->usable(request.bank_token,failure))return false;
     auto& token=state_->tokens.at(request.bank_token.value);
     if(token.program_work){failure.kind=BankServiceError::program_work_active;return false;}
     if(state_->operations==std::numeric_limits<std::size_t>::max()){failure.kind=BankServiceError::host_capacity;return false;}
     ++state_->operations;activity.state=state_;token.state=State::TokenState::unloading;}
    bool ok=false,unknown=false;
    try {ok=state_->protocol.unload(request,failure.protocol);}
    catch(...){unknown=true;failure.kind=BankServiceError::native_exception;}
    {std::lock_guard<std::mutex> lock(state_->mutex);
     state_->tokens.at(request.bank_token.value).state=unknown?State::TokenState::unknown:
         ok?State::TokenState::unloaded:State::TokenState::live;}
    if(!ok&&!unknown)failure.kind=BankServiceError::protocol_failed;
    return ok;
}
std::shared_ptr<const PreparedProgram> AudioBankService::prepare_program(
    const AudioBankCommand& request,PreparationFailure& preparation,BankServiceFailure& failure) {
    preparation={};failure={};
    if(!state_){failure.kind=BankServiceError::moved_service;return {};}
    State::Activity activity;
    {std::lock_guard<std::mutex> lock(state_->mutex);
     if(state_->closed){failure.kind=BankServiceError::closed;return {};}
     if(!state_->usable(request.bank_token,failure))return {};
     ++state_->operations; activity.state=state_;}
    auto program=state_->protocol.prepare_program(request,preparation,failure.protocol);
    if(!program)failure.kind=BankServiceError::protocol_failed;
    return program;
}
bool AudioBankService::cancel_async(BankServiceFailure& failure){
    failure={};if(!state_){failure.kind=BankServiceError::moved_service;return false;}
    std::lock_guard<std::mutex> lock(state_->mutex);
    failure.kind=state_->closed?BankServiceError::closed:BankServiceError::cancellation_unsupported;return false;
}
bool AudioBankService::close_when_idle(BankServiceFailure& failure){
    failure={};if(!state_){failure.kind=BankServiceError::moved_service;return false;}
    {std::lock_guard<std::mutex> lock(state_->mutex);
     if(state_->closed){failure.kind=BankServiceError::closed;return false;}
     if(state_->busy){failure.kind=BankServiceError::cancellation_unsupported;return false;}
     if(state_->operations){failure.kind=BankServiceError::worker_active;return false;}
     if(!state_->completions.empty()){failure.kind=BankServiceError::pending_completions;return false;}
     state_->closed=true;} // Admission barrier before releasing mutex/closing protocol.
    if(!state_->protocol.close_session(failure.protocol)){
        std::lock_guard<std::mutex> lock(state_->mutex);state_->closed=false;
        failure.kind=BankServiceError::protocol_failed;return false;
    }
    {std::lock_guard<std::mutex> lock(state_->mutex);for(auto& t:state_->tokens)t.second.state=State::TokenState::unloaded;}
    return true;
}
} // namespace mka::sound
