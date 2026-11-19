#include "mka/movie_decoder_mode.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <limits>
#include <new>
#include <stdexcept>
#include <thread>
#include <type_traits>

namespace { int fail_after=-1; }
void* operator new(std::size_t n){
    if(fail_after==0){fail_after=-1;throw std::bad_alloc();}
    if(fail_after>0)--fail_after;
    if(void* p=std::malloc(n?n:1))return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
namespace {
using namespace mka::video;
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
MovieDecoderModeCapture snapshot(NativeMovieDecoderMode& owner){MovieDecoderModeFailure f;auto s=owner.capture_dispatch(f);check(s.has_value()&&f.kind==MovieDecoderModeError::none,"owned dispatch capture");return *s;}
}
int main(){try{
    static_assert(!std::is_default_constructible_v<NativeMovieDecoderMode>);
    static_assert(!std::is_copy_constructible_v<NativeMovieDecoderMode>);
    static_assert(!std::is_default_constructible_v<MovieDecoderModeCapture>);
    MovieDecoderModeFailure f;auto owner=NativeMovieDecoderMode::activate_native_movie_content(f);
    check(owner&&f.kind==MovieDecoderModeError::none,"explicit native content activation");
    const auto initial=snapshot(*owner);
    check(initial.source_word_bits()==0&&initial.write_generation()==0&&initial.route()==MovieDecoderPlaneRoute::preserve_source_planes,"compiled source initializer, not normal first-play admission");
    check(owner->assign_source_word_bits(0,f)&&snapshot(*owner).write_generation()==0,"equal-value setter preserves value generation");
    std::uint64_t generation=0;std::uint32_t prior=0;
    for(const auto word:std::array<std::uint32_t,10>{1,2,0x80000000u,0xffffffffu,0,0,17,17,0x7fffffffu,0}){
        check(owner->assign_source_word_bits(word,f)&&f.kind==MovieDecoderModeError::none,"all source-word bit patterns admitted");
        if(word!=prior)++generation;prior=word;
        const auto s=snapshot(*owner);
        check(s.source_word_bits()==word&&s.write_generation()==generation&&
            s.route()==(word?MovieDecoderPlaneRoute::downmix_duplicate:MovieDecoderPlaneRoute::preserve_source_planes),"exact source zero/nonzero dispatch and mutable generation");
    }
    check(owner->assign_source_word_bits(1,f),"dispatch batch route capture");
    const auto earlier_batch=snapshot(*owner);check(owner->assign_source_word_bits(0,f),"later shared setting update");
    check(earlier_batch.route()==MovieDecoderPlaneRoute::downmix_duplicate&&snapshot(*owner).route()==MovieDecoderPlaneRoute::preserve_source_planes,"earlier dispatch immutable, no packet-boundary resample");
    auto current=owner->is_current(earlier_batch,f);check(current.has_value()&&!*current&&f.kind==MovieDecoderModeError::none,"old same-epoch capture remains valid data");
    const auto before=snapshot(*owner);
    for(auto bad:std::array<std::uint64_t,2>{0x100000000ull,std::numeric_limits<std::uint64_t>::max()}){
        check(!owner->assign_source_word_bits(bad,f)&&f.kind==MovieDecoderModeError::source_word_range,"no silent native64 source-word truncation");
        check(snapshot(*owner).source_word_bits()==before.source_word_bits()&&snapshot(*owner).write_generation()==before.write_generation(),"range rejection is atomic");
    }
    bool thread_passed=false;std::exception_ptr thread_failure;
    std::thread wrong([&]{try{MovieDecoderModeFailure tf;check(!owner->capture_dispatch(tf)&&tf.kind==MovieDecoderModeError::wrong_thread,"cross-thread capture rejected");check(!owner->assign_source_word_bits(2,tf)&&tf.kind==MovieDecoderModeError::wrong_thread,"cross-thread setter rejected");check(!owner->retire_content(tf)&&tf.kind==MovieDecoderModeError::wrong_thread,"cross-thread replacement rejected");thread_passed=true;}catch(...){thread_failure=std::current_exception();}});wrong.join();
    if(thread_failure)std::rethrow_exception(thread_failure);
    check(thread_passed&&snapshot(*owner).write_generation()==before.write_generation(),"thread rejection retains owner state");
    for(int stage=0;stage<2;++stage){fail_after=stage;auto failed=NativeMovieDecoderMode::activate_native_movie_content(f);fail_after=-1;check(!failed&&f.kind==MovieDecoderModeError::allocation,"each native activation allocation fails named");check(snapshot(*owner).write_generation()==before.write_generation(),"failed new epoch does not reset existing players");}
    auto fresh=NativeMovieDecoderMode::activate_native_movie_content(f);check(bool(fresh),"distinct newly activated content");
    check(!fresh->is_current(before,f)&&f.kind==MovieDecoderModeError::wrong_epoch,"different active epochs reject mixed identity");
    check(owner->retire_content(f)&&owner->retire_content(f),"explicit content replacement is idempotent");
    check(!owner->capture_dispatch(f)&&f.kind==MovieDecoderModeError::inactive_epoch,"retired epoch cannot produce new dispatch");
    check(!owner->assign_source_word_bits(0,f)&&f.kind==MovieDecoderModeError::inactive_epoch,"retired epoch cannot mutate");
    check(!owner->is_current(before,f)&&f.kind==MovieDecoderModeError::inactive_epoch,"retired owner is not active ingress");
    owner.reset();check(earlier_batch.source_word_bits()==1&&earlier_batch.route()==MovieDecoderPlaneRoute::downmix_duplicate,"owned capture survives original module owner");
    check(snapshot(*fresh).source_word_bits()==0&&snapshot(*fresh).write_generation()==0,"actual reactivation restores compiled initializer only");
    std::puts("PASS native mutable decoder mode content epoch/capture; caller and processing chronology remain unbound");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"[decoder-mode] %s\n",e.what());return 1;}}
