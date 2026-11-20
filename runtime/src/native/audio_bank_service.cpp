#include "mka/audio_bank_service.hpp"
#include <list>
#include <mutex>
#include <new>
#include <stdexcept>
#include <unordered_map>
namespace mka::sound {
struct AudioBankService::State {
    explicit State(AudioBankProtocol&& p):protocol(std::move(p)){}
    enum class TokenState { reserved, completion_pending, live, unloaded };
    struct Completion { bool ready=false; BankLoadCompletion value; };
    struct Job { AudioBankCommand request; BankWireToken identity; std::list<Completion>::iterator completion; };
    AudioBankProtocol protocol;
    mutable std::mutex mutex;
    std::list<Completion> completions;
    std::unordered_map<std::uint32_t,TokenState> tokens;
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
                if (token != state->tokens.end() && token->second == TokenState::reserved) state->tokens.erase(token);
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
        if(i->second==TokenState::unloaded){f.kind=BankServiceError::unloaded_bank_token;return false;}
        if(i->second!=TokenState::live){f.kind=BankServiceError::unpublished_bank_token;return false;}
        return true;
    }
};
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
         if(!state_->tokens.emplace(token.value,State::TokenState::reserved).second){failure.kind=BankServiceError::duplicate_bank_token;return std::nullopt;}
         ++state_->operations; activity.state=state_; activity.reserved_token=token.value;}
        BankLoadOutcome result;
        result=state_->execute(request,token);
        {std::lock_guard<std::mutex> lock(state_->mutex);
         if(result.bank.value)state_->tokens.at(token.value)=State::TokenState::live;
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
        try{state_->tokens.emplace(token.value,State::TokenState::reserved);}
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
     if(outcome.bank.value)state_->tokens.at(job->identity.value)=State::TokenState::completion_pending;
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
        if(result.words[0]==0)state_->tokens.at(result.words[1])=State::TokenState::live;
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
     ++state_->operations; activity.state=state_;}
    if(!state_->protocol.unload(request,failure.protocol)){failure.kind=BankServiceError::protocol_failed;return false;}
    {std::lock_guard<std::mutex> lock(state_->mutex);state_->tokens.at(request.bank_token.value)=State::TokenState::unloaded;}
    return true;
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
    {std::lock_guard<std::mutex> lock(state_->mutex);for(auto& t:state_->tokens)t.second=State::TokenState::unloaded;}
    return true;
}
} // namespace mka::sound
