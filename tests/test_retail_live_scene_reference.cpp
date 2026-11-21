#include "mka/retail_live_scene_reference.hpp"
#include "mka/retail_game_session.hpp"

#include <atomic>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <thread>

using namespace mka::retail;
namespace {
void check(bool okay,const char* label) {
    if(!okay) throw std::runtime_error(label);
}
class FixtureSceneObject final : public NativeRetailSceneObject {
public:
    FixtureSceneObject(const NativeRetailGameSession& owner,std::uint32_t cookie) noexcept
        : owner_(&owner), cookie_(cookie) {}
    std::uint32_t source_cookie() const noexcept override {
        return cookie_.load(std::memory_order_acquire);
    }
    void cleanup_from_12fa00() override {
        ++cleanups_;
        cookie_.store(0,std::memory_order_release);
        live_.store(false,std::memory_order_release);
    }
    bool scene_identity_live() const noexcept override {
        return live_.load(std::memory_order_acquire);
    }
    const NativeRetailGameSession* owner_session() const noexcept override {
        ++owner_reads_;
        return owner_;
    }
    void change_cookie(std::uint32_t value) noexcept {
        cookie_.store(value,std::memory_order_release);
    }
    int cleanups() const noexcept { return cleanups_; }
    unsigned owner_reads() const noexcept { return owner_reads_; }
private:
    const NativeRetailGameSession* owner_;
    std::atomic<std::uint32_t> cookie_;
    std::atomic<bool> live_{true};
    int cleanups_{};
    mutable unsigned owner_reads_{};
};
struct Fixture {
    std::shared_ptr<NativeRetailGameSession> session;
    NativeRetailSceneReference* reference{};
    Fixture() {
        RetailGameSessionFailure sf;
        session=NativeRetailGameSession::create(sf);
        check(bool(session),"actual retained game Session");
        reference=session->live_scene_reference(sf);
        check(reference&&sf.kind==RetailGameSessionError::none,
              "scene reference borrowed from actual Session");
    }
};
}

int main() {
    try {
        Fixture fixture;
        RetailLiveSceneFailure f;
        auto value=fixture.reference->lookup_from_2f18f0(f);
        check(value&&!*value&&f.kind==RetailLiveSceneError::none,
              "BSS-born pair returns source null");
        auto outcome=fixture.reference->publish_from_2f1920({},f);
        check(outcome&&*outcome==RetailLiveScenePublishOutcome::null_no_effect,
              "null publisher is source no-op");
        auto state=fixture.reference->observe(f);
        check(state&&!state->reference_published&&state->cached_cookie==0,
              "null did not clear or publish pair");

        auto zero=std::make_shared<FixtureSceneObject>(*fixture.session,0);
        outcome=fixture.reference->publish_from_2f1920(zero,f);
        value=fixture.reference->lookup_from_2f18f0(f);
        check(outcome&&*outcome==RetailLiveScenePublishOutcome::published&&
              value&&value->get()==zero.get(),
              "matching cookie zero remains valid");
        zero->cleanup_from_12fa00();
        value=fixture.reference->lookup_from_2f18f0(f);
        state=fixture.reference->observe(f);
        check(value&&!*value&&state&&state->reference_published&&
              state->object_retained&&!state->object_live&&!state->valid&&
              state->cached_cookie==0&&state->current_cookie==0,
              "logical retirement hides retained cookie-zero object without clearing pair");
        const auto owner_reads_before=zero->owner_reads();
        outcome=fixture.reference->publish_from_2f1920(zero,f);
        const bool retired_input_rejected=!outcome&&
            f.kind==RetailLiveSceneError::retired_object;
        state=fixture.reference->observe(f);
        check(retired_input_rejected&&zero->owner_reads()==owner_reads_before&&
              state&&state->reference_published&&state->cached_cookie==0,
              "retired incoming object refused even when old pair is stale");

        auto first=std::make_shared<FixtureSceneObject>(*fixture.session,0);
        outcome=fixture.reference->publish_from_2f1920(first,f);
        check(outcome&&*outcome==RetailLiveScenePublishOutcome::published,
              "new live cookie-zero object replaces logically retired owner");
        first->change_cookie(1);
        value=fixture.reference->lookup_from_2f18f0(f);
        state=fixture.reference->observe(f);
        check(value&&!*value&&state&&state->reference_published&&
              state->object_retained&&state->cached_cookie==0&&
              state->current_cookie==1&&!state->valid,
              "stale getter returns null without clearing pair");
        outcome=fixture.reference->publish_from_2f1ad0(first,f);
        value=fixture.reference->lookup_from_2f18f0(f);
        check(outcome&&*outcome==RetailLiveScenePublishOutcome::published&&
              value&&value->get()==first.get(),
              "same object republishes cookie one");

        auto second=std::make_shared<FixtureSceneObject>(*fixture.session,7);
        outcome=fixture.reference->publish_from_2f1920(second,f);
        value=fixture.reference->lookup_from_2f18f0(f);
        check(outcome&&*outcome==
                  RetailLiveScenePublishOutcome::refused_valid_different&&
              value&&value->get()==first.get(),
              "different valid current object blocks replacement");
        first->cleanup_from_12fa00();
        value=fixture.reference->lookup_from_2f18f0(f);
        check(value&&!*value&&first->cleanups()==1,
              "actual cookie mutation makes reference stale without cleanup replay");
        outcome=fixture.reference->publish_from_2f1ad0({},f);
        state=fixture.reference->observe(f);
        check(outcome&&*outcome==RetailLiveScenePublishOutcome::null_no_effect&&
              state&&state->reference_published&&state->cached_cookie==1,
              "null publisher preserves stale reference and cached cookie");
        outcome=fixture.reference->publish_from_2f1ad0(second,f);
        value=fixture.reference->lookup_from_2f18f0(f);
        check(outcome&&*outcome==RetailLiveScenePublishOutcome::published&&
              value&&value->get()==second.get(),
              "stale current reference admits a real replacement");

        Fixture foreign;
        auto foreign_object=std::make_shared<FixtureSceneObject>(*foreign.session,8);
        outcome=fixture.reference->publish_from_2f1920(foreign_object,f);
        const bool foreign_rejected=!outcome&&
            f.kind==RetailLiveSceneError::foreign_object;
        value=fixture.reference->lookup_from_2f18f0(f);
        check(foreign_rejected&&value&&value->get()==second.get(),
              "foreign Session object refused without changing live reference");

        bool wrong_thread=false;
        std::thread thread([&] {
            RetailLiveSceneFailure tf;
            auto from_thread=fixture.reference->lookup_from_2f18f0(tf);
            const bool lookup_refused=!from_thread&&
                tf.kind==RetailLiveSceneError::wrong_thread;
            auto publication=fixture.reference->publish_from_2f1920(first,tf);
            wrong_thread=lookup_refused&&!publication&&
                tf.kind==RetailLiveSceneError::wrong_thread;
        });
        thread.join();
        check(wrong_thread,"foreign thread refused before object access");

        check(fixture.reference->reset_from_2f1750(f),
              "explicit 2F1750 source reset");
        state=fixture.reference->observe(f);
        check(state&&!state->reference_published&&state->cached_cookie==0&&
              !state->object_retained,"reset clears pointer then cached cookie");
        {
            auto temporary=std::make_shared<FixtureSceneObject>(*fixture.session,9);
            outcome=fixture.reference->publish_from_2f1920(temporary,f);
            check(outcome&&*outcome==RetailLiveScenePublishOutcome::published,
                  "real producer publishes temporary owner");
        }
        state=fixture.reference->observe(f);
        value=fixture.reference->lookup_from_2f18f0(f);
        check(state&&state->reference_published&&!state->object_retained&&
              value&&!*value,"weak scene reference does not prolong producer lifetime");
        check(fixture.reference->reset_from_2f1ad0(f),
              "conditional 2F1AD0 reset explicitly clears pair");
        return 0;
    } catch(const std::exception& error) {
        std::fprintf(stderr,"retail live scene reference: %s\n",error.what());
        return 1;
    }
}
