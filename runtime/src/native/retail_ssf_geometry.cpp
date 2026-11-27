#include "mka/retail_ssf_geometry.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <new>
#include <stdexcept>

namespace mka::retail {
namespace {
struct Rejected {};
struct Reader {
    std::span<const std::byte> bytes;
    RetailSsfGeometryFailure& failure;
    [[noreturn]] void reject(RetailSsfGeometryError kind, const char* operation, std::size_t at) {
        failure = {kind, operation, at}; throw Rejected{};
    }
    void need(bool value, RetailSsfGeometryError kind, const char* operation, std::size_t at) {
        if (!value) reject(kind, operation, at);
    }
    std::uint32_t word(std::size_t at) {
        need(at <= bytes.size() && bytes.size() - at >= 4, RetailSsfGeometryError::range, "SSF geometry bounded word", at);
        std::uint32_t value{};
        for (unsigned i = 0; i < 4; ++i) value |= std::to_integer<std::uint32_t>(bytes[at+i]) << (i*8);
        return value;
    }
    float scalar(std::size_t at) {
        const auto value = std::bit_cast<float>(word(at));
        need(std::isfinite(value), RetailSsfGeometryError::geometry, "SSF geometry finite source scalar", at);
        return value;
    }
    std::uint16_t half(std::size_t at) {
        need(at < bytes.size() && bytes.size()-at >= 2, RetailSsfGeometryError::range, "SSF geometry bounded halfword", at);
        return std::to_integer<std::uint16_t>(bytes[at]) | (std::to_integer<std::uint16_t>(bytes[at+1]) << 8);
    }
    struct Chunk { std::uint32_t id; std::size_t at, begin, end; };
    std::vector<Chunk> children(std::size_t begin, std::size_t end) {
        need(begin <= end && end <= bytes.size(), RetailSsfGeometryError::range, "SSF geometry bounded child extent", begin);
        std::vector<Chunk> result;
        while (begin != end) {
            need(end-begin >= 12, RetailSsfGeometryError::chunk, "SSF geometry trailing or truncated child header", begin);
            const auto id = word(begin), count = word(begin+4), stamp = word(begin+8);
            const auto child_end=ssf_geometry_detail::bounded_chunk_end(begin,count,end);
            need(bool(child_end), RetailSsfGeometryError::chunk, "SSF geometry child crosses parent", begin);
            need(stamp == 0x1803ffffu, RetailSsfGeometryError::version, "SSF geometry unsupported serialization version", begin+8);
            result.push_back({id,begin,begin+12,*child_end}); begin=*child_end;
        }
        return result;
    }
    void shape(const Chunk& c, std::uint32_t id, std::size_t count, const char* operation) {
        need(c.id == id && c.end-c.begin == count, RetailSsfGeometryError::chunk, operation, c.at);
    }
    std::string text(const Chunk& c) {
        need(c.id == 2 && c.begin < c.end, RetailSsfGeometryError::material, "SSF material bounded string chunk", c.at);
        const auto start = bytes.begin()+c.begin, end = bytes.begin()+c.end;
        const auto zero = std::find(start,end,std::byte{});
        need(zero != end, RetailSsfGeometryError::material, "SSF material terminated string", c.begin);
        // Original strings end at NUL. The complete bounded chunk can contain
        // opaque writer padding after it; authentic SSF does not zero it.
        return std::string(reinterpret_cast<const char*>(&*start),std::size_t(zero-start));
    }
    RetailSsfGeometryMaterial material(const Chunk& c) {
        auto parts = children(c.begin,c.end);
        need(c.id == 7 && parts.size() >= 2, RetailSsfGeometryError::material, "SSF material complete structure", c.at);
        shape(parts[0],1,28,"SSF material source structure size");
        RetailSsfGeometryMaterial result;
        const auto p = parts[0].begin;
        result.flags = word(p);
        for (unsigned i=0;i<4;++i) result.stored_color[i]=std::to_integer<std::uint8_t>(bytes[p+4+i]);
        // p+8 is an original serialized pointer placeholder, never a native identity.
        need(word(p+12) <= 1, RetailSsfGeometryError::material, "SSF material textured boolean", p+12);
        result.textured = word(p+12) != 0;
        for(unsigned i=0;i<3;++i) result.surface[i]=scalar(p+16+i*4);
        std::size_t index=1;
        if(result.textured) {
            need(index < parts.size() && parts[index].id == 6, RetailSsfGeometryError::material, "SSF material explicit texture reference", c.at);
            const auto texture = children(parts[index].begin,parts[index].end);
            need(texture.size()==4, RetailSsfGeometryError::material, "SSF material texture child partition", parts[index].at);
            shape(texture[0],1,4,"SSF texture sampler structure");
            result.filter_addressing=word(texture[0].begin);
            result.texture_name=text(texture[1]); result.mask_name=text(texture[2]);
            need(!result.texture_name.empty(),RetailSsfGeometryError::material,"SSF material empty required texture name",texture[1].at);
            shape(texture[3],3,16,"SSF texture extension partition");
            auto extensions=children(texture[3].begin,texture[3].end);
            need(extensions.size()==1, RetailSsfGeometryError::unsupported_plugin,"SSF texture unknown extension",texture[3].at);
            shape(extensions[0],0x110,4,"SSF texture mip extension");
            result.texture_mip_metadata=word(extensions[0].begin);
            ++index;
        }
        need(parts.size()==index+1 && parts[index].id==3,RetailSsfGeometryError::material,"SSF material extension partition",c.at);
        const auto ext=children(parts[index].begin,parts[index].end);
        need(ext.size()==1,RetailSsfGeometryError::unsupported_plugin,"SSF material unknown extension",parts[index].at);
        shape(ext[0],0x895303,28,"SSF material version-5 plain extension extent");
        for(unsigned i=0;i<7;++i)result.plugin_words[i]=word(ext[0].begin+i*4);
        need(result.plugin_words[0]==5,RetailSsfGeometryError::unsupported_plugin,"SSF material unsupported extension version or flags",ext[0].begin);
        return result;
    }
};
}

std::optional<RetailSsfGeometryData> decode_retail_ssf_geometry(std::span<const std::byte> bytes,
    RetailSsfGeometrySelection selection, RetailSsfGeometryFailure& failure) noexcept {
    failure={};
    try {
        Reader r{bytes,failure}; const auto roots=r.children(0,bytes.size());
        r.need(roots.size()==1&&roots[0].id==16,RetailSsfGeometryError::chunk,"SSF geometry requires one complete clump",0);
        auto parts=r.children(roots[0].begin,roots[0].end);
        r.need(parts.size()>=4,RetailSsfGeometryError::chunk,"SSF clump required children",0);
        r.shape(parts[0],1,12,"SSF clump structure");
        const auto atomic_count=r.word(parts[0].begin);
        r.need(!r.word(parts[0].begin+4)&&!r.word(parts[0].begin+8),RetailSsfGeometryError::unsupported_plugin,"SSF clump lights or cameras require their decoder",parts[0].at);
        r.need(selection.atomic_index<atomic_count,RetailSsfGeometryError::selection,"SSF explicit atomic ordinal out of range",parts[0].at);
        r.need(parts[1].id==14&&parts[2].id==26,RetailSsfGeometryError::chunk,"SSF clump frame and geometry order",parts[1].at);
        r.need(atomic_count<=parts.size()-4&&parts.size()==std::size_t(atomic_count)+4,RetailSsfGeometryError::chunk,"SSF clump atomic count and complete partition",parts[0].at);
        r.shape(parts.back(),3,0,"SSF clump unknown extension");
        RetailSsfGeometryData result;result.atomic_index=selection.atomic_index;result.morph_index=selection.morph_index;
        const auto frame_chunks=r.children(parts[1].begin,parts[1].end);
        r.need(!frame_chunks.empty()&&frame_chunks[0].id==1&&frame_chunks[0].end-frame_chunks[0].begin>=4,RetailSsfGeometryError::frame,"SSF frame list structure",parts[1].at);
        const auto frame_count=r.word(frame_chunks[0].begin);
        r.need(frame_count<= (frame_chunks[0].end-frame_chunks[0].begin-4)/56 &&
            frame_chunks[0].end-frame_chunks[0].begin==4+std::size_t(frame_count)*56 && frame_chunks.size()==std::size_t(frame_count)+1,
            RetailSsfGeometryError::frame,"SSF exact frame list and extension counts",frame_chunks[0].at);
        result.frames.reserve(frame_count);
        for(std::size_t i=0;i<frame_count;++i) {
            r.shape(frame_chunks[i+1],3,0,"SSF frame plugin remains unsupported");
            const auto p=frame_chunks[0].begin+4+i*56;RetailSsfGeometryFrame frame;
            float v[12];for(unsigned j=0;j<12;++j)v[j]=r.scalar(p+j*4);
            frame.modelling={{v[0],v[1],v[2]},{v[3],v[4],v[5]},{v[6],v[7],v[8]},{v[9],v[10],v[11]}};
            frame.parent=std::bit_cast<std::int32_t>(r.word(p+48));frame.flags=r.word(p+52);
            r.need(frame.parent>=-1 && (frame.parent<0||std::size_t(frame.parent)<frame_count),RetailSsfGeometryError::frame,"SSF frame parent index",p+48);
            result.frames.push_back(frame);
        }
        for(std::size_t i=0;i<frame_count;++i) {
            auto current=std::int64_t(i);std::size_t steps=0;
            while(current>=0) {r.need(++steps<=frame_count,RetailSsfGeometryError::frame,"SSF cyclic frame ancestry",parts[1].at);current=result.frames[std::size_t(current)].parent;}
        }
        const auto geometry_chunks=r.children(parts[2].begin,parts[2].end);
        r.need(!geometry_chunks.empty(),RetailSsfGeometryError::geometry,"SSF missing geometry list",parts[2].at);
        r.shape(geometry_chunks[0],1,4,"SSF geometry list structure");
        const auto geometry_count=r.word(geometry_chunks[0].begin);
        r.need(geometry_chunks.size()==std::size_t(geometry_count)+1,RetailSsfGeometryError::geometry,"SSF geometry list count",geometry_chunks[0].at);
        for(std::size_t i=0;i<atomic_count;++i) {
            const auto& atomic=parts[3+i];r.need(atomic.id==20,RetailSsfGeometryError::chunk,"SSF atomic child type",atomic.at);
            const auto ac=r.children(atomic.begin,atomic.end);r.need(ac.size()==2,RetailSsfGeometryError::chunk,"SSF atomic partition",atomic.at);
            r.shape(ac[0],1,16,"SSF atomic structure");
            const auto frame=r.word(ac[0].begin),geometry=r.word(ac[0].begin+4);
            r.need(frame<frame_count&&geometry<geometry_count&&!r.word(ac[0].begin+12),RetailSsfGeometryError::topology,"SSF atomic frame or geometry index",ac[0].at);
            r.need(ac[1].id==3,RetailSsfGeometryError::chunk,"SSF atomic extension",ac[1].at);
            const auto ae=r.children(ac[1].begin,ac[1].end);r.need(ae.size()==1,RetailSsfGeometryError::unsupported_plugin,"SSF atomic unknown extension",ac[1].at);
            r.shape(ae[0],0x895302,16,"SSF atomic version-3 extension extent");
            r.need(r.word(ae[0].begin)==3,RetailSsfGeometryError::unsupported_plugin,"SSF atomic unsupported extension version",ae[0].begin);
            if(i==selection.atomic_index) {
                result.frame_index=frame;result.geometry_index=geometry;result.atomic_flags=r.word(ac[0].begin+8);
                for(unsigned j=0;j<4;++j)result.atomic_plugin_words[j]=r.word(ae[0].begin+j*4);
            }
        }
        const auto& geometry=geometry_chunks[result.geometry_index+1];
        r.need(geometry.id==15,RetailSsfGeometryError::geometry,"SSF selected geometry chunk type",geometry.at);
        // Read the fixed header before walking STRUCT extent: retail native
        // geometries may have stream-consumer lengths that are not nesting bounds.
        r.need(geometry.end-geometry.begin>=28&&r.word(geometry.begin)==1,RetailSsfGeometryError::geometry,"SSF selected geometry fixed header",geometry.at);
        result.geometry_flags=r.word(geometry.begin+12);
        r.need(!(result.geometry_flags&0x01000000u),RetailSsfGeometryError::native_geometry_required,"SSF native PS2 geometry needs its proved attribute decoder",geometry.begin+12);
        const auto gc=r.children(geometry.begin,geometry.end);
        r.need(gc.size()==3&&gc[0].id==1&&gc[1].id==8&&gc[2].id==3,RetailSsfGeometryError::geometry,"SSF non-native geometry child partition",geometry.at);
        auto p=gc[0].begin;const auto flags=r.word(p),triangle_count=r.word(p+4),vertex_count=r.word(p+8),morph_count=r.word(p+12);p+=16;
        r.need(vertex_count&&triangle_count&&morph_count&&selection.morph_index<morph_count,RetailSsfGeometryError::selection,"SSF geometry vertex triangle and selected morph counts",gc[0].begin);
        r.need((flags&~0x000100ffu)==0 && ((flags>>16)&0xff)==1 && (flags&0x4u),RetailSsfGeometryError::geometry,"SSF geometry unsupported flags or UV count",gc[0].begin);
        const auto fits=[&](std::size_t count,std::size_t size){r.need(p<=gc[0].end&&count<=(gc[0].end-p)/size,RetailSsfGeometryError::range,"SSF geometry attribute extent",p);};
        result.morph_count=morph_count;result.has_colors=(flags&8)!=0;result.has_uv=true;result.has_normals=(flags&0x10)!=0;
        fits(vertex_count,1);result.vertices.resize(vertex_count);
        if(result.has_colors) {fits(vertex_count,4);for(auto& v:result.vertices)for(auto& c:v.stored_color)c=std::to_integer<std::uint8_t>(bytes[p++]);}
        fits(vertex_count,8);for(auto& v:result.vertices){v.uv={r.scalar(p),r.scalar(p+4)};p+=8;}
        fits(triangle_count,8);result.triangles.reserve(triangle_count);
        for(std::size_t i=0;i<triangle_count;++i) {
            // 22BD90 retains this asset's native triangle array directly. The
            // observed SSF layout is(v0,v1,v2,material), not the standard RW
            // portable-stream word permutation used by the older fixture path.
            RetailSsfGeometryTriangle t{{r.half(p),r.half(p+2),r.half(p+4)},r.half(p+6)};p+=8;
            for(const auto index:t.vertices)r.need(index<vertex_count,RetailSsfGeometryError::topology,"SSF source triangle vertex index",p-8);
            result.triangles.push_back(t);
        }
        fits(morph_count,24);
        for(std::size_t i=0;i<morph_count;++i) {
            fits(1,24);const RetailRendererPoint3 center{r.scalar(p),r.scalar(p+4),r.scalar(p+8)};const auto radius=r.scalar(p+12);
            const auto has_vertices=r.word(p+16),has_normals=r.word(p+20);p+=24;
            r.need(radius>=0&&has_vertices<=1&&has_normals<=1,RetailSsfGeometryError::geometry,"SSF source morph sphere or attribute booleans",p-24);
            if(i==selection.morph_index){r.need(has_vertices,RetailSsfGeometryError::geometry,"SSF selected morph has no source vertices",p-8);result.bound_center=center;result.bound_radius=radius;}
            for(unsigned attribute=0;attribute<2;++attribute)if(attribute?has_normals:has_vertices) {
                // Original22BD90 aligns each source position/normal array to16,
                // then skips(16-leadingPadding) after its12*vertexCount bytes.
                // Serialized coordinates are used; native allocation addresses
                // cannot change the asset interpretation.
                const auto residue=(selection.clump_offset_in_section%16+p%16)%16;
                const auto padding=std::size_t((16-residue)%16);
                fits(padding,1);p+=padding;
                fits(vertex_count,12);
                for(auto& v:result.vertices) {const RetailRendererPoint3 value{r.scalar(p),r.scalar(p+4),r.scalar(p+8)};p+=12;if(i==selection.morph_index){if(attribute)v.normal=value;else v.position=value;}}
                fits(1,16-padding);p+=16-padding;
            }
            r.need(bool(has_normals)==result.has_normals,RetailSsfGeometryError::geometry,"SSF morph normals disagree with source flags",p);
        }
        r.need(p==gc[0].end,RetailSsfGeometryError::geometry,"SSF unexplained non-native geometry structure bytes",p);
        const auto ml=r.children(gc[1].begin,gc[1].end);r.need(!ml.empty()&&ml[0].id==1,RetailSsfGeometryError::material,"SSF material list structure",gc[1].at);
        const auto material_count=r.word(ml[0].begin);
        r.need(material_count&&ml[0].end-ml[0].begin==4+std::uint64_t(material_count)*4&&ml.size()==std::size_t(material_count)+1,RetailSsfGeometryError::material,"SSF material list count or aliases",ml[0].at);
        for(std::size_t i=0;i<material_count;++i) {
            r.need(r.word(ml[0].begin+4+i*4)==0xffffffffu,RetailSsfGeometryError::material,"SSF aliased material list requires its owner mapping",ml[0].begin+4+i*4);
            result.materials.push_back(r.material(ml[i+1]));
        }
        for(const auto& t:result.triangles)r.need(t.material<result.materials.size(),RetailSsfGeometryError::topology,"SSF source triangle material index",gc[0].at);
        const auto ext=r.children(gc[2].begin,gc[2].end);
        r.need(ext.size()==1&&ext[0].id==0x50e,RetailSsfGeometryError::unsupported_plugin,"SSF unknown geometry extension",gc[2].at);
        // The BINMESH index stream is retained source corroboration, not a VIF
        // device. Rendering uses the independently serialized triangle records.
        const auto& bm=ext[0];r.need(bm.end-bm.begin>=12,RetailSsfGeometryError::topology,"SSF binmesh header",bm.at);
        auto q=bm.begin;const auto strip=r.word(q),meshes=r.word(q+4),indices=r.word(q+8);q+=12;
        r.need(strip<=1&&meshes&&meshes<=material_count,RetailSsfGeometryError::topology,"SSF binmesh topology or material count",bm.at);
        std::uint64_t seen=0;
        std::vector<std::array<std::size_t,4>> mesh_triangles;
        const auto canonical=[](std::size_t a,std::size_t b,std::size_t c,std::size_t material) {
            const std::array<std::size_t,3> first{a,b,c},second{b,c,a},third{c,a,b};
            const auto v=std::min({first,second,third});return std::array<std::size_t,4>{v[0],v[1],v[2],material};
        };
        for(std::size_t i=0;i<meshes;++i) {
            r.need(bm.end-q>=8,RetailSsfGeometryError::topology,"SSF binmesh record",q);
            const auto count=r.word(q),material=r.word(q+4);q+=8;
            r.need(material<material_count&&count<=(bm.end-q)/4,RetailSsfGeometryError::topology,"SSF binmesh material or index extent",q-8);
            std::vector<std::size_t> stream;stream.reserve(count);
            for(std::size_t j=0;j<count;++j){const auto value=r.word(q);r.need(value<vertex_count,RetailSsfGeometryError::topology,"SSF binmesh source index",q);stream.push_back(value);q+=4;}
            r.need(strip||count%3==0,RetailSsfGeometryError::topology,"SSF binmesh triangle-list remainder",q);
            for(std::size_t j=0;j+2<stream.size();j+=strip?1:3) {
                const auto a=stream[j+(strip&&(j&1)?1:0)],b=stream[j+(strip&&(j&1)?0:1)],c=stream[j+2];
                if(a!=b&&a!=c&&b!=c)mesh_triangles.push_back(canonical(a,b,c,material));
            }
            seen+=count;
        }
        r.need(q==bm.end&&seen==indices,RetailSsfGeometryError::topology,"SSF complete binmesh extent and count",q);
        std::vector<std::array<std::size_t,4>> source_triangles;source_triangles.reserve(result.triangles.size());
        for(const auto& t:result.triangles)source_triangles.push_back(canonical(t.vertices[0],t.vertices[1],t.vertices[2],t.material));
        std::sort(source_triangles.begin(),source_triangles.end());std::sort(mesh_triangles.begin(),mesh_triangles.end());
        r.need(source_triangles==mesh_triangles,RetailSsfGeometryError::topology,"SSF triangle array and binmesh oriented topology disagree",bm.at);
        return result;
    } catch(const Rejected&) {}
      catch(const std::bad_alloc&){failure={RetailSsfGeometryError::allocation,"SSF geometry native allocation",0};}
      catch(const std::length_error&){failure={RetailSsfGeometryError::allocation,"SSF geometry host capacity",0};}
      catch(...){failure={RetailSsfGeometryError::native_exception,"SSF geometry native exception",0};}
    return {};
}
} // namespace mka::retail
