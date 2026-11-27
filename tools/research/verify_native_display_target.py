"""Real native display source plants and constructed partial-failure ownership.

No oracle, gameplay/native-credit claim, fake GL, or production mutation.
Compilers run sequentially with inherited Windows affinity CPUs4..7.
"""
from pathlib import Path
import argparse
import ctypes
import hashlib
import json
import os
import re
import shlex
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
SCHEMA = "mkar.native-display-target.verification.v1"
PRODUCTION = [ROOT / "runtime/src/native" / name for name in (
    "retail_renderer_cameras.cpp", "retail_display_service.cpp", "retail_renderer_rasters.cpp")]


def need(value, message):
    if not value:
        raise ValueError(message)


def sha(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(4 << 20), b""):
            h.update(block)
    return h.hexdigest()


def pin(path):
    path = Path(path).resolve()
    return dict(path=str(path), bytes=path.stat().st_size, sha256=sha(path))


def save(path, value):
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf8", newline="\n")


def replace_once(text, before, after):
    need(text.count(before) == 1, "source mutation requires exactly one real site: " + before)
    return text.replace(before, after)


FAULT_HARNESS = r'''
#include "mka/retail_display_service.hpp"
#include <array>
#include <cstdio>
#include <memory>
#include <stdexcept>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <GL/gl.h>
using namespace mka::retail;
namespace {
struct Owner final : NativeDisplayDrawOwner {};
unsigned checks{};
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
template<class T>T proc(const char* name){auto p=wglGetProcAddress(name);
 check(p&&p!=reinterpret_cast<PROC>(1)&&p!=reinterpret_cast<PROC>(2)&&p!=reinterpret_cast<PROC>(3)&&p!=reinterpret_cast<PROC>(-1),name);return reinterpret_cast<T>(p);}
}
int main(int argc,char**argv){try{
 check(argc==2,"constructed fault mode required");const bool pre=std::string_view(argv[1])=="pre_bind";
 check(pre||std::string_view(argv[1])=="partial","known constructed fault mode");
 DisplayServiceFailure f;auto display=NativeOffscreenDisplayService::create(64,48,f);
 check(display&&display->acquire_context(f),"real hidden native WGL context");
 PIXELFORMATDESCRIPTOR chosen{};const HDC dc=wglGetCurrentDC();const int format=GetPixelFormat(dc);
 check(format>0&&DescribePixelFormat(dc,format,sizeof(chosen),&chosen)>0,"actual chosen native pixel format");
 const auto vendor=glGetString(GL_VENDOR),renderer=glGetString(GL_RENDERER),version=glGetString(GL_VERSION);
 check(vendor&&renderer&&version&&glGetError()==GL_NO_ERROR,"actual native GL driver strings");
 std::printf("NATIVE_DISPLAY_FAULT_DRIVER vendor=%s renderer=%s version=%s pixel_format=%d flags=0x%08lx generic_format=%u generic_accelerated=%u depth_bits=%u alpha_bits=%u hardware_acceleration_not_inferred=1\n",
  reinterpret_cast<const char*>(vendor),reinterpret_cast<const char*>(renderer),reinterpret_cast<const char*>(version),format,
  static_cast<unsigned long>(chosen.dwFlags),unsigned(bool(chosen.dwFlags&PFD_GENERIC_FORMAT)),
  unsigned(bool(chosen.dwFlags&PFD_GENERIC_ACCELERATED)),unsigned(chosen.cDepthBits),unsigned(chosen.cAlphaBits));
 auto gen=proc<void(APIENTRY*)(GLsizei,GLuint*)>("glGenFramebuffers");
 auto bind=proc<void(APIENTRY*)(GLenum,GLuint)>("glBindFramebuffer");
 auto remove=proc<void(APIENTRY*)(GLsizei,const GLuint*)>("glDeleteFramebuffers");
 auto viewport=proc<void(APIENTRY*)(GLuint,GLfloat,GLfloat,GLfloat,GLfloat)>("glViewportIndexedf");
 auto get=proc<void(APIENTRY*)(GLenum,GLuint,GLfloat*)>("glGetFloati_v");
 GLuint fbos[2]{};gen(2,fbos);check(fbos[0]&&fbos[1],"real prior draw/read framebuffer identities");
 bind(0x8ca9,fbos[0]);bind(0x8ca8,fbos[1]);viewport(0,2.25f,3.5f,19.75f,17.25f);
 auto unchanged=[&]{GLint draw{},read{};std::array<GLfloat,4> v{};
  glGetIntegerv(0x8ca6,&draw);glGetIntegerv(0x8caa,&read);get(GL_VIEWPORT,0,v.data());
  check(draw==GLint(fbos[0])&&read==GLint(fbos[1]),"prior real draw/read framebuffer restored");
  check(v==std::array<GLfloat,4>{2.25f,3.5f,19.75f,17.25f},"prior fractional viewport restored");
  check(glGetError()==GL_NO_ERROR,"constructed branch leaves real GL error-free");};
 unchanged();auto owner=std::make_shared<Owner>(),other=std::make_shared<Owner>();
 if(pre){
  check(!display->begin_render_target(owner,{5,7,13,11},true,f),"constructed pre-bind refusal reached");
  check(!display->owns_render_target(*owner),"pre-bind refusal has no owner");unchanged();
  check(display->restore_abandoned_render_target(f),"empty pass recovery is explicit no-op");
 }else{
  check(!display->begin_render_target(owner,{5,7,13,11},true,f),"first constructed snapshot refusal");
  check(display->owns_render_target(*owner),"failed snapshot retains real owner");
  check(!display->clear_render_target(*owner,std::array<float,4>{1,0,0,1},0.375f,f)&&f.error==DisplayServiceError::target_owner,"partial pass cannot clear before ready");
  check(!display->begin_render_target(other,{1,2,3,4},false,f)&&f.error==DisplayServiceError::resource_busy,"another owner cannot begin/steal partial pass");
  check(!display->end_render_target(*other,f)&&f.error==DisplayServiceError::target_owner,"another owner cannot end partial pass");
  check(!display->restore_abandoned_render_target(f)&&f.error==DisplayServiceError::resource_busy,"live partial owner cannot be abandoned");
  check(display->end_render_target(*owner,f),"actual owner end restores after injected rollback failure");
  check(!display->owns_render_target(*owner),"successful actual end releases owner");unchanged();
  check(!display->begin_render_target(owner,{5,7,13,11},true,f),"second snapshot refusal rolls back");
  check(!display->owns_render_target(*owner),"successful automatic rollback releases owner");unchanged();
  check(!display->begin_render_target(owner,{5,7,13,11},true,f),"third snapshot refusal retains recovery pass");
  check(display->owns_render_target(*owner),"third snapshot failure retains actual owner");owner.reset();
  check(!display->begin_render_target(other,{1,2,3,4},false,f)&&f.error==DisplayServiceError::resource_busy,"abandoned pass cannot be stolen");
  check(display->restore_abandoned_render_target(f),"expired owner explicit recovery succeeds");unchanged();
 }
 bind(0x8ca9,0);bind(0x8ca8,0);remove(2,fbos);check(display->release_context(f),"explicit native context release");
 std::printf("PASS_NATIVE_DISPLAY_CONSTRUCTED_FAULT mode=%s checks=%u real_GL_WGL=1 gameplay_equivalence=0\n",argv[1],checks);return 0;
}catch(const std::exception&e){std::fprintf(stderr,"FAIL_NATIVE_DISPLAY_CONSTRUCTED_FAULT %s\n",e.what());return 1;}}
'''


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--compiler", type=Path, required=True)
    ap.add_argument("--output", type=Path, required=True)
    ap.add_argument("--parent-root", type=Path)
    ap.add_argument("--build-receipt", type=Path,
                    help="Passing final archive build receipt; defaults to preserved baseline retry-02")
    args = ap.parse_args()
    output = args.output.resolve()
    need(output.is_relative_to(ROOT / "b/phase7-capture-run/native-camera-frame-01") and not output.exists(), "fresh private output child")
    need(os.name == "nt" and os.cpu_count() >= 8, "Windows host with CPUs4..7 required")
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.GetCurrentProcess.restype = ctypes.c_void_p
    kernel.SetProcessAffinityMask.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    need(kernel.SetProcessAffinityMask(kernel.GetCurrentProcess(), 0xf0), "set inherited CPUs4..7 affinity")
    need(shutil.disk_usage(ROOT).free >= 12 * (1 << 30), "12GiB reserve")
    ninja = ROOT / "b/build.ninja"
    text = ninja.read_text()
    start = text.index("build test_native_retail_renderer_cameras.exe:")
    line = next(x.strip().split(" = ", 1)[1] for x in text[start:].splitlines() if x.strip().startswith("LINK_LIBRARIES ="))
    libraries = [x if x.startswith("-") else str((ROOT / "b" / x).resolve()) for x in shlex.split(line)]
    includes = [ROOT / "runtime/include", ROOT / "runtime/src", ROOT / "b/native-retail-startup", ROOT / "tests"]
    line = next(x for x in text.splitlines() if x.strip().startswith("INCLUDES =") and "native-retail-startup" in x)
    includes += [Path(x[2:]) for x in shlex.split(line.split(" = ", 1)[1]) if x.startswith("-I") and Path(x[2:]).is_dir()]
    includes = list(dict.fromkeys(p.resolve() for p in includes))
    fixture = ROOT / "tests/test_retail_renderer_cameras.cpp"
    depth = ROOT / "research/runs/native-xbox-renderer-0405-20261122/depth-expected.tsv"
    baseline = ROOT / "b/phase7-capture-run/native-camera-frame-01/display-baseline-01"
    plan = json.loads((baseline / "PLAN.json").read_bytes())
    camera_plan = next(x for x in plan["invocations"] if x["name"] == "test_native_retail_renderer_cameras")
    need(camera_plan["argv"][1:] == [".", "research/runs/native-xbox-renderer-0405-20261122/depth-expected.tsv"], "frozen camera two-argument roster")
    build_receipt = args.build_receipt or baseline / "retry-02/BUILD.json"
    need(build_receipt.is_file(), "final archive build receipt required before verifier")
    need(json.loads(build_receipt.read_bytes())["exit_code"] == 0,
         "final archive build must pass before verifier; preserve failed build receipts")
    # All included project headers and generated startup headers are pinned;
    # compiler dependency files are retained per translation unit as well.
    sources = [Path(__file__), fixture, ninja, depth, args.compiler, baseline / "PLAN.json", build_receipt, *PRODUCTION,
               *[Path(x) for x in libraries if not x.startswith("-")], ROOT / "b/SDL2.dll"]
    for folder in includes:
        sources += list(folder.rglob("*.hpp")) + list(folder.rglob("*.h"))
    sources += list(args.compiler.resolve().parent.glob("clang*.exe"))
    sources += list(args.compiler.resolve().parent.glob("*lld*.exe"))
    before = [pin(p) for p in dict.fromkeys(sources)]
    output.mkdir(parents=True);shutil.copyfile(ROOT / "b/SDL2.dll", output / "SDL2.dll")
    shutil.copyfile(Path(__file__), output / "DRIVER.py")
    need(sha(output / "DRIVER.py") == sha(Path(__file__)), "archived exact verifier source")
    env = dict(os.environ, PATH=str(output) + os.pathsep + str(args.compiler.resolve().parent) + os.pathsep + os.environ["PATH"])
    base = [str(args.compiler), "-std=c++20", "-O1", "-mavx2", "-ffp-contract=off", "-static-libgcc", "-static-libstdc++", *["-I" + str(x) for x in includes]]
    commands = []
    save(output / "PLAN.json", dict(schema=SCHEMA, affinity_mask="0xf0", sequential_compilers=True,
        source_pins=before, baseline_camera_argv=camera_plan, parent_root=str(args.parent_root) if args.parent_root else None,
        native_credit_added=0, oracle_runs=0, historical_gameplay_equivalence=False))

    def command(argv, log, timeout=240):
        began = time.monotonic()
        try:
            completed = subprocess.run(argv, capture_output=True, cwd=ROOT, env=env, timeout=timeout)
            code, stdout, stderr, timed_out = completed.returncode, completed.stdout, completed.stderr, False
        except subprocess.TimeoutExpired as error:
            code, stdout, stderr, timed_out = None, error.stdout or b"", error.stderr or b"", True
        (output / log).write_bytes(stdout + stderr)
        return dict(argv=argv, cwd=str(ROOT), child_PATH_prefix=[str(output), str(args.compiler.resolve().parent)],
                    exit_code=code, seconds=time.monotonic()-began, timed_out=timed_out,
                    execution_scope="LOADER_FAILURE_NATIVE_MAIN_NOT_REACHED" if code == 0xc0000135 else "process_return_observed",
                    log=pin(output / log), stdout=stdout.decode(errors="replace"), stderr=stderr.decode(errors="replace"))

    def case(name, kind, inputs, link, run_args, expected, needle):
        need(shutil.disk_usage(ROOT).free >= 12 * (1 << 30), "12GiB reserve during verification")
        folder = output / name;folder.mkdir()
        objects = [];builds = []
        for i, source in enumerate(inputs):
            obj = folder / f"source_{i}.o";dep = folder / f"source_{i}.d"
            build = command([*base, "-MMD", "-MF", str(dep), "-c", str(source), "-o", str(obj)], name + f"/compile_{i}.log")
            builds.append(build)
            if build["exit_code"]:break
            objects.append(obj)
        binary = folder / (name + ".exe")
        if len(objects) == len(inputs):
            builds.append(command([*base, *map(str, objects), *link, "-o", str(binary)], name + "/link.log"))
        build_ok = len(objects) == len(inputs) and all(x["exit_code"] == 0 for x in builds)
        run = command([str(binary), *run_args], name + "/run.log") if build_ok else None
        ok = bool(build_ok and run["exit_code"] == expected and
                  needle in (run["stderr"] if expected else run["stdout"]))
        if name == "positive":
            ok = bool(ok and "PASS_NATIVE_CAMERA_FRAME_ASSOCIATION scenarios=5 checks=144" in run["stdout"])
        result = dict(schema=SCHEMA, id=name, kind=kind,
            status="PASS_NATIVE_DISPLAY_CASE_EXPECTED_BEHAVIOR" if ok else "FAIL_NATIVE_DISPLAY_CASE_EXPECTED_BEHAVIOR", inputs=[pin(p) for p in inputs],
            build=builds, run=run, expected_run_exit=expected, required_output=needle,
            binary=pin(binary) if binary.exists() else None,
            dependency_files=[pin(p) for p in folder.glob("*.d")])
        save(folder / "RESULT.json", result);commands.append(result)
        print(json.dumps(dict(schema=SCHEMA, id=name, status=result["status"], build_ok=build_ok, exit=run["exit_code"] if run else None)), flush=True)

    originals = [p.read_text() for p in PRODUCTION]
    plants = [
        ("positive", None, None, None, 0, "PASS_NATIVE_CAMERA_GPU_TARGET scenarios=4 checks=182"),
        ("missing_clear", 1, "    glClear(mask);", "    /* planted omitted actual glClear */", 1, "actual RGBA bytes within1 inside viewport; outside pixels retained"),
        ("forced_depth_one", 1, "glClearDepth(*depth);mask|=GL_DEPTH_BUFFER_BIT;", "glClearDepth(1.0);mask|=GL_DEPTH_BUFFER_BIT;", 1, "explicit0.375 GPU depth inside viewport; outside depth retained"),
        ("global_color_mask", 1, "s.color_mask_(0,GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);", "glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);", 1, "GPU clear preserves distinct indexed color mask1"),
        ("enabled_discard", 1, "glDisable(GL_DITHER);glDisable(0x8C89);glDisable(0x8DB9);", "glDisable(GL_DITHER);/* planted discard remains enabled */glDisable(0x8DB9);", 1, "actual RGBA bytes within1 inside viewport; outside pixels retained"),
        ("global_viewport", 1, "s.viewport_(0,viewport.x,viewport.y,viewport.width,viewport.height);", "glViewport(viewport.x,viewport.y,viewport.width,viewport.height);", 1, "GPU begin/clear preserves indexed scissor1 and viewport1"),
        ("missing_viewport_restore", 1, "        viewport_(0,prior_viewport_[0],prior_viewport_[1],prior_viewport_[2],prior_viewport_[3]);", "        /* planted omitted viewport restoration */", 1, "end restores fractional host viewport0 exactly"),
        ("foreign_session_raster", 0, " if(!owner||owner->session_.lock()!=session){", " if(!owner){", 1, "both raster fields reject actual foreign Session owners"),
    ]
    for name, index, old, new, expected, needle in plants:
        inputs = [fixture, *PRODUCTION]
        if index is not None:
            source = output / (name + ".cpp");source.write_text(replace_once(originals[index], old, new), encoding="utf8", newline="\n");inputs[index+1] = source
        case(name, "actual_runtime_source_positive" if index is None else "actual_runtime_source_plant", inputs, libraries, [str(ROOT), str(depth)], expected, needle)
    harness = output / "constructed_fault.cpp";harness.write_text(FAULT_HARNESS, encoding="utf8", newline="\n")
    pre = replace_once(originals[1], "    // Retain an actual owner BEFORE the first GL mutation.",
        '    f={DisplayServiceError::native_operation_failed,"constructed pre-bind refusal"};return false;\n    // Retain an actual owner BEFORE the first GL mutation.')
    pre_path = output / "fault_pre_bind.cpp";pre_path.write_text(pre, encoding="utf8", newline="\n")
    injected = replace_once(originals[1], "    bool restore_draw(DisplayServiceFailure& f) noexcept {",
        '    bool restore_draw(DisplayServiceFailure& f) noexcept {\n        static unsigned constructed_restore_calls{};\n        ++constructed_restore_calls;\n        if(constructed_restore_calls==1||constructed_restore_calls==4){f={DisplayServiceError::native_operation_failed,"constructed rollback failure"};return false;}')
    snapshot = '        f={DisplayServiceError::native_operation_failed,"native default target snapshot failed"};return false;'
    position = injected.index(snapshot)
    branch = injected.rfind("    if(glGetError()!=GL_NO_ERROR) {", 0, position)
    need(branch >= 0, "real default snapshot failure branch")
    injected = injected[:branch] + injected[branch:].replace("    if(glGetError()!=GL_NO_ERROR) {", "    if((glGetError()!=GL_NO_ERROR)||true /* constructed snapshot refusal */) {", 1)
    partial_path = output / "fault_partial.cpp";partial_path.write_text(injected, encoding="utf8", newline="\n")
    negative = replace_once(injected, "    s.draw_active_=true;s.draw_ready_=false;s.draw_has_depth_=false;", "    s.draw_active_=false;s.draw_ready_=false;s.draw_has_depth_=false; /* planted missing early ownership */")
    negative_path = output / "fault_missing_ownership.cpp";negative_path.write_text(negative, encoding="utf8", newline="\n")
    host_libs = ["-lopengl32", "-lgdi32", "-luser32", "-lkernel32", "-lpthread"]
    case("pre_bind_refusal", "constructed_real_GL_fault_positive", [harness, pre_path], host_libs, ["pre_bind"], 0, "PASS_NATIVE_DISPLAY_CONSTRUCTED_FAULT mode=pre_bind")
    case("partial_ownership", "constructed_real_GL_fault_positive", [harness, partial_path], host_libs, ["partial"], 0, "PASS_NATIVE_DISPLAY_CONSTRUCTED_FAULT mode=partial")
    case("missing_early_ownership", "constructed_fault_source_plant", [harness, negative_path], host_libs, ["partial"], 1, "failed snapshot retains real owner")
    changes = [p["path"] for p in before if sha(p["path"]) != p["sha256"]]
    ok = len(commands) == 11 and not changes and all(x["status"] == "PASS_NATIVE_DISPLAY_CASE_EXPECTED_BEHAVIOR" for x in commands)
    result = dict(schema=SCHEMA, status="PASS_NATIVE_DISPLAY_TARGET_SOURCE_PLANTS_AND_CONSTRUCTED_FAULTS" if ok else "FAIL_NATIVE_DISPLAY_TARGET_SOURCE_PLANTS_AND_CONSTRUCTED_FAULTS",
        source_pins=before, input_changes=changes, cases=commands, affinity_mask="0xf0", sequential_compilers=True,
        native_credit_added=0, ownership_admissions=0, oracle_runs=0, historical_gameplay_equivalence=False,
        canonical_camera_scope=dict(arguments=2, GPU_scenarios=4, GPU_checks=182, association_scenarios=5,
                                    association_checks=144, optional_diagnostic_file_assertions_not_requested=4),
        driver_query_lines=[line for case in commands if case["run"] for line in case["run"]["stdout"].splitlines()
                            if line.startswith("NATIVE_DISPLAY_FAULT_DRIVER ")],
        hardware_acceleration_inferred_from_counters=False,
        limits=["Real native owner/GPU component behavior and actual-source falsifications only; no game/oracle invocation or full scene claim.",
                "Constructed snapshot/restore failures are injected branch conditions in private actual-service copies; they do not claim observed driver failures.",
                "Two camera arguments are the existing content root and depth corpus; explicit GPU diagnostic readbacks are test observations, not production frame readbacks."])
    save(output / "RESULT.json", result);print(result["status"], "sha256=" + sha(output / "RESULT.json"), flush=True)
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
