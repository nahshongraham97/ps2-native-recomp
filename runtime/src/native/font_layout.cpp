#include "mka/font_layout.hpp"
#include <cmath>
#include <limits>
#include <new>
#include <stdexcept>
#if defined(__FAST_MATH__)
#error "font_layout requires ordered float32 operations; disable fast-math"
#endif
namespace mka::text {
const char* font_layout_error_name(FontLayoutError e) noexcept {
    switch(e){
    case FontLayoutError::none:return "none";
    case FontLayoutError::unsupported_control:return "unsupported_control";
    case FontLayoutError::missing_glyph:return "missing_glyph";
    case FontLayoutError::invalid_metrics:return "invalid_metrics";
    case FontLayoutError::nonprogress:return "nonprogress";
    case FontLayoutError::host_overflow:return "host_overflow";
    case FontLayoutError::allocation_failed:return "allocation_failed";
    }return "unknown_font_layout_error";
}
namespace {
bool add(std::int64_t a,std::int64_t b,std::int64_t& out){
    if((b>0 && a>INT64_MAX-b)||(b<0 && a<INT64_MIN-b))return false;
    out=a+b;return true;
}
bool source_advance(std::int64_t a,float b,std::int64_t& out){
    // Original converts the integer pen to float32, adds once, then truncates.
    const float result=static_cast<float>(a)+b;
    if(!std::isfinite(result))return false;
    // Exact power-of-two bound also works where long double aliases double;
    // converting INT64_MAX to float/double would round upward to 2^63.
    if(result>=0x1p63f || result< -0x1p63f)return false;
    out=static_cast<std::int64_t>(result);return true;
}
}
std::optional<FontLayout> layout_font_bytes(const NativeFontMetrics& metrics,
    std::span<const std::uint8_t> bytes,const FontLayoutConfig& c,FontLayoutFailure& failure){
    failure={};std::size_t at=0;
    const auto fail=[&](FontLayoutError e,std::size_t offset)->std::optional<FontLayout>{failure={e,offset};return {};};
    try {
        if(metrics.raw_bytes().empty())return fail(FontLayoutError::invalid_metrics,0);
        FontLayout result;std::size_t consumed=0,last_break=0;std::int64_t baseline=c.baseline;
        while(consumed<bytes.size() && consumed<c.total_byte_budget){
            at=consumed;std::size_t count=0,selected=last_break;
            std::int64_t sum=0,last_space_width=0,width=0;bool newline=false;
            while(count<c.per_line_scan_budget){
                width=sum;selected=count;
                if(count>=bytes.size()-consumed || count>=c.total_byte_budget-consumed)break;
                at=consumed+count;const auto ch=bytes[at];
                if(ch=='\\' && at+1<bytes.size() && bytes[at+1]=='n'){newline=true;break;}
                if(ch<32)return fail(FontLayoutError::unsupported_control,at);
                FontMetricsFailure f;const auto advance=metrics.advance(ch,f);
                if(!advance)return fail(f.kind==FontMetricsError::missing_glyph?FontLayoutError::missing_glyph:FontLayoutError::invalid_metrics,at);
                if(ch==32){last_space_width=sum;last_break=count;}
                if(!source_advance(sum,*advance,sum))return fail(FontLayoutError::host_overflow,at);
                ++count;width=last_space_width;selected=last_break;
            }
            if(selected==0 && !newline)return fail(FontLayoutError::nonprogress,consumed);
            // Native division rounds toward zero, matching source's negative
            // width+1 / arithmetic-shift sequence without overflowing width+1.
            const auto half=width/2;
            std::int64_t x;
            if(half==INT64_MIN || !add(c.center,-half,x))return fail(FontLayoutError::host_overflow,consumed);
            result.lines.push_back({consumed,selected,count,width,x,baseline,newline});
            for(std::size_t j=0;j<selected;++j){
                if(j>=bytes.size()-consumed)return fail(FontLayoutError::host_overflow,consumed);
                at=consumed+j;const auto ch=bytes[at];
                if(ch<32)return fail(FontLayoutError::unsupported_control,at);
                FontMetricsFailure f;
                if(ch!=32){
                    const auto glyph=metrics.glyph(ch,f);
                    if(!glyph)return fail(f.kind==FontMetricsError::missing_glyph?FontLayoutError::missing_glyph:FontLayoutError::invalid_metrics,at);
                    result.glyphs.push_back({at,ch,x,baseline,*glyph});
                }
                const auto advance=metrics.advance(ch,f);
                if(!advance)return fail(FontLayoutError::invalid_metrics,at);
                if(!source_advance(x,*advance,x))return fail(FontLayoutError::host_overflow,at);
            }
            if(selected>bytes.size()-consumed)return fail(FontLayoutError::host_overflow,consumed);
            consumed+=selected;last_break=selected;
            if(newline){
                if(bytes.size()-consumed<2)return fail(FontLayoutError::host_overflow,consumed);
                consumed+=2;
            }
            // The original advances its baseline after every selected line,
            // including the final one. Check the extended native arithmetic.
            if(!add(baseline,c.line_advance,baseline))return fail(FontLayoutError::host_overflow,consumed);
        }
        result.consumed_bytes=consumed;return result;
    }catch(const std::bad_alloc&){return fail(FontLayoutError::allocation_failed,at);}
     catch(const std::length_error&){return fail(FontLayoutError::allocation_failed,at);}
}
} // namespace mka::text
