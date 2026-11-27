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
        need(ext.size()==1&&ext[0].id==0x895303,RetailSsfGeometryError::unsupported_plugin,"SSF material unknown extension",parts[index].at);
        const auto ext_size=ext[0].end-ext[0].begin;
        need(ext_size>=28,RetailSsfGeometryError::chunk,"SSF material version-5 extension extent",ext[0].at);
        for(unsigned i=0;i<7;++i)result.plugin_words[i]=word(ext[0].begin+i*4);
        if(result.plugin_words[0]==5u) {
            need(ext_size==28,RetailSsfGeometryError::chunk,"SSF material version-5 plain extension extent",ext[0].at);
        } else if(result.plugin_words[0]==0x80000005u) {
            need(ext_size==44,RetailSsfGeometryError::chunk,"SSF material version-5 extended vec4 extent",ext[0].at);
            for(unsigned i=0;i<4;++i)result.material_ext_vec4[i]=scalar(ext[0].begin+28+i*4);
            result.has_material_ext_vec4=true;
        } else {
            reject(RetailSsfGeometryError::unsupported_plugin,"SSF material unsupported extension version or flags",ext[0].begin);
        }
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
            const auto& fc=frame_chunks[i+1];
            r.need(fc.id==3,RetailSsfGeometryError::chunk,"SSF frame extension chunk type",fc.at);
            const auto p=frame_chunks[0].begin+4+i*56;RetailSsfGeometryFrame frame;
            if(fc.end>fc.begin) {
                const auto fext=r.children(fc.begin,fc.end);
                r.need(fext.size()==1&&fext[0].id==0x11eu,RetailSsfGeometryError::unsupported_plugin,"SSF frame plugin remains unsupported",fc.at);
                r.need(fext[0].end-fext[0].begin>=8,RetailSsfGeometryError::unsupported_plugin,"SSF frame HAnim extension extent",fext[0].at);
                r.need(r.word(fext[0].begin)==0x100u,RetailSsfGeometryError::unsupported_plugin,"SSF frame HAnim unsupported version",fext[0].begin);
                frame.hanim_node_id=r.word(fext[0].begin+4);
                if(fext[0].end-fext[0].begin>=16)frame.hanim_flags=r.word(fext[0].begin+12);
            }
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
        if(result.geometry_flags&0x01000000u) {
            result.is_native_ps2_vif=true;
            const auto p0=geometry.begin+12;
            const auto flags=r.word(p0),triangle_count=r.word(p0+4),vertex_count=r.word(p0+8),morph_count=r.word(p0+12);
            r.need(morph_count>=1&&selection.morph_index==0,RetailSsfGeometryError::selection,"SSF native geometry morph selection",p0);
            r.need(vertex_count&&triangle_count,RetailSsfGeometryError::geometry,"SSF native geometry vertex and triangle counts",p0);
            r.need(morph_count<=(geometry.end-(p0+16))/24,RetailSsfGeometryError::range,"SSF native geometry morph extent",p0+16);
            auto mp=p0+16;
            for(std::size_t i=0;i<morph_count;++i) {
                const RetailRendererPoint3 center{r.scalar(mp),r.scalar(mp+4),r.scalar(mp+8)};
                const auto radius=r.scalar(mp+12);
                const auto has_v=r.word(mp+16),has_n=r.word(mp+20);
                r.need(radius>=0&&has_v==0&&has_n==0,RetailSsfGeometryError::geometry,"SSF native geometry morph must have zero non-native arrays",mp);
                if(i==0){result.bound_center=center;result.bound_radius=radius;}
                mp+=24;
            }
            result.morph_count=morph_count;
            result.has_colors=(flags&8)!=0;
            result.has_uv=((flags&4)!=0)||(((flags>>16)&0xffu)>0);
            result.has_normals=(flags&0x10)!=0;

            r.need(geometry.end-mp>=12,RetailSsfGeometryError::material,"SSF native geometry missing material list",mp);
            const auto ml_id=r.word(mp),ml_len=r.word(mp+4),ml_ver=r.word(mp+8);
            r.need(ml_ver==0x1803ffffu,RetailSsfGeometryError::version,"SSF native material list version",mp+8);
            const auto ml_end=ssf_geometry_detail::bounded_chunk_end(mp,ml_len,geometry.end);
            r.need(bool(ml_end)&&ml_id==8,RetailSsfGeometryError::material,"SSF native material list bounds",mp);
            const auto ml=r.children(mp+12,*ml_end);
            r.need(!ml.empty()&&ml[0].id==1,RetailSsfGeometryError::material,"SSF native material list struct",mp);
            const auto material_count=r.word(ml[0].begin);
            r.need(material_count&&ml[0].end-ml[0].begin==4+std::uint64_t(material_count)*4&&ml.size()==std::size_t(material_count)+1,
                RetailSsfGeometryError::material,"SSF native material list count",ml[0].at);
            for(std::size_t i=0;i<material_count;++i) {
                r.need(r.word(ml[0].begin+4+i*4)==0xffffffffu,RetailSsfGeometryError::material,"SSF native aliased material unsupported",ml[0].begin+4+i*4);
                result.materials.push_back(r.material(ml[i+1]));
            }

            auto ep=*ml_end;
            r.need(geometry.end-ep>=12,RetailSsfGeometryError::unsupported_plugin,"SSF native geometry missing extension",ep);
            const auto ex_id=r.word(ep),ex_len=r.word(ep+4),ex_ver=r.word(ep+8);
            r.need(ex_ver==0x1803ffffu,RetailSsfGeometryError::version,"SSF native extension version",ep+8);
            const auto ex_end=ssf_geometry_detail::bounded_chunk_end(ep,ex_len,geometry.end);
            r.need(bool(ex_end)&&ex_id==3,RetailSsfGeometryError::unsupported_plugin,"SSF native extension bounds",ep);
            const auto ext=r.children(ep+12,*ex_end);
            for(const auto& e:ext)if(e.id==0x116u)r.reject(RetailSsfGeometryError::skinned_native_pipe_unsupported,"SSF native geometry RpSkin extension unsupported",e.at);
            r.need(ext.size()==1&&ext[0].id==0x50e,RetailSsfGeometryError::unsupported_plugin,"SSF native geometry requires rwID_BINMESHPLG",ep);
            const auto& bm=ext[0];
            r.need(bm.end-bm.begin>=12,RetailSsfGeometryError::topology,"SSF native binmesh header",bm.at);
            const auto strip=r.word(bm.begin),meshes=r.word(bm.begin+4);
            r.need(strip<=1&&meshes>=1&&meshes<=material_count&&bm.end-bm.begin==12+std::size_t(meshes)*8,
                RetailSsfGeometryError::topology,"SSF native binmesh header counts",bm.at);
            std::vector<std::size_t> mesh_materials(meshes);
            for(std::size_t mi=0;mi<meshes;++mi) {
                const auto mat_idx=r.word(bm.begin+12+mi*8+4);
                r.need(mat_idx<material_count,RetailSsfGeometryError::topology,"SSF native binmesh material index",bm.begin+12+mi*8+4);
                mesh_materials[mi]=mat_idx;
            }

            auto np=*ex_end;
            for(std::size_t mi=0;mi<meshes;++mi) {
                r.need(geometry.end-np>=8,RetailSsfGeometryError::vif_bounds,"SSF native mesh packet header",np);
                const auto pkt_size=r.word(np),align_flag=r.word(np+4);
                (void)align_flag;
                np+=8;
                r.need(pkt_size>=16&&std::size_t(pkt_size)<=geometry.end-np,RetailSsfGeometryError::vif_bounds,"SSF native mesh packet extent",np-8);
                const std::size_t pkt_begin=np,pkt_end=np+std::size_t(pkt_size);
                np=pkt_end;

                static constexpr char kPad128[]="PAD128";
                std::size_t dp=pkt_begin;
                while(dp<pkt_end&&bytes[dp]==static_cast<std::byte>(kPad128[(dp-pkt_begin)%6]))++dp;

                struct BatchVertex {
                    RetailSsfGeometryVertex v{};
                    bool adc{false};
                    bool has_pos{false};
                };
                std::vector<BatchVertex> batch;
                std::uint32_t cl=1;
                auto flush_batch=[&]() {
                    if(batch.empty())return;
                    const std::size_t base_v=result.vertices.size();
                    result.vertices.reserve(base_v+batch.size());
                    for(const auto& bv:batch){
                        r.need(bv.has_pos,RetailSsfGeometryError::geometry,"SSF native VIF batch vertex missing position",dp);
                        result.vertices.push_back(bv.v);
                    }
                    std::size_t strip_len=0;
                    bool prev_adc=false;
                    for(std::size_t vi=0;vi<batch.size();++vi) {
                        if(batch[vi].adc){
                            ++result.adc_strip_breaks;
                            strip_len=prev_adc?2u:1u;
                            prev_adc=true;
                        } else {
                            ++strip_len;
                            prev_adc=false;
                            if(strip_len>=3) {
                                const std::size_t i0=base_v+vi-2,i1=base_v+vi-1,i2=base_v+vi;
                                const bool odd=(strip_len&1u)==0u;
                                result.triangles.push_back({{odd?i1:i0,odd?i0:i1,i2},mesh_materials[mi]});
                            }
                        }
                    }
                    batch.clear();
                };

                bool dma_done=false;
                while(!dma_done) {
                    r.need(pkt_end-dp>=16,RetailSsfGeometryError::dma_chain,"SSF native DMA tag truncated",dp);
                    const auto tag=r.word(dp);
                    const auto qwc=std::size_t(tag&0xffffu);
                    const auto dma_id=(tag>>28)&0x7u;
                    r.need(dma_id==0||dma_id==1||dma_id==6,RetailSsfGeometryError::dma_chain,"SSF native unsupported DMA tag ID",dp);
                    r.need(qwc<=(pkt_end-(dp+16))/16,RetailSsfGeometryError::dma_chain,"SSF native DMA QWC out of bounds",dp);
                    const std::size_t vend=dp+16+qwc*16;
                    std::size_t vp=dp+8;
                    while(vp<vend) {
                        r.need(vend-vp>=4,RetailSsfGeometryError::vif_bounds,"SSF native VIF command truncated",vp);
                        const auto vw=r.word(vp);
                        const auto cmd=(vw>>24)&0x7fu;
                        if(cmd==0x00u||cmd==0x04u||cmd==0x05u||cmd==0x10u||cmd==0x11u||cmd==0x13u) {
                            vp+=4;
                        } else if(cmd==0x01u) {
                            cl=vw&0xffu;
                            vp+=4;
                        } else if(cmd==0x20u) {
                            r.need(vend-vp>=8,RetailSsfGeometryError::vif_bounds,"SSF native VIF STMASK truncated",vp);
                            vp+=8;
                        } else if(cmd==0x30u||cmd==0x31u) {
                            r.need(vend-vp>=20,RetailSsfGeometryError::vif_bounds,"SSF native VIF STROW/STCOL truncated",vp);
                            vp+=20;
                        } else if(cmd==0x14u||cmd==0x15u||cmd==0x17u) {
                            flush_batch();
                            vp+=4;
                        } else if(cmd>=0x60u) {
                            const std::uint32_t vn=(cmd>>2)&3u,vl=cmd&3u;
                            std::size_t num=(vw>>16)&0xffu;
                            if(num==0)num=256;
                            const std::uint32_t addr=vw&0x3ffu,usn=(vw>>14)&1u;
                            const std::uint32_t vnvl=cmd&0x6fu;
                            const std::size_t bits=std::size_t(32u>>vl)*std::size_t(vn+1u);
                            const std::size_t dbytes=((bits*num+31u)/32u)*4u;
                            r.need(dbytes<=vend-(vp+4),RetailSsfGeometryError::vif_bounds,"SSF native VIF UNPACK payload out of bounds",vp);
                            const std::size_t dp_off=vp+4;
                            if(vnvl==0x6cu&&usn==0&&addr==0x000u) {
                                // Bounding sphere per batch
                            } else if(vnvl==0x6du&&usn==1&&addr==0x002u&&cl==1u) {
                                // V4_16 VU1 strip-kick descriptor table preceding vertex batch
                            } else if(vnvl==0x6eu&&usn==1&&addr==0x001u&&cl==1u) {
                                const std::size_t bcount=std::to_integer<std::size_t>(bytes[dp_off]);
                                r.need(bcount>0,RetailSsfGeometryError::vif_bounds,"SSF native VIF batch vertex count zero",vp);
                                batch.assign(bcount,BatchVertex{});
                            } else if(vnvl==0x68u&&usn==0&&(addr==0x011u||addr==0x002u||addr==0x000u)) {
                                r.need(num==batch.size(),RetailSsfGeometryError::vif_bounds,"SSF native VIF position count mismatch",vp);
                                for(std::size_t vi=0;vi<num;++vi) {
                                    batch[vi].v.position={r.scalar(dp_off+vi*12),r.scalar(dp_off+vi*12+4),r.scalar(dp_off+vi*12+8)};
                                    batch[vi].has_pos=true;
                                }
                            } else if(vnvl==0x6au&&usn==0&&(addr==0x012u||addr==0x003u||addr==0x002u)) {
                                r.need(num==batch.size(),RetailSsfGeometryError::vif_bounds,"SSF native VIF normal count mismatch",vp);
                                for(std::size_t vi=0;vi<num;++vi) {
                                    const auto sx=static_cast<std::int8_t>(std::to_integer<std::uint8_t>(bytes[dp_off+vi*3+0]));
                                    const auto sy=static_cast<std::int8_t>(std::to_integer<std::uint8_t>(bytes[dp_off+vi*3+1]));
                                    const auto sz=static_cast<std::int8_t>(std::to_integer<std::uint8_t>(bytes[dp_off+vi*3+2]));
                                    batch[vi].v.normal={float(sx)/127.0f,float(sy)/127.0f,float(sz)/127.0f};
                                }
                            } else if(vnvl==0x64u&&usn==0&&(addr==0x013u||addr==0x004u||addr==0x001u)) {
                                r.need(num==batch.size(),RetailSsfGeometryError::vif_bounds,"SSF native VIF UV count mismatch",vp);
                                for(std::size_t vi=0;vi<num;++vi) {
                                    batch[vi].v.uv={r.scalar(dp_off+vi*8),r.scalar(dp_off+vi*8+4)};
                                }
                            } else if(vnvl==0x66u&&usn==1&&(addr==0x013u||addr==0x004u)) {
                                r.need(num==batch.size(),RetailSsfGeometryError::vif_bounds,"SSF native VIF UV16 count mismatch",vp);
                            } else if(vnvl==0x6eu&&((usn==1&&(addr==0x014u||addr==0x002u||addr==0x005u))||((cl==3u||cl==4u)&&addr==0x001u))) {
                                r.need(num==batch.size(),RetailSsfGeometryError::vif_bounds,"SSF native VIF color count mismatch",vp);
                                for(std::size_t vi=0;vi<num;++vi) {
                                    for(unsigned c=0;c<4;++c)batch[vi].v.stored_color[c]=std::to_integer<std::uint8_t>(bytes[dp_off+vi*4+c]);
                                }
                            } else if(vnvl==0x6cu&&usn==0&&addr==0x004u&&(cl==3u||cl==4u)) {
                                r.need(num==batch.size(),RetailSsfGeometryError::vif_bounds,"SSF native VIF cl3/4 vec4 count mismatch",vp);
                            } else if(vnvl==0x61u&&(addr==0x011u||addr==0x012u||addr==0x002u||addr==0x003u||addr==0x000u)) {
                                r.need(num==batch.size(),RetailSsfGeometryError::vif_bounds,"SSF native VIF ADC count mismatch",vp);
                                if(addr==0x011u||addr==0x002u||addr==0x000u) {
                                    for(std::size_t vi=0;vi<num;++vi)batch[vi].adc=(r.half(dp_off+vi*2)&0x8000u)!=0;
                                }
                            } else {
                                r.reject(RetailSsfGeometryError::unsupported_unpack_format,"SSF native VIF UNPACK format or VU1 address unsupported",vp);
                            }
                            vp+=4+dbytes;
                        } else {
                            r.reject(RetailSsfGeometryError::unsupported_vif_opcode,"SSF native VIF unsupported opcode",vp);
                        }
                    }
                    flush_batch();
                    if(dma_id==0||dma_id==6)dma_done=true;
                    else dp=vend;
                }
            }
            r.need(np==geometry.end,RetailSsfGeometryError::skinned_native_pipe_unsupported,"SSF native geometry trailing skinning/custom pipe unsupported",np);
            r.need(!result.triangles.empty(),RetailSsfGeometryError::topology,"SSF native geometry produced zero triangles",geometry.at);
            return result;
        }
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
