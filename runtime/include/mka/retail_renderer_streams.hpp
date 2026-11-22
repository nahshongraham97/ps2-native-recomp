#pragma once
#include "mka/retail_renderer_core.hpp"
#include <atomic>
#include <memory>
#include <optional>
#include <span>
#include <thread>
#include <vector>

namespace mka::retail {
class NativeRetailRendererStreams;
struct NativeRetailRendererStreamRecord;
enum class RetailRendererStreamError {
 none,owner,wrong_thread,retired,reentrant,invalid_handle,foreign_owner,retired_stream,
 allocator,allocation,native_exception,invalid_backing,content,error_domain,
 native_range,source_wrapping_skip_unbound,after_effect_failure
};
struct RetailRendererStreamFailure {
 RetailRendererStreamError kind{RetailRendererStreamError::none};const char* operation{""};
 RetailRendererAllocatorFailure allocator;content::ContentVolumeFailure content;
 RetailRendererCoreFailure error_domain;
};
enum class RetailRendererStreamKind {memory_read,borrowed_file_read,owned_file_read};
class NativeRetailRendererStreamHandle final {
public:
 NativeRetailRendererStreamHandle()noexcept=default;
 bool empty()const noexcept{return !assigned_;}
 bool same_identity(const NativeRetailRendererStreamHandle& other)const noexcept{
  return assigned_&&other.assigned_&&!record_.owner_before(other.record_)&&!other.record_.owner_before(record_);
 }
private:
 friend class NativeRetailRendererStreams;
 NativeRetailRendererStreamHandle(std::weak_ptr<NativeRetailRendererStreams> owner,
     std::weak_ptr<NativeRetailRendererStreamRecord> record):owner_(std::move(owner)),record_(std::move(record)),assigned_(true){}
 std::weak_ptr<NativeRetailRendererStreams> owner_;
 std::weak_ptr<NativeRetailRendererStreamRecord> record_;
 bool assigned_{};
};
struct RetailRendererStreamRead {
 std::size_t bytes{};bool eof{};std::error_code host_error;
 std::optional<std::uint32_t> source_diagnostic;
};
struct RetailRendererStreamObservation {RetailRendererStreamKind kind;std::uint64_t position{};};
// Real growing native0404 resource. Read-only selected paths only; no source
// stream union, FILE layout, registry offset or16-entry pool is represented.
class NativeRetailRendererStreams final:public std::enable_shared_from_this<NativeRetailRendererStreams> {
public:
 ~NativeRetailRendererStreams();
 NativeRetailRendererStreams(const NativeRetailRendererStreams&)=delete;
 NativeRetailRendererStreams& operator=(const NativeRetailRendererStreams&)=delete;
 std::optional<NativeRetailRendererStreamHandle> open_memory_read(
     std::shared_ptr<const std::vector<std::uint8_t>>,RetailRendererStreamFailure&)noexcept;
 std::optional<NativeRetailRendererStreamHandle> open_resource_read(std::string_view,RetailRendererStreamFailure&);
 // Same explicitly shared opened cursor; stream close does not fclose a
 // caller-owned file. Callers serialize access to that actual native file.
 std::optional<NativeRetailRendererStreamHandle> adopt_borrowed_file_read(
     std::shared_ptr<content::NativeContentFile>,RetailRendererStreamFailure&);
 // An engaged result preserves partial bytes even on native I/O failure.
 // Inspect host_error/source_diagnostic and the named native failure too.
 std::optional<RetailRendererStreamRead> read(const NativeRetailRendererStreamHandle&,
     std::span<std::uint8_t>,RetailRendererStreamFailure&);
 bool skip_forward(const NativeRetailRendererStreamHandle&,std::uint64_t,RetailRendererStreamFailure&);
 // Consumes the record even on checked owned-file close failure.
 bool close(const NativeRetailRendererStreamHandle&,RetailRendererStreamFailure&)noexcept;
 std::optional<RetailRendererStreamObservation> observe(
     const NativeRetailRendererStreamHandle&,RetailRendererStreamFailure&);
 std::optional<std::size_t> live_streams(RetailRendererStreamFailure&)const noexcept;
 std::optional<RetailRendererErrorRecord> take_source_error(RetailRendererStreamFailure&)noexcept;
private:
 friend bool renderer_required::construct_core_plugin_0404(NativeRetailRendererCore&,RetailRendererCoreFailure&);
 friend void renderer_required::destroy_core_plugin_0404(NativeRetailRendererCore&)noexcept;
 NativeRetailRendererStreams(std::weak_ptr<NativeRetailGameSession>,
     std::shared_ptr<NativeRetailRendererAllocatorService>,content::NativeContentVolume&&,
     std::shared_ptr<NativeRetailRendererErrorDomain>)noexcept;
 static std::shared_ptr<NativeRetailRendererStreams> create(std::weak_ptr<NativeRetailGameSession>,
     std::shared_ptr<NativeRetailRendererAllocatorService>,const content::NativeContentVolume&,
     std::shared_ptr<NativeRetailRendererErrorDomain>,RetailRendererStreamFailure&)noexcept;
 std::shared_ptr<NativeRetailGameSession> admit(RetailRendererStreamFailure&,bool cleanup=false)const noexcept;
 std::shared_ptr<NativeRetailRendererStreamRecord> record(
     const NativeRetailRendererStreamHandle&,RetailRendererStreamFailure&)const noexcept;
 std::shared_ptr<NativeRetailRendererStreamRecord> prepare_record(RetailRendererStreamFailure&)noexcept;
 NativeRetailRendererStreamHandle publish(std::shared_ptr<NativeRetailRendererStreamRecord>)noexcept;
 bool report(std::uint32_t,RetailRendererStreamFailure&)noexcept;
 void release_record(NativeRetailRendererStreamRecord*)noexcept;
 content::ContentVolumeFailure retire()noexcept;
 std::weak_ptr<NativeRetailGameSession> session_;
 std::shared_ptr<NativeRetailRendererAllocatorService> allocator_;
 content::NativeContentVolume content_;
 std::shared_ptr<NativeRetailRendererErrorDomain> errors_;
 std::vector<std::shared_ptr<NativeRetailRendererStreamRecord>> records_;
 std::atomic<bool> active_{true};
 bool busy_{},after_effect_failure_{};
 const std::thread::id thread_{std::this_thread::get_id()};
};
}
