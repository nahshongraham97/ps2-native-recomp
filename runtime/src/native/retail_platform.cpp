#define SDL_MAIN_HANDLED
#include "mka/retail_platform.hpp"
#include "mka/retail_keyboard_sdl.hpp"
#include "mka/movie_gl_device.hpp"
#include "keymap.hpp"
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>
#ifdef _WIN32
#include <SDL_syswm.h>
#endif
#include <algorithm>
#include <atomic>
#include <bit>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <new>
#include <set>
#include <thread>
#include <utility>

namespace mka::retail {
namespace {
bool fail(RetailPlatformFailure& f,RetailPlatformError error,const char* operation,const char* detail="") noexcept {
    f.kind=error;f.operation=operation;try{f.detail=detail;}catch(...){f.detail.clear();}return false;
}
struct Gl {
    PFNGLCREATESHADERPROC CreateShader=nullptr;PFNGLSHADERSOURCEPROC ShaderSource=nullptr;
    PFNGLCOMPILESHADERPROC CompileShader=nullptr;PFNGLGETSHADERIVPROC GetShaderiv=nullptr;
    PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog=nullptr;PFNGLDELETESHADERPROC DeleteShader=nullptr;
    PFNGLCREATEPROGRAMPROC CreateProgram=nullptr;PFNGLATTACHSHADERPROC AttachShader=nullptr;
    PFNGLLINKPROGRAMPROC LinkProgram=nullptr;PFNGLGETPROGRAMIVPROC GetProgramiv=nullptr;
    PFNGLGETPROGRAMINFOLOGPROC GetProgramInfoLog=nullptr;PFNGLDELETEPROGRAMPROC DeleteProgram=nullptr;
    PFNGLUSEPROGRAMPROC UseProgram=nullptr;PFNGLGETUNIFORMLOCATIONPROC GetUniformLocation=nullptr;
    PFNGLUNIFORM1IPROC Uniform1i=nullptr;PFNGLGENVERTEXARRAYSPROC GenVertexArrays=nullptr;
    PFNGLBINDVERTEXARRAYPROC BindVertexArray=nullptr;PFNGLDELETEVERTEXARRAYSPROC DeleteVertexArrays=nullptr;
    PFNGLACTIVETEXTUREPROC ActiveTexture=nullptr;
    bool load(RetailPlatformFailure& f) {
#define MKA_NATIVE_GL(field) field=reinterpret_cast<decltype(field)>(SDL_GL_GetProcAddress("gl" #field));if(!field)return fail(f,RetailPlatformError::graphics,"required native GL entry","gl" #field)
        MKA_NATIVE_GL(CreateShader);MKA_NATIVE_GL(ShaderSource);MKA_NATIVE_GL(CompileShader);
        MKA_NATIVE_GL(GetShaderiv);MKA_NATIVE_GL(GetShaderInfoLog);MKA_NATIVE_GL(DeleteShader);
        MKA_NATIVE_GL(CreateProgram);MKA_NATIVE_GL(AttachShader);MKA_NATIVE_GL(LinkProgram);
        MKA_NATIVE_GL(GetProgramiv);MKA_NATIVE_GL(GetProgramInfoLog);MKA_NATIVE_GL(DeleteProgram);
        MKA_NATIVE_GL(UseProgram);MKA_NATIVE_GL(GetUniformLocation);MKA_NATIVE_GL(Uniform1i);
        MKA_NATIVE_GL(GenVertexArrays);MKA_NATIVE_GL(BindVertexArray);MKA_NATIVE_GL(DeleteVertexArrays);
        MKA_NATIVE_GL(ActiveTexture);
#undef MKA_NATIVE_GL
        return true;
    }
};
constexpr std::pair<SDL_GameControllerButton,std::uint16_t> pad_buttons[]{
    {SDL_CONTROLLER_BUTTON_BACK,1u<<0},{SDL_CONTROLLER_BUTTON_LEFTSTICK,1u<<1},
    {SDL_CONTROLLER_BUTTON_RIGHTSTICK,1u<<2},{SDL_CONTROLLER_BUTTON_START,1u<<3},
    {SDL_CONTROLLER_BUTTON_DPAD_UP,1u<<4},{SDL_CONTROLLER_BUTTON_DPAD_RIGHT,1u<<5},
    {SDL_CONTROLLER_BUTTON_DPAD_DOWN,1u<<6},{SDL_CONTROLLER_BUTTON_DPAD_LEFT,1u<<7},
    {SDL_CONTROLLER_BUTTON_LEFTSHOULDER,1u<<10},{SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,1u<<11},
    {SDL_CONTROLLER_BUTTON_Y,1u<<12},{SDL_CONTROLLER_BUTTON_B,1u<<13},
    {SDL_CONTROLLER_BUTTON_A,1u<<14},{SDL_CONTROLLER_BUTTON_X,1u<<15}
};
constexpr std::pair<keymap::Action,std::uint16_t> key_buttons[]{
    {keymap::Select,1u<<0},{keymap::L3,1u<<1},{keymap::R3,1u<<2},{keymap::Start,1u<<3},
    {keymap::DpadUp,1u<<4},{keymap::DpadRight,1u<<5},{keymap::DpadDown,1u<<6},{keymap::DpadLeft,1u<<7},
    {keymap::L2,1u<<8},{keymap::R2,1u<<9},{keymap::L1,1u<<10},{keymap::R1,1u<<11},
    {keymap::Triangle,1u<<12},{keymap::Circle,1u<<13},{keymap::Cross,1u<<14},{keymap::Square,1u<<15}
};
std::uint8_t compatibility_axis(std::int16_t native) {
    return static_cast<std::uint8_t>((static_cast<int>(native)+32768)>>8);
}
std::uint8_t compatibility_trigger(std::int16_t native) {
    const int value=native;
    if(value<=2000)return 0;
    return static_cast<std::uint8_t>(std::max(1,value*255/32767));
}
} // namespace

struct NativeRetailPlatform::State {
    const std::thread::id owner=std::this_thread::get_id();
    bool initialized=false,closed=false;std::atomic_bool quit{false};
    std::optional<RetailLogicalViewport> logical_viewport;
    SDL_Window* window=nullptr;SDL_GLContext context=nullptr;
    Gl gl;GLuint texture=0,program=0,vao=0;GLint sampler=-1;
    std::shared_ptr<video::NativeMovieGpuRetirementOwner> image_retirement;
    std::shared_ptr<video::NativeMovieGlDevice> image_device;
    input::ControllerInput samples;
    std::unique_ptr<NativeRetailKeyboard> keyboard_events;
    keymap::Config keys=keymap::defaults();
    struct Pad {
        SDL_GameController* handle=nullptr;
        std::optional<std::uint32_t> player;
    };
    std::map<SDL_JoystickID,Pad> pads;
    std::optional<std::uint32_t> keyboard_player;
    std::set<std::uint32_t> published_players;
    Uint32 wheel_up_until=0,wheel_down_until=0;
    int last_dx=0,last_dy=-1;
    bool access(RetailPlatformFailure& f) const {
        if(std::this_thread::get_id()!=owner)return fail(f,RetailPlatformError::wrong_thread,"native platform owner thread");
        if(closed)return fail(f,RetailPlatformError::closed,"native platform closed");
        return true;
    }
    bool current(RetailPlatformFailure& f) {
        return access(f)&&(SDL_GL_MakeCurrent(window,context)==0 || fail(f,RetailPlatformError::context,"SDL_GL_MakeCurrent",SDL_GetError()));
    }
    bool open_pad(int device,RetailPlatformFailure& f) {
        if(!SDL_IsGameController(device))return true; // Not a mapped controller; no invented mapping.
        const auto id=SDL_JoystickGetDeviceInstanceID(device);
        if(id<0)return fail(f,RetailPlatformError::input,"SDL_JoystickGetDeviceInstanceID",SDL_GetError());
        if(pads.contains(id))return true;
        auto* handle=SDL_GameControllerOpen(device);
        if(!handle)return fail(f,RetailPlatformError::input,"SDL_GameControllerOpen",SDL_GetError());
        try{pads.emplace(id,Pad{handle,{}});}catch(...){SDL_GameControllerClose(handle);throw;}
        return true;
    }
    bool pressed(keymap::Action action,const Uint8* keys_down,int key_count,Uint32 mouse,Uint32 now) const {
        for(const auto code:keys.inputs[action]) {
            if(code>=0&&code<key_count&&keys_down[code])return true;
            if(code>=keymap::kMouseLeft&&code<=keymap::kMouseX2) {
                const int buttons[]{SDL_BUTTON_LEFT,SDL_BUTTON_MIDDLE,SDL_BUTTON_RIGHT,SDL_BUTTON_X1,SDL_BUTTON_X2};
                if(mouse&SDL_BUTTON(buttons[code-keymap::kMouseLeft]))return true;
            }
            if(code==keymap::kWheelUp&&std::bit_cast<Sint32>(wheel_up_until-now)>0)return true;
            if(code==keymap::kWheelDown&&std::bit_cast<Sint32>(wheel_down_until-now)>0)return true;
        }
        return false;
    }
    input::ControllerState keyboard() {
        if(SDL_GetKeyboardFocus()!=window)
            return input::digital_state(0,true,{{128,128,128,128}},255,255,
                input::NativeControllerAxes{});
        int count=0;const auto* down=SDL_GetKeyboardState(&count);
        const auto mouse=SDL_GetMouseState(nullptr,nullptr);
        const auto now=SDL_GetTicks();
        auto held=[&](keymap::Action action){return pressed(action,down,count,mouse,now);};
        std::uint16_t buttons=0;for(const auto [action,mask]:key_buttons)if(held(action))buttons|=mask;
        if(held(keymap::Rage))buttons|=(1u<<1)|(1u<<2);
        auto direction=[&](keymap::Action minus,keymap::Action plus){return int(held(plus))-int(held(minus));};
        auto push=[](int dx,int dy,std::uint8_t& x,std::uint8_t& y,
                     std::int16_t& native_x,std::int16_t& native_y){
            const bool diagonal=dx!=0&&dy!=0;
            if(dx){x=diagonal?std::uint8_t(128+dx*90):std::uint8_t(dx<0?0:255);
                native_x=diagonal?static_cast<std::int16_t>(dx*23040):
                    static_cast<std::int16_t>(dx<0?-32768:32767);}
            if(dy){y=diagonal?std::uint8_t(128+dy*90):std::uint8_t(dy<0?0:255);
                native_y=diagonal?static_cast<std::int16_t>(dy*23040):
                    static_cast<std::int16_t>(dy<0?-32768:32767);}
        };
        const int dx=direction(keymap::LeftLeft,keymap::LeftRight),dy=direction(keymap::LeftUp,keymap::LeftDown);
        std::array<std::uint8_t,4> axes{{128,128,128,128}};
        input::NativeControllerAxes native_axes;
        push(direction(keymap::RightLeft,keymap::RightRight),direction(keymap::RightUp,keymap::RightDown),
             axes[0],axes[1],native_axes.sticks[0],native_axes.sticks[1]);
        push(dx,dy,axes[2],axes[3],native_axes.sticks[2],native_axes.sticks[3]);
        if(dx||dy){last_dx=dx;last_dy=dy;}
        if(held(keymap::Roll))push(dx||dy?dx:last_dx,dx||dy?dy:last_dy,
                                   axes[0],axes[1],native_axes.sticks[0],native_axes.sticks[1]);
        native_axes.triggers[0]=held(keymap::L2)?32767:0;
        native_axes.triggers[1]=held(keymap::R2)?32767:0;
        return input::digital_state(buttons,true,axes,255,255,native_axes);
    }
    static input::ControllerState controller(SDL_GameController* pad) {
        if(!SDL_GameControllerGetAttached(pad))return {};
        std::uint16_t buttons=0;for(const auto [button,mask]:pad_buttons)if(SDL_GameControllerGetButton(pad,button))buttons|=mask;
        input::NativeControllerAxes native_axes;
        native_axes.sticks={{SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_RIGHTX),
            SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_RIGHTY),
            SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_LEFTX),
            SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_LEFTY)}};
        native_axes.triggers={{SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_TRIGGERLEFT),
            SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_TRIGGERRIGHT)}};
        const auto left=compatibility_trigger(native_axes.triggers[0]);
        const auto right=compatibility_trigger(native_axes.triggers[1]);
        if(left)buttons|=1u<<8;if(right)buttons|=1u<<9;
        return input::digital_state(buttons,true,{{compatibility_axis(native_axes.sticks[0]),
            compatibility_axis(native_axes.sticks[1]),compatibility_axis(native_axes.sticks[2]),
            compatibility_axis(native_axes.sticks[3])}},left,right,native_axes);
    }
    static void merge(input::ControllerState& into,const input::ControllerState& from) {
        into.connected=into.connected||from.connected;into.buttons|=from.buttons;
        for(std::size_t i=0;i<into.pressure.size();++i)into.pressure[i]=std::max(into.pressure[i],from.pressure[i]);
        for(std::size_t i=0;i<into.axes.size();++i) {
            if(into.axes[i]==128)into.axes[i]=from.axes[i];
        }
        if(from.native_axes) {
            if(!into.native_axes)into.native_axes=from.native_axes;
            else {
                for(std::size_t i=0;i<into.native_axes->sticks.size();++i)
                    if(into.native_axes->sticks[i]==0)
                        into.native_axes->sticks[i]=from.native_axes->sticks[i];
                for(std::size_t i=0;i<into.native_axes->triggers.size();++i)
                    into.native_axes->triggers[i]=std::max(into.native_axes->triggers[i],
                        from.native_axes->triggers[i]);
            }
        }
    }
    void release() noexcept {
        if(std::this_thread::get_id()!=owner) {
            std::fputs("[native-platform-stop] destruction on foreign thread\n",stderr);std::terminate();
        }
        if(keyboard_events) {
            RetailKeyboardFailure failure;
            const auto observation=keyboard_events->observe(failure);
            if(!observation || (!observation->closed && !keyboard_events->shutdown(failure))) {
                std::fputs("[native-platform-stop] keyboard lifetime retirement failed\n",stderr);std::terminate();
            }
        }
        if(context) {
            if(SDL_GL_MakeCurrent(window,context)==0) {
                glFinish();
                if(image_device) {
                    auto result=image_device->shutdown();
                    if(result==video::MovieGpuError::pending) {
                        glFinish(); result=image_device->shutdown();
                    }
                    if(result!=video::MovieGpuError::none ||
                       image_retirement->poll().state!=video::MovieFenceState::complete) {
                        std::fputs("[native-platform-stop] actual image/context retirement failed\n",stderr);
                        std::terminate();
                    }
                    image_device.reset(); image_retirement.reset();
                }
                if(texture)glDeleteTextures(1,&texture);
                if(program&&gl.DeleteProgram)gl.DeleteProgram(program);
                if(vao&&gl.DeleteVertexArrays)gl.DeleteVertexArrays(1,&vao);
            } else if(image_device) {
                std::fputs("[native-platform-stop] image owner requires its actual current context for retirement\n",stderr);
                std::terminate();
            }
            SDL_GL_DeleteContext(context);context=nullptr;texture=program=vao=0;
        }
        if(window){SDL_DestroyWindow(window);window=nullptr;}
        for(auto& [id,pad]:pads)SDL_GameControllerClose(pad.handle);pads.clear();
        if(initialized){SDL_QuitSubSystem(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS);initialized=false;}
    }
    ~State(){release();}
    bool shader(GLenum kind,const char* source,GLuint& result,RetailPlatformFailure& f) {
        result=gl.CreateShader(kind);
        if(!result)return fail(f,RetailPlatformError::graphics,"native loading shader allocation");
        gl.ShaderSource(result,1,&source,nullptr);gl.CompileShader(result);
        GLint success=0;gl.GetShaderiv(result,GL_COMPILE_STATUS,&success);
        if(!success) {
            GLint length=0;gl.GetShaderiv(result,GL_INFO_LOG_LENGTH,&length);
            std::string message(std::max(1,length),'\0');gl.GetShaderInfoLog(result,static_cast<GLsizei>(message.size()),nullptr,message.data());
            gl.DeleteShader(result);result=0;return fail(f,RetailPlatformError::graphics,"native loading shader compile",message.c_str());
        }
        return true;
    }
    bool upload_loading(const RetailLoadingPicture& picture,RetailPlatformFailure& f) {
        if(!gl.load(f))return false;
        constexpr const char* vertex=R"GLSL(#version 330 core
out vec2 uv;
void main(){
    vec2 corners[6]=vec2[6](vec2(0,0),vec2(1,0),vec2(0,1),vec2(0,1),vec2(1,0),vec2(1,1));
    uv=corners[gl_VertexID];
    vec2 point=vec2(192,48)+uv*vec2(256,128);
    gl_Position=vec4(point.x/320.0-1.0,1.0-point.y/112.0,0,1);
}
)GLSL";
        constexpr const char* fragment=R"GLSL(#version 330 core
in vec2 uv;
uniform sampler2D loading_rgb;
out vec4 color;
void main(){color=vec4(texture(loading_rgb,uv).rgb,1.0);}
)GLSL";
        GLuint vs=0,fs=0;
        if(!shader(GL_VERTEX_SHADER,vertex,vs,f))return false;
        if(!shader(GL_FRAGMENT_SHADER,fragment,fs,f)){gl.DeleteShader(vs);return false;}
        program=gl.CreateProgram();gl.AttachShader(program,vs);gl.AttachShader(program,fs);gl.LinkProgram(program);
        gl.DeleteShader(vs);gl.DeleteShader(fs);
        GLint success=0;gl.GetProgramiv(program,GL_LINK_STATUS,&success);
        if(!success)return fail(f,RetailPlatformError::graphics,"native loading shader link");
        sampler=gl.GetUniformLocation(program,"loading_rgb");
        if(sampler<0)return fail(f,RetailPlatformError::graphics,"native loading sampler");
        gl.GenVertexArrays(1,&vao);glGenTextures(1,&texture);
        gl.ActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,texture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT,1);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGB8,picture.width,picture.height,0,GL_RGB,GL_UNSIGNED_BYTE,picture.rgb.data());
        return glGetError()==GL_NO_ERROR || fail(f,RetailPlatformError::graphics,"native loading RGB upload");
    }
    bool draw_loading(RetailPlatformFailure& f) {
        int width=0,height=0;SDL_GL_GetDrawableSize(window,&width,&height);
        if(width<=0||height<=0)return fail(f,RetailPlatformError::window,"native loading drawable extent");
        glDisable(GL_BLEND);glDisable(GL_DEPTH_TEST);glDisable(GL_SCISSOR_TEST);
        glViewport(0,0,width,height);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT);
        // Exact logical-pixel fit only. Analog/physical pixel aspect is not
        // established here and must not be claimed as retail display parity.
        int w=width,h=static_cast<int>(static_cast<std::int64_t>(width)*224/640);
        if(h>height){h=height;w=static_cast<int>(static_cast<std::int64_t>(height)*640/224);}
        glViewport((width-w)/2,(height-h)/2,w,h);
        gl.UseProgram(program);gl.Uniform1i(sampler,0);gl.ActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D,texture);gl.BindVertexArray(vao);glDrawArrays(GL_TRIANGLES,0,6);
        gl.BindVertexArray(0);gl.UseProgram(0);
        return glGetError()==GL_NO_ERROR || fail(f,RetailPlatformError::graphics,"native loading draw");
    }
};

NativeRetailPlatform::NativeRetailPlatform(std::unique_ptr<State> state):state_(std::move(state)){}
NativeRetailPlatform::~NativeRetailPlatform()=default;
std::unique_ptr<NativeRetailPlatform> NativeRetailPlatform::create(const content::NativeContentVolume& volume,RetailPlatformFailure& f) noexcept {
    f={};
    try {
        auto picture=load_retail_loading_picture(volume,f);if(!picture)return {};
        auto state=std::make_unique<State>();
        state->keyboard_events=NativeRetailKeyboard::create(f.keyboard);
        if(!state->keyboard_events){fail(f,RetailPlatformError::input,"native keyboard owner creation");return {};}
        SDL_SetMainReady();
        if(SDL_InitSubSystem(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER|SDL_INIT_EVENTS)!=0){fail(f,RetailPlatformError::initialization,"SDL native platform initialization",SDL_GetError());return {};}
        state->initialized=true;
        char* base=SDL_GetBasePath();
        if(!base){fail(f,RetailPlatformError::settings,"native executable settings path",SDL_GetError());return {};}
        std::unique_ptr<char,decltype(&SDL_free)> base_owner(base,SDL_free);
        const auto key_path=std::filesystem::u8path(base)/"keys.ini";
        std::error_code ec;const bool has_keys=std::filesystem::exists(key_path,ec);
        if(ec){fail(f,RetailPlatformError::settings,"native keyboard settings query");return {};}
        if(has_keys) {
            std::ifstream file(key_path,std::ios::binary);
            if(!file){fail(f,RetailPlatformError::settings,"native keyboard settings read");return {};}
            const std::string text((std::istreambuf_iterator<char>(file)),{});
            if(file.bad()){fail(f,RetailPlatformError::settings,"native keyboard settings complete read");return {};}
            std::vector<std::string> problems;keymap::parse(text,state->keys,problems);
            if(!problems.empty()){fail(f,RetailPlatformError::settings,"native keyboard settings parse",problems.front().c_str());return {};}
        }
        if(state->keys.mouse_stick!=0){fail(f,RetailPlatformError::settings,"native mouse-stick mapping not implemented");return {};}
        for(const auto [attribute,value]:{std::pair{SDL_GL_CONTEXT_MAJOR_VERSION,3},std::pair{SDL_GL_CONTEXT_MINOR_VERSION,3},
            std::pair{SDL_GL_CONTEXT_PROFILE_MASK,int(SDL_GL_CONTEXT_PROFILE_COMPATIBILITY)},std::pair{SDL_GL_DOUBLEBUFFER,1}}) {
            if(SDL_GL_SetAttribute(attribute,value)!=0){fail(f,RetailPlatformError::context,"SDL_GL_SetAttribute",SDL_GetError());return {};}
        }
        state->window=SDL_CreateWindow("Mortal Kombat Armageddon",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
            640,224,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI|SDL_WINDOW_HIDDEN);
        if(!state->window){fail(f,RetailPlatformError::window,"SDL_CreateWindow",SDL_GetError());return {};}
        state->context=SDL_GL_CreateContext(state->window);
        if(!state->context){fail(f,RetailPlatformError::context,"SDL_GL_CreateContext",SDL_GetError());return {};}
        if(!state->current(f))return {};
        GLint major=0,minor=0;glGetIntegerv(GL_MAJOR_VERSION,&major);glGetIntegerv(GL_MINOR_VERSION,&minor);
        if(major<3||(major==3&&minor<3)){fail(f,RetailPlatformError::graphics,"native renderer requires OpenGL3.3");return {};}
        glFinish();if(glGetError()!=GL_NO_ERROR){fail(f,RetailPlatformError::graphics,"initial native graphics drain");return {};}
        if(!state->upload_loading(*picture,f)||!state->draw_loading(f))return {};
        SDL_ClearError();SDL_GL_SwapWindow(state->window);
        if(*SDL_GetError()){fail(f,RetailPlatformError::graphics,"native loading swap",SDL_GetError());return {};}
        const auto devices=SDL_NumJoysticks();
        if(devices<0){fail(f,RetailPlatformError::input,"SDL_NumJoysticks",SDL_GetError());return {};}
        for(int device=0;device<devices;++device)if(!state->open_pad(device,f))return {};
        return std::unique_ptr<NativeRetailPlatform>(new NativeRetailPlatform(std::move(state)));
    } catch(const std::bad_alloc&) {f.kind=RetailPlatformError::allocation;f.operation="native platform allocation";}
    catch(...) {f.kind=RetailPlatformError::native_exception;f.operation="native platform creation";}
    return {};
}
input::ControllerInput& NativeRetailPlatform::controller_samples() noexcept {return state_->samples;}
NativeRetailKeyboard& NativeRetailPlatform::keyboard_events() noexcept {return *state_->keyboard_events;}
bool NativeRetailPlatform::validate_input_owner(
    const input::ControllerInput& expected,RetailPlatformFailure& f) const noexcept {
    f={};
    if(!state_->access(f)) return false;
    if(!state_->initialized || !state_->window || !state_->keyboard_events)
        return fail(f,RetailPlatformError::input,"actual native input service not initialized");
    if(&expected!=&state_->samples)
        return fail(f,RetailPlatformError::publication,"foreign controller sample owner");
    const auto keyboard=state_->keyboard_events->observe(f.keyboard);
    if(!keyboard || keyboard->closed)
        return fail(f,RetailPlatformError::input,"actual native keyboard queue inactive");
    // Zero connected controllers is valid. No guest port count, module,
    // transport or fabricated participant assignment establishes readiness.
    return true;
}
bool NativeRetailPlatform::poll_events_and_input(RetailPlatformFailure& f) noexcept {
    f={};try {
        if(!state_->access(f))return false;SDL_Event event;
        while(SDL_PollEvent(&event)) {
            if(!publish_retail_keyboard_sdl_event(*state_->keyboard_events,event,SDL_GetWindowID(state_->window),f.keyboard))
                return fail(f,RetailPlatformError::input,"native keyboard event publication");
            if(event.type==SDL_QUIT||(event.type==SDL_WINDOWEVENT&&event.window.windowID==SDL_GetWindowID(state_->window)&&event.window.event==SDL_WINDOWEVENT_CLOSE))state_->quit=true;
            else if(event.type==SDL_CONTROLLERDEVICEADDED) {if(!state_->open_pad(event.cdevice.which,f))return false;}
            else if(event.type==SDL_CONTROLLERDEVICEREMOVED) {
                const auto found=state_->pads.find(event.cdevice.which);
                if(found!=state_->pads.end()){SDL_GameControllerClose(found->second.handle);state_->pads.erase(found);}
            } else if(event.type==SDL_MOUSEWHEEL) {
                if(event.wheel.y>0)state_->wheel_up_until=SDL_GetTicks()+100;
                if(event.wheel.y<0)state_->wheel_down_until=SDL_GetTicks()+100;
            }
        }
        std::map<std::uint32_t,input::ControllerState> samples;
        for(auto player:state_->published_players)samples.emplace(player,input::ControllerState{});
        if(state_->keyboard_player){const auto player=*state_->keyboard_player;State::merge(samples[player],state_->keyboard());}
        for(const auto& [id,pad]:state_->pads)if(pad.player)State::merge(samples[*pad.player],State::controller(pad.handle));
        for(const auto& [player,sample]:samples)if(!state_->samples.publish(player,sample))return fail(f,RetailPlatformError::publication,"native controller sample publication");
        return true;
    } catch(const std::bad_alloc&) {f.kind=RetailPlatformError::allocation;f.operation="native input allocation";}
    catch(...) {f.kind=RetailPlatformError::native_exception;f.operation="native input poll";}
    return false;
}
std::optional<std::vector<RetailHostController>> NativeRetailPlatform::controllers(RetailPlatformFailure& f) const noexcept {
    f={};try {
        if(!state_->access(f))return {};
        std::vector<RetailHostController> result;
        for(const auto& [id,pad]:state_->pads){const auto* name=SDL_GameControllerName(pad.handle);result.push_back({id,name?name:""});}
        return result;
    } catch(const std::bad_alloc&) {f.kind=RetailPlatformError::allocation;f.operation="native controller inventory";}
    catch(...) {f.kind=RetailPlatformError::native_exception;f.operation="native controller inventory";}
    return {};
}
bool NativeRetailPlatform::assign_keyboard(std::uint32_t player,RetailPlatformFailure& f) noexcept {
    f={};try {if(!state_->access(f))return false;state_->published_players.insert(player);state_->keyboard_player=player;return true;}
    catch(...){f.kind=RetailPlatformError::allocation;f.operation="native keyboard assignment";return false;}
}
bool NativeRetailPlatform::assign_controller(std::int32_t instance,std::uint32_t player,RetailPlatformFailure& f) noexcept {
    f={};try {
        if(!state_->access(f))return false;const auto found=state_->pads.find(instance);
        if(found==state_->pads.end())return fail(f,RetailPlatformError::unknown_device,"native controller assignment");
        for(const auto& [id,pad]:state_->pads)if(id!=instance&&pad.player==player)return fail(f,RetailPlatformError::invalid_assignment,"native player already owns a controller");
        state_->published_players.insert(player);found->second.player=player;return true;
    } catch(...){f.kind=RetailPlatformError::allocation;f.operation="native controller assignment";return false;}
}
bool NativeRetailPlatform::remove_keyboard_assignment(RetailPlatformFailure& f) noexcept {
    f={};if(!state_->access(f))return false;state_->keyboard_player.reset();return true;
}
bool NativeRetailPlatform::remove_controller_assignment(std::int32_t instance,RetailPlatformFailure& f) noexcept {
    f={};if(!state_->access(f))return false;const auto found=state_->pads.find(instance);
    if(found==state_->pads.end())return fail(f,RetailPlatformError::unknown_device,"remove native controller assignment");
    found->second.player.reset();return true;
}
bool NativeRetailPlatform::close_requested() const noexcept {return state_->quit.load(std::memory_order_relaxed);}
bool NativeRetailPlatform::configure_logical_viewport(
    const RetailLogicalViewport& viewport,RetailPlatformFailure& f) noexcept {
    f={}; if(!state_->access(f)) return false;
    if(!state_->initialized || !state_->window || !state_->context)
        return fail(f,RetailPlatformError::initialization,"225DB0 actual native presentation owner missing");
    if(!viewport.width || !viewport.height)
        return fail(f,RetailPlatformError::settings,"native logical game extent must be nonzero");
    // Preserve the host window/drawable dimensions and its native user sizing.
    // Later renderer consumers read this distinct game-coordinate contract.
    state_->logical_viewport=viewport;
    return true;
}
std::optional<RetailLogicalViewport> NativeRetailPlatform::logical_viewport(
    RetailPlatformFailure& f) const noexcept {
    f={}; if(!state_->access(f)) return {};
    if(!state_->logical_viewport) {
        fail(f,RetailPlatformError::settings,"225DB0 native logical game viewport unbound"); return {};
    }
    return state_->logical_viewport;
}
bool NativeRetailPlatform::prepare_frame(RetailPlatformFailure& f) noexcept {f={};return state_->current(f);}
std::shared_ptr<graphics::NativeGlTextureUpload> NativeRetailPlatform::texture_upload_device(
    RetailPlatformFailure& f) noexcept {
    f={}; if(!state_->current(f)) return {};
    if(state_->image_device) return state_->image_device;
#ifdef _WIN32
    try {
        SDL_SysWMinfo info{}; SDL_VERSION(&info.version);
        if(!SDL_GetWindowWMInfo(state_->window,&info)||info.subsystem!=SDL_SYSWM_WINDOWS) {
            fail(f,RetailPlatformError::context,"actual platform Win32 GL window",SDL_GetError()); return {};
        }
        video::MovieGpuError error;
        if(!state_->image_retirement) {
            state_->image_retirement=video::NativeMovieGpuRetirementOwner::create(error);
            if(!state_->image_retirement) {
                fail(f,RetailPlatformError::graphics,"native image host retirement owner"); return {};
            }
        }
        auto device=video::NativeMovieGlDevice::attach_current_context(
            info.info.win.window,1,state_->image_retirement,error);
        if(!device) {
            fail(f,RetailPlatformError::graphics,"attach actual platform native image context"); return {};
        }
        state_->image_device=std::move(device); return state_->image_device;
    } catch(const std::bad_alloc&) { fail(f,RetailPlatformError::allocation,"native image context capability"); }
    catch(...) { fail(f,RetailPlatformError::native_exception,"native image context capability"); }
#else
    fail(f,RetailPlatformError::context,"native image context acquisition unsupported on this host");
#endif
    return {};
}
bool NativeRetailPlatform::present(RetailPlatformFailure& f) noexcept {
    f={};if(!state_->current(f))return false;SDL_ClearError();SDL_GL_SwapWindow(state_->window);
    return !*SDL_GetError() || fail(f,RetailPlatformError::graphics,"native frame swap",SDL_GetError());
}
bool NativeRetailPlatform::drain(RetailPlatformFailure& f) noexcept {
    f={};if(!state_->current(f))return false;glFinish();
    return glGetError()==GL_NO_ERROR || fail(f,RetailPlatformError::graphics,"native graphics completion");
}
bool NativeRetailPlatform::shutdown(RetailPlatformFailure& f) noexcept {
    f={};if(!drain(f))return false;state_->release();state_->closed=true;return true;
}
} // namespace mka::retail
