#pragma once

#include "mka/retail_observer_list.hpp"

namespace mka::retail {
class NativeRetailGameSession;

// Native identity/lifetime contract for objects published by the actual scene
// producers. This is not SceneEventGraph.current, a source address, or an
// object factory. Concrete producers supply both cookie and real cleanup.
class NativeRetailSceneObject : public NativeRetailObservedObject {
public:
    virtual const NativeRetailGameSession* owner_session() const noexcept = 0;
    // Native logical release is distinct from the numeric source cookie.
    // A live cookie0 remains valid; a released object cannot become live
    // merely because a temporary C++ reference keeps its storage allocated.
    virtual bool scene_identity_live() const noexcept = 0;
};
} // namespace mka::retail
