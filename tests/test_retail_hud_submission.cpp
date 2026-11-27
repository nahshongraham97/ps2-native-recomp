// Reuse the actual Session/Core test fixture and its unopened suffix boundaries.
// No GL implementation is replaced; the older component main is not invoked.
#define main retained_camera_component_main
#include "test_retail_renderer_cameras.cpp"
#undef main
#include "mka/movie_gl_device.hpp"

#ifdef _WIN32
namespace {
unsigned hud_checks{};
void hud_check(bool value,const char* reason){
 ++hud_checks;if(!value)std::fprintf(stderr,"HUD_ASSERTION: %s\n",reason);check(value,reason);
}
struct HudHarness {
 CameraGpuFixture native;
 std::shared_ptr<NativeOffscreenDisplayService> display;
 std::shared_ptr<NativeRetailRendererFrames> frames;
 std::shared_ptr<NativeRetailRendererMaterials> materials;
 std::shared_ptr<NativeRetailRendererTextures> textures;
 std::shared_ptr<NativeRetailRendererWorldModule> worlds;
 NativeRetailWorldHandle world_handle;
 std::shared_ptr<NativeRetailWorld> world;
 NativeRetailRendererCameraHandle camera;
 std::shared_ptr<NativeRetailCameraWorldLink> link;
 explicit HudHarness(const std::filesystem::path& root):native(root),display(native_display()){
  auto& core=*native.core;auto& cf=native.native.f;
  hud_check(renderer_required::construct_core_plugin_0403(core,cf)&&
   renderer_required::construct_core_plugin_0408(core,cf)&&renderer_required::construct_core_plugin_040a(core,cf)&&
   renderer_required::construct_core_plugin_0412(core,cf),"HUD actual frame/texture/material/world owners");
  frames=core.frames(cf);materials=core.materials(cf);textures=core.textures(cf);worlds=core.world_module(cf);
  RetailRendererWorldFailure wf;auto handle=worlds->create_world(wf);hud_check(bool(handle),"HUD actual world allocation");
  world_handle=*handle;world=worlds->resolve_world(*handle,wf);
  hud_check(world&&world->bind_gpu_resources(display,native.world_retirement,wf),"HUD explicit display and retirement binding");
  const auto color=native.backed(2,display),depth=native.backed(1,display);
  camera=native.camera(color,depth,{{0,0,64,48},0.25f});RetailRendererFrameFailure ff;
  auto frame=frames->create_base_frame(ff);RetailRendererCameraFailure cf2;
  hud_check(frame&&native.cameras->set_frame(camera,*frame,cf2),"HUD actual camera/frame association");
  RetailRendererMatrixValue transform{{{1,0,0},{0,1,0},{0,0,1},{0,0,-4}},0x20000u};
  hud_check(frames->set_modelling(*frame,transform,ff),"HUD explicit native camera transform");
  RetailCameraWorldFailure lf;link=NativeRetailCameraWorldLink::create_initial(native.native.startup.game_session,lf);
  RetailCameraProducerFailure pf;
  hud_check(link&&attach_camera_to_world_1cf300(*native.cameras,*frames,camera,link,world,pf),"HUD actual camera world link");
 }
 ~HudHarness(){
  DisplayServiceFailure f;
  if(display&&display->acquire_context(f)){
   renderer_required::destroy_core_plugin_0405(*native.core);
   if(worlds)worlds->retire();world.reset();
   auto state=mka::video::MovieFenceState::pending;
   for(unsigned i=0;i<8&&state==mka::video::MovieFenceState::pending;++i){glFinish();state=native.world_retirement->poll().state;}
   if(state!=mka::video::MovieFenceState::complete){std::fputs("HUD cleanup retained native GPU work\n",stderr);std::abort();}
  }
 }
 bool render(RetailCameraProducerFailure& failure){
  return render_world_scene_frame(*native.cameras,*frames,camera,link,world,{0,0,1,1},1,false,
    failure,materials.get(),textures.get(),native.rasters.get()).has_value();
 }
 void add_occluded_textured_triangle(){
  RetailRendererRasterFailure rf;auto raster=native.rasters->create_raster({2,2,32,4,4,0x500},rf);
  auto pixels=raster?native.rasters->lock_pixels(*raster,rf):std::nullopt;
  hud_check(pixels&&pixels->size()==16,"HUD actual root RGBA raster storage");
  std::fill(pixels->begin(),pixels->end(),std::byte{255});hud_check(native.rasters->unlock_pixels(*raster,rf),"HUD unlock root raster");
  RetailRendererTextureFailure tf;auto texture=textures->create_texture({},*raster,tf);
  hud_check(texture&&textures->set_filter_addressing(*texture,RetailRendererTextureFilter::nearest,
    RetailRendererTextureAddress::clamp,RetailRendererTextureAddress::clamp,tf),"HUD real sampled scene texture");
  RetailRendererMaterialFailure mf;auto material=materials->create_material(mf);
  hud_check(material&&materials->set_texture(*material,*texture,mf),"HUD actual textured scene material");
  RetailRendererFrameFailure ff;auto frame=frames->create_base_frame(ff);hud_check(bool(frame),"HUD real scene object frame");
  RetailWorldSceneObject object;object.id=1;object.frame=*frame;object.material=*material;object.bounding_radius=2;
  object.triangles={{{-1,-1,0},0,0},{{1,-1,0},1,0},{{0,1,0},0.5f,1}};
  RetailRendererWorldFailure wf;hud_check(world->add_scene_object(std::move(object),wf),"HUD actual scene triangle before overlay");
 }
};

struct HostProgram {
 using CreateShader=GLuint(APIENTRY*)(GLenum);using Source=void(APIENTRY*)(GLuint,GLsizei,const char*const*,const GLint*);
 using ShaderOp=void(APIENTRY*)(GLuint);using Get=void(APIENTRY*)(GLuint,GLenum,GLint*);
 using CreateProgram=GLuint(APIENTRY*)();using Attach=void(APIENTRY*)(GLuint,GLuint);
 CreateShader create_shader=gpu_proc<CreateShader>("glCreateShader");Source source=gpu_proc<Source>("glShaderSource");
 ShaderOp compile=gpu_proc<ShaderOp>("glCompileShader"),erase_shader=gpu_proc<ShaderOp>("glDeleteShader");
 Get shader_status=gpu_proc<Get>("glGetShaderiv"),program_status=gpu_proc<Get>("glGetProgramiv");
 CreateProgram create_program=gpu_proc<CreateProgram>("glCreateProgram");Attach attach=gpu_proc<Attach>("glAttachShader");
 ShaderOp link=gpu_proc<ShaderOp>("glLinkProgram"),use=gpu_proc<ShaderOp>("glUseProgram"),erase_program=gpu_proc<ShaderOp>("glDeleteProgram");
 GLuint program{};
 std::shared_ptr<NativeOffscreenDisplayService> display;
 explicit HostProgram(std::shared_ptr<NativeOffscreenDisplayService> owner):display(std::move(owner)){
  const char* vertex="#version 330 compatibility\nvoid main(){gl_Position=gl_Vertex;}";
  const char* fragment="#version 330 compatibility\nout vec4 color;void main(){color=vec4(1,1,0,1);}";
  const auto vs=create_shader(0x8b31),fs=create_shader(0x8b30);source(vs,1,&vertex,nullptr);source(fs,1,&fragment,nullptr);
  compile(vs);compile(fs);GLint good{};shader_status(vs,0x8b81,&good);hud_check(good!=0,"actual host vertex shader compiled");
  shader_status(fs,0x8b81,&good);hud_check(good!=0,"actual host fragment shader compiled");
  program=create_program();attach(program,vs);attach(program,fs);link(program);program_status(program,0x8b82,&good);
  erase_shader(vs);erase_shader(fs);hud_check(good!=0,"actual host shader program linked");use(program);
 }
 ~HostProgram(){DisplayServiceFailure f;if(display->acquire_context(f)){use(0);erase_program(program);}}
};

void hud_gpu_cases(const std::filesystem::path& root){
 HudHarness h(root);h.add_occluded_textured_triangle();
 mka::video::MovieGpuError ge;auto device=mka::video::NativeMovieGlDevice::attach_current_context(
  h.display->native_display_handle(),h.display->destination_identity(),h.native.world_retirement,ge);
 hud_check(device&&ge==mka::video::MovieGpuError::none,"actual typed native texture uploader");
 const std::array<std::uint8_t,4> green{0,255,0,128};
 auto image=device->upload_rgba_image({1,1,green},{mka::graphics::NativeTextureFilter::nearest,
   mka::graphics::NativeTextureAddress::clamp_to_edge,mka::graphics::NativeTextureAddress::clamp_to_edge});
 hud_check(image.texture&&image.failure.kind==mka::graphics::NativeTextureError::none,"actual native HUD texture capability");
 RetailHudSubmission batch{64,48,{}};
 batch.quads.push_back({{8,8,16,16},{0,0,1,1},{1,0,0,0.5f},{}});
 batch.quads.push_back({{32,8,16,16},{0,0,1,1},{1,1,1,0.5f},image.texture});
 RetailRendererWorldFailure wf;hud_check(h.world->set_hud_submission(batch,wf),"explicit typed native solid/textured HUD batch");
 fail_after=0;const bool allocated=h.world->set_hud_submission(batch,wf);fail_after=-1;
 hud_check(!allocated&&wf.kind==RetailRendererWorldError::allocation&&h.world->hud_submission()->quads.size()==2,
   "HUD allocation refusal preserves the previously owned complete batch");
 auto invalid=batch;invalid.logical_width=0;
 hud_check(!h.world->set_hud_submission(std::move(invalid),wf)&&wf.kind==RetailRendererWorldError::invalid_hud&&
   h.world->hud_submission()->quads.size()==2,"invalid HUD batch refuses before replacing valid input");
 bool wrong_thread=false;std::thread thread([&]{RetailRendererWorldFailure f;
  wrong_thread=!h.world->set_hud_submission(batch,f)&&f.kind==RetailRendererWorldError::wrong_thread;});thread.join();
 hud_check(wrong_thread,"HUD actual world owner thread enforced");

 // Prefilled zero depth makes a depth-test-on HUD defect visibly occlude both quads.
 glDrawBuffer(GL_BACK);glDisable(GL_SCISSOR_TEST);glDepthMask(GL_TRUE);glClearDepth(0);glClear(GL_DEPTH_BUFFER_BIT);
 GLint depth_bits{};glGetIntegerv(GL_DEPTH_BITS,&depth_bits);hud_check(depth_bits>0,"HUD actual depth storage queried");
 const auto vendor=glGetString(GL_VENDOR),renderer=glGetString(GL_RENDERER),version=glGetString(GL_VERSION);
 PIXELFORMATDESCRIPTOR format{};const auto dc=wglGetCurrentDC();const auto chosen=GetPixelFormat(dc);
 hud_check(vendor&&renderer&&version&&chosen&&DescribePixelFormat(dc,chosen,sizeof(format),&format),"HUD actual driver and chosen pixel format queried");
 std::printf("NATIVE_HUD_DRIVER vendor=%s renderer=%s version=%s pixel_format=%d generic_format=%u generic_accelerated=%u hardware_acceleration_not_inferred=1\n",
  reinterpret_cast<const char*>(vendor),reinterpret_cast<const char*>(renderer),reinterpret_cast<const char*>(version),chosen,
  unsigned(bool(format.dwFlags&PFD_GENERIC_FORMAT)),unsigned(bool(format.dwFlags&PFD_GENERIC_ACCELERATED)));
 std::printf("HUD_NATIVE_READY depth_bits=%d physical=64x48 logical=64x48\n",depth_bits);std::fflush(stdout);
 HostProgram host(h.display);
 using Gen=void(APIENTRY*)(GLsizei,GLuint*);using Erase=void(APIENTRY*)(GLsizei,const GLuint*);
 using Bind=void(APIENTRY*)(GLenum,GLuint);using BindOne=void(APIENTRY*)(GLuint);
 using Mask=void(APIENTRY*)(GLuint,GLboolean,GLboolean,GLboolean,GLboolean);
 using Enable=void(APIENTRY*)(GLenum,GLuint);using GetI=void(APIENTRY*)(GLenum,GLuint,GLint*);
 const auto gen_vao=gpu_proc<Gen>("glGenVertexArrays"),gen_sampler=gpu_proc<Gen>("glGenSamplers"),gen_buffer=gpu_proc<Gen>("glGenBuffers");
 const auto erase_vao=gpu_proc<Erase>("glDeleteVertexArrays"),erase_sampler=gpu_proc<Erase>("glDeleteSamplers"),erase_buffer=gpu_proc<Erase>("glDeleteBuffers");
 const auto bind_vao=gpu_proc<BindOne>("glBindVertexArray");const auto bind_buffer=gpu_proc<Bind>("glBindBuffer");
 const auto bind_sampler=gpu_proc<void(APIENTRY*)(GLuint,GLuint)>("glBindSampler");
 const auto active=gpu_proc<void(APIENTRY*)(GLenum)>("glActiveTexture");
 const auto mask=gpu_proc<Mask>("glColorMaski");const auto disable=gpu_proc<Enable>("glDisablei");
 const auto enable=gpu_proc<Enable>("glEnablei");const auto get_i=gpu_proc<GetI>("glGetIntegeri_v");
 const auto enabled_i=gpu_proc<GLboolean(APIENTRY*)(GLenum,GLuint)>("glIsEnabledi");
 const auto boolean_i=gpu_proc<void(APIENTRY*)(GLenum,GLuint,GLboolean*)>("glGetBooleani_v");
 const auto blend_func=gpu_proc<void(APIENTRY*)(GLuint,GLenum,GLenum,GLenum,GLenum)>("glBlendFuncSeparatei");
 const auto blend_equation=gpu_proc<void(APIENTRY*)(GLuint,GLenum,GLenum)>("glBlendEquationSeparatei");
 const auto gen_pipeline=gpu_proc<Gen>("glGenProgramPipelines");const auto erase_pipeline=gpu_proc<Erase>("glDeleteProgramPipelines");
 const auto bind_pipeline=gpu_proc<BindOne>("glBindProgramPipeline");
 GLuint vao{},sampler{},pbo{},pipeline{};gen_vao(1,&vao);gen_sampler(1,&sampler);gen_buffer(1,&pbo);gen_pipeline(1,&pipeline);
 bind_vao(vao);bind_sampler(0,sampler);bind_pipeline(pipeline);bind_buffer(0x88ec,pbo);
 glPixelStorei(GL_UNPACK_ALIGNMENT,8);glPixelStorei(GL_UNPACK_ROW_LENGTH,9);glPixelStorei(GL_UNPACK_SKIP_ROWS,2);glPixelStorei(GL_UNPACK_SKIP_PIXELS,3);
 glMatrixMode(GL_PROJECTION);glLoadIdentity();glScalef(2,3,4);
 glMatrixMode(GL_MODELVIEW);glLoadIdentity();glTranslatef(5,6,7);
 active(0x84c0);glMatrixMode(GL_TEXTURE);glLoadIdentity();glTranslatef(0.25f,0.5f,0);
 glTexCoord4f(0.25f,0.5f,0.75f,1);glColor4f(0.125f,0.25f,0.5f,0.75f);
 active(0x84c1);glEnable(GL_TEXTURE_2D);glMatrixMode(GL_PROJECTION);
 enable(GL_BLEND,0);disable(GL_BLEND,1);mask(0,GL_FALSE,GL_TRUE,GL_FALSE,GL_TRUE);mask(1,GL_TRUE,GL_FALSE,GL_TRUE,GL_FALSE);
 blend_func(0,GL_ONE,GL_ZERO,GL_ZERO,GL_ONE);blend_equation(0,0x800b /*reverse subtract*/,0x800a /*subtract*/);
 glEnable(0x8c89);glEnable(0x8db9);glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);
 glEnable(GL_CULL_FACE);glCullFace(GL_FRONT_AND_BACK);glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_NEVER,0.5f);
 glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
 std::array<float,16> projection{},modelview{},texmatrix{};glGetFloatv(GL_PROJECTION_MATRIX,projection.data());glGetFloatv(GL_MODELVIEW_MATRIX,modelview.data());
 active(0x84c0);glGetFloatv(GL_TEXTURE_MATRIX,texmatrix.data());active(0x84c1);
 RetailCameraProducerFailure failure;hud_check(h.render(failure),failure.operation&&*failure.operation?failure.operation:"native HUD render failed");
 GLint actual{};glGetIntegerv(0x8b8d,&actual);hud_check(actual==static_cast<GLint>(host.program),"HUD restores host program");
 glGetIntegerv(0x825a,&actual);hud_check(actual==static_cast<GLint>(pipeline),"HUD restores host program pipeline");
 glGetIntegerv(0x85b5,&actual);hud_check(actual==static_cast<GLint>(vao),"HUD restores host VAO");
 glGetIntegerv(0x84e0,&actual);hud_check(actual==0x84c1&&glIsEnabled(GL_TEXTURE_2D),"HUD restores active texture and independent unit1 enable");
 get_i(0x8919,0,&actual);hud_check(actual==static_cast<GLint>(sampler),"HUD restores sampler0");
 GLboolean mask0[4]{},mask1[4]{};boolean_i(GL_COLOR_WRITEMASK,0,mask0);boolean_i(GL_COLOR_WRITEMASK,1,mask1);
 hud_check(mask0[0]==GL_FALSE&&mask0[1]==GL_TRUE&&mask0[2]==GL_FALSE&&mask0[3]==GL_TRUE&&
   mask1[0]==GL_TRUE&&mask1[1]==GL_FALSE&&mask1[2]==GL_TRUE&&mask1[3]==GL_FALSE,
   "HUD preserves distinct indexed color masks");
 hud_check(enabled_i(GL_BLEND,0)&&!enabled_i(GL_BLEND,1),"HUD restores indexed blend0 without changing blend1");
 get_i(0x80c9,0,&actual);hud_check(actual==GL_ONE,"HUD restores RGB source blend factor");
 get_i(0x80ca,0,&actual);hud_check(actual==GL_ONE,"HUD restores independent alpha destination blend factor");
 get_i(0x8009,0,&actual);hud_check(actual==0x800b,"HUD restores RGB blend equation");
 get_i(0x883d,0,&actual);hud_check(actual==0x800a,"HUD restores independent alpha blend equation");
 glGetIntegerv(0x88ef,&actual);hud_check(actual==static_cast<GLint>(pbo),"HUD restores actual unpack PBO");
 glGetIntegerv(GL_UNPACK_ROW_LENGTH,&actual);hud_check(actual==9,"HUD restores noncanonical unpack row length");
 glGetIntegerv(GL_UNPACK_SKIP_ROWS,&actual);hud_check(actual==2,"HUD restores unpack skipped rows");
 glGetIntegerv(GL_UNPACK_SKIP_PIXELS,&actual);hud_check(actual==3,"HUD restores unpack skipped pixels");
 std::array<float,16> current{};glGetFloatv(GL_PROJECTION_MATRIX,current.data());hud_check(current==projection,"HUD restores projection matrix without stack effects");
 glGetFloatv(GL_MODELVIEW_MATRIX,current.data());hud_check(current==modelview,"HUD restores modelview matrix");
 active(0x84c0);glGetFloatv(GL_TEXTURE_MATRIX,current.data());hud_check(current==texmatrix,"HUD restores texture matrix");
 std::array<float,4> color{},uv{};glGetFloatv(GL_CURRENT_COLOR,color.data());glGetFloatv(GL_CURRENT_TEXTURE_COORDS,uv.data());
 hud_check(color==std::array<float,4>{0.125f,0.25f,0.5f,0.75f}&&uv==std::array<float,4>{0.25f,0.5f,0.75f,1},"HUD restores current color and texture coordinates");
 active(0x84c1);hud_check(glIsEnabled(0x8c89)&&glIsEnabled(0x8db9)&&glIsEnabled(GL_CULL_FACE)&&glIsEnabled(GL_ALPHA_TEST),"HUD restores hostile fixed-function enables");
 hud_check(glGetError()==GL_NO_ERROR,"HUD submission and host state restoration have no GL errors");
 // Readback uses independent default read framebuffer; rendering did not perform a readback.
 auto bind_fbo=gpu_proc<RealFramebuffers::Bind>("glBindFramebuffer");bind_fbo(read_framebuffer,0);glReadBuffer(GL_BACK);
 std::vector<unsigned char> rgba(64*48*4);std::vector<float> depths(64*48);
 glReadPixels(0,0,64,48,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());glReadPixels(0,0,64,48,GL_DEPTH_COMPONENT,GL_FLOAT,depths.data());
 auto pixel=[&](unsigned x,unsigned y){const auto i=(y*64+x)*4;return std::array<int,3>{rgba[i],rgba[i+1],rgba[i+2]};};
 auto close=[](std::array<int,3> a,std::array<int,3> b){for(unsigned i=0;i<3;++i)if(std::abs(a[i]-b[i])>2)return false;return true;};
 hud_check(close(pixel(16,32),{128,0,127}),"HUD_DEPTH_AND_ALPHA_PIXELS: red half-alpha overlay survives prefilled zero depth");
 hud_check(close(pixel(40,32),{0,64,191}),"HUD_TEXTURE_ALPHA_PIXELS: native texture alpha multiplied by explicit quad alpha");
 hud_check(close(pixel(60,44),{0,0,255})&&depths[32*64+16]==0&&depths[32*64+40]==0,"HUD preserves outside color and zero depth storage");
 hud_check(!h.world->gpu_texture_cache().empty(),"world resource upload occurred under poisoned PBO/unpack state");
 const auto cached=h.world->gpu_texture_cache()[0].gl_texture_id;
 auto other=native_display();DisplayServiceFailure df;
 hud_check(!h.world->release_gpu_resources(wf)&&wf.kind==RetailRendererWorldError::gpu_context&&!h.world->gpu_texture_cache().empty(),"wrong-context release retains actual GPU names");
 hud_check(!h.render(failure),"HUD draw refuses unrelated current context");
 hud_check(h.display->acquire_context(df)&&glIsTexture(cached),"original GPU object remains live after wrong-context refusal");
 glPixelTransferf(GL_RED_SCALE,2);
 hud_check(!h.render(failure)&&!h.native.cameras->is_updating(h.camera,failure.camera),
   "unsupported pixel transfer refuses before camera target publication");glPixelTransferf(GL_RED_SCALE,1);
 RetailWorldBspSector cycle;cycle.id=77;cycle.is_leaf=false;cycle.left_child=0;cycle.right_child=0;
 auto index=h.world->add_bsp_sector(std::move(cycle),wf);hud_check(index&&*index==0,"native malformed BSP fixture constructed");
 h.world->set_bsp_root(0);hud_check(!h.render(failure),"cyclic BSP is refused before GPU work instead of overflowing host stack");h.world->clear_bsp_sectors();
 // Return host-owned test objects to neutral state before deleting those objects.
 host.use(0);bind_pipeline(0);bind_vao(0);bind_sampler(0,0);bind_buffer(0x88ec,0);active(0x84c0);
 erase_pipeline(1,&pipeline);erase_vao(1,&vao);erase_sampler(1,&sampler);erase_buffer(1,&pbo);
 h.world->clear_hud_submission();batch.quads.clear();image.texture.reset();
 glFinish();auto shutdown=device->shutdown();
 for(unsigned i=0;i<8&&shutdown==mka::video::MovieGpuError::pending;++i){glFinish();shutdown=device->shutdown();}
 hud_check(shutdown==mka::video::MovieGpuError::none,"actual native HUD texture device shutdown completes");device.reset();
 hud_check(h.world->release_gpu_resources(wf)&&!glIsTexture(cached),"original-context close deletes actual world GPU storage");
 hud_check(h.native.world_retirement->poll().state==mka::video::MovieFenceState::complete&&h.native.world_retirement->empty(),"actual registered GPU retirement drains before display teardown");
 hud_check(!h.render(failure),"closed GPU world requires explicit new binding");
 hud_check(h.worlds->destroy_world(h.world_handle,wf)&&!h.world->set_hud_submission(batch,wf)&&wf.kind==RetailRendererWorldError::retired_world,"retained retired world cannot submit HUD");
 hud_check(glGetError()==GL_NO_ERROR,"HUD test ends without GPU errors");
 std::printf("PASS_NATIVE_HUD_SUBMISSION checks=%u; native solid/textured quads,orthographic alpha/depth pixels,host GL restoration,actual context-owned resources and retirement; no retail HUD producer/dispatch claim\n",hud_checks);
}
}
#endif
int main(int argc,char** argv)try{
 check(argc==2,"content root required");
#ifdef _WIN32
 hud_gpu_cases(argv[1]);return 0;
#else
 std::fprintf(stderr,"HUD native GPU boundary requires Windows display service\n");return 2;
#endif
}catch(const std::exception& error){std::fprintf(stderr,"FAIL native HUD submission: %s\n",error.what());return 1;}
