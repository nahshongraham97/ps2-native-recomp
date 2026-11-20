#include "mka/audio_bank_service.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <stdexcept>
#include <thread>
using namespace mka::sound;
namespace {
void check(bool ok,const char* why){if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
struct Gate {std::promise<void> reached,release;std::shared_future<void> proceed=release.get_future().share();};
enum class Edge {none,before,after,throw_before,throw_after,refuse};
thread_local Edge edge=Edge::none;
thread_local Gate* gate=nullptr;
void await_gate(){gate->reached.set_value();check(gate->proceed.wait_for(std::chrono::seconds(10))==std::future_status::ready,"explicit gap releases");}
AudioBankCommand command(AudioBankOpcode op,std::uint32_t token){AudioBankCommand c;c.opcode=op;c.bank_token={token};return c;}
AudioBankService service(const char* file){
 AudioBanksFailure b;auto session=AudioBankSession::open(file,std::nullopt,b);check(bool(session),"actual session");
 AudioProtocolFailure p;auto protocol=AudioBankProtocol::create(std::move(*session),p);check(bool(protocol),"actual protocol");
 BankServiceFailure f;auto s=AudioBankService::create(std::move(*protocol),f);check(bool(s),"actual Service");return std::move(*s);
}
void load(AudioBankService& s,std::uint32_t token){AudioBankCommand c;c.opcode=AudioBankOpcode::load_sync;c.pathname="announcr";BankServiceFailure f;auto result=s.load_sync(c,{token},f);check(result&&result->bank.value==token,"actual bank publication");}
}
extern "C" std::unique_ptr<RuntimeProgramReference> real_acquire(AudioBankProtocol*,const AudioBankCommand&,PreparationFailure&,AudioProtocolFailure&) __asm__("__real__ZN3mka5sound17AudioBankProtocol23acquire_runtime_programERKNS0_16AudioBankCommandERNS0_18PreparationFailureERNS0_20AudioProtocolFailureE");
extern "C" std::unique_ptr<RuntimeProgramReference> wrapped_acquire(AudioBankProtocol*,const AudioBankCommand&,PreparationFailure&,AudioProtocolFailure&) __asm__("__wrap__ZN3mka5sound17AudioBankProtocol23acquire_runtime_programERKNS0_16AudioBankCommandERNS0_18PreparationFailureERNS0_20AudioProtocolFailureE");
extern "C" std::unique_ptr<RuntimeProgramReference> wrapped_acquire(AudioBankProtocol* self,const AudioBankCommand& c,PreparationFailure& pf,AudioProtocolFailure& f){
 if(edge==Edge::before)await_gate();
 if(edge==Edge::throw_before)throw std::runtime_error("before actual acquisition");
 if(edge==Edge::refuse){f.kind=AudioProtocolError::bank_failure;return {};}
 auto ref=real_acquire(self,c,pf,f);
 if(edge==Edge::after)await_gate();
 if(edge==Edge::throw_after)throw std::runtime_error("after actual acquisition");
 return ref;
}
namespace {
void gap(const char* file,Edge where){
 auto s=service(file);load(s,0x9876abcd);load(s,0x1234abcd);Gate g;
 std::unique_ptr<RuntimeProgramReference> result;
 std::thread worker([&]{edge=where;gate=&g;BankServiceFailure f;PreparationFailure pf;result=s.acquire_runtime_program(command(AudioBankOpcode::play,0x9876abcd),pf,f);check(bool(result),"actual runtime acquisition completes");});
 check(g.reached.get_future().wait_for(std::chrono::seconds(10))==std::future_status::ready,"downstream boundary reached");
 BankServiceFailure f;PreparationFailure pf;
 check(!s.unload(command(AudioBankOpcode::unload,0x9876abcd),f)&&f.kind==BankServiceError::runtime_reference_active,"pre and post lower acquisition retain exact-bank guard");
 check(!s.close_when_idle(f)&&f.kind==BankServiceError::runtime_reference_active,"pre and post lower acquisition retain Service");
 auto other=s.acquire_runtime_program(command(AudioBankOpcode::play,0x1234abcd),pf,f);check(bool(other),"unrelated actual bank still admitted");
 check(other->release(pf),"unrelated actual semantic release");
 check(s.unload(command(AudioBankOpcode::unload,0x1234abcd),f),"unrelated bank unload not globally blocked");
 g.release.set_value();worker.join();check(result&&result->active(),"keeper attached before publication");
 check(!s.unload(command(AudioBankOpcode::unload,0x9876abcd),f)&&f.kind==BankServiceError::runtime_reference_active,"returned exact reference still protects bank");
 check(result->release(pf),"actual returned semantic release");
 check(s.unload(command(AudioBankOpcode::unload,0x9876abcd),f)&&s.close_when_idle(f),"all real guards released");
}
void rejected(const char* file,Edge mode){
 auto s=service(file);load(s,0x91827364);BankServiceFailure f;PreparationFailure pf;
 edge=mode;auto result=s.acquire_runtime_program(command(AudioBankOpcode::play,0x91827364),pf,f);edge=Edge::none;
 check(!result&&f.kind==(mode==Edge::refuse?BankServiceError::protocol_failed:BankServiceError::native_exception),"actual failed admission remains named");
 auto retry=s.acquire_runtime_program(command(AudioBankOpcode::play,0x91827364),pf,f);check(bool(retry)&&retry->release(pf),"failed before or after actual acquisition supports real retry");
 check(s.unload(command(AudioBankOpcode::unload,0x91827364),f)&&s.close_when_idle(f),"failure keeper rolls back exactly once");
}
}
int main(int argc,char** argv){
 if(argc!=2)return 2;gap(argv[1],Edge::before);gap(argv[1],Edge::after);
 rejected(argv[1],Edge::refuse);rejected(argv[1],Edge::throw_before);rejected(argv[1],Edge::throw_after);
 std::puts("PASS native Service runtime pre/post acquisition guards and failure rollback: five CPU-only injected boundaries");
}
