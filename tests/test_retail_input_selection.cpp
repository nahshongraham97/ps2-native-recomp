#include "mka/retail_input_selection.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <io.h>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>

using namespace mka::retail;
namespace {
bool count_allocations=false;
long allocation_attempts=0, fail_allocation_at=-1;
void check(bool value,const char* message) {
    if(!value) throw std::runtime_error(message);
}
class CaptureStderr final {
public:
    CaptureStderr() {
        file_=std::tmpfile();
        if(!file_) throw std::runtime_error("temporary native diagnostic file");
        std::fflush(stderr);
        saved_=_dup(_fileno(stderr));
        if(saved_<0||_dup2(_fileno(file_),_fileno(stderr))!=0) {
            if(saved_>=0) _close(saved_);
            std::fclose(file_);
            throw std::runtime_error("capture native stderr");
        }
        active_=true;
    }
    CaptureStderr(const CaptureStderr&)=delete;
    CaptureStderr& operator=(const CaptureStderr&)=delete;
    ~CaptureStderr() { restore(); if(file_) std::fclose(file_); }
    std::string finish() {
        restore();
        check(std::fseek(file_,0,SEEK_SET)==0,"rewind native diagnostic");
        std::string result;
        char chunk[128];
        for(;;) {
            const auto size=std::fread(chunk,1,sizeof(chunk),file_);
            result.append(chunk,size);
            if(size<sizeof(chunk)) {
                check(!std::ferror(file_),"read native diagnostic");
                break;
            }
        }
        return result;
    }
private:
    void restore() noexcept {
        if(!active_) return;
        std::fflush(stderr);
        _dup2(saved_,_fileno(stderr));
        _close(saved_);
        active_=false;
    }
    std::FILE* file_{};
    int saved_{-1};
    bool active_{};
};
struct Pristine {
    std::shared_ptr<NativeRetailGameSession> session;
    std::shared_ptr<NativeRetailInputRoles> roles;
    std::shared_ptr<NativeRetailParticipants> participants;
    Pristine() {
        RetailGameSessionFailure sf;
        RetailInputRoleFailure rf;
        RetailParticipantFailure pf;
        session=NativeRetailGameSession::create(sf);
        roles=NativeRetailInputRoles::create(rf);
        participants=NativeRetailParticipants::create_first_two(pf);
        check(session&&roles&&participants,"pristine source owners born");
    }
};
struct Fixture {
    std::shared_ptr<NativeRetailGameSession> session;
    std::shared_ptr<NativeRetailGameControls> controls;
    std::shared_ptr<NativeRetailInputRoles> roles;
    std::shared_ptr<NativeRetailParticipants> participants;
    std::shared_ptr<NativeRetailInputAssignments> assignments;
    std::unique_ptr<NativeRetailInputSelection> selection;
    std::shared_ptr<NativeRetailInputParticipant> first,second;
    Fixture() {
        RetailGameSessionFailure sf;
        RetailInputRoleFailure rf;
        RetailParticipantFailure pf;
        RetailInputAssignmentFailure af;
        RetailInputSelectionFailure f;
        session=NativeRetailGameSession::create(sf);
        roles=NativeRetailInputRoles::create(rf);
        participants=NativeRetailParticipants::create_first_two(pf);
        check(session&&roles&&participants,"retained source input owners born");
        controls=session->controls(sf);
        first=participants->first_two(0,pf);
        second=participants->first_two(1,pf);
        check(controls&&first&&second,"real first two participant identities");
        selection=NativeRetailInputSelection::create(session,roles,participants,f);
        check(bool(selection),"physical selector joins retained session and game domain");
        assignments=NativeRetailInputAssignments::create(roles,participants,af);
        check(bool(assignments),"second adapter shares persistent association count");
    }
    std::shared_ptr<NativeRetailInputParticipant> choose(RetailInputRole role) {
        RetailInputSelectionFailure f;
        const auto chosen=selection->select_physical(role,f);
        check(bool(chosen)&&f.kind==RetailInputSelectionError::none,
              "source candidate selection completed");
        return *chosen;
    }
    void mode(std::int32_t value) {
        RetailGameSessionFailure f;
        check(session->store_scalar_mode_from_source(value,f),"source scalar mode store");
    }
    void flags(std::uint8_t value) {
        RetailGameControlsFailure f;
        check(controls->store_selection_flags_from_source(value,f),
              "actual 660084 source game controls store");
    }
};
}

void* operator new(std::size_t size) {
    if(count_allocations&&allocation_attempts++==fail_allocation_at)
        throw std::bad_alloc();
    if(void* p=std::malloc(size?size:1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }

int main() {
    try {
        Pristine measured;
        RetailInputSelectionFailure allocation_failure;
        allocation_attempts=0; fail_allocation_at=-1; count_allocations=true;
        auto measured_selection=NativeRetailInputSelection::create(
            measured.session,measured.roles,measured.participants,allocation_failure);
        count_allocations=false;
        const auto creation_allocations=allocation_attempts;
        check(measured_selection&&creation_allocations>0,
              "actual selector and assignment adapter allocate before publication");
        for(long cut=0;cut<creation_allocations;++cut) {
            Pristine pristine;
            allocation_attempts=0; fail_allocation_at=cut; count_allocations=true;
            auto failed=NativeRetailInputSelection::create(
                pristine.session,pristine.roles,pristine.participants,allocation_failure);
            count_allocations=false; fail_allocation_at=-1;
            check(!failed&&allocation_failure.kind!=RetailInputSelectionError::none,
                  "every selector create allocation cut returns a named failure");
            RetailParticipantFailure pf;
            RetailInputAssignmentFailure af;
            auto other_participants=NativeRetailParticipants::create_first_two(pf);
            auto rebound=NativeRetailInputAssignments::create(
                pristine.roles,other_participants,af);
            check(other_participants&&rebound,
                  "failed selector create leaves original roles free for a different domain");
        }
        Pristine prebound;
        RetailInputAssignmentFailure prebound_failure;
        auto prior_adapter=NativeRetailInputAssignments::create(
            prebound.roles,prebound.participants,prebound_failure);
        check(bool(prior_adapter),"valid game domain bound before selector fault");
        allocation_attempts=0; fail_allocation_at=0; count_allocations=true;
        auto prebound_failed=NativeRetailInputSelection::create(
            prebound.session,prebound.roles,prebound.participants,allocation_failure);
        count_allocations=false; fail_allocation_at=-1;
        check(!prebound_failed&&allocation_failure.kind==RetailInputSelectionError::allocation,
              "prebound selector shell allocation cut returns named failure");
        auto joined=NativeRetailInputSelection::create(
            prebound.session,prebound.roles,prebound.participants,allocation_failure);
        check(bool(joined),"prebound valid domain survives failed factory unchanged");
        Fixture source;
        RetailGameSessionFailure sf;
        RetailGameControlsFailure cf;
        check(source.session->scalar_mode(sf)==0&&
              source.controls->selection_flags(cf)==0,
              "scalar mode and source control word are separately BSS zero");
        check(source.choose(RetailInputRole::role0)==source.first&&
              source.choose(RetailInputRole::role1)==source.second,
              "source count-zero physical role parity");
        for(const auto mode:std::array{7,15}) {
            source.mode(mode);
            check(source.choose(RetailInputRole::role0)==source.first&&
                  source.choose(RetailInputRole::role1)==source.second,
                  "mode7/15 physical indexes return constructed first two");
        }
        source.mode(8);
        check(source.choose(RetailInputRole::role0)==source.first&&
              source.choose(RetailInputRole::role1)==source.first,
              "mode8 overrides role parity with first participant");
        source.mode(4);
        check(source.choose(RetailInputRole::role0)==source.first&&
              source.choose(RetailInputRole::role1)==source.second,
              "mode4 role2-only source word is not read for physical roles");
        source.mode(0);
        RetailInputAssignmentFailure af;
        check(source.assignments->increment_count_before_280c50_bind(af)&&
              source.assignments->bind_direct(source.first,RetailInputRole::role2,af),
              "real association count and role2 inverse seeded by source operations");
        check(source.choose(RetailInputRole::role0)==source.second&&
              source.choose(RetailInputRole::role1)==source.second,
              "first linked participant redirects both physical candidates to other");
        check(source.assignments->bind_direct(source.second,RetailInputRole::role0,af)&&
              source.choose(RetailInputRole::role0)==source.first,
              "role scan starts at role0 and picks opposite of first linked second");
        Fixture count_without_link;
        check(count_without_link.assignments->increment_count_before_280c50_bind(af)&&
              count_without_link.choose(RetailInputRole::role0)==count_without_link.first&&
              count_without_link.choose(RetailInputRole::role1)==count_without_link.second,
              "nonzero count with no linked roles returns physical parity");

        Fixture flags;
        flags.flags(1);
        {
            CaptureStderr capture;
            check(flags.choose(RetailInputRole::role0)==flags.second,
                  "flag bit0 set bit1 clear chooses unassigned second");
            check(capture.finish()=="assigning plyr_info to P2\n",
                  "source second-participant native diagnostic exact bytes");
        }
        flags.flags(3);
        {
            CaptureStderr capture;
            check(flags.choose(RetailInputRole::role1)==flags.first,
                  "flag bits0/1 choose unassigned first independent of physical role");
            check(capture.finish()=="assigning plyr_info to P1\n",
                  "source first-participant native diagnostic exact bytes");
        }
        check(flags.assignments->bind_direct(flags.first,RetailInputRole::role0,af),
              "actual first participant assigned before null candidate check");
        {
            CaptureStderr capture;
            check(!flags.choose(RetailInputRole::role1),
                  "already-assigned flag-selected participant returns source null");
            check(capture.finish().empty(),"null candidate emits no diagnostic");
        }
        RetailInputSelectionFailure f;
        check(!flags.selection->select_physical(RetailInputRole::role2,f)&&
              f.kind==RetailInputSelectionError::invalid_physical_role,
              "source role2 BSS record is outside physical selector");
        bool wrong_thread=false;
        std::thread other([&] {
            RetailInputSelectionFailure off;
            wrong_thread=!flags.selection->select_physical(RetailInputRole::role0,off)&&
                off.kind==RetailInputSelectionError::wrong_thread;
        });
        other.join();
        check(wrong_thread,"foreign thread cannot read retained input owners");
        RetailParticipantFailure foreign_failure;
        auto foreign_participants=NativeRetailParticipants::create_first_two(
            foreign_failure);
        check(bool(foreign_participants),"foreign collection exists for domain refusal");
        // The foreign collection is checked against the role's already bound
        // domain. Its own constructor has no host route or assignment effect.
        auto foreign=NativeRetailInputSelection::create(flags.session,flags.roles,
            foreign_participants,f);
        check(!foreign&&f.kind==RetailInputSelectionError::assignment&&
              f.assignment.kind==RetailInputAssignmentError::foreign_domain,
              "foreign participant collection cannot reuse game role domain");
        auto retained=flags.selection.get();
        flags.session.reset();
        check(retained->select_physical(RetailInputRole::role0,f).has_value(),
              "selector retains persistent game session after caller release");
        std::puts("PASS native physical-role source candidate selection, controls, diagnostics and ownership");
        return 0;
    } catch(const std::exception& e) {
        std::fprintf(stderr,"FAIL native retail input selection: %s\n",e.what());
        return 1;
    }
}
