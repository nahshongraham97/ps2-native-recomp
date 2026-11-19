#include "mka/audio_bank_protocol.hpp"
#include <cstdio>
#include <cstdlib>
using namespace mka::sound;
namespace {
void check(bool ok,const char* message){if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
void put(std::vector<std::uint8_t>& b,std::size_t off,std::uint32_t value){for(unsigned i=0;i<4;++i)b[off+i]=static_cast<std::uint8_t>(value>>(8*i));}
std::vector<std::uint8_t> frame(std::uint32_t op,std::size_t payload_words,std::uint32_t callback=0xfedc1234){
 std::vector<std::uint8_t>b((payload_words+2)*4);put(b,0,payload_words+2);put(b,4,callback);put(b,8,op);return b;
}
std::vector<std::uint8_t> load(std::uint32_t op,const char* name){
 std::string text(name);std::size_t start=op==0x3c?16:8;auto b=frame(op,(start+text.size()+1+3)/4);
 put(b,12,0x89abcdef);if(op==0x3c){put(b,16,0x10203040);put(b,20,0xaabbccdd);}
 for(std::size_t i=0;i<text.size();++i)b[8+start+i]=static_cast<std::uint8_t>(text[i]);return b;
}
}
int main(int argc,char** argv){
 AudioProtocolFailure f;
 auto a=load(0x3c,"announcr.msb");auto d=decode_audio_bank_frame(a.data(),a.size(),f);
 check(d && d->opcode==AudioBankOpcode::load_async && d->callback_token==0xfedc1234 && d->context_word==0x89abcdef &&
 d->opaque_load_argument==0x10203040 && d->opaque_load_tag==0xaabbccdd && d->pathname=="announcr.msb" && d->payload.size()+8==a.size(),"async exact fields and full callbacktoken");
 auto sync=load(0x14,"announcr");auto sd=decode_audio_bank_frame(sync.data(),sync.size(),f);check(sd && sd->pathname=="announcr" && sd->context_word==0x89abcdef,"sync path at+8 distinct fromasync+10");
 auto play=frame(0x36,8);put(play,12,0x89ab1234);put(play,16,0xffffffff);put(play,36,0xaabbccdd);
 auto pd=decode_audio_bank_frame(play.data(),play.size(),f);check(pd && pd->bank_token.value==0x89ab1234 && pd->selector==-1 && pd->payload[28]==0xdd,"full banktoken signedselector opaqueplaywords");
 auto extended=frame(0x6a,10);put(extended,12,0x34cd1234);put(extended,16,0x80000000);auto ed=decode_audio_bank_frame(extended.data(),extended.size(),f);check(ed && ed->selector==(-2147483647-1) && ed->payload.size()==40,"extendedexacttenwords signedminimum");
 auto joined=a;joined.insert(joined.end(),play.begin(),play.end());auto first=decode_audio_bank_frame(joined.data(),joined.size(),f);check(first && first->frame_bytes==a.size(),"oneframeconsumption notnextframe");
 check(!decode_audio_bank_frame(nullptr,99,f) && f.kind==AudioProtocolError::null_input,"null buffer");
 check(!decode_audio_bank_frame(a.data(),7,f) && f.kind==AudioProtocolError::truncated_frame,"shortheader");
 auto bad=a;put(bad,0,2);check(!decode_audio_bank_frame(bad.data(),bad.size(),f)&&f.kind==AudioProtocolError::invalid_frame_size,"countmustincludeopcode");
 bad=a;put(bad,0,0xffffffff);check(!decode_audio_bank_frame(bad.data(),bad.size(),f)&&f.kind==AudioProtocolError::truncated_frame,"hugecount nooverflow");
 bad=frame(0x777,1);check(!decode_audio_bank_frame(bad.data(),bad.size(),f)&&f.kind==AudioProtocolError::unknown_opcode,"unknownopcode noack");
 bad=frame(0x36,7);check(!decode_audio_bank_frame(bad.data(),bad.size(),f)&&f.kind==AudioProtocolError::invalid_command_size,"shortplay");
 bad=frame(0x6a,11);check(!decode_audio_bank_frame(bad.data(),bad.size(),f)&&f.kind==AudioProtocolError::invalid_command_size,"extendedsurplusunsupported");
 bad=frame(0x15,3);check(!decode_audio_bank_frame(bad.data(),bad.size(),f)&&f.kind==AudioProtocolError::invalid_command_size,"unloadexacttwo");
 bad=frame(0x3c,5);for(std::size_t i=24;i<bad.size();++i)bad[i]='x';check(!decode_audio_bank_frame(bad.data(),bad.size(),f)&&f.kind==AudioProtocolError::unterminated_path,"pathmustterminatewithinframe");
 bad=frame(0x3c,5);check(!decode_audio_bank_frame(bad.data(),bad.size(),f)&&f.kind==AudioProtocolError::empty_path,"emptypath noalias");
 if(argc!=2){std::fprintf(stderr,"usage: test_audio_bank_protocol <pinned mslasset.ms2>\n");return 2;}
 AudioBanksFailure bf;auto session=AudioBankSession::open(argv[1],std::nullopt,bf);check(bool(session),"real session");auto bridge=AudioBankProtocol::create(std::move(*session),f);check(bool(bridge),"own session");
 auto loaded=bridge->load_unprepared(*d,f);check(loaded && loaded->request.callback_token==0xfedc1234,"parsedunpreparedload retainsrequest no callback");
 check(bridge->bind_unprepared({0x89ab1234},loaded->handle,f),"explicit full external token binding");
 check(!bridge->bind_unprepared({0x89ab1234},loaded->handle,f)&&f.kind==AudioProtocolError::duplicate_bank_token,"no implicit rebinding");
 check(!bridge->bind_unprepared({0},loaded->handle,f)&&f.kind==AudioProtocolError::unsupported_null_bank_token,"null bank token explicitly unsupported hostroute");
 check(!bridge->lookup({0x1234},f)&&f.kind==AudioProtocolError::unknown_bank_token,"low16 never aliases");
 auto otherFrame=load(0x14,"bg_balcony");auto otherRequest=decode_audio_bank_frame(otherFrame.data(),otherFrame.size(),f);auto other=bridge->load_unprepared(*otherRequest,f);check(bool(other),"secondactualbank");check(bridge->bind_unprepared({0x34cd1234},other->handle,f),"same low16 distinct fulltoken");
 pd->selector=0;auto resource=bridge->resolve_play_kind1(*pd,0,f);check(resource&&resource->bank_origin.name=="announcr.mst"&&resource->resolved_key==0x20000002,"wirebank exactnormalname/selectorkey");
 ed->selector=1;auto stream=bridge->resolve_play_kind1(*ed,0,f);check(stream&&stream->bank_origin.name=="bg_balcony.mst"&&stream->resolved_key==0x2000208a,"same low16 resolvesdifferentexplicitbank");
 auto u=frame(0x15,2);put(u,12,0x89ab1234);auto ud=decode_audio_bank_frame(u.data(),u.size(),f);check(ud&&bridge->unload(*ud,f),"decodedunload exactfullbank");
 check(!bridge->lookup({0x89ab1234},f)&&f.kind==AudioProtocolError::bank_failure&&f.bank.kind==AudioBanksError::unloaded_handle,"revokedbindingfailsnamed");check(bool(bridge->lookup({0x34cd1234},f)),"otherfulltokenstilllive");
 auto missing=load(0x14,"no-such-retail-bank");auto md=decode_audio_bank_frame(missing.data(),missing.size(),f);check(!bridge->load_unprepared(*md,f)&&f.kind==AudioProtocolError::bank_failure&&f.bank.archive.kind==ArchiveError::absent_bank,"unknown real name does not default");
 auto permanentFrame=load(0x14,"permanent");auto permanentRequest=decode_audio_bank_frame(permanentFrame.data(),permanentFrame.size(),f);auto permanent=bridge->load_unprepared(*permanentRequest,f);check(bool(permanent),"actual permanent.mst is looked up as a real name, not banned or aliased");check(bridge->discard_unprepared(permanent->handle,f),"cleanup actual permanent bank");
 auto moved=std::move(*bridge);check(!bridge->lookup({0x34cd1234},f)&&f.kind==AudioProtocolError::moved_bridge,"moved bridge failsnamed");check(moved.discard_unprepared(other->handle,f),"cleanupunprepared withoutcallback");
 std::puts("PASS verified audio bank framing and explicit host token binding; no preparation/completion/playback");
}
