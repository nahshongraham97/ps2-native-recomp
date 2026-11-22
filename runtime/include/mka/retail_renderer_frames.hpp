#pragma once
#include "mka/retail_renderer_matrix.hpp"
#include <atomic>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

namespace mka::retail {
class NativeRetailRendererFrames;
struct NativeRetailRendererFrameRecord;
enum class RetailRendererFrameError {
 none,owner,wrong_thread,retired,invalid_handle,foreign_owner,retired_frame,
 allocator,allocation,native_exception,reentrant,cycle,parent_required,
 attached_hierarchy_required,nonleaf_single_delete_required,synchronization_required
};
struct RetailRendererFrameFailure {
 RetailRendererFrameError kind{RetailRendererFrameError::none};
 const char* operation{""};
 RetailRendererAllocatorFailure allocator;
};
class NativeRetailRendererFrameHandle final {
public:
 bool empty()const noexcept{return !assigned_;}
 bool same_identity(const NativeRetailRendererFrameHandle& other)const noexcept{
  return assigned_&&other.assigned_&&!record_.owner_before(other.record_)&&!other.record_.owner_before(record_);
 }
private:
 friend class NativeRetailRendererFrames;
 NativeRetailRendererFrameHandle(std::weak_ptr<NativeRetailRendererFrames> owner,
     std::weak_ptr<NativeRetailRendererFrameRecord> record):owner_(std::move(owner)),record_(std::move(record)),assigned_(true){}
 std::weak_ptr<NativeRetailRendererFrames> owner_;
 std::weak_ptr<NativeRetailRendererFrameRecord> record_;
 bool assigned_{};
public:
 NativeRetailRendererFrameHandle()noexcept=default;
};
// Diagnostic snapshot of the real native base record, NOT a synchronized
// world-matrix getter, full game object, source offset or extension byte block.
struct RetailRendererFrameObservation {
 RetailRendererMatrixValue modelling,world;
 std::uint8_t private_flags{};
 NativeRetailRendererFrameHandle parent,first_child,next_sibling,root;
};
struct RetailRendererFrameModuleObservation {std::size_t live_base_frames{},dirty_roots{};};

class NativeRetailRendererFrames final:public std::enable_shared_from_this<NativeRetailRendererFrames> {
public:
 ~NativeRetailRendererFrames();
 NativeRetailRendererFrames(const NativeRetailRendererFrames&)=delete;
 NativeRetailRendererFrames& operator=(const NativeRetailRendererFrames&)=delete;
 // Source-proved base birth under the initial EMPTY frame-extension registry.
 // Nonempty game plugins/attached objects/clone are not silently skipped.
 std::optional<NativeRetailRendererFrameHandle> create_base_frame(RetailRendererFrameFailure&)noexcept;
 std::optional<RetailRendererFrameObservation> observe(
     const NativeRetailRendererFrameHandle&,RetailRendererFrameFailure&)const noexcept;
 std::optional<RetailRendererFrameModuleObservation> observe_module(RetailRendererFrameFailure&)const noexcept;
 std::optional<std::vector<NativeRetailRendererFrameHandle>> dirty_roots(RetailRendererFrameFailure&)const noexcept;
 bool mark_dirty(const NativeRetailRendererFrameHandle&,RetailRendererFrameFailure&)noexcept;
 bool add_child(const NativeRetailRendererFrameHandle& parent,const NativeRetailRendererFrameHandle& child,
     RetailRendererFrameFailure&)noexcept;
 bool detach(const NativeRetailRendererFrameHandle&,RetailRendererFrameFailure&)noexcept;
 bool destroy_leaf(const NativeRetailRendererFrameHandle&,RetailRendererFrameFailure&)noexcept;
 bool destroy_root_hierarchy(const NativeRetailRendererFrameHandle&,RetailRendererFrameFailure&)noexcept;
 // Refuses outstanding work; does NOT implement generic/game synchronization.
 bool require_fully_synchronized(const NativeRetailRendererFrameHandle&,RetailRendererFrameFailure&)const noexcept;
private:
 friend bool renderer_required::construct_core_plugin_0403(NativeRetailRendererCore&,RetailRendererCoreFailure&);
 friend void renderer_required::destroy_core_plugin_0403(NativeRetailRendererCore&)noexcept;
 NativeRetailRendererFrames(std::weak_ptr<NativeRetailGameSession>,
     std::shared_ptr<NativeRetailRendererAllocatorService>)noexcept;
 static std::shared_ptr<NativeRetailRendererFrames> create(
     std::weak_ptr<NativeRetailGameSession>,std::shared_ptr<NativeRetailRendererAllocatorService>,
     RetailRendererFrameFailure&)noexcept;
 std::shared_ptr<NativeRetailGameSession> admit(RetailRendererFrameFailure&)const noexcept;
 std::shared_ptr<NativeRetailRendererFrameRecord> record(
     const NativeRetailRendererFrameHandle&,RetailRendererFrameFailure&)const noexcept;
 NativeRetailRendererFrameHandle handle(NativeRetailRendererFrameRecord*)const noexcept;
 void mark_record(NativeRetailRendererFrameRecord*)noexcept;
 void unlink_dirty(NativeRetailRendererFrameRecord*)noexcept;
 void rewrite_roots(NativeRetailRendererFrameRecord*,NativeRetailRendererFrameRecord*)noexcept;
 void detach_record(NativeRetailRendererFrameRecord*)noexcept;
 void release_record(NativeRetailRendererFrameRecord*)noexcept;
 void retire()noexcept;
 std::weak_ptr<NativeRetailGameSession> session_;
 std::shared_ptr<NativeRetailRendererAllocatorService> allocator_;
 std::vector<std::shared_ptr<NativeRetailRendererFrameRecord>> records_;
 NativeRetailRendererFrameRecord* dirty_first_{};
 NativeRetailRendererFrameRecord* dirty_last_{};
 std::atomic<bool> active_{true};
 bool busy_{};
 const std::thread::id thread_{std::this_thread::get_id()};
};
}
