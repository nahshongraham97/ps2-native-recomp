#include "mka/retail_main.hpp"
#include "mka/retail_middleware_frame.hpp"
#include "mka/retail_middleware_work.hpp"
#include "native_retail_startup.hpp"
#include "retail_main_control.hpp"
#include <cstdio>
#include <filesystem>
#include <functional>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>
using namespace mka::retail;
namespace {
void check(bool value,const char* text) { if (!value) throw std::runtime_error(text); }
struct Complete final: NativeRetailMiddlewareChild {
    NativeRetailMiddlewareFrame& frame;
    std::vector<int>& events;
    bool retired{};
    Complete(NativeRetailMiddlewareFrame& f,std::vector<int>& e):frame(f),events(e){}
    std::optional<RetailMiddlewarePollStatus> poll(RetailMiddlewareWorkFailure&) override {
        events.push_back(1);return RetailMiddlewarePollStatus::transfer_to_parent;
    }
    bool primary_complete(RetailMiddlewareWorkFailure& f) override {
        events.push_back(2);
        RetailMiddlewareFrameFailure failure;
        if (!frame.install_callback([&events=events](std::uint8_t argument) {
                check(argument==0,"actual main frame callback source argument is zero");
                events.push_back(4);
            },failure)) {
            f.kind=RetailMiddlewareWorkError::child_failed;return false;
        }
        return true;
    }
    std::optional<bool> release_required(RetailMiddlewareWorkFailure&) const override {return true;}
    std::optional<RetailMiddlewareReleaseResult> release(RetailMiddlewareWorkFailure&) override {
        events.push_back(3);retired=true;return RetailMiddlewareReleaseResult::released;
    }
};
struct MutatingCompletion final: NativeRetailMiddlewareChild {
    std::function<void()> mutate;
    explicit MutatingCompletion(std::function<void()> action):mutate(std::move(action)){}
    std::optional<RetailMiddlewarePollStatus> poll(RetailMiddlewareWorkFailure&) override {
        return RetailMiddlewarePollStatus::transfer_to_parent;
    }
    bool primary_complete(RetailMiddlewareWorkFailure&) override {mutate();return true;}
    std::optional<bool> release_required(RetailMiddlewareWorkFailure&) const override {return false;}
    std::optional<RetailMiddlewareReleaseResult> release(RetailMiddlewareWorkFailure&) override {
        throw std::runtime_error("no release requested");
    }
};
}
int main(int argc,char** argv) {try {
    check(argc==2,"existing disposable root");
    mka::content::ContentVolumeFailure volume_failure;
    auto volume=mka::content::NativeContentVolume::open(std::filesystem::path(argv[1]),volume_failure);
    check(bool(volume),"real native volume");
    NativeRetailStartupState startup(std::move(*volume),{});
    mka::input::ControllerInput samples;
    NativeRetailMainState main(startup,samples);
    StartupFailure failure;
    check(!main_binding::pump_retail_middleware(main,failure)&&failure.kind==StartupError::crt,
          "missing native CRT cannot masquerade as successful frame work");
    check(initialize_native_retail_game_session(startup,failure)&&
          initialize_native_retail_crt(startup,failure)&&
          finalize_native_retail_pre_main(startup,failure)&&
          main_binding::construct_retail_middleware_work_group(main,failure)&&
          main_binding::initialize_retail_middleware_files(main,failure)&&startup.middleware_frame,
          "actual native startup births persistent frame owner and same source group/root");
    auto* frame=startup.middleware_frame.get();
    RetailMiddlewareFrameFailure frame_failure;
    check(frame->has_callback(frame_failure)==false&&main_binding::pump_retail_middleware(main,failure),
          "source BSS-empty callback branch uses actual native time and dispatcher");
    std::vector<int> events;
    auto child=std::make_shared<Complete>(*frame,events);
    RetailMiddlewareWorkFailure work_failure;
    check(bool(main.middleware_work_group->append(child,work_failure)),"real source-group child admitted");
    check(main_binding::pump_retail_middleware(main,failure)&&
          events==std::vector<int>({1,2,3,4})&&child->retired,
          "actual main pump polls/completes/releases before fresh callback selection");
    check(frame->install_callback([&main](std::uint8_t value) {
        check(value==0,"reentry callback argument");
        reenter_native_retail_main(main,0);
    },frame_failure),"actual native reentry callback installed");
    bool transferred=false;
    try { (void)main_binding::pump_retail_middleware(main,failure); }
    catch(const detail::NativeMainReentry& transfer) {
        transferred=transfer.owner==&main&&transfer.value==1;
    }
    check(transferred,"REAL native main transfer passes callback/pump and preserves zero-to-one normalization");
    check(frame->install_callback([&main](std::uint8_t) {
        reenter_native_retail_main(main,37);
    },frame_failure),"nonzero native reentry installed");
    transferred=false;
    try { (void)main_binding::pump_retail_middleware(main,failure); }
    catch(const detail::NativeMainReentry& transfer) {
        transferred=transfer.owner==&main&&transfer.value==37;
    }
    check(transferred&&frame->clear_callback(frame_failure),"real nonzero main transfer preserved");
    // Adversarial native completion may mutate the public startup context.
    // Keep originals alive; a pump must not silently switch ownership domain.
    std::unique_ptr<NativeRetailMiddlewareFrame> saved_frame;
    unsigned foreign_callback_calls=0;
    auto replacement=NativeRetailMiddlewareFrame::create(*startup.crt,frame_failure);
    check(replacement&&replacement->install_callback([&](std::uint8_t) {
        ++foreign_callback_calls;
    },frame_failure),"real separate replacement frame with observable callback");
    auto mutator=std::make_shared<MutatingCompletion>([&] {
        saved_frame=std::move(startup.middleware_frame);
        startup.middleware_frame=std::move(replacement);
    });
    check(bool(main.middleware_work_group->append(mutator,work_failure)),"actual frame-replacement completion");
    check(!main_binding::pump_retail_middleware(main,failure)&&
          failure.kind==StartupError::middleware_frame&&foreign_callback_calls==0,
          "actual completion cannot replace persistent frame and run a different callback");
    replacement=std::move(startup.middleware_frame);
    startup.middleware_frame=std::move(saved_frame);
    replacement.reset(); // Before its retained CRT.
    RetailCrtFailure crt_failure;
    auto other_crt=NativeRetailCrt::create(crt_failure);
    check(other_crt&&other_crt->finalize_pre_main(crt_failure),"real replacement CRT finalized");
    std::unique_ptr<NativeRetailCrt> saved_crt;
    auto clock_mutator=std::make_shared<MutatingCompletion>([&] {
        saved_crt=std::move(startup.crt);startup.crt=std::move(other_crt);
    });
    check(bool(main.middleware_work_group->append(clock_mutator,work_failure)),"actual clock-replacement completion");
    check(!main_binding::pump_retail_middleware(main,failure)&&failure.kind==StartupError::crt,
          "completion cannot change retained CRT domain inside one frame");
    other_crt=std::move(startup.crt);startup.crt=std::move(saved_crt);
    check(other_crt->shutdown(crt_failure),"unused replacement clock closed");
    auto retained_frame=std::move(startup.middleware_frame);
    check(!main_binding::pump_retail_middleware(main,failure)&&
          failure.kind==StartupError::middleware_frame,"missing frame owner refuses without source-default success");
    startup.middleware_frame=std::move(retained_frame);
    auto actual_dispatcher=main.middleware_dispatcher;
    auto foreign=NativeRetailMiddlewareDispatcher::create(work_failure);
    check(bool(foreign),"independent actual native dispatcher");
    main.middleware_dispatcher=foreign;
    check(!main_binding::pump_retail_middleware(main,failure)&&
          failure.middleware.kind==RetailMiddlewareWorkError::foreign_parent,"wrong native root/group pair refused");
    main.middleware_dispatcher=actual_dispatcher;
    check(foreign->shutdown(work_failure),"unused foreign dispatcher closes");
    bool wrong_thread=false;
    std::thread other([&] {
        StartupFailure off;
        wrong_thread=!main_binding::pump_retail_middleware(main,off)&&
                     off.middleware.kind==RetailMiddlewareWorkError::wrong_thread;
    });
    other.join();check(wrong_thread,"main middleware pump refuses foreign thread");
    NativeRetailMainState reentry(startup,samples);
    check(startup.middleware_frame.get()==frame,"persistent owner survives a separate native main control object");
    check(!main_binding::pump_retail_middleware(reentry,failure)&&failure.kind==StartupError::middleware,
          "new control object cannot borrow a missing dispatcher by default");
    check(main_binding::shutdown_retail_middleware_files(main,failure)&&
          startup.middleware_frame.get()==frame,"file teardown retains program-lifetime native time/callback owner");
    check(!main_binding::pump_retail_middleware(main,failure)&&failure.kind==StartupError::middleware,
          "retired file dispatcher cannot silently tick");
    auto first_sample=frame->sample(*startup.crt,frame_failure);
    check(bool(first_sample),"retained time before actual second-main startup");
    unsigned reentry_callback_calls=0;
    check(frame->install_callback([&reentry_callback_calls](std::uint8_t argument) {
        check(argument==0,"retained callback argument");++reentry_callback_calls;
    },frame_failure),"real program callback retained across file lifecycle");
    check(main_binding::construct_retail_middleware_work_group(reentry,failure)&&
          main_binding::initialize_retail_middleware_files(reentry,failure)&&
          startup.middleware_frame.get()==frame&&
          main_binding::pump_retail_middleware(reentry,failure)&&reentry_callback_calls==1,
          "actual second-main file initialization retains same time/callback owner");
    auto second_sample=frame->sample(*startup.crt,frame_failure);
    check(second_sample&&frame->elapsed_nanoseconds(*first_sample,*second_sample,frame_failure),
          "original typed time mark remains valid after second-main startup");
    check(frame->clear_callback(frame_failure)&&
          main_binding::shutdown_retail_middleware_files(reentry,failure),
          "second real control object retires without dropping persistent owner");
    std::puts("PASS actual native middleware main pump, source completion order and real main control transfer; game open");
    return 0;
} catch(const std::exception& error) {
    std::fprintf(stderr,"FAIL native middleware main: %s\n",error.what());return 1;
}}
