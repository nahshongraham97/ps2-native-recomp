import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

for _p in [Path(__file__).resolve(), *Path(__file__).resolve().parents]:
    if (_p / "research" / "test_vectors" / "traces").exists():
        REPO = _p
        break
else:
    REPO = Path(__file__).resolve().parents[2]
VECTORS_DIR = REPO / "research" / "test_vectors"
TRACES_DIR = VECTORS_DIR / "traces"
SCHEMA_PATH = VECTORS_DIR / "schema.json"

# In-memory registry for native callable functions/simulators when executing within Python harness
NATIVE_DISPATCH_REGISTRY = {}


def register_native_dispatch(symbol_name: str, func, implementation_kind: str = "infrastructure_testing_python"):
    """Registers a native function callable for test vector execution.
    implementation_kind: 'compiled_cpp' for production native implementation,
                        'infrastructure_testing_python' for harness simulation (infrastructure testing only; zero production implementation credit).
    """
    NATIVE_DISPATCH_REGISTRY[symbol_name] = {
        "callable": func,
        "implementation_kind": implementation_kind,
    }


def execute_native_function(symbol_name: str, target_address: str, args: list) -> any:
    """Executes the recompiled/ported native function or harness with provided arguments."""
    if symbol_name in NATIVE_DISPATCH_REGISTRY:
        entry = NATIVE_DISPATCH_REGISTRY[symbol_name]
        return entry["callable"](*args)

    # Check if a compiled native test runner executable exists in build directory
    runner_exe = REPO / "build" / "bin" / "test_retail_native_runner.exe"
    if runner_exe.exists():
        cmd = [str(runner_exe), "--symbol", symbol_name, "--addr", target_address, "--args", json.dumps(args)]
        res = subprocess.run(cmd, capture_output=True, text=True)
        if res.returncode != 0:
            raise RuntimeError(f"Native runner execution failed: {res.stderr}")
        return json.loads(res.stdout)["return_value"]

    raise RuntimeError(
        f"Native execution target unavailable for symbol '{symbol_name}' ({target_address}). "
        f"Native binary must be compiled or registered via register_native_dispatch()."
    )


def validate_vector_file(path: Path, execute_native: bool = True) -> dict:
    """Validates a single cross-platform test vector file against schema, oracle hash, and native execution."""
    if not path.exists():
        raise AssertionError(f"Test vector file does not exist: {path}")

    content = path.read_text(encoding="utf-8").strip()
    if not content:
        raise AssertionError(f"Test vector file is empty: {path}")

    try:
        data = json.loads(content)
    except json.JSONDecodeError as e:
        raise AssertionError(f"Test vector file contains invalid JSON ({path}): {e}")

    if not isinstance(data, dict) or len(data) == 0:
        raise AssertionError(f"Test vector file is an empty placeholder dict: {path}")

    # Required top-level fields
    required_keys = [
        "schema", "target_platform", "donor_platform",
        "target_address", "donor_address", "symbol_name",
        "oracle_source", "oracle_trace_hash", "test_cases"
    ]
    for k in required_keys:
        if k not in data:
            raise AssertionError(f"Test vector missing required key '{k}': {path}")

    if data["schema"] != "cross_platform_test_vector_v1":
        raise AssertionError(f"Unsupported schema '{data.get('schema')}' in {path}")

    # Validate oracle source and 64-character SHA-256 hash
    oracle_src = data.get("oracle_source", "")
    if not isinstance(oracle_src, str) or not oracle_src.strip():
        raise AssertionError(f"Test vector missing non-empty oracle_source: {path}")

    oracle_hash = data.get("oracle_trace_hash", "")
    if not isinstance(oracle_hash, str) or not re.match(r"^[0-9a-fA-F]{64}$", oracle_hash):
        raise AssertionError(
            f"Test vector missing valid 64-character hexadecimal SHA-256 oracle_trace_hash "
            f"(got '{oracle_hash}'): {path}"
        )

    # Verify that the oracle trace file exists on disk in TRACES_DIR and its hash matches
    trace_path = TRACES_DIR / oracle_src
    if not trace_path.exists():
        trace_path = TRACES_DIR / f"{oracle_src}.bin"
    if not trace_path.exists():
        trace_path = TRACES_DIR / f"{oracle_src}.trace"
    if not trace_path.exists():
        raise AssertionError(
            f"Oracle trace file '{oracle_src}' does not exist on disk in {TRACES_DIR}"
        )
    actual_trace_hash = hashlib.sha256(trace_path.read_bytes()).hexdigest().lower()
    if actual_trace_hash != oracle_hash.lower():
        raise AssertionError(
            f"Stored oracle trace file '{trace_path.name}' SHA-256 ({actual_trace_hash}) "
            f"does not match vector oracle_trace_hash ({oracle_hash})"
        )

    test_cases = data.get("test_cases", [])
    if not isinstance(test_cases, list) or len(test_cases) == 0:
        raise AssertionError(f"Test vector contains zero test cases: {path}")

    symbol_name = data.get("symbol_name", "")
    target_addr = data.get("target_address", "")

    for idx, tc in enumerate(test_cases):
        if not isinstance(tc, dict):
            raise AssertionError(f"Test case {idx} is not an object in {path}")
        for field in ["case_id", "inputs", "expected_outputs", "ps2_result", "xbox_result", "outputs_match"]:
            if field not in tc:
                raise AssertionError(f"Test case {idx} missing required field '{field}' in {path}")
        if not tc.get("outputs_match"):
            raise AssertionError(f"Test case {idx} outputs do not match across platforms in {path}")
        if not isinstance(tc.get("inputs"), dict) or "arguments" not in tc["inputs"]:
            raise AssertionError(f"Test case {idx} inputs missing arguments in {path}")

        # Native execution verification against oracle expected outputs
        if execute_native:
            args = tc["inputs"]["arguments"]
            expected_ret = tc["expected_outputs"].get("return_value")
            try:
                actual_ret = execute_native_function(symbol_name, target_addr, args)
                if actual_ret != expected_ret:
                    raise AssertionError(
                        f"Test case '{tc.get('case_id')}' output mismatch for {symbol_name}: "
                        f"native function returned {actual_ret!r}, expected {expected_ret!r} "
                        f"from oracle '{oracle_src}' (trace hash: {oracle_hash})"
                    )
            except RuntimeError as e:
                raise AssertionError(
                    f"Native execution failed for {symbol_name} ({target_addr}) on test case '{tc.get('case_id')}': {e}"
                )

    return data


def verify_function_donor_vector(address: str, execute_native: bool = True) -> dict:
    """Verifies that an implemented-via-donor function has a valid test vector."""
    clean_addr = address.replace("0x", "").lower().zfill(8)
    vector_path = VECTORS_DIR / f"vector_{clean_addr}.json"
    if not vector_path.exists():
        raise AssertionError(
            f"Function {address} marked implemented-via-donor requires verified test vectors on disk, "
            f"but {vector_path} was not found."
        )
    return validate_vector_file(vector_path, execute_native=execute_native)


def run_all_vectors(execute_native: bool = True) -> int:
    """Validates all vector files in research/test_vectors/."""
    if not VECTORS_DIR.exists():
        return 0
    vector_files = list(VECTORS_DIR.glob("vector_*.json"))
    for vf in vector_files:
        validate_vector_file(vf, execute_native=execute_native)
    return len(vector_files)


if __name__ == "__main__":
    count = run_all_vectors()
    print(f"PASS: Validated {count} cross-platform test vector files.")
