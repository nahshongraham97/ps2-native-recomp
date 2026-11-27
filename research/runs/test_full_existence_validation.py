import json
import re
from pathlib import Path
import copy
import sys
import subprocess
import hashlib
import ast
import tempfile

_this_file = Path(__file__).resolve()
if (_this_file.parents[2] / "external" / "forks" / "ps2-native-recomp").exists():
    REPO = _this_file.parents[2]
    FORK_ROOT = REPO / "external" / "forks" / "ps2-native-recomp"
    PARENT_PRESENT = True
elif any((p / "external/forks/ps2-native-recomp").resolve() == _this_file.parents[2] for p in _this_file.parents):
    FORK_ROOT = _this_file.parents[2]
    REPO = next(p for p in _this_file.parents if (p / "external/forks/ps2-native-recomp").resolve() == FORK_ROOT)
    PARENT_PRESENT = True
else:
    FORK_ROOT = _this_file.parents[2]
    REPO = FORK_ROOT
    PARENT_PRESENT = False

# Import validate_ledgers logic and sync_verify
if PARENT_PRESENT:
    sys.path.insert(0, str(REPO / "tools" / "research"))
sys.path.insert(0, str(FORK_ROOT / "tools" / "research"))
from generate_ledgers import validate_ledgers, validate_current_docs, validate_progress_aot_tier, validate_classifications, compute_aot_metrics, FORK_COMMIT_SHA
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
    except AssertionError as e:
        print(f"  Planted failure 1 caught as expected: {e}")
    else:
        raise AssertionError("Planted failure 1 FAILED to raise AssertionError!")

    # Planted Failure 1b: Signature itself claiming 'exact' while cfg_topology_match is false
    print("Testing planted failure 1b: signature claiming exact with cfg_topology_match false...")
    sig_p = REPO / "research/cross_platform/signatures/signature_0012a720_0023d4d0.json"
    orig_sig_text = sig_p.read_text(encoding="utf-8")
    try:
        sig_obj = json.loads(orig_sig_text)
        sig_obj["match_confidence"] = "exact"
        sig_p.write_text(json.dumps(sig_obj), encoding="utf-8", newline="\n")
        validate_ledgers(ps2_ledger, xbox_ledger, bad_map1)
    except AssertionError as e:
        print(f"  Planted failure 1b caught as expected: {e}")
    else:
        raise AssertionError("Planted failure 1b FAILED to raise AssertionError!")
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
    except AssertionError as e:
        print(f"  Planted failure 2 caught as expected: {e}")
    else:
        raise AssertionError("Planted failure 2 FAILED to raise AssertionError!")

    # Planted Failure 3: Foreign address in assertion text
    print("Testing planted failure 3: foreign ported address injected into assertion text...")
    bad_ps2_ledger = copy.deepcopy(ps2_ledger)
    for r in bad_ps2_ledger:
        if r["address"] == "0012e4e0":
            r["evidence_path"] = r["evidence_path"].replace("12E4E0", "12E670")
            break
    try:
        validate_ledgers(bad_ps2_ledger, xbox_ledger, ps2_map)
    except AssertionError as e:
        print(f"  Planted failure 3 caught as expected: {e}")
    else:
        raise AssertionError("Planted failure 3 FAILED to raise AssertionError!")

    # Planted Failure 4: Crediting PS2 function as implemented-via-donor without test vectors
    print("Testing planted failure 4: crediting PS2 function as implemented-via-donor without test vectors...")
    bad_ps2_ledger4 = copy.deepcopy(ps2_ledger)
    for r in bad_ps2_ledger4:
        if r["address"] == "0012a720":
            r["status"] = "implemented-via-donor"
            break
    try:
        validate_ledgers(bad_ps2_ledger4, xbox_ledger, ps2_map)
    except AssertionError as e:
        assert "Donor credit rejected" in str(e), f"Expected donor credit rejection, got: {e}"
        assert "vector_0012a720.json does not exist" in str(e), f"Expected missing vector file, got: {e}"
        print(f"  Planted failure 4 caught as expected: {e}")
    else:
        raise AssertionError("Planted failure 4 FAILED to raise AssertionError!")

    # Planted Failure 4b: Crediting Xbox function as implemented-via-donor without test vectors
    print("Testing planted failure 4b: crediting Xbox function as implemented-via-donor without test vectors...")
    bad_xb_ledger4 = copy.deepcopy(xbox_ledger)
    for r in bad_xb_ledger4:
        if r["address"] == "0023d4d0":
            r["status"] = "implemented-via-donor"
            break
    try:
        validate_ledgers(ps2_ledger, bad_xb_ledger4, ps2_map)
    except AssertionError as e:
        assert "Donor credit rejected" in str(e), f"Expected donor credit rejection, got: {e}"
        assert "vector_0023d4d0.json does not exist" in str(e), f"Expected missing vector file, got: {e}"
        print(f"  Planted failure 4b caught as expected: {e}")
    else:
        raise AssertionError("Planted failure 4b FAILED to raise AssertionError!")

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
        except AssertionError as e:
            print(f"  Empty vector caught as expected: {e}")
        else:
            raise AssertionError("Empty test vector file FAILED to raise AssertionError!")

        temp_placeholder.write_text("{}", encoding="utf-8")
        try:
            validate_vector_file(temp_placeholder)
        except AssertionError as e:
            print(f"  Placeholder dict caught as expected: {e}")
        else:
            raise AssertionError("Placeholder dict test vector file FAILED to raise AssertionError!")
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
        except AssertionError as e:
            print(f"  Invalid oracle_trace_hash caught as expected: {e}")
        else:
            raise AssertionError("Invalid oracle_trace_hash FAILED to raise AssertionError!")

        dummy_vec["oracle_trace_hash"] = "a" * 64
        dummy_vec["oracle_source"] = ""
        temp_bad_vec.write_text(json.dumps(dummy_vec), encoding="utf-8")
        try:
            validate_vector_file(temp_bad_vec, execute_native=False)
        except AssertionError as e:
            print(f"  Empty oracle_source caught as expected: {e}")
        else:
            raise AssertionError("Empty oracle_source FAILED to raise AssertionError!")

        # Missing trace file test
        dummy_vec["oracle_source"] = "nonexistent_trace_file.bin"
        temp_bad_vec.write_text(json.dumps(dummy_vec), encoding="utf-8")
        try:
            validate_vector_file(temp_bad_vec, execute_native=False)
        except AssertionError as e:
            print(f"  Missing trace file caught as expected: {e}")
        else:
            raise AssertionError("Missing trace file FAILED to raise AssertionError!")

        # Stored trace hash mismatch test
        dummy_vec["oracle_source"] = "retail_SLUS_214.10_mips_trace.bin"
        dummy_vec["oracle_trace_hash"] = "b" * 64
        temp_bad_vec.write_text(json.dumps(dummy_vec), encoding="utf-8")
        try:
            validate_vector_file(temp_bad_vec, execute_native=False)
        except AssertionError as e:
            print(f"  Stored trace hash mismatch caught as expected: {e}")
        else:
            raise AssertionError("Stored trace hash mismatch FAILED to raise AssertionError!")

        # Correct hash with native execution mismatch (Python stand-in labeled as infrastructure testing)
        dummy_vec["oracle_trace_hash"] = real_trace_hash
        register_native_dispatch("SoundCatalog_ValidateIndex", lambda x: 0, implementation_kind="infrastructure_testing_python")
        temp_bad_vec.write_text(json.dumps(dummy_vec), encoding="utf-8")
        try:
            validate_vector_file(temp_bad_vec, execute_native=True)
        except AssertionError as e:
            print(f"  Native output mismatch caught as expected: {e}")
        else:
            raise AssertionError("Native output mismatch FAILED to raise AssertionError!")

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
        except AssertionError as e:
            print(f"  Corrupted GP access caught as expected: {e}")
        else:
            raise AssertionError("Corrupted GP access FAILED to raise AssertionError!")
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
        except AssertionError as e:
            assert "materials has invalid or missing verification_class" in str(e), f"Unexpected error: {e}"
            print(f"  Untagged component caught as expected: {e}")
        else:
            raise AssertionError("Untagged component FAILED to raise AssertionError!")

        # native-design-check claiming credit
        bad_ev2 = json.loads(orig_ev_text)
        bad_ev2["components"]["materials"]["verification_class"] = "native-design-check"
        bad_ev2["components"]["materials"]["credited"] = True
        ev_path.write_text(json.dumps(bad_ev2), encoding="utf-8", newline="\n")
        try:
            validate_ledgers(ps2_ledger, xbox_ledger, ps2_map)
        except AssertionError as e:
            assert "native-design-check must not count toward credit" in str(e), f"Unexpected error: {e}"
            print(f"  native-design-check claiming credit caught as expected: {e}")
        else:
            raise AssertionError("native-design-check claiming credit FAILED to raise AssertionError!")
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
        except AssertionError as e:
            assert "Acceptance claimed while placeholder registry is non-empty" in str(e), f"Unexpected error: {e}"
            print(f"  Acceptance claim with non-empty placeholders caught as expected: {e}")
        else:
            raise AssertionError("Acceptance claim with non-empty placeholders FAILED to raise AssertionError!")
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
        except AssertionError as e:
            assert "is registered as an unrecovered placeholder" in str(e), f"Unexpected error: {e}"
            print(f"  Credited function with placeholder dependency caught as expected: {e}")
        else:
            raise AssertionError("Credited function with placeholder dependency FAILED to raise AssertionError!")
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
        except AssertionError as e:
            assert "file:///" in str(e), f"Unexpected error: {e}"
            print(f"  file:/// link caught as expected: {e}")
        else:
            raise AssertionError("file:/// link FAILED to raise AssertionError!")

        readme_p.write_text(orig_readme + "\n[broken](nonexistent_phase1_doc_xyz.md)\n", encoding="utf-8", newline="\n")
        try:
            validate_current_docs(REPO)
        except AssertionError as e:
            assert "Broken relative markdown link" in str(e), f"Unexpected error: {e}"
            print(f"  Broken relative link caught as expected: {e}")
        else:
            raise AssertionError("Broken relative link FAILED to raise AssertionError!")
    finally:
        readme_p.write_text(orig_readme, encoding="utf-8", newline="\n")

    print("Testing planted failure 13b: fork links cannot escape to other repositories...")
    scratch = FORK_ROOT / "build/oracle-telemetry"
    scratch.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="fork-doc-links-", dir=scratch) as fixture_dir:
        fixture = Path(fixture_dir)
        fork = fixture / "fork"
        fork.mkdir()
        (fixture / "outside.md").write_text("outside repository\n", encoding="utf-8", newline="\n")
        (fork / "inside.md").write_text("inside repository\n", encoding="utf-8", newline="\n")
        readme = fork / "README.md"
        # Existing targets are deliberate: existence must not admit an escape.
        bad_links = [
            ("relative escape", "../outside.md", "Cross-repository relative"),
            ("encoded relative escape", "%2e%2e/outside.md", "Cross-repository relative"),
            ("backslash escape", "..\\outside.md", "Cross-repository relative"),
            ("Windows absolute", "C:/outside.md", "Absolute filesystem"),
            ("Windows drive-relative", "C:outside.md", "Absolute filesystem"),
            ("UNC", "\\\\server\\share\\outside.md", "Absolute filesystem"),
            ("POSIX absolute", "/outside.md", "Absolute filesystem"),
            ("parent HTTPS", "https://github.com/nahshongraham97/armageddon-reimplementation/blob/main/README.md", "Cross-repository or external"),
            ("upstream HTTPS", "https://example.org/reference", "Cross-repository or external"),
            ("same-fork missing file", "https://github.com/nahshongraham97/ps2-native-recomp/blob/main/missing.md", "Missing or escaped same-fork"),
            ("same-fork encoded escape", "https://github.com/nahshongraham97/ps2-native-recomp/blob/main/%2e%2e/outside.md", "Missing or escaped same-fork"),
            ("same-fork file as tree", "https://github.com/nahshongraham97/ps2-native-recomp/tree/main/inside.md", "not a directory"),
            ("same-fork unknown commit", "https://github.com/nahshongraham97/ps2-native-recomp/commit/" + "0" * 40, "Unknown same-fork commit"),
        ]
        bad_documents = [(label, f"[planted]({target})", expected) for label, target, expected in bad_links]
        bad_documents.extend([
            ("reference definition", "[source]: ../outside.md", "Cross-repository relative"),
            ("external autolink", "<https://example.org/outside>", "Cross-repository or external"),
        ])
        for label, document, expected in bad_documents:
            readme.write_text(document + "\n", encoding="utf-8", newline="\n")
            try:
                validate_current_docs(fork, fork_only=True)
            except AssertionError as error:
                assert expected in str(error), f"{label}: unexpected rejection: {error}"
            else:
                raise AssertionError(f"{label}: fork-only link check admitted planted failure")
        for target in ("inside.md", "inside.md#fragment", "#fragment",
                       "https://github.com/nahshongraham97/ps2-native-recomp/blob/main/inside.md"):
            readme.write_text(f"[positive]({target})\n", encoding="utf-8", newline="\n")
            validate_current_docs(fork, fork_only=True)
        print(f"  Rejected all {len(bad_documents)} cross-repository/absolute/missing-file cases; four in-fork positives passed.")

    # Planted Failure 14: Old Xbox-first 'everything' wording fails closed
    print("Testing planted failure 14: old Xbox-first everything wording rejection...")
    agents_p = REPO / "AGENTS.md"
    orig_agents = agents_p.read_text(encoding="utf-8")
    try:
        agents_p.write_text(orig_agents + "\nOwner priority is Xbox-first investigation for everything still to be implemented.\n", encoding="utf-8", newline="\n")
        try:
            validate_current_docs(REPO)
        except AssertionError as e:
            assert "Xbox-first 'everything' wording" in str(e), f"Unexpected error: {e}"
            print(f"  Old Xbox-first everything wording caught as expected: {e}")
        else:
            raise AssertionError("Old Xbox-first everything wording FAILED to raise AssertionError!")
    finally:
        agents_p.write_text(orig_agents, encoding="utf-8", newline="\n")

    # Planted Failure 15: Unmeasured / hardcoded AOT tier numerators fail closed
    print("Testing planted failure 15: hardcoded or unmeasured AOT tier numerators rejection...")
    inv_p = REPO / "research" / "ledger" / "aot_coverage_inventory.json"
    orig_inv = inv_p.read_text(encoding="utf-8")
    try:
        bad_inv = json.loads(orig_inv)
        bad_inv["modules"]["MOVIER.ovl"]["static_aot_count"] = 2537
        bad_inv["modules"]["MOVIER.ovl"]["static_overlay_sources"] = []
        inv_p.write_text(json.dumps(bad_inv, indent=2) + "\n", encoding="utf-8", newline="\n")
        try:
            compute_aot_metrics(ps2_ledger)
        except AssertionError as e:
            assert "Unmeasured AOT numerator" in str(e), f"Unexpected error: {e}"
            print(f"  Unmeasured overlay AOT numerator caught as expected: {e}")
        else:
            raise AssertionError("Unmeasured MOVIER.ovl AOT numerator FAILED to raise AssertionError!")

        bad_inv2 = json.loads(orig_inv)
        bad_inv2["modules"]["SLUS_214.10"]["static_aot_count"] = 9999
        inv_p.write_text(json.dumps(bad_inv2, indent=2) + "\n", encoding="utf-8", newline="\n")
        try:
            compute_aot_metrics(ps2_ledger)
        except AssertionError as e:
            assert "Hardcoded/mismatched SLUS_214.10 AOT numerator" in str(e), f"Unexpected error: {e}"
            print(f"  Hardcoded SLUS_214.10 AOT numerator caught as expected: {e}")
        else:
            raise AssertionError("Mismatched SLUS_214.10 AOT numerator FAILED to raise AssertionError!")
    finally:
        inv_p.write_text(orig_inv, encoding="utf-8", newline="\n")

    # Planted Failure 15b: Hand-edited Tier 2 AOT numerator inside README.md PROGRESS block fails closed
    print("Testing planted failure 15b: hand-edited Tier 2 AOT number in README.md rejection...")
    orig_readme2 = readme_p.read_text(encoding="utf-8")
    try:
        readme_p.write_text(orig_readme2.replace("2,537 / 2,537", "0 / 2,537"), encoding="utf-8", newline="\n")
        try:
            validate_progress_aot_tier(REPO, ps2_ledger)
        except AssertionError as e:
            assert "missing '2,537 / 2,537'" in str(e), f"Unexpected error: {e}"
            print(f"  Hand-edited README.md Tier 2 numerator caught as expected: {e}")
        else:
            raise AssertionError("Hand-edited README.md Tier 2 numerator FAILED to raise AssertionError!")
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
        except AssertionError as e:
            assert "Forbidden retired fork directory reference" in str(e), f"Unexpected error: {e}"
            print(f"  Retired fork reference caught as expected: {e}")
        else:
            raise AssertionError("Retired fork directory name FAILED to raise AssertionError!")
    finally:
        handoff_p.write_text(orig_handoff, encoding="utf-8", newline="\n")


    # Planted Failure 17: Multi-tier hand-edited numbers / Xbox % / mismatched classification sums fail closed
    print("Testing planted failure 17: multi-tier hand-edit, Xbox %, and classification sum rejection...")
    orig_readme_p3 = readme_p.read_text(encoding="utf-8")
    try:
        measured_transform_token = f"**{compute_aot_metrics(ps2_ledger)['transformed_count']:,} / 15,670**"
        assert measured_transform_token in orig_readme_p3
        readme_p.write_text(orig_readme_p3.replace(measured_transform_token, "**500 / 15,670**"), encoding="utf-8", newline="\n")
        try:
            validate_progress_aot_tier(REPO, ps2_ledger)
        except AssertionError as e:
            assert "Unmeasured/mismatched multi-tier token" in str(e), f"Unexpected error: {e}"
            print(f"  Hand-edited Tier 3 count caught as expected: {e}")
        else:
            raise AssertionError("Hand-edited Tier 3 transformed count FAILED to raise AssertionError!")

        bad_xb = orig_readme_p3.replace("22,579** `default.xbe` routines inspected", "22,579** `default.xbe` routines inspected (0.00%)")
        readme_p.write_text(bad_xb, encoding="utf-8", newline="\n")
        try:
            validate_progress_aot_tier(REPO, ps2_ledger)
        except AssertionError as e:
            assert "Forbidden percentage metric in Xbox Reference" in str(e), f"Unexpected error: {e}"
            print(f"  Xbox Reference percentage caught as expected: {e}")
        else:
            raise AssertionError("Percentage on Xbox Reference line FAILED to raise AssertionError!")
    finally:
        readme_p.write_text(orig_readme_p3, encoding="utf-8", newline="\n")

    cls_p = REPO / "research" / "ledger" / "function_classification_inventory.json"
    orig_cls = cls_p.read_text(encoding="utf-8")
    try:
        bad_cls = json.loads(orig_cls)
        bad_cls["catalog_3way_partition"]["platform-boundary"] += 5
        cls_p.write_text(json.dumps(bad_cls, indent=2) + "\n", encoding="utf-8", newline="\n")
        try:
            validate_classifications(REPO, ps2_ledger)
        except AssertionError as e:
            assert "Classification catalog sum" in str(e), f"Unexpected error: {e}"
            print(f"  Mismatched classification partition sum caught as expected: {e}")
        else:
            raise AssertionError("Mismatched classification sum FAILED to raise AssertionError!")
    finally:
        cls_p.write_text(orig_cls, encoding="utf-8", newline="\n")

    # A function start inside a unit is not covered when its entry-map slot is
    # zero. Plant a plausible range-based claim with a matching edited count.
    print("Testing planted failure 18: range-only static AOT coverage rejection...")
    original_inventory = inv_p.read_bytes()
    gamer_source = FORK_ROOT / "build/generated-aot/phase4-all/m_GAMER_unit_0000.cpp"
    original_source = gamer_source.read_bytes()
    try:
        bad = json.loads(original_inventory)
        text = original_source.decode("utf-8")
        array = re.search(r"const std::uint16_t m_GAMER_entries_0000\[4096\] = \{(.*?)\};", text, re.S)
        assert array
        first = next(m for m in re.finditer(r"\b\d+\b", array[1]) if int(m[0]) != 0)
        offset = array.start(1) + first.start()
        changed = (text[:offset] + "0" + text[offset + len(first[0]):]).encode("utf-8")
        gamer_source.write_bytes(changed)
        # Authenticate the planted source bytes too: hash checking alone must
        # not let unchanged unit ranges or stale callable counts pass.
        bad["entry_measurement"]["source_sha256"][gamer_source.name] = hashlib.sha256(changed).hexdigest()
        inv_p.write_text(json.dumps(bad, indent=2) + "\n", encoding="utf-8", newline="\n")
        try:
            compute_aot_metrics(ps2_ledger)
        except AssertionError as exc:
            assert "Static AOT callable entry mismatch" in str(exc), f"Unexpected error: {exc}"
            print("  Zero dispatch entry rejected despite unit-range membership and a matching edited numerator.")
        else:
            raise AssertionError("Range-only static AOT claim was accepted")
    finally:
        gamer_source.write_bytes(original_source)
        inv_p.write_bytes(original_inventory)

    print("Testing planted failure 19: runner failure propagation and resolved output confinement...")
    runner = ast.parse((FORK_ROOT / "tools/research/run_check.py").read_text(encoding="utf-8"))
    guards = [node for node in runner.body if isinstance(node, ast.FunctionDef)
              and node.name in ("checked_output_path", "require_check_success")]
    assert len(guards) == 2, "Runner must expose its real result/path guards"
    namespace = {"Path": Path}
    exec(compile(ast.Module(body=guards, type_ignores=[]), "runner-guards", "exec"), namespace)
    for result in [(17, False, False), (0, True, False), (0, False, True)]:
        try:
            namespace["require_check_success"](*result)
        except SystemExit as exc:
            assert exc.code == 1
        else:
            raise AssertionError(f"Runner accepted failed/timed-out/skipped check: {result}")
    namespace["require_check_success"](0, False, False)
    try:
        namespace["checked_output_path"](FORK_ROOT / ".." / "_must_not_be_created_", FORK_ROOT)
    except ValueError:
        pass
    else:
        raise AssertionError("Runner accepted a lexical-prefix path escaping through '..'")
    assert namespace["checked_output_path"](FORK_ROOT / "b", FORK_ROOT) == (FORK_ROOT / "b").resolve()
    print("  Failed links, timeouts, skipped checks and escaped output paths rejected by the actual runner guards.")

    print("Testing planted failure 20: provenance-bound VU input admission before staging...")
    # Execute the actual pure admission/staging helpers without importing a
    # potentially stale fork copy or invoking their CLI/generator entrypoint.
    inventory_module = ast.parse((REPO / "tools/research/aot_inventory.py").read_text(encoding="utf-8"))
    helpers = [node for node in inventory_module.body if isinstance(node, ast.FunctionDef)
               and node.name in ("sha", "classify_vu_inputs", "stage_vu_inputs")]
    assert len(helpers) == 3
    vu_namespace = {"Path": Path, "json": json, "hashlib": hashlib, "re": re}
    exec(compile(ast.Module(body=helpers, type_ignores=[]), "vu-admission-helpers", "exec"), vu_namespace)
    classify = vu_namespace["classify_vu_inputs"]
    good_name = "0123456789ABCDEF_0000.bin"
    rejected_name = "883AC7731B3857A8_0000.bin"
    unknown_name = "FEDCBA9876543210_0000.bin"
    # Even identical, decodable/hash-key-plausible bytes do not confer source
    # provenance. This is a synthetic instruction-shaped fixture, not an
    # independently executed VU program or a replacement original capture.
    raw = bytes.fromhex("00000410ff020000")
    identity = {"bytes": len(raw), "sha256": hashlib.sha256(raw).hexdigest()}
    legacy = {good_name: dict(identity)}
    rejected = {rejected_name: {**identity, "reason": "known wrong source extent"}}
    actual = {good_name: dict(identity), rejected_name: dict(identity)}
    admitted, excluded = classify(actual, legacy, rejected)
    assert admitted == legacy and excluded == {rejected_name: identity}

    def require_vu_rejection(operation, message):
        try:
            operation()
        except AssertionError as exc:
            assert message in str(exc), f"Unexpected VU admission failure: {exc}"
        else:
            raise AssertionError(f"VU admission accepted planted failure: {message}")

    require_vu_rejection(lambda: classify({**actual, unknown_name: identity}, legacy, rejected),
                         "Unknown/missing VU capture inputs")
    for field, value in (("bytes", len(raw) + 1), ("sha256", "0" * 64)):
        changed = copy.deepcopy(actual)
        changed[good_name][field] = value
        require_vu_rejection(lambda: classify(changed, legacy, rejected), "source hash/extent mismatch")
    require_vu_rejection(lambda: classify(actual, legacy, {**rejected, good_name: identity}),
                         "both admitted and rejected")

    temp_root = (FORK_ROOT / "b/check-temp").resolve()
    assert temp_root.is_relative_to(FORK_ROOT.resolve()) and temp_root != FORK_ROOT.resolve()
    temp_root.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="vu-admission-", dir=temp_root) as temporary:
        sandbox = Path(temporary).resolve()
        assert sandbox.is_relative_to(temp_root) and sandbox != temp_root
        try:
            repo = sandbox / "repo"
            fork = sandbox / "fork"
            fork.mkdir()
            ledger = repo / "research/ledger"
            ledger.mkdir(parents=True)
            originals = repo / "private-inputs/mkarmageddon-aot-inputs/vu1"
            originals.mkdir(parents=True)
            for name in actual:
                (originals / name).write_bytes(raw)
            receipt = ledger / "synthetic-legacy.json"
            receipt.write_text(json.dumps({"captures": {good_name: identity}}), encoding="utf-8")
            frozen_receipt = receipt.read_bytes()
            policy = {"sources": {"vu_corpus_strict": {
                "rel_path": "research/ledger/synthetic-legacy.json",
                "sha256": hashlib.sha256(frozen_receipt).hexdigest()}},
                "vu_microprograms": {"rejected_captures": rejected}}
            (ledger / "aot_coverage_inventory.json").write_text(json.dumps(policy), encoding="utf-8")
            stage = vu_namespace["stage_vu_inputs"]
            receipt.write_bytes(frozen_receipt + b" ")
            target = fork / "bad-receipt"
            require_vu_rejection(lambda: stage(repo, fork, target), "legacy VU capture receipt changed")
            assert not target.exists(), "Receipt rejection created staging output"
            receipt.write_bytes(frozen_receipt)
            (originals / unknown_name).write_bytes(raw)
            target = fork / "unknown-input"
            require_vu_rejection(lambda: stage(repo, fork, target), "Unknown/missing VU capture inputs")
            assert not target.exists(), "Input rejection created staging output"
            # Remove only this synthetic unknown fixture; originals are never
            # referenced by this sandbox and are never altered by the test.
            (originals / unknown_name).unlink()
            for label, altered_raw in (("extent", raw + b"\0"),
                                       ("hash", bytes([raw[0] ^ 1]) + raw[1:])):
                (originals / good_name).write_bytes(altered_raw)
                target = fork / ("altered-" + label)
                require_vu_rejection(lambda: stage(repo, fork, target), "source hash/extent mismatch")
                assert not target.exists(), "Altered input rejection created staging output"
                (originals / good_name).write_bytes(raw)
            overlap_policy = copy.deepcopy(policy)
            overlap_policy["vu_microprograms"]["rejected_captures"][good_name] = identity
            (ledger / "aot_coverage_inventory.json").write_text(json.dumps(overlap_policy), encoding="utf-8")
            target = fork / "overlap"
            require_vu_rejection(lambda: stage(repo, fork, target), "both admitted and rejected")
            assert not target.exists(), "Overlapping provenance rejection created staging output"
            (ledger / "aot_coverage_inventory.json").write_text(json.dumps(policy), encoding="utf-8")
            target = fork / "admitted"
            result = stage(repo, fork, target)
            assert set(path.name for path in target.iterdir()) == {good_name}
            assert (target / good_name).read_bytes() == raw
            assert result["excluded"] == {rejected_name: identity}
            assert result["original_inputs_modified"] is False
            assert {path.name: path.read_bytes() for path in originals.iterdir()} == {
                good_name: raw, rejected_name: raw}
        finally:
            # TemporaryDirectory recursively removes only this verified child.
            assert sandbox.resolve().is_relative_to(temp_root) and sandbox.resolve() != temp_root
    print("  Unknown/altered/overlapping captures rejected; frozen receipt checked before output; known mis-extraction excluded.")

    print("Testing planted failure 21: different valid commit pins must fail exact document sync...")
    if PARENT_PRESENT:
        readme = REPO / "README.md"
        original = readme.read_bytes()
        match = re.search(rb"/commit/([0-9a-f]{40})", original)
        assert match, "Missing generated fork source pin"
        pinned = match.group(1).decode("ascii")
        other = subprocess.check_output(["git", "rev-parse", pinned + "^"],
                                        cwd=FORK_ROOT, text=True).strip()
        # Both pins resolve. Only exact equality is acceptable; normalizing
        # different valid pins would conceal this planted mismatch.
        altered = original.replace(pinned.encode("ascii"), other.encode("ascii"))
        altered = altered.replace(pinned[:8].encode("ascii"), other[:8].encode("ascii"))
        assert altered != original
        try:
            readme.write_bytes(altered)
            result = sync_verify.verify_sync(FORK_ROOT, REPO)
            assert any("README.md: MISMATCH" in msg for msg in result["mismatches"]), result
        finally:
            readme.write_bytes(original)
        assert readme.read_bytes() == original
        print("  Different valid source pins rejected; original document bytes restored.")

    print("Testing planted failure 22: catalogue data exceptions cannot hide executable functions...")
    from aot_inventory import verified_nonfunction_catalogue_addresses, verify_entries
    ledger_rows = __import__("csv").DictReader((REPO / "research/ledger/functions.csv").open(encoding="utf-8"))
    ledger_rows = list(ledger_rows)
    inventory_path = REPO / "research/ledger/aot_coverage_inventory.json"
    original_inventory = inventory_path.read_bytes()
    inventory = json.loads(original_inventory)
    proved = verified_nonfunction_catalogue_addresses(REPO, ledger_rows)
    assert proved and not proved.intersection(int(address, 16) for address in
        inventory["modules"]["SLUS_214.10"]["callable_catalogue_entries"])
    for field, value in (("address", "002b68e0"), ("proof_id", "unreviewed-executable-exclusion"),
                         ("elf_sha256", "0" * 64), ("callback_address", "005bf218")):
        planted = copy.deepcopy(inventory)
        planted["nonfunction_catalogue_records"][0][field] = value
        try:
            inventory_path.write_text(json.dumps(planted) + "\n", encoding="utf-8", newline="\n")
            try:
                verified_nonfunction_catalogue_addresses(REPO, ledger_rows)
            except AssertionError:
                pass
            else:
                raise AssertionError("Catalogue data proof accepted planted " + field)
        finally:
            inventory_path.write_bytes(original_inventory)
    assert inventory_path.read_bytes() == original_inventory
    print("  Function masking, unknown proofs, altered original identity and self-callback rejected.")

    print("Testing planted failure 23: a dropped dynamic window must fail despite repinned table hash...")
    table_path = FORK_ROOT / "build/generated-aot/phase4-all/table.cpp"
    original_table = table_path.read_bytes()
    window_name = inventory["dynamic_overlay_windows_kModules"][0]["name"].encode("ascii")
    pattern = rb'(?m)^    \{"' + re.escape(window_name) + rb'",[^\r\n]*\r?\n'
    altered_table, replaced = re.subn(pattern, b"", original_table)
    assert replaced == 1 and altered_table != original_table
    forged = copy.deepcopy(inventory)
    forged["entry_measurement"]["table_sha256"] = hashlib.sha256(altered_table).hexdigest()
    try:
        table_path.write_bytes(altered_table)
        try:
            verify_entries(REPO, forged, ledger_rows)
        except AssertionError as exc:
            assert "Unregistered generated module" in str(exc) or "Dynamic window" in str(exc), str(exc)
        else:
            raise AssertionError("Repinning a table concealed a dropped dynamic window")
    finally:
        table_path.write_bytes(original_table)
    assert table_path.read_bytes() == original_table
    print("  Dropped module rejected independently of stored coverage/table hashes; exact bytes restored.")

    print("Testing planted failure 24: renamed guest-memory accessors fail the semantic native audit...")
    tx = inventory.get("memory_abi_transform", {})
    if not tx.get("functions"):
        print("  SKIP: no measured Phase 5 prototype exists yet; no native-transform pass claimed.")
    else:
        script = FORK_ROOT / "tools/research/transform_generated_units.py"
        native_dir = FORK_ROOT / "b/native-overlay-initializers"
        cache = (FORK_ROOT / "b/CMakeCache.txt").read_text(encoding="utf-8")
        compiler = next(line.split("=", 1)[1] for line in cache.splitlines()
                        if line.startswith("CMAKE_CXX_COMPILER:"))
        command = [sys.executable, "-B", str(script), "--compiler", compiler, "--audit-native"]
        good = subprocess.run(command + [str(native_dir)], capture_output=True, timeout=60)
        assert good.returncode == 0, (good.stdout + good.stderr).decode(errors="replace")[-2000:]
        with tempfile.TemporaryDirectory(dir=FORK_ROOT / "build/oracle-telemetry") as scratch:
            scratch = Path(scratch)
            (scratch / "native_overlay_initializers.hpp").write_bytes(
                (native_dir / "native_overlay_initializers.hpp").read_bytes())
            # Innocuous identifiers deliberately avoid a name blacklist. This
            # is still integer-addressed flat storage behind a renamed helper.
            planted_source = '''#include "native_overlay_initializers.hpp"
namespace mka::retail::generated {
static unsigned char records[33554432];
static std::uint32_t find_record(std::uint32_t key) noexcept {
    return reinterpret_cast<const std::uint32_t*>(records)[key >> 2];
}
std::uint64_t initialize_gamer(OverlaySelection& owner) noexcept {
    owner.selected_mode = find_record(0x64f1a0u); return owner.selected_mode;
}
std::uint64_t initialize_movier(OverlaySelection& owner) noexcept {
    owner.selected_mode = find_record(0x64f1a0u); return owner.selected_mode;
}
}
'''
            (scratch / "native_overlay_initializers.cpp").write_text(
                planted_source, encoding="utf-8", newline="\n")
            bad = subprocess.run(command + [str(scratch)], capture_output=True, timeout=60)
            assert bad.returncode != 0, "Renamed array/accessor concealed guest memory"
            refusal = (bad.stdout + bad.stderr).decode(errors="replace")
            assert "native_" in refusal.lower() or "audit" in refusal.lower(), refusal[-2000:]
        print("  Actual native output admitted; renamed array/address accessor refused structurally.")

    print("Testing planted failure 25: editable transform counts do not create measured functions...")
    from aot_inventory import verify_transformed_functions
    forged_count = copy.deepcopy(inventory)
    forged_count["memory_abi_transform"]["transformed_count"] += 1
    try:
        verify_transformed_functions(REPO, forged_count, ledger_rows)
    except AssertionError as error:
        assert "transformed function count" in str(error), str(error)
    else:
        raise AssertionError("Hand-entered transform credit was accepted")
    if tx.get("functions"):
        forged_proof = copy.deepcopy(inventory)
        forged_proof["memory_abi_transform"]["measurement"]["proof_sha256"] = "0" * 64
        try:
            verify_transformed_functions(REPO, forged_proof, ledger_rows)
        except AssertionError as error:
            assert "proof/measurement changed" in str(error), str(error)
        else:
            raise AssertionError("Unauthenticated transform proof was accepted")
    print("  Scalar-only credit and altered proof rejected without changing measured files.")

    test_bounded_census_failures(ps2_ledger, ps2_map)
    print("PASS: All planted-failure cases successfully caught and proven to fail closed.")


def test_bounded_census_failures(ledger, mappings):
    from bounded_ownership import anchor_words, census_exit_code, clusters, exact_verdict, game_class_provenance, named_member, recovery_report, resource_anchor, runtime_edge, validate_ledger_rows, verified_trace_edges
    from audit_subzero_retail_hierarchy_inputs import elf_segments, va_to_offset
    from generate_ledgers import bounded_progress, validate_bounded_progress, validate_prototype_levels, capability_map_rows, donor_comparison

    def rejects(operation, message):
        try:
            operation()
        except (AssertionError, ValueError) as error:
            assert message.lower() in str(error).lower(), str(error)
        else:
            raise AssertionError(f"Planted failure escaped: {message}")

    known = ["verified-game", "verified-renderware", "verified-other-library"]
    from measure_renderer_scope import GHIDRA_EXPORT_SHA256, verify_export_pin
    for name, expected_digest in GHIDRA_EXPORT_SHA256.items():
        verify_export_pin(name, expected_digest)
        rejects(lambda: verify_export_pin(name, "0" * 64), "Ghidra export identity changed")
    pin = next(line for line in (REPO / "README.md").read_text(encoding="utf-8").splitlines() if line.startswith("**Verified Fork Commit**:"))
    assert generate_ledgers.pin_refresh_only(["README.md"], "-" + pin + "\n+" + pin)
    assert not generate_ledgers.pin_refresh_only(["README.md"], "+" + pin + "\n+Other documentation")
    assert not generate_ledgers.pin_refresh_only(["runtime/app.cpp"], "+" + pin)
    assert census_exit_code([], True, exact_verdict(known)) == 0
    for incomplete in ([], [*known, "unresolved"], [*known, "inferred-renderware"]):
        assert census_exit_code([], True, exact_verdict(incomplete)) == 2
        assert census_exit_code([], False, exact_verdict(incomplete)) == 0
    assert census_exit_code(["missing"], True, True) == 1
    assert not exact_verdict(row["ownership_status"] for row in ledger if row["catalogue_kind"] == "callable")
    seed = next(row for row in ledger if row["module"] == "SLUS_214.10" and row["address"] == "0019f0b0")
    validate_ledger_rows([seed])
    rejects(lambda: validate_prototype_levels([dict(seed, transform_prototype_level="differential")], []), "Prototype differential")
    unrelated = next(row for row in ledger if row["module"] == "SLUS_214.10" and row["ownership_status"] == "unresolved")
    keys = ("catalogue_kind", "ownership_status", "ownership_evidence_kinds", "ownership_evidence_ref")
    values = lambda r: {k: r[k] for k in keys}
    expected_ownership = {r["module"] + ":" + r["address"]: values(r) for r in (seed, unrelated)}
    swapped = [dict(seed, **values(unrelated)), dict(unrelated, **values(seed))]
    assert bounded_progress([seed, unrelated]) == bounded_progress(swapped)
    rejects(lambda: validate_ledger_rows(swapped, expected_ownership), "differs from original witnesses")
    rejects(lambda: validate_ledger_rows([dict(seed, verification_level="differential")]), "native level without")
    rejects(lambda: validate_ledger_rows([dict(seed, verification_level="differential", native_verification_ref="fiction")]), "Unverified integrated receipt")
    rejects(lambda: validate_ledger_rows([dict(seed, ownership_evidence_kinds="E5")]), "Missing E1")
    rejects(lambda: validate_ledger_rows([dict(seed, ownership_status="verified-game")]), "Unnamed vtable")
    rejects(lambda: validate_ledger_rows([seed, seed]), "Duplicate")
    fixture = bytearray(200)
    fixture[12:23] = b"9MidwayFoo\0"
    for slot, value in ((36, 12), (60, 32), (68, 0x101000)):
        fixture[slot:slot+4] = value.to_bytes(4, "little")
    member = {"class_name": "MidwayFoo", "encoded_name": "9MidwayFoo", "name_va": "0000000c",
              "descriptor_va": "00000020", "name_pointer_slot": "00000024", "header_slot": "0000003c",
              "vtable_va": "00000040", "method_slot": "00000044", "method": "00101000",
              "game_owner_evidence_ref": "synthetic-only", "evidence_ref": "synthetic-only"}
    read = lambda slot, n: bytes(fixture[slot:slot+n])
    source = "mk7/MidwayFoo.cpp"
    fixture[128:129+len(source)] = source.encode() + b"\0"
    fixture[160:168] = bytes.fromhex("0000053c8000a524")
    annotated = dict(member, game_source={"path": source, "site":"000000a0", "string_va":"00000080"})
    key, citation = game_class_provenance(annotated, read, lambda pc: 0x101000)
    assert named_member(dict(annotated, game_owner_evidence_ref=citation), read, {key: citation})[0] == 0x101000
    rejects(lambda: game_class_provenance(annotated, read, lambda pc: 0x101004), "not in the verified original")
    rejects(lambda: game_class_provenance(member, read, lambda pc: 0x101000), "Missing class-qualified")
    admitted = {("MidwayFoo", 64): "synthetic-only"}
    assert named_member(member, read, admitted)[0] == 0x101000
    rejects(lambda: named_member(member, read), "no independent source witness")
    for changes in ({"name_va": "00000008"}, {"header_slot": "00000034"}, {"method": "00101004"}):
        rejects(lambda: named_member(dict(member, **changes), read, admitted), "Unproved original RTTI")
    assert clusters([0, 512, 1024], {0, 1024}) == [([0, 512, 1024], [0, 1024])]
    assert clusters([0, 1025], {0, 1025}) == []
    assert clusters(list(range(6)), {0, 5}) == []
    elf = (REPO / "research/inputs/SLUS_214.10").read_bytes()
    loads = elf_segments(elf)
    def original(address, size):
        offset = va_to_offset(loads, address, size)
        return elf[offset:offset + size]
    words = anchor_words(original)
    resource = {"reference_pc":"0044e904", "method":"0044e1c0", "pointer_word_va":"005b6e68",
                "string_va":"00618280", "string":"konquest_common.sec"}
    assert resource_anchor(resource, original, lambda pc: 0x44E1C0)[0] == 0x44E1C0
    rejects(lambda: resource_anchor(dict(resource, string="konquest_changed.sec"), original, lambda pc: 0x44E1C0),
            "Unverified game-specific resource")
    rejects(lambda: resource_anchor(resource, original, lambda pc: None), "Unverified game-specific resource")
    pair = (0x19F1E0, 0x19F0B0)
    site = 0x19F214
    sample = {"caller": "0019f1e0", "callee": "0019f0b0", "call_site": f"{site:08x}",
              "return_address": f"{site+8:08x}", "evidence_ref": "synthetic-test-only", "jal_word_le": original(site, 4).hex()}
    assert runtime_edge(sample, {pair: [site]}, original)[0] == pair[1]
    for changes in ({"return_address": "0019f218"}, {"caller": "0019f0b0"}, {"jal_word_le": "00000000"}):
        rejects(lambda: runtime_edge(dict(sample, **changes), {pair: [site]}, original), "Unproved runtime caller")
    indirect_site = 0x31DA58
    indirect = dict(sample, caller="0031d7b0", call_kind="jalr", call_site=f"{indirect_site:08x}", return_address="0031da60",
                    callee="001d1dc0", jalr_register=2, jalr_register_value="001d1dc0",
                    jal_word_le=original(indirect_site, 4).hex(), delay_word_le=original(indirect_site+4, 4).hex())
    owner = lambda pc: int(indirect["caller"], 16)
    assert runtime_edge(indirect, {}, original, owner)[0] == 0x1d1dc0
    for changes in ({"jalr_register": 1}, {"jalr_register_value": "001d1dc4"}, {"delay_word_le": "00000000"}):
        rejects(lambda: runtime_edge(dict(indirect, **changes), {}, original, owner), "Unproved runtime caller")
    runtime = json.loads((FORK_ROOT / "research/runs/renderer-scope.json").read_bytes())["bounded_inputs"]
    import struct
    table_offset = struct.unpack_from("<I", elf, 28)[0]
    row_size, row_count = struct.unpack_from("<HH", elf, 42)
    segments = [struct.unpack_from("<8I", elf, table_offset + i * row_size) for i in range(row_count)]
    bounded_recovery = recovery_report(runtime, elf, segments)
    from custom_vtable_members import recover_camera_cut, self_test as test_custom_class
    class_controls = test_custom_class(original)
    assert class_controls["status"] == "PASS"
    base_entries = {int(row["address"], 16) for row in ledger if row["module"] == "SLUS_214.10"}
    custom = recover_camera_cut(original, base_entries)
    assert custom["class_membership_verified"] and custom["verified_game_count"] == 0
    assert len(custom["catalogued_methods"]) == 6 and len(custom["outside_raw_catalogue"]) == 3
    from installed_vtable_members import self_test as test_installed_tables
    import csv
    from collections import defaultdict
    grouped = defaultdict(list)
    for store in runtime["nis_class_frontier"]["global_abi_audit"]["constructor_like_table_stores"]:
        grouped[store["candidate_vptr_va"]].append(store)
    candidates = [{"vtable_va": address, "stores": values} for address, values in sorted(grouped.items())]
    listing = REPO / "research/runs/ghidra-function-coverage-20261118/export-20261118-230132-1875-18160/instructions.tsv"
    with listing.open(encoding="utf-8", newline="") as stream:
        listed_code = {int(row["address"], 16) for row in csv.DictReader(stream, delimiter="\t")}
    executable_ranges = [(start, start+size) for kind, _, start, _, size, _, flags, _ in segments if kind == 1 and flags & 1 and size]
    assert test_installed_tables(original, candidates, executable_ranges, listed_code, base_entries)["status"] == "PASS"
    assert bounded_recovery["result"] == "BOUNDED_RECOVERY_RECORDED"
    assert bounded_recovery["conventional_named_methods"] == 0
    corrupted = copy.deepcopy(runtime)
    corrupted["rtti_load_audit"]["file_sha256"] = "0" * 64
    rejects(lambda: recovery_report(corrupted, elf, segments), "original image changed")
    corrupted = copy.deepcopy(runtime)
    corrupted["rtti_cli"]["commands"] = [v for v in corrupted["rtti_cli"]["commands"] if v["name"] != "strict"]
    rejects(lambda: recovery_report(corrupted, elf, segments), "command set is incomplete")
    corrupted = copy.deepcopy(runtime)
    corrupted["rtti_load_audit"]["file_backed_load_bytes"] -= 4
    rejects(lambda: recovery_report(corrupted, elf, segments), "coverage or controls failed")
    trace = runtime["admitted_runtime_traces"][0]
    oracle = (REPO.parents[1] / "milestone_work_verified_playable_20261110/ps2run.exe").read_bytes()
    assert list(verified_trace_edges(trace, oracle)) == trace["runtime_calls"]
    rejects(lambda: list(verified_trace_edges(trace, b"wrong oracle")), "Wrong saved-state oracle")
    incorrect = copy.deepcopy(trace)
    incorrect["runtime_calls"][0]["caller"] = "00100008"
    rejects(lambda: list(verified_trace_edges(incorrect, oracle)), "differs from dispatch event")
    incorrect = copy.deepcopy(trace)
    event = next(e for e in incorrect["selected_dispatch_observations"] if e.get("caller_admitted"))
    event["preceding_host_call_event"]["host_prefix_hex"] = "00" + event["preceding_host_call_event"]["host_prefix_hex"][2:]
    rejects(lambda: list(verified_trace_edges(incorrect, oracle)), "Unproved host call/return linkage")
    for sites in words.values():
        for address in sites:
            rejects(lambda: anchor_words(lambda pc, n: bytes(n) if pc == int(address, 16) else original(pc, n)), "Stream anchor")

    expected = capability_map_rows(donor_comparison())
    from compute_cross_isa_evidence import load_cross_map
    signature_pairs = load_cross_map()
    for row in mappings:
        key = (row["ps2_address"], row["xbox_address"])
        if key in expected:
            assert row["capability_comparison"] == expected[key]["capability_comparison"]
            if row["map_evidence"].endswith("#capability_comparison.mapping_entries"):
                assert signature_pairs.get(key[0]) != key[1], "Inferred callee leaked into signature verification"
    with tempfile.TemporaryDirectory(dir=REPO / "external/forks/ps2-native-recomp/build/oracle-telemetry") as temporary:
        fake = Path(temporary)
        (fake / "README.md").write_text("Historical line. Owner no action.", encoding="utf-8")
        rejects(lambda: validate_current_docs(fake), "Retired handoff wording")
        from bounded_ownership import direct_calls
        table = fake / "function-bodies.tsv"
        table.write_text("entry\tstart\tend\n0019f1e0\t0019f214\t0019f217\n", encoding="utf-8")
        assert direct_calls(fake, original, {pair[1]})[pair] == [site]
        assert not direct_calls(fake, lambda pc, n: bytes(n), {pair[1]})
        assert not direct_calls(fake, original, set())
    lines = bounded_progress(ledger)
    for name in ("README.md", "TASK_BOARD.md", "PORT_STATUS.md"):
        current = (REPO / name).read_text(encoding="utf-8")
        assert all(line in current for line in lines)
        altered = current.replace(lines[0], "unresolved 0")
        rejects(lambda: validate_bounded_progress(altered, ledger), "does not match measured ownership")
    print("  Bounded census rejects strict unknowns, unsupported verification, duplicate rows, unbounded clusters, changed stream words and obsolete handoff wording.")



def test_withdrawn_source_identity_bindings():
    """A moving test/comment anchor cannot establish an original function role."""
    import csv
    print("Testing planted failure 26: contradicted native aliases and upload cross-map stay withdrawn...")
    with (REPO / "research/ledger/functions.csv").open(newline="", encoding="utf8") as stream:
        rows = list(csv.DictReader(stream))
    with (REPO / "research/ledger/ps2_xbox_map.csv").open(newline="", encoding="utf8") as stream:
        mappings = list(csv.DictReader(stream))
    check = generate_ledgers.validate_source_identity_bindings
    check(rows, mappings)
    for address, bad_status in (("00225690", "host-replaced"), ("002b9c70", "ported")):
        changed = copy.deepcopy(rows)
        row = next(row for row in changed if row["module"] == "SLUS_214.10" and row["address"] == address)
        row["status"] = bad_status
        try:
            check(changed, mappings)
        except AssertionError as error:
            assert str(error) == "Contradicted legacy raster binding cannot regain native credit"
        else:
            raise AssertionError("Restored false native credit was admitted: " + address)
    changed = copy.deepcopy(mappings)
    row = next(row for row in changed if row["ps2_module"] == "SLUS_214.10" and row["ps2_address"] == "00225690")
    row.update(xbox_module="default.xbe", xbox_address="002c0020", match_confidence="mapped-by-role")
    try:
        check(rows, changed)
    except AssertionError as error:
        assert str(error) == "Contradicted upload cross-map restored"
    else:
        raise AssertionError("Restored false upload cross-map was admitted")
    print("  Both restored credits and the false upload mapping rejected; original rows, signatures and native services retained.")


if __name__ == "__main__":
    test_parent_fork_sync()
    test_sha_resolution()
    test_production_existence()
    test_withdrawn_source_identity_bindings()
    test_planted_failures()
