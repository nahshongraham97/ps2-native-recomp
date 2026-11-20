#include "mka/audio_bank_service.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <stdexcept>
#include <thread>
using namespace mka::sound;
namespace {
void check(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
struct Gate {std::promise<void> reached,release;std::shared_future<void> proceed=release.get_future().share();};
enum class Edge {none,before,after,refuse,throw_before,throw_after};
thread_local Edge edge=Edge::none;
thread_local Gate* gate=nullptr;
void wait_gate(){gate->reached.set_value();check(gate->proceed.wait_for(std::chrono::seconds(10))==std::future_status::ready,"fixture gate releases");}
AudioBankCommand command(AudioBankOpcode opcode,std::uint32_t token){AudioBankCommand c;c.opcode=opcode;c.bank_token={token};return c;}
AudioBankService service(const char* path){
 AudioBanksFailure b;auto session=AudioBankSession::open(path,std::nullopt,b);check(bool(session),"actual Session");
 AudioProtocolFailure p;auto protocol=AudioBankProtocol::create(std::move(*session),p);check(bool(protocol),"actual Protocol");
 BankServiceFailure f;auto result=AudioBankService::create(std::move(*protocol),f);check(bool(result),"actual Service");return std::move(*result);
}
void load(AudioBankService& s,std::uint32_t token){AudioBankCommand c;c.opcode=AudioBankOpcode::load_sync;c.pathname="announcr";BankServiceFailure f;auto r=s.load_sync(c,{token},f);check(r&&r->bank.value==token,"actual source-deferred bank");}
}
// This single linked call is wrapped ONLY in this fixture. No fault flags enter
// the native archive or a production executable.
extern "C" bool real_unload(AudioBankProtocol*,const AudioBankCommand&,AudioProtocolFailure&) __asm__("__real__ZN3mka5sound17AudioBankProtocol6unloadERKNS0_16AudioBankCommandERNS0_20AudioProtocolFailureE");
extern "C" bool wrapped_unload(AudioBankProtocol*,const AudioBankCommand&,AudioProtocolFailure&) __asm__("__wrap__ZN3mka5sound17AudioBankProtocol6unloadERKNS0_16AudioBankCommandERNS0_20AudioProtocolFailureE");
extern "C" bool wrapped_unload(AudioBankProtocol* self,const AudioBankCommand& c,AudioProtocolFailure& f){
 if(edge==Edge::before)wait_gate();
 if(edge==Edge::refuse){f.kind=AudioProtocolError::bank_failure;return false;}
 if(edge==Edge::throw_before)throw std::runtime_error("fixture before mutation");
 const bool result=real_unload(self,c,f);
 if(edge==Edge::after)wait_gate();
 if(edge==Edge::throw_after)throw std::runtime_error("fixture after actual mutation");
 return result;
}
namespace {
void in_flight(const char* path,Edge where){
 auto s=service(path);load(s,0xabcdef01);load(s,0x12345678);Gate g;
 std::thread worker([&]{edge=where;gate=&g;BankServiceFailure f;check(s.unload(command(AudioBankOpcode::unload,0xabcdef01),f),"actual unload completes");});
 check(g.reached.get_future().wait_for(std::chrono::seconds(10))==std::future_status::ready,"actual Service unload gap");
 BankServiceFailure f;PreparationFailure pf;const auto play=command(AudioBankOpcode::play,0xabcdef01);
 check(!s.admit_deferred_program(play,pf,f)&&f.kind==BankServiceError::bank_unloading,"new actual work excluded throughout unload");
 check(!s.prepare_program(play,pf,f)&&f.kind==BankServiceError::bank_unloading,"new sync preparation excluded throughout unload");
 check(!s.resolve_play_kind1(play,0,f)&&f.kind==BankServiceError::bank_unloading,"new resource resolution excluded throughout unload");
 check(!s.unload(command(AudioBankOpcode::unload,0xabcdef01),f)&&f.kind==BankServiceError::bank_unloading,"duplicate unload excluded");
 check(!s.close_when_idle(f)&&f.kind==BankServiceError::worker_active,"close retains actual unload operation");
 auto other=s.admit_deferred_program(command(AudioBankOpcode::play,0x12345678),pf,f);check(bool(other),"unrelated actual bank remains admitted");
 check(other->cancel()==ProgramWorkCancellation::queued_removed&&other->retire(pf),"unrelated reservation retires");
 g.release.set_value();worker.join();
 check(!s.admit_deferred_program(play,pf,f)&&f.kind==BankServiceError::unloaded_bank_token,"completed unload revokes exact token");
 check(s.close_when_idle(f),"actual unload activity drained");
}
void rejected(const char* path,Edge mode){
 auto s=service(path);load(s,0x89ab1234);BankServiceFailure f;PreparationFailure pf;edge=mode;
 check(!s.unload(command(AudioBankOpcode::unload,0x89ab1234),f),"injected mutation outcome rejects");edge=Edge::none;
 const auto play=command(AudioBankOpcode::play,0x89ab1234);
 if(mode==Edge::refuse){
  check(f.kind==BankServiceError::protocol_failed,"known refusal remains named");
  auto work=s.admit_deferred_program(play,pf,f);check(bool(work),"known refusal restores admission");
  check(work->cancel()==ProgramWorkCancellation::queued_removed&&work->retire(pf),"restored reservation can actually retire");
  check(s.unload(command(AudioBankOpcode::unload,0x89ab1234),f),"known refusal allows actual unload retry");
 }else{
  check(f.kind==BankServiceError::native_exception,"unexpected outcome remains named");
  check(!s.admit_deferred_program(play,pf,f)&&f.kind==BankServiceError::bank_state_unknown,"unknown mutation never silently reopens admission");
  check(!s.prepare_program(play,pf,f)&&f.kind==BankServiceError::bank_state_unknown,"unknown mutation rejects sync preparation");
  check(!s.resolve_play_kind1(play,0,f)&&f.kind==BankServiceError::bank_state_unknown,"unknown mutation rejects resource resolution");
  check(!s.unload(command(AudioBankOpcode::unload,0x89ab1234),f)&&f.kind==BankServiceError::bank_state_unknown,"unknown mutation never retries guessed unload");
 }
 check(s.close_when_idle(f),"explicit close releases whole Session after mutation outcome");
}
}
int main(int argc,char** argv){
 if(argc!=2)return 2;
 in_flight(argv[1],Edge::before);in_flight(argv[1],Edge::after);
 rejected(argv[1],Edge::refuse);rejected(argv[1],Edge::throw_before);rejected(argv[1],Edge::throw_after);
 std::puts("PASS native Service program admission unload gates/known refusal/unknown mutation; five CPU-only injected boundaries");
}
