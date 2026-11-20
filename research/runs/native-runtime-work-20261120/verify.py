"""Real private runtime work, cancellation cut and independent Service keepers."""
import ast, ctypes, datetime, importlib.util, json, os, pathlib, re, subprocess, sys

F = pathlib.Path(__file__).resolve().parents[3]
P = F.parents[2]
R = P / 'research/runs/native-movie-ingress-20261119'
BASE = F / 'research/runs/native-forced-output-20261120/verify.py'
spec = importlib.util.spec_from_file_location('_runtime_work_prior', BASE)
old = importlib.util.module_from_spec(spec)
spec.loader.exec_module(old)
c = old.c
CHANGED = ['runtime/include/mka/bank_preparation.hpp',
    'runtime/src/native/bank_preparation.cpp', 'runtime/include/mka/runtime_preparation.hpp',
    'runtime/include/mka/audio_banks.hpp', 'runtime/src/native/audio_banks.cpp',
    'runtime/include/mka/audio_bank_protocol.hpp', 'runtime/src/native/audio_bank_protocol.cpp',
    'runtime/include/mka/audio_bank_service.hpp', 'runtime/src/native/audio_bank_service.cpp',
    'cmake/native_sound_resources.cmake']
NEW = ['tests/test_runtime_program_work.cpp', 'tests/test_service_runtime_work.cpp']


def prior_cases(ms2, ms4):
    tree = ast.parse(BASE.read_text())
    fun = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == 'verify')
    node = next(n for n in fun.body if isinstance(n, ast.Assign)
        and any(isinstance(t, ast.Name) and t.id == 'cases' for t in n.targets))
    scope = dict(ms2=ms2, ms4=ms4, prior_cases=old.prior_cases)
    exec(compile(ast.Module(body=[node], type_ignores=[]), 'prior24nativecases', 'exec'), scope)
    c.require(len(scope['cases']) == 24, 'actual prior24 case schema')
    return scope['cases']


def verify():
    need, sha, write = c.require, c.sha, c.write
    cache = (c.BUILD / 'CMakeCache.txt').read_text(encoding='utf-8-sig')
    need(bool(re.search(r'^PS2_GENERATED:PATH=$', cache, re.M)) and
        bool(re.search(r'^CMAKE_BUILD_TYPE:STRING=Release$', cache, re.M)),
        'native Release with empty generated input')
    prior_path = F / 'research/runs/native-forced-output-20261120/validation-02/verification.json'
    prior = json.loads(prior_path.read_bytes())
    need(prior['status'] == 'PASS_NATIVE_FORCED_OUTPUT_RETIREMENT_AND_RETAINED_TAILS'
        and len(prior['source_after_sha256']) == 545 and len(prior['fixtures']) == 24,
        'actual prior evidence schema')
    changed = {str(F / n) for n in CHANGED}
    untouched = {n: h for n, h in prior['source_after_sha256'].items() if n not in changed}
    need(len(untouched) == 535 and all(sha(n) == h for n, h in untouched.items()),
        '535 unchanged prior inputs')
    review_path = R / 'NATIVE_RUNTIME_WORK_SOURCE_REVIEW_20261120.json'
    review = json.loads(review_path.read_bytes())
    need(review['status'] == 'SOURCE_REVIEW_NATIVE_PRIVATE_RUNTIME_WORK_VERIFIED'
        and review['source_only'] is True and
        set(review['input_sha256']) == {str(F / n) for n in CHANGED + NEW},
        'actual twelve-input source review schema')
    need(all(sha(n) == h for n, h in review['input_sha256'].items()), 'reviewed current bytes')
    paths = {pathlib.Path(n) for n in prior['source_after_sha256']}
    paths.update(F / n for n in CHANGED + NEW)
    paths.update([prior_path, BASE, pathlib.Path(__file__).resolve(), review_path,
        review_path.with_suffix('.md'), R / 'NATIVE_RUNTIME_WORK_PLAN_20261120.md',
        R / 'NATIVE_RUNTIME_WORK_IMPLEMENTATION_20261120.md', F / 'tests/test_bank_preparation.cpp'])
    need(isinstance(review['source_evidence_sha256'], dict) and
        review['primary_word_rows_checked'] == 181, 'bounded original source evidence schema')
    for name, digest in review['source_evidence_sha256'].items():
        need(sha(name) == digest, 'original cached/fresh source evidence')
        paths.add(pathlib.Path(name))
    snapshot = R / 'native-runtime-work-before-20261120'
    provenance = json.loads((snapshot / 'provenance.json').read_bytes())
    need(set(provenance['source_sha256']) == set(CHANGED), 'actual ten-file before snapshot schema')
    paths.add(snapshot / 'provenance.json')
    for name in CHANGED:
        q = snapshot / name
        need(sha(q) == provenance['source_sha256'][name] == prior['source_after_sha256'][str(F / name)],
            'exact measured working before bytes ' + name)
        paths.add(q)
    before = {str(q): sha(q) for q in sorted(paths)}
    for name in CHANGED + NEW:
        data = (F / name).read_bytes()
        need(b'\r' not in data and data.endswith(b'\n') and not data.endswith(b'\n\n')
            and all(line == line.rstrip(b' \t') for line in data.splitlines()), 'LF/EOF/whitespace ' + name)
    for name, digest in c.PRESERVE.items():
        need(sha(F / name) == digest, 'protected dirty source')
    write('provenance.json', dict(at_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
        fork_head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=F, text=True).strip(),
        parent_head=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=P, text=True).strip(),
        source_before_sha256=before, unchanged_prior_pins=len(untouched),
        compiler_sha256=sha(c.BIN / 'clang++.exe'),
        scope='Actual owned runtime build/decision/retirement; not retail queue/callback/game'))
    ms2 = F / 'games/mkarmageddon/disc-files/SNDSPS2/MSLASSET.MS2'
    ms4 = ms2.with_suffix('.MS4')
    cases = [
        ('runtime-work', 'test_native_runtime_program_work', [ms2],
         r'PASS_NATIVE_RUNTIME_PROGRAM_WORK: 4 actual private build, cancellation, commit and retirement groups'),
        ('service-work', 'test_native_service_runtime_work', [ms2],
         r'PASS_NATIVE_SERVICE_RUNTIME_WORK: 4 actual Service job and independent runtime-reference ownership groups'),
        ('preparation', 'test_native_bank_preparation', [ms2, c.OUT / 'preparation-owned'],
         r'PASS 344 actualbanks 5500eager 4156deferred 179skipprecreation; nativepreparation only nocallback/playback')
    ] + prior_cases(ms2, ms4)
    need(len(cases) == 27 and len({x[1] for x in cases}) == 27, '27 actual affected clients')
    c.execute('build.json', [c.CMAKE, '--build', c.BUILD, '--target', *[x[1] for x in cases], '-j2'])
    fixtures = []
    for label, target, args, footer in cases:
        print('RUN ' + label, flush=True)
        exe = c.BUILD / (target + '.exe')
        run = c.execute(label + '.json', [exe, *args])
        need(bool(run['stdout'].splitlines()) and bool(re.fullmatch(footer, run['stdout'].splitlines()[-1])),
            'complete actual assertions ' + label)
        if label == 'runtime-work':
            required = ['mka::sound::RuntimeProgramWork::finish_build', 'mka::sound::RuntimeProgramWork::retire']
        elif label == 'service-work':
            required = ['mka::sound::AudioBankService::admit_runtime_program', 'mka::sound::BankRuntimeProgramWork::retire']
        elif label == 'voice':
            required = ['mka::sound::HostVoice::start', 'mka::sound::HostVoice::stop_for_resource_retirement']
        elif label in ['forced-output', 'forced-failure']:
            required = ['mka::sound::NativePlaybackOwner::force_retire', 'mka::sound::NativeProgramCopy::transfer_forced_outputs']
        elif label == 'host-clock':
            required = ['mka::sound::NativeProgramHostClock::read']
        elif label == 'output-activation':
            required = ['mka::sound::NativePlaybackOwner::activate', 'mka::sound::NativePlaybackOwner::update_ordinary_context']
        elif label in ['default-runtime', 'playback', 'reservation', 'owner-gaps', 'service-runtime', 'runtime-gaps']:
            required = ['mka::sound::AudioBankService::acquire_runtime_program', 'mka::sound::RuntimeProgramWork::finish_build']
        elif label.startswith('copy'):
            required = ['mka::sound::NativeProgramCopy::clone']
        elif label == 'program':
            required = ['mka::sound::sample_start_operands']
        else:
            required = ['mka::sound::PreparedBank::acquire_runtime_program', 'mka::sound::RuntimeProgramWork::finish_build']
        audit = c.symbols(label + '-symbols', exe, run['executable_sha256'], required, True)
        fixtures.append(dict(component=label, target=target, executed_target_sha256=run['executable_sha256'], audit=audit))
    ninja = c.BUILD / 'build.ninja'
    text = ninja.read_text()
    rules = []
    for label, target, _, _ in cases:
        match = re.search(r'^build ' + re.escape(target + '.exe') + r':.*(?:\n[ \t].*)*', text, re.M)
        need(match is not None and 'ps2runtime' not in match.group() and 'source_oracle' not in match.group(),
            'actual native-only target ' + target)
        wraps = re.findall(r'--wrap=([^\s"]+)', match.group())
        need(len(wraps) == {'forced-failure': 1, 'default-runtime': 1, 'runtime-gaps': 1,
            'copy-failure': 1, 'gates': 1, 'exceptions': 1, 'owner-gaps': 5}.get(label, 0),
            'only designated test fault wrappers ' + label)
        rules.append(dict(target=target, actual_build_rule=match.group(), test_only_wraps=wraps))
    write('linkage-audit.json', dict(ninja_sha256=sha(ninja), target_rules=rules))
    need(before == {n: sha(n) for n in before}, 'inputs/tools/reviews unchanged during execution')
    frozen = F / 'research/runs/native-adx-composition-20261119/validation-02/audio-oracle.json'
    need(sha(frozen) == prior['frozen_decoder_certificate_sha256'], 'completed decoder stays frozen')
    for name, digest in c.PRESERVE.items():
        need(sha(F / name) == digest, 'dirty IOP preserved after execution')
    write('verification.json', dict(status='PASS_NATIVE_PRIVATE_RUNTIME_WORK_AND_SERVICE_RETIREMENT',
        source_after_sha256=before, unchanged_prior_pins=len(untouched), fixtures=fixtures,
        frozen_decoder_certificate_sha256=sha(frozen), limits=[
            'Real private runtime/cached reference transaction; source direct predicate, queue, callbacks and public identity remain OPEN',
            'Preallocated Service result capacity is safety ownership, not delivered reference or source outstanding-job chronology',
            'Changed core bank code justifies existing344-bank preparation check; offline movie decoder/corpus not rerun',
            'No game, real audio/input/GPU endpoint, local model, driver/settings or user-card/save/key operation']))
    print(json.dumps(dict(status='PASS_NATIVE_PRIVATE_RUNTIME_WORK_AND_SERVICE_RETIREMENT',
        fixtures=len(fixtures), pins=len(before), untouched=len(untouched))), flush=True)


if __name__ == '__main__':
    ctypes.windll.kernel32.SetErrorMode(3)
    if len(sys.argv) != 2 or not re.fullmatch(r'validation-[0-9]+', sys.argv[1]):
        raise SystemExit('usage: verify.py validation-NN')
    c.OUT = pathlib.Path(__file__).resolve().parent / sys.argv[1]
    c.OUT.mkdir(exist_ok=False)
    c.ENV = dict(os.environ, PATH=str(c.BIN) + os.pathsep + os.environ['PATH'])
    try:
        verify()
    except Exception as error:
        c.write('aborted.json', dict(status='NATIVE_RUNTIME_WORK_CHECK_FAILED', error=str(error)))
        raise
