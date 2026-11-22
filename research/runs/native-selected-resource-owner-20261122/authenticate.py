"""Pin bounded original EE/data evidence; do not modify any game input."""
from pathlib import Path
import hashlib
import json
import struct
import subprocess

RECORD = Path(__file__).resolve().parent
ROOT = RECORD.parents[2]
PARENT = ROOT.parents[2]
ELF = PARENT / "private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/SLUS_214.10"
EXPECTED = "a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f"
data = ELF.read_bytes()
assert hashlib.sha256(data).hexdigest() == EXPECTED
assert data[:7] == b"\x7fELF\x01\x01\x01"
assert struct.unpack_from("<H", data, 18)[0] == 8
phoff = struct.unpack_from("<I", data, 28)[0]
phentsize, phnum = struct.unpack_from("<HH", data, 42)
segments = []
for i in range(phnum):
    kind, offset, vma, physical, filesz, memsz, flags, align = struct.unpack_from(
        "<8I", data, phoff + i * phentsize)
    if kind == 1:
        segments.append(dict(index=i, p_offset=offset, p_vaddr=vma, p_filesz=filesz,
                             p_memsz=memsz, p_flags=flags))

def mapped(vma, size):
    matches = [s for s in segments if s["p_vaddr"] <= vma and
               vma + size <= s["p_vaddr"] + s["p_filesz"]]
    assert len(matches) == 1, (hex(vma), size, matches)
    s = matches[0]
    offset = s["p_offset"] + vma - s["p_vaddr"]
    return data[offset:offset+size], dict(segment=s["index"], file_offset=offset,
                                        length=size, vma=f"0x{vma:08x}", file_backed=True)

def c_string(vma):
    raw, _ = mapped(vma, 256)
    raw = raw[:raw.index(0)+1]
    _, mapping = mapped(vma, len(raw))
    return dict(mapping=mapping, hex_bytes=raw.hex(), text=raw[:-1].decode("ascii"))

functions = {}
for path in sorted((RECORD / "source-01").glob("*.asm")):
    rows = [line.split("\t", 2) for line in path.read_text().splitlines()]
    addresses = [int(row[0], 16) for row in rows]
    begin, end = addresses[0], addresses[-1] + 4
    assert addresses == list(range(begin, end, 4)), path
    original, mapping = mapped(begin, end-begin)
    listed = bytes.fromhex("".join(row[1] for row in rows))
    assert listed == original, path
    functions[path.stem] = dict(mapping=mapping, end_exclusive=f"0x{end:08x}",
        words_le32=[f"{word[0]:08x}" for word in struct.iter_unpack("<I", original)],
        original_bytes_sha256=hashlib.sha256(original).hexdigest(),
        listing_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        listed_words=len(rows), full_contiguous_exported_span=True,
        scope=("suffix of original18D970, not an independent full function"
               if begin == 0x18da30 else "full exported body; see source record for semantic limits"))

row, mapping = mapped(0x5a1af0, 24)
values = struct.unpack("<6I", row)
table = dict(mapping=mapping, words_le32=[f"{w:08x}" for w in values],
             texture_name=c_string(values[1]), metrics_name=c_string(values[2]))
assert table["texture_name"]["text"] == "EDDMONDGRAY14"
assert table["metrics_name"]["text"] == "EDDMONDGRAY14_MET"
assert values[3] == 10 and values[4:] == (0, 0)
lookup_record = ROOT / "research/runs/parallel-native-font-zero-20261122/ASSET_SOURCE.json"
assets = json.loads(lookup_record.read_text())
asset_path = Path(assets["path"])
asset_bytes = asset_path.read_bytes()
assert hashlib.sha256(asset_bytes).hexdigest() == assets["sha256"]
selected = []
for r in assets["records"]:
    if r["name"] not in {"EDDMONDGRAY14", "EDDMONDGRAY14_MET"}:
        continue
    # Producer offsets are relative to the containing SEC, whose locale ranges
    # are separately pinned in the existing CPU source record.
    base = [0x800, 0x61800, 0xc2800, 0x123800, 0x184800][r["locale"]]
    actual = asset_bytes[base+r["offset"]:base+r["offset"]+r["length"]]
    assert hashlib.sha256(actual).hexdigest() == r["sha256"]
    selected.append(r)
assert len(selected) == 10

def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()

production = {}
for name in ["runtime/src/native/retail_saved_profile_bindings.cpp",
             "runtime/src/native/retail_saved_profile_records.cpp",
             "runtime/src/native/retail_arcade_ladder.cpp",
             "runtime/src/native/retail_system_art_cpu.cpp",
             "runtime/include/mka/retail_system_art_cpu.hpp",
             "cmake/native_game.cmake"]:
    raw = (ROOT / name).read_bytes()
    committed = subprocess.check_output(["git", "show", f"HEAD:{name}"], cwd=ROOT)
    assert raw.replace(b"\r\n", b"\n") == committed.replace(b"\r\n", b"\n")
    production[name] = dict(worktree_sha256=hashlib.sha256(raw).hexdigest(),
                             committed_blob_sha256=hashlib.sha256(committed).hexdigest())

value = dict(status="SOURCE_DERIVED_ONLY_NO_NEW_IMPLEMENTATION",
    fork_head=git("rev-parse", "HEAD"), origin_main=git("rev-parse", "origin/main"),
    elf=dict(path=str(ELF), sha256=EXPECTED, pt_load_segments=segments),
    function_spans=functions, font_zero_descriptor=table,
    selected_asset_source_record_sha256=hashlib.sha256(lookup_record.read_bytes()).hexdigest(),
    original_gameart_sha256=assets["sha256"], selected_original_payloads=selected,
    production_sources=production, new_build=False, new_game_run=False,
    visible_window=False, audio_endpoint=False, acceptance="OPEN")
with (RECORD / "SOURCE_RECEIPT.json").open("x", newline="\n") as f:
    json.dump(value, f, indent=2)
    f.write("\n")
print("SOURCE_DERIVED: exact original EE/data joins; no implementation/test/game pass")
