#pragma once
#include "mka/logon_surround.hpp"
#include <stdexcept>
// Test callbacks only. This does not implement retail platform operations.
class LogonSurroundProbe final : public mka::native_logon::NativeLogonSurround {
public:
    using Effect=mka::native_logon::Effect;
    using Failure=mka::native_logon::SurroundFailure;
    using Progress=mka::native_logon::EffectProgress;
    LogonSurroundProbe(Effect begin,Effect finish,unsigned wait_begin=0,unsigned wait_finish=0):begin_(std::move(begin)),finish_(std::move(finish)),begin_left_(wait_begin),finish_left_(wait_finish){}
    Progress begin(Failure& f) override {if(!begin_(f.primary))return Progress::error;return begin_left_?Progress::pending:Progress::complete;}
    Progress poll_begin(Failure&) override {return begin_left_&&--begin_left_?Progress::pending:Progress::complete;}
    Progress begin_finish(Failure& f) override {if(!finish_(f.primary))return Progress::error;return finish_left_?Progress::pending:Progress::complete;}
    Progress poll_finish(Failure&) override {return finish_left_&&--finish_left_?Progress::pending:Progress::complete;}
    Progress retry_finish(Failure& f) override {if(!finish_(f.primary))return Progress::error;return finish_left_?Progress::pending:Progress::complete;}
private:
    Effect begin_,finish_;
    unsigned begin_left_,finish_left_;
};
inline mka::native_logon::SurroundFactory logon_surround_probe(
    mka::native_logon::Effect begin,mka::native_logon::Effect finish,unsigned wait_begin=0,unsigned wait_finish=0){
    return [begin=std::move(begin),finish=std::move(finish),wait_begin,wait_finish](mka::native_logon::EffectFailure& f){
        f={};return std::make_unique<LogonSurroundProbe>(begin,finish,wait_begin,wait_finish);
    };
}
