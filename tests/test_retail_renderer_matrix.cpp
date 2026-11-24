#include "mka/retail_renderer_matrix.hpp"
#include "mka/retail_renderer_transforms.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"
#include "../runtime/src/native/retail_main_control.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <new>
#include <stdexcept>
#include <thread>
#include <type_traits>

extern "C" int SDL_InitSubSystem(unsigned){std::abort();}
namespace {thread_local bool fail_new=false;thread_local int fail_after=-1;}
void* operator new(std::size_t n){if(fail_new)throw std::bad_alloc();if(fail_after==0){fail_after=-1;throw std::bad_alloc();}if(fail_after>0)--fail_after;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}void operator delete(void* p,std::size_t)noexcept{std::free(p);}

using namespace mka::retail;
namespace {
void check(bool b,const char* what){if(!b)throw std::runtime_error(what);}
unsigned custom_calls{};
const RetailRendererAffine* observed_left{};const RetailRendererAffine* observed_right{};
NativeRetailMainState* transfer_main{};bool throw_native{};
auto bits(const RetailRendererMatrixTolerances& t){static_assert(sizeof(t)==12);return std::bit_cast<std::array<std::uint32_t,3>>(t);}

bool custom_product(RetailRendererAffine& out,const RetailRendererAffine& left,const RetailRendererAffine& right,RetailRendererMatrixFailure&){
 ++custom_calls;observed_left=&left;observed_right=&right;
 if(transfer_main)reenter_native_retail_main(*transfer_main,29);
 if(throw_native)throw std::runtime_error("explicit native math fixture failure");
 std::memmove(&out,&left,sizeof(out));return true; // Fixture code,not claimed as 19F680 arithmetic.
}

struct Fixture {
 NativeRetailStartupState startup;mka::input::ControllerInput samples;NativeRetailMainState main;
 std::shared_ptr<NativeRetailRendererAllocatorService> allocator;std::shared_ptr<NativeRetailRendererCore> core;
 RetailRendererCoreFailure cf;StartupFailure sf;
 explicit Fixture(const std::filesystem::path& root):startup(open(root),{}),main(startup,samples){
  check(initialize_native_retail_game_session(startup,sf),"actual Session");check(initialize_retail_memory_owners(startup,sf),"actual memory group");
  RetailRendererAllocatorFailure af;allocator=NativeRetailRendererAllocatorService::create(*startup.memory_owners,af);check(bool(allocator),"actual Permanent capability");
  core=NativeRetailRendererCore::for_session(startup.game_session,allocator,startup.content,cf);check(bool(core),"actual persistent renderer Core");
  check(bool(core->register_factory({0x402,24},{renderer_required::construct_core_plugin_0402,renderer_required::destroy_core_plugin_0402},cf)),"actual0402 factory bound in real registry");
 }
 static mka::content::NativeContentVolume open(const std::filesystem::path& p){mka::content::ContentVolumeFailure f;auto v=mka::content::NativeContentVolume::open(p,f);check(bool(v),"native existing root");return std::move(*v);}
};
}

namespace mka::retail::renderer_required {
// Other core plugins remain stubbed in this isolated matrix test.
bool construct_core_plugin_0403(NativeRetailRendererCore&,RetailRendererCoreFailure&){std::abort();}
void destroy_core_plugin_0403(NativeRetailRendererCore&)noexcept{std::abort();}
bool construct_core_plugin_0404(NativeRetailRendererCore&,RetailRendererCoreFailure&){std::abort();}
void destroy_core_plugin_0404(NativeRetailRendererCore&)noexcept{std::abort();}
bool construct_core_plugin_0405(NativeRetailRendererCore&,RetailRendererCoreFailure&){std::abort();}
void destroy_core_plugin_0405(NativeRetailRendererCore&)noexcept{std::abort();}
bool construct_core_plugin_0406(NativeRetailRendererCore&,RetailRendererCoreFailure&){std::abort();}
void destroy_core_plugin_0406(NativeRetailRendererCore&)noexcept{std::abort();}
bool construct_core_plugin_0407(NativeRetailRendererCore&,RetailRendererCoreFailure&){std::abort();}
void destroy_core_plugin_0407(NativeRetailRendererCore&)noexcept{std::abort();}
bool construct_core_plugin_0408(NativeRetailRendererCore&,RetailRendererCoreFailure&){std::abort();}
void destroy_core_plugin_0408(NativeRetailRendererCore&)noexcept{std::abort();}
bool construct_core_plugin_0409(NativeRetailRendererCore&,RetailRendererCoreFailure&){std::abort();}
void destroy_core_plugin_0409(NativeRetailRendererCore&)noexcept{std::abort();}
bool construct_core_plugin_0412(NativeRetailRendererCore&,RetailRendererCoreFailure&){std::abort();}
void destroy_core_plugin_0412(NativeRetailRendererCore&)noexcept{std::abort();}
bool construct_core_plugin_040a(NativeRetailRendererCore&,RetailRendererCoreFailure&){std::abort();}
void destroy_core_plugin_040a(NativeRetailRendererCore&)noexcept{std::abort();}
bool construct_core_plugin_040b(NativeRetailRendererCore&,RetailRendererCoreFailure&){std::abort();}
void destroy_core_plugin_040b(NativeRetailRendererCore&)noexcept{std::abort();}
std::optional<bool> register_object_extensions_1b72b0(NativeRetailRendererCore&,RetailRendererCoreFailure&){std::abort();}
void discard_object_extensions_after_core_init(NativeRetailRendererCore&)noexcept{std::abort();}
}

static_assert(!std::is_default_constructible_v<NativeRetailRendererMatrixDomain>);
static_assert(!std::is_copy_constructible_v<NativeRetailRendererMatrixDomain>);
static_assert(!std::is_move_constructible_v<NativeRetailRendererMatrixDomain>);

int main(int argc, char** argv) try {
 check(argc == 2, "existing CPU-only content root required");
 Fixture x(argv[1]);
 RetailRendererMatrixFailure f;
 RetailMemoryOwnerFailure memory;
 const auto heap_before = x.startup.memory_owners->observe(RetailMemoryRole::permanent, memory)->heap.live_allocations;

 // ------------------------------------------------------------------------
 // [source-derived] Stage 1: 0402 Matrix registration, defaults, tolerance bit preservation
 // ------------------------------------------------------------------------
 std::puts("[source-derived] Stage 1: 0402 Matrix registration, defaults, tolerance bit preservation");
 check(!x.core->matrix_domain(x.cf) && *x.core->matrix_domain_count(x.cf) == 0, "actual module BSS count/missing owner");
 check(x.core->construct_registered(x.cf), "real registry constructs complete 0402 owner/settings");
 auto module = x.core->matrix_domain(x.cf);
 check(x.startup.memory_owners->observe(RetailMemoryRole::permanent, memory)->heap.live_allocations == heap_before + 1, "actual domain storage belongs to existing Permanent owner");
 check(module && *x.core->matrix_domain_count(x.cf) == 1, "actual 0402 module count published after construction");
 check(bits(*module->tolerances(f)) == std::array<std::uint32_t,3>{0x3c23d70au, 0x3c23d70au, 0x3c23d70au}, "all three original 5A3600 defaults exactly copied");
 check(!renderer_required::construct_core_plugin_0402(*x.core, x.cf) && x.cf.kind == RetailRendererCoreError::reentrant && *x.core->matrix_domain_count(x.cf) == 1, "native duplicate live-domain construction preserves ownership");

 const auto input = std::bit_cast<RetailRendererMatrixTolerances>(std::array<std::uint32_t,3>{0x80000000u, 0x7f800000u, 0x7fc01234u});
 check(module->set_tolerances(input, f) && bits(*module->tolerances(f)) == bits(input), "1A0210 exact data copies,not numeric validation;unusualbits are fixtures only");

 // ------------------------------------------------------------------------
 // [native-design-check] Stage 2: Domain admission, thread check, fault cuts
 // ------------------------------------------------------------------------
 std::puts("[native-design-check] Stage 2: Domain admission, thread check, fault cuts");
 RetailRendererAffine dummy_left{}, dummy_right{}, dummy_out{};
 check(module->set_product(custom_product, f), "custom product installs cleanly");
 transfer_main = &x.main;
 bool transferred = false;
 try {
  module->multiply(dummy_out, dummy_left, dummy_right, f);
 } catch (const detail::NativeMainReentry& e) {
  transferred = (e.owner == &x.main && e.value == 29);
 }
 transfer_main = nullptr;
 check(transferred && module->tolerances(f).has_value(), "legitimate Main transfer propagates without poisoning native owner");

 throw_native = true;
 check(!module->multiply(dummy_out, dummy_left, dummy_right, f) && f.kind == RetailRendererMatrixError::after_effect_unknown, "native kernel exception names possible partial caller output");
 throw_native = false;

 bool wrong = false;
 std::thread thread([&] {
  RetailRendererMatrixFailure bad;
  wrong = !module->set_product(nullptr, bad) && bad.kind == RetailRendererMatrixError::wrong_thread;
 });
 thread.join();
 check(wrong, "foreign thread rejected before module mutation");

 check(module->set_product(nullptr, f), "restore authentic Option C product");

 // ------------------------------------------------------------------------
 // [native-design-check] Stage 3: Matrix multiplication & classification (Option C: Xbox 0x002be460 / 0x002be150)
 // ------------------------------------------------------------------------
 std::puts("[native-design-check] Stage 3: Matrix multiplication & classification (Option C: Xbox 0x002be460 / 0x002be150)");
 const RetailRendererMatrixTolerances standard_tol{0.001f, 0.001f, 0.001f};
 check(module->set_tolerances(standard_tol, f), "set standard tolerances");

 RetailRendererAffine left{
  {0.0f, 1.0f, 0.0f},
  {-1.0f, 0.0f, 0.0f},
  {0.0f, 0.0f, 1.0f},
  {10.0f, 20.0f, 30.0f}
 };
 RetailRendererAffine right{
  {1.0f, 0.0f, 0.0f},
  {0.0f, 1.0f, 0.0f},
  {0.0f, 0.0f, 1.0f},
  {5.0f, 6.0f, 7.0f}
 };
 RetailRendererAffine prod{};
 check(module->multiply(prod, left, right, f), "multiply left * right under Option C");
 // Hand-derived expected reference product:
 // R = (0*1 + 1*0 + 0*0, 0*0 + 1*1 + 0*0, 0*0 + 1*0 + 0*1) = (0, 1, 0)
 // U = (-1*1 + 0*0 + 0*0, -1*0 + 0*1 + 0*0, -1*0 + 0*0 + 0*1) = (-1, 0, 0)
 // A = (0*1 + 0*0 + 1*0, 0*0 + 0*1 + 1*0, 0*0 + 0*0 + 1*1) = (0, 0, 1)
 // T = (10*1 + 20*0 + 30*0 + 5, 10*0 + 20*1 + 30*0 + 6, 10*0 + 20*0 + 30*1 + 7) = (15, 26, 37)
 RetailRendererAffine expected_prod{
  {0.0f, 1.0f, 0.0f},
  {-1.0f, 0.0f, 0.0f},
  {0.0f, 0.0f, 1.0f},
  {15.0f, 26.0f, 37.0f}
 };
 float max_mul_delta = std::max({
  std::abs(prod.right.x - expected_prod.right.x), std::abs(prod.right.y - expected_prod.right.y), std::abs(prod.right.z - expected_prod.right.z),
  std::abs(prod.up.x - expected_prod.up.x), std::abs(prod.up.y - expected_prod.up.y), std::abs(prod.up.z - expected_prod.up.z),
  std::abs(prod.at.x - expected_prod.at.x), std::abs(prod.at.y - expected_prod.at.y), std::abs(prod.at.z - expected_prod.at.z),
  std::abs(prod.translation.x - expected_prod.translation.x), std::abs(prod.translation.y - expected_prod.translation.y), std::abs(prod.translation.z - expected_prod.translation.z)
 });
 std::printf("  multiplication max delta from hand-derived oracle = %.8f (within host IEEE 754 precision)\n", max_mul_delta);
 check(max_mul_delta <= 1e-6f, "multiplication matches hand-derived affine product");

 RetailRendererMatrixValue id_val{
  {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}},
  0
 };
 check(module->classify(id_val, std::nullopt, f), "classify identity matrix");
 check((id_val.properties & 0x01) != 0, "identity matrix has orthonormal flag (0x01)");
 check((id_val.properties & NativeRetailRendererMatrixDomain::identity_shortcut_mask) != 0, "identity matrix has shortcut flag (0x20000)");

 RetailRendererMatrixValue rot_val{
  left,
  0
 };
 check(module->classify(rot_val, std::nullopt, f), "classify rotated matrix");
 check((rot_val.properties & 0x01) != 0, "rotated matrix is orthonormal");
 check((rot_val.properties & NativeRetailRendererMatrixDomain::identity_shortcut_mask) == 0, "translated/rotated matrix does NOT have identity flag");

 RetailRendererMatrixValue scaled_val{
  {{2.0f, 0.0f, 0.0f}, {0.0f, 2.0f, 0.0f}, {0.0f, 0.0f, 2.0f}, {0.0f, 0.0f, 0.0f}},
  0
 };
 check(module->classify(scaled_val, std::nullopt, f), "classify scaled matrix");
 check(scaled_val.properties == 0, "scaled matrix has neither orthonormal nor identity flag");
 std::printf("  classification results: identity=0x%x, orthonormal_rot=0x%x, scaled=0x%x (tolerance bound 0.001f)\n",
  id_val.properties, rot_val.properties, scaled_val.properties);

 // ------------------------------------------------------------------------
 // [native-design-check] Stage 4: Matrix inversion & identity shortcut (Option C: Xbox 0x002be560)
 // ------------------------------------------------------------------------
 std::puts("[native-design-check] Stage 4: Matrix inversion & identity shortcut (Option C: Xbox 0x002be560)");
 RetailRendererMatrixValue id_input{id_val.affine, NativeRetailRendererMatrixDomain::identity_shortcut_mask};
 RetailRendererMatrixValue id_out{};
 check(module->inverse(id_out, id_input, f), "identity shortcut inverse");
 check(id_out.affine == id_input.affine, "identity shortcut bitcopy preserves affine");

 RetailRendererMatrixValue inv_out{};
 rot_val.properties = 0x01;
 check(module->inverse(inv_out, rot_val, f), "invert non-identity matrix");
 RetailRendererAffine should_be_id{};
 check(module->multiply(should_be_id, rot_val.affine, inv_out.affine, f), "rot_val * inv_out");
 float max_inv_delta = std::max({
  std::abs(should_be_id.right.x - 1.0f), std::abs(should_be_id.right.y - 0.0f), std::abs(should_be_id.right.z - 0.0f),
  std::abs(should_be_id.up.x - 0.0f), std::abs(should_be_id.up.y - 1.0f), std::abs(should_be_id.up.z - 0.0f),
  std::abs(should_be_id.at.x - 0.0f), std::abs(should_be_id.at.y - 0.0f), std::abs(should_be_id.at.z - 1.0f),
  std::abs(should_be_id.translation.x - 0.0f), std::abs(should_be_id.translation.y - 0.0f), std::abs(should_be_id.translation.z - 0.0f)
 });
 std::printf("  M * M^-1 delta from identity = %.8f (native host tolerance limit 1e-6)\n", max_inv_delta);
 check(max_inv_delta <= 1e-6f, "M * M^-1 yields identity within float precision");

 RetailRendererMatrixValue singular{};
 singular.affine = {{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}};
 check(!module->inverse(inv_out, singular, f) && f.kind == RetailRendererMatrixError::numeric_policy_unbound, "singular matrix rejection");
 std::printf("  singular matrix correctly rejected: %s (threshold |det| < 1e-12 is native host design choice)\n", f.operation);

 // ------------------------------------------------------------------------
 // [native-design-check] Stage 5: Vector & point transforms (PS2 0x0019fa00/fa70/fb00/fb80, Xbox counterpart unverified)
 // ------------------------------------------------------------------------
 std::puts("[native-design-check] Stage 5: Vector & point transforms (PS2 0x0019fa00/fa70/fb00/fb80, Xbox counterpart unverified)");
 RetailRendererPoint3 pt_in{1.0f, 2.0f, 3.0f};
 RetailRendererPoint3 pt_out{};
 renderer_required::transform_point_exact(pt_out, pt_in, rot_val.affine);
 // Hand-derived expected: P' = (1, 2, 3) * rot_val + (10, 20, 30) = (-2+10, 1+20, 3+30) = (8, 21, 33)
 float pt_delta = std::max({std::abs(pt_out.x - 8.0f), std::abs(pt_out.y - 21.0f), std::abs(pt_out.z - 33.0f)});
 std::printf("  transform_point_exact delta from hand-derived oracle = %.8f (expected (8, 21, 33))\n", pt_delta);
 check(pt_delta <= 1e-6f, "point transform delta <= 1e-6f");

 RetailRendererPoint3 vec_in{1.0f, 2.0f, 3.0f};
 RetailRendererPoint3 vec_out{};
 renderer_required::transform_vector_exact(vec_out, vec_in, rot_val.affine);
 // Hand-derived expected: V' = (1, 2, 3) * rot_val = (-2, 1, 3)
 float vec_delta = std::max({std::abs(vec_out.x - (-2.0f)), std::abs(vec_out.y - 1.0f), std::abs(vec_out.z - 3.0f)});
 std::printf("  transform_vector_exact delta from hand-derived oracle = %.8f (expected (-2, 1, 3))\n", vec_delta);
 check(vec_delta <= 1e-6f, "vector transform delta <= 1e-6f");

 std::array<RetailRendererPoint3, 2> pts_in{RetailRendererPoint3{1.0f, 2.0f, 3.0f}, RetailRendererPoint3{0.0f, 0.0f, 0.0f}};
 std::array<RetailRendererPoint3, 2> pts_out{};
 renderer_required::transform_points_exact(pts_out, pts_in, rot_val.affine);
 check(std::abs(pts_out[0].x - 8.0f) <= 1e-6f && std::abs(pts_out[1].x - 10.0f) <= 1e-6f, "transform_points_exact batch check");

 std::array<RetailRendererPoint3, 2> vecs_in{RetailRendererPoint3{1.0f, 2.0f, 3.0f}, RetailRendererPoint3{0.0f, 0.0f, 0.0f}};
 std::array<RetailRendererPoint3, 2> vecs_out{};
 renderer_required::transform_vectors_exact(vecs_out, vecs_in, rot_val.affine);
 check(std::abs(vecs_out[0].x - (-2.0f)) <= 1e-6f && std::abs(vecs_out[1].x - 0.0f) <= 1e-6f, "transform_vectors_exact batch check");

 // ------------------------------------------------------------------------
 // Teardown and allocation fault cuts
 // ------------------------------------------------------------------------
 check(x.core->destroy_constructed(x.cf) && *x.core->matrix_domain_count(x.cf) == 0 && !x.core->matrix_domain(x.cf), "dtor clears actual Core owner and source count");
 check(!module->tolerances(f) && f.kind == RetailRendererMatrixError::retired, "external native owner retires immediately with real factory teardown");
 check(x.startup.memory_owners->observe(RetailMemoryRole::permanent, memory)->heap.live_allocations == heap_before + 1, "retired external native capability retains safe storage, not live game access");
 module.reset();
 check(x.startup.memory_owners->observe(RetailMemoryRole::permanent, memory)->heap.live_allocations == heap_before, "last typed capability releases actual Permanent storage");

 fail_new = true;
 const bool failed = !x.core->construct_registered(x.cf);
 fail_new = false;
 check(failed && x.cf.kind == RetailRendererCoreError::allocation && *x.core->matrix_domain_count(x.cf) == 0 && !x.core->matrix_domain(x.cf), "native allocation failure publishes no partial owner/count");

 unsigned failed_cuts = 0, completed_cuts = 0;
 for (int cut = 0; cut < 8; ++cut) {
  fail_after = cut;
  const bool constructed = x.core->construct_registered(x.cf);
  fail_after = -1;
  if (constructed) {
   ++completed_cuts;
   check(x.core->destroy_constructed(x.cf), "successful allocation-cut probe retires real owner");
  } else {
   ++failed_cuts;
   check(x.cf.kind == RetailRendererCoreError::allocation, "actual native allocation failure remains named");
  }
  check(*x.core->matrix_domain_count(x.cf) == 0 && !x.core->matrix_domain(x.cf) && x.startup.memory_owners->observe(RetailMemoryRole::permanent, memory)->heap.live_allocations == heap_before, "every native creation fault cut preserves counter/owner/Permanent allocation balance");
 }
 check(failed_cuts >= 2 && completed_cuts > 0, "both storage/control-block failure cuts plus normal native construction reached");

 check(x.core->construct_registered(x.cf), "real registry retry after failed native allocation");
 module = x.core->matrix_domain(x.cf);
 check(bits(*module->tolerances(f)) == std::array<std::uint32_t,3>{0x3c23d70au, 0x3c23d70au, 0x3c23d70au}, "recreation resets actual defaults, not previous retired settings");

 std::weak_ptr<NativeRetailGameSession> lifetime = x.startup.game_session;
 x.startup.game_record_reset.reset();
 x.startup.game_session.reset();
 check(lifetime.expired(), "all actual Session owners retired");
 check(!module->tolerances(f) && f.kind == RetailRendererMatrixError::owner, "retained domain refuses after actual Session lifetime");

 std::puts("PASS_NATIVE0402_MATRIX_OWNER_DEFAULTS_SETTER_DISPATCH_IDENTITYCOPY_LIFETIME; exact math callbacks verified under Option C; no renderer/GPU acceptance");
 return 0;
} catch (const std::exception& e) {
 fail_new = false;
 std::fprintf(stderr, "0402 matrix: %s\n", e.what());
 return 1;
}
