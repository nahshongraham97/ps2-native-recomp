"""Retail renderer and stream functions with cross-version source evidence.

The pinned SDK bodies identify roles by their control flow and operations. The
retail body and its original diagnostic call are both checked independently of
the reference build's addresses and the similarity search.
"""
import hashlib
import struct
from pathlib import Path


SDK_REVISION = "1a8f7da4c58f6725cb73fbe71a8803fce9e0c752"
SOURCE_FILES = {
    "babinary.c": "779f7efde3e2bba6a255d7965ab76227aadf60ce164ea52f23a8f271c8b9fc94",
    "batkbin.c": "0941df5e6a27b29ec86cb5cd1acf9f21eb232d59cd8d86f64771c9ad5549f10f",
    "badevice.c": "3d9e69ef67917f780a1df604a933833e3a3b0a27b1c085c7766eb57324951639",
    "baframe.c": "47ee0bdf20bbae224bfaa0b1808ee8fa9d805d12d1e09ac382cd5c38f37e7536",
    "bamatrix.c": "6a65d8140fcddf75bf44bf0b3daf973d0c6d55c0ff6b866ec15ccfe8f256c751",
    "bacamera.c": "66282a6f54d42804dbadee9e8c1a6cf032bbe927ec5677f19ed2678fce978fd8",
    "baresour.c": "340976c2a9d15503737a1897c6b63ba6371170afb60f44890d537b35d0014027",
}
# start, exclusive end, error argument setup, reference name, whole retail body
MATRIX_ROLES = (
    (0x1a04d0, 0x1a06ac, 0x1a0674, "RwMatrixRotateOneMinusCosineSine",
     "eeb4d6c9bb12e6dac442873917f2e7a0a5c61806255e7174ce32957f2d7d7ac2",
     "Axis products with sine and one-minus-cosine form a rotation matrix before the combine-mode dispatch."),
    (0x1a0900, 0x1a0b28, 0x1a0ae0, "RwMatrixScale",
     "fd77365890d35e0bc042d6ea29c23f54ba351995e298b619115b623ee2ea07d7",
     "Combine modes scale components versus basis vectors, or construct the replacement diagonal."),
    (0x1a0b30, 0x1a0cc8, 0x1a0c80, "RwMatrixTranslate",
     "0e0e50ad931cdc7bd4236bebe580eea14abd77684fd9842df163ca38edd67a45",
     "Combine modes add translation directly, multiply it by the basis, or set identity and translation."),
    (0x1a0cd0, 0x1a0de8, 0x1a0db0, "RwMatrixTransform",
     "62f978b45cf3beeb12c1d6705f9a5152c1ef3e096ce30c02737432652a88ef7e",
     "Combine modes swap the matrix-product operands or copy the replacement matrix."),
)
CAMERA_ROLE = (0x1a6410, 0x1a649c, 0x1a6468, "RwCameraSetProjection",
               "4d308fe0e012e274fc02ef2d44caa0f7a7577bd0c0e7e6fa67136cef154ef313",
               "Modes 1 and 2 store projection, conditionally update the frame and update depth. An invalid mode reports an error and returns null.")


def validate_resource_arena_semantics(original, source):
    """Original source/dataflow witness for the single precision-audit correction.

    Reference SDK3.1 identifies the resource-arena role; MKA's allocation-hint
    argument is retained as a version difference. Code-carried global/field
    references are checked without reading or assuming live BSS contents.
    """
    marker = b'RWAPIFUNCTION(RWSTRING("RwResourcesSetArenaSize"))'
    if source.count(marker) != 1:
        raise ValueError("Missing unique SDK resource-arena API marker")
    body = source.split(marker)[1].split(b"\n}", 1)[0]
    operations = (
        b"resourcesModule.numInstances == 0", b"rwResourcesOverrideDefaultArenaSize = size",
        b"res->maxSize = size", b"RwResourcesEmptyArena()", b"_rwResHeapClose(res->memHeap)",
        b"RwFree(res->memHeap)", b"res->memHeap = RwMalloc(size)",
        b"_rwResHeapInit(res->memHeap, size)", b"E_RW_NOMEM", b"E_RW_INSUFFICIENTRESOURCES",
    )
    if any(operation not in body for operation in operations):
        raise ValueError("SDK resource-arena source operation changed")
    words = (
        (0x1a2034, 0x8f82a3a4, "resource module instance state"),
        (0x1a2038, 0x14400005, "pre-open versus live arena branch"),
        (0x1a203c, 0x0080902d, "retain requested size"),
        (0x1a204c, 0xac72f3e4, "pre-open default-size override"),
        (0x1a2050, 0x8f83a3a0, "resource globals offset"),
        (0x1a2054, 0x3c02006a, "engine globals base high"),
        (0x1a2058, 0x2442f290, "engine globals base low"),
        (0x1a205c, 0x00438821, "select resource global data"),
        (0x1a2060, 0x0c06884c, "empty old arena"),
        (0x1a2064, 0xae320000, "max-size publication"),
        (0x1a2068, 0x0c068f10, "close old resource heap"),
        (0x1a206c, 0x8e24000c, "old heap field at offset12"),
        (0x1a207c, 0x0040f809, "free old heap callback"),
        (0x1a2080, 0x8e24000c, "pass old heap to free"),
        (0x1a2084, 0x3c020003, "retail allocation hint high"),
        (0x1a2088, 0x3445040b, "retail resources-module allocation hint"),
        (0x1a2094, 0x0040f809, "allocate replacement heap callback"),
        (0x1a2098, 0x0240202d, "replacement allocation uses requested size"),
        (0x1a20a0, 0xae22000c, "publish replacement heap at offset12"),
        (0x1a20b4, 0x34440013, "out-of-memory error"),
        (0x1a20bc, 0xae200000, "failed allocation clears max size"),
        (0x1a20d4, 0x0240282d, "heap initialization uses requested size"),
        (0x1a20d8, 0x0c068ef8, "initialize replacement resource heap"),
        (0x1a20dc, 0x0040202d, "initialize exact replacement heap"),
        (0x1a20ec, 0x0040f809, "free replacement heap after init failure"),
        (0x1a20f8, 0x2404000c, "insufficient-resources error"),
    )
    witnesses = []
    for address, expected, operation in words:
        raw = original(address, 4)
        if raw != struct.pack("<I", expected):
            raise ValueError(f"Resource arena semantic mismatch at {address:08x} ({operation})")
        witnesses.append({"address": f"{address:08x}", "word_le": raw.hex(), "operation": operation})
    return {"original_semantic_words": witnesses,
            "source_operations": [operation.decode("ascii") for operation in operations],
            "data_layout": {"maxSize_offset": 0, "memHeap_offset": 12,
                            "global_reference_limit": "Original load/store instructions only; live BSS values are not claimed."},
            "version_limit": "Pinned SDK3.1 source role; retail uses an additional memory-hint argument. No native/differential equivalence is claimed."}


def matched_sdk_roles(original, sdk_root: Path):
    sources = {}
    for name, expected in SOURCE_FILES.items():
        data = (sdk_root / "rwsdk/src" / name).read_bytes().replace(b"\r\n", b"\n")
        if hashlib.sha256(data).hexdigest() != expected:
            raise ValueError(f"Expected SDK {SDK_REVISION} source identity for {name}")
        sources[name] = data

    results = []
    for start, end, setup, api, expected_hash, operations in (*MATRIX_ROLES, CAMERA_ROLE):
        camera = start == CAMERA_ROLE[0]
        file = "bacamera.c" if camera else "bamatrix.c"
        message = b"Invalid projection type specified" if camera else b"Invalid combination type"
        source = sources[file]
        marker = b'RWAPIFUNCTION(RWSTRING("' + api.encode("ascii") + b'"))'
        if source.count(marker) != 1:
            raise ValueError(f"Missing unique SDK API marker for {api}")
        function = source.split(marker)[1].split(b"\n}", 1)[0]
        if message not in function:
            raise ValueError(f"Diagnostic outside reference function {api}")
        if hashlib.sha256(original(start, end - start)).hexdigest() != expected_hash:
            raise ValueError(f"Retail role body changed at {start:08x}")
        words = struct.unpack("<7I", original(setup, 28))
        lo = words[4 if camera else 2]
        # The camera completes a1 in the JAL delay slot. The matrices materialize
        # it before that call. All subsequently call the same error publisher.
        if words[0] >> 16 != 0x3c05 or lo >> 16 != 0x24a5 or words[3] != 0x0c067d44 or words[6] != 0x0c067d18:
            raise ValueError(f"Original error call or argument setup changed at {setup:08x}")
        low = lo & 0xffff
        address = ((words[0] & 0xffff) << 16) + (low - 0x10000 if low & 0x8000 else low)
        if original(address, len(message) + 1) != message + b"\0":
            raise ValueError(f"Original diagnostic changed at {address:08x}")
        results.append((start, end, {"kind": "sdk_source_role_and_original_diagnostic",
                                     "source_role": api, "source_file": "rwsdk/src/" + file,
                                     "source_normalized_sha256": SOURCE_FILES[file],
                                     "original_body_sha256": expected_hash,
                                     "diagnostic_address": f"{address:08x}", "operations": operations}))
    # These roles have an identified caller AND a separately checked operation
    # in the callee. A JAL target or an above-threshold BSim score alone cannot
    # establish what a helper does.
    connected = (
        (0x19f0b0, 0x19f1dc, "_rwStreamReadChunkHeader", "babinary.c",
         "cb25996eb63b13497cb309deb97b36ce6dd5463598f29c0eb3d441233b92e0c3",
         [0x19f214, 0x19f2f8, 0x1a2c48],
         "Requires a 12-byte read. Decodes the old and packed library IDs and conditionally "
         "stores type, length, version and build. The retail build output extends the 3.1 reference ABI."),
        (0x19f1e0, 0x19f328, "RwStreamFindChunk", "babinary.c",
         "321bf1b926b4a272899bd7ef8b4b00ebc7282183caeb93cc387a6cd8ee639a53",
         [0x1a2bec],
         "Reads headers, skips the payload of each nonmatching type, then repeats. A matching type "
         "requires an unsigned version from 0x34000 through 0x36003 before length and version are returned."),
        (0x1a2bc0, 0x1a2d84, "_rwPluginRegistryReadDataChunks", "batkbin.c",
         "2e00688d1e437ea46bf97acb96c1c31f4e96da68d21371bf30dc05d2b45d0429",
         [0x1a4b34, 0x1a4e70],
         "Finds the extension chunk (type 3), dispatches nested chunks by registered ID or skips "
         "their payloads, subtracts each payload plus 12 bytes, then invokes registered always callbacks."),
        (0x1a0440, 0x1a04bc, "RwMatrixMultiply", "bamatrix.c",
         "01748f4a8196e4758275d1d9aa936178bd2826f70fb496a8338c2e9f4266a38e",
         [0x1a05f4, 0x1a0638, 0x1a0d30, 0x1a0d74],
         "Two operand orders in both rotate and transform call the same affine product. The callee combines four vector rows and intersects input flags."),
        (0x1a7510, 0x1a7578, "RwFrameUpdateObjects", "baframe.c",
         "624ddc6f40f31d001aa72a3b386aa2ce2754f08918b32646261f254348ac5906",
         [0x1a644c],
         "Projection updates its parent frame. The callee links a clean root into the dirty list and sets root and subtree flags."),
        (0x1a4f00, 0x1a4fb4, "CameraUpdateZShiftScale", "bacamera.c",
         "9002768dad2f8f67cd5c860eb4e8fbabb83ff089a7693863b1a0c194bf865b18",
         [0x1a6458],
         "Projection changes recompute the two depth coefficients from near and far planes using reciprocal depth in perspective mode."),
        (0x1a64a0, 0x1a650c, "RwCameraSetViewWindow", "bacamera.c",
         "c27ee861601bb38263e7ee430287b8ca18c9d65e6807fc158eb6feb73cadb398",
         [0x126174, 0x17f0c0, 0x1815d8, 0x21ca50, 0x21d314],
         "Stores both window components and their reciprocals, updates a non-null parent frame "
         "through 001a7510, and returns the camera on both branches."),
        (0x1a2020, 0x1a2130, "RwResourcesSetArenaSize", "baresour.c",
         "2ba736d40c4bf1d56d1bef12714117164632ee50c16d3e3783e89e94096413e3",
         [0x33e338, 0x44e464],
         "Before resource-module open, overrides the default arena size. Otherwise sets maxSize, empties and closes the old heap, "
         "frees it, allocates and initializes the replacement heap, and reports distinct allocation/initialization errors."),
    )
    for start, end, api, file, body_hash, sites, operations in connected:
        semantic_evidence = (validate_resource_arena_semantics(original, sources[file])
                             if start == 0x1a2020 else {})
        if hashlib.sha256(original(start, end - start)).hexdigest() != body_hash:
            raise ValueError(f"Connected helper body changed at {start:08x}")
        if f'RWSTRING("{api}")'.encode() not in sources[file]:
            raise ValueError(f"Missing SDK helper {api}")
        for site in sites:
            if original(site, 4) != struct.pack("<I", 0x0c000000 | (start >> 2)):
                raise ValueError(f"Unexpected callee at {site:08x}")
        results.append((start, end, {"kind": "sdk_role_and_verified_retail_callee", "source_role": api,
                                     "source_file": "rwsdk/src/" + file, "source_normalized_sha256": SOURCE_FILES[file],
                                     "original_body_sha256": body_hash, "operations": operations,
                                     "caller_sites": [f"{site:08x}" for site in sites], **semantic_evidence}))

    # Original device fallback jump table for requests 13..18. The zero-index
    # subsystem-info case passes this literal in the delay slot of its copy call.
    start, end = 0x1a68f0, 0x1a6a04
    body_hash = "1806875d03b532f009ebb5bda0f6f8bb64c5f31134ca0929416b0bf0f19a5f2d"
    message = b"Only rendering sub system"
    if (hashlib.sha256(original(start, end - start)).hexdigest() != body_hash
            or original(0x5a36a0, len(message) + 1) != message + b"\0"
            or struct.unpack("<6I", original(0x5fce90, 24)) != (
                0x1a696c, 0x1a6978, 0x1a69a8, 0x1a69b4, 0x1a6964, 0x1a6964)
            or message not in sources["badevice.c"]):
        raise ValueError("Device-system fallback body, table or diagnostic changed")
    results.append((start, end, {"kind": "sdk_device_request_and_retail_switch",
                                 "source_role": "_rwDeviceSystemRequest", "source_file": "rwsdk/src/badevice.c",
                                 "source_normalized_sha256": SOURCE_FILES["badevice.c"], "original_body_sha256": body_hash,
                                 "operations": "The device callback runs first. Six fallback requests select count, name, current and selected subsystem or finalization defaults."}))
    return results
