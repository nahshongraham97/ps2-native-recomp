#include "mka/retail_renderer_rasters.hpp"
#include "mka/retail_display_service.hpp"
#include "mka/retail_renderer_images.hpp"
#include "mka/retail_renderer_transforms.hpp"
#include "mka/retail_renderer_matrix.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_scene_bootstrap_required.hpp"
#include "mka/retail_task_domain.hpp"
#include "native_retail_startup.hpp"
#include "../runtime/src/native/retail_main_control.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <new>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#include <GL/gl.h>
#include "mka/movie_gl_device.hpp"
#endif

extern "C" int SDL_InitSubSystem(unsigned) { std::abort(); }

namespace {
thread_local int fail_after = -1;
}

void* operator new(std::size_t n) {
 if (fail_after == 0) throw std::bad_alloc();
 if (fail_after > 0) --fail_after;
 if (auto p = std::malloc(n ? n : 1)) return p;
 throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

using namespace mka::retail;
using namespace mka::graphics;
using namespace mka::video;

namespace {

void check(bool v, const char* why) {
 if (!v) throw std::runtime_error(why);
}

enum class Later { refuse, transfer, allow };
Later later{Later::refuse};
NativeRetailMainState* transfer_main{};
std::shared_ptr<NativeRetailRendererRasters> captured_rasters;
NativeRetailRasterHandle captured_raster;
unsigned suffix_calls{};

bool fixture_construct(unsigned id, NativeRetailRendererCore& c, RetailRendererCoreFailure&) {
 if (id == 0x409 && later != Later::allow) {
  RetailRendererCoreFailure cf;
  captured_rasters = c.rasters(cf);
  check(bool(captured_rasters), "actual 0407 before later 0409 fixture");
  RetailRendererRasterFailure rf;
  auto h = captured_rasters->create_raster({4, 4, 32, 4, 4, 0x0500}, rf);
  check(bool(h), "actual raster before later 0409 fixture");
  captured_raster = *h;
  if (later == Later::transfer) {
   reenter_native_retail_main(*transfer_main, 91);
  }
  return false;
 }
 if (later != Later::allow) std::abort();
 return true;
}

#ifdef _WIN32
struct HiddenGl {
 HWND window{nullptr};
 HDC dc{nullptr};
 HGLRC context{nullptr};
 std::shared_ptr<mka::video::NativeMovieGpuRetirementOwner> retirement;
 std::shared_ptr<mka::video::NativeMovieGlDevice> device;

 HiddenGl() {
  WNDCLASSW wc{};
  wc.style = CS_OWNDC;
  wc.lpfnWndProc = DefWindowProcW;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.lpszClassName = L"NativeRetailRendererRastersGlTest";
  check(RegisterClassW(&wc) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS, "fixture window class");
  window = CreateWindowExW(0, wc.lpszClassName, L"", WS_POPUP | WS_DISABLED, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr);
  check(window && !IsWindowVisible(window), "never-visible native test window");
  dc = GetDC(window);
  PIXELFORMATDESCRIPTOR p{};
  p.nSize = sizeof(p);
  p.nVersion = 1;
  p.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
  p.iPixelType = PFD_TYPE_RGBA;
  p.cColorBits = 32;
  p.cAlphaBits = 8;
  p.iLayerType = PFD_MAIN_PLANE;
  const auto format = ChoosePixelFormat(dc, &p);
  check(format && SetPixelFormat(dc, format, &p), "test native pixel format");
  context = wglCreateContext(dc);
  check(context && wglMakeCurrent(dc, context), "test current WGL context");
  mka::video::MovieGpuError error;
  retirement = mka::video::NativeMovieGpuRetirementOwner::create(error);
  check(bool(retirement) && error == mka::video::MovieGpuError::none, "actual native retirement owner");
  device = mka::video::NativeMovieGlDevice::attach_current_context(window, 1, retirement, error);
  check(bool(device) && error == mka::video::MovieGpuError::none, "actual native GPU owner");
 }

 ~HiddenGl() {
  if (device) {
   for (;;) {
    const auto result = device->shutdown();
    if (result == mka::video::MovieGpuError::none) break;
    if (result != mka::video::MovieGpuError::pending) std::abort();
    glFinish();
   }
  }
  if (retirement) {
   check(retirement->poll().state == mka::video::MovieFenceState::complete, "real context retirement completed");
  }
  check(!IsWindowVisible(window), "fixture was never visible");
  device.reset();
  retirement.reset();
  if (wglGetCurrentContext() == context) wglMakeCurrent(nullptr, nullptr);
  if (context) wglDeleteContext(context);
  if (dc) ReleaseDC(window, dc);
  if (window) DestroyWindow(window);
 }
};
#endif

struct Fixture {
 NativeRetailStartupState startup;
 mka::input::ControllerInput input;
 NativeRetailMainState main;
 StartupFailure sf;
 RetailRendererCoreFailure f;
 RetailRendererAllocatorFailure af;
 std::shared_ptr<NativeRetailRendererAllocatorService> allocator;

 explicit Fixture(const std::filesystem::path& root)
  : startup(open(root), {}),
    main(startup, input) {
  check(initialize_native_retail_game_session(startup, sf) &&
        initialize_retail_memory_owners(startup, sf), "actual Session/heaps");
  allocator = NativeRetailRendererAllocatorService::create(*startup.memory_owners, af);
  check(bool(allocator), "actual native Permanent service");
 }

 static mka::content::NativeContentVolume open(const std::filesystem::path& root) {
  mka::content::ContentVolumeFailure cf;
  auto v = mka::content::NativeContentVolume::open(root, cf);
  check(bool(v), "actual native content");
  return std::move(*v);
 }

 std::shared_ptr<NativeRetailRendererCore> core() {
  auto c = NativeRetailRendererCore::for_session(startup.game_session, allocator, startup.content, f);
  check(bool(c), f.operation);
  return c;
 }

 void prepare() {
  auto c = core();
  check(c->initialize_core(f).value_or(false), "actual CoreInit");
 }

 std::shared_ptr<NativeRetailRendererRasters> module() {
  prepare();
  auto c = core();
  check(renderer_required::construct_core_plugin_0407(*c, f), "ACTUAL 0407 factory");
  return c->rasters(f);
 }

 std::size_t live() {
  RetailMemoryOwnerFailure mf;
  auto o = startup.memory_owners->observe(RetailMemoryRole::permanent, mf);
  check(bool(o), "actual Permanent observation");
  return o->heap.actual_live_allocations;
 }
};

} // namespace

namespace mka::retail::renderer_required {
#define OTHER(ID, V) \
 bool construct_core_plugin_##ID(NativeRetailRendererCore& c, RetailRendererCoreFailure& f) { \
  return fixture_construct(V, c, f); \
 } \
 void destroy_core_plugin_##ID(NativeRetailRendererCore&) noexcept { \
  if (later != Later::allow) std::abort(); \
 }
OTHER(0409, 0x409)
OTHER(040b, 0x40b)
// Actual0408/0412/040A services are now linked dependencies. The
// refusal/transfer fixture remains at0409; all original assertions remain.
#undef OTHER

void transform_point_exact(RetailRendererPoint3&, const RetailRendererPoint3&, const RetailRendererAffine&) { std::abort(); }
void transform_points_exact(std::span<RetailRendererPoint3>, std::span<const RetailRendererPoint3>, const RetailRendererAffine&) { std::abort(); }
void transform_vector_exact(RetailRendererPoint3&, const RetailRendererPoint3&, const RetailRendererAffine&) { std::abort(); }
void transform_vectors_exact(std::span<RetailRendererPoint3>, std::span<const RetailRendererPoint3>, const RetailRendererAffine&) { std::abort(); }
bool multiply_affines_19f680(RetailRendererAffine&, const RetailRendererAffine&, const RetailRendererAffine&, RetailRendererMatrixFailure&) { std::abort(); }
bool classify_matrix_1a0240(RetailRendererMatrixValue&, const RetailRendererMatrixTolerances&, RetailRendererMatrixFailure&) { std::abort(); }
bool invert_nonidentity_matrix_1a07a0(RetailRendererMatrixValue&, const RetailRendererMatrixValue&, RetailRendererMatrixFailure&) { std::abort(); }
} // namespace mka::retail::renderer_required

namespace mka::retail::scene_bootstrap_required {
RetailStartupGate continue_first_scene_at_225d40(
 NativeRetailMainState&,
 std::shared_ptr<NativeRetailRendererCore> c,
 StartupFailure& f) {
 ++suffix_calls;
 RetailRendererCoreFailure cf;
 check(!c->construct_registered(cf), "explicit later 0409 refusal after actual 0407");
 f = {StartupError::memory_owners, "test-only 0409/full renderer suffix refusal"};
 return RetailStartupGate::native_failure;
}
bool continue_scene_reset_at_125d60(NativeRetailMainState&, StartupFailure&) { std::abort(); }
} // namespace mka::retail::scene_bootstrap_required

int main() {
 try {
  const std::filesystem::path root = ".";

  // 1. Default Sentinel & Initial Context Stack
  {
   Fixture fix(root);
   auto rasters = fix.module();
   RetailRendererRasterFailure rf;

   auto sentinel = rasters->default_sentinel();
   check(!sentinel.empty(), "default sentinel handle not empty");

   auto obs = rasters->observe(sentinel, rf);
   check(obs.has_value(), "observe default sentinel");
   check(obs->description.width == 0 && obs->description.height == 0 && obs->description.depth == 0, "sentinel zero dimensions");
   check(obs->flags == 0x80, "sentinel flags 0x80");
   check(obs->is_sentinel, "sentinel is marked sentinel");
   check(!obs->has_gpu_backing, "sentinel has no GPU readiness");
   auto sentinel_parent = rasters->parent(sentinel, rf);
   check(sentinel_parent && sentinel_parent->empty(), "host context sentinel has no raster parent");

   // Context stack initial target is default sentinel
   auto cur = rasters->current_target(rf);
   check(cur.has_value() && cur->same_identity(sentinel), "initial target is sentinel");
   check(rasters->context_depth(rf) == 1, "initial context depth is 1");
  }

  // 2. Raster Creation & Format Normalization (2C8BD0)
  {
   Fixture fix(root);
   auto rasters = fix.module();
   RetailRendererRasterFailure rf;

   // Type 4 texture with format 0: normalizes to 0x0500 and depth 32
   RetailRendererRasterDescription desc;
   desc.width = 16;
   desc.height = 8;
   desc.flags = 4; // type = 4
   auto r1 = rasters->create_raster(desc, rf);
   check(r1.has_value(), "create normalized texture raster");
   auto obs1 = rasters->observe(*r1, rf);
   check(obs1.has_value(), "observe r1");
   check(obs1->format == 0x0500, "type 4 normalized format 0x0500");
   check(obs1->description.depth == 32, "type 4 normalized depth 32");
   check(obs1->stride == 16 * 4, "stride is 4-byte aligned");
   check(obs1->pixel_bytes == 16 * 4 * 8, "pixel bytes computed");

   // Type 4 texture with nonzero incoming depth (e.g. 16): unconditionally normalizes to depth 32
   RetailRendererRasterDescription desc16;
   desc16.width = 16;
   desc16.height = 8;
   desc16.depth = 16;
   desc16.flags = 4;
   auto r16 = rasters->create_raster(desc16, rf);
   check(r16.has_value(), "create type 4 with nonzero incoming depth");
   auto obs16 = rasters->observe(*r16, rf);
   check(obs16->format == 0x0500, "type 4 normalized format 0x0500");
   check(obs16->description.depth == 32, "type 4 unconditionally normalizes to depth 32");
   check(rasters->destroy_raster(*r16, rf), "destroy r16");

   // Camera target (type 5, flags 5, depth 32)
   RetailRendererRasterDescription cam_desc;
   cam_desc.width = 640;
   cam_desc.height = 448;
   cam_desc.flags = 5;
   cam_desc.depth = 32;
   auto r_cam = rasters->create_raster(cam_desc, rf);
   check(r_cam.has_value(), "camera target raster");
   auto obs_cam = rasters->observe(*r_cam, rf);
   check(obs_cam->description.type == 5, "camera target type 5");

   // Depth buffer (type 1, flags 1, depth 0)
   RetailRendererRasterDescription depth_desc;
   depth_desc.width = 640;
   depth_desc.height = 448;
   depth_desc.flags = 1;
   depth_desc.depth = 0;
   auto r_depth = rasters->create_raster(depth_desc, rf);
   check(r_depth.has_value(), "depth raster");
   auto obs_depth = rasters->observe(*r_depth, rf);
   check(obs_depth->description.type == 1, "depth buffer type 1");

   check(rasters->live_rasters(rf) == 3, "three live rasters");
   check(rasters->destroy_raster(*r1, rf), "destroy r1");
   check(rasters->live_rasters(rf) == 2, "two live rasters");
  }

  // 3. Context Target Stack Push / Pop (2B9CB0 / 2B9D50)
  {
   Fixture fix(root);
   auto rasters = fix.module();
   RetailRendererRasterFailure rf;

   auto sentinel = rasters->default_sentinel();
   auto r1 = rasters->create_raster({8, 8, 32, 4, 4, 0x0500}, rf);
   auto r2 = rasters->create_raster({8, 8, 32, 4, 4, 0x0500}, rf);

   bool callback_invoked = false;
   bool callback_refuse = false;
   std::optional<NativeRetailRasterHandle> callback_received_target;

   RetailRendererRasterBackendCallbacks cb;
   cb.set_context = [&](NativeRetailRendererRasters&, const std::optional<NativeRetailRasterHandle>& target, RetailRendererRasterFailure& f) {
    callback_invoked = true;
    callback_received_target = target;
    if (callback_refuse) {
     f.kind = RetailRendererRasterError::backend_refusal;
     f.operation = "simulated set_context refusal";
     return false;
    }
    return true;
   };
   rasters->set_backend_callbacks(cb);

   // Refusal on push: stack remains unchanged
   callback_refuse = true;
   callback_invoked = false;
   check(!rasters->push_context(r1, rf), "push refused");
   check(callback_invoked, "callback was invoked on push");
   check(rasters->context_depth(rf) == 1, "stack depth unchanged on push refusal");
   check(rasters->current_target(rf)->same_identity(sentinel), "current target still sentinel");

   // Success on push: publishes target
   callback_refuse = false;
   callback_invoked = false;
   check(rasters->push_context(r1, rf), "push succeeded");
   check(callback_invoked, "callback invoked");
   check(callback_received_target->same_identity(*r1), "callback received r1");
   check(rasters->context_depth(rf) == 2, "depth is 2");
   check(rasters->current_target(rf)->same_identity(*r1), "current target is r1");

   // Successful push of null target returns null after updating stack
   callback_invoked = false;
   check(rasters->push_context(std::nullopt, rf), "push null succeeded");
   check(callback_invoked, "callback invoked for null push");
   check(!callback_received_target.has_value() || callback_received_target->empty(), "callback received null/empty");
   check(rasters->context_depth(rf) == 3, "depth is 3");
   auto cur_null = rasters->current_target(rf);
   check(!cur_null.has_value() || cur_null->empty(), "current target is null");

   // Pop context: decrements stack index BEFORE selecting previous target
   // Pop refusal does NOT restore index!
   callback_refuse = true;
   callback_invoked = false;
   auto pop_res = rasters->pop_context(rf);
   check(!pop_res.has_value(), "pop callback refused");
   check(rasters->context_depth(rf) == 2, "stack index remained decremented despite callback failure");

   // Successful pop returns the popped target (r1), while current_target becomes previous (sentinel)
   callback_refuse = false;
   pop_res = rasters->pop_context(rf);
   check(pop_res.has_value() && pop_res->same_identity(*r1), "pop returned popped target r1");
   check(rasters->current_target(rf)->same_identity(sentinel), "current target after pop is restored previous sentinel");
   check(rasters->context_depth(rf) == 1, "stack back to base depth 1");

   // Underflow test: cannot pop below base sentinel
   check(!rasters->pop_context(rf) && rf.kind == RetailRendererRasterError::context_underflow, "underflow refused by name");
   check(rasters->context_depth(rf) == 1, "stack still has sentinel");
  }

  // Source 001A8880 parent identity is independent of CPU/GPU storage readiness.
  {
   Fixture fix(root);
   auto rasters = fix.module();
   RetailRendererRasterFailure rf;
   int create_callbacks = 0;
   RetailRendererRasterBackendCallbacks cb;
   cb.create = [&](NativeRetailRendererRasters& m, NativeRetailRasterHandle h,
                   std::uint32_t, RetailRendererRasterFailure& f) {
    auto p = m.parent(h, f);
    check(p && p->same_identity(h), "self parent published before platform create callback");
    ++create_callbacks;
    return true;
   };
   rasters->set_backend_callbacks(cb);
   auto color = rasters->create_raster({640, 448, 0, 0, 2, 0}, rf);
   auto depth = rasters->create_raster({640, 448, 0, 0, 1, 0}, rf);
   check(color && depth && create_callbacks == 2, "source flag2/flag1 logical raster creation");
   for (const auto h : {*color, *depth}) {
    auto p = rasters->parent(h, rf);
    auto obs = rasters->observe(h, rf);
    check(p && p->same_identity(h), "ordinary raster parent is its own typed identity");
    check(obs && obs->pixel_bytes == 0 && !obs->has_gpu_backing,
          "self parent does not assert pixel or display backing");
   }
   Fixture foreign(root);
   auto foreign_rasters = foreign.module();
   check(!foreign_rasters->parent(*color, rf) && rf.kind == RetailRendererRasterError::foreign_owner,
         "parent rejects foreign Session/module raster");
   check(!rasters->parent({}, rf) && rf.kind == RetailRendererRasterError::invalid_handle,
         "parent rejects empty raster");
   check(!rasters->parent(NativeRetailRasterHandle::test_handle(), rf) &&
         rf.kind == RetailRendererRasterError::foreign_owner, "parent rejects unowned test identity");
   bool wrong_thread = false;
   std::thread other([&] {
    RetailRendererRasterFailure f;
    wrong_thread = !rasters->parent(*color, f) && f.kind == RetailRendererRasterError::wrong_thread;
   });
   other.join();
   check(wrong_thread, "parent rejects wrong thread");
   check(rasters->destroy_raster(*color, rf), "destroy self-parented logical raster");
   check(!rasters->parent(*color, rf) && rf.kind == RetailRendererRasterError::retired_raster,
         "parent rejects destroyed raster identity");
   renderer_required::destroy_core_plugin_0407(*fix.core());
   check(!rasters->parent(*depth, rf) && rf.kind == RetailRendererRasterError::retired,
         "parent rejects retired raster module");
  }

  // Explicit native display capability; these descriptors borrow, rather than own, host storage.
#ifdef _WIN32
  {
   Fixture fix(root);
   auto rasters = fix.module();
   RetailRendererRasterFailure rf;
   DisplayServiceFailure df;
   auto a = NativeOffscreenDisplayService::create(32, 24, df);
   auto b = NativeOffscreenDisplayService::create(32, 24, df);
   check(a && b, "two actual native hidden display services");
   auto color = rasters->create_raster({640, 448, 0, 0, 2, 0}, rf);
   auto depth = rasters->create_raster({640, 448, 0, 0, 1, 0}, rf);
   auto spare = rasters->create_raster({640, 448, 0, 0, 2, 0}, rf);
   auto texture = rasters->create_raster({4, 4, 32, 0, 5, 0}, rf);
   check(color && depth && spare && texture, "actual logical display and texture descriptors");
   check(!rasters->display_target(*color, rf) && rf.kind == RetailRendererRasterError::missing_backing,
         "never-bound display fails by name");
   check(!rasters->bind_display_target(*color, {}, rf) && rf.kind == RetailRendererRasterError::display_binding,
         "null display does not publish a backing");
   check(!rasters->bind_display_target(*color, a, rf) && rf.kind == RetailRendererRasterError::display_binding,
         "display binding requires current context");
   check(a->acquire_context(df), "acquire first native display");
   check(rasters->bind_display_target(*color, a, rf) && rasters->bind_display_target(*depth, a, rf),
         "color and depth descriptors explicitly borrow same host target");
   check(rasters->bind_display_target(*color, a, rf), "same live binding is idempotent");
   check(rasters->display_target(*color, rf) == a && rasters->display_target(*depth, rf) == a,
         "backing getter returns actual native service");
   check(!rasters->bind_display_target(*texture, a, rf) && rf.kind == RetailRendererRasterError::display_binding,
         "type5 texture is outside display-target admission");
   auto sub = rasters->create_subview(*color, 0, 0, 16, 16, rf);
   check(sub && !rasters->bind_display_target(*sub, a, rf) && rf.kind == RetailRendererRasterError::display_binding,
         "display binding rejects subview alias");
   Fixture foreign(root);
   auto foreign_rasters = foreign.module();
   check(!foreign_rasters->bind_display_target(*color, a, rf) && rf.kind == RetailRendererRasterError::foreign_owner,
         "foreign Session cannot bind another module raster");
   auto foreign_color = foreign_rasters->create_raster({640, 448, 0, 0, 2, 0}, rf);
   check(foreign_color && foreign_rasters->bind_display_target(*foreign_color, a, rf),
         "host display may be explicitly shared across Sessions");
   bool wrong_thread = false;
   std::thread other([&] {
    RetailRendererRasterFailure f;
    wrong_thread = !rasters->bind_display_target(*color, a, f) && f.kind == RetailRendererRasterError::wrong_thread;
   });
   other.join();
   check(wrong_thread, "display binding rejects wrong thread before publication");
   check(b->acquire_context(df), "acquire second native display");
   check(!rasters->bind_display_target(*color, b, rf) && rf.kind == RetailRendererRasterError::already_backed,
         "live display binding cannot switch services");
   check(!rasters->display_target(*color, rf) && rf.kind == RetailRendererRasterError::display_binding,
         "bound non-current context is not ready");
   check(rasters->bind_display_target(*spare, b, rf), "bind second service to separate descriptor");
   std::weak_ptr<NativeProductionDisplayService> expired = b;
   b.reset();
   check(expired.expired(), "raster descriptor does not retain display lifetime");
   check(!rasters->display_target(*spare, rf) && rf.kind == RetailRendererRasterError::stale_display,
         "expired assigned backing differs from never-bound");
   check(a->acquire_context(df), "restore first native display");
   check(!rasters->bind_display_target(*spare, a, rf) && rf.kind == RetailRendererRasterError::stale_display,
         "expired generation cannot be silently rebound");
   check(rasters->display_target(*color, rf) == a, "refused rebind retained original backing");
   check(rasters->destroy_raster(*color, rf) && a->is_context_current(),
         "raster destruction leaves borrowed host display live");
   check(!rasters->display_target(*color, rf) && rf.kind == RetailRendererRasterError::retired_raster,
         "destroyed descriptor cannot retrieve display");
   check(rasters->display_target(*depth, rf) == a, "other display alias remains usable");
   renderer_required::destroy_core_plugin_0407(*fix.core());
   check(!rasters->display_target(*depth, rf) && rf.kind == RetailRendererRasterError::retired,
         "retired module cannot retrieve backing");
   check(a->release_context(df), "release actual display context");
  }
#endif

  // 4. Subview & 16-bit Offset Wrapping (2B9DF0 / 2C9810)
  {
   Fixture fix(root);
   auto rasters = fix.module();
   RetailRendererRasterFailure rf;

   auto parent = rasters->create_raster({64, 64, 32, 4, 4, 0x0500}, rf);
   check(parent.has_value(), "parent raster");

   // Custom subraster callback tracking
   bool sub_invoked = false;
   bool sub_refusal = false;
   std::uint16_t observed_sub_x = 0;
   std::uint16_t observed_sub_y = 0;

   RetailRendererRasterBackendCallbacks cb;
   cb.subraster = [&](NativeRetailRendererRasters& m, NativeRetailRasterHandle subview, NativeRetailRasterHandle, RetailRendererRasterFailure& f) {
    sub_invoked = true;
    auto obs = m.observe(subview, f);
    if (obs) {
     observed_sub_x = obs->offset_x;
     observed_sub_y = obs->offset_y;
    }
    if (sub_refusal) {
     f.kind = RetailRendererRasterError::backend_refusal;
     f.operation = "simulated subraster refusal";
     return false;
    }
    return true;
   };
   rasters->set_backend_callbacks(cb);

   // Wrapping test: parent offset (0xFFFF, 0x7FFF) + rect (1, 1) -> (0x0000, 0x8000)
   // First create subview 1 with offset (0xFFFF, 0x7FFF)
   auto sub1 = rasters->create_subview(*parent, 0xFFFF, 0x7FFF, 32, 32, rf);
   check(sub1.has_value(), "sub1 created");
   auto obs_sub1 = rasters->observe(*sub1, rf);
   auto parent_sub1 = rasters->parent(*sub1, rf);
   check(parent_sub1 && parent_sub1->same_identity(*parent), "subview parent names root raster");
   check(obs_sub1->offset_x == 0xFFFF && obs_sub1->offset_y == 0x7FFF, "sub1 offsets");

   // Now create subview 2 from sub1 with rect (1, 1)
   // Wrapping arithmetic: (0xFFFF + 1) = 0x0000, (0x7FFF + 1) = 0x8000
   auto sub2 = rasters->create_subview(*sub1, 1, 1, 16, 16, rf);
   check(sub2.has_value(), "sub2 created");
   auto obs_sub2 = rasters->observe(*sub2, rf);
   auto parent_sub2 = rasters->parent(*sub2, rf);
   check(parent_sub2 && parent_sub2->same_identity(*parent), "nested subview parent names root, not immediate view");
   check(obs_sub2->offset_x == 0x0000 && obs_sub2->offset_y == 0x8000, "wrapping 16-bit offset: (0x0000, 0x8000)");
   check(obs_sub2->is_subview, "sub2 is marked subview");
   check(obs_sub2->flags == 0x80, "sub2 has flag 0x80");
   check(obs_sub2->description.width == 16 && obs_sub2->description.height == 16, "sub2 dimensions");
   check(obs_sub2->stride == obs_sub1->stride, "sub2 inherits parent stride");

   // Backend refusal: metadata stores remain updated on the record, but root alias is NOT published
   sub_refusal = true;
   sub_invoked = false;
   auto sub_fail = rasters->create_subview(*parent, 5, 5, 10, 10, rf);
   check(!sub_fail.has_value() && sub_invoked, "subview creation failed on backend refusal");
   check(observed_sub_x == 5 && observed_sub_y == 5, "offsets stored before callback");

   // Destroying subview does NOT free parent root storage
   check(rasters->destroy_raster(*sub2, rf), "destroy sub2");
   auto parent_obs = rasters->observe(*parent, rf);
   check(parent_obs.has_value() && parent_obs->pixel_bytes > 0, "parent root storage still valid");
   check(rasters->destroy_raster(*parent, rf), "destroy root while child view identity remains");
   check(!rasters->parent(*sub1, rf) && rf.kind == RetailRendererRasterError::retired_raster,
         "live subview cannot publish a destroyed root identity");
  }

  // 5. Raster Extensions (2B9E50 / 2B9C70)
  {
   Fixture fix(root);
   auto core = fix.core();
   check(core->initialize_core(fix.f).value_or(false), "core init");

   std::vector<int> ext_events;
   auto ext1 = std::make_shared<RetailRendererRasterExtension>();
   ext1->id = 0x101;
   ext1->construct = [&](NativeRetailRendererRasters&, NativeRetailRasterHandle, std::shared_ptr<NativeRetailRasterExtensionState>&, RetailRendererRasterFailure&) {
    ext_events.push_back(0x101);
    return true;
   };
   ext1->destroy = [&](NativeRetailRendererRasters&, NativeRetailRasterHandle, std::shared_ptr<NativeRetailRasterExtensionState>&) {
    ext_events.push_back(-0x101);
   };

   auto ext2 = std::make_shared<RetailRendererRasterExtension>();
   ext2->id = 0x102;
   bool ext2_refuse = false;
   ext2->construct = [&](NativeRetailRendererRasters&, NativeRetailRasterHandle, std::shared_ptr<NativeRetailRasterExtensionState>&, RetailRendererRasterFailure&) {
    ext_events.push_back(0x102);
    return !ext2_refuse;
   };
   ext2->destroy = [&](NativeRetailRendererRasters&, NativeRetailRasterHandle, std::shared_ptr<NativeRetailRasterExtensionState>&) {
    ext_events.push_back(-0x102);
   };

   check(core->register_raster_extension(ext1, fix.f), "reg ext1");
   check(core->register_raster_extension(ext2, fix.f), "reg ext2");

   check(renderer_required::construct_core_plugin_0407(*core, fix.f), "construct 0407 with extensions");
   auto rasters = core->rasters(fix.f);
   check(bool(rasters), "rasters module");

   // Case A: ext2 refuses.
   // 2B9E50 ignores extension refusal return. States are retained for full definition-chain destruction.
   ext2_refuse = true;
   ext_events.clear();
   RetailRendererRasterFailure rf;
   auto r_ext_fail = rasters->create_raster({8, 8, 32, 4, 4, 0x0500}, rf);
   check(r_ext_fail.has_value(), "raster creation succeeds even when extension refuses");
   check(ext_events.size() == 2 && ext_events[0] == 0x101 && ext_events[1] == 0x102, "extensions attempted forward in definition order");
   auto obs_ext_fail = rasters->observe(*r_ext_fail, rf);
   check(!obs_ext_fail->extension_chain_completed, "extension chain marked incomplete");

   // Subsequent destruction traverses the full reverse definition chain
   ext_events.clear();
   check(rasters->destroy_raster(*r_ext_fail, rf), "destroy raster with refused extension");
   check(ext_events.size() == 2 && ext_events[0] == -0x102 && ext_events[1] == -0x101,
         "subsequent destruction traverses full reverse definition chain");

   // Case B: all extensions succeed.
   ext2_refuse = false;
   ext_events.clear();
   auto r_ext_ok = rasters->create_raster({8, 8, 32, 4, 4, 0x0500}, rf);
   check(r_ext_ok.has_value(), "raster creation with all extensions succeeding");
   check(ext_events.size() == 2 && ext_events[0] == 0x101 && ext_events[1] == 0x102, "extensions constructed forward");
   auto obs_ext_ok = rasters->observe(*r_ext_ok, rf);
   check(obs_ext_ok->extension_chain_completed, "extension chain marked complete");

   // Destroy raster: reverse extension destruction BEFORE platform destroy
   ext_events.clear();
   bool platform_destroy_called = false;
   RetailRendererRasterBackendCallbacks cb;
   cb.destroy = [&](NativeRetailRendererRasters&, NativeRetailRasterHandle) {
    platform_destroy_called = true;
    check(ext_events.size() == 2 && ext_events[0] == -0x102 && ext_events[1] == -0x101,
          "reverse extension destruction precedes platform destroy");
   };
   rasters->set_backend_callbacks(cb);
   check(rasters->destroy_raster(*r_ext_ok, rf), "destroy raster");
   check(platform_destroy_called, "platform destroy called");

   renderer_required::destroy_core_plugin_0407(*core);
  }

  // 6. Mip Query (2B9D90)
  {
   Fixture fix(root);
   auto rasters = fix.module();
   RetailRendererRasterFailure rf;

   // Non-mipped raster: format bit 0x80 clear -> returns 1
   auto r_non_mip = rasters->create_raster({8, 8, 32, 4, 4, 0x0500}, rf);
   check(rasters->mip_count(*r_non_mip, rf) == 1, "non-mipped format bit 0x80 clear returns 1");

   // Mipped raster: format bit 0x80 set -> calls backend
   auto r_mip = rasters->create_raster({8, 8, 32, 4, 4, 0x0580}, rf);
   check(rasters->mip_count(*r_mip, rf) == -1, "mipped without backend returns signed -1");

   RetailRendererRasterBackendCallbacks cb;
   cb.query_mips = [](NativeRetailRendererRasters&, NativeRetailRasterHandle, RetailRendererRasterFailure&) -> std::optional<std::int32_t> {
    return 4;
   };
   rasters->set_backend_callbacks(cb);
   check(rasters->mip_count(*r_mip, rf) == 4, "backend query mips returns 4");
  }

  // 7. Image to Raster Transfer & Scalar Conversion (2C0E90 / 2D14C0 / 2D04E0 / 2D04C0)
  {
   Fixture fix(root);
   fix.prepare();
   auto core = fix.core();
   check(renderer_required::construct_core_plugin_0406(*core, fix.f), "construct 0406");
   check(renderer_required::construct_core_plugin_0407(*core, fix.f), "construct 0407");

   auto images = core->images(fix.f);
   auto rasters = core->rasters(fix.f);
   RetailRendererImageFailure img_f;
   RetailRendererRasterFailure rf;

   // Create 2x2 32-bit RGBA image
   auto img_h = images->create_image({2, 2, 32}, img_f);
   check(img_h.has_value() && images->allocate_pixels(*img_h, img_f), "allocate image");

   // Verify that texture conversion without attached GPU is rejected by name (missing_backing)
   auto r_unbacked = rasters->create_raster({2, 2, 32, 4, 4, 0x0500}, rf);
   check(r_unbacked.has_value(), "create unbacked raster");
   check(!rasters->set_from_image(*r_unbacked, *images, *img_h, rf) &&
         rf.kind == RetailRendererRasterError::missing_backing, "texture conversion without GPU rejected");
   check(rasters->destroy_raster(*r_unbacked, rf), "destroy unbacked test raster");

#ifdef _WIN32
   HiddenGl host_gl;
   check(rasters->attach_gpu_device(host_gl.device, rf), "attach GPU device");
#endif

   // RGBA bytes:
   // Pixel 0: 0x12, 0x34, 0x56, 0x78
   // Pixel 1: 0xAA, 0xBB, 0xCC, 0xDD
   // Pixel 2: 0x10, 0x20, 0x30, 0x40
   // Pixel 3: 0x50, 0x60, 0x70, 0x80
   const std::uint8_t src_rgba[16]{
    0x12, 0x34, 0x56, 0x78,
    0xAA, 0xBB, 0xCC, 0xDD,
    0x10, 0x20, 0x30, 0x40,
    0x50, 0x60, 0x70, 0x80
   };
   check(images->write_bytes(*img_h, false, 0, std::as_bytes(std::span(src_rgba)), img_f), "write image bytes");

   // Case A: Format 0x0500 (RGBA 8888 -> LE word 0x78123456, bytes 56 34 12 78)
   auto r_0500 = rasters->create_raster({2, 2, 32, 4, 4, 0x0500}, rf);
   check(r_0500.has_value(), "create 0500 raster");
   check(rasters->set_from_image(*r_0500, *images, *img_h, rf), "set_from_image 0500");

   auto span_0500_opt = rasters->lock_pixels(*r_0500, rf);
   check(span_0500_opt.has_value(), "lock 0500 pixels");
   const auto* p_0500 = reinterpret_cast<const std::uint8_t*>(span_0500_opt->data());
   // Check pixel 0 in memory: B=0x56, G=0x34, R=0x12, A=0x78
   check(p_0500[0] == 0x56 && p_0500[1] == 0x34 && p_0500[2] == 0x12 && p_0500[3] == 0x78, "0500 scalar word 0x78123456");
   check(rasters->unlock_pixels(*r_0500, rf), "unlock 0500");

   // Case B: Format 0x0600 (RGBX 8888 -> LE word 0xFF123456, alpha forced to 0xFF)
   auto r_0600 = rasters->create_raster({2, 2, 32, 4, 4, 0x0600}, rf);
   check(r_0600.has_value(), "create 0600 raster");
   check(rasters->set_from_image(*r_0600, *images, *img_h, rf), "set_from_image 0600");

   auto span_0600_opt = rasters->lock_pixels(*r_0600, rf);
   check(span_0600_opt.has_value(), "lock 0600 pixels");
   const auto* p_0600 = reinterpret_cast<const std::uint8_t*>(span_0600_opt->data());
   // Check pixel 0 in memory: B=0x56, G=0x34, R=0x12, A=0xFF
   check(p_0600[0] == 0x56 && p_0600[1] == 0x34 && p_0600[2] == 0x12 && p_0600[3] == 0xFF, "0600 scalar word 0xFF123456");
   check(rasters->unlock_pixels(*r_0600, rf), "unlock 0600");

   // Case C: Gamma flag 2 propagation into raster private bit 0 (mask 0x01 per 2C0EB0)
   // Image conversion must succeed before gamma flag 2 propagates into raster private bit 0
   // Create an image with gamma applied (which sets image flag 2)
   auto img_gamma = images->create_image({2, 2, 32}, img_f);
   check(images->allocate_pixels(*img_gamma, img_f), "allocate gamma image");
   check(images->set_gamma(1.0f, img_f), "set gamma 1.0");
   check(images->apply_gamma(*img_gamma, img_f), "apply gamma");
   auto img_gamma_obs = images->observe(*img_gamma, img_f);
   check((img_gamma_obs->flags & 2) != 0, "image has gamma flag 2");

   auto r_gamma = rasters->create_raster({2, 2, 32, 4, 4, 0x0500}, rf);
   check(rasters->set_from_image(*r_gamma, *images, *img_gamma, rf), "set_from_image with gamma");
   auto r_gamma_obs = rasters->observe(*r_gamma, rf);
   check((r_gamma_obs->private_flags & 1) != 0, "raster private bit 0 set from image gamma flag 2");

   // Case D: Destination capacity / bounds validation (2x2 image into 1x1 raster fails extent)
   auto r_small = rasters->create_raster({1, 1, 32, 4, 4, 0x0500}, rf);
   check(r_small.has_value(), "create 1x1 raster");
   check(!rasters->set_from_image(*r_small, *images, *img_h, rf) &&
         rf.kind == RetailRendererRasterError::extent, "2x2 image into 1x1 raster rejected by extent");

   // Case E: Unsupported source image depth (e.g. 16 bpp) rejected by name
   auto img_16 = images->create_image({2, 2, 16}, img_f);
   check(img_16.has_value(), "create 16bpp image");
   check(images->allocate_pixels(*img_16, img_f), "allocate 16bpp image pixels");
   auto r_for_16 = rasters->create_raster({2, 2, 32, 4, 4, 0x0500}, rf);
   check(!rasters->set_from_image(*r_for_16, *images, *img_16, rf) &&
         rf.kind == RetailRendererRasterError::unsupported_depth, "unsupported 16bpp image depth rejected");

   // Case F: Unsupported destination raster depth (e.g. 16 bpp) rejected by name
   auto r_depth16 = rasters->create_raster({2, 2, 16, 1, 1, 0x0100}, rf);
   check(r_depth16.has_value(), "create 16bpp raster");
   check(!rasters->set_from_image(*r_depth16, *images, *img_h, rf) &&
         rf.kind == RetailRendererRasterError::unsupported_depth, "unsupported 16bpp raster depth rejected");

   renderer_required::destroy_core_plugin_0407(*core);
   renderer_required::destroy_core_plugin_0406(*core);
  }

  // 8. Lock / Unlock State & Subview Offset Lock (2C85F0 / 2C8960)
  {
   Fixture fix(root);
   auto rasters = fix.module();
   RetailRendererRasterFailure rf;

   auto root_r = rasters->create_raster({8, 8, 32, 4, 4, 0x0500}, rf);
   check(root_r.has_value(), "create root raster");

   // Populate root raster with known coordinates
   auto span_opt = rasters->lock_pixels(*root_r, rf);
   check(span_opt.has_value(), "lock root");
   auto* root_ptr = reinterpret_cast<std::uint32_t*>(span_opt->data());
   for (std::size_t y = 0; y < 8; ++y) {
    for (std::size_t x = 0; x < 8; ++x) {
     root_ptr[y * 8 + x] = static_cast<std::uint32_t>((y << 16) | x);
    }
   }
   // Recursive lock fails
   check(!rasters->lock_pixels(*root_r, rf) && rf.kind == RetailRendererRasterError::lock_state, "double lock rejected");
   check(rasters->unlock_pixels(*root_r, rf), "unlock root");
   check(!rasters->unlock_pixels(*root_r, rf) && rf.kind == RetailRendererRasterError::lock_state, "double unlock rejected");

   // Create subview at x=2, y=3
   auto sub = rasters->create_subview(*root_r, 2, 3, 4, 4, rf);
   check(sub.has_value(), "create subview");
   auto sub_span_opt = rasters->lock_pixels(*sub, rf);
   check(sub_span_opt.has_value(), "lock subview");
   const auto* sub_ptr = reinterpret_cast<const std::uint32_t*>(sub_span_opt->data());
   // Subview offset points to root pixel at (2, 3)
   check(sub_ptr[0] == ((3 << 16) | 2), "subview locks offset into root backing");
   check(rasters->unlock_pixels(*sub, rf), "unlock subview");
  }

  // 9. GPU Backing Integration via NativeGlTextureUpload
  {
   Fixture fix(root);
   fix.prepare();
   auto core = fix.core();
   check(renderer_required::construct_core_plugin_0406(*core, fix.f), "construct 0406");
   check(renderer_required::construct_core_plugin_0407(*core, fix.f), "construct 0407");

   auto images = core->images(fix.f);
   auto rasters = core->rasters(fix.f);
   RetailRendererImageFailure img_f;
   RetailRendererRasterFailure rf;

#ifdef _WIN32
   HiddenGl host_gl;
   check(rasters->attach_gpu_device(host_gl.device, rf), "attach real GPU device");

   auto img_h = images->create_image({4, 4, 32}, img_f);
   check(images->allocate_pixels(*img_h, img_f), "allocate image");

   auto r_gpu = rasters->create_raster({4, 4, 32, 4, 4, 0x0500}, rf);
   check(rasters->set_from_image(*r_gpu, *images, *img_h, rf), "set_from_image with real GPU attached");

   auto obs = rasters->observe(*r_gpu, rf);
   check(obs->has_gpu_backing, "raster observes real GPU backing");
   auto tex = rasters->gpu_texture(*r_gpu, rf);
   check(bool(tex), "gpu_texture handle retrieved");
   NativeTextureFailure gtf;
   auto gl_id = tex->gl_object(gtf);
   check(gl_id.has_value() && *gl_id > 0, "real OpenGL texture object allocated");
   check(glIsTexture(*gl_id) == GL_TRUE, "valid OpenGL texture object verified via glIsTexture");

   // Create a subview and verify it shares the GPU texture
   auto sub_gpu = rasters->create_subview(*r_gpu, 0, 0, 2, 2, rf);
   check(sub_gpu.has_value(), "subview of GPU raster created");
   auto sub_obs = rasters->observe(*sub_gpu, rf);
   check(sub_obs->has_gpu_backing, "subview observes GPU backing");

   // Re-upload with second image: verify prior GPU texture is retired cleanly before new one is assigned
   auto img_h2 = images->create_image({4, 4, 32}, img_f);
   check(images->allocate_pixels(*img_h2, img_f), "allocate second image");
   check(rasters->set_from_image(*r_gpu, *images, *img_h2, rf), "re-upload over existing GPU raster");
   auto tex2 = rasters->gpu_texture(*r_gpu, rf);
   check(bool(tex2), "second texture handle valid");

   // Sampler configuration
   NativeTextureSampler custom_sampler{
    NativeTextureFilter::nearest,
    NativeTextureAddress::repeat,
    NativeTextureAddress::repeat
   };
   check(rasters->set_sampler(*r_gpu, custom_sampler, rf), "set custom sampler on GPU raster");
   auto s_opt = rasters->sampler(*r_gpu, rf);
   check(s_opt.has_value() && s_opt->filter == mka::graphics::NativeTextureFilter::nearest, "custom sampler verified");

   // Destroying root raster revokes GPU texture and clears subview GPU backing
   check(rasters->destroy_raster(*r_gpu, rf), "destroy GPU raster");
   auto sub_obs_after = rasters->observe(*sub_gpu, rf);
   check(!sub_obs_after->has_gpu_backing, "subview GPU backing cleared after root destruction");
#endif

   renderer_required::destroy_core_plugin_0407(*core);
   renderer_required::destroy_core_plugin_0406(*core);
  }

  // 10. Thread & Session Invalidation
  {
   Fixture fix(root);
   auto rasters = fix.module();
   RetailRendererRasterFailure rf;
   auto r1 = rasters->create_raster({4, 4, 32, 4, 4, 0x0500}, rf);

   bool wrong_thread_caught = false;
   std::thread other([&] {
    RetailRendererRasterFailure other_f;
    auto obs = rasters->observe(*r1, other_f);
    if (!obs && other_f.kind == RetailRendererRasterError::wrong_thread) {
     wrong_thread_caught = true;
    }
   });
   other.join();
   check(wrong_thread_caught, "wrong thread rejected by name");
  }

  // 11. Core Plugin Unwind (later 0409 refusal & transfer)
  {
   Fixture fix(root);
   later = Later::refuse;
   check(main_binding::initialize_retail_first_scene(fix.main, fix.sf) == RetailStartupGate::native_failure && suffix_calls == 1,
         "actual Main/Core prefix 0403..0408 then explicit 0409 refusal");
   RetailRendererRasterFailure rf;
   check(captured_rasters && !captured_rasters->observe(captured_raster, rf) &&
         rf.kind == RetailRendererRasterError::retired,
         "later failure retires actual 0407 payloads");
   captured_rasters.reset();
  }
  {
   Fixture fix(root);
   fix.prepare();
   auto c = fix.core();
   later = Later::transfer;
   transfer_main = &fix.main;
   bool moved = false;
   try {
    (void)c->construct_registered(fix.f);
   } catch (const detail::NativeMainReentry& e) {
    moved = (e.owner == &fix.main && e.value == 91);
   }
   RetailRendererRasterFailure rf;
   check(moved && captured_rasters && captured_rasters->observe(captured_raster, rf),
         "Main transfer preserves actual 0407 prefix");
   check(c->destroy_constructed(fix.f) && !captured_rasters->live_rasters(rf) && rf.kind == RetailRendererRasterError::retired, "real prefix cleanup");
   captured_rasters.reset();
   transfer_main = nullptr;
   later = Later::refuse;
  }
  {
   std::shared_ptr<NativeRetailRendererRasters> held;
   std::weak_ptr<NativeRetailGameSession> session;
   later = Later::allow;
   {
    Fixture fix(root);
    fix.prepare();
    auto c = fix.core();
    check(c->construct_registered(fix.f), "explicit later fixtures around real 0407");
    held = c->rasters(fix.f);
    session = fix.startup.game_session;
   }
   RetailRendererRasterFailure rf;
   check(session.expired() && !held->live_rasters(rf) && rf.kind == RetailRendererRasterError::retired,
         "retained module cannot retain usable Session/payload");
   later = Later::refuse;
  }

  // 12. Extension Constructor Reentry & Resumable Destruction Cursor
  {
   Fixture fix(root);
   fix.prepare();
   auto core = fix.core();

   std::vector<int> lifecycle_events;
   bool throw_in_destroy = true;

   auto ext_a = std::make_shared<RetailRendererRasterExtension>();
   ext_a->id = 0x201;
   ext_a->construct = [&](NativeRetailRendererRasters&, NativeRetailRasterHandle, std::shared_ptr<NativeRetailRasterExtensionState>&, RetailRendererRasterFailure&) {
    lifecycle_events.push_back(0x201);
    return true;
   };
   ext_a->destroy = [&](NativeRetailRendererRasters&, NativeRetailRasterHandle, std::shared_ptr<NativeRetailRasterExtensionState>&) {
    lifecycle_events.push_back(-0x201);
   };

   auto ext_b = std::make_shared<RetailRendererRasterExtension>();
   ext_b->id = 0x202;
   ext_b->construct = [&](NativeRetailRendererRasters&, NativeRetailRasterHandle, std::shared_ptr<NativeRetailRasterExtensionState>&, RetailRendererRasterFailure&) {
    lifecycle_events.push_back(0x202);
    return true;
   };
   ext_b->destroy = [&](NativeRetailRendererRasters&, NativeRetailRasterHandle, std::shared_ptr<NativeRetailRasterExtensionState>&) {
    lifecycle_events.push_back(-0x202);
    if (throw_in_destroy) {
     throw_in_destroy = false;
     reenter_native_retail_main(fix.main, 42);
    }
   };

   check(core->register_raster_extension(ext_a, fix.f), "reg ext_a");
   check(core->register_raster_extension(ext_b, fix.f), "reg ext_b");
   check(renderer_required::construct_core_plugin_0407(*core, fix.f), "construct 0407");
   auto rasters = core->rasters(fix.f);
   RetailRendererRasterFailure rf;

   auto r = rasters->create_raster({8, 8, 32, 4, 4, 0x0500}, rf);
   check(r.has_value(), "create raster for resumable destruction");
   check(lifecycle_events.size() == 2 && lifecycle_events[0] == 0x201 && lifecycle_events[1] == 0x202, "extensions constructed in order");

   // First destruction attempt: ext_b throws Main reentry
   lifecycle_events.clear();
   bool reentry_caught = false;
   try {
    (void)rasters->destroy_raster(*r, rf);
   } catch (const detail::NativeMainReentry& e) {
    reentry_caught = (e.value == 42);
   }
   check(reentry_caught, "Main reentry thrown from extension destructor");
   check(lifecycle_events.size() == 1 && lifecycle_events[0] == -0x202, "ext_b destructor executed before reentry");

   auto obs_destroying = rasters->observe(*r, rf);
   check(obs_destroying.has_value() && obs_destroying->destroying, "raster marked destroying with preserved cursor");

   // Second destruction attempt: resumes from destroy_cursor, executing ext_a destructor
   check(rasters->destroy_raster(*r, rf), "destroy_raster resumed and completed");
   check(lifecycle_events.size() == 2 && lifecycle_events[1] == -0x201, "resumed destruction executed ext_a destructor");

   renderer_required::destroy_core_plugin_0407(*core);
  }

  std::printf("PASS_NATIVE0407_SHARED_RASTERS; default sentinel flags80, format normalization 0500/0600, stack push/pop/underflow, wrapping 16-bit subview (0000, 8000), extension rollback/reverse order, mip query, image->raster conversion with gamma propagation, lock/unlock, GPU upload/revocation, thread/session boundaries, resumable cursor\n");
  return 0;
 } catch (const std::exception& e) {
  fail_after = -1;
  std::fprintf(stderr, "FAIL native0407 rasters: %s\n", e.what());
  return 1;
 }
}
