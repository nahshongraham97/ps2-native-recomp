#pragma once
#include "mka/retail_renderer_core.hpp"
#include <array>
#include <functional>
#include <span>
#include <string>
namespace mka::retail {
class NativeRetailRendererImages;
struct NativeRetailImageRecord;
enum class RetailRendererImageError {none,owner,wrong_thread,retired,foreign_owner,invalid_handle,retired_image,reentrant,
 allocator,allocation,extent,already_backed,missing_backing,unsupported_depth,unverified_gamma_domain,
 callback_required,callback_result,after_effect_unknown,no_format,no_reader,not_found,content,native_exception};
struct RetailRendererImageFailure {
 RetailRendererImageError kind{};const char* operation{""};std::uint32_t source_code{};
 RetailRendererAllocatorFailure allocator;content::ContentVolumeFailure content;
};
// A stable host-owned buffer;adapters can retain existing decoded data.
class NativeRetailImageBytes {
public:virtual ~NativeRetailImageBytes()=default;virtual std::span<std::byte> bytes()noexcept=0;
};
class NativeRetailImageHandle final {
public:
 NativeRetailImageHandle()noexcept=default;
 bool empty()const noexcept{return !assigned_;}
 bool same_identity(const NativeRetailImageHandle& x)const noexcept{
  return assigned_&&x.assigned_&&!record_.owner_before(x.record_)&&!x.record_.owner_before(record_);
 }
private:
 friend class NativeRetailRendererImages;
 NativeRetailImageHandle(std::weak_ptr<NativeRetailRendererImages> o,std::weak_ptr<NativeRetailImageRecord> r)
  :owner_(std::move(o)),record_(std::move(r)),assigned_(true){}
 std::weak_ptr<NativeRetailRendererImages> owner_;std::weak_ptr<NativeRetailImageRecord> record_;bool assigned_{};
};
struct RetailRendererImageDescription {std::int32_t width{},height{},depth{};};
struct RetailRendererImageObservation {
 RetailRendererImageDescription description;std::optional<std::size_t> stride;
 std::size_t pixel_bytes{},palette_bytes{};std::uint32_t flags{};bool extension_chain_completed{},destroying{};
};
class NativeRetailImageExtensionState {public:virtual ~NativeRetailImageExtensionState()=default;};
struct RetailRendererImageExtension {
 std::uint32_t id{};
 std::function<bool(NativeRetailRendererImages&,NativeRetailImageHandle,std::shared_ptr<NativeRetailImageExtensionState>&,RetailRendererImageFailure&)> construct;
 std::function<void(NativeRetailRendererImages&,NativeRetailImageHandle,std::shared_ptr<NativeRetailImageExtensionState>&)> destroy;
};
using RetailRendererImageReader=std::function<std::optional<NativeRetailImageHandle>(
 NativeRetailRendererImages&,std::string_view,content::NativeContentFile&,RetailRendererImageFailure&)>;
using RetailRendererImageWriter=std::function<bool(NativeRetailRendererImages&,NativeRetailImageHandle,std::string_view,RetailRendererImageFailure&)>;
struct RetailRendererImageFormatObservation {std::string lower_extension,upper_extension;bool reader{},writer{};};
class NativeRetailRendererImages final:public std::enable_shared_from_this<NativeRetailRendererImages> {
public:
 ~NativeRetailRendererImages();
 NativeRetailRendererImages(const NativeRetailRendererImages&)=delete;
 NativeRetailRendererImages& operator=(const NativeRetailRendererImages&)=delete;
 std::optional<NativeRetailImageHandle> create_image(RetailRendererImageDescription,RetailRendererImageFailure&);
 bool destroy_image(const NativeRetailImageHandle&,RetailRendererImageFailure&);
 std::optional<RetailRendererImageObservation> observe(const NativeRetailImageHandle&,RetailRendererImageFailure&)const noexcept;
 std::optional<std::size_t> live_images(RetailRendererImageFailure&)const noexcept;
 bool allocate_pixels(const NativeRetailImageHandle&,RetailRendererImageFailure&)noexcept;
 bool bind_borrowed(const NativeRetailImageHandle&,std::size_t,std::shared_ptr<NativeRetailImageBytes>,
  std::shared_ptr<NativeRetailImageBytes>,RetailRendererImageFailure&)noexcept;
 bool write_bytes(const NativeRetailImageHandle&,bool palette,std::size_t,std::span<const std::byte>,RetailRendererImageFailure&)noexcept;
 bool read_bytes(const NativeRetailImageHandle&,bool palette,std::size_t,std::span<std::byte>,RetailRendererImageFailure&)const noexcept;
 // Current verified descriptor/callers use1.0;other gamma domains fail by name.
 bool set_gamma(float,RetailRendererImageFailure&)noexcept;
 std::optional<std::array<std::uint8_t,256>> gamma_table(bool inverse,RetailRendererImageFailure&)const noexcept;
 bool apply_gamma(const NativeRetailImageHandle&,RetailRendererImageFailure&)noexcept;
 bool set_search_path(std::string_view,RetailRendererImageFailure&)noexcept;
 std::optional<std::string> search_path(RetailRendererImageFailure&)const noexcept;
 bool register_format(std::string_view,RetailRendererImageReader,RetailRendererImageWriter,RetailRendererImageFailure&)noexcept;
 std::optional<std::vector<RetailRendererImageFormatObservation>> formats(RetailRendererImageFailure&)const noexcept;
 std::optional<NativeRetailImageHandle> read_image(std::string_view,RetailRendererImageFailure&);
private:
 friend bool renderer_required::construct_core_plugin_0406(NativeRetailRendererCore&,RetailRendererCoreFailure&);
 friend void renderer_required::destroy_core_plugin_0406(NativeRetailRendererCore&)noexcept;
 struct Format;
 NativeRetailRendererImages(std::weak_ptr<NativeRetailGameSession>,std::shared_ptr<NativeRetailRendererAllocatorService>,
  content::NativeContentVolume&&,std::weak_ptr<NativeRetailRendererCore>,std::vector<std::shared_ptr<const RetailRendererImageExtension>>);
 static std::shared_ptr<NativeRetailRendererImages> create(NativeRetailRendererCore&,RetailRendererImageFailure&)noexcept;
 std::shared_ptr<NativeRetailGameSession> admit(RetailRendererImageFailure&)const noexcept;
 std::shared_ptr<NativeRetailImageRecord> record(const NativeRetailImageHandle&,RetailRendererImageFailure&)const noexcept;
 bool reject(RetailRendererImageFailure&,RetailRendererImageError,const char*,std::uint32_t=0)const noexcept;
 std::span<std::byte> region(const NativeRetailImageRecord&,bool)const noexcept;
 void retire()noexcept;
 std::weak_ptr<NativeRetailGameSession> session_;std::shared_ptr<NativeRetailRendererAllocatorService> allocator_;
 content::NativeContentVolume content_;std::weak_ptr<NativeRetailRendererCore> core_;
 std::vector<std::shared_ptr<const RetailRendererImageExtension>> extensions_;
 std::vector<std::shared_ptr<NativeRetailImageRecord>> records_;std::vector<std::shared_ptr<Format>> formats_;
 memory::NativeAllocation path_,scratch_;std::size_t path_length_{};
 std::array<std::uint8_t,256> gamma_{},inverse_gamma_{};
 bool active_{true},after_effect_unknown_{};const std::thread::id thread_{std::this_thread::get_id()};
};
}
