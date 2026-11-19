#include "mka/program_executor.hpp"
#include <cstdio>
#include <cstdlib>
using namespace mka::sound;
namespace {
void check(bool v,const char* s){if(!v){std::fprintf(stderr,"FAIL: %s\n",s);std::exit(1);}}
void put(CommandRecord& c,std::size_t a,std::uint32_t v,unsigned n=4){for(unsigned i=0;i<n;++i)c.bytes[a+i]=v>>(8*i);}
std::shared_ptr<PreparedProgram> copy(const std::shared_ptr<const PreparedProgram>& p){return std::make_shared<PreparedProgram>(*p);}
}
int main(int argc,char** argv){
 if(argc!=2){std::fprintf(stderr,"usage: test_program_executor <pinnedMS2>\n");return 2;}
 ArchiveFailure af;auto archive=SoundArchive::open(argv[1],af);check(bool(archive),"explicitrealMS2");auto raw=archive->read_bank("announcr",af);check(bool(raw),"actualannouncrname");
 BankPreparationReport report;PreparationFailure pf;auto bank=PreparedBank::prepare(std::move(*raw),report,pf);check(bool(bank),"actualnativebankpreparation");
 auto p=bank->prepare_deferred_normal(0,pf);check(p&&p->commands.size()==2&&p->commands[0].kind()==1&&p->commands[1].kind()==7&&p->commands[1].delay_milliseconds()==1571,"exactactualselector0commandstream");
 ProgramFailure f;auto e=ProgramExecutor::create(p,{1000},f);check(bool(e),"explicitinitialdeadline");
 auto waiting=e->step({999},f);check(waiting&&waiting->kind==ProgramStepKind::waiting,"nothingbeforedeadline");
 auto start=e->step({1000},f);check(start&&start->kind==ProgramStepKind::sample_start&&start->command_index==0&&start->clip&&start->clip->pcm.size()==1&&start->clip->pcm[0].samples.size()==34720,"actualownedreadyclipstartoperation");
 check(start->start.repeat_operand==1&&!start->start.set_descriptor_flag_1&&start->start.set_descriptor_flag_4&&start->start.parameter_44==16383&&start->start.parameter_46==0&&start->start.parameter_48==4096&&start->start.descriptor_word_28==0&&start->start.start_argument==0,"exactactualcommandoperands withoutmixguess");
 check(e->step({100000},f)->kind==ProgramStepKind::awaiting_start_result&&e->command_index()==0,"noadvanceorfakestartbeforeACK");check(e->acknowledge_start(true,f)&&e->deadline().milliseconds==2571&&e->command_index()==1,"nextcommand1571msnotPCMduration");
 check(e->step({2570},f)->kind==ProgramStepKind::waiting,"notearlystop");auto stop=e->step({2571},f);check(stop&&stop->kind==ProgramStepKind::program_stop&&stop->stop_reason==ProgramStopReason::command_kind7&&stop->command_index==1&&stop->owner==p,"actualkind7stopoperation");check(e->step({2572},f)->kind==ProgramStepKind::finished,"stopnotrepeated");
 bank->unload();check(start->clip->pcm[0].samples.size()==34720,"ownedclipoutlivesbank");
 auto rejected=ProgramExecutor::create(p,{0},f);check(rejected&&rejected->step({0},f)->kind==ProgramStepKind::sample_start&&!rejected->acknowledge_start(false,f)&&f.kind==ProgramError::native_start_failed,"actualhoststartfailureisnamed");check(!rejected->step({9999},f)&&f.kind==ProgramError::native_start_failed,"failedstartneveradvanced");auto cleanup=rejected->stop_from_owner(f);check(cleanup&&cleanup->stop_reason==ProgramStopReason::owner_request&&!rejected->step({9999},f)&&f.kind==ProgramError::native_start_failed,"ownercleanupdoesnoterasefailure");
 auto fields=copy(p);put(fields->commands[0],10,0xfffd,2);put(fields->commands[0],32,0xfffe,2);put(fields->commands[0],34,0x8000,2);put(fields->commands[0],36,0x7fff,2);put(fields->commands[0],16,0x87654321);put(fields->commands[0],12,0xfedcba98);auto params=ProgramExecutor::create(fields,{0},f);auto event=params->step({0},f);check(event&&event->start.repeat_operand==-3&&event->start.set_descriptor_flag_1&&event->start.parameter_44==-2&&event->start.parameter_46==-32768&&event->start.parameter_48==32767&&event->start.descriptor_word_28==0x87654321&&event->start.start_argument==0xfedcba98,"signedoperands fullopaquewords");
 for(std::uint8_t kind:{std::uint8_t(0),std::uint8_t(2),std::uint8_t(3),std::uint8_t(4),std::uint8_t(5),std::uint8_t(6),std::uint8_t(8),std::uint8_t(9),std::uint8_t(0xff)}){
  auto q=copy(p);q->commands[1].bytes[8]=kind;put(q->commands[1],24,20);auto x=ProgramExecutor::create(q,{100},f);check(x&&x->step({100},f)->kind==ProgramStepKind::sample_start&&x->acknowledge_start(true,f),"knownstartbeforeunsupportedcontrol");
  if(!(kind&128))check(x->step({119},f)->kind==ProgramStepKind::waiting,"unknownnotexecutedbeforeitsdeadline");
  check(!x->step({120},f)&&f.kind==ProgramError::unsupported_command_kind&&f.command_index==1&&f.command_kind==q->commands[1].kind()&&x->command_index()==1,"unknownkindnamednotnoop");
 }
 auto indefinite=copy(p);put(indefinite->commands[1],24,0xffffffff);auto i=ProgramExecutor::create(indefinite,{0},f);check(i&&i->step({0},f)->kind==ProgramStepKind::sample_start&&i->acknowledge_start(true,f)&&i->deadline().milliseconds==0xffffffffu&&i->step({0xfffffffe},f)->kind==ProgramStepKind::waiting,"negativedelayisFFFFFFFFnotend");check(i->step({0xffffffff},f)->kind==ProgramStepKind::program_stop,"unsignedsourceclockpredicate");
 auto one=copy(p);one->commands.resize(1);auto structural=ProgramExecutor::create(one,{0},f);check(structural&&structural->step({0},f)->kind==ProgramStepKind::sample_start&&structural->acknowledge_start(true,f)&&structural->step({0},f)->kind==ProgramStepKind::structural_end&&structural->step({0},f)->kind==ProgramStepKind::finished,"countenddistinctfromstopcleanup");
 auto late=ProgramExecutor::create(p,{1000},f);auto late_event=late->step({1042},f);check(late_event&&late_event->scheduled.milliseconds==1000&&late_event->observed&&late_event->observed->milliseconds==1042&&late->acknowledge_start(true,f)&&late->deadline().milliseconds==2571,"lateobservationdoesnotreschedulenextdeadlinefromnow");
 auto malformed=copy(p);auto malformed_sample=std::make_shared<PreparedNormal>(*malformed->normal_instances[0].sample);malformed_sample->pcm[0].samples.pop_back();malformed->normal_instances[0].sample=malformed_sample;auto bad_pcm=ProgramExecutor::create(malformed,{0},f);check(bad_pcm&&!bad_pcm->step({0},f)&&f.kind==ProgramError::invalid_prepared_sample,"malformedreadyPCMdoesnotstart");
 auto wrong_key=copy(p);put(wrong_key->commands[0],0,0x30000002);auto alias=ProgramExecutor::create(wrong_key,{0},f);check(alias&&!alias->step({0},f)&&f.kind==ProgramError::invalid_prepared_sample,"fulldifferentresourcekeycannotaliaspreparedclip");
 auto missing=copy(p);missing->normal_instances.clear();auto m=ProgramExecutor::create(missing,{0},f);check(m&&!m->step({0},f)&&f.kind==ProgramError::missing_prepared_sample,"nopreparedsample nosilentstart");
 auto ambiguous=copy(p);ambiguous->normal_instances.push_back(ambiguous->normal_instances[0]);auto a=ProgramExecutor::create(ambiguous,{0},f);check(a&&!a->step({0},f)&&f.kind==ProgramError::ambiguous_prepared_sample,"ambiguousinstancefails");
 auto empty=copy(p);empty->commands.clear();check(!ProgramExecutor::create(empty,{0},f)&&f.kind==ProgramError::empty_program,"emptyprogramnotreadyplayback");check(!ProgramExecutor::create({}, {0}, f)&&f.kind==ProgramError::null_program,"nullprogram");
 auto owner=ProgramExecutor::create(p,{999999},f);auto external=owner->stop_from_owner(f);check(external&&external->kind==ProgramStepKind::program_stop&&external->stop_reason==ProgramStopReason::owner_request&&owner->step({0},f)->kind==ProgramStepKind::finished,"ownerunloadstopwithoutwaitingdeadline");
 auto moved=std::move(*owner);check(!owner->step({0},f)&&f.kind==ProgramError::moved_executor,"movedexecutor");
 std::puts("PASS actualannouncrtimeline/ownedclip/exactoperands/startACK/stop/unsupportedcontrols; no mix/audio execution");
}
