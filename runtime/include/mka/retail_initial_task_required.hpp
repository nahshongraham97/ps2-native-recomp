#pragma once

#include <cstdint>
#include <memory>

namespace mka::retail {
struct NativeRetailMainState;
class NativeRetailGameSession;

// An original child procedure retains its own native locals and resource
// owners across source virtual+14 wait. No default implementation exists.
struct RetailInitialChildStep {
    enum class Kind { invalid, completed, wait } kind{Kind::invalid};
    float duration{};
    static RetailInitialChildStep completed() noexcept { return {Kind::completed,0.0f}; }
    static RetailInitialChildStep wait(float value) noexcept { return {Kind::wait,value}; }
};
class NativeRetailInitialChild {
public:
    virtual ~NativeRetailInitialChild() = default;
    virtual RetailInitialChildStep advance() = 0;
};

enum class RetailInitialTransitionBody { source_245930, menu_18db60 };

// Strong source-effect dependencies. Production defines none of these until
// the complete corresponding retail effects are ported. A test translation
// unit may define explicit child procedures for a bounded CPU fixture.
namespace initial_task_required {
void run_reset_18d970(NativeRetailMainState&, NativeRetailGameSession&);
std::shared_ptr<NativeRetailInitialChild> begin_intro_18deb0(
    NativeRetailMainState&, NativeRetailGameSession&);
std::shared_ptr<NativeRetailInitialChild> begin_prompt_18dc10(
    NativeRetailMainState&, NativeRetailGameSession&);
void transition_1225d0(NativeRetailMainState&, std::uint32_t mode,
    RetailInitialTransitionBody next_body);
std::shared_ptr<NativeRetailInitialChild> begin_menu_18d450(
    NativeRetailMainState&, NativeRetailGameSession&);
std::shared_ptr<NativeRetailInitialChild> begin_menu_18d870(
    NativeRetailMainState&, NativeRetailGameSession&);
std::shared_ptr<NativeRetailInitialChild> begin_menu_18d8e0(
    NativeRetailMainState&, NativeRetailGameSession&);
std::shared_ptr<NativeRetailInitialChild> begin_menu_18d700(
    NativeRetailMainState&, NativeRetailGameSession&);
std::shared_ptr<NativeRetailInitialChild> begin_menu_18d800(
    NativeRetailMainState&, NativeRetailGameSession&);
std::shared_ptr<NativeRetailInitialChild> begin_menu_18d660(
    NativeRetailMainState&, NativeRetailGameSession&);
std::shared_ptr<NativeRetailInitialChild> begin_menu_18d770(
    NativeRetailMainState&, NativeRetailGameSession&);
} // namespace initial_task_required
} // namespace mka::retail
