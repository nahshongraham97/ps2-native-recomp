"""Owned native geometry/semantic payload integration; movie decoder frozen."""
import ctypes
import datetime
import importlib.util
import json
import os
import pathlib
import re
import subprocess
import sys
FORK=pathlib.Path(__file__).resolve().parents[3]
PARENT=FORK.parents[2]
RECORDS=PARENT/'research/runs/native-movie-ingress-20261119'
COMMON=FORK/'research/runs/native-movie-kernel-policies-20261119/validate.py'
spec=importlib.util.spec_from_file_location('_geometry_evidence',COMMON)
common=importlib.util.module_from_spec(spec)
spec.loader.exec_module(common)
CHANGED=['cmake/native_game.cmake','runtime/include/mka/material_maintenance.hpp',
    'runtime/src/native/material_maintenance.cpp','tests/test_logon_frontend_task.cpp']
NEW=['runtime/include/mka/geometry_attributes.hpp','runtime/src/native/geometry_attributes.cpp',
    'tests/test_geometry_attributes.cpp','runtime/include/mka/geometry_companion_quantizer.hpp',
    'runtime/src/native/geometry_companion_quantizer.cpp','tests/test_geometry_companion_quantizer.cpp',
    'tests/geometry_companion_source_oracle.cpp']

def cache_contract():
    cache=(common.BUILD/'CMakeCache.txt').read_text(encoding='utf-8-sig')
    common.require(bool(re.search(r'^PS2_GENERATED:PATH=$',cache,re.M)),'no generated guest game target')
    common.require(bool(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$',cache,re.M)),'Release')

def verify():
    require,sha,write=common.require,common.sha,common.write
    cache_contract()
    prior_path=FORK/'research/runs/native-hierarchy-preparation-20261120/validation-02/verification.json'
    prior=json.loads(prior_path.read_bytes())
    require(prior['status']=='PASS_NATIVE_HIERARCHY_PREPARATION_QUEUED_INTEGRATION' and
        isinstance(prior['source_after_sha256'],dict) and isinstance(prior['fixtures'],list) and
        len(prior['fixtures'])==4 and prior['queued_runtime_scenarios']==18,'actual prior schema')
    changed={str(FORK/name) for name in CHANGED}
    untouched={name:digest for name,digest in prior['source_after_sha256'].items() if name not in changed}
    require(all(sha(name)==digest for name,digest in untouched.items()),'untouched prior source/tool/asset/library pins')
    review_paths=[RECORDS/'NATIVE_GEOMETRY_ATTRIBUTES_FIXTURE_CLOSURE_A_20261120.json',
        RECORDS/'NATIVE_GEOMETRY_ATTRIBUTES_PEER_B_20261120.json']
    for path,status in zip(review_paths,['SOURCE_SUPPLEMENT_NO_CONCRETE_DEFECT_NOT_EXECUTED',
            'SOURCE_REVIEW_NO_CONCRETE_DEFECT_NOT_BUILT_NOT_EXECUTED']):
        report=json.loads(path.read_bytes())
        require(report['status']==status and isinstance(report['pins'],dict),'actual final source review schema/status')
        require(all(sha(name)==digest for name,digest in report['pins'].items()),'final exact reviewed pins')
    model_path=RECORDS/'native-geometry-companion-model-20261120/reference.py'
    vectors=model_path.with_name('vectors.tsv')
    model_spec=importlib.util.spec_from_file_location('_geometry_companion_model',model_path)
    model=importlib.util.module_from_spec(model_spec)
    model_spec.loader.exec_module(model)
    words=model.inputs(vectors)
    require(len(words)==4096,'actual fixed numeric input schema')
    numeric=FORK/'research/runs/native-material-maintenance-20261120/validation-02/numeric-comparison.json'
    decoder=FORK/'research/runs/native-adx-composition-20261119/validation-02/audio-oracle.json'
    require(sha(numeric)==prior['retained_numeric_certificate_sha256'],'unchanged old matrix numeric certificate')
    require(sha(decoder)==prior['frozen_decoder_certificate_sha256'],'unchanged original decoder certificate')
    paths={pathlib.Path(name) for name in prior['source_after_sha256']}
    paths.update(FORK/name for name in CHANGED+NEW)
    paths.update([COMMON,pathlib.Path(__file__).resolve(),prior_path,model_path,vectors,numeric,decoder,
        FORK/'runtime/include/ps2/fpu.hpp',*review_paths])
    before={str(path):sha(path) for path in sorted(paths)}
    for name in CHANGED+NEW:require(b'\r' not in (FORK/name).read_bytes(),'LF authored source '+name)
    for name,digest in common.PRESERVE.items():require(sha(FORK/name)==digest,'preserved dirty IOP '+name)
    for name,digest in prior['generated_source_sha256'].items():require(sha(name)==digest,'unchanged actual AOT caller')
    write('provenance.json',dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        fork_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=FORK,text=True).strip(),
        parent_head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=PARENT,text=True).strip(),
        source_before_sha256=before,unchanged_prior_pins=len(untouched),preserved_dirty_sha256=common.PRESERVE,
        scope='Actual owned native geometry gather/closed attribute data emission/material ownership and queued integration; no full topology/draw'))
    libraries=PARENT.parent/'dependencies/blender/blender-4.5.15-windows-x64/blender.shared'
    assets=FORK/'games/mkarmageddon/disc-files/MOVIEPS2'
    overlay=PARENT/'research/inputs/modules/MOVIER.ovl'
    cases=[
        ('quantizer','test_native_geometry_companion_quantizer',[],model.NATIVE_FOOTER,
         ['mka::game::quantize_geometry_companion_component']),
        ('attributes','test_native_geometry_attributes',[],
         'PASS native owned geometry gather/attribute payloads/material binding; topology/program construction and draw remain open',
         ['mka::game::NativeGeometryAttributes::vertex','mka::game::emit_geometry_vertex_attributes',
          'mka::game::NativeMaterialGeometry::create_with_attributes']),
        ('maintenance','test_native_material_maintenance',[],
         'PASS native populated material tracking/publication/lifetimes/game step; no hierarchy preparation or renderer bootstrap',
         ['mka::game::NativeMaterialMaintenanceTask::update']),
        ('hierarchy','test_native_material_hierarchy',[],
         'PASS native retail hierarchy preparation/registration/owned geometry; serialized program producer and renderer remain open',
         ['mka::game::NativeMaterialMaintenance::register_hierarchy']),
        ('queued-logon','test_native_logon_frontend',[libraries,assets,'LOGON.SFD',overlay],
         'PASS native queued LOGON with owned geometry/hierarchy/material tasks; deterministic ports and explicit world/frontend probes',
         ['mka::native_logon::NativeLogonFrontendTask::update','mka::game::NativeGeometryAttributes::vertex',
          'mka::game::NativeMaterialGeometry::create_with_attributes'])]
    common.execute('build.json',[common.CMAKE,'--build',common.BUILD,'--target',
        *[row[1] for row in cases],'geometry_companion_source_oracle','-j2'])
    fixtures=[]
    for label,target,args,footer,required in cases:
        print('RUN '+label,flush=True)
        executable=common.BUILD/(target+'.exe')
        run=common.execute(label+'.json',[executable,*args])
        require(run['stdout'].splitlines()[-1]==footer,'complete '+label+' assertions')
        audit=common.symbols(label+'-symbols',executable,run['executable_sha256'],required,True)
        fixtures.append(dict(component=label,executed_target_sha256=run['executable_sha256'],audit=audit))
    candidate=common.execute('quantizer-vectors.json',[common.BUILD/'test_native_geometry_companion_quantizer.exe',vectors])
    require(candidate['executable_sha256']==fixtures[0]['executed_target_sha256'],'same literal/vector candidate executable')
    oracle=common.execute('offline-source-oracle.json',[common.BUILD/'geometry_companion_source_oracle.exe',vectors])
    # compare raises on mismatch, returns None on success (actual producer API).
    model.compare(candidate['stdout'],words,model.NATIVE_FOOTER)
    model.compare(oracle['stdout'],words,model.SOURCE_FOOTER)
    require(candidate['stdout'].splitlines()[:-1]==oracle['stdout'].splitlines()[:-1],'full three-way numeric rows')
    lines=candidate['stdout'].splitlines()
    first=lines[0].split();first[2]=str((int(first[2])+1)%256)
    bad_word=lines[0].split();bad_word[1]='deadbeef' if bad_word[1]!='deadbeef' else '00000000'
    negatives=[]
    for label,bad in [('missing-row','\n'.join(lines[1:])+'\n'),
            ('wrong-value',' '.join(first)+'\n'+'\n'.join(lines[1:])+'\n'),
            ('wrong-source-identity',' '.join(bad_word)+'\n'+'\n'.join(lines[1:])+'\n'),
            ('wrong-footer','\n'.join(lines[:-1]+['PASS'])+'\n'),
            ('extra-row',lines[0]+'\n'+candidate['stdout'])]:
        try:model.compare(bad,words,model.NATIVE_FOOTER)
        except ValueError:negatives.append(label)
        else:raise RuntimeError('numeric checker accepted '+label)
    write('numeric-comparison.json',dict(status='PASS_GEOMETRY_COMPANION_THREE_WAY_PROJECT_SOURCE_NOT_HARDWARE',
        vectors=len(words),literal_cases=19,checker_negatives=negatives,
        candidate_sha256=candidate['executable_sha256'],offline_oracle_sha256=oracle['executable_sha256'],
        limits=['New business attribute encoding only; no decoder or old matrix corpus rerun',
            'Separate offline project-helper executable, never candidate/runtime linkage or hardware evidence']))
    archive=common.BUILD/'libmka_native_material.a'
    common.symbols('material-archive-symbols',archive,sha(archive),
        ['mka::game::NativeGeometryAttributes::vertex','mka::game::quantize_geometry_companion_component'],False)
    ninja=common.BUILD/'build.ninja';build_text=ninja.read_text();linkage=[]
    for target in ['libmka_native_material.a','test_native_logon_frontend.exe','test_native_geometry_attributes.exe']:
        block=re.search(r'^build '+re.escape(target)+r':.*(?:\n[ \t].*)*',build_text,re.M)
        require(block is not None,'actual native target rule')
        text=block.group()
        require('geometry_companion_source_oracle' not in text and 'material_translation_source_oracle' not in text and
            'ps2runtime' not in text,'no offline source oracle/guest runtime candidate linkage')
        linkage.append(dict(target=target,actual_build_rule=text))
    write('linkage-audit.json',dict(ninja_sha256=sha(ninja),candidate_rules=linkage))
    require(before=={name:sha(name) for name in before},'stable source/review/tool/asset/library pins')
    for name,digest in common.PRESERVE.items():require(sha(FORK/name)==digest,'preserved IOP after run')
    cache_contract()
    write('verification.json',dict(status='PASS_NATIVE_GEOMETRY_ATTRIBUTES_QUEUED_INTEGRATION',
        source_after_sha256=before,unchanged_prior_pins=len(untouched),generated_source_sha256=prior['generated_source_sha256'],
        fixtures=fixtures,queued_runtime_scenarios=18,companion_vectors=len(words),
        retained_numeric_certificate_sha256=sha(numeric),frozen_decoder_certificate_sha256=sha(decoder),
        limits=['Actual native gather/closed semantic attribute payloads and same resource ownership, not full topology/draw assembly',
            'New companion encoding agrees with two software references; no new hardware proof',
            'Source importer/world/frontend/settings remain fixture boundaries; GPU/audio/clock ports deterministic',
            'Primitive admission/cache/topology, actual native programs/rendering, audio/world/bootstrap and all-game acceptance OPEN',
            'No decoder/old matrix numeric corpus rerun; no game/physical endpoint/local model/server/driver/settings/backup/card/save action']))
    print(json.dumps(dict(status='PASS_NATIVE_GEOMETRY_ATTRIBUTES_QUEUED_INTEGRATION',fixtures=len(fixtures),
        pins=len(before),unchanged_prior_pins=len(untouched),queued_scenarios=18,companion_vectors=len(words))),flush=True)

if __name__=='__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    common.require(len(sys.argv)==2 and re.fullmatch(r'validation-[0-9]+',sys.argv[1]),'usage: verify.py validation-NN')
    out=pathlib.Path(__file__).resolve().parent/sys.argv[1];out.mkdir(exist_ok=False)
    common.OUT=out;common.ENV=dict(os.environ,PATH=str(common.BIN)+os.pathsep+os.environ['PATH'])
    try:verify()
    except Exception as error:
        common.write('aborted.json',dict(status='NATIVE_GEOMETRY_ATTRIBUTES_CHECK_FAILED',error=str(error)))
        raise
