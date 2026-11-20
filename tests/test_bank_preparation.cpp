#include "mka/bank_preparation.hpp"
#include "mka/audio_banks.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <new>
using namespace mka::sound;
namespace { bool fail_next_allocation=false; }
void* operator new(std::size_t n){if(fail_next_allocation){fail_next_allocation=false;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}

namespace {
void check(bool ok,const char* s){if(!ok){std::fprintf(stderr,"FAIL: %s\n",s);std::exit(1);}}
void put(std::vector<std::uint8_t>& b,std::size_t o,std::uint32_t v,unsigned n=4){for(unsigned i=0;i<n;++i)b[o+i]=v>>(8*i);}
CommandRecord command(std::uint32_t key,std::uint8_t kind=1){CommandRecord c;for(unsigned i=0;i<4;++i)c.bytes[i]=key>>(8*i);c.bytes[8]=kind;c.bytes[10]=0xfd;c.bytes[11]=0xff;return c;}
std::vector<std::uint8_t> bank(const std::vector<std::uint32_t>& flags,const std::vector<std::vector<CommandRecord>>& cmds,bool alternate=false,bool bad_extent=false){
 auto n=flags.size();std::size_t count=0;for(const auto& c:cmds)count+=c.size();std::size_t ro=48,po=84,ao=po+16*n,co=ao+16*n,so=co+count*40;std::vector<std::uint8_t>b(so+2*n);
 put(b,0,11);put(b,4,0x13);put(b,8,1,2);put(b,10,n,2);put(b,12,n,2);put(b,16,ro);put(b,20,po);put(b,24,ao);put(b,28,co);put(b,32,so);
 put(b,ro,0xdeadbeef);put(b,ro+4,0xdeadbeef);put(b,ro+8,0x20000002);put(b,ro+12,22050);put(b,ro+16,bad_extent?4096:256);put(b,ro+20,36);put(b,ro+32,alternate?1:0);
 std::size_t at=co;for(std::size_t i=0;i<n;++i){put(b,po+i*16+12,flags[i]);put(b,ao+i*16+8,cmds[i].size());put(b,so+i*2,i+1,2);for(const auto& c:cmds[i]){std::copy(c.bytes.begin(),c.bytes.end(),b.begin()+at);at+=40;}}
 return b;
}
BankAssetSource source(const std::shared_ptr<SoundArchiveBacking>& backing,std::vector<std::uint8_t> bytes){auto size=bytes.size();return {{1,0,size,"synthetic.mst"},std::move(bytes),backing};}
}
int main(int argc,char** argv){
 if(argc!=3){std::fprintf(stderr,"usage: test_bank_preparation <pinnedMS2> <NEW diagnostics dir>\n");return 2;}
 const std::filesystem::path dir(argv[2]);check(std::filesystem::create_directory(dir),"exclusive diagnostics dir");
 std::vector<std::uint8_t>b(292);put(b,260,65536);put(b,264,1);put(b,268,22050);put(b,272,16);b[276]=12;b[277]=7;for(unsigned i=278;i<292;++i)b[i]=0x11;
 {std::ofstream f(dir/"synthetic-source.bin",std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),b.size());check(bool(f),"synthetic backing");}
 ArchiveFailure af;auto backing=SoundArchiveBacking::open(dir/"synthetic-source.bin",af);check(bool(backing),"open host backing");BankPreparationReport report;PreparationFailure f;
 auto prepared=PreparedBank::prepare(source(backing,bank({0},{{command(0x20000002),command(0,7)}})),report,f);
 check(prepared&&report.eager_error_count==0&&!report.resource_precreation_skipped&&report.resource_precreation_attempts==1&&report.programs[0].state==ProgramPreparationState::eager_prepared,"actualeagerpolicy");
 auto program=prepared->eager_program(0,f);check(program&&program->commands.size()==2&&program->normal_instances.size()==1&&program->normal_instances[0].repeat_operand==-3&&program->normal_instances[0].source_preserve_resource_after_instance,"onlykind1prepared allcommandsretained signedrepeat no serialized runtimepointers");
 auto sample=program->normal_instances[0].sample;prepared->unload();check(!prepared->eager_program(0,f)&&f.kind==PreparationError::unloaded&&sample->pcm[0].samples.size()==28,"unloadrevokesnewwork retainsownedPCM");
 auto deferred=PreparedBank::prepare(source(backing,bank({2,0},{{command(0x20000002)},{command(0x20000002)}})),report,f);
 check(deferred&&report.resource_precreation_skipped&&report.resource_precreation_attempts==0&&report.programs[0].state==ProgramPreparationState::source_deferred&&report.programs[1].state==ProgramPreparationState::eager_prepared,"anyrawflag2skipswholeprecreation notonlystreaming");
 check(!deferred->eager_program(0,f)&&f.kind==PreparationError::source_deferred,"deferredisnotemptyready");auto dp=deferred->prepare_deferred_normal(0,f);check(dp&&dp->normal_instances.size()==1,"deferrednormalprovedwork");
 auto alternate=PreparedBank::prepare(source(backing,bank({0},{{command(0x20000002)}},true)),report,f);
 check(alternate&&report.programs[0].serialized_flags==0&&report.programs[0].effective_flags==2&&report.programs[0].state==ProgramPreparationState::source_deferred&&report.eager_error_count==0,"alternatedetectoraddsdeferredwithoutinventingeagerdecode");
 check(!alternate->prepare_deferred_normal(0,f)&&f.kind==PreparationError::unsupported_alternate_preparation,"alternateworkexplicitunsupported");
 auto unused=PreparedBank::prepare(source(backing,bank({0},{{command(0,7)}},false,true)),report,f);
 check(unused&&report.resource_precreation_failures==1&&report.eager_error_count==0,"unusedprecreationfailurealoneisnotloaderror");
 auto failed=PreparedBank::prepare(source(backing,bank({0,0},{{command(0x20000002),command(0x20000099)},{command(0x20000099)}})),report,f);
 check(!failed&&f.kind==PreparationError::eager_program_failures&&report.eager_error_count==2&&report.programs[0].failure.kind==PreparationError::absent_resource&&report.programs[0].failure.command_index==1,"all eagererrorscounted andvalidprefixrolledback");
 // Actual session candidate-failure transaction using an isolated type2 archive.
 auto good_bytes=bank({0},{{command(0x20000002),command(0,7)}});put(good_bytes,64,1024);
 auto bad_bytes=bank({0},{{command(0x20000002),command(0,7)}},false,true);
 std::vector<std::uint8_t>archive_bytes(1100);put(archive_bytes,0,2);put(archive_bytes,4,3);put(archive_bytes,8,54);
 put(archive_bytes,12+4,archive_bytes.size());put(archive_bytes,24,1);put(archive_bytes,28,128);put(archive_bytes,32,good_bytes.size());put(archive_bytes,36,2);put(archive_bytes,40,512);put(archive_bytes,44,bad_bytes.size());
 const char names[]="\0good.mst\0bad.mst\0";std::copy_n(reinterpret_cast<const std::uint8_t*>(names),18,archive_bytes.begin()+48);
 std::copy(good_bytes.begin(),good_bytes.end(),archive_bytes.begin()+128);std::copy(bad_bytes.begin(),bad_bytes.end(),archive_bytes.begin()+512);std::copy(b.begin()+256,b.end(),archive_bytes.begin()+1024);
 {std::ofstream out(dir/"transaction.ms2",std::ios::binary);out.write(reinterpret_cast<const char*>(archive_bytes.data()),archive_bytes.size());check(bool(out),"synthetic archive");}
 AudioBanksFailure txf;auto transaction=AudioBankSession::open(dir/"transaction.ms2",std::nullopt,txf);check(bool(transaction),"open transaction fixture");
 auto good_handle=transaction->load_bank("good",txf),bad_handle=transaction->load_bank("bad",txf);check(good_handle&&bad_handle,"load two host candidates");
 check(!transaction->prepare_bank(*bad_handle,report,f,txf)&&f.kind==PreparationError::eager_program_failures&&report.eager_error_count==1,"failed eager candidate rejected");
 check(!transaction->load_state(*bad_handle,txf)&&txf.kind==AudioBanksError::unloaded_handle&&transaction->load_state(*good_handle,txf)==AudioBankLoadState::loaded_unprepared,"failed prep revokes onlycandidate");
 auto allocation_handle=transaction->load_bank("good",txf);check(bool(allocation_handle),"allocationfailurecandidate");fail_next_allocation=true;
 check(!transaction->prepare_bank(*allocation_handle,report,f,txf)&&!fail_next_allocation&&f.kind==PreparationError::allocation_failed,"real new allocationfailure typed");
 check(!transaction->load_state(*allocation_handle,txf)&&txf.kind==AudioBanksError::unloaded_handle&&transaction->load_state(*good_handle,txf)==AudioBankLoadState::loaded_unprepared,"allocationfailure cannot revoke unrelatedbank");
 auto success=transaction->prepare_bank(*good_handle,report,f,txf);check(bool(success),"remainingcandidate succeeds");auto result=success->eager_program(0,f);check(bool(result),"retained preparedprogram");
 auto copied_preparation=*success;copied_preparation.unload();
 check(!transaction->load_state(*good_handle,txf)&&txf.kind==AudioBanksError::preparation_revoked,"copied preparation unload reflected bysessionstate");
 check(!transaction->prepare_bank(*good_handle,report,f,txf)&&txf.kind==AudioBanksError::preparation_revoked&&f.kind==PreparationError::unloaded,"revoked preparation never returned asidempotentsuccess");
 check(bool(transaction->resolve_kind1(*good_handle,0,0,txf))&&result->normal_instances[0].sample->pcm[0].samples.size()==28,"revocation retains sourcebank andexistingPCM");
 auto lifetime_handle=transaction->load_bank("good",txf);check(bool(lifetime_handle),"separatelifetimecandidate");auto lifetime=transaction->prepare_bank(*lifetime_handle,report,f,txf);check(bool(lifetime),"freshlifetimepreparation");transaction.reset();
 check(!lifetime->eager_program(0,f)&&f.kind==PreparationError::unloaded&&result->normal_instances[0].sample->pcm[0].samples.size()==28,"sessiondestruction revokes newwork yetownedresults survive");
 auto archive=SoundArchive::open(argv[1],af);check(bool(archive),"pinnedactualMS2");std::size_t banks=0,eager=0,deferred_count=0,skipped=0;
 std::ofstream records(dir/"bank-preparation.tsv");records<<"bank\tprograms\teager\tdeferred\tskipped_precreation\tprecreation_failures\n";
 for(const auto& r:archive->directory_records())if(!r.name.empty()){
  auto raw=archive->read_bank(r.name,af);check(bool(raw),"exactrealbankname");auto x=PreparedBank::prepare(std::move(*raw),report,f);
  if(!x){std::fprintf(stderr,"realbank %s failed kind%d eagererrors%zu\n",r.name.c_str(),int(f.kind),report.eager_error_count);for(std::size_t i=0;i<report.programs.size();++i)if(report.programs[i].state==ProgramPreparationState::failed)std::fprintf(stderr,"  program%zu command%zu kind%d payload%d codec%d count2%d\n",i,report.programs[i].failure.command_index,int(report.programs[i].failure.kind),int(report.programs[i].failure.payload.kind),int(report.programs[i].failure.codec),int(report.programs[i].failure.count2.kind));check(false,"realnativebankpreparationmustnotbeomitted");}
  std::size_t e=0,d=0;for(const auto& p:report.programs)if(p.state==ProgramPreparationState::eager_prepared)++e;else if(p.state==ProgramPreparationState::source_deferred)++d;else check(false,"nofailedpublishedbank");
  records<<r.name<<'\t'<<report.programs.size()<<'\t'<<e<<'\t'<<d<<'\t'<<report.resource_precreation_skipped<<'\t'<<report.resource_precreation_failures<<'\n';++banks;eager+=e;deferred_count+=d;skipped+=report.resource_precreation_skipped;
 }
 records.flush();check(bool(records),"flushcorpus record beforelaterchecks");
 check(banks==344&&eager==5500&&deferred_count==4156&&skipped==179,"fullcorpus sourcepolicy counts");
 AudioBanksFailure bf;auto session=AudioBankSession::open(argv[1],std::nullopt,bf);check(bool(session),"sessionopen");auto h=session->load_bank("announcr",bf);check(bool(h),"actualannouncrname");auto owned=session->prepare_bank(*h,report,f,bf);check(bool(owned)&&session->load_state(*h,bf)==AudioBankLoadState::eager_prepared_source_deferred_allowed,"sessionpublishesonlyaftereagererrorzero");check(report.programs.size()==192&&report.programs[0].serialized_flags==2&&report.programs[0].state==ProgramPreparationState::source_deferred,"actualannouncrselector0 is source-deferrednormal");
 check(!owned->eager_program(0,f)&&f.kind==PreparationError::source_deferred,"announcrsourceflag2 never falselyeager");
 auto retained=owned->prepare_deferred_normal(0,f);
 if(!retained)std::fprintf(stderr,"announcrselector0 deferredprepare failed program%zu command%zu kind%d payload%d codec%d count2%d\n",f.program_index,f.command_index,int(f.kind),int(f.payload.kind),int(f.codec),int(f.count2.kind));
 check(retained&&retained->program_index==0&&retained->normal_instances.size()==1&&retained->normal_instances[0].sample->pcm.size()==1&&retained->normal_instances[0].sample->pcm[0].samples.size()==34720,"actualdeferrednormalpreparedprogram exactPCM");
 std::printf("announcr selector0 program%zu effectiveflags%u instances%zu PCM%zu\n",retained->program_index,retained->effective_flags,retained->normal_instances.size(),retained->normal_instances[0].sample->pcm[0].samples.size());
 check(session->unload_bank(*h,bf),"sessionunload");check(!owned->eager_program(0,f)&&f.kind==PreparationError::unloaded&&retained->normal_instances[0].sample->pcm[0].samples.size()==34720,"sessionunloadrevokesbank yetownedresults survive");
 std::printf("PASS %zu actualbanks %zueager %zudeferred %zuskipprecreation; nativepreparation only nocallback/playback\n",banks,eager,deferred_count,skipped);
}
