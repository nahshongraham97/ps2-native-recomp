"""Test gzip determinism and log stream ordering for run_check runner."""
import gzip
import hashlib
import io

def compress_deterministic(data: bytes) -> bytes:
    buf = io.BytesIO()
    with gzip.GzipFile(filename="", mode="wb", fileobj=buf, mtime=0) as gz:
        gz.write(data)
    return buf.getvalue()

def test_gzip_determinism():
    stream_a1 = b"stage 1: init\nstage 2: execute\nstage 3: cleanup\n"
    stream_a2 = b"stage 1: init\nstage 2: execute\nstage 3: cleanup\n"
    # Reordered stream:
    stream_b = b"stage 2: execute\nstage 1: init\nstage 3: cleanup\n"

    gz_a1 = compress_deterministic(stream_a1)
    gz_a2 = compress_deterministic(stream_a2)
    gz_b = compress_deterministic(stream_b)

    hash_a1 = hashlib.sha256(gz_a1).hexdigest()
    hash_a2 = hashlib.sha256(gz_a2).hexdigest()
    hash_b = hashlib.sha256(gz_b).hexdigest()

    raw_hash_a1 = hashlib.sha256(stream_a1).hexdigest()
    raw_hash_a2 = hashlib.sha256(stream_a2).hexdigest()
    raw_hash_b = hashlib.sha256(stream_b).hexdigest()

    assert hash_a1 == hash_a2, f"Deterministic compression failed: {hash_a1} != {hash_a2}"
    assert hash_a1 != hash_b, f"Reordered content collision: {hash_a1} == {hash_b}"
    assert raw_hash_a1 == raw_hash_a2, "Raw SHA-256 mismatch for identical data"
    assert raw_hash_a1 != raw_hash_b, "Raw SHA-256 collision for reordered data"

    # Verify no embedded filename (header byte 3 FNAME flag must be 0)
    flags = gz_a1[3]
    assert (flags & 0x08) == 0, f"FNAME flag unexpectedly set in gzip header: {flags:02x}"
    # Verify mtime bytes (bytes 4-7) are all zero
    mtime = gz_a1[4:8]
    assert mtime == b"\x00\x00\x00\x00", f"Non-zero mtime in gzip header: {mtime.hex()}"

    print(f"PASS: test_gzip_determinism verified identical hash ({hash_a1[:16]}...) and distinct reordered hash ({hash_b[:16]}...).")

if __name__ == "__main__":
    test_gzip_determinism()
