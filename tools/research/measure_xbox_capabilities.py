"""Rejoin bounded PS2/Xbox renderer evidence; identification never awards native credit.

Run locally against the existing private images. Capstone is an analysis-only
dependency already installed on this research host; no runtime code is copied.
The existing donor PUBLICATION.json owns the generated capability_comparison.
"""
from __future__ import annotations

import argparse
import copy
import csv
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct
import subprocess
import sys

ELF_SHA = "a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f"
XBE_SHA = "78f99fca447fddc5c5116c09d27e6f1c04af8572dace94f559966588c6f5e393"
RECORD = "research/runs/xbox-donor-20261121/PUBLICATION.json"
READ_ME = "research/runs/xbox-donor-20261121/README.md"
JOIN_DIRS = ["native-xbox-renderer-0403-20261122", "native-xbox-renderer-0404-20261122",
             "native-xbox-renderer-0405-20261122", "native-xbox-renderer-0406-20261122",
             "native-xbox-renderer-0407-20261122"]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8"))


def verify_original_identity(data, expected):
    assert sha(data) == expected, "Original image identity mismatch"


class Image:
    def __init__(self, data, platform):
        self.data, self.platform = data, platform
        self.sections = []
        if platform == "xbox":
            assert data[:4] == b"XBEH", "Wrong Xbox image"
            base = struct.unpack_from("<I", data, 0x104)[0]
            count, table = struct.unpack_from("<II", data, 0x11c)
            for index in range(count):
                flags, va, virtual_size, offset, file_size, name = struct.unpack_from("<6I", data, table-base+index*56)
                label = data[name-base:data.index(0, name-base)].decode("ascii")
                self.sections.append((va, offset, min(virtual_size, file_size), label))
        else:
            assert data[:4] == b"\x7fELF", "Wrong PS2 image"
            table = struct.unpack_from("<I", data, 28)[0]
            stride, count = struct.unpack_from("<HH", data, 42)
            for index in range(count):
                kind, offset, va, physical, size, memory, flags, align = struct.unpack_from("<8I", data, table+index*stride)
                if kind == 1:
                    self.sections.append((va, offset, size, "PT_LOAD"))

    def mapping(self, va, length):
        rows = [row for row in self.sections if row[0] <= va and va+length <= row[0]+row[2]]
        assert len(rows) == 1, f"Unmapped/ambiguous {self.platform} VA {va:08x}+{length}"
        v, offset, size, label = rows[0]
        at = offset+va-v
        assert at+length <= len(self.data)
        return at, label

    def raw(self, va, length):
        offset, label = self.mapping(va, length)
        return self.data[offset:offset+length]


def disassembler():
    import capstone
    result = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    result.detail = True
    return result


def decode_saved_span(decoder, data, va):
    instructions = list(decoder.disasm(data, va))
    assert sum(ins.size for ins in instructions) == len(data), "Partial instruction decode"
    # Ghidra groups the legacy WAIT prefix with these two saved x87 stores;
    # Capstone exposes WAIT as its own instruction. Keep the producer's saved
    # group count distinct and accept only the exact observed prefix families.
    if len(instructions) != 1:
        assert (len(instructions) == 2 and data[:1] == b"\x9b"
                and instructions[0].mnemonic == "wait"
                and instructions[1].mnemonic in ("fnstcw", "fnsave")), "Unexpected multi-instruction saved span"
    return instructions


def xbox_registrations(image, decoder):
    # Original registration function already identified by the published map;
    # validate its complete contiguous bytes and actual PUSH/CALL instructions.
    begin, length = 0x2c0020, 0x230
    raw = image.raw(begin, length)
    instructions = list(decoder.disasm(raw, begin))
    assert sum(i.size for i in instructions) == length and instructions[-1].mnemonic == "ret"
    pushes, rows = [], []
    for ins in instructions:
        if ins.mnemonic == "push":
            pushes.append(int(ins.op_str, 0) if re.fullmatch(r"(?:0x[0-9a-f]+|[0-9]+)", ins.op_str) else None)
        if ins.mnemonic == "call" and ins.op_str == "0x2c2990":
            zero, destructor, constructor, plugin_id, extent, registry = pushes[-6:]
            assert zero == 0 and registry == 0x5a6788 and constructor and destructor
            rows.append({"plugin_id": f"{plugin_id:04x}", "source_extent": extent,
                         "constructor": f"{constructor:08x}", "destructor": f"{destructor:08x}",
                         "call_address": f"{ins.address:08x}"})
    assert len(rows) == len({row["plugin_id"] for row in rows}) == 14, "Invalid/duplicate registration IDs"
    return rows, {"va": f"{begin:08x}", "file_offset": image.mapping(begin, length)[0],
                  "bytes": length, "instruction_count": len(instructions), "sha256": sha(raw)}


def source_ids(image):
    result = []
    for hit in re.finditer(rb"@+\(#\)\$Id: //RenderWare/RW36Active/(rwsdk/[^$\x00]+#\d+) \$(?=\x00)", image.data):
        mapped = [(va+hit.start()-offset, label) for va, offset, size, label in image.sections
                  if offset <= hit.start() and hit.end() <= offset+size]
        assert len(mapped) == 1
        result.append({"file_offset": hit.start(), "va": f"{mapped[0][0]:08x}",
                       "section": mapped[0][1], "source_revision": hit[1].decode("ascii")})
    assert len(result) == 85 and len({row["source_revision"] for row in result}) == 85
    return result


def joined_functions(fork, images, decoder):
    """Verify saved instruction spans against originals without trusting names."""
    output, pins = {}, {}
    for directory in JOIN_DIRS:
        for path in sorted((fork/"research/runs"/directory).glob("*.join.json")):
            record = read_json(path)
            platform = record.get("platform", "xbox")
            for address, entry in record.get("functions", {}).items():
                spans = entry.get("original_spans")
                if not spans:
                    continue
                image = images[platform]
                checked = []
                for span in spans:
                    va = int(span["vma"], 16)
                    data = bytes.fromhex(span["bytes_hex"])
                    assert image.raw(va, len(data)) == data, f"Changed original span {path.name}:{va:08x}"
                    assert image.mapping(va, len(data))[0] == span["file_offset"]
                    if platform == "xbox":
                        decode_saved_span(decoder, data, va)
                    else:
                        assert len(data) == 4
                    checked.append((va, len(data)))
                assert entry["instructions"] == len(checked), "Saved instruction count disagrees with spans"
                output[(platform, address)] = {"spans": checked, "record": str(path.relative_to(fork))}
                pins["fork/"+str(path.relative_to(fork)).replace("\\", "/")] = sha(path.read_bytes())
    # Older allocator/matrix records use explicit per-instruction mapping tuples.
    for directory, name, field in [("parallel-renderer-plugin-0401-20261122", "SOURCE_JOIN.json", "Xbox_listings"),
                                    ("parallel-renderer-plugin-0402-20261122", "XBOX_JOIN.json", "functions")]:
        path = fork/"research/runs"/directory/name
        for name, entry in read_json(path)[field].items():
            address = Path(name).stem.lower()
            spans = entry.get("mapped_spans", entry.get("mapped_instructions", []))
            for va, length, offset in spans:
                assert images["xbox"].mapping(va, length)[0] == offset
                ins = list(decoder.disasm(images["xbox"].raw(va, length), va))
                assert len(ins) == 1 and ins[0].size == length
            assert len(spans) == entry["instructions"]
            output[("xbox", address)] = {"spans": [(va, length) for va, length, offset in spans],
                                          "record": str(path.relative_to(fork))}
        pins["fork/"+str(path.relative_to(fork)).replace("\\", "/")] = sha(path.read_bytes())
    return output, pins


def bounded_xbox_cfg(image, decoder, begin):
    """Decode direct-flow blocks, without treating summed Ghidra size as a span."""
    pending, instructions, stops = [begin], {}, []
    while pending:
        at = pending.pop()
        if at in instructions:
            continue
        if not begin <= at < begin+0x2000:
            stops.append({"va": f"{at:08x}", "reason": "direct tail/branch outside bounded window"})
            continue
        assert len(instructions) < 2048, "Bounded Xbox decode exceeded instruction cap"
        decoded = list(decoder.disasm(image.raw(at, 15), at, count=1))
        assert len(decoded) == 1, f"Undecodable reachable Xbox instruction {at:08x}"
        ins = decoded[0]
        instructions[at] = ins
        if ins.mnemonic.startswith("ret"):
            continue
        branch = ins.mnemonic.startswith("j") or ins.mnemonic in ("loop", "loope", "loopne")
        if branch:
            if ins.op_str.startswith("0x"):
                pending.append(int(ins.op_str,16))
            else:
                stops.append({"va": f"{at:08x}", "reason": "indirect jump target unproved"})
            if ins.mnemonic == "jmp":
                continue
        pending.append(at+ins.size)
    return [(at, ins.size) for at,ins in sorted(instructions.items())], stops


def function_measurement(platform, address, images, joined, decoder, census, xbox_sizes):
    image, va = images[platform], int(address, 16)
    known = joined.get((platform, address))
    if known:
        spans = known["spans"]
        count_scope = "individually rejoined saved listing instructions; complete semantic/body coverage is not inferred"
        evidence_record = known["record"]
    elif platform == "ps2":
        entries = census["entries"] + census.get("source_proved_starts_outside_in_scope_raw_catalogue_not_counted", [])
        found = next((row for row in entries if row.get("address") == address), {})
        reason = next((reason for reason in found.get("reasons", []) if reason.get("end_exclusive")), None)
        if reason:
            length = int(reason["end_exclusive"], 16)-va
            data = image.raw(va, length)
            expected = reason.get("body_sha256", reason.get("original_body_sha256"))
            if expected:
                assert sha(data) == expected, "Changed authenticated PS2 body"
            spans = [(at, 4) for at in range(va, va+length, 4)]
            count_scope = "authenticated PS2 byte interval, fixed 4-byte instruction words including delay slots"
        else:
            # This is explicitly a prefix, never a claimed full function bound.
            spans = []
            for at in range(va, va+1024, 4):
                word = struct.unpack("<I", image.raw(at, 4))[0]
                spans.append((at, 4))
                if word == 0x03e00008:
                    spans.append((at+4, 4))
                    break
            count_scope = "bounded prefix through first JR RA and its delay slot; full function extent unknown"
        evidence_record = "fork/research/runs/renderer-scope.json seed hashes and tracked SOURCE_JOIN byte proofs"
    else:
        spans, stops = bounded_xbox_cfg(image, decoder, va)
        count_scope = "bounded direct-control-flow instruction set; indirect targets/tail boundaries and semantic completeness unproved"
        evidence_record = "original XBE; reachable direct blocks decoded by Capstone, never a contiguous Ghidra-size assumption"
    calls, indirect_calls, constants, displacements, memory_accesses = [], [], [], set(), []
    payload = bytearray()
    ranges = []
    for at, length in spans:
        data = image.raw(at, length)
        payload.extend(data)
        if ranges and ranges[-1][1] == at:
            ranges[-1][1] += length
        else:
            ranges.append([at, at+length])
        if platform == "xbox":
            ins = list(decoder.disasm(data, at))
            assert len(ins) == 1 and ins[0].size == length
            ins = ins[0]
            if ins.mnemonic == "call":
                if ins.op_str.startswith("0x"):
                    calls.append({"site": f"{at:08x}", "target": f"{int(ins.op_str,16):08x}"})
                else:
                    indirect_calls.append({"site": f"{at:08x}", "operand": ins.op_str})
            for op in ins.operands:
                if op.type == 2:
                    constants.append(op.imm & 0xffffffff)
                elif op.type == 3:
                    displacements.add(op.mem.disp)
        else:
            word = struct.unpack("<I", data)[0]
            if word >> 26 == 3:
                calls.append({"site": f"{at:08x}", "target": f"{(((at+4)&0xf0000000)|((word&0x3ffffff)<<2)):08x}"})
            if word >> 26 == 0 and word & 63 == 9:
                indirect_calls.append({"site": f"{at:08x}", "register": (word>>21)&31})
            constants.append(word & 0xffff)
            if word >> 26 in (0x1e, 0x1f, 0x23, 0x2b, 0x31, 0x37, 0x39, 0x3f):
                offset = (word & 0xffff) - (0x10000 if word & 0x8000 else 0)
                memory_accesses.append({"site": f"{at:08x}", "opcode": word>>26,
                                        "base_register": (word>>21)&31, "signed_offset": offset})
    return {"platform": platform, "va": address, "address_form": "virtual-address",
            "file_offset": image.mapping(va, 4)[0], "instruction_count": len(spans),
            "catalogued_size_hint_not_a_contiguous_span": xbox_sizes.get(address) if platform == "xbox" else None,
            "direct_flow_stops": stops if platform == "xbox" and not known else [],
            "listed_bytes": len(payload), "original_instruction_bytes_sha256": sha(payload),
            "count_scope": count_scope, "evidence_record": evidence_record.replace("\\", "/"),
            "ranges": [{"va_start": f"{start:08x}", "va_end_exclusive": f"{end:08x}",
                        "file_offset": image.mapping(start, end-start)[0], "bytes": end-start}
                       for start, end in ranges], "direct_calls": calls, "indirect_calls": indirect_calls,
            "constants": sorted(set(constants)), "observed_memory_displacements": sorted(displacements),
            "ps2_memory_accesses": memory_accesses}


def identity_anchor(image, literal):
    assert image.data.count(literal) == 1
    offset = image.data.index(literal)
    rows = [(va+offset-start, label) for va, start, length, label in image.sections
            if start <= offset and offset+len(literal) <= start+length]
    assert len(rows) == 1
    return {"literal": literal.rstrip(b"\0").decode("ascii"), "file_offset": offset,
            "va": f"{rows[0][0]:08x}", "section": rows[0][1], "bytes_sha256": sha(literal)}


def ps2_evidence_from_tracked_records(fork, image):
    """Regenerate registry rows; recover authenticated lengths by pinned hashes."""
    scope = read_json(fork/"research/runs/renderer-scope.json")["instruction_phase6"]
    parser_path = fork/"tools/research/measure_renderer_scope.py"
    spec = importlib.util.spec_from_file_location("xbox_capability_registry_parser", parser_path)
    parser = importlib.util.module_from_spec(spec)
    # This calls only the tracked instruction parser, not measure_scope(), so a
    # clean checkout needs neither an ignored census nor generated AOT output.
    sys.path.insert(0, str(fork/"tools/research"))
    try:
        spec.loader.exec_module(parser)
    finally:
        sys.path.pop(0)
    registrations = parser.registry_calls(image.raw, 0x1a66a0, 0x1a6890)
    entries = []
    admissions = scope["rw36_xbox_and_ps2_streams"]["ps2_admissions"]
    for row in admissions:
        start = int(row["address"], 16)
        digest = hashlib.sha256()
        matched = None
        for at in range(start, start+8192, 4):
            digest.update(image.raw(at, 4))
            if digest.hexdigest() == row["original_body_sha256"]:
                matched = at+4
                break
        assert matched is not None, "No bounded original interval matches the published PS2 body hash"
        entries.append({"address": row["address"], "reasons": [dict(row, end_exclusive=f"{matched:08x}")]})
    for directory, field in [("parallel-renderer-plugin-0401-20261122", "full_listings"),
                             ("parallel-renderer-plugin-0402-20261122", "functions")]:
        path = fork/"research/runs"/directory/"SOURCE_JOIN.json"
        for name, row in read_json(path)[field].items():
            start, end = int(row["begin"],16), int(row["end_exclusive"],16)
            raw = image.raw(start,end-start)
            expected = row.get("bytes_sha256")
            if expected:
                assert sha(raw) == expected
            elif row.get("mapping",{}).get("bytes"):
                assert raw == bytes.fromhex(row["mapping"]["bytes"])
            else:
                raise AssertionError("Tracked PS2 listing has no original-byte proof")
            entries.append({"address": f"{start:08x}", "reasons": [{"end_exclusive": f"{end:08x}", "body_sha256": sha(raw)}]})
    return {"registrations": registrations, "entries": entries}, scope


def validate_mapping_entries(rows, images=None):
    seen = set()
    for row in rows:
        pair = (row["ps2_address"], row["xbox_address"])
        assert pair not in seen and all(re.fullmatch("[0-9a-f]{8}", value) for value in pair)
        seen.add(pair)
        assert row["address_form"] == "virtual-address"
        assert row["match_confidence"] == "mapped-by-role" and row["ownership_status"] == "inferred-renderware"
        assert row["identification_verification_level"] == "cross-platform"
        assert row["native_verification_level"] == "none" and row["native_credit"] is False
        assert row["adoption_permitted"] is False and "E3" in row["evidence_kinds"]
        assert type(row["ps2_file_offset"]) is int and type(row["xbox_file_offset"]) is int
        if images:
            for platform in ("ps2", "xbox"):
                assert images[platform].mapping(int(row[platform+"_address"],16),4)[0] == row[platform+"_file_offset"], "Mapping VA/file-offset identity mismatch"


def build(parent, fork):
    decoder = disassembler()
    elf = (parent/"private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/SLUS_214.10").read_bytes()
    xbe = (parent/"private-inputs/xbox-mka-20261121/default.xbe").read_bytes()
    verify_original_identity(elf, ELF_SHA)
    verify_original_identity(xbe, XBE_SHA)
    images = {"ps2": Image(elf, "ps2"), "xbox": Image(xbe, "xbox")}
    census, published_scope = ps2_evidence_from_tracked_records(fork, images["ps2"])
    catalogue_path = parent/"research/ledger/xbox_raw_functions.csv"
    with catalogue_path.open(encoding="utf-8", newline="") as stream:
        xbox_sizes = {row["address"].lower(): int(row["size"]) for row in csv.DictReader(stream)
                      if re.fullmatch("[0-9a-fA-F]{8}", row["address"])}
    joined, pins = joined_functions(fork, images, decoder)
    pins["fork/tools/research/measure_renderer_scope.py"] = sha((fork/"tools/research/measure_renderer_scope.py").read_bytes())
    pins["parent/research/ledger/xbox_raw_functions.csv"] = sha(catalogue_path.read_bytes())
    ids = source_ids(images["xbox"])
    xbox_rows, registration = xbox_registrations(images["xbox"], decoder)
    ps2_rows = {row["plugin_id"]: row for row in census["registrations"]}
    assert set(ps2_rows) == {row["plugin_id"] for row in xbox_rows}
    measures = {}
    def measure(platform, address):
        key = platform+":"+address
        if key not in measures:
            measures[key] = function_measurement(platform, address, images, joined, decoder, census, xbox_sizes)
        return measures[key]
    mapping = []
    with (parent/"research/ledger/functions.csv").open(encoding="utf-8", newline="") as stream:
        catalogue_keys = {(row["module"], row["address"].lower()) for row in csv.DictReader(stream)}
    module_capabilities = {"0401": "heap", "0402": "matrix_transforms", "0403": "matrix_transforms",
                           "0404": "streams_chunks", "0405": "camera_view_window", "0406": "textures",
                           "0407": "textures", "0408": "textures", "0409": "textures", "040a": "materials",
                           "040b": "geometry", "0412": "world", "040f": "renderer_core", "040d": "renderer_core"}
    def add_map(ps2, xbox, capabilities, role, method, evidence=("E3",)):
        a, b = measure("ps2", ps2), measure("xbox", xbox)
        mapping.append({"ps2_module": "SLUS_214.10", "ps2_address": ps2,
                        "xbox_module": "default.xbe", "xbox_address": xbox,
                        "capabilities": capabilities, "role": role, "match_confidence": "mapped-by-role",
                        "ownership_status": "inferred-renderware", "evidence_kinds": list(evidence),
                        "identification_verification_level": "cross-platform", "native_verification_level": "none",
                        "native_credit": False, "adoption_permitted": False, "address_form": "virtual-address",
                        "ps2_file_offset": a["file_offset"], "xbox_file_offset": b["file_offset"],
                        "ps2_body_sha256": a["original_instruction_bytes_sha256"],
                        "xbox_body_sha256": b["original_instruction_bytes_sha256"],
                        "body_hash_scope": "Digests cover the explicitly measured instruction ranges; bounded prefixes/direct-CFG sets are not assumed complete function bodies.",
                        "ps2_count_scope": a["count_scope"], "xbox_count_scope": b["count_scope"],
                        "catalogue_row_exists": ("SLUS_214.10", ps2) in catalogue_keys,
                        "evidence_path": RECORD, "map_evidence": RECORD+"#capability_comparison.mapping_entries",
                        "method": (dict(kind="registration-argument-roles", **method) if isinstance(method, dict)
                                   else {"kind": "constants-and-call-graph", "summary": method}),
                        "limit": "Role identification only; no differential behavior or code-adoption permission."})
    for xb in xbox_rows:
        ps = ps2_rows[xb["plugin_id"]]
        for role in ("constructor", "destructor"):
            add_map(ps[role], xb[role], [module_capabilities[xb["plugin_id"]]],
                    "plugin "+xb["plugin_id"]+" "+role,
                    {"shared_plugin_id": xb["plugin_id"], "ps2_registration_call_va": ps["call_address"],
                     "xbox_registration_call_va": xb["call_address"], "ps2_module_bytes": ps["source_extent"],
                     "xbox_module_bytes": xb["source_extent"], "shape": "same numeric registration and constructor/destructor argument roles; layouts may differ"})
    # New cross-platform candidates: matched constants and dependent-call shape,
    # not a symbol import or an equivalence claim.
    add_map("0019f0b0", "002b9680", ["streams_chunks"], "candidate chunk-header reader",
            "12-byte header and packed/legacy library-ID decoding; callee of both chunk finder and registry reader", ("E3", "E5"))
    add_map("0019f1e0", "002b97a0", ["streams_chunks"], "candidate RwStreamFindChunk",
            "same unsigned 0x34000..0x36003 bounds and header-read/skip/repeat call shape", ("E3", "E5"))
    add_map("001a2bc0", "002c24b0", ["plugin_registry"], "candidate _rwPluginRegistryReadDataChunks",
            "extension type 3, header finder/reader/skip calls, payload-plus-12 accounting and separate read/always indirect callback loops", ("E3", "E5"))
    add_map("001a64a0", "002bb870", ["camera_view_window", "viewport_hd"], "candidate RwCameraSetViewWindow",
            "two window stores and reciprocal stores followed by conditional parent-frame dirty path", ("E3", "E5"))
    add_map("0019f680", "002bd250", ["matrix_transforms"], "candidate matrix product kernels",
            "matrix-product role and module dispatch relation; EE accumulator versus scalar SSE ordering differs and is not verified equivalent", ("E3", "E5"))
    add_map("001a66a0", "002c0020", ["renderer_core"], "candidate engine registration assembly",
            "same independently decoded ordered plugin-ID/extent/constructor/destructor argument table; separate registries and calling conventions", ("E3", "E5"))
    validate_mapping_entries(mapping, images)
    # Inspect two platform-boundary setup routines without suggesting a PS2
    # pairing where none has been authenticated.
    measure("xbox", "002695f0")
    measure("xbox", "002a37cb")
    groups = [
        ("renderer_core", "Renderer core", "unknown", "Plugin registration is common; complete device Open/Start/Stop and scene consumers remain untransformed.", "retail_renderer_core", "Fourteen registered module callbacks; guest registry and device ownership are replaced by typed Core/Session owners.", ["GPU device/context", "file/heap services", "plugin callbacks"]),
        ("streams_chunks", "Streams and chunks", "equal", "Matched chunk structure/version gate and existing native read cursors; neither side removes the parsing or lifetime work.", "retail_renderer_streams", "Three header words and optional decoded version/build outputs; platform file representations are separate.", ["native file/memory cursor", "error domain", "read/skip/close"]),
        ("plugin_registry", "Plugin registry", "equal", "Both sides traverse extension IDs and callbacks; the native callback ABI and ownership transformation are required for either.", "retail_renderer_core", "Xbox observed entry offsets 0/4/8/0c/18/30; these are evidence, not a native or PS2 layout.", ["typed callbacks", "allocation", "stream services"]),
        ("camera_view_window", "Camera and view window", "equal", "Window/reciprocal operations match in role; projection/depth arithmetic and attached-frame behavior still need independent verification.", "retail_renderer_cameras", "Xbox stores window at +68/+6c and reciprocals at +70/+74; PS2 offsets are independently decoded, never shared by assumption.", ["frame dirty/sync", "device depth", "raster attachment"]),
        ("matrix_transforms", "Matrix and transforms", "PS2", "PS2 AOT already preserves the gameplay-critical EE accumulator order; Xbox SSE product is a candidate with a known ordering difference, so changing kernels adds an unclosed numerical obligation.", "retail_renderer_matrix", "Typed matrix values exist; console module locators, guest pointers and $gp are not native fields.", ["EE numeric order", "classification/inverse", "frame hierarchy"]),
        ("heap", "Heap", "equal", "Both module lifecycles require real allocation; existing native Permanent/heap services already remove console pool representations. General allocator-algorithm comparison remains unknown.", "retail_renderer_allocator", "Module slot is 24 bytes in both registrations; equal slot sizes do not prove allocator object layouts or identical policies.", ["existing native allocations", "lifetime/error", "no console pool capacity"]),
        ("materials", "Materials", "unknown", "Only bounded module/container behavior is established; high-level material/shader and asset-consumer contracts are incomplete.", "retail_renderer_materials", "Registration modules differ: PS2 124 bytes, Xbox 116 bytes; raw structures are not interchangeable.", ["texture ownership", "material extension callbacks", "shader binding"]),
        ("geometry", "Geometry", "unknown", "The 40-byte module registration does not recover mesh/vertex/triangle/skinning semantics; shorter constructor code cannot select a complete rendering route.", "retail_renderer_geometry", "Module container is 40 bytes in both registrations; native num_vertices/num_triangles fields remain design checks rather than recovered retail layout.", ["mesh/index format", "skin/morph", "platform instancing/pipeline"]),
        ("textures", "Textures", "equal", "Native image/raster/texture owners and bounded PS2 atlas decode already exist. Shared scalar evidence is useful, while native backing formats differ; no full texture-decoder route advantage is proved.", "retail_renderer_textures", "Image/raster/texture/dictionary module slots are 544/100/52/96 bytes in both registrations; module sizes are not texture payload formats.", ["native upload/sampler/fence", "PS2 packed atlas", "Xbox native raster format"]),
        ("world", "World", "unknown", "A four-byte module slot and tested container lifetime do not establish sector/BSP/atomic draw algorithms or full scene linkage.", "retail_renderer_world", "Four-byte module registration on both builds; native world/sector data layout remains incomplete.", ["geometry/material/texture", "sector/BSP", "pipeline and camera"]),
        ("viewport_hd", "Viewport and HD presentation", "Xbox", "The actual Xbox setup supplies measured 1280x720/720x480/640x480 branches and widescreen parameters; it is the bounded HD reference. This is a reference choice, not adoption or visual parity.", "retail_renderer_cameras", "Keep logical 640x448 coordinates, target resolution and output presentation separate; movie aspect/timing and simulation remain independent.", ["video flags", "projection/aspect", "native target/presentation", "timing independence"]),
    ]
    comparisons = []
    for key, name, route, reason, native, layout, dependencies in groups:
        candidates = [row for row in mapping if key in row["capabilities"]]
        native_path = fork/("runtime/src/native/"+native+".cpp")
        if not native_path.exists() and key == "matrix_transforms":
            native_path = fork/"runtime/src/native/retail_renderer_transforms.cpp"
        native_data = native_path.read_bytes() if native_path.is_file() else None
        ps_functions = sorted({row["ps2_address"] for row in candidates})
        xb_functions = sorted({row["xbox_address"] for row in candidates})
        if key == "viewport_hd":
            xb_functions += ["002695f0", "002a37cb"]
        comparisons.append({"capability": key, "name": name, "ps2_vas": ps_functions, "xbox_vas": xb_functions,
                            "ps2_instruction_count_in_measured_sets": sum(measure("ps2", a)["instruction_count"] for a in ps_functions),
                            "xbox_instruction_count_in_measured_sets": sum(measure("xbox", a)["instruction_count"] for a in xb_functions),
                            "count_limit": "Bounded sets with each function's count_scope; totals are not full subsystem instruction totals or estimates of effort.",
                            "layout_evidence_and_limits": layout, "dependencies_and_platform_boundaries": dependencies,
                            "easier_route": route, "measured_reason": reason,
                            "existing_native": {"path": str(native_path.relative_to(fork)).replace("\\", "/") if native_data else None,
                                                "sha256": sha(native_data) if native_data else None,
                                                "stage": "existing compiled/tested bounded component; no new implementation in this work",
                                                "verification_level": "static for this comparison; prior component tests do not make this cross-platform identification differential"},
                            "adoption": "owner decision required after complete per-capability evidence; no code adopted",
                            "unresolved": "Whole capability layouts/consumers, native connected draw and independent behavior/visual comparisons remain open."})
        if native_data:
            pins["fork/"+str(native_path.relative_to(fork)).replace("\\", "/")] = sha(native_data)
    native_sources = sorted((fork/"runtime/src/native").glob("retail_renderer_*.cpp"))
    gs_sources = [fork/"runtime/src/gs"/name for name in ["native_renderer.cpp", "native_gl_backend.cpp", "gif.cpp", "gs.cpp", "raster.cpp"]]
    route_measurement = {"native_renderer_cpp_files": len(native_sources),
                         "native_renderer_cpp_lines": sum(len(path.read_bytes().splitlines()) for path in native_sources),
                         "gs_comparison_cpp_files": len(gs_sources),
                         "gs_comparison_cpp_lines": sum(len(path.read_bytes().splitlines()) for path in gs_sources),
                         "registered_modules": len(xbox_rows), "original_ctor_dtor_candidates": len(xbox_rows)*2,
                         "scope": "Regenerated source file/line inventories, not equivalent-function totals or effort estimates."}
    for path in native_sources+gs_sources:
        pins["fork/"+str(path.relative_to(fork)).replace("\\", "/")] = sha(path.read_bytes())
    aot = read_json(parent/"research/ledger/aot_coverage_inventory.json")
    aot_summary = {"in_scope_static_callable_entries": sum(row["in_scope_static_aot_count"] for name,row in aot["modules"].items() if name != "NETR.ovl"),
                   "catalogue_descriptor_rows": len(aot["nonfunction_catalogue_records"]),
                   "table_sha256": aot["entry_measurement"]["table_sha256"],
                   "published_renderer_callbacks": published_scope["route_measurements"]["original_ctor_dtor_callable_joins"],
                   "scope": "Tracked authenticated AOT inventory and published renderer joins; this analyzer does not rerun the AOT generator or award library ownership/native credit."}
    librw = parent.parent/"forks/librw"
    if (librw/"LICENSE").is_file():
        license_data = (librw/"LICENSE").read_bytes()
        assert b"The MIT License (MIT)" in license_data and b"Copyright (c) 2014 aap" in license_data
        librw_evidence = {"commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=librw).decode().strip(),
                          "license": "MIT", "copyright": "2014 aap", "license_sha256": sha(license_data),
                          "inspected": {path: sha((librw/path).read_bytes()) for path in ["src/rwobjects.h", "src/rwbase.h", "src/rwplg.h"]},
                          "scope": "Material/geometry/camera/matrix/plugin structural organization only. No source copied; librw layouts do not establish either retail ABI. Copying would require retaining the license/copyright notice."}
    else:
        # The prior license audit is a tracked work-item value, not a required
        # ignored checkout or an implicit download on another machine.
        librw_evidence = read_json(parent/RECORD).get("capability_comparison", {}).get("librw_structural_reference")
        assert librw_evidence and librw_evidence["license"] == "MIT", "No local or tracked librw license audit"
        librw_evidence = dict(librw_evidence, local_checkout="absent; prior tracked structural/license audit reused")
    result = {"schema": "ps2_xbox_capabilities_v1", "status": "BOUNDED_STATIC_COMPARISON_NO_ADOPTION",
              "input_images": {"ps2": {"sha256": sha(elf), "bytes": len(elf)}, "xbox": {"sha256": sha(xbe), "bytes": len(xbe)}},
              "address_contract": "All function/caller fields are virtual addresses. file_offset fields are offsets into the separately mapped original image, never interchangeable with VAs.",
              "xbox_registration": {"body": registration, "rows": xbox_rows}, "renderware_source_ids": ids,
              "identity_anchors": {"heap": [identity_anchor(images["xbox"], literal) for literal in
                                             [b"System Heap\0", b"1.5 rev 1\0", br"\games\mk7xbox\mk7\mwMEM\mwMem.c"+b"\0",
                                              br"\games\mk7xbox\mk7\mwMEM\mwMemHeap.c"+b"\0",
                                              b"memDebugHeap() is unavailable with MEM_DEBUG_BUILD disabled!\n\0"]],
                                   "renderware_build": identity_anchor(images["xbox"], b"Core built at Dec  1 2005 15:24:51\n\0"),
                                   "xbox_game_version": identity_anchor(images["xbox"], b"0.099\0"),
                                   "ps2_game_version": identity_anchor(images["ps2"], b"0.084\0")},
              "identity_limits": "The 85 RW36Active source IDs, heap diagnostics and build label identify compiled library provenance; source-ID order alone is not a function ownership boundary.",
              "function_measurements": measures, "mapping_entries": mapping, "capabilities": comparisons,
              "mapping_coverage": {"candidate_pairs": len(mapping), "catalogued_pairs": sum(row["catalogue_row_exists"] for row in mapping),
                                   "outside_catalogue_ps2_vas": sorted({row["ps2_address"] for row in mapping if not row["catalogue_row_exists"]}),
                                   "policy": "Keep uncatalogued role candidates in the cross-map and report only; do not add native ledger/census rows or denominator credit."},
              "mapping_policy": "Add inferred role candidates through the generator without changing the existing 18 evidence-row validators. No heuristic row authorizes implementation or native credit.",
              "route_recommendation": {"recommendation": "Transform existing generated PS2 renderer/game units onto the existing native GPU and service boundaries. Keep GS/VU decoding as an isolated interim differential route.",
                                        "why_0401_through_0412_are_not_manual_port_work": "All measured registration callbacks already have AOT entries; component lifecycle wrappers do not complete native scene drawing. Preserve behavior while transforming instead of writing another hand-authored duplicate.",
                                        "GS_VU_resolution_limit": "Raising the interim target resolution alone cannot prove required camera/projection/texture/viewport behavior or remove guest GS/VU register/memory identity.",
                                        "original_route_measurements": route_measurement,
                                        "retained_GS_VU_dependencies": ["GS/GIF register and packet protocol", "4 MiB guest GS memory/PSM/framebuffer identities", "VU register/memory ABI", "fixed target/scissor/UV/alpha/depth interpretation", "diagnostic/readback paths distinct from native final drawing"],
                                        "asset_format_risks": published_scope["asset_format_evidence"],
                                        "AOT_scope": {"registered_modules": len(xbox_rows), "registered_callback_pairs": len(xbox_rows)*2,
                                                      "tracked_inventory": aot_summary,
                                                      "callback_scope": "Published renderer joins already authenticate the callback AOT entries; callback totals are not complete RenderWare code coverage."}},
              "librw_structural_reference": librw_evidence,
              "source_pins": dict(pins, **{"parent/tools/research/measure_xbox_capabilities.py": sha(Path(__file__).read_bytes())}),
              "source_value_pins": {"fork/research/runs/renderer-scope.json#instruction_phase6.rw36_xbox_and_ps2_streams.ps2_admissions": sha(json.dumps(published_scope["rw36_xbox_and_ps2_streams"]["ps2_admissions"], sort_keys=True).encode()),
                                    "fork/research/runs/renderer-scope.json#instruction_phase6.asset_format_evidence": sha(json.dumps(published_scope["asset_format_evidence"], sort_keys=True).encode()),
                                    "parent/research/ledger/aot_coverage_inventory.json#selected_AOT_fields": sha(json.dumps(aot_summary, sort_keys=True).encode())},
              "instruction_producer_schema": "Saved Ghidra listed-instruction counts remain distinct from Capstone's optional WAIT-prefix split for exact observed FNSTCW/FNSAVE encodings. No original byte checks or span-count checks were waived.",
              "Not verified": ["General cross-platform functional/numeric equivalence", "Complete native renderer/asset layouts", "Whole-game native acceptance"],
              "Contradictions with earlier documents": ["The older 0409 implementation note names PS2 001a4a80/001a4ac0; authentic registration and current census instead bind 001c4a80/001c4ac0.", "Module lifecycle components and optional geometry design fields do not establish a complete native draw path."],
              "Falsification attempts": []}
    return result


def self_test(report, parent):
    decoder = disassembler()
    image = Image((parent/"private-inputs/xbox-mka-20261121/default.xbe").read_bytes(), "xbox")
    original = image.data
    rows, registration = xbox_registrations(image, decoder)
    # Mutate the actual 0405 ID immediate to duplicate 0406 in a private in-memory
    # bytearray. The original file is never opened for writing.
    altered = bytearray(original)
    at = image.mapping(0x2c00f5, 5)[0]
    assert altered[at:at+5] == bytes.fromhex("6805040000")
    altered[at+1:at+5] = struct.pack("<I", 0x406)
    try:
        xbox_registrations(Image(bytes(altered), "xbox"), decoder)
    except AssertionError:
        pass
    else:
        raise AssertionError("Duplicate plugin ID mutation admitted")
    try:
        verify_original_identity(bytes(altered), XBE_SHA)
    except AssertionError:
        pass
    else:
        raise AssertionError("Changed original image identity admitted")
    changed = copy.deepcopy(report["mapping_entries"])
    changed[0]["native_verification_level"] = "differential"
    try:
        validate_mapping_entries(changed)
    except AssertionError:
        pass
    else:
        raise AssertionError("Identification-only differential promotion admitted")
    images = {"xbox": image, "ps2": Image((parent/"private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/SLUS_214.10").read_bytes(), "ps2")}
    changed = copy.deepcopy(report["mapping_entries"])
    changed[0]["xbox_address"] = f"{changed[0]['xbox_file_offset']:08x}"
    try:
        validate_mapping_entries(changed, images)
    except AssertionError:
        pass
    else:
        raise AssertionError("Mapping file offset admitted as a VA")
    changed_xbe = bytearray(original)
    changed_xbe[image.mapping(0x2c3fc0,1)[0]] ^= 1
    try:
        joined_functions(parent/"external/forks/ps2-native-recomp",
                         dict(images, xbox=Image(bytes(changed_xbe),"xbox")), decoder)
    except AssertionError:
        pass
    else:
        raise AssertionError("Changed saved-listing original bytes admitted")
    try:
        decode_saved_span(decoder, b"\x90\x90", 0x1000)
    except AssertionError:
        pass
    else:
        raise AssertionError("Arbitrary grouped instructions admitted as a legacy WAIT-prefix span")
    for row in report["renderware_source_ids"][:1]:
        offset, va = row["file_offset"], int(row["va"],16)
        assert image.raw(va, 8) == original[offset:offset+8]
        assert image.raw(offset, 8) != original[offset:offset+8], "File offset falsely accepted as VA"
    return ["actual registration duplicate-ID mutation refused", "changed original image identity refused",
            "identification-only native differential promotion refused", "mapping file offset used as VA refused",
            "changed saved-listing original bytes refused", "arbitrary grouped-instruction span refused",
            "source-ID file offset used as VA selects different original bytes"]


def publish_readme(parent, result):
    path = parent/READ_ME
    text = path.read_text(encoding="utf-8")
    text = text.replace("[Phase 6 work record](../phase4-aot-completion-20261124/VERIFICATION.json)",
                        "[capability comparison](PUBLICATION.json)")
    counts = result["route_recommendation"]["original_route_measurements"]
    mapping = result["mapping_coverage"]
    lines = ["<!-- CAPABILITY_COMPARISON:START -->", "## Measured capability comparison — static evidence",
             "", "The generated `capability_comparison` in [PUBLICATION.json](PUBLICATION.json) contains "
             f"{len(result['capabilities'])} capability rows, {len(result['function_measurements'])} measured function sets, "
             f"and {mapping['candidate_pairs']} additional PS2/Xbox role pairs. "
             f"{len(mapping['outside_catalogue_ps2_vas'])} pairs have PS2 starts outside the raw catalogue: "
             "they belong only in the cross-map/report and add no native ledger rows, census entries or credit. "
             "Every pair remains inferred, has native verification `none`, and bars adoption. "
             "The existing 18 mapping validations remain separate.", "",
             "Instruction totals below cover the explicitly reported saved listing, authenticated byte "
             "interval or bounded direct-flow sets. They are not complete subsystem totals, semantic "
             "equivalence, or estimates of implementation effort. Full addresses, separate file offsets, "
             "byte hashes, calls, layout limits and native-source pins are in the generated object.", "",
             "| Capability | PS2 measured instructions | Xbox measured instructions | Easier route within measured scope | Measured reason |",
             "| --- | ---: | ---: | --- | --- |"]
    for row in result["capabilities"]:
        lines.append(f"| {row['name']} | {row['ps2_instruction_count_in_measured_sets']} | {row['xbox_instruction_count_in_measured_sets']} | {row['easier_route']} | {row['measured_reason']} |")
    lines += ["", "The chunk/header/registry candidates are independently rejoined to the original "
              "images. Xbox `002b97a0` and PS2 `0019f1e0` both enforce unsigned versions "
              "`0x34000..0x36003`; Xbox `002c24b0` and PS2 `001a2bc0` use extension type 3, "
              "header-read/skip dependencies, payload-plus-12 accounting and read/always callbacks. "
              "These matched operations and call shapes are E3/E5 identification, not a differential. "
              "The Xbox function's summed Ghidra size is not used as a contiguous byte interval; "
              "direct-control-flow decoding preserves its separate original ranges.", "",
              f"Current source inventory: {counts['native_renderer_cpp_files']} native renderer C++ files / "
              f"{counts['native_renderer_cpp_lines']} lines versus {counts['gs_comparison_cpp_files']} GS "
              f"comparison files / {counts['gs_comparison_cpp_lines']} lines. "
              f"The original registration has {counts['registered_modules']} module rows and "
              f"{counts['original_ctor_dtor_candidates']} constructor/destructor roles; "
              "the tracked AOT inventory and earlier authenticated joins already cover these callbacks. "
              "This does not establish a complete RenderWare ownership total or native scene renderer.", "",
              "Continue transforming the existing generated PS2 units onto the existing native file, "
              "allocation, upload, sampler, fence and GPU boundaries. Modules 0401–0412 describe "
              "behavior/dependencies already represented in AOT; their typed conversion remains necessary. "
              "Rewriting those retail lifecycles by hand creates duplicates. GS/VU packet translation "
              "can exercise the current producers as an interim comparison route, but changing its target "
              "resolution alone retains guest GS/PSM/VRAM and VU state and does not establish correct "
              "projection, geometry, material or asset interpretation. PS2 packed textures, platform "
              "raster headers, skin/morph/sector layouts, alpha roles and movie timing remain separate risks.", "",
              "Xbox's original video setup is a bounded HD reference; it supplies 1280×720, 720×480 "
              "and 640×480 branches. Rendering, logical UI coordinates, movie aspect/timing and simulation "
              "remain independent. No Xbox algorithm is adopted by this report.", "",
              "The clean librw checkout's MIT license (copyright 2014 aap) was inspected at the commit "
              f"recorded in the generated object, `{result['librw_structural_reference']['commit']}`. "
              "Camera, material, geometry, matrix and plugin structures were read only for organization; "
              "no implementation was copied, and those structures do not prove either retail ABI.", "",
              "Regenerate from the existing hash-pinned local images and tracked evidence:", "",
              "```powershell", "python -B tools/research/measure_xbox_capabilities.py --publish", "```", "",
              "The analyzer uses the already installed Capstone x86 decoder. It regenerates PS2 "
              "registrations from the tracked parser, obtains seed extents by matching tracked "
              "original-body hashes, and remeasures current source inventories. Ignored historical "
              "telemetry and generated AOT C++ are not analyzer inputs. A missing librw checkout reuses "
              "the explicitly recorded tracked license audit without fetching or copying source.", "",
              "Not verified: full function/subsystem coverage where bounds are stated as partial, "
              "cross-platform numerical/functional equivalence, complete asset layouts, native scene "
              "drawing and whole-game acceptance remain OPEN.", "",
              "Contradictions with earlier documents: the old 0409 note's `001a4a80/001a4ac0` "
              "addresses are not the authentic registered constructors/destructors, which are "
              "`001c4a80/001c4ac0`. The earlier Phase 4 link has been replaced with this work item's "
              "actual capability record. Summed Ghidra body sizes are not contiguous intervals.", "",
              "Falsification attempts: " + "; ".join(result["Falsification attempts"]) + ".", "<!-- CAPABILITY_COMPARISON:END -->", ""]
    block = "\n".join(lines)
    pattern = r"<!-- CAPABILITY_COMPARISON:START -->.*?<!-- CAPABILITY_COMPARISON:END -->\n?"
    if re.search(pattern, text, flags=re.S):
        text = re.sub(pattern, lambda match: block, text, flags=re.S)
    else:
        first, rest = text.split("\n", 1)
        text = first+"\n\n"+block+rest
    path.write_text(text, encoding="utf-8", newline="\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    default_parent = next((p for p in Path(__file__).resolve().parents
                           if (p/"external/forks/ps2-native-recomp").is_dir()), Path(__file__).resolve().parents[2])
    parser.add_argument("--parent-root", type=Path, default=default_parent)
    parser.add_argument("--fork-root", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--publish", action="store_true")
    args = parser.parse_args()
    parent = args.parent_root.resolve()
    fork = args.fork_root.resolve() if args.fork_root else parent/"external/forks/ps2-native-recomp"
    result = build(parent, fork)
    result["Falsification attempts"] = self_test(result, parent)
    data = json.dumps(result, indent=2)+"\n"
    if args.output:
        assert args.output.resolve().is_relative_to(fork/"build/oracle-telemetry")
        args.output.write_text(data, encoding="utf-8", newline="\n")
    if args.publish:
        path = parent/RECORD
        record = read_json(path)
        record["capability_comparison"] = result
        for section in ("Not verified", "Contradictions with earlier documents", "Falsification attempts"):
            record.pop(section, None)
            record[section] = result[section]
        path.write_text(json.dumps(record, indent=2)+"\n", encoding="utf-8", newline="\n")
        publish_readme(parent, result)
    print(json.dumps({"status": result["status"], "capabilities": len(result["capabilities"]),
                      "mapping_candidates": len(result["mapping_entries"]), "measured_functions": len(result["function_measurements"]),
                      "report_sha256": sha(data.encode()), "falsification_attempts": result["Falsification attempts"]}))


if __name__ == "__main__":
    main()
