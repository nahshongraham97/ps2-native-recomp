// Native file worker -> full actual sysart image conversion -> production16FC70.
// Owns a never-visible test WGL context; no game window/swap/audio is permitted.
#include "mka/retail_loaded_system_art.hpp"
#include "mka/retail_initial_reset_required.hpp"
#include "mka/retail_main.hpp"
#include "mka/movie_gl_device.hpp"
#include "mka/font_layout.hpp"
#include "mka/font_geometry.hpp"
#include "native_retail_startup.hpp"
#include "native_test_sha256.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <new>
#include <stdexcept>
#include <thread>
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>
using namespace mka::retail;
using namespace mka::graphics;
using namespace mka::video;
extern "C" int SDL_InitSubSystem(unsigned) { std::abort(); }
namespace { thread_local int allocation_cut=-1; }
void* operator new(std::size_t n) {
    if(allocation_cut>=0&&allocation_cut--==0) throw std::bad_alloc();
    if(auto p=std::malloc(n?n:1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
// Permanent-string descendants now link their real production implementations.
// This client still measures the font path; the string client exercises them.
namespace {
void check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
std::string sha(std::span<const std::uint8_t> bytes) {
    NativeTestSha256 h; h.add(bytes.data(),bytes.size()); return h.finish();
}
struct HiddenGl {
    HWND window{}; HDC dc{}; HGLRC context{};
    std::shared_ptr<NativeMovieGpuRetirementOwner> retirement;
    std::shared_ptr<NativeMovieGlDevice> device;
    HiddenGl() {
        WNDCLASSW wc{}; wc.style=CS_OWNDC; wc.lpfnWndProc=DefWindowProcW;
        wc.hInstance=GetModuleHandleW(nullptr); wc.lpszClassName=L"NativeLoadedSystemArtTest";
        check(RegisterClassW(&wc)||GetLastError()==ERROR_CLASS_ALREADY_EXISTS,"test window class");
        window=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP|WS_DISABLED,
            0,0,64,64,nullptr,nullptr,wc.hInstance,nullptr);
        check(window&&!IsWindowVisible(window),"never-visible native test window");
        dc=GetDC(window); PIXELFORMATDESCRIPTOR p{};
        p.nSize=sizeof(p); p.nVersion=1; p.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;
        p.iPixelType=PFD_TYPE_RGBA; p.cColorBits=32; p.cAlphaBits=8; p.iLayerType=PFD_MAIN_PLANE;
        const auto format=ChoosePixelFormat(dc,&p);
        check(format&&SetPixelFormat(dc,format,&p),"test native pixel format");
        context=wglCreateContext(dc); check(context&&wglMakeCurrent(dc,context),"test current WGL context");
        MovieGpuError error;
        retirement=NativeMovieGpuRetirementOwner::create(error);
        device=NativeMovieGlDevice::attach_current_context(window,1,retirement,error);
        check(device&&error==MovieGpuError::none,"actual native GPU owner");
    }
    ~HiddenGl() {
        for(;;) {
            const auto result=device->shutdown();
            if(result==MovieGpuError::none) break;
            if(result!=MovieGpuError::pending) std::abort();
            glFinish();
        }
        check(retirement->poll().state==MovieFenceState::complete,"real context retirement completed");
        check(!IsWindowVisible(window),"fixture was never visible");
        device.reset(); retirement.reset(); wglMakeCurrent(nullptr,nullptr);
        wglDeleteContext(context); ReleaseDC(window,dc); DestroyWindow(window);
    }
};
struct Game {
    mka::input::ControllerInput input;
    std::unique_ptr<NativeRetailStartupState> startup;
    std::unique_ptr<NativeRetailMainState> main;
    std::shared_ptr<NativeRetailLoadedSystemArt> owner;
    explicit Game(const std::filesystem::path& root,bool make_owner=true) {
        mka::content::ContentVolumeFailure cf;
        auto volume=mka::content::NativeContentVolume::open(root,cf); check(bool(volume),"actual content volume");
        startup=std::make_unique<NativeRetailStartupState>(std::move(*volume),std::vector<std::string>{});
        StartupFailure f; check(initialize_native_retail_game_session(*startup,f),"actual persistent Session");
        main=std::make_unique<NativeRetailMainState>(*startup,input);
        RetailFilePathFailure pf; main->file_paths=NativeRetailFilePaths::create(pf);
        RetailPackageFailure bf;
        main->package=NativeRetailPackage::open(startup->content,*main->file_paths,bf);
        check(bool(main->package),"actual loose package owner");
        RetailLoadedArtFailure af;
        if(make_owner) {
            owner=NativeRetailLoadedSystemArt::for_session(startup->game_session,af);
            check(bool(owner),"actual persistent loaded owner");
            RetailGameSessionFailure sf;
            check(owner==startup->game_session->loaded_system_art(sf),"same actual Session owner");
        }
    }
    void queue(int language) {
        RetailLoadedArtFailure f;
        check(owner->queue_from_19b530(*main->package,language,f),"actual asynchronous sysart read");
    }
    void load(const std::shared_ptr<NativeGlTextureUpload>& device) {
        RetailLoadedArtFailure f; const auto deadline=GetTickCount64()+10000;
        for(;;) {
            auto step=owner->advance_from_19aa30(device,f);
            if(!step) {
                std::fprintf(stderr,"load kind=%d operation=%s ordinal=%zu\n",int(f.kind),f.operation,f.ordinal);
                check(false,"native section conversion");
            }
            if(*step==RetailLoadedArtStep::complete) break;
            check(GetTickCount64()<deadline,"bounded native worker completion"); Sleep(1);
        }
    }
    void retire() {
        RetailLoadedArtFailure f; const auto deadline=GetTickCount64()+10000;
        for(;;) {
            auto step=owner->retire_from_19cfa0(f);
            check(bool(step),"actual native section retirement");
            if(*step==RetailLoadedArtStep::complete) break;
            check(GetTickCount64()<deadline,"bounded native cancellation"); Sleep(1);
        }
    }
};
struct FailAfterRealUploads final : NativeGlTextureUpload {
    std::shared_ptr<NativeGlTextureUpload> actual;
    std::vector<GLuint> created;
    explicit FailAfterRealUploads(std::shared_ptr<NativeGlTextureUpload> p):actual(std::move(p)){}
    NativeTextureUpload upload_rgba_image(const NativeTextureRgbaImage& image,NativeTextureSampler sampler) override {
        if(created.size()==3) return {{NativeTextureError::unavailable,"explicit test fourth-upload failure"},{}};
        auto result=actual->upload_rgba_image(image,sampler);
        if(result.texture) {
            NativeTextureFailure f; auto name=result.texture->gl_object(f);
            check(bool(name),"real prefix image"); created.push_back(*name);
        }
        return result;
    }
    bool revoke_image(const std::shared_ptr<const NativeGlTexture>& image,NativeTextureFailure& f) override {
        return actual->revoke_image(image,f);
    }
    bool set_image_sampler(const std::shared_ptr<const NativeGlTexture>& image,NativeTextureSampler sampler,NativeTextureFailure& f) override {
        return actual->set_image_sampler(image,sampler,f);
    }
};
void mutate_word(const std::filesystem::path& path,std::size_t offset,std::uint32_t word) {
    std::fstream f(path,std::ios::binary|std::ios::in|std::ios::out); check(bool(f),"disposable file writer");
    f.seekp(std::streamoff(offset));
    for(unsigned i=0;i<4;++i) f.put(char(word>>(8*i)));
    check(bool(f),"disposable word mutation");
}
void rename_record(const std::filesystem::path& path,std::size_t ordinal) {
    std::fstream f(path,std::ios::binary|std::ios::in|std::ios::out); check(bool(f),"disposable name writer");
    f.seekg(std::streamoff(0x800+0x1c+ordinal*16+12));
    std::array<unsigned char,4> b{}; f.read(reinterpret_cast<char*>(b.data()),4);
    const auto relative=std::uint32_t(b[0])|(std::uint32_t(b[1])<<8)|(std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);
    f.seekp(std::streamoff(0x800+0x1c+34*16+relative)); f.put('X');
    check(bool(f),"disposable source name change");
}
}
int main(int argc,char** argv) try {
    check(argc==3,"actual loose root and exclusive disposable fixture root");
    const auto fixtures=std::filesystem::path(argv[2]);
    check(!std::filesystem::exists(fixtures),"new fixtures only");
    std::filesystem::create_directories(fixtures);
    HiddenGl host; RetailLoadedArtFailure f; StartupFailure sf;
    check(font_zero_sampler_from_16fc70(0xabcd1102u)==0xabcd3301u,"source upper sampler bits preserved");
    check(font_zero_sampler_from_16fc70(0xffffffffu)==0xffff3301u,"source bitwise masks do not sign-extend stored word");
    check(!native_sampler_from_retail_texture(0x1104u,f)&&f.kind==RetailLoadedArtError::sampler,"unproved mip filter refused");
    check(!native_sampler_from_retail_texture(0x1201u,f)&&f.kind==RetailLoadedArtError::sampler,"unproved address mode refused");
    {
        unsigned failures=0;
        for(int cut=0;cut<8;++cut) {
            Game g(argv[1],false); allocation_cut=cut;
            auto owner=NativeRetailLoadedSystemArt::for_session(g.startup->game_session,f);
            allocation_cut=-1;
            if(owner) break;
            check(f.kind==RetailLoadedArtError::allocation,"owner allocation failure names its boundary");
            RetailGameSessionFailure gf;
            check(!g.startup->game_session->loaded_system_art(gf),"failed construction publishes no owner");
            ++failures;
        }
        check(failures==2,"native owner and shared-control construction cuts");
    }
    for(int language=0;language<5;++language) {
        Game g(argv[1]); g.queue(language);
        const auto queued=g.owner->observe(f);
        check(queued&&queued->phase==RetailLoadedArtPhase::reading&&queued->pending_job_retained,"queued job publication");
        check(initial_reset_required::configure_selected_resource_zero_16fc70(*g.main,sf),"production binding preserves successful empty slot");
        check(g.owner->observe(f)->metrics_lookups==0,"null texture skips MET lookup");
        g.load(host.device);
        const auto ready=g.owner->observe(f);
        check(ready&&ready->phase==RetailLoadedArtPhase::converted&&ready->converted_textures==21&&
              ready->directory_records==34&&!ready->pending_job_retained,"all images then job release then converted");
        check(initial_reset_required::configure_selected_resource_zero_16fc70(*g.main,sf),"actual production16FC70 binding");
        auto font=g.owner->configure_zero_from_16fc70(f);
        check(font&&font->texture&&font->metrics,"actual selected texture/MET owners");
        const auto atlas=font->texture->atlas(f);
        check(atlas&&atlas->serialized().size()==34560&&font->metrics->raw_bytes().size()==8116,"original bounded font payloads");
        if(language==0) check(sha(font->metrics->raw_bytes())=="b91b6ee00f3bed46a1e89471c247ea659b88af927d7bd9576311b57fd0cfd497","original metrics identity");
        mka::text::FontMetricsFailure mf;
        check(font->metrics->advance(32,mf)==6,"source special-space advance");
        check(font->texture->sampler_metadata(f)==0x3301u,"actual configured source sampler");
        auto image=font->texture->image(f); NativeTextureFailure tf;
        const auto name=image->gl_object(tf); check(bool(name),"actual native font image");
        glBindTexture(GL_TEXTURE_2D,*name);
        GLint filter=0,u=0,v=0; glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,&filter);
        glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,&u); glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,&v);
        check(filter==GL_NEAREST&&u==0x812f&&v==0x812f,"source3301 maps to actual nearest/clamp sampler");
        std::vector<std::uint8_t> pixels(atlas->rgba_raw_alpha().size());
        glPixelStorei(GL_PACK_ALIGNMENT,1); glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        check(pixels==atlas->rgba_raw_alpha()&&glGetError()==GL_NO_ERROR,"actual selected font GPU bytes");
        const std::array<std::uint8_t,1> text{{'P'}};
        mka::text::FontLayoutFailure lf;
        auto layout=mka::text::layout_font_bytes(*font->metrics,text,{144,48,320,410,18},lf);
        check(layout&&layout->glyphs.size()==1,"selected owner uses existing native layout");
        mka::text::FontGeometryFailure gf;
        auto geometry=mka::text::geometry_font_glyph(layout->glyphs[0],
            {640,448,640,448,atlas->width(),atlas->height(),mka::text::FontVerticalScale::one},gf);
        check(geometry&&geometry->x1-geometry->x0==15&&geometry->y1-geometry->y0==14&&
              geometry->u0_sixteenths==720&&geometry->u1_sixteenths==960&&
              geometry->v0_sixteenths==1056&&geometry->v1_sixteenths==1280,
              "original P metrics and independent existing geometry expectations");
        const auto before=g.owner->observe(f);
        check(initial_reset_required::configure_selected_resource_zero_16fc70(*g.main,sf),"source cache hit");
        const auto after=g.owner->observe(f);
        check(before->texture_lookups==after->texture_lookups&&before->metrics_lookups==after->metrics_lookups,"cache hit does not repeat lookup");
        bool wrong=false;
        std::thread t([&]{RetailLoadedArtFailure other;wrong=!g.owner->observe(other)&&other.kind==RetailLoadedArtError::wrong_thread;});
        t.join(); check(wrong,"foreign thread refused before GPU/native record access");
        glBindTexture(GL_TEXTURE_2D,0);
        if(language==0) {
            glBindTexture(0xffffffffu,0);
            check(!g.owner->retire_from_19cfa0(f)&&f.kind==RetailLoadedArtError::texture,"actual pre-delete error retains resource owner");
            check(glIsTexture(*name),"failed pre-delete cleanup did not delete actual storage");
        }
        g.retire();
        check(!glIsTexture(*name)&&!font->texture->image(f),"actual cleanup invalidates retained font capability");
        check(!initial_reset_required::configure_selected_resource_zero_16fc70(*g.main,sf),"stale cache never silently resurrects");
    }
    {
        Game g(argv[1]); check(!g.owner->queue_from_19b530(*g.main->package,-1,f),"negative language refused");
        check(!g.owner->queue_from_19b530(*g.main->package,5,f),"unproved language refused");
        g.queue(0); auto faulty=std::make_shared<FailAfterRealUploads>(host.device);
        const auto deadline=GetTickCount64()+10000;
        for(;;) {
            const auto step=g.owner->advance_from_19aa30(faulty,f);
            if(!step) break;
            check(*step==RetailLoadedArtStep::pending&&GetTickCount64()<deadline,"partial failure bounded wait"); Sleep(1);
        }
        const auto failed=g.owner->observe(f);
        check(failed&&failed->phase==RetailLoadedArtPhase::failed&&failed->converted_textures==3&&
              failed->pending_job_retained&&faulty->created.size()==3,"failed GPU prefix retained without converted publication");
        check(!initial_reset_required::configure_selected_resource_zero_16fc70(*g.main,sf),"failed conversion is not an empty-font success");
        g.retire(); for(const auto name:faulty->created) check(!glIsTexture(name),"real partial prefix retired once");
        g.queue(0); g.load(host.device); g.retire();
    }
    {
        Game g(argv[1]); g.queue(0); const auto deadline=GetTickCount64()+10000;
        for(;;) {
            allocation_cut=0;
            const auto step=g.owner->advance_from_19aa30(host.device,f);
            allocation_cut=-1;
            if(!step) break;
            check(*step==RetailLoadedArtStep::pending&&GetTickCount64()<deadline,"bounded allocation-cut read");
            Sleep(1);
        }
        check(f.kind==RetailLoadedArtError::allocation,"completed-read CPU staging allocation failure");
        const auto failed=g.owner->observe(f);
        check(failed&&failed->phase==RetailLoadedArtPhase::failed&&failed->pending_job_retained&&
              failed->converted_textures==0,"allocation failure retains actual job with no GPU publication");
        g.retire();
    }
    {
        Game g(argv[1]); g.queue(0); g.load(host.device);
        glBindTexture(0xffffffffu,0);
        check(!initial_reset_required::configure_selected_resource_zero_16fc70(*g.main,sf),"actual sampler failure blocks production binding");
        const auto failed=g.owner->observe(f);
        check(failed&&failed->phase==RetailLoadedArtPhase::failed&&!failed->font_zero_cached,"failed sampler never publishes cache");
        check(!g.owner->configure_zero_from_16fc70(f),"possible partial sampler failure remains sticky");
        g.retire();
    }
    for(unsigned variant=0;variant<3;++variant) {
        const auto root=fixtures/("source-"+std::to_string(variant));
        std::filesystem::create_directories(root/"art");
        const auto file=root/"art/gameart.ssf";
        std::filesystem::copy_file(std::filesystem::path(argv[1])/"art/gameart.ssf",file);
        if(variant==0) rename_record(file,4);
        if(variant==1) mutate_word(file,0x800+0x1c+4*16+4,0);
        if(variant==2) rename_record(file,17);
        Game g(root); g.queue(0); g.load(host.device);
        check(initial_reset_required::configure_selected_resource_zero_16fc70(*g.main,sf),"source missing/null resource result");
        const auto font=g.owner->configure_zero_from_16fc70(f);
        check(font&&bool(font->texture)==(variant!=2)&&!font->metrics,"source texture may cache independently of MET");
        if(variant==1) {
            std::uint32_t length=7; const auto bytes=g.owner->find_data_from_300440("EDDMONDGRAY14_MET",length,f);
            check(bytes&&bytes->empty()&&length==8116,"matched-null payload still writes source length");
        }
        if(variant==2) check(g.owner->observe(f)->metrics_lookups==0,"missing atlas never looks up MET");
        g.retire();
    }
    {
        Game g(argv[1]); g.queue(0);
        const std::weak_ptr<NativeRetailGameSession> last_owner=g.startup->game_session;
        g.main.reset(); g.startup->game_record_reset.reset(); g.startup->game_session.reset();
        check(last_owner.expired(),"all actual declared Session owners released");
        check(!g.owner->observe(f)&&f.kind==RetailLoadedArtError::session,"retired Session not retained by its child");
        g.retire(); // Native file cleanup remains valid after Session retirement.
    }
    check(!IsWindowVisible(host.window)&&wglGetCurrentContext()==host.context,"no visible window and borrowed context preserved");
    std::puts("PASS actual native read/all105 sysart images/production16FC70/cache/null MET/layout/geometry/partial GPU failure/cancel/retirement; visible_window=false audio_endpoint=false game_acceptance=OPEN");
    return 0;
} catch(const std::exception& e) {
    allocation_cut=-1;
    std::fprintf(stderr,"FAIL loaded system art: %s\n",e.what()); return 1;
}
