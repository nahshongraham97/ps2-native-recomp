"""Actual-source real SSF/SEC preview falsification with exact current Ninja recipes.

Only isolated copies, objects, executables and constructed test data are written.
No oracle/game, production mutation, inferred hardware acceleration or new credit.
"""
from pathlib import Path
import argparse
import ctypes
import hashlib
import json
import os
import re
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[2]
SCHEMA = "mkar.native-ssf-bundle.falsification.v1"


def need(value, message):
    if not value:
        raise RuntimeError(message)


def sha(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for data in iter(lambda: stream.read(1 << 20), b""):
            h.update(data)
    return h.hexdigest()


def pin(path):
    path = Path(path).resolve()
    return dict(path=str(path), sha256=sha(path), bytes=path.stat().st_size)


def save(path, value):
    with path.open("x", encoding="utf8", newline="\n") as stream:
        stream.write(json.dumps(value, indent=2) + "\n")


def windows_argv(command):
    shell = ctypes.WinDLL("shell32", use_last_error=True)
    shell.CommandLineToArgvW.argtypes = [ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_int)]
    shell.CommandLineToArgvW.restype = ctypes.POINTER(ctypes.c_wchar_p)
    count = ctypes.c_int()
    args = shell.CommandLineToArgvW(command, ctypes.byref(count))
    need(bool(args), "parse actual Windows compiler command")
    try:
        return [args[i] for i in range(count.value)]
    finally:
        kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        kernel.LocalFree.argtypes = [ctypes.c_void_p]
        kernel.LocalFree(args)


def dependency_paths(path):
    """Clang's actual make dependency file; escaped spaces are not separators."""
    text = path.read_text().replace("\\\n", " ")
    need(": " in text, "actual make dependency target delimiter")
    body = text.split(": ", 1)[1]
    tokens = re.findall(r"(?:\\.|[^\s])+", body)
    return {Path(re.sub(r"\\([ #:$\\])", r"\1", token)).resolve() for token in tokens}


def protected_snapshot():
    parent = ROOT.parents[2]
    base = ROOT / "build/r192-hooks"
    paths = {p for p in (base / "game").rglob("*")
             if p.is_file() and "logs" not in p.relative_to(base / "game").parts}
    paths.update(p for p in (base / "out").glob("*") if p.is_file() and
                 (p.suffix in (".exe", ".dll", ".bat", ".ini") or p.name == ".source-sha256"))
    paths.update(p for p in (parent / "launchers").glob("*")
                 if p.is_file() and p.suffix in (".bat", ".json"))
    return dict(files={str(p): sha(p) for p in sorted(paths)},
                frozen_ref=subprocess.check_output(
                    ["git", "-C", str(ROOT), "rev-parse", "refs/heads/r192-aot-closure-proof"], text=True).strip())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--build-receipt", type=Path, required=True)
    parser.add_argument("--original-volume", type=Path, required=True)
    args = parser.parse_args()
    original_volume = args.original_volume.resolve()
    original_pak = original_volume / "MKDA.PAK"
    out = args.output.resolve()
    need(os.name == "nt", "actual Windows native context required")
    need(out.is_relative_to(ROOT / "b/native-ssf-bundle-01") and not out.exists(), "fresh ignored output child")
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.GetCurrentProcess.restype = ctypes.c_void_p
    kernel.SetProcessAffinityMask.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
    need(kernel.SetProcessAffinityMask(kernel.GetCurrentProcess(), 15), "inherit CPU0..3 affinity")
    need(shutil.disk_usage(ROOT).free >= 12 * (1 << 30), "12GiB reserve")
    need(original_pak.is_file() and sha(original_pak) == "ff51cdff04e6487460ef3c3a1db309a68ced43e4c4bfd084fa965cb41c8c227a", "authenticated original MKDA.PAK input")
    build = json.loads(args.build_receipt.read_bytes())
    need(isinstance(build, dict) and build.get("schema") == "mkar.native-ssf-bundle.focused-build.v1",
         "inspected actual focused build receipt schema")
    need(build.get("exit_code") == 0 and build.get("source_changes") == [], "passing immutable focused build")
    focused_pins = build.get("source_pins")
    need(isinstance(focused_pins, dict) and focused_pins, "actual focused build source pin mapping")
    focused_join = {name: sha(ROOT / name) for name in focused_pins}
    need(focused_join == focused_pins, "all focused compiled-source identities must join before reuse")
    binary_dir = ROOT / "b"
    cache = (binary_dir / "CMakeCache.txt").read_text()
    compiler = Path(re.search(r"^CMAKE_CXX_COMPILER:[^=]+=(.*)$", cache, re.M)[1])
    ninja = Path(re.search(r"^CMAKE_MAKE_PROGRAM:[^=]+=(.*)$", cache, re.M)[1])
    environment = dict(os.environ, PYTHONDONTWRITEBYTECODE="1", OMP_NUM_THREADS="4", OPENBLAS_NUM_THREADS="4")
    environment["PATH"] = str(out) + os.pathsep + str(compiler.parent) + os.pathsep + str(binary_dir) + os.pathsep + environment["PATH"]
    compdb_run = subprocess.run([str(ninja), "-C", str(binary_dir), "-t", "compdb"],
                               capture_output=True, cwd=ROOT, env=environment, creationflags=subprocess.CREATE_NO_WINDOW)
    need(compdb_run.returncode == 0, "read actual Ninja compilation database")
    database = json.loads(compdb_run.stdout)

    def recipe(path):
        matches = [r for r in database if Path(r.get("file", "")).resolve() == path.resolve()]
        need(len(matches) == 1, "unique actual compilation recipe: " + str(path))
        need(Path(matches[0]["directory"]).resolve() == binary_dir, "actual recipe build directory")
        argv = windows_argv(matches[0]["command"])
        need(Path(argv[0]).resolve() == compiler.resolve() and "-c" in argv, "actual compiler and native compilation")
        return matches[0], argv

    families = {
        "scene": ("test_native_retail_ssf_scene.exe", ROOT / "tests/test_retail_ssf_scene.cpp"),
        "texture": ("test_native_retail_ssf_texture.exe", ROOT / "tests/test_retail_ssf_texture.cpp"),
        "mode": ("test_native_retail_archive_mode.exe", ROOT / "tests/test_retail_archive_mode.cpp"),
    }
    recipes = {}
    links = {}
    files = {Path(__file__), compiler, ninja, args.build_receipt, binary_dir / "build.ninja", binary_dir / "SDL2.dll",
             ROOT / "tests/test_retail_renderer_cameras.cpp"}
    files.add(original_pak)
    files.update(ROOT / name for name in focused_pins)
    raw_links = {}
    for family, (target, fixture) in families.items():
        record, argv = recipe(fixture)
        recipes[str(fixture)] = (record, argv)
        files.update((fixture, binary_dir / target))
        result = subprocess.run([str(ninja), "-C", str(binary_dir), "-t", "commands", target],
                                capture_output=True, text=True, cwd=ROOT, env=environment, creationflags=subprocess.CREATE_NO_WINDOW)
        need(result.returncode == 0, "read actual Ninja target commands")
        matches = [line for line in result.stdout.splitlines() if " -o " + target + " " in line]
        need(len(matches) == 1, "unique actual target linker recipe")
        raw = matches[0]
        normalized = raw.replace("\\", "/").lower()
        compiler_at = normalized.index(str(compiler).replace("\\", "/").lower())
        begin = compiler_at - 1 if raw[compiler_at - 1] == '"' else compiler_at
        tail = raw.find(" && ", compiler_at + len(str(compiler)))
        need(tail >= 0, "actual linker shell boundary")
        link = windows_argv(raw[begin:tail])
        need(Path(link[0]).resolve() == compiler.resolve(), "actual Ninja linker compiler")
        need("--allow-multiple-definition" not in " ".join(link), "no duplicate-definition admission")
        links[family] = link
        raw_links[family] = raw
        for item in link[1:]:
            if not item.startswith("-") and Path(item).suffix in (".a", ".dll"):
                files.add((binary_dir / item).resolve())
    sources = {
        "geometry": ROOT / "runtime/src/native/retail_ssf_geometry.cpp",
        "texture": ROOT / "runtime/src/native/retail_ssf_texture.cpp",
        "scene": ROOT / "runtime/src/native/retail_scene_archive.cpp",
        "camera": ROOT / "runtime/src/native/retail_renderer_cameras.cpp",
        "world": ROOT / "runtime/src/native/retail_renderer_world.cpp",
        "continuation": ROOT / "runtime/src/native/retail_first_scene_continuations.cpp",
    }
    for source in sources.values():
        recipes[str(source)] = recipe(source)
        files.add(source)
    canonical_dependencies = {}
    for name, (record, _) in recipes.items():
        result = subprocess.run([str(ninja), "-C", str(binary_dir), "-t", "deps", record["output"]],
                                capture_output=True, text=True, cwd=ROOT, env=environment, creationflags=subprocess.CREATE_NO_WINDOW)
        need(result.returncode == 0 and "(VALID)" in result.stdout.splitlines()[0], "valid canonical Ninja dependency closure")
        paths = [Path(line.strip()).resolve() for line in result.stdout.splitlines()[1:] if line.startswith("    ")]
        need(paths and all(path.is_file() for path in paths), "canonical dependency files must exist")
        files.update(paths)
        canonical_dependencies[name] = dict(object=record["output"], raw=result.stdout, dependencies=[str(path) for path in paths])
    for _, argv in recipes.values():
        for item in argv:
            if item.startswith("-I") and Path(item[2:]).is_dir():
                directory = Path(item[2:])
                files.update(directory.rglob("*.hpp"))
                files.update(directory.rglob("*.h"))
    files.update(compiler.parent.glob("clang*.exe"))
    files.update(compiler.parent.glob("*lld*.exe"))
    files = sorted(files)
    before = [pin(p) for p in files]
    before_by_path = {p["path"]: p["sha256"] for p in before}
    protected_before = protected_snapshot()
    need(len(protected_before["files"]) == 4730 and protected_before["frozen_ref"] == "31d087fa59274d15e35dba1e6c62fe1c53001563", "retained protected inventory and frozen ref")
    out.mkdir(parents=True)
    shutil.copyfile(binary_dir / "SDL2.dll", out / "SDL2.dll")
    shutil.copyfile(Path(__file__), out / "DRIVER.py")
    (out / "NINJA_COMPDB.json").write_bytes(compdb_run.stdout)
    save(out / "NINJA_LINK_RECIPES.json", raw_links)
    save(out / "NINJA_DEPENDENCIES.json", canonical_dependencies)
    save(out / "FOCUSED_SOURCE_JOIN.json", dict(source_pins=focused_pins, current=focused_join,
                                                joined=True, count=len(focused_pins), build_receipt=pin(args.build_receipt)))
    save(out / "PLAN.json", dict(schema=SCHEMA, source_pins=before, protected=protected_before, affinity_mask=15,
        sequential_compilers=True, actual_recipe_scope="Current Ninja compdb and target link commands; no optimization/define/ABI flag substitutions",
        path_adapters=["isolated source/output/dependency/import-library names", "original source quote-include directory"],
        fixture_readiness_adapter="Texture clone gains only a passive input-ready print after the original bounded source-read assertion; all original assertions remain unchanged",
        focused_compiled_source_pin_count=len(focused_pins), focused_source_join=pin(out / "FOCUSED_SOURCE_JOIN.json"),
        native_credit_added=0, oracle_runs=0))
    commands = []

    def command(argv, log):
        need(shutil.disk_usage(ROOT).free >= 12 * (1 << 30), "12GiB reserve during verification")
        started = time.monotonic()
        process = subprocess.Popen(list(map(str, argv)), cwd=binary_dir, env=environment,
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE, creationflags=subprocess.CREATE_NO_WINDOW)
        timed = False
        try:
            stdout, stderr = process.communicate(timeout=300)
        except subprocess.TimeoutExpired:
            timed = True
            # This PID was created here; never kill by executable name or touch another owner's process.
            if process.poll() is None:
                subprocess.run(["taskkill", "/PID", str(process.pid), "/T", "/F"], capture_output=True,
                               creationflags=subprocess.CREATE_NO_WINDOW)
            stdout, stderr = process.communicate()
        log.write_bytes(stdout + stderr)
        row = dict(argv=list(map(str, argv)), cwd=str(binary_dir), exit_code=process.returncode,
                   timed_out=timed, seconds=time.monotonic() - started, log=pin(log),
                   stdout=stdout.decode(errors="replace"), stderr=stderr.decode(errors="replace"))
        commands.append(row)
        return row

    fixture_objects = {}

    def compile_copy(original, copy, obj, log):
        record, actual = recipes[str(original)]
        argv = list(actual)
        for option, destination in (("-o", obj), ("-MF", obj.with_suffix(".d")), ("-MT", obj)):
            need(argv.count(option) == 1, "unique actual compile output/dependency option")
            argv[argv.index(option) + 1] = str(destination)
        argv[argv.index("-c") + 1] = str(copy)
        argv += ["-iquote", str(original.parent)]
        return command(argv, log)

    for family, (_, fixture) in families.items():
        folder = out / ("fixture-" + family)
        folder.mkdir()
        text = fixture.read_text()
        if family == "texture":
            site = "    RetailSsfTextureFailure f;"
            need(text.count(site) == 1, "unique bounded actual texture input readiness site")
            text = text.replace(site, '    std::puts("SSF_TEXTURE_INPUT_READY"); std::fflush(stdout);\n' + site)
        clone = folder / fixture.name
        clone.write_text(text, encoding="utf8", newline="\n")
        obj = folder / "fixture.obj"
        built = compile_copy(fixture, clone, obj, folder / "compile.log")
        need(built["exit_code"] == 0 and not built["timed_out"], "fixture compilation must pass; never accepted as a plant")
        fixture_objects[family] = (obj, built, clone, obj.with_suffix(".d"))

    extent_site = "if(pixels->size()!=t.rgba_raw_alpha.size()) {"
    rollback_site = "~Rollback(){if(!committed)(void)owner.cleanup(failure.cleanup);}"
    cases = [
        ("scene_positive", "scene", None, None, None, 0, "PASS real SSF scene", "SSF_GPU_READY"),
        ("texture_positive", "texture", None, None, None, 0, "PASS native SSF texture", "SSF_TEXTURE_READY"),
        ("mode_resource_positive", "mode", None, None, None, 0, "PASS native archive mode GPU", "ARCHIVE_MODE_GL"),
        ("morph_alignment_omitted", "scene", "geometry", "fits(padding,1);p+=padding;", "fits(padding,1);p+=0;", 1,
         "SSF_ASSERTION: actual selected SSF CPU decode", "SSF_NATIVE_READY"),
        ("triangle_layout_corrupted", "scene", "geometry",
         "RetailSsfGeometryTriangle t{{r.half(p),r.half(p+2),r.half(p+4)},r.half(p+6)};",
         "RetailSsfGeometryTriangle t{{r.half(p+2),r.half(p),r.half(p+6)},r.half(p+4)};", 1,
         "SSF_ASSERTION: actual selected SSF CPU decode", "SSF_NATIVE_READY"),
        ("source_vertex_dropped", "scene", "geometry", "else v.position=value;", "else v.position={0,0,0};", 1,
         "SSF_ASSERTION: actual aligned first morph vertex words", "SSF_NATIVE_READY"),
        ("palette_permutation_corrupted", "texture", "texture",
         "const auto palette_index=(index&~0x18u)|((index&8u)<<1)|((index&16u)>>1);", "const auto palette_index=index;", 1,
         "SSF_TEXTURE_ASSERTION: all4096 original palette-derived RGBA texels", "SSF_TEXTURE_INPUT_READY"),
        ("unproved_mip_profile_admitted", "texture", "texture", "||fields[7]!=0", "", 1,
         "SSF_TEXTURE_ASSERTION: malformed/unsupported native texture field", "SSF_TEXTURE_READY"),
        ("retail_policy_guard_omitted", "scene", "scene",
         "if(config.render_policy!=RetailSsfSceneRenderPolicy::diagnostic_unlit_raw_rgba8)", "if(false)", 1,
         "SSF_ASSERTION: unbound retail render policy refuses before native resource publication", "SSF_NATIVE_READY"),
        ("texture_range_guard_omitted", "scene", "scene",
         "if(std::find(allowed.begin(),allowed.end(),ordinal)==allowed.end()||", "if(false||", 1,
         "SSF_ASSERTION: texture outside clump wrapper range refused", "SSF_NATIVE_READY"),
        ("real_geometry_draw_omitted", "scene", "camera",
         "  glBegin(GL_TRIANGLES);\n  for(const auto& v:obj.triangles){",
         "  glBegin(GL_TRIANGLES);\n  if(false)for(const auto& v:obj.triangles){", 1,
         "SSF_ASSERTION: SSF_GPU_PIXELS: actual mesh depth and unlit texture pixels visible", "SSF_GPU_READY"),
        ("explicit_unlit_policy_omitted", "scene", "camera",
         "const auto shaded=obj.diagnostic_unlit_raw_rgba8?base_c:shade_color(base_c,world_n);",
         "const auto shaded=shade_color(base_c,world_n);", 1,
         "SSF_ASSERTION: SSF_GPU_PIXELS: actual mesh depth and unlit texture pixels visible", "SSF_GPU_READY"),
        ("real_gpu_texture_delete_omitted", "mode", "world",
         "const GLuint id=entry.gl_texture_id;glDeleteTextures(1,&id);",
         "const GLuint id=entry.gl_texture_id;(void)id;", 1,
         "mode transition releases previous native texture names before new draw", "ARCHIVE_MODE_GL"),
        ("foreign_frame_owner_guard_omitted", "scene", "world",
         "||(frames&&core->frames(cf).get()!=frames)", "", 1,
         "SSF_ASSERTION: SSF foreign Frames owner refused before publication", "SSF_NATIVE_READY"),
        ("journal_rollback_defect_after_locked_extent", "scene", "scene", [extent_site, rollback_site],
         ["if(true) { std::puts(\"SSF_EXTENT_LOCK_REACHED\");",
          "~Rollback(){if(!committed&&std::strcmp(failure.operation,\"SSF actual raster storage extent\")!=0)(void)owner.cleanup(failure.cleanup);}"], 1,
         "SSF_ASSERTION: SSF allocation prefix restores actual owner journals and module counts", "SSF_EXTENT_LOCK_REACHED"),
        ("ssf_continuation_skipped", "scene", "continuation", "if(selected_ssf) {", "if(false&&selected_ssf) {", 1,
         "SSF_ASSERTION: connected default retail render policy remains unbound without partial publication", "SSF_GPU_READY"),
        ("locked_extent_clean_fault", "scene", "scene", extent_site, "if(true) {", 1,
         "SSF_ASSERTION: actual SSF decoded data reaches native owners", "SSF_FAILURE_CLEAN"),
    ]
    rows = []
    for name, family, kind, old, new, expected, needle, ready in cases:
        folder = out / name
        folder.mkdir()
        fixture_obj, fixture_build, fixture_clone, fixture_dep = fixture_objects[family]
        build_rows = [dict(fixture_build, reused_within_this_verifier=True)]
        injected = None
        mutation = None
        if kind is not None:
            source = sources[kind]
            text = source.read_text()
            edits = list(zip(old, new)) if isinstance(old, list) else [(old, new)]
            need(not isinstance(old, list) or len(old) == len(new), "complete recorded multi-site mutation")
            for before_edit, after_edit in edits:
                need(text.count(before_edit) == 1, "unique actual source mutation site: " + name)
                text = text.replace(before_edit, after_edit)
            clone = folder / source.name
            clone.write_text(text, encoding="utf8", newline="\n")
            injected = folder / "plant.obj"
            build_rows.append(compile_copy(source, clone, injected, folder / "compile.log"))
            mutation = dict(source=pin(source), clone=pin(clone), before=old, after=new)
        artifact_paths = [fixture_clone, fixture_obj, fixture_dep]
        if injected is not None and injected.exists():
            artifact_paths += [clone, injected, injected.with_suffix(".d")]
        artifacts_before_link = [pin(path) for path in artifact_paths]
        closure = set()
        for path in artifact_paths:
            if path.suffix == ".d":
                closure.update(dependency_paths(path))
        private_sources = {fixture_clone.resolve()}
        if mutation is not None:
            private_sources.add(clone.resolve())
        closure_pins = []
        for dependency in sorted(closure):
            actual = pin(dependency)
            if dependency.is_relative_to(out):
                need(dependency in private_sources, "only the recorded private clones may enter dependency closure")
            else:
                need(before_by_path.get(actual["path"]) == actual["sha256"], "actual dependency must join precompiled source/toolchain pins")
            closure_pins.append(actual)
        argv = list(links[family])
        original_object = recipes[str(families[family][1])][0]["output"]
        need(argv.count(original_object) == 1, "exact actual fixture linker input")
        index = argv.index(original_object)
        argv[index] = str(fixture_obj)
        if injected is not None:
            argv.insert(index + 1, str(injected))
        exe = folder / (name + ".exe")
        argv[argv.index("-o") + 1] = str(exe)
        for i, value in enumerate(argv):
            if value.startswith("-Wl,--out-implib,"):
                argv[i] = "-Wl,--out-implib," + str(folder / "isolated.dll.a")
        if all(r["exit_code"] == 0 and not r["timed_out"] for r in build_rows):
            build_rows.append(command(argv, folder / "link.log"))
        built = len(build_rows) == (3 if injected is not None else 2) and all(
            r["exit_code"] == 0 and not r["timed_out"] for r in build_rows)
        arguments = [str(original_volume), str(folder / "gpu-output")] if family == "scene" else ([str(original_pak)] if family == "texture" else [str(folder / "fixtures")])
        run = command([str(exe), *arguments], folder / "run.log") if built else None
        artifacts_after_run = [pin(path) for path in artifact_paths]
        okay = bool(built and run["exit_code"] == expected and not run["timed_out"] and ready in run["stdout"] and
                    needle in (run["stdout"] if expected == 0 else run["stderr"]) and artifacts_before_link == artifacts_after_run)
        row = dict(schema=SCHEMA, name=name, status="PASS_EXPECTED_ASSERTION" if okay else "FAIL_PRESERVED",
                   source_mutation=mutation, build=build_rows, run=run, expected_exit=expected,
                   required_assertion=needle, required_ready_marker=ready, binary=pin(exe) if exe.exists() else None,
                   artifacts_before_link=artifacts_before_link, artifacts_after_run=artifacts_after_run,
                   compiled_artifacts_unchanged=artifacts_before_link == artifacts_after_run,
                   actual_dependency_closure=closure_pins,
                   dependency_files=[pin(p) for p in artifact_paths if p.suffix == ".d"])
        rows.append(row)
        save(folder / "RESULT.json", row)
        (out / "PROGRESS.json").write_text(json.dumps(rows, indent=2) + "\n", encoding="utf8", newline="\n")
        print(name, row["status"], "exit=" + str(run["exit_code"] if run else None), flush=True)
    changed = [p["path"] for p in before if sha(p["path"]) != p["sha256"]]
    protected_after = protected_snapshot()
    okay = len(rows) == len(cases) and all(r["status"] == "PASS_EXPECTED_ASSERTION" for r in rows) and not changed and protected_after == protected_before
    result = dict(schema=SCHEMA, status="PASS_REAL_SSF_NATIVE_PREVIEW_SOURCE_PLANTS" if okay else "FAIL_PRESERVED",
                  cases=rows, case_count=len(rows), positives=3, negative_plants=13, fault_controls=1, source_pins=before, source_changes=changed,
                  focused_compiled_source_pin_count=len(focused_pins), focused_source_join=pin(out / "FOCUSED_SOURCE_JOIN.json"),
                  protected_before=protected_before, protected_after=protected_after, protected_unchanged=protected_after == protected_before,
                  actual_compile_steps=sum("-c" in r["argv"] for r in commands),
                  actual_link_steps=sum("-o" in r["argv"] and "-c" not in r["argv"] for r in commands),
                  affinity_mask=15, sequential_compilers=True, oracle_runs=0, owned_processes_remaining=0,
                  hardware_acceleration_inferred_from_counters=False, native_credit_added=0,
                  limits=["Actual pinned MKDA.PAK SSF source for scene/texture; the separate constructed mode fixture checks the same real GPU resource deletion owner. Explicit native diagnostic unlit/raw RGBA8 preview only, not a retail scene/morph/material/color producer or whole-game parity.",
                          "Every accepted plant requires compile/link0, exact exit1/assertion and native readiness. Crashes/loader errors do not qualify."])
    save(out / "RESULT.json", result)
    print(result["status"], sha(out / "RESULT.json"), flush=True)
    return 0 if okay else 1


if __name__ == "__main__":
    raise SystemExit(main())
