#include "mka/hud_health_geometry.hpp"
#include <array>
#include <bit>
#include <charconv>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <utility>

using namespace mka::retail;
namespace {
void check(bool ok, const char* text) { if (!ok) { std::fprintf(stderr, "FAIL: %s\n", text); std::abort(); } }
auto vertices(std::size_t count = 6) {
    std::vector<HudPositionVertex> v;
    for (std::size_t i = 0; i < count; ++i)
        v.push_back({float(i + 10), float(i + 20), {std::byte{0x31}, std::byte(i), std::byte{0x93}}});
    return v;
}
auto geometry(std::size_t count = 6) {
    HudHealthGeometryFailure f; auto g = NativeHudPositionGeometry::create(vertices(count), f);
    check(g && f.kind == HudHealthGeometryError::none, "real owned native position geometry"); return g;
}
auto object(std::shared_ptr<NativeHudPositionGeometry> g) {
    HudHealthGeometryFailure f; auto result = NativeHudHealthObject::create(std::move(g), f);
    check(result.has_value(), "separate live native object association"); return std::move(*result);
}
auto reference(const NativeHudHealthObject& o) {
    HudHealthGeometryFailure f; auto result = o.reference(f);
    check(result.has_value(), "actual generation token"); return *result;
}
auto snapshot(const std::shared_ptr<NativeHudPositionGeometry>& g) {
    HudHealthGeometryFailure f; auto result = g->snapshot(f);
    check(result.has_value(), "real owned snapshot"); return *result;
}
void same(const HudPositionSnapshot& a, const HudPositionSnapshot& b) {
    check(a.dirty == b.dirty && a.vertices == b.vertices, "no position/channel/dirty mutation");
}
void quad(const HudPositionSnapshot& s, const std::array<float,4>& x) {
    check(s.dirty, "actual geometry dirty state");
    for (std::size_t i = 0; i < 4; ++i) {
        check(s.vertices[i].x == x[i] && s.vertices[i].y == (i == 0 || i == 3 ? 392.0f : 410.0f),
              "source ordered XY positions");
    }
    auto original = vertices(s.vertices.size());
    for (std::size_t i = 0; i < s.vertices.size(); ++i) {
        check(s.vertices[i].untouched_channels == original[i].untouched_channels, "uninterpreted native channels preserved");
        if (i >= 4) check(s.vertices[i] == original[i], "extra vertices preserved");
    }
}
void coordinates_and_preservation() {
    struct Case { HudHealthGeometryConfig config; std::array<float,4> l, r; };
    const Case cases[]{
        {{8,640,0.5f,0.25f}, {168,161,296,296}, {344,344,415,409}},
        {{8,640,0,0}, {296,296,296,296}, {344,344,344,345}},
        {{8,640,-0.5f,-0.25f}, {424,296,296,296}, {344,344,344,281}},
        {{8,640,1,1}, {40,33,296,296}, {344,344,607,601}},
        {{-9,704,0.5f,1}, {151,144,279,279}, {425,425,688,682}},
        {{32,1024,0,0}, {320,320,320,320}, {704,704,704,705}}
    };
    for (const auto& c : cases) {
        auto l = geometry(), r = geometry(); auto lo = object(l), ro = object(r);
        const auto lr = reference(lo), rr = reference(ro); HudHealthGeometryFailure f;
        check(update_hud_health_geometry(lr, rr, c.config, f) == HudHealthUpdate::updated &&
              f.kind == HudHealthGeometryError::none, "actual pair update succeeds");
        quad(snapshot(l), c.l); quad(snapshot(r), c.r);
        auto owned = snapshot(l); owned.vertices[0].x = -999; owned.vertices[0].untouched_channels.clear();
        quad(snapshot(l), c.l); // Snapshot owns copies, no hidden resource aliases.
    }
}
void preconditions_are_atomic() {
    auto l = geometry(), r = geometry(); auto lo = object(l), ro = object(r);
    const auto lr = reference(lo), rr = reference(ro); HudHealthGeometryFailure f;
    const auto lb = snapshot(l), rb = snapshot(r); const HudHealthGeometryConfig c{8,640,0.5f,0.25f};
    check(!update_hud_health_geometry(lr, HudHealthObjectRef{}, c, f) &&
          f.kind == HudHealthGeometryError::unbound_reference, "unknown producer is named, not absent");
    same(lb, snapshot(l)); same(rb, snapshot(r));
    check(update_hud_health_geometry(lr, HudHealthObjectRef::known_absent(), c, f) == HudHealthUpdate::skipped,
          "source known absence skips both writes");
    same(lb, snapshot(l)); same(rb, snapshot(r));
    auto bad = c; bad.displayed_right = std::numeric_limits<float>::quiet_NaN();
    check(!update_hud_health_geometry(lr, rr, bad, f) && f.kind == HudHealthGeometryError::invalid_configuration,
          "nonfinite second input prevents first edit");
    same(lb, snapshot(l)); same(rb, snapshot(r));
    bad = c; bad.displayed_left = std::numeric_limits<float>::max();
    check(!update_hud_health_geometry(lr, rr, bad, f) && f.kind == HudHealthGeometryError::invalid_configuration,
          "finite input with overflowing output rejected before writes");
    same(lb, snapshot(l)); same(rb, snapshot(r));
    auto short_g = geometry(3); auto short_o = object(short_g); const auto short_b = snapshot(short_g);
    check(!update_hud_health_geometry(lr, reference(short_o), c, f) && f.kind == HudHealthGeometryError::insufficient_vertices,
          "both actual buffers validated before either edit");
    same(lb, snapshot(l)); same(short_b, snapshot(short_g));
    check(ro.retire(f), "actual second object's generation revocation");
    check(update_hud_health_geometry(lr, rr, c, f) == HudHealthUpdate::skipped,
          "stale second token prevents valid first mutation");
    same(lb, snapshot(l)); same(rb, snapshot(r));
    check(!ro.reference(f) && f.kind == HudHealthGeometryError::inactive_owner, "retired object cannot mint token");
}
void aliases_and_lifetime() {
    const HudHealthGeometryConfig c{8,640,0.5f,0.25f}; HudHealthGeometryFailure f;
    auto g = geometry(); auto first = object(g), second = object(g);
    const auto a = reference(first), b = reference(second);
    check(update_hud_health_geometry(a,b,c,f) == HudHealthUpdate::updated, "distinct objects sharing one buffer avoid double lock");
    quad(snapshot(g), {344,344,415,409});
    check(update_hud_health_geometry(a,a,c,f) == HudHealthUpdate::updated, "same owner token twice avoids duplicate lifecycle lock");
    quad(snapshot(g), {344,344,415,409}); // Sequential left then right overwrites the same resource.
    NativeHudHealthObject moved(std::move(first));
    check(!first.reference(f) && f.kind == HudHealthGeometryError::invalid_owner, "moved-from owner cannot mint new reference");
    check(update_hud_health_geometry(a,b,c,f) == HudHealthUpdate::updated, "pre-move weak generation token remains bound to actual identity");
    auto replacement_g = geometry(); auto replacement = object(replacement_g); const auto old = reference(replacement);
    replacement = std::move(moved);
    const auto before = snapshot(replacement_g);
    check(update_hud_health_geometry(old,b,c,f) == HudHealthUpdate::skipped, "move assignment retires previous object association");
    same(before, snapshot(replacement_g));
    auto orphan_geometry = geometry(); std::weak_ptr<NativeHudPositionGeometry> weak = orphan_geometry;
    auto retained = object(orphan_geometry); const auto live = reference(retained);
    orphan_geometry.reset();
    check(!weak.expired() && update_hud_health_geometry(live,live,c,f) == HudHealthUpdate::updated,
          "actual object keeps native geometry alive independently of external resource owner");
    check(retained.retire(f) && weak.expired(), "actual retirement drops retained geometry outside life lock");
    check(update_hud_health_geometry(live,b,c,f) == HudHealthUpdate::skipped, "expired lifetime cannot update surviving other object");
    HudHealthObjectRef dropped;
    { auto temporary = object(g); dropped = reference(temporary); }
    const auto final = snapshot(g);
    check(update_hud_health_geometry(a,dropped,c,f) == HudHealthUpdate::skipped, "destruction invalidates weak association");
    same(final, snapshot(g));
}
std::uint32_t oracle_bits(const std::string& text) {
    std::uint32_t result{};
    const auto parsed = std::from_chars(text.data(), text.data()+text.size(), result, 16);
    check(parsed.ec == std::errc{} && parsed.ptr == text.data()+text.size(), "exact oracle float word");
    return result;
}
std::size_t saved_full_health_oracle(const char* path) {
    std::ifstream file(path);
    check(file.is_open(), "external saved full-health oracle opens");
    std::string line;
    check(bool(std::getline(file,line)) && line == "frame\torigin\tlogical_width\tdisplayed_left_bits\tdisplayed_right_bits\tside\tvertex\tx\ty\tx_bits\ty_bits\tdirty",
          "actual external oracle table schema");
    struct Expected {
        std::uint64_t frame;
        HudHealthGeometryConfig config;
        std::array<std::array<std::array<std::uint32_t,2>,4>,2> bits{};
        std::array<std::array<bool,4>,2> seen{};
        std::array<int,2> dirty{-1,-1};
    };
    std::vector<Expected> frames;
    while (std::getline(file,line)) {
        std::istringstream row(line);
        std::uint64_t frame{}; std::int64_t origin{}, width{}; std::size_t vertex{};
        std::string left_bits,right_bits,side,x_bits,y_bits,trailing; float x{},y{}; int dirty{};
        check(bool(row >> frame >> origin >> width >> left_bits >> right_bits >> side >> vertex >> x >> y >> x_bits >> y_bits >> dirty) &&
              !(row >> trailing), "complete exact oracle row");
        check((side == "left" || side == "right") && vertex < 4 && (dirty == 0 || dirty == 1), "captured side vertex dirty fields");
        const auto lb = oracle_bits(left_bits), rb = oracle_bits(right_bits);
        const auto xb = oracle_bits(x_bits), yb = oracle_bits(y_bits);
        check(std::bit_cast<std::uint32_t>(x) == xb && std::bit_cast<std::uint32_t>(y) == yb,
              "captured decimal values agree with captured float words");
        auto found = frames.size();
        for (std::size_t i = 0; i < frames.size(); ++i) if (frames[i].frame == frame) { found = i; break; }
        if (found == frames.size()) frames.push_back(Expected{frame,{origin,width,std::bit_cast<float>(lb),std::bit_cast<float>(rb)}});
        auto& e = frames[found];
        check(e.config.design_origin == origin && e.config.logical_width == width &&
              std::bit_cast<std::uint32_t>(e.config.displayed_left) == lb && std::bit_cast<std::uint32_t>(e.config.displayed_right) == rb,
              "one captured frame has consistent actual inputs");
        const auto side_index = side == "left" ? 0u : 1u;
        check(!e.seen[side_index][vertex] && (e.dirty[side_index] == -1 || e.dirty[side_index] == dirty),
              "no duplicate or conflicting oracle rows");
        e.seen[side_index][vertex] = true; e.bits[side_index][vertex] = {xb,yb}; e.dirty[side_index] = dirty;
    }
    check(file.eof() && !frames.empty(), "oracle rows fully read");
    for (const auto& e : frames) {
        for (const auto& side : e.seen) for (bool present : side) check(present, "all captured XY entries required");
        // Captured input scope is full health only; fractional cases above remain source-formula tests.
        check(e.config.design_origin == 8 && e.config.logical_width == 640 &&
              e.config.displayed_left == 1.0f && e.config.displayed_right == 1.0f, "captured full-health input scope");
        auto l = geometry(), r = geometry(); auto lo = object(l), ro = object(r); HudHealthGeometryFailure f;
        check(update_hud_health_geometry(reference(lo),reference(ro),e.config,f) == HudHealthUpdate::updated,
              "actual native update for captured full-health input");
        const std::array snapshots{snapshot(l),snapshot(r)};
        for (std::size_t side = 0; side < 2; ++side) {
            check(snapshots[side].dirty == bool(e.dirty[side]), "captured full-health dirty flag");
            for (std::size_t vertex = 0; vertex < 4; ++vertex)
                check(std::bit_cast<std::uint32_t>(snapshots[side].vertices[vertex].x) == e.bits[side][vertex][0] &&
                      std::bit_cast<std::uint32_t>(snapshots[side].vertices[vertex].y) == e.bits[side][vertex][1],
                      "actual native XY bits equal external captured full-health oracle");
        }
    }
    return frames.size();
}
} // namespace
int main(int argc, char** argv) {
    coordinates_and_preservation(); preconditions_are_atomic(); aliases_and_lifetime();
    check(argc == 1 || argc == 2, "optional external oracle path only");
    const auto oracle_frames = argc == 2 ? saved_full_health_oracle(argv[1]) : 0;
    std::printf("PASS_NATIVE_HUD_HEALTH_GEOMETRY: 3 groups actual paired position writes/dirty/preserved channels/lifetimes; saved full-health oracle frames=%zu; no task/resource producer/GPU or full game claim\n", oracle_frames);
    return std::fflush(stdout) || std::ferror(stdout) ? 1 : 0;
}
