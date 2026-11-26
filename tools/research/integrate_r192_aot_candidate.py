"""Integrate a sealed EE package with the owned R192-v8 AOT build.

The protected source/build is input only. SOURCE_INPUTS.json records the exact
private baseline; this tool patches only its owned copy. No game is launched.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(value, reason):
    if not value:
        raise ValueError(reason)


def integrate(build):
    build = build.resolve()
    require(build.is_relative_to((ROOT/'build').resolve()), 'build must be owned by this worktree')
    manifest = build/'out/aot-modules/ee/manifest.txt'
    first = manifest.read_text(encoding='ascii').splitlines()[0].split()
    require(len(first)==5 and first[0]=='R192_AOT_EE_V1', 'manifest schema')
    stamp = int(first[1],16)
    require(first[2:4]==['1456','328'], 'exact R192 v8 ABI required')
    inputs = json.loads((build/'SOURCE_INPUTS.json').read_text(encoding='utf8'))
    by_copy = {str(Path(r['copy']).resolve()):r for r in inputs['files']}

    def original(relative):
        row = by_copy[str((build/'src'/relative).resolve())]
        source = Path(row['original'])
        require(sha(source)==row['sha256'], 'baseline source changed: '+relative)
        return source.read_text(encoding='utf8')

    def put(relative, source):
        destination = build/'src'/relative
        require(destination.resolve().is_relative_to((build/'src').resolve()), 'source escaped owned build')
        destination.parent.mkdir(parents=True,exist_ok=True)
        data=source.encode('utf8')
        if not destination.exists() or destination.read_bytes()!=data:
            destination.write_bytes(data)

    mappings=[]
    for unit,old,pc in [('0018',309,0x162e70),('0018',328,0x1637d0),('0020',126,0x181f90),('0025',204,0x197bc0)]:
        text=(build/'generated_ee'/('unit_'+unit+'.cpp')).read_text(encoding='utf8')
        match=re.search(r'case\s+(\d+)u?:\s*goto\s+L_'+f'{pc:08X}'+r'\s*;',text)
        require(match is not None,'missing intended hook PC '+hex(pc))
        mappings.append(dict(unit=unit,old=old,new=int(match[1]),pc=f'{pc:08X}'))
    for relative in ['host/mka_combat_hooks.hpp','host/mka_unit_wrappers.cpp']:
        text=original(relative)
        for mapping in mappings:
            text=re.sub(r'\b'+str(mapping['old'])+r'u\b',str(mapping['new'])+'u',text)
        put(relative,text)
    for filename in ['loader.cpp','loader.hpp']:
        source=ROOT/'tools/research/r192_aot_modules'/filename
        destination=build/'src/host/r192_aot_modules'/filename
        destination.parent.mkdir(parents=True,exist_ok=True)
        if not destination.exists() or sha(destination)!=sha(source):shutil.copyfile(source,destination)

    main=original('host/main.cpp')
    main=main.replace('#include "mka_combat_hooks.hpp"','#include "mka_combat_hooks.hpp"\n#include "r192_aot_modules/loader.hpp"',1)
    main=main.replace('bool no_record = false;','bool no_record = false;\n    bool verify_aot_package = false;',1)
    main=main.replace('else if (a == "--no-record") no_record = true;',
                      'else if (a == "--no-record") no_record = true;\n        else if (a == "--verify-aot-package") verify_aot_package = true;',1)
    needle='    std::sort(ps2::g_options.frame_capture_frames.begin()'
    require(needle in main,'argument parsing seam')
    main=main.replace(needle,'    if (verify_aot_package && window) return usage();\n'+needle,1)
    start=main.index('    // Vector programs the build does not know are compiled in the background')
    end=main.index('    register_generated_code();',start)
    main=main[:start]+main[end:]
    start=main.index('    // Main processor code the build does not know')
    end=main.index('    const auto emulate =',start)
    main=main[:start]+main[end:]
    seam='    register_generated_code();\n    register_generated_vu();'
    require(main.count(seam)==1,'generated registry startup seam')
    startup='''
    try {
        std::wstring executable(32768, L'\\0');
        const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
        if (!length || length >= executable.size()) throw std::runtime_error("[sealed-ee-aot] cannot locate executable directory");
        executable.resize(length);
        mka::aot::initialize_precompiled_aot_modules(
            std::filesystem::path(executable).parent_path() / "aot-modules" / "ee");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\\n", error.what());
        return 6;
    }
    if (verify_aot_package) return 0;
'''
    main=main.replace(seam,seam+startup,1)
    needle='        if (ps2::replay_active()) return ps2::replay_status();'
    replacement='''        if (ps2::halt_reason().starts_with("missing code at") || ps2::halt_reason()=="bad entry" ||
            ps2::halt_reason().starts_with("VU1 AOT missing")) return 4;
'''+needle
    require(main.count(needle)==2,'run result seams')
    main=main.replace(needle,replacement)
    require('PS2_EE_LIVE' not in main and 'PS2_VU_LIVE' not in main,'live startup survived')
    put('host/main.cpp',main)

    vu=original('runtime/src/vu_micro.cpp')
    vu=vu.replace('static const bool interpret_only = std::getenv("PS2_VU_INTERPRET") != nullptr;','constexpr bool interpret_only = false;')
    vu=vu.replace('static const bool check_vs_interpreter = std::getenv("PS2_VU_CHECK") != nullptr;','constexpr bool check_vs_interpreter = false;')
    vu=vu.replace('static const bool exact = std::getenv("PS2_VU_EXACT") != nullptr;','constexpr bool exact = false;')
    needle='        const ProgramFn fn = lookup(pc);'
    require(vu.count(needle)==1,'VU compiled function seam')
    vu=vu.replace(needle,needle+'\n        if (!fn) { ps2::halt("VU1 AOT missing program"); return; }',1)
    vu=vu.replace('if (fn && !is_target_diag) {','if (fn) {',1)
    put('runtime/src/vu_micro.cpp',vu)

    cmake=original('CMakeLists.txt')
    needle='add_executable(ps2run host/main.cpp host/mka_unit_wrappers.cpp)'
    require(cmake.count(needle)==1,'CMake executable seam')
    cmake=cmake.replace(needle,'add_executable(ps2run host/main.cpp host/mka_unit_wrappers.cpp host/r192_aot_modules/loader.cpp)\n    target_compile_definitions(ps2run PRIVATE R192_AOT_EE_STAMP=0x'+f'{stamp:016x}ULL'+ '\n        R192_AOT_MANIFEST_SHA256="'+sha(manifest)+'")',1)
    cmake=cmake.replace('target_link_libraries(ps2run PRIVATE ps2game ps2runtime)','target_link_libraries(ps2run PRIVATE ps2game ps2runtime bcrypt)',1)
    cmake=cmake.replace('    target_link_options(ps2run PRIVATE\n','    target_link_options(ps2run PRIVATE\n        "-Wl,-Map,${CMAKE_BINARY_DIR}/r192-aot-link.map"\n',1)
    require('runtime/src/ee_live.cpp' not in cmake and 'runtime/src/vu_live.cpp' not in cmake,'translator linked')
    put('CMakeLists.txt',cmake)
    receipt=dict(status='INTEGRATED_SOURCE',manifest_sha256=sha(manifest),stamp=f'{stamp:016X}',
                 compiled_source_cpp_count=len(list((build/'generated_ee').glob('*.cpp'))),
                 hook_entries=mappings,loader_sha256=sha(build/'src/host/r192_aot_modules/loader.cpp'),
                 tool_sha256=sha(Path(__file__)),no_runtime_EE_VU_compiler=True,
                 no_VU1_interpreter_fallback=True,verification='Requires compile/package/replay; no native credit')
    (build/'INTEGRATION.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf8',newline='\n')
    return receipt


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--build-root',type=Path,required=True)
    ap.add_argument('--compile',action='store_true')
    ap.add_argument('--vu-stage',type=Path)
    args=ap.parse_args()
    if args.vu_stage:
        build=args.build_root.resolve(); stage=args.vu_stage.resolve()
        require(build.is_relative_to((ROOT/'build').resolve()) and stage.is_relative_to(build), 'VU stage must stay owned')
        files=sorted(stage.glob('vu1_*.cpp'))
        require(files and (stage/'vu1_table.cpp').is_file(),'complete generated VU stage required')
        present=sorted((build/'generated_ee').glob('vu1_*.cpp'))
        identical={p.name:sha(p) for p in present}=={p.name:sha(p) for p in files}
        if not identical:
            archive=build/'baseline-vu-generated'
            require(not archive.exists(),'VU baseline archive already exists; inspect before replacing again')
            archive.mkdir()
            for path in present:
                destination=archive/path.name
                require(path.resolve().is_relative_to(build) and destination.resolve().is_relative_to(build),'VU move escaped owned build')
                shutil.move(str(path),str(destination))
            for path in files:
                shutil.copyfile(path,build/'generated_ee'/path.name)
        (build/'VU_INTEGRATION.json').write_text(json.dumps(dict(stage=str(stage),files={p.name:sha(p) for p in files},prior_sources_preserved=str(build/'baseline-vu-generated')),indent=2)+'\n',encoding='utf8',newline='\n')
    receipt=integrate(args.build_root)
    print(json.dumps(receipt),flush=True)
    if args.compile:
        config=json.loads((args.build_root/'CONFIGURE.json').read_text())
        env=os.environ.copy();env['PATH']=str(Path(config['compiler']).parent)+';'+env.get('PATH','')
        run=subprocess.run(config['argv'],env=env,capture_output=True)
        (args.build_root/'configure-sealed.log').write_bytes(run.stdout+run.stderr)
        require(run.returncode==0,'sealed configure failed')
        argv=json.loads((args.build_root/'BUILD_ARGV.json').read_text())
        with (args.build_root/'build-sealed.log').open('wb') as log:
            run=subprocess.run(argv,env=env,stdout=log,stderr=subprocess.STDOUT)
        require(run.returncode==0,'sealed compile/link failed')
        print(json.dumps(dict(status='COMPILED',binary_sha256=sha(args.build_root/'out/ps2run.exe'))),flush=True)


if __name__=='__main__':
    main()
