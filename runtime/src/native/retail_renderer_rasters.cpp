#include "mka/retail_renderer_rasters.hpp"
#include "mka/retail_game_session.hpp"
#include "mka/retail_display_service.hpp"
#include "retail_main_control.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <new>

namespace mka::retail {

namespace {

template<class T, class Create>
std::shared_ptr<T> heap_object(
 const std::shared_ptr<NativeRetailRendererAllocatorService>& heap,
 RetailRendererRasterFailure& f,
 Create create) noexcept {
 static_assert(alignof(T) <= 16);
 auto allocation = heap->allocate(sizeof(T), f.allocator);
 if (!allocation) {
  f.kind = RetailRendererRasterError::allocator;
  f.operation = "0407 actual typed Permanent allocation";
  return {};
 }
 try {
  auto* object = create(allocation->data());
  return std::shared_ptr<T>(object, [heap, alloc = std::move(*allocation)](T* p) mutable {
   p->~T();
   alloc = memory::NativeAllocation{};
  });
 } catch (const std::bad_alloc&) {
  f.kind = RetailRendererRasterError::allocation;
  f.operation = "0407 actual native ownership";
 } catch (...) {
  f.kind = RetailRendererRasterError::native_exception;
  f.operation = "0407 native construction";
 }
 return {};
}

struct Layout {
 std::size_t bpp{0};
 std::size_t row{0};
 std::size_t stride{0};
 std::size_t pixels{0};
 std::size_t palette{0};
};

std::optional<Layout> compute_layout(RetailRendererRasterDescription d) noexcept {
 if (d.width < 0 || d.height < 0 || d.depth < 0) return {};
 if (d.flags & 0x80) {
  // No allocation / sentinel / subview
  return Layout{0, 0, 0, 0, 0};
 }
 const auto maximum = std::numeric_limits<std::size_t>::max();
 const std::size_t bpp = d.depth > 0 ? (static_cast<std::size_t>(d.depth) + 7) / 8 : 0;
 const auto row = bpp * static_cast<std::size_t>(d.width);
 if (row > maximum - 3) return {};
 const auto stride = (row + 3) & ~std::size_t(3);
 if (stride && static_cast<std::size_t>(d.height) > maximum / stride) return {};
 const auto pixels = stride * static_cast<std::size_t>(d.height);
 const std::size_t palette = d.depth == 4 ? 64 : d.depth == 8 ? 1024 : 0;
 if (pixels > maximum - palette) return {};
 return Layout{bpp, row, stride, pixels, palette};
}

} // namespace

struct NativeRetailRasterRecord {
 RetailRendererRasterDescription description{};
 std::uint32_t format{0};
 std::uint16_t offset_x{0};
 std::uint16_t offset_y{0};
 std::size_t stride{0};
 std::size_t pixel_bytes{0};
 std::size_t palette_bytes{0};
 std::uint32_t flags{0};
 std::uint32_t private_flags{0};
 bool is_subview{false};
 bool is_sentinel{false};

 std::weak_ptr<NativeRetailRasterRecord> parent;
 std::shared_ptr<NativeRetailRasterRecord> root_alias;
 // Native display descriptors borrow host storage; raster destruction never owns it.
 std::weak_ptr<NativeProductionDisplayService> display;
 std::uint64_t display_destination{0};
 bool display_assigned{false};

 std::vector<std::byte> pixel_data;
 std::vector<std::byte> palette_data;
 std::shared_ptr<const graphics::NativeGlTexture> gpu_texture;
 graphics::NativeTextureSampler sampler{
  graphics::NativeTextureFilter::linear,
  graphics::NativeTextureAddress::clamp_to_edge,
  graphics::NativeTextureAddress::clamp_to_edge
 };

 std::vector<std::shared_ptr<NativeRetailRasterExtensionState>> extensions;
 std::size_t destroy_cursor{0};
 bool alive{true};
 bool constructing{false};
 bool extensions_complete{false};
 bool destroying{false};
 bool callout_active{false};
 bool backend_destroyed{false};
 bool locked_pixels{false};
 bool locked_palette{false};
};

NativeRetailRendererRasters::NativeRetailRendererRasters(
 std::weak_ptr<NativeRetailGameSession> session,
 std::shared_ptr<NativeRetailRendererAllocatorService> allocator,
 std::weak_ptr<NativeRetailRendererCore> core,
 std::vector<std::shared_ptr<const RetailRendererRasterExtension>> extensions)
 : session_(std::move(session)),
   allocator_(std::move(allocator)),
   core_(std::move(core)),
   extensions_(std::move(extensions)) {
 // 2B9F70: Create default sentinel object.
 // Flags 0x80, zero dimensions, depth 0, no backing.
 sentinel_ = std::make_shared<NativeRetailRasterRecord>();
 sentinel_->description.width = 0;
 sentinel_->description.height = 0;
 sentinel_->description.depth = 0;
 sentinel_->description.type = 0;
 sentinel_->description.flags = 0x80;
 sentinel_->description.format = 0;
 sentinel_->flags = 0x80;
 sentinel_->is_sentinel = true;
 sentinel_->alive = true;
 // Initial context stack has default sentinel as target 0.
 context_stack_.push_back(sentinel_);
}

NativeRetailRendererRasters::~NativeRetailRendererRasters() {
 retire();
}

void NativeRetailRendererRasters::retire() noexcept {
 active_ = false;
 if (gpu_device_) {
  graphics::NativeTextureFailure tf;
  for (const auto& r : records_) {
   if (r && r->gpu_texture && !r->is_subview) {
    gpu_device_->revoke_image(r->gpu_texture, tf);
    r->gpu_texture.reset();
   }
  }
  for (const auto& r : records_) {
   if (r && r->is_subview) {
    r->gpu_texture.reset();
   }
  }
 }
 for (const auto& r : records_) {
  if (r) r->alive = false;
 }
 records_.clear();
 context_stack_.clear();
 if (sentinel_) {
  sentinel_->alive = false;
  sentinel_.reset();
 }
 extensions_.clear();
 gpu_device_.reset();
}

std::shared_ptr<NativeRetailRendererRasters> NativeRetailRendererRasters::create(
 NativeRetailRendererCore& core,
 RetailRendererRasterFailure& f) noexcept {
 f = {};
 RetailRendererCoreFailure cf;
 auto session = core.session_.lock();
 if (!session) {
  f.kind = RetailRendererRasterError::owner;
  f.operation = "0407 live session required";
  return {};
 }
 auto owner = NativeRetailRendererCore::for_session(session, core.allocator_, core.content_, cf);
 if (!owner) {
  f.kind = RetailRendererRasterError::owner;
  f.operation = "0407 actual Core identity";
  return {};
 }
 auto extensions = core.raster_extensions_;
 return heap_object<NativeRetailRendererRasters>(core.allocator_, f, [&](void* p) {
  return new(p) NativeRetailRendererRasters(session, core.allocator_, owner, std::move(extensions));
 });
}

std::shared_ptr<NativeRetailGameSession> NativeRetailRendererRasters::admit(
 RetailRendererRasterFailure& f) const noexcept {
 f = {};
 if (!active_) {
  reject(f, RetailRendererRasterError::retired, "0407 raster module retired");
  return {};
 }
 if (std::this_thread::get_id() != thread_) {
  reject(f, RetailRendererRasterError::wrong_thread, "0407 raster thread violation");
  return {};
 }
 auto session = session_.lock();
 RetailGameSessionFailure sf;
 if (!session || !session->validate_current_thread(sf)) {
  reject(f, RetailRendererRasterError::owner, "0407 raster session validation");
  return {};
 }
 return session;
}

std::shared_ptr<NativeRetailRasterRecord> NativeRetailRendererRasters::record(
 const NativeRetailRasterHandle& handle,
 RetailRendererRasterFailure& f) const noexcept {
 if (handle.empty()) {
  reject(f, RetailRendererRasterError::invalid_handle, "0407 empty raster handle");
  return {};
 }
 auto owner = handle.owner_.lock();
 if (owner.get() != this) {
  reject(f, RetailRendererRasterError::foreign_owner, "0407 foreign raster owner");
  return {};
 }
 auto rec = handle.record_.lock();
 if (!rec || !rec->alive) {
  reject(f, RetailRendererRasterError::retired_raster, "0407 retired raster record");
  return {};
 }
 return rec;
}

bool NativeRetailRendererRasters::reject(
 RetailRendererRasterFailure& f,
 RetailRendererRasterError kind,
 const char* op,
 std::uint32_t code) const noexcept {
 f.kind = kind;
 f.operation = op;
 f.source_code = code;
 return false;
}

NativeRetailRasterHandle NativeRetailRendererRasters::default_sentinel() const noexcept {
 if (!sentinel_) return {};
 return NativeRetailRasterHandle(
  std::const_pointer_cast<NativeRetailRendererRasters>(shared_from_this()), sentinel_);
}

void NativeRetailRendererRasters::set_backend_callbacks(RetailRendererRasterBackendCallbacks callbacks) noexcept {
 backend_callbacks_ = std::move(callbacks);
}

std::optional<NativeRetailRasterHandle> NativeRetailRendererRasters::create_raster(
 RetailRendererRasterDescription desc,
 RetailRendererRasterFailure& f) {
 if (!admit(f)) return {};

 // Format normalization and 2C8BD0 contracts:
 const std::uint32_t type = desc.flags & 7;
 if (desc.type == 0) desc.type = type;
 if ((desc.flags & 0xFF00) != 0 && desc.format == 0) {
  desc.format = desc.flags & 0xFF00;
 }
 if (desc.type == 4 && desc.format == 0) {
  desc.format = 0x0500;
  desc.depth = 32;
 }
 if ((desc.flags & 0x80) == 0 && desc.depth == 0) {
  if (desc.format == 0x0500 || desc.format == 0x0600 || desc.type == 4 || desc.type == 5) {
   desc.depth = 32;
  }
 }

 auto layout_opt = compute_layout(desc);
 if (!layout_opt) {
  reject(f, RetailRendererRasterError::extent, "0407 invalid raster dimensions or extent");
  return {};
 }
 const auto layout = *layout_opt;

 std::shared_ptr<NativeRetailRasterRecord> rec;
 try {
  rec = std::make_shared<NativeRetailRasterRecord>();
  // 001A8880: raster.parent = raster, before the platform create callback.
  rec->parent = rec;
  rec->description = desc;
  rec->format = desc.format;
  rec->flags = desc.flags;
  rec->stride = layout.stride;
  rec->pixel_bytes = layout.pixels;
  rec->palette_bytes = layout.palette;
  rec->offset_x = 0;
  rec->offset_y = 0;
  rec->alive = true;
  rec->constructing = true;
  rec->destroy_cursor = extensions_.size();

  if (layout.pixels > 0) {
   rec->pixel_data.resize(layout.pixels, std::byte{0});
  }
  if (layout.palette > 0) {
   rec->palette_data.resize(layout.palette, std::byte{0});
  }

  if (gpu_device_ && (desc.flags & 0x80) == 0 && (desc.type == 4 || desc.type == 5) &&
      layout.pixels > 0 && desc.width > 0 && desc.height > 0) {
   std::vector<std::uint8_t> blank_pixels(layout.pixels, 0);
   graphics::NativeTextureRgbaImage upload_img{
    static_cast<std::uint32_t>(desc.width),
    static_cast<std::uint32_t>(desc.height),
    blank_pixels
   };
   auto upload = gpu_device_->upload_rgba_image(upload_img, rec->sampler);
   if (upload.failure.kind == graphics::NativeTextureError::none) {
    rec->gpu_texture = std::move(upload.texture);
   }
  }
 } catch (const std::bad_alloc&) {
  reject(f, RetailRendererRasterError::allocation, "0407 raster record allocation");
  return {};
 } catch (...) {
  reject(f, RetailRendererRasterError::native_exception, "0407 raster record creation");
  return {};
 }

 // Publish record before callbacks so Main reentry retains ownership
 records_.push_back(rec);
 NativeRetailRasterHandle handle(shared_from_this(), rec);

 auto rollback_record = [&]() {
  auto it = std::find(records_.begin(), records_.end(), rec);
  if (it != records_.end()) records_.erase(it);
  rec->alive = false;
  if (rec->gpu_texture && gpu_device_) {
   graphics::NativeTextureFailure tf;
   gpu_device_->revoke_image(rec->gpu_texture, tf);
   rec->gpu_texture.reset();
  }
 };

 // Invoke concrete create(0, raster, flags) callback
 if (backend_callbacks_.create) {
  bool ok = false;
  rec->callout_active = true;
  try {
   ok = backend_callbacks_.create(*this, handle, desc.flags, f);
   rec->callout_active = false;
  } catch (const detail::NativeMainReentry&) {
   rec->callout_active = false;
   throw;
  } catch (...) {
   rec->callout_active = false;
   rollback_record();
   reject(f, RetailRendererRasterError::after_effect_unknown, "0407 backend create exception");
   return {};
  }
  if (!ok) {
   rollback_record();
   if (f.kind == RetailRendererRasterError::none) {
    reject(f, RetailRendererRasterError::backend_refusal, "0407 backend create refusal");
   }
   return {};
  }
 }

 // Extension initialization: 2B9E50 ignores extension refusal return
 if (!extensions_.empty()) {
  rec->extensions.assign(extensions_.size(), nullptr);
  bool ext_ok = true;

  for (std::size_t i = 0; i < extensions_.size(); ++i) {
   std::shared_ptr<NativeRetailRasterExtensionState> state;
   RetailRendererRasterFailure ext_f;
   bool ok = false;
   rec->callout_active = true;
   try {
    ok = extensions_[i]->construct(*this, handle, state, ext_f);
    rec->callout_active = false;
   } catch (const detail::NativeMainReentry&) {
    rec->callout_active = false;
    throw;
   } catch (...) {
    rec->callout_active = false;
    ok = false;
   }
   if (!ok) {
    ext_ok = false;
    break;
   }
   rec->extensions[i] = std::move(state);
  }
  rec->extensions_complete = ext_ok;
 }

 rec->constructing = false;
 return handle;
}

bool NativeRetailRendererRasters::destroy_raster(
 const NativeRetailRasterHandle& handle,
 RetailRendererRasterFailure& f) {
 if (!admit(f)) return false;
 auto rec = record(handle, f);
 if (!rec) return false;

 if (rec->callout_active) {
  return reject(f, RetailRendererRasterError::reentrant, "0407 raster destruction reentrant callout");
 }

 if (!rec->destroying) {
  rec->destroying = true;
  rec->destroy_cursor = rec->extensions.size();
 }

 // 2B9C70: Reverse extension destruction BEFORE platform destroy.
 // Resumable cleanup cursor preserves progress on Main reentry.
 while (rec->destroy_cursor > 0) {
  const auto idx = rec->destroy_cursor - 1;
  rec->destroy_cursor = idx;
  if (idx < extensions_.size() && extensions_[idx]->destroy) {
   rec->callout_active = true;
   try {
    extensions_[idx]->destroy(*this, handle, rec->extensions[idx]);
    rec->callout_active = false;
   } catch (const detail::NativeMainReentry&) {
    rec->callout_active = false;
    rec->extensions[idx].reset();
    throw;
   } catch (...) {
    rec->callout_active = false;
   }
  }
  rec->extensions[idx].reset();
 }
 rec->extensions.clear();

 // Platform destroy callback (caller ignores platform return)
 if (!rec->backend_destroyed && backend_callbacks_.destroy) {
  rec->callout_active = true;
  try {
   backend_callbacks_.destroy(*this, handle);
   rec->callout_active = false;
   rec->backend_destroyed = true;
  } catch (const detail::NativeMainReentry&) {
   rec->callout_active = false;
   throw;
  } catch (...) {
   rec->callout_active = false;
   rec->backend_destroyed = true;
  }
 }

 // Release backing and GPU texture
 bool gpu_revocation_failed = false;
 if (!rec->is_subview) {
  if (rec->gpu_texture && gpu_device_) {
   graphics::NativeTextureFailure tf;
   if (!gpu_device_->revoke_image(rec->gpu_texture, tf)) {
    gpu_revocation_failed = true;
   }
   rec->gpu_texture.reset();
  }
  rec->pixel_data.clear();
  rec->palette_data.clear();

  // Invalidate any subviews aliasing this root raster
  for (const auto& other : records_) {
   if (other && other->is_subview && (other->root_alias == rec || other->parent.lock() == rec)) {
    other->gpu_texture.reset();
    other->root_alias.reset();
   }
  }
 } else {
  rec->root_alias.reset();
  rec->gpu_texture.reset();
 }

 rec->alive = false;
 auto it = std::find(records_.begin(), records_.end(), rec);
 if (it != records_.end()) records_.erase(it);

 if (gpu_revocation_failed) {
  return reject(f, RetailRendererRasterError::after_effect_unknown, "0407 GPU texture revocation failed");
 }

 return true;
}

std::optional<RetailRendererRasterObservation> NativeRetailRendererRasters::observe(
 const NativeRetailRasterHandle& handle,
 RetailRendererRasterFailure& f) const noexcept {
 if (!admit(f)) return {};
 auto rec = record(handle, f);
 if (!rec) return {};

 RetailRendererRasterObservation obs;
 obs.description = rec->description;
 obs.format = rec->format;
 obs.offset_x = rec->offset_x;
 obs.offset_y = rec->offset_y;
 obs.stride = rec->stride;
 obs.pixel_bytes = rec->pixel_bytes;
 obs.palette_bytes = rec->palette_bytes;
 obs.flags = rec->flags;
 obs.private_flags = rec->private_flags;
 obs.is_subview = rec->is_subview;
 obs.is_sentinel = rec->is_sentinel;
 obs.has_gpu_backing = bool(rec->gpu_texture);
 obs.locked_pixels = rec->locked_pixels;
 obs.locked_palette = rec->locked_palette;
 obs.destroying = rec->destroying;
 obs.extension_chain_completed = rec->extensions_complete;
 return obs;
}

std::optional<NativeRetailRasterHandle> NativeRetailRendererRasters::parent(
 const NativeRetailRasterHandle& handle,
 RetailRendererRasterFailure& f) const noexcept {
 if (!admit(f)) return {};
 auto rec = record(handle, f);
 if (!rec) return {};
 if (rec->is_sentinel) return NativeRetailRasterHandle{};
 auto parent_rec = rec->parent.lock();
 if (!parent_rec || !parent_rec->alive) {
  reject(f, RetailRendererRasterError::retired_raster, "0407 retired raster parent");
  return {};
 }
 return NativeRetailRasterHandle(handle.owner_, parent_rec);
}

bool NativeRetailRendererRasters::bind_display_target(
 const NativeRetailRasterHandle& handle,
 std::shared_ptr<NativeProductionDisplayService> display,
 RetailRendererRasterFailure& f) noexcept {
 if (!admit(f)) return false;
 auto rec = record(handle, f);
 if (!rec) return false;
 if (rec->is_sentinel || rec->is_subview || rec->parent.lock() != rec || rec->destroying ||
     (rec->description.type != 1 && rec->description.type != 2) ||
     (rec->flags & 7) != rec->description.type) {
  return reject(f, RetailRendererRasterError::display_binding, "0407 display requires base flag1/flag2 raster");
 }
 if (rec->display_assigned) {
  auto bound = rec->display.lock();
  if (!bound || bound->destination_identity() != rec->display_destination) {
   return reject(f, RetailRendererRasterError::stale_display, "0407 stale display binding");
  }
  if (!display || rec->display.owner_before(display) || display.owner_before(rec->display) || bound.get() != display.get()) {
   return reject(f, RetailRendererRasterError::already_backed, "0407 display already bound to another target");
  }
 }
 if (!display || !display->is_context_current() || !display->destination_identity() ||
     !display->target_width() || !display->target_height()) {
  return reject(f, RetailRendererRasterError::display_binding, "0407 display requires current nonzero host target");
 }
 rec->display = display;
 rec->display_destination = display->destination_identity();
 rec->display_assigned = true;
 return true;
}

std::shared_ptr<NativeProductionDisplayService> NativeRetailRendererRasters::display_target(
 const NativeRetailRasterHandle& handle,
 RetailRendererRasterFailure& f) const noexcept {
 if (!admit(f)) return {};
 auto rec = record(handle, f);
 if (!rec) return {};
 if (rec->is_sentinel || rec->is_subview || rec->parent.lock() != rec || rec->destroying ||
     (rec->description.type != 1 && rec->description.type != 2) ||
     (rec->flags & 7) != rec->description.type) {
  reject(f, RetailRendererRasterError::display_binding, "0407 display requires base flag1/flag2 raster");
  return {};
 }
 if (!rec->display_assigned) {
  reject(f, RetailRendererRasterError::missing_backing, "0407 display target never bound");
  return {};
 }
 auto display = rec->display.lock();
 if (!display || display->destination_identity() != rec->display_destination) {
  reject(f, RetailRendererRasterError::stale_display, "0407 stale display binding");
  return {};
 }
 if (!display->is_context_current() || !display->destination_identity() ||
     !display->target_width() || !display->target_height()) {
  reject(f, RetailRendererRasterError::display_binding, "0407 display requires current nonzero host target");
  return {};
 }
 return display;
}

std::optional<std::size_t> NativeRetailRendererRasters::live_rasters(
 RetailRendererRasterFailure& f) const noexcept {
 if (!admit(f)) return {};
 std::size_t count = 0;
 for (const auto& r : records_) {
  if (r && r->alive) ++count;
 }
 return count;
}

std::optional<NativeRetailRasterHandle> NativeRetailRendererRasters::current_target(
 RetailRendererRasterFailure& f) const noexcept {
 if (!admit(f)) return {};
 if (context_stack_.empty()) {
  reject(f, RetailRendererRasterError::context_underflow, "0407 context target stack empty");
  return {};
 }
 auto target = context_stack_.back();
 if (!target) {
  return NativeRetailRasterHandle{};
 }
 return NativeRetailRasterHandle(
  std::const_pointer_cast<NativeRetailRendererRasters>(shared_from_this()), target);
}

bool NativeRetailRendererRasters::push_context(
 const std::optional<NativeRetailRasterHandle>& target_opt,
 RetailRendererRasterFailure& f) {
 if (!admit(f)) return false;

 std::shared_ptr<NativeRetailRasterRecord> target_rec;
 if (target_opt && !target_opt->empty()) {
  target_rec = record(*target_opt, f);
  if (!target_rec) return false;
 }

 // 2B9CB0: Push publishes stack only AFTER context callback success
 if (backend_callbacks_.set_context) {
  bool ok = false;
  try {
   ok = backend_callbacks_.set_context(*this, target_opt, f);
  } catch (const detail::NativeMainReentry&) {
   throw;
  } catch (...) {
   return reject(f, RetailRendererRasterError::after_effect_unknown, "0407 context callback exception");
  }
  if (!ok) {
   if (f.kind == RetailRendererRasterError::none) {
    reject(f, RetailRendererRasterError::backend_refusal, "0407 context callback refusal");
   }
   return false;
  }
 }

 context_stack_.push_back(target_rec);
 return true;
}

std::optional<NativeRetailRasterHandle> NativeRetailRendererRasters::pop_context(
 RetailRendererRasterFailure& f) {
 if (!admit(f)) return {};

 // Named underflow contract: cannot pop below base sentinel
 if (context_stack_.size() <= 1) {
  reject(f, RetailRendererRasterError::context_underflow, "0407 context target stack underflow");
  return {};
 }

 // 2B9D50: Pop decrements BEFORE selecting previous target and does NOT restore index on failure.
 // Returns the popped target upon successful restoration of the previous target.
 auto popped = context_stack_.back();
 context_stack_.pop_back();
 auto previous = context_stack_.back();
 std::optional<NativeRetailRasterHandle> prev_handle;
 if (previous) {
  prev_handle = NativeRetailRasterHandle(shared_from_this(), previous);
 }

 if (backend_callbacks_.set_context) {
  bool ok = false;
  try {
   ok = backend_callbacks_.set_context(*this, prev_handle, f);
  } catch (const detail::NativeMainReentry&) {
   throw;
  } catch (...) {
   reject(f, RetailRendererRasterError::after_effect_unknown, "0407 context callback exception on pop");
   return {};
  }
  if (!ok) {
   if (f.kind == RetailRendererRasterError::none) {
    reject(f, RetailRendererRasterError::backend_refusal, "0407 context callback refusal on pop");
   }
   return {};
  }
 }

 return popped ? NativeRetailRasterHandle(shared_from_this(), popped) : NativeRetailRasterHandle{};
}

std::optional<std::size_t> NativeRetailRendererRasters::context_depth(
 RetailRendererRasterFailure& f) const noexcept {
 if (!admit(f)) return {};
 return context_stack_.size();
}

std::optional<NativeRetailRasterHandle> NativeRetailRendererRasters::create_subview(
 const NativeRetailRasterHandle& parent_handle,
 std::int32_t x,
 std::int32_t y,
 std::int32_t width,
 std::int32_t height,
 RetailRendererRasterFailure& f) {
 if (!admit(f)) return {};
 auto parent_rec = record(parent_handle, f);
 if (!parent_rec) return {};
 if (width < 0 || height < 0) {
  reject(f, RetailRendererRasterError::extent, "0407 subview negative dimensions");
  return {};
 }

 // 2B9DF0: Requires flags 0x80 (no allocation)
 // Dimensions and wrapping 16-bit parent + rectangle offsets store BEFORE callback
 const auto new_offset_x = static_cast<std::uint16_t>(parent_rec->offset_x + x);
 const auto new_offset_y = static_cast<std::uint16_t>(parent_rec->offset_y + y);

 std::shared_ptr<NativeRetailRasterRecord> subview_rec;
 try {
  subview_rec = std::make_shared<NativeRetailRasterRecord>();
  subview_rec->description.width = width;
  subview_rec->description.height = height;
  subview_rec->description.depth = parent_rec->description.depth;
  subview_rec->description.type = parent_rec->description.type;
  subview_rec->description.flags = 0x80;
  subview_rec->description.format = parent_rec->format;
  subview_rec->format = parent_rec->format;
  subview_rec->flags = 0x80;
  subview_rec->offset_x = new_offset_x;
  subview_rec->offset_y = new_offset_y;
  subview_rec->is_subview = true;
  subview_rec->alive = true;
  subview_rec->parent = subview_rec;
 } catch (const std::bad_alloc&) {
  reject(f, RetailRendererRasterError::allocation, "0407 subview allocation");
  return {};
 } catch (...) {
  reject(f, RetailRendererRasterError::native_exception, "0407 subview creation");
  return {};
 }

 NativeRetailRasterHandle subview_handle(shared_from_this(), subview_rec);

 // 2C9810: Backend callback
 if (backend_callbacks_.subraster) {
  bool ok = false;
  try {
   ok = backend_callbacks_.subraster(*this, subview_handle, parent_handle, f);
  } catch (const detail::NativeMainReentry&) {
   throw;
  } catch (...) {
   reject(f, RetailRendererRasterError::after_effect_unknown, "0407 subraster callback exception");
   return {};
  }
  if (!ok) {
   // Callback refusal: metadata stores remain updated on the record, but root alias is NOT published
   if (f.kind == RetailRendererRasterError::none) {
    reject(f, RetailRendererRasterError::backend_refusal, "0407 subraster callback refusal");
   }
   return {};
  }
 }

 // Only on success aliases the parent's ROOT; backend copies stride/depth/type/format
 auto root = parent_rec->root_alias ? parent_rec->root_alias : parent_rec;
 // 001A87C4/001A87CC: publish the parent's root identity only after success.
 subview_rec->parent = parent_rec->parent;
 subview_rec->root_alias = root;
 subview_rec->stride = root->stride;
 subview_rec->format = root->format;
 subview_rec->description.depth = root->description.depth;
 subview_rec->description.type = root->description.type;
 subview_rec->gpu_texture = root->gpu_texture;

 records_.push_back(subview_rec);
 return subview_handle;
}

std::optional<std::int32_t> NativeRetailRendererRasters::mip_count(
 const NativeRetailRasterHandle& handle,
 RetailRendererRasterFailure& f) {
 if (!admit(f)) return {};
 auto rec = record(handle, f);
 if (!rec) return {};

 // 2B9D90: Format bit 0x80 clear -> 1
 if ((rec->format & 0x80) == 0) {
  return 1;
 }

 if (backend_callbacks_.query_mips) {
  try {
   return backend_callbacks_.query_mips(*this, handle, f);
  } catch (const detail::NativeMainReentry&) {
   throw;
  } catch (...) {
   reject(f, RetailRendererRasterError::after_effect_unknown, "0407 query_mips callback exception");
   return -1;
  }
 }

 return -1;
}

std::optional<std::span<std::byte>> NativeRetailRendererRasters::lock_pixels(
 const NativeRetailRasterHandle& handle,
 RetailRendererRasterFailure& f) noexcept {
 if (!admit(f)) return {};
 auto rec = record(handle, f);
 if (!rec) return {};
 if (rec->locked_pixels) {
  reject(f, RetailRendererRasterError::lock_state, "0407 raster pixels already locked");
  return {};
 }

 auto root = rec->is_subview ? rec->root_alias : rec;
 if (!root || !root->alive) {
  reject(f, RetailRendererRasterError::missing_backing, "0407 missing raster backing");
  return {};
 }
 if (root->pixel_data.empty()) {
  reject(f, RetailRendererRasterError::missing_backing, "0407 empty raster pixel backing");
  return {};
 }

 std::size_t offset = 0;
 if (rec->is_subview) {
  const std::size_t bpp = (root->description.depth + 7) / 8;
  offset = (static_cast<std::size_t>(rec->offset_y) * root->stride) +
           (static_cast<std::size_t>(rec->offset_x) * bpp);
  if (offset >= root->pixel_data.size()) {
   reject(f, RetailRendererRasterError::subview_invalid, "0407 subview offset out of bounds");
   return {};
  }
 }

 rec->locked_pixels = true;
 return std::span<std::byte>(root->pixel_data.data() + offset, root->pixel_data.size() - offset);
}

bool NativeRetailRendererRasters::unlock_pixels(
 const NativeRetailRasterHandle& handle,
 RetailRendererRasterFailure& f) noexcept {
 if (!admit(f)) return false;
 auto rec = record(handle, f);
 if (!rec) return false;
 if (!rec->locked_pixels) {
  return reject(f, RetailRendererRasterError::lock_state, "0407 raster pixels not locked");
 }
 rec->locked_pixels = false;
 return true;
}

std::optional<std::span<std::byte>> NativeRetailRendererRasters::lock_palette(
 const NativeRetailRasterHandle& handle,
 RetailRendererRasterFailure& f) noexcept {
 if (!admit(f)) return {};
 auto rec = record(handle, f);
 if (!rec) return {};
 if (rec->locked_palette) {
  reject(f, RetailRendererRasterError::lock_state, "0407 raster palette already locked");
  return {};
 }

 auto root = rec->is_subview ? rec->root_alias : rec;
 if (!root || !root->alive) {
  reject(f, RetailRendererRasterError::missing_backing, "0407 missing raster backing for palette");
  return {};
 }
 if (root->palette_data.empty()) {
  reject(f, RetailRendererRasterError::missing_backing, "0407 raster has no palette backing");
  return {};
 }

 rec->locked_palette = true;
 return std::span<std::byte>(root->palette_data.data(), root->palette_data.size());
}

bool NativeRetailRendererRasters::unlock_palette(
 const NativeRetailRasterHandle& handle,
 RetailRendererRasterFailure& f) noexcept {
 if (!admit(f)) return false;
 auto rec = record(handle, f);
 if (!rec) return false;
 if (!rec->locked_palette) {
  return reject(f, RetailRendererRasterError::lock_state, "0407 raster palette not locked");
 }
 rec->locked_palette = false;
 return true;
}

bool NativeRetailRendererRasters::set_from_image(
 const NativeRetailRasterHandle& raster_handle,
 NativeRetailRendererImages& images,
 const NativeRetailImageHandle& image_handle,
 RetailRendererRasterFailure& f) {
 if (!admit(f)) return false;
 auto raster = record(raster_handle, f);
 if (!raster) return false;

 RetailRendererImageFailure img_f;
 auto img_obs_opt = images.observe(image_handle, img_f);
 if (!img_obs_opt) {
  return reject(f, RetailRendererRasterError::image_conversion, "0407 cannot observe source image");
 }
 const auto img_obs = *img_obs_opt;
 if (img_obs.destroying) {
  return reject(f, RetailRendererRasterError::image_conversion, "0407 source image is destroying");
 }

 // Depth validation: source image must be 32 or 24 bpp
 if (img_obs.description.depth != 32 && img_obs.description.depth != 24) {
  return reject(f, RetailRendererRasterError::unsupported_depth, "0407 unsupported source image depth");
 }

 // Destination raster depth and format validation
 if (raster->description.depth != 32) {
  return reject(f, RetailRendererRasterError::unsupported_depth, "0407 unsupported destination raster depth");
 }
 if (raster->format != 0x0500 && raster->format != 0x0600 &&
     (raster->format & 0x0F00) != 0x0500 && (raster->format & 0x0F00) != 0x0600 &&
     raster->format != 0) {
  return reject(f, RetailRendererRasterError::unsupported_format, "0407 unsupported raster format for image conversion");
 }

 const auto w = static_cast<std::size_t>(img_obs.description.width);
 const auto h = static_cast<std::size_t>(img_obs.description.height);
 if (w == 0 || h == 0) {
  return reject(f, RetailRendererRasterError::extent, "0407 zero image dimensions");
 }

 // Destination capacity validation: raster dimensions must be sufficient for image
 if (static_cast<std::size_t>(raster->description.width) < w ||
     static_cast<std::size_t>(raster->description.height) < h) {
  return reject(f, RetailRendererRasterError::extent, "0407 destination dimensions insufficient");
 }

 // For texture rasters (types 4 and 5), GPU device is required for connected conversion
 if ((raster->description.type == 4 || raster->description.type == 5) && !gpu_device_) {
  return reject(f, RetailRendererRasterError::missing_backing, "0407 GPU device required for texture raster upload");
 }

 const auto max_size = std::numeric_limits<std::size_t>::max();

 // Destination buffer capacity check
 const auto dst_stride = raster->stride;
 const std::size_t dst_bpp = 4; // depth 32
 if (dst_stride > 0 && h > 0 && (h - 1) > max_size / dst_stride) {
  return reject(f, RetailRendererRasterError::extent, "0407 destination stride overflow");
 }
 const auto required_dst_bytes = (h - 1) * dst_stride + (w * dst_bpp);

 // Acquire raster pixels lock
 auto raster_span_opt = lock_pixels(raster_handle, f);
 if (!raster_span_opt) return false;
 auto raster_span = *raster_span_opt;

 if (raster_span.size() < required_dst_bytes) {
  unlock_pixels(raster_handle, f);
  return reject(f, RetailRendererRasterError::extent, "0407 destination buffer capacity insufficient");
 }

 // Source backing bounds and stride check
 const std::size_t src_bpp = static_cast<std::size_t>(img_obs.description.depth) / 8;
 if (w > max_size / src_bpp) {
  unlock_pixels(raster_handle, f);
  return reject(f, RetailRendererRasterError::extent, "0407 source row width overflow");
 }
 const auto src_row_bytes = w * src_bpp;
 const auto src_stride = img_obs.stride.value_or(src_row_bytes);
 if (src_stride < src_row_bytes) {
  unlock_pixels(raster_handle, f);
  return reject(f, RetailRendererRasterError::extent, "0407 source stride smaller than row bytes");
 }
 if (src_stride > 0 && h > 0 && (h - 1) > max_size / src_stride) {
  unlock_pixels(raster_handle, f);
  return reject(f, RetailRendererRasterError::extent, "0407 source stride overflow");
 }
 const auto required_src_bytes = (h - 1) * src_stride + src_row_bytes;

 // Read image pixels
 std::vector<std::byte> img_bytes(img_obs.pixel_bytes);
 if (!images.read_bytes(image_handle, false, 0, img_bytes, img_f)) {
  unlock_pixels(raster_handle, f);
  return reject(f, RetailRendererRasterError::image_conversion, "0407 failed to read image bytes");
 }

 if (img_bytes.size() < required_src_bytes) {
  unlock_pixels(raster_handle, f);
  return reject(f, RetailRendererRasterError::extent, "0407 source image buffer insufficient");
 }

 // Buffer for GPU upload if device attached
 std::vector<std::uint8_t> gpu_rgba;
 if (gpu_device_ && (raster->description.type == 4 || raster->description.type == 5)) {
  if (w > max_size / (h * 4)) {
   unlock_pixels(raster_handle, f);
   return reject(f, RetailRendererRasterError::extent, "0407 GPU upload buffer overflow");
  }
  gpu_rgba.resize(w * h * 4);
 }

 // Convert rows based on format:
 // 2D04E0: Format 0x0500 maps RGBA -> word 0xAARRGGBB (on LE bytes: B G R A)
 // 2D04C0: Format 0x0600 maps RGBA -> word 0xFFRRGGBB (forces alpha 0xFF)
 for (std::size_t y = 0; y < h; ++y) {
  const auto* src_row = reinterpret_cast<const std::uint8_t*>(img_bytes.data() + (y * src_stride));
  auto* dst_row = reinterpret_cast<std::uint8_t*>(raster_span.data() + (y * dst_stride));

  for (std::size_t x = 0; x < w; ++x) {
   std::uint8_t r = 0, g = 0, b = 0, a = 255;
   if (img_obs.description.depth == 32) {
    r = src_row[x * 4 + 0];
    g = src_row[x * 4 + 1];
    b = src_row[x * 4 + 2];
    a = src_row[x * 4 + 3];
   } else if (img_obs.description.depth == 24) {
    r = src_row[x * 3 + 0];
    g = src_row[x * 3 + 1];
    b = src_row[x * 3 + 2];
   }

   if (raster->format == 0x0600) {
    a = 255;
   }

   // Destination depth 32
   // Little-endian word AARRGGBB -> byte 0: B, byte 1: G, byte 2: R, byte 3: A
   dst_row[x * 4 + 0] = b;
   dst_row[x * 4 + 1] = g;
   dst_row[x * 4 + 2] = r;
   dst_row[x * 4 + 3] = a;

   if (!gpu_rgba.empty()) {
    const auto idx = (y * w + x) * 4;
    gpu_rgba[idx + 0] = r;
    gpu_rgba[idx + 1] = g;
    gpu_rgba[idx + 2] = b;
    gpu_rgba[idx + 3] = a;
   }
  }
 }

 // GPU texture upload
 if (!gpu_rgba.empty()) {
  graphics::NativeTextureRgbaImage upload_img{
   static_cast<std::uint32_t>(w),
   static_cast<std::uint32_t>(h),
   gpu_rgba
  };
  auto upload = gpu_device_->upload_rgba_image(upload_img, raster->sampler);
  if (upload.failure.kind != graphics::NativeTextureError::none) {
   unlock_pixels(raster_handle, f);
   return reject(f, RetailRendererRasterError::image_conversion, "0407 native GL texture upload failed");
  }

  // Explicit retirement of prior backing before assigning new texture
  if (raster->gpu_texture) {
   graphics::NativeTextureFailure tf;
   if (!gpu_device_->revoke_image(raster->gpu_texture, tf)) {
    unlock_pixels(raster_handle, f);
    return reject(f, RetailRendererRasterError::after_effect_unknown, "0407 prior GPU texture revocation failed");
   }
   raster->gpu_texture.reset();
  }

  raster->gpu_texture = std::move(upload.texture);

  // Synchronize GPU texture to aliased subviews
  for (const auto& other : records_) {
   if (other && other->is_subview && (other->root_alias == raster || other->parent.lock() == raster)) {
    other->gpu_texture = raster->gpu_texture;
   }
  }
 }

 unlock_pixels(raster_handle, f);

 // 2C0EB0: Image conversion must succeed before gamma flag 2 propagates into raster private bit 0 (mask 0x01)
 if (img_obs.flags & 2) {
  raster->private_flags |= 1;
 }

 return true;
}

bool NativeRetailRendererRasters::attach_gpu_device(
 std::shared_ptr<graphics::NativeGlTextureUpload> device,
 RetailRendererRasterFailure& f) noexcept {
 if (!admit(f)) return false;
 if (gpu_device_ && gpu_device_ != device) {
  // Revoke live GPU textures from old device
  graphics::NativeTextureFailure tf;
  bool failed = false;
  for (const auto& r : records_) {
   if (r && r->gpu_texture && !r->is_subview) {
    if (!gpu_device_->revoke_image(r->gpu_texture, tf)) {
     failed = true;
    }
    r->gpu_texture.reset();
   }
  }
  for (const auto& r : records_) {
   if (r && r->is_subview) r->gpu_texture.reset();
  }
  if (failed) {
   return reject(f, RetailRendererRasterError::after_effect_unknown, "0407 prior GPU texture revocation failed on device replacement");
  }
 }
 gpu_device_ = std::move(device);
 return true;
}

bool NativeRetailRendererRasters::set_sampler(
 const NativeRetailRasterHandle& handle,
 graphics::NativeTextureSampler sampler,
 RetailRendererRasterFailure& f) noexcept {
 if (!admit(f)) return false;
 auto rec = record(handle, f);
 if (!rec) return false;
 rec->sampler = sampler;
 if (rec->gpu_texture && gpu_device_) {
  graphics::NativeTextureFailure tf;
  if (!gpu_device_->set_image_sampler(rec->gpu_texture, sampler, tf)) {
   return reject(f, RetailRendererRasterError::after_effect_unknown, "0407 failed to set GPU sampler");
  }
 }
 return true;
}

std::optional<graphics::NativeTextureSampler> NativeRetailRendererRasters::sampler(
 const NativeRetailRasterHandle& handle,
 RetailRendererRasterFailure& f) const noexcept {
 if (!admit(f)) return {};
 auto rec = record(handle, f);
 if (!rec) return {};
 return rec->sampler;
}

std::shared_ptr<const graphics::NativeGlTexture> NativeRetailRendererRasters::gpu_texture(
 const NativeRetailRasterHandle& handle,
 RetailRendererRasterFailure& f) const noexcept {
 if (!admit(f)) return {};
 auto rec = record(handle, f);
 if (!rec) return {};
 return rec->gpu_texture;
}

bool NativeRetailRendererCore::attach_gpu_device(
 std::shared_ptr<graphics::NativeGlTextureUpload> device,
 RetailRendererCoreFailure& f) noexcept {
 if (!access(f)) return false;
 if (!rasters_) {
  f.kind = RetailRendererCoreError::unbound_factory;
  f.operation = "0407 rasters module unbound for GPU device attachment";
  return false;
 }
 RetailRendererRasterFailure rf;
 if (!rasters_->attach_gpu_device(std::move(device), rf)) {
  f.kind = RetailRendererCoreError::after_effect_unknown;
  f.operation = rf.operation;
  return false;
 }
 return true;
}

std::shared_ptr<NativeRetailRendererRasters> NativeRetailRendererCore::rasters(RetailRendererCoreFailure& f) const noexcept {
 if (!access(f)) return {};
 if (!rasters_) {
  f.kind = RetailRendererCoreError::unbound_factory;
  f.operation = "0407 actual raster resource required";
  return {};
 }
 return rasters_;
}

std::optional<std::uint32_t> NativeRetailRendererCore::module_0407_count(RetailRendererCoreFailure& f) const noexcept {
 if (!access(f)) return {};
 return rasters_ ? 1u : 0u;
}

bool NativeRetailRendererCore::register_raster_extension(
 std::shared_ptr<const RetailRendererRasterExtension> extension,
 RetailRendererCoreFailure& f) noexcept {
 if (!access(f)) return false;
 if (sealed_ || rasters_) {
  f.kind = RetailRendererCoreError::registration_closed;
  f.operation = "0407 raster extension definitions sealed";
  return false;
 }
 if (!extension || !extension->construct || !extension->destroy) {
  f.kind = RetailRendererCoreError::unbound_factory;
  f.operation = "0407 real native extension callouts required";
  return false;
 }
 for (const auto& x : raster_extensions_) if (x->id == extension->id) return true;
 try {
  raster_extensions_.push_back(std::make_shared<const RetailRendererRasterExtension>(*extension));
  return true;
 } catch (...) {
  f.kind = RetailRendererCoreError::allocation;
  f.operation = "0407 native raster extension definition";
  return false;
 }
}

namespace renderer_required {

bool construct_core_plugin_0407(NativeRetailRendererCore& core, RetailRendererCoreFailure& f) {
 if (!core.access(f)) return false;
 if (core.rasters_) {
  f.kind = RetailRendererCoreError::reentrant;
  f.operation = "0407 live raster resource already published";
  return false;
 }
 RetailRendererRasterFailure detail;
 auto owner = NativeRetailRendererRasters::create(core, detail);
 if (!owner) {
  f.kind = (detail.kind == RetailRendererRasterError::allocation ||
            detail.allocator.heap.kind == memory::NativeHeapError::allocation_failed)
   ? RetailRendererCoreError::allocation
   : RetailRendererCoreError::allocator;
  f.operation = detail.operation;
  f.allocator = detail.allocator;
  return false;
 }
 core.rasters_ = std::move(owner);
 core.module_0407_count_.fetch_add(1, std::memory_order_relaxed);
 return true;
}

void destroy_core_plugin_0407(NativeRetailRendererCore& core) noexcept {
 if (core.rasters_) {
  core.rasters_->retire();
  core.rasters_.reset();
  core.module_0407_count_.fetch_sub(1, std::memory_order_relaxed);
 }
}

} // namespace renderer_required

} // namespace mka::retail
