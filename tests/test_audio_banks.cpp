#include "mka/audio_banks.hpp"
#include <cstdio>
#include <cstdlib>
using namespace mka::sound;
namespace {
void check(bool ok, const char* what) { if (!ok) { std::fprintf(stderr,"FAIL: %s\n",what); std::exit(1); } }
std::uint64_t hash(PayloadBytes bytes) {
    std::uint64_t h=14695981039346656037ull;
    for(std::size_t i=0;i<bytes.size;++i) h=(h^bytes.data[i])*1099511628211ull;
    return h;
}
}
int main(int argc,char** argv) {
    if(argc!=3) { std::fprintf(stderr,"usage: test_audio_banks <pinned mslasset.ms2> <explicitly associated mslasset.ms4>\n"); return 2; }
    AudioBanksFailure f;
    auto a=AudioBankSession::open(argv[1],std::filesystem::path(argv[2]),f);
    auto b=AudioBankSession::open(argv[1],std::filesystem::path(argv[2]),f);
    check(a && b,"two separately opened explicitly paired sessions");
    AudioBankHandle unknown;
    check(!a->load_state(unknown,f) && f.kind==AudioBanksError::unknown_handle,"unknown handle");
    check(!a->load_bank("no-such-retail-bank",f) && f.kind==AudioBanksError::archive_failed && f.archive.kind==ArchiveError::absent_bank,"missing name never defaults");
    auto h=a->load_bank("announcr",f); check(bool(h),"real announcr by directory name");
    check(a->load_state(*h,f)==AudioBankLoadState::loaded_unprepared,"parse never ready");
    auto resource=a->resolve_kind1(*h,0,0,f);
    check(resource && resource->bank_origin.name=="announcr.mst" && resource->program_index==0 &&
        resource->command.kind()==1 && resource->command.resource_bits()==0x20000002 &&
        resource->resolved_key==0x20000002 && resource->resource.key_mode==ResourceKeyMode::numeric,
        "real announcr selector0 command0 exact fullkey numeric route");
    check(!b->resolve_kind1(*h,0,0,f) && f.kind==AudioBanksError::foreign_session,"foreign session cannot reuse handle");
    check(!a->resolve_kind1(*h,-1,0,f) && f.kind==AudioBanksError::absent_selector,"invalid selector");
    check(!a->resolve_kind1(*h,0,999999,f) && f.kind==AudioBanksError::command_out_of_bounds,"invalid command ordinal");
    check(!a->resolve_kind1(*h,0,1,f) && f.kind==AudioBanksError::unsupported_command_kind,"stop command not silently treated as sample");
    auto sample=a->read_normal(*h,0,0,f);
    check(sample && sample->payload.encoded_payload().size==19840,"real exact normal payload");
    const auto payload_hash=hash(sample->payload.encoded_payload());
    auto copied=*h; auto moved=std::move(*h);
    check(!a->load_state(*h,f) && f.kind==AudioBanksError::moved_handle,"moved handle named error");
    auto moved_session=std::move(*a);
    check(!a->load_state(moved,f) && f.kind==AudioBanksError::moved_session,"moved session named error");
    check(moved_session.load_state(copied,f)==AudioBankLoadState::loaded_unprepared,"session move retains handle identity");
    check(moved_session.unload_bank(moved,f),"unload active host bank");
    check(!moved_session.load_state(copied,f) && f.kind==AudioBanksError::unloaded_handle,"copied handle revoked on unload");
    check(!moved_session.unload_bank(moved,f) && f.kind==AudioBanksError::unloaded_handle,"repeat unload is not fake success");
    check(hash(sample->payload.encoded_payload())==payload_hash && resource->resolved_key==0x20000002,"returned source survives unload");
    auto stream_bank=moved_session.load_bank("bg_balcony",f); check(bool(stream_bank),"real stream bank name");
    auto sr=moved_session.resolve_kind1(*stream_bank,1,0,f);
    check(sr && sr->resolved_key==0x2000208a,"real stream full resource key");
    auto first=moved_session.read_stream_group(*stream_bank,1,0,0,f);
    auto last=moved_session.read_stream_group(*stream_bank,1,0,41,f);
    check(first && last && first->metadata().group_count==42 && last->range0().size==15648 && last->range1().size==15648 &&
        last->metadata().tail_control==15536 && hash(first->raw_group())==0xe87bfe61200144f2ull && hash(last->raw_group())==0x82db892c8e123881ull,
        "pinned raw first/final groups full S never T-trimmed");
    check(!moved_session.read_stream_group(*stream_bank,1,0,42,f) && f.kind==AudioBanksError::stream_read_failed &&
        f.stream.kind==StreamError::group_out_of_bounds,"onepast resource group rejected");
    check(moved_session.close(f),"close session");
    check(!moved_session.load_state(*stream_bank,f) && f.kind==AudioBanksError::closed_session,"closed session named error");
    check(hash(last->raw_group())==0x82db892c8e123881ull && hash(sample->payload.encoded_payload())==payload_hash,"owned results survive close");
    AudioBankHandle expired;
    { auto temporary=AudioBankSession::open(argv[1],std::nullopt,f);check(bool(temporary),"normal-only explicit session");
      auto handle=temporary->load_bank("announcr",f);check(bool(handle),"temporary bank");expired=*handle; }
    check(!b->load_state(expired,f) && f.kind==AudioBanksError::expired_session,"destroyed session handle cannot alias");
    auto normal_only=AudioBankSession::open(argv[1],std::nullopt,f);check(bool(normal_only),"session without associated MS4");
    auto sh=normal_only->load_bank("bg_balcony",f);check(bool(sh),"normal-only context parsed stream bank without readiness");
    check(!normal_only->read_stream_group(*sh,1,0,0,f) && f.kind==AudioBanksError::no_associated_ms4,"missing explicit MS4 association never falls back");
    std::puts("PASS audio bank host ownership, exact real lookup and retained source bytes; no preparation/playback claim");
}
