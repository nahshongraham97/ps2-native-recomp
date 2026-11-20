#pragma once
#include "mka/material_translation.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>
#include <utility>
namespace mka::game {
// Derived native resource owners MUST perform their real final release.
// This abstract lifetime interface does not implement a renderer or texture.
class NativeMatFxProperty {
public: virtual ~NativeMatFxProperty() noexcept=0;
};
enum class MatFxBindingError {none,wrong_thread,reentry,invalid_state,missing_slot,missing_property,stale_matrix,unbound_hierarchy,allocation};
struct MatFxBindingFailure {MatFxBindingError kind=MatFxBindingError::none;const char* operation="";};
enum class MatFxMode : std::uint32_t {disabled=0,effect1=1,effect2=2,combined12=3,property=4,parameter_pair=5,parameter_pair_property=6};
enum class MatFxEffectKind : std::uint32_t {none=0,effect1=1,effect2=2,property=4,parameter_pair=5};
class NativeMatFxBinding;
class MatFxMatrixReference {
public:
 static MatFxMatrixReference null() noexcept {return {};}
 static MatFxMatrixReference borrow(const std::shared_ptr<const NativeMaterialMatrix>& matrix) noexcept {return MatFxMatrixReference(matrix);}
 bool is_null() const noexcept {return !nonnull_;}
private:
 friend class NativeMatFxBinding;
 MatFxMatrixReference()=default;
 explicit MatFxMatrixReference(const std::shared_ptr<const NativeMaterialMatrix>& p) noexcept:matrix_(p),nonnull_(bool(p)){}
 std::weak_ptr<const NativeMaterialMatrix> matrix_;bool nonnull_=false;
};
struct MatFxParameterPair {MatFxMatrixReference first=MatFxMatrixReference::null(),second=MatFxMatrixReference::null();};
struct MatFxParameterPairLease {std::shared_ptr<const NativeMaterialMatrix> first,second;};
struct MatFxSelectors {std::int32_t first,second;};
// Kind1 owns primary and secondary, kind2 owns secondary, kind4 owns primary.
// Kind5 borrows pair. Unused fields must be empty, not hidden resources.
struct MatFxEffectSlot {
 MatFxEffectKind kind=MatFxEffectKind::none;
 std::shared_ptr<NativeMatFxProperty> primary,secondary;
 MatFxSelectors selectors{0,0};MatFxParameterPair pair;
};
struct MatFxImportedState {
 MatFxMode mode;std::array<MatFxEffectSlot,2> slots;
 MatFxImportedState(MatFxMode m,std::array<MatFxEffectSlot,2> s) noexcept:mode(m),slots(std::move(s)){}
};
struct MatFxBindingSnapshot {MatFxMode mode;std::array<MatFxEffectSlot,2> slots;};
class NativeMatFxBinding final {
public:
 static std::unique_ptr<NativeMatFxBinding> create(MatFxImportedState&&,MatFxBindingFailure&) noexcept;
 ~NativeMatFxBinding();
 NativeMatFxBinding(const NativeMatFxBinding&)=delete;NativeMatFxBinding& operator=(const NativeMatFxBinding&)=delete;
 std::optional<MatFxBindingSnapshot> snapshot(MatFxBindingFailure&) const noexcept;
 // Source185350: old4 ->6 preserves property/selectors; other modes ->5.
 // Existing mode5 retains its borrowed pair (equal nonzero source mode).
 bool prepare_direct_tracking(MatFxBindingFailure&) noexcept;
 bool prepare_hierarchy_tracking(MatFxBindingFailure&) noexcept;
 std::optional<MatFxParameterPairLease> first_parameter_pair(MatFxBindingFailure&) const noexcept;
 bool set_first_parameter_pair(const MatFxParameterPair&,MatFxBindingFailure&) noexcept;
 std::optional<std::shared_ptr<NativeMatFxProperty>> first_property(MatFxBindingFailure&) const noexcept;
 std::optional<MatFxSelectors> property_selectors(MatFxBindingFailure&) const noexcept;
 bool replace_property(const std::shared_ptr<NativeMatFxProperty>&,MatFxBindingFailure&) noexcept;
 bool set_property_selectors(MatFxSelectors,MatFxBindingFailure&) noexcept;
private:
 explicit NativeMatFxBinding(MatFxImportedState&&) noexcept;
 bool access(MatFxBindingFailure&) const noexcept;
 bool lease(const MatFxParameterPair&,MatFxParameterPairLease&,MatFxBindingFailure&) const noexcept;
 MatFxEffectSlot* find(MatFxEffectKind) noexcept;
 const MatFxEffectSlot* find(MatFxEffectKind) const noexcept;
 MatFxMode mode_;std::array<MatFxEffectSlot,2> slots_;
 std::thread::id owner_=std::this_thread::get_id();mutable bool busy_=false;
};
} // namespace mka::game
