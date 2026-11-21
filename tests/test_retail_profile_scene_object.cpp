#include "mka/retail_profile_scene_object.hpp"
#include "mka/retail_game_session.hpp"

#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <thread>

namespace {
bool fail_allocation=false;
void check(bool yes,const char* message) { if(!yes) throw std::runtime_error(message); }
}
void* operator new(std::size_t n) {
    if(fail_allocation) throw std::bad_alloc();
    if(auto* p=std::malloc(n?n:1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }

int main() {
    try {
        using namespace mka::retail;
        RetailGameSessionFailure sf;
        auto session=NativeRetailGameSession::create(sf);
        check(bool(session),"real retained Session");
        RetailProfileSceneValues initial;
        initial.lanes={RetailProfileSceneLaneValues{7,44,66},
                       RetailProfileSceneLaneValues{9,55,77}};
        initial.word_60=0;
        RetailProfileSceneObjectFailure f;
        check(!NativeRetailProfileSceneObject::create({},1,initial,f) &&
              f.kind==RetailProfileSceneObjectError::missing_session,"no missing-Session default object");
        fail_allocation=true;
        auto failed=NativeRetailProfileSceneObject::create(session,17,initial,f);
        fail_allocation=false;
        check(!failed && f.kind==RetailProfileSceneObjectError::allocation,
              "native allocation failure cannot publish an object");
        auto object=NativeRetailProfileSceneObject::create(session,17,initial,f);
        check(object && object->owner_session()==session.get() && object->source_cookie()==17,
              "actual caller-supplied identity, cookie and initial field values");
        check(object->stage_from_18a260(0,true,f),"source bit-one stage");
        auto values=object->observe(f);
        check(values && values->values.lanes[0].word_2c==2 &&
              values->values.lanes[0].word_24==0 && values->values.lanes[0].word_10==7 &&
              values->values.lanes[1].word_24==55 && values->values.lanes[1].word_2c==77 &&
              values->values.word_60==0xffffffffu,"selected lane only and32-bit zero decrement wrap");
        check(object->stage_from_18a260(1,false,f),"source bit-zero stage");
        values=object->observe(f);
        check(values && values->values.lanes[1].word_2c==1 && values->values.lanes[1].word_24==0 &&
              values->values.word_60==0xfffffffeu,"second lane and repeated source decrement");
        check(!object->stage_from_18a260(2,true,f) && f.kind==RetailProfileSceneObjectError::invalid_role &&
              object->observe(f)->values.word_60==0xfffffffeu,"invalid native role does not mutate fields");
        bool foreign_refused=false;
        std::thread other([&] {
            RetailProfileSceneObjectFailure other_failure;
            foreign_refused=!object->stage_from_18a260(0,false,other_failure) &&
                other_failure.kind==RetailProfileSceneObjectError::wrong_thread;
        });
        other.join();
        check(foreign_refused && object->observe(f)->values.word_60==0xfffffffeu,
              "foreign thread cannot mutate scene fields");
        auto* reference=session->live_scene_reference(sf);
        RetailLiveSceneFailure lf;
        check(reference && reference->publish_from_2f1920(object,lf)==
              RetailLiveScenePublishOutcome::published,"actual nonowning scene publication");
        {
            const auto live=reference->lookup_from_2f18f0(lf);
            check(live && *live==object,"actual Session lookup returns same menu object");
        }
        object->cleanup_from_12fa00();
        check(object->source_cookie()==0 && !object->scene_identity_live() &&
              object->observe(f)->retired,"12DF60 invalidates cookie before logical native release");
        {
            const auto live=reference->lookup_from_2f18f0(lf);
            const auto pair=reference->observe(lf);
            check(live && !*live && pair && pair->reference_published &&
                  pair->cached_cookie==17 && !pair->valid,
                  "released native object is absent without rewriting the cached source pair");
        }
        check(!object->stage_from_18a260(0,false,f) &&
              f.kind==RetailProfileSceneObjectError::retired,
              "retired host object cannot be mutated through a retained reference");
        check(!object->retire_from_12df60(f) && f.kind==RetailProfileSceneObjectError::retired,
              "logical native release cannot replay");
        check(reference->reset_from_2f1750(lf),"explicit source pair reset");
        {
            auto zero=NativeRetailProfileSceneObject::create(session,0,initial,f);
            check(zero && zero->scene_identity_live() &&
                  reference->publish_from_2f1920(zero,lf)==RetailLiveScenePublishOutcome::published,
                  "live zero-cookie object is not rejected by numeric value");
            {
                auto live=reference->lookup_from_2f18f0(lf);
                check(live && *live==zero,"matching live zero-cookie returns actual object");
            }
            check(zero->retire_from_12df60(f),"release object whose source cookie already equals zero");
            const auto live=reference->lookup_from_2f18f0(lf);
            const auto pair=reference->observe(lf);
            check(live && !*live && pair && !pair->valid &&
                  pair->reference_published && pair->cached_cookie==0,
                  "logical retirement is independent of the zero-equals-zero numeric comparison");
        }
        auto orphan=NativeRetailProfileSceneObject::create(session,42,initial,f);
        check(orphan && orphan->scene_identity_live(),
              "live scene retained across its actual Session's destruction");
        std::weak_ptr<NativeRetailGameSession> lifetime=session;
        session.reset();
        check(lifetime.expired(),"scene cannot keep a Session alive through its producer task/list");
        check(!object->scene_identity_live() && !object->observe(f) &&
              f.kind==RetailProfileSceneObjectError::expired_session,
              "retained object cannot access game fields after actual Session lifetime ends");
        check(!orphan->scene_identity_live() && !orphan->observe(f) &&
              f.kind==RetailProfileSceneObjectError::expired_session,
              "previously live orphan refuses field access after Session expiration");
        auto next_session=NativeRetailGameSession::create(sf);
        auto* next_reference=next_session?next_session->live_scene_reference(sf):nullptr;
        check(next_reference,"new real Session reference");
        const auto expired_publication=next_reference->publish_from_2f1920(orphan,lf);
        check(!expired_publication && lf.kind==RetailLiveSceneError::retired_object,
              "expired candidate rejected by lifetime before former owner identity");
        const auto unchanged_pair=next_reference->observe(lf);
        check(unchanged_pair && !unchanged_pair->reference_published &&
              unchanged_pair->cached_cookie==0,
              "expired candidate does not mutate a new Session's source reference");
        check(orphan->retire_from_12df60(f) && orphan->source_cookie()==0,
              "local logical cleanup remains valid after Session expiration");
        object.reset();
        check(lifetime.expired(),"nonowning scene reference creates no Session/object ownership cycle");
        std::puts("PASS native profile scene fields, logical cookie retirement and Session lifetime; real scene producers open");
        return 0;
    } catch(const std::exception& e) {
        fail_allocation=false;
        std::fprintf(stderr,"FAIL native profile scene object: %s\n",e.what());return 1;
    }
}
