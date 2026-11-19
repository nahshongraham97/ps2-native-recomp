#include "mka/font_geometry.hpp"
#include <array>
#include <cmath>
#if defined(__FAST_MATH__)
#error "font_geometry requires ordered float32 operations; disable fast-math"
#endif
#pragma STDC FP_CONTRACT OFF
namespace mka::text {
const char* font_geometry_error_name(FontGeometryError e) noexcept {
    switch(e){
    case FontGeometryError::none:return "none";
    case FontGeometryError::invalid_extent:return "invalid_extent";
    case FontGeometryError::unsupported_scale:return "unsupported_scale";
    case FontGeometryError::invalid_placement:return "invalid_placement";
    case FontGeometryError::nonfinite_metrics:return "nonfinite_metrics";
    case FontGeometryError::host_overflow:return "host_overflow";
    }return "unknown_font_geometry_error";
}
namespace {
bool add(std::int64_t a,std::int64_t b,std::int64_t& r) noexcept {
    if((b>0&&a>INT64_MAX-b)||(b<0&&a<INT64_MIN-b))return false;r=a+b;return true;
}
bool sub(std::int64_t a,std::int64_t b,std::int64_t& r) noexcept {
    if((b>0&&a<INT64_MIN+b)||(b<0&&a>INT64_MAX+b))return false;r=a-b;return true;
}
bool cvt(float f,std::int64_t& r) noexcept {
    // EE CVT.W.S is toward zero (primary instruction manual p356). Native
    // results extend to checked int64 rather than source32 clamp/wrapping.
    if(!std::isfinite(f)||f>=0x1p63f||f< -0x1p63f)return false;
    r=static_cast<std::int64_t>(f);return true;
}
}
std::optional<FontGlyphGeometry> geometry_font_glyph(const FontGlyphPlacement& p,
    const FontGeometryConfig& c,FontGeometryFailure& failure) noexcept {
    failure={};
    const auto fail=[&](FontGeometryError e)->std::optional<FontGlyphGeometry>{failure={e,p.source_offset};return {};};
    if(!c.logical_screen_width||!c.logical_screen_height||!c.draw_canvas_width||
       !c.draw_canvas_height||!c.atlas_width||!c.atlas_height)return fail(FontGeometryError::invalid_extent);
    float scale;
    switch(c.vertical_scale){case FontVerticalScale::one:scale=1.0f;break;
        case FontVerticalScale::fifteen_sixteenths:scale=0.9375f;break;
        default:return fail(FontGeometryError::unsupported_scale);}
    if(p.source_character<=32)return fail(FontGeometryError::invalid_placement);
    const auto& m=p.metrics;
    for(float f:std::array<float,9>{m.width,m.height,m.bearing_x,m.bearing_y,m.u0,m.u1,m.v0,m.v1,m.advance})
        if(!std::isfinite(f))return fail(FontGeometryError::nonfinite_metrics);
    // Positive native extents make /2 source-compatible. uint64 /2 always
    // fits int64. No original short-field truncation or canvas-size cap.
    const auto half_w=static_cast<std::int64_t>(c.logical_screen_width/2);
    const auto half_h=static_cast<std::int64_t>(c.logical_screen_height/2);
    const auto canvas_half_w=static_cast<std::int64_t>(c.draw_canvas_width/2);
    const auto canvas_half_h=static_cast<std::int64_t>(c.draw_canvas_height/2);
    std::int64_t ix,offset_x,offset_y,base_y_int;
    if(!add(p.pen_x,2048,ix)||!sub(ix,half_w,ix)||
       !sub(2048,canvas_half_w,offset_x)||!sub(2048,canvas_half_h,offset_y))return fail(FontGeometryError::host_overflow);
    const float base_x=static_cast<float>(ix);
    // Keep each source float32 step and the origin BEFORE baseline conversion.
    const float scaled_baseline=static_cast<float>(p.baseline)*scale;
    const float baseline_with_origin=scaled_baseline+2048.0f;
    const float centered_baseline=baseline_with_origin-static_cast<float>(half_h);
    if(!cvt(centered_baseline,base_y_int))return fail(FontGeometryError::host_overflow);
    const float base_y=static_cast<float>(base_y_int);
    const float bx1=m.bearing_x+m.width;
    const float by0=-m.bearing_y;
    const float by1=by0+m.height;
    const float x0=m.bearing_x+base_x,x1=bx1+base_x;
    const float sy0=by0*scale,sy1=by1*scale;
    const float y0=(sy0+base_y)+0.0f,y1=(sy1+base_y)+0.0f;
    FontGlyphGeometry out{};out.source_offset=p.source_offset;out.source_character=p.source_character;
    if(!cvt(x0,out.x0)||!cvt(x1,out.x1)||!cvt(y0,out.y0)||!cvt(y1,out.y1)||
       !sub(out.x0,offset_x,out.x0)||!sub(out.x1,offset_x,out.x1)||
       !sub(out.y0,offset_y,out.y0)||!sub(out.y1,offset_y,out.y1))return fail(FontGeometryError::host_overflow);
    const float aw=static_cast<float>(c.atlas_width),ah=static_cast<float>(c.atlas_height);
    const float u0=m.u0*aw,u1=m.u1*aw,v0=m.v0*ah,v1=m.v1*ah;
    if(!cvt(u0*16.0f,out.u0_sixteenths)||!cvt(u1*16.0f,out.u1_sixteenths)||
       !cvt(v0*16.0f,out.v0_sixteenths)||!cvt(v1*16.0f,out.v1_sixteenths))return fail(FontGeometryError::host_overflow);
    return out;
}
} // namespace mka::text
