#include "mka/program_copy.hpp"
#include <cstdio>
#include <cstdlib>
#include <new>
using namespace mka::sound;
namespace {
thread_local std::int64_t fail_after=-1;
void check(bool ok,const char* why){if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
RuntimeCacheObservation record(PreparedBank& bank){PreparationFailure f;auto r=bank.runtime_cache(f);check(r&&!r->empty(),"actual runtime record");return (*r)[0];}
}
extern "C" void* real_new(std::size_t) __asm__("__real__Znwy");
extern "C" void* wrapped_new(std::size_t) __asm__("__wrap__Znwy");
extern "C" void* wrapped_new(std::size_t size){
 if(fail_after==0)throw std::bad_alloc();
 if(fail_after>0)--fail_after;
 return real_new(size);
}
int main(int argc,char** argv){
 if(argc!=2)return 2;
 ArchiveFailure af;auto archive=SoundArchive::open(argv[1],af);check(bool(archive),"actual archive");
 auto source=archive->read_bank("announcr",af);check(bool(source),"actual ANNOUNCR");
 BankPreparationReport report;PreparationFailure pf;auto bank=PreparedBank::prepare(std::move(*source),report,pf);check(bool(bank),"actual bank");
 unsigned rejected=0;bool success=false;
 for(unsigned n=0;n<64&&!success;++n){
  auto ref=bank->acquire_runtime_program(0,pf);check(ref&&record(*bank).references==1,"actual semantic reference before injection");
  ProgramCopyFailure f;fail_after=n;auto copy=NativeProgramCopy::clone(ref,f);fail_after=-1;
  if(!copy){
   check(f.kind==ProgramCopyError::allocation_failed,"actual allocation error named");
   check(ref&&ref->active()&&record(*bank).references==1,"every rejected allocation preserves caller reference");
   check(bank->acquire_runtime_program(0,pf)!=nullptr,"failed copy leaves actual bank usable");
   ++rejected;
  }else{
   check(!ref&&record(*bank).references==1,"successful copy alone owns semantic reference");
   copy.reset();success=true;
  }
  ref.reset();check(record(*bank).references==0&&!record(*bank).cached,"cleanup leaves no semantic/cache acquisition");
 }
 check(success&&rejected>=4,"all allocating prefix sites reached before successful copy");
 auto ref=bank->acquire_runtime_program(0,pf);ProgramCopyFailure f;auto copy=NativeProgramCopy::clone(ref,f);check(bool(copy),"actual retry after all fault sites");
 auto schedule=copy->prepare_ordinary_schedule({1000},0,f);check(bool(schedule),"actual independent schedule plan");
 ProgramFailure ef;auto executor=ProgramExecutor::create(schedule->data,schedule->deadline,ef);check(bool(executor),"actual executor");
 auto start=executor->step(schedule->deadline,ef);check(start&&start->kind==ProgramStepKind::sample_start,"actual source start step");
 const auto initial=copy->instance(0,f);check(initial&&!initial->output_bound&&initial->state.gain_q14==0x4000,"donor state before native start failure");
 NormalOutputParameters parameters{{0x4000,0,0x1000,0x4000,0,0x1000},{0x4000,0,0x1000,0x4000,0,0x1000},{{1,32767,false},{1,-1,false},{1,0,false},{1,-1000,false},20000},0,0};
 check(!copy->start_output(*start,parameters,f)&&f.kind==ProgramCopyError::voice_failed&&f.voice.kind==HostVoiceError::invalid_output_rate,"actual native voice factory failure");
 auto staged=copy->instance(0,f);check(staged&&!staged->output_bound&&staged->state.gain_q14==16383,"source operands remain staged without fake output");
 parameters.host_output_rate=48000;check(copy->start_output(*start,parameters,f),"actual output retry with explicit valid rate");
 check(copy->instance(0,f)->output_bound,"only actual output construction binds voice");
 copy.reset();check(record(*bank).references==0&&!record(*bank).cached,"native outputs cleaned before final semantic release");
 std::printf("PASS native program copy allocation rollback: %u failure sites; actual voice failure/retry; test-only allocator wrap\n",rejected);
}
