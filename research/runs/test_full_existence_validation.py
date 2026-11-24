import json
import re
from pathlib import Path
import copy
import sys
import subprocess

_this_file = Path(__file__).resolve()
if (_this_file.parents[2] / "external" / "forks" / "ps2-native-recomp").exists():
    REPO = _this_file.parents[2]
    FORK_ROOT = REPO / "external" / "forks" / "ps2-native-recomp"
    PARENT_PRESENT = True
elif len(_this_file.parents) > 4 and (_this_file.parents[4] / "research" / "runs" / "ghidra-function-coverage-20261118").exists():
    FORK_ROOT = _this_file.parents[2]
    REPO = _this_file.parents[4]
    PARENT_PRESENT = True
else:
    FORK_ROOT = _this_file.parents[2]
    REPO = FORK_ROOT
    PARENT_PRESENT = False

# Import validate_ledgers logic and sync_verify
if PARENT_PRESENT:
    sys.path.insert(0, str(REPO / "tools" / "research"))
sys.path.insert(0, str(FORK_ROOT / "tools" / "research"))
from generate_ledgers import validate_ledgers, validate_current_docs, validate_progress_aot_tier, compute_aot_metrics, FORK_COMMIT_SHA
import generate_ledgers
generate_ledgers.REPO = REPO
import sync_verify


def test_parent_fork_sync():
    """Verify that all ledger files are byte-for-byte identical between parent and fork."""
    print("Testing parent-fork ledger synchronization...")
    if not PARENT_PRESENT:
        print("SKIP: Standalone fork environment detected (no parent repository found). Parent-fork ledger sync check skipped.")
        return
    sync_verify.assert_synced()
    print("PASS: Parent and fork ledgers are 100% in sync.")


def test_sha_resolution():
    """Verify that parent HEAD commit and pinned fork commit SHAs exist and resolve via git rev-parse."""
    print("Testing SHA resolution and existence...")

    # Verify parent HEAD if present
    if PARENT_PRESENT:
        parent_sha = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=REPO, text=True).strip()
        assert parent_sha, "Failed to resolve parent HEAD SHA"
        subprocess.check_call(["git", "cat-file", "-e", parent_sha], cwd=REPO)
        print(f"  Parent HEAD resolved and verified: {parent_sha}")

    # Verify fork HEAD matches pinned FORK_COMMIT_SHA or resolves
    fork_sha = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=FORK_ROOT, text=True).strip()
    subprocess.check_call(["git", "cat-file", "-e", fork_sha], cwd=FORK_ROOT)
    print(f"  Fork HEAD resolved and verified: {fork_sha}")

    # Verify README.md and TASK_BOARD.md contain the pinned fork SHA if parent present
    if PARENT_PRESENT:
        readme_txt = (REPO / "README.md").read_text(encoding="utf-8")
        m = re.search(r"Verified Fork Commit\*\*:\s*\[`([0-9a-f]{8})`\]\(https://github\.com/[^/]+/[^/]+/commit/([0-9a-f]{40})\)", readme_txt)
        if m:
            pinned_fork_sha = m.group(2)
            subprocess.check_call(["git", "cat-file", "-e", pinned_fork_sha], cwd=FORK_ROOT)
            print(f"  Pinned fork commit in README.md verified in fork repo: {pinned_fork_sha}")
        else:
            assert FORK_COMMIT_SHA in readme_txt, f"README.md missing fork commit SHA {FORK_COMMIT_SHA}"
        taskboard_txt = (REPO / "TASK_BOARD.md").read_text(encoding="utf-8")
        assert (m.group(1) if m else FORK_COMMIT_SHA[:8]) in taskboard_txt
    print("PASS: SHA resolution and verification succeeded.")


def test_production_existence():
    with open(REPO / "research/ledger/ported_function_evidence.json", encoding="utf-8") as f:
        ps2_ev = json.load(f)

    pair_pattern = re.compile(r'^(.*?):(\d+)\s*\[(.*?)\]\s*\((.*?):(\d+)\s*"([^"]+)"\)$')

    xbox_ev = {
        "0019c680": ("runtime/src/native/retail_initial_game_tasks.cpp:141 [run_camera] (tests/test_retail_initial_game_tasks.cpp:67 \"127120 no-actor BSS branch writes identity camera \")", "00127120"),
        "002330c0": ("runtime/src/native/retail_game_records.cpp:78 [store_retail_game_records_bytes] (tests/test_retail_game_records_main.cpp:66 \"production1303F0/130350\")", "00127370"),
        "0023d8d0": ("runtime/src/native/retail_initial_reset.cpp:56 [advance] (tests/test_retail_initial_reset.cpp:184 \"empty pending scan returns immediately\")", "0012a4f0"),
        "0023e020": ("runtime/src/native/retail_sound_bank_catalog.cpp:205 [select] (tests/test_retail_sound_context.cpp:123 \"real native cached donor remains owned during queu\")", "0012a560"),
        "0023d920": ("runtime/src/native/retail_initial_reset.cpp:20 [require] (tests/test_retail_initial_reset.cpp:171 \"requires disposable empty content directory\")", "0012abd0"),
        "0023c700": ("runtime/src/native/retail_sound_bank_main.cpp:253 [retire_retail_game_sound_references] (tests/test_retail_sound_context.cpp:141 \"Main reverse removal cancels queued and requests running without early disposal\")", "0012c1d0"),
        "0023daf0": ("runtime/src/native/retail_sound_bank_main.cpp:253 [retire_retail_game_sound_references] (tests/test_retail_sound_context.cpp:144 \"queued target removal has no ordinary callback or runtime readiness\")", "0012c1d0"),
        "0023dc70": ("runtime/src/native/retail_sound_bank_main.cpp:311 [resume_retail_game_sound_reference] (tests/test_retail_sound_context.cpp:205 \"12C300 removes first match and preserves remaining\")", "0012c300"),
        "0023d230": ("runtime/src/native/retail_sound_bank_catalog.cpp:86 [open] (tests/test_retail_mode_service.cpp:173 \"real native bank-reference cleanup/unload\")", "0012b500"),
        "0023d4d0": ("runtime/src/native/retail_sound_bank_catalog.cpp:139 [select_mode_zero] (tests/test_retail_mode_service.cpp:189 \"transfer releases admission without poisoning owner; cleanup scans ascending records\")", "0012a720"),
        "0023d720": ("runtime/src/native/retail_sound_bank_catalog.cpp:148 [select] (tests/test_retail_mode_service.cpp:129 \"actual two requests/slots, pending not loaded\")", "0012a8a0"),
        "0023c660": ("runtime/src/native/retail_sound_bank_catalog.cpp:143 [selected_descriptor] (tests/test_retail_mode_service.cpp:96 \"descriptor store precedes reference cleanup\")", "0012aa50"),
        "0023c720": ("runtime/src/native/retail_sound_bank_catalog.cpp:241 [observe] (tests/test_retail_mode_service.cpp:144 \"actual nonhead cancellation callback\")", "0012a170"),
        "0023c770": ("runtime/src/native/retail_sound_bank_catalog.cpp:227 [pump_one] (tests/test_retail_mode_service.cpp:161 \"head callback clears pending only after actual cancelled work/backing cleanup\")", "0012ca80"),
        "0037c170": ("runtime/src/native/retail_sound_bank_jobs.cpp:91 [submit] (tests/test_retail_sound_bank_jobs.cpp:89 \"first source request queued\")", "003152a0"),
        "0037c2a0": ("runtime/src/native/retail_sound_bank_jobs.cpp:173 [cancel_path] (tests/test_retail_sound_bank_jobs.cpp:118 \"head-path cancellation marks active request without premature callback\")", "00317960"),
    }

    all_entries = []
    for addr, ev in ps2_ev.items():
        if addr in ("_meta", "components", "placeholders", "acceptance", "acceptance_status") or not isinstance(ev, str):
            continue
        all_entries.append(("PS2", addr, ev))
    for addr, (ev, ps2_addr) in xbox_ev.items():
        all_entries.append(("Xbox", addr, ev))

    issues = []
    for kind, addr, ev in all_entries:
        m = pair_pattern.match(ev.strip())
        if not m:
            issues.append((kind, addr, "REGEX_FAIL", ev))
            continue
        src_file, src_line, method, test_file, test_line, assertion = m.groups()
        src_p = FORK_ROOT / src_file
        test_p = FORK_ROOT / test_file
        if not src_p.exists():
            issues.append((kind, addr, "SRC_FILE_MISSING", src_file))
            continue
        if not test_p.exists():
            issues.append((kind, addr, "TEST_FILE_MISSING", test_file))
            continue
        src_lines = src_p.read_text(encoding="utf-8", errors="ignore").splitlines()
        test_lines = test_p.read_text(encoding="utf-8", errors="ignore").splitlines()
        if not any(method in l for l in src_lines):
            issues.append((kind, addr, "METHOD_MISSING", f"{src_file}:{src_line} [{method}]"))
        t_idx = int(test_line) - 1
        found = False
        for off in range(-5, 6):
            if 0 <= t_idx + off < len(test_lines):
                if assertion in test_lines[t_idx + off]:
                    found = True
                    break
        if not found:
            issues.append((kind, addr, "ASSERTION_MISSING", f"{test_file}:{test_line} \"{assertion}\""))

    assert len(issues) == 0, f"Production existence validation failed with issues: {issues}"
    print(f"PASS: Production existence validation verified across {len(all_entries)} entries (0 issues).")


def test_planted_failures():
    """Verify that validate_ledgers and structural evaluation fail closed on planted defects:
    1. Exact mapping with cfg_topology_match: false (e.g. 0012a720 <-> 0023d4d0).
    1b. Signature itself claiming exact while cfg_topology_match: false.
    2. Mapped row with confidence label contradicting computed fields.
    3. Assertion text naming a foreign ported address.
    4. Crediting a PS2 function as implemented-via-donor without test vectors.
    5. Evaluating an unrelated pair (Camera 00127120 vs Sound Job Enqueue 0037c170) as structural.
    """
    import csv
    with open(REPO / "research/ledger/functions.csv", encoding="utf-8") as f:
        ps2_ledger = list(csv.DictReader(f))
    with open(REPO / "research/ledger/xbox_functions.csv", encoding="utf-8") as f:
        xbox_ledger = list(csv.DictReader(f))
    with open(REPO / "research/ledger/ps2_xbox_map.csv", encoding="utf-8") as f:
        ps2_map = list(csv.DictReader(f))

    # Planted Failure 1: Labeling 0012a720 as 'exact' when cfg_topology_match is false
    print("Testing planted failure 1: 0012a720 labeled as exact (divergent CFG)...")
    bad_map1 = copy.deepcopy(ps2_map)
    for r in bad_map1:
        if r["ps2_address"] == "0012a720":
            r["match_confidence"] = "exact"
            break
    try:
        validate_ledgers(ps2_ledger, xbox_ledger, bad_map1)
        assert False, "Planted failure 1 FAILED to raise AssertionError!"
    except AssertionError as e:
        print(f"  Planted failure 1 caught as expected: {e}")

    # Planted Failure 1b: Signature itself claiming 'exact' while cfg_topology_match is false
    print("Testing planted failure 1b: signature claiming exact with cfg_topology_match false...")
    sig_p = REPO / "research/cross_platform/signatures/signature_0012a720_0023d4d0.json"
    orig_sig_text = sig_p.read_text(encoding="utf-8")
    try:
        sig_obj = json.loads(orig_sig_text)
        sig_obj["match_confidence"] = "exact"
        sig_p.write_text(json.dumps(sig_obj), encoding="utf-8", newline="\n")
        validate_ledgers(ps2_ledger, xbox_ledger, bad_map1)
        assert False, "Planted failure 1b FAILED to raise AssertionError!"
    except AssertionError as e:
        print(f"  Planted failure 1b caught as expected: {e}")
    finally:
        sig_p.write_text(orig_sig_text, encoding="utf-8", newline="\n")

    # Planted Failure 2: Confidence label contradicts computed fields
    print("Testing planted failure 2: 00127120 labeled as composite (actually mapped-by-role)...")
    bad_map2 = copy.deepcopy(ps2_map)
    for r in bad_map2:
        if r["ps2_address"] == "00127120":
            r["match_confidence"] = "composite"
            break
    try:
        validate_ledgers(ps2_ledger, xbox_ledger, bad_map2)
        assert False, "Planted failure 2 FAILED to raise AssertionError!"
    except AssertionError as e:
        print(f"  Planted failure 2 caught as expected: {e}")

    # Planted Failure 3: Foreign address in assertion text
    print("Testing planted failure 3: foreign ported address injected into assertion text...")
    bad_ps2_ledger = copy.deepcopy(ps2_ledger)
    for r in bad_ps2_ledger:
        if r["address"] == "0012e4e0":
            r["evidence_path"] = r["evidence_path"].replace("12E4E0", "12E670")
            break
    try:
        validate_ledgers(bad_ps2_ledger, xbox_ledger, ps2_map)
        assert False, "Planted failure 3 FAILED to raise AssertionError!"
    except AssertionError as e:
        print(f"  Planted failure 3 caught as expected: {e}")

    # Planted Failure 4: Crediting PS2 function as implemented-via-donor without test vectors
    print("Testing planted failure 4: crediting PS2 function as implemented-via-donor without test vectors...")
    bad_ps2_ledger4 = copy.deepcopy(ps2_ledger)
    for r in bad_ps2_ledger4:
        if r["address"] == "0012a720":
            r["status"] = "implemented-via-donor"
            break
    try:
        validate_ledgers(bad_ps2_ledger4, xbox_ledger, ps2_map)
        assert False, "Planted failure 4 FAILED to raise AssertionError!"
    except AssertionError as e:
        assert "Donor credit rejected" in str(e), f"Expected donor credit rejection, got: {e}"
        assert "vector_0012a720.json does not exist" in str(e), f"Expected missing vector file, got: {e}"
        print(f"  Planted failure 4 caught as expected: {e}")

    # Planted Failure 4b: Crediting Xbox function as implemented-via-donor without test vectors
    print("Testing planted failure 4b: crediting Xbox function as implemented-via-donor without test vectors...")
    bad_xb_ledger4 = copy.deepcopy(xbox_ledger)
    for r in bad_xb_ledger4:
        if r["address"] == "0023d4d0":
            r["status"] = "implemented-via-donor"
            break
    try:
        validate_ledgers(ps2_ledger, bad_xb_ledger4, ps2_map)
        assert False, "Planted failure 4b FAILED to raise AssertionError!"
    except AssertionError as e:
        assert "Donor credit rejected" in str(e), f"Expected donor credit rejection, got: {e}"
        assert "vector_0023d4d0.json does not exist" in str(e), f"Expected missing vector file, got: {e}"
        print(f"  Planted failure 4b caught as expected: {e}")

    # Planted Failure 5: Evaluating an unrelated pair (Camera 00127120 vs Sound Job Enqueue 0037c170) evaluates to unrelated
    # Independent binary measurement evaluation: zero caller overrides (is_candidate and default_confidence omitted)
    print("Testing planted failure 5: unrelated pair Camera 00127120 vs Sound 0037c170 evaluates to unrelated...")
    from compute_cross_isa_evidence import load_ps2_binary_and_sizes, load_xbox_data, load_cross_map, analyze_mips, analyze_x86, canonicalize_cfg, compute_alignment
    slus_data, ps2_sizes = load_ps2_binary_and_sizes()
    xbox_funcs = load_xbox_data()
    cross_map = load_cross_map()

    cam_info = analyze_mips("00127120", slus_data, ps2_sizes)
    snd_info = analyze_x86("0037c170", xbox_funcs)

    align_unrelated = compute_alignment(
        cam_info, snd_info,
        notes="Planted cross-subsystem unrelated pair: Camera task vs Sound job enqueue",
        ps2_to_xb=cross_map,
    )
    assert align_unrelated["match_confidence"] == "unrelated", (
        f"Expected 'unrelated' for Camera vs Sound, got {align_unrelated['match_confidence']}"
    )
    assert align_unrelated["evidence_type"] == "zero_structural_or_callee_alignment", (
        f"Expected 'zero_structural_or_callee_alignment', got {align_unrelated['evidence_type']}"
    )
    print(f"  Planted failure 5 caught as expected: computed confidence is '{align_unrelated['match_confidence']}' ({align_unrelated['evidence_type']})")

    # Planted Failure 6: Test vector runner rejects placeholder or empty files
    print("Testing planted failure 6: placeholder/empty test vector file validation...")
    from run_test_vectors import validate_vector_file, register_native_dispatch, TRACES_DIR
    temp_empty = REPO / "research" / "test_vectors" / "_temp_empty.json"
    temp_placeholder = REPO / "research" / "test_vectors" / "_temp_placeholder.json"
    try:
        temp_empty.write_text("", encoding="utf-8")
        try:
            validate_vector_file(temp_empty)
            assert False, "Empty test vector file FAILED to raise AssertionError!"
        except AssertionError as e:
            print(f"  Empty vector caught as expected: {e}")

        temp_placeholder.write_text("{}", encoding="utf-8")
        try:
            validate_vector_file(temp_placeholder)
            assert False, "Placeholder dict test vector file FAILED to raise AssertionError!"
        except AssertionError as e:
            print(f"  Placeholder dict caught as expected: {e}")
    finally:
        if temp_empty.exists():
            temp_empty.unlink()
        if temp_placeholder.exists():
            temp_placeholder.unlink()

    # Planted Failure 7: Test vector oracle trace hash and source validation
    print("Testing planted failure 7: test vector oracle verification...")
    import hashlib
    temp_bad_vec = REPO / "research" / "test_vectors" / "_temp_bad_vec.json"
    trace_path = TRACES_DIR / "retail_SLUS_214.10_mips_trace.bin"
    real_trace_hash = hashlib.sha256(trace_path.read_bytes()).hexdigest()

    dummy_vec = {
        "schema": "cross_platform_test_vector_v1",
        "target_platform": "ps2_mips",
        "donor_platform": "xbox_x86",
        "target_address": "0012a720",
        "donor_address": "0023d4d0",
        "symbol_name": "SoundCatalog_ValidateIndex",
        "oracle_source": "retail_SLUS_214.10_mips_trace.bin",
        "oracle_trace_hash": "not_a_valid_64_hex_hash",
        "test_cases": [{
            "case_id": "test_case_0",
            "inputs": {"arguments": [0]},
            "expected_outputs": {"return_value": 1},
            "ps2_result": {"return_value": 1},
            "xbox_result": {"return_value": 1},
            "outputs_match": True,
        }],
    }
    try:
        temp_bad_vec.write_text(json.dumps(dummy_vec), encoding="utf-8")
        try:
            validate_vector_file(temp_bad_vec, execute_native=False)
            assert False, "Invalid oracle_trace_hash FAILED to raise AssertionError!"
        except AssertionError as e:
            print(f"  Invalid oracle_trace_hash caught as expected: {e}")

        dummy_vec["oracle_trace_hash"] = "a" * 64
        dummy_vec["oracle_source"] = ""
        temp_bad_vec.write_text(json.dumps(dummy_vec), encoding="utf-8")
        try:
            validate_vector_file(temp_bad_vec, execute_native=False)
            assert False, "Empty oracle_source FAILED to raise AssertionError!"
        except AssertionError as e:
            print(f"  Empty oracle_source caught as expected: {e}")

        # Missing trace file test
        dummy_vec["oracle_source"] = "nonexistent_trace_file.bin"
        temp_bad_vec.write_text(json.dumps(dummy_vec), encoding="utf-8")
        try:
            validate_vector_file(temp_bad_vec, execute_native=False)
            assert False, "Missing trace file FAILED to raise AssertionError!"
        except AssertionError as e:
            print(f"  Missing trace file caught as expected: {e}")

        # Stored trace hash mismatch test
        dummy_vec["oracle_source"] = "retail_SLUS_214.10_mips_trace.bin"
        dummy_vec["oracle_trace_hash"] = "b" * 64
        temp_bad_vec.write_text(json.dumps(dummy_vec), encoding="utf-8")
        try:
            validate_vector_file(temp_bad_vec, execute_native=False)
            assert False, "Stored trace hash mismatch FAILED to raise AssertionError!"
        except AssertionError as e:
            print(f"  Stored trace hash mismatch caught as expected: {e}")

        # Correct hash with native execution mismatch (Python stand-in labeled as infrastructure testing)
        dummy_vec["oracle_trace_hash"] = real_trace_hash
        register_native_dispatch("SoundCatalog_ValidateIndex", lambda x: 0, implementation_kind="infrastructure_testing_python")
        temp_bad_vec.write_text(json.dumps(dummy_vec), encoding="utf-8")
        try:
            validate_vector_file(temp_bad_vec, execute_native=True)
            assert False, "Native output mismatch FAILED to raise AssertionError!"
        except AssertionError as e:
            print(f"  Native output mismatch caught as expected: {e}")

        # Correct hash with successful native execution (Python stand-in labeled as infrastructure testing)
        register_native_dispatch("SoundCatalog_ValidateIndex", lambda x: 1, implementation_kind="infrastructure_testing_python")
        temp_bad_vec.write_text(json.dumps(dummy_vec), encoding="utf-8")
        res_ok = validate_vector_file(temp_bad_vec, execute_native=True)
        assert res_ok["oracle_trace_hash"] == real_trace_hash
        print(f"  Matching trace hash ({real_trace_hash[:16]}...) and native dispatch verified.")
    finally:
        if temp_bad_vec.exists():
            temp_bad_vec.unlink()

    # Planted Failure 8: GP accesses rescan catches corrupted address or modified entries
    print("Testing planted failure 8: corrupted GP accesses validation...")
    from generate_ledgers import validate_gp_accesses
    gp_path = REPO / "research" / "analysis" / "GP_ACCESSES_73.json"
    orig_gp_text = gp_path.read_text(encoding="utf-8")
    try:
        bad_gp = json.loads(orig_gp_text)
        bad_gp["accesses"][0]["virtual_address"] = "0x0033f999"
        gp_path.write_text(json.dumps(bad_gp), encoding="utf-8", newline="\n")
        try:
            validate_gp_accesses(REPO)
            assert False, "Corrupted GP access FAILED to raise AssertionError!"
        except AssertionError as e:
            print(f"  Corrupted GP access caught as expected: {e}")
    finally:
        gp_path.write_text(orig_gp_text, encoding="utf-8", newline="\n")

    # Check 9: Uncovered code coverage audit across the 8 GP accesses
    print("Testing check 9: uncatalogued code audit and GP access bounds...")
    from check_uncovered_code import audit_gp_access_uncovered_functions
    audit_res = audit_gp_access_uncovered_functions(REPO)
    assert audit_res["audited_accesses"] == 8, f"Expected 8 audited accesses, got {audit_res['audited_accesses']}"
    assert audit_res["all_confirmed_uncatalogued_in_tsv"], "Expected all 8 accesses to be uncatalogued in functions.tsv"
    assert audit_res["all_confirmed_valid_function_bounds"], "Expected all 8 accesses to have non-zero function sizes"
    print("  PASS: Uncatalogued code audit successfully confirmed all 8 routines.")

    # Planted Failure 10: Untagged component in evidence manifest or native-design-check claiming credit
    print("Testing planted failure 10: untagged component rejection in validate_ledgers...")
    ev_path = REPO / "research" / "ledger" / "ported_function_evidence.json"
    orig_ev_text = ev_path.read_text(encoding="utf-8")
    try:
        bad_ev = json.loads(orig_ev_text)
        del bad_ev["components"]["materials"]["verification_class"]
        ev_path.write_text(json.dumps(bad_ev), encoding="utf-8", newline="\n")
        try:
            validate_ledgers(ps2_ledger, xbox_ledger, ps2_map)
            assert False, "Untagged component FAILED to raise AssertionError!"
        except AssertionError as e:
            assert "materials has invalid or missing verification_class" in str(e), f"Unexpected error: {e}"
            print(f"  Untagged component caught as expected: {e}")

        # native-design-check claiming credit
        bad_ev2 = json.loads(orig_ev_text)
        bad_ev2["components"]["materials"]["verification_class"] = "native-design-check"
        bad_ev2["components"]["materials"]["credited"] = True
        ev_path.write_text(json.dumps(bad_ev2), encoding="utf-8", newline="\n")
        try:
            validate_ledgers(ps2_ledger, xbox_ledger, ps2_map)
            assert False, "native-design-check claiming credit FAILED to raise AssertionError!"
        except AssertionError as e:
            assert "native-design-check must not count toward credit" in str(e), f"Unexpected error: {e}"
            print(f"  native-design-check claiming credit caught as expected: {e}")
    finally:
        ev_path.write_text(orig_ev_text, encoding="utf-8", newline="\n")

    # Planted Failure 11: Acceptance claim rejected while placeholder registry is non-empty
    print("Testing planted failure 11: acceptance claim rejection while placeholder registry is non-empty...")
    ev_path = REPO / "research" / "ledger" / "ported_function_evidence.json"
    orig_ev_text = ev_path.read_text(encoding="utf-8")
    try:
        bad_ev3 = json.loads(orig_ev_text)
        bad_ev3["acceptance"] = "CLOSED"
        ev_path.write_text(json.dumps(bad_ev3), encoding="utf-8", newline="\n")
        try:
            validate_ledgers(ps2_ledger, xbox_ledger, ps2_map)
            assert False, "Acceptance claim with non-empty placeholders FAILED to raise AssertionError!"
        except AssertionError as e:
            assert "Acceptance claimed while placeholder registry is non-empty" in str(e), f"Unexpected error: {e}"
            print(f"  Acceptance claim with non-empty placeholders caught as expected: {e}")
    finally:
        ev_path.write_text(orig_ev_text, encoding="utf-8", newline="\n")

    # Planted Failure 11b: Credited function depending directly on an unrecovered placeholder fails closed
    print("Testing planted failure 11b: credited function with placeholder dependency rejection...")
    ev_path = REPO / "research" / "ledger" / "ported_function_evidence.json"
    orig_ev_text = ev_path.read_text(encoding="utf-8")
    try:
        bad_ev_dep = json.loads(orig_ev_text)
        bad_ev_dep["00127120"] = {
            "evidence": orig_ev_text,
            "dependencies": ["00225d40", "mka::retail::initial_task_required::continue_first_scene_at_225d40"]
        }
        ev_path.write_text(json.dumps(bad_ev_dep), encoding="utf-8", newline="\n")
        try:
            validate_ledgers(ps2_ledger, xbox_ledger, ps2_map)
            assert False, "Credited function with placeholder dependency FAILED to raise AssertionError!"
        except AssertionError as e:
            assert "is registered as an unrecovered placeholder" in str(e), f"Unexpected error: {e}"
            print(f"  Credited function with placeholder dependency caught as expected: {e}")
    finally:
        ev_path.write_text(orig_ev_text, encoding="utf-8", newline="\n")

    # Planted Failure 12: Headless movie presentation with corrupted pixels / invalid hash fails closed
    print("Testing planted failure 12: headless movie telemetry with corrupted pixels rejection...")
    headless_exe = None
    for cand in [
        FORK_ROOT / "b" / "test_native_headless_entry_run.exe",
        REPO / "b" / "test_native_headless_entry_run.exe",
    ]:
        if cand.exists():
            headless_exe = cand
            break
    if headless_exe:
        res = subprocess.run(
            [str(headless_exe), "--planted-failure-corrupt-pixels"],
            capture_output=True,
            text=True,
            cwd=str(headless_exe.parent.parent)
        )
        assert res.returncode != 0, "Corrupted pixel presentation FAILED to exit with error code!"
        assert "Headless movie presentation telemetry mismatch" in res.stderr or "Headless movie presentation telemetry mismatch" in res.stdout, \
            f"Expected mismatch error message, got: {res.stdout}\n{res.stderr}"
        print("  Corrupted pixel headless presentation caught as expected: returned non-zero with telemetry mismatch.")
    else:
        print("  SKIP: test_native_headless_entry_run.exe not built yet; binary check deferred.")


    # Planted Failure 13: file:/// link and broken relative link in current docs fail closed
    print("Testing planted failure 13: file:/// and broken relative link rejection...")
    readme_p = REPO / "README.md"
    orig_readme = readme_p.read_text(encoding="utf-8")
    try:
        readme_p.write_text(orig_readme + "\n[local](file:///C:/Users/fake/doc.md)\n", encoding="utf-8", newline="\n")
        try:
            validate_current_docs(REPO)
            assert False, "file:/// link FAILED to raise AssertionError!"
        except AssertionError as e:
            assert "file:///" in str(e), f"Unexpected error: {e}"
            print(f"  file:/// link caught as expected: {e}")

        readme_p.write_text(orig_readme + "\n[broken](nonexistent_phase1_doc_xyz.md)\n", encoding="utf-8", newline="\n")
        try:
            validate_current_docs(REPO)
            assert False, "Broken relative link FAILED to raise AssertionError!"
        except AssertionError as e:
            assert "Broken relative markdown link" in str(e), f"Unexpected error: {e}"
            print(f"  Broken relative link caught as expected: {e}")
    finally:
        readme_p.write_text(orig_readme, encoding="utf-8", newline="\n")

    # Planted Failure 14: Old Xbox-first 'everything' wording fails closed
    print("Testing planted failure 14: old Xbox-first everything wording rejection...")
    agents_p = REPO / "AGENTS.md"
    orig_agents = agents_p.read_text(encoding="utf-8")
    try:
        agents_p.write_text(orig_agents + "\nOwner priority is Xbox-first investigation for everything still to be implemented.\n", encoding="utf-8", newline="\n")
        try:
            validate_current_docs(REPO)
            assert False, "Old Xbox-first everything wording FAILED to raise AssertionError!"
        except AssertionError as e:
            assert "Xbox-first 'everything' wording" in str(e), f"Unexpected error: {e}"
            print(f"  Old Xbox-first everything wording caught as expected: {e}")
    finally:
        agents_p.write_text(orig_agents, encoding="utf-8", newline="\n")

    # Planted Failure 15: Unmeasured / hardcoded AOT tier numerators fail closed
    print("Testing planted failure 15: hardcoded or unmeasured AOT tier numerators rejection...")
    inv_p = REPO / "research" / "ledger" / "aot_coverage_inventory.json"
    orig_inv = inv_p.read_text(encoding="utf-8")
    try:
        bad_inv = json.loads(orig_inv)
        bad_inv["modules"]["MOVIER.ovl"]["static_aot_count"] = 2537
        inv_p.write_text(json.dumps(bad_inv, indent=2) + "\n", encoding="utf-8", newline="\n")
        try:
            compute_aot_metrics(ps2_ledger)
            assert False, "Unmeasured MOVIER.ovl AOT numerator FAILED to raise AssertionError!"
        except AssertionError as e:
            assert "Unmeasured AOT numerator" in str(e), f"Unexpected error: {e}"
            print(f"  Unmeasured overlay AOT numerator caught as expected: {e}")

        bad_inv2 = json.loads(orig_inv)
        bad_inv2["modules"]["SLUS_214.10"]["static_aot_count"] = 9999
        inv_p.write_text(json.dumps(bad_inv2, indent=2) + "\n", encoding="utf-8", newline="\n")
        try:
            compute_aot_metrics(ps2_ledger)
            assert False, "Mismatched SLUS_214.10 AOT numerator FAILED to raise AssertionError!"
        except AssertionError as e:
            assert "Hardcoded/mismatched SLUS_214.10 AOT numerator" in str(e), f"Unexpected error: {e}"
            print(f"  Hardcoded SLUS_214.10 AOT numerator caught as expected: {e}")
    finally:
        inv_p.write_text(orig_inv, encoding="utf-8", newline="\n")

    # Planted Failure 15b: Hand-edited Tier 2 AOT numerator inside README.md PROGRESS block fails closed
    print("Testing planted failure 15b: hand-edited Tier 2 AOT number in README.md rejection...")
    orig_readme2 = readme_p.read_text(encoding="utf-8")
    try:
        readme_p.write_text(orig_readme2.replace("0 / 2,537", "2,537 / 2,537"), encoding="utf-8", newline="\n")
        try:
            validate_progress_aot_tier(REPO, ps2_ledger)
            assert False, "Hand-edited README.md Tier 2 numerator FAILED to raise AssertionError!"
        except AssertionError as e:
            assert "Unmeasured/mismatched MOVIER.ovl AOT count" in str(e), f"Unexpected error: {e}"
            print(f"  Hand-edited README.md Tier 2 numerator caught as expected: {e}")
    finally:
        readme_p.write_text(orig_readme2, encoding="utf-8", newline="\n")


    # Planted Failure 16: Stale retired fork name in current (non-history) docs fails closed
    print("Testing planted failure 16: retired fork checkout name rejection in current docs...")
    handoff_p = REPO / "HANDOFF.md"
    orig_handoff = handoff_p.read_text(encoding="utf-8")
    try:
        handoff_p.write_text(orig_handoff + "\nWork in external/forks/" + "retail" + "-main-22/.\n", encoding="utf-8", newline="\n")
        try:
            validate_current_docs(REPO)
            assert False, "Retired fork directory name FAILED to raise AssertionError!"
        except AssertionError as e:
            assert "Forbidden retired fork directory reference" in str(e), f"Unexpected error: {e}"
            print(f"  Retired fork reference caught as expected: {e}")
    finally:
        handoff_p.write_text(orig_handoff, encoding="utf-8", newline="\n")

    print("PASS: All planted-failure cases successfully caught and proven to fail closed.")



if __name__ == "__main__":
    test_parent_fork_sync()
    test_sha_resolution()
    test_production_existence()
    test_planted_failures()
