#pragma once
#include "mka/frontend_scheduler.hpp"
#include "mka/matfx_binding.hpp"
#include <list>
#include <thread>
#include <vector>

namespace mka::game {
enum class MaterialMaintenanceError {none,wrong_thread,reentry,invalid_input,
    foreign_owner,stale_record,expired_target,allocation,overflow,binding,step_state,
    unbound_collection,unbound_hierarchy_resource};
struct MaterialMaintenanceFailure {
    MaterialMaintenanceError kind=MaterialMaintenanceError::none;
    const char* operation="";
    std::uint64_t record=0;
    MatFxBindingFailure binding;
};
// Source game selection, never the host monitor rate or a sampled wall clock.
struct MaterialGameStepPolicy {bool game_flag0,scaling_override;std::int32_t stored_game_rate;};
struct MaterialGameStepSnapshot {MaterialGameStepPolicy policy;MaterialStepDelta material;MaterialScalarWord companion_a,companion_b;};
class NativeMaterialGameStep final {
public:
    static std::shared_ptr<NativeMaterialGameStep> create(MaterialGameStepPolicy,MaterialMaintenanceFailure&) noexcept;
    bool refresh(MaterialGameStepPolicy,MaterialMaintenanceFailure&) noexcept;
    std::optional<MaterialGameStepSnapshot> snapshot(MaterialMaintenanceFailure&) const noexcept;
private:
    explicit NativeMaterialGameStep(MaterialGameStepPolicy) noexcept;
    bool access(MaterialMaintenanceFailure&) const noexcept;
    MaterialGameStepSnapshot state_;
    const std::thread::id owner_=std::this_thread::get_id();
};

class NativeMaterialMaintenance;
// Owned material lists only. This does not implement the original hierarchy's
// pipeline/resource preparation or assert that any renderer has consumed it.
class NativeMaterialGroup final {
public:
    static std::shared_ptr<NativeMaterialGroup> create(
        std::vector<std::shared_ptr<NativeMatFxBinding>>,MaterialMaintenanceFailure&) noexcept;
private:
    friend class NativeMaterialMaintenance;
    friend class NativeTrackedMaterialOwner;
    friend class NativeMaterialGeometry;
    explicit NativeMaterialGroup(std::vector<std::shared_ptr<NativeMatFxBinding>> x):materials_(std::move(x)){}
    std::vector<std::shared_ptr<NativeMatFxBinding>> materials_;
    const std::thread::id owner_=std::this_thread::get_id();
};

// Geometry plugin presence and count are source resource data, not device
// state. Separate geometry/atomic owners preserve shared-geometry identity.
struct MaterialGeometryImport {
    MatFxHierarchyRoute plugin_route;
    std::int32_t source_count;
};
struct MaterialGeometrySnapshot {
    MaterialGeometryImport imported;
    std::size_t materials;
};
class NativeMaterialGeometry final {
public:
    static std::shared_ptr<NativeMaterialGeometry> create(
        std::shared_ptr<NativeMaterialGroup>,MaterialGeometryImport,MaterialMaintenanceFailure&) noexcept;
    std::optional<MaterialGeometrySnapshot> snapshot(MaterialMaintenanceFailure&) const noexcept;
private:
    friend class NativeMaterialMaintenance;
    friend class NativeMaterialHierarchy;
    NativeMaterialGeometry(std::shared_ptr<NativeMaterialGroup> group,MaterialGeometryImport imported)
        :group_(std::move(group)),imported_(imported){}
    std::shared_ptr<NativeMaterialGroup> group_;
    const MaterialGeometryImport imported_;
    const std::thread::id owner_=std::this_thread::get_id();
};
struct MaterialHierarchyImport {std::int32_t resource_kind;bool effect_enabled;};
struct MaterialHierarchySnapshot {
    std::shared_ptr<const NativeMaterialGeometry> geometry;
    MaterialHierarchyImport state;
};
class NativeMaterialHierarchy final {
public:
    static std::shared_ptr<NativeMaterialHierarchy> create(
        std::shared_ptr<NativeMaterialGeometry>,MaterialHierarchyImport,MaterialMaintenanceFailure&) noexcept;
    ~NativeMaterialHierarchy();
    std::optional<MaterialHierarchySnapshot> snapshot(MaterialMaintenanceFailure&) const noexcept;
private:
    friend class NativeMaterialMaintenance;
    NativeMaterialHierarchy(std::shared_ptr<NativeMaterialGeometry> geometry,MaterialHierarchyImport state)
        :geometry_(std::move(geometry)),state_(state){}
    bool access(MaterialMaintenanceFailure&) const noexcept;
    std::shared_ptr<NativeMaterialGeometry> geometry_;
    MaterialHierarchyImport state_;
    const std::thread::id owner_=std::this_thread::get_id();
    bool busy_=false;
};
struct MaterialOwnerGeneration;
class NativeTrackedMaterialOwner final {
public:
    static std::shared_ptr<NativeTrackedMaterialOwner> create(
        std::vector<std::shared_ptr<NativeMaterialGroup>>,MaterialMaintenanceFailure&) noexcept;
    ~NativeTrackedMaterialOwner();
    bool replace_groups(std::vector<std::shared_ptr<NativeMaterialGroup>>,MaterialMaintenanceFailure&) noexcept;
    bool invalidate_generation(MaterialMaintenanceFailure&) noexcept;
private:
    friend class NativeMaterialMaintenance;
    explicit NativeTrackedMaterialOwner(std::vector<std::shared_ptr<NativeMaterialGroup>>);
    bool access(MaterialMaintenanceFailure&) const noexcept;
    std::vector<std::shared_ptr<NativeMaterialGroup>> groups_;
    std::shared_ptr<const MaterialOwnerGeneration> generation_;
    const std::thread::id owner_=std::this_thread::get_id();
    bool busy_=false;
};

// Source identity writes OR existing flags and leave three pad words alone.
// Callers supply owned initialization words explicitly; no old heap is read.
struct MaterialTrackingSeed {
    std::array<std::uint32_t,2> flags;
    std::array<std::array<std::uint32_t,3>,2> padding;
};
enum class MaterialTrackingRoute {direct_material,prepared_group,owner_collection,hierarchy};
class MaterialTrackingToken {
public:
    MaterialTrackingToken(const MaterialTrackingToken&)=default;
    MaterialTrackingToken& operator=(const MaterialTrackingToken&)=default;
private:
    friend class NativeMaterialMaintenance;
    MaterialTrackingToken(std::shared_ptr<const void> p,std::uint64_t id):owner_(std::move(p)),id_(id){}
    std::shared_ptr<const void> owner_;std::uint64_t id_;
};
struct MaterialTrackedSnapshot {
    std::uint64_t id;
    MaterialTrackingRoute route;
    bool associated_live;
    MaterialTranslationRates rates;
    MaterialLayerSelection selection;
    MaterialTransformPair matrices;
};
struct MaterialMaintenancePass {std::uint64_t visited=0,retired=0,skipped=0,advances=0,pair_publications=0;};
class NativeMaterialMaintenance final {
public:
    static std::shared_ptr<NativeMaterialMaintenance> create(MaterialMaintenanceFailure&) noexcept;
    ~NativeMaterialMaintenance();
    // Source direct null-owner/target is normal absence: nullopt + no failure.
    std::optional<MaterialTrackingToken> register_direct(
        std::shared_ptr<NativeTrackedMaterialOwner>,std::shared_ptr<NativeMatFxBinding>,
        MaterialTranslationRates,MaterialTrackingSeed,MaterialMaintenanceFailure&) noexcept;
    // These register the proved maintenance data route. They do NOT replace
    // full source185630/333130 resource/hierarchy preparation or its producers.
    std::optional<MaterialTrackingToken> register_prepared_group(
        std::shared_ptr<NativeTrackedMaterialOwner>,std::shared_ptr<NativeMaterialGroup>,
        MaterialTranslationRates,MaterialTrackingSeed,MaterialMaintenanceFailure&) noexcept;
    std::optional<MaterialTrackingToken> register_owner_collection(
        std::shared_ptr<NativeTrackedMaterialOwner>,MaterialTranslationRates,
        MaterialTrackingSeed,MaterialMaintenanceFailure&) noexcept;
    // Actual source185630: initialize, prepare native geometry/materials, then
    // prepend. Absent-plugin nonzero resource kinds need the actual program
    // producer and fail by name until it is implemented; no draw is invented.
    std::optional<MaterialTrackingToken> register_hierarchy(
        std::shared_ptr<NativeTrackedMaterialOwner>,std::shared_ptr<NativeMaterialHierarchy>,
        MaterialTranslationRates,MaterialTrackingSeed,MaterialMaintenanceFailure&) noexcept;
    bool retire(const MaterialTrackingToken&,MaterialMaintenanceFailure&) noexcept;
    bool clear(MaterialMaintenanceFailure&) noexcept;
    std::optional<MaterialTrackedSnapshot> record(const MaterialTrackingToken&,MaterialMaintenanceFailure&) const noexcept;
    std::optional<MatFxParameterPair> parameter_references(const MaterialTrackingToken&,MaterialMaintenanceFailure&) const noexcept;
    std::optional<std::vector<std::uint64_t>> order(MaterialMaintenanceFailure&) const noexcept;
    std::optional<MaterialMaintenancePass> step(MaterialStepDelta,MaterialMaintenanceFailure&) noexcept;
private:
    struct Record;
    NativeMaterialMaintenance();
    bool access(MaterialMaintenanceFailure&) const noexcept;
    bool find(const MaterialTrackingToken&,const Record*&,MaterialMaintenanceFailure&) const noexcept;
    std::optional<MaterialTrackingToken> register_owned(MaterialTrackingRoute,
        std::shared_ptr<NativeTrackedMaterialOwner>,std::shared_ptr<NativeMatFxBinding>,
        std::shared_ptr<NativeMaterialGroup>,std::shared_ptr<NativeMaterialHierarchy>,
        MaterialTranslationRates,MaterialTrackingSeed,
        MaterialMaintenanceFailure&) noexcept;
    bool publish(Record&,const std::shared_ptr<NativeMatFxBinding>&,bool,MaterialMaintenancePass&,MaterialMaintenanceFailure&) noexcept;
    bool advance_group(Record&,const NativeMaterialGroup&,MaterialStepDelta,MaterialMaintenancePass&,MaterialMaintenanceFailure&) noexcept;
    std::list<Record> records_;
    std::shared_ptr<const void> identity_;
    std::uint64_t generation_=0;
    const std::thread::id owner_=std::this_thread::get_id();
    mutable bool busy_=false;
};

// Actual populated maintenance body, yielding the proved +1 outcome. Neither
// a missing registry nor an unknown step producer is replaced with success.
class NativeMaterialMaintenanceTask final : public NativeFrontendTask {
public:
    static std::unique_ptr<NativeMaterialMaintenanceTask> create(
        std::shared_ptr<NativeMaterialMaintenance>,std::shared_ptr<NativeMaterialGameStep>,
        MaterialMaintenanceFailure&) noexcept;
    TaskUpdate update(NativeFrontendExecution&,FrontendSchedulerFailure&) override;
private:
    NativeMaterialMaintenanceTask(std::shared_ptr<NativeMaterialMaintenance> r,std::shared_ptr<NativeMaterialGameStep> s):registry_(std::move(r)),step_(std::move(s)){}
    std::shared_ptr<NativeMaterialMaintenance> registry_;
    std::shared_ptr<NativeMaterialGameStep> step_;
};
} // namespace mka::game
