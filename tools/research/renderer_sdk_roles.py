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
    "batypehf.c": "1359c0747ebf269e47dbc17009bd222634d2f252557f4174ed18acea85493935",
    "bamatrix.c": "6a65d8140fcddf75bf44bf0b3daf973d0c6d55c0ff6b866ec15ccfe8f256c751",
    "bacamera.c": "66282a6f54d42804dbadee9e8c1a6cf032bbe927ec5677f19ed2678fce978fd8",
    "baresour.c": "340976c2a9d15503737a1897c6b63ba6371170afb60f44890d537b35d0014027",
    "resmem.c": "bacc01b37e6c796117f8112d8cc758233710c58052fbd32adf325889358cbc69",
}
HEAP_CLOSE_HEADERS = {
    "rwsdk/src/resmem.h": "90f4fca3c0284a49c97da3b261e3dbf092196c2cdc956f93dadbe49265e9a45d",
    "rwsdk/os/sky/ostypes.h": "a83ffefb17e206ba34cb9f44b9d1e8827f581337895d2f9014063c26224d90a8",
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

CAMERA_FRAME_HEADERS = {
    "batypehf.h": "009f810477cb58522182ddcdca62b799b1ccfb9333a502b535f7dfba38adaf14",
    "batype.h": "96b352759d892b828f60de801169b8d4394bb124e3427b536a7d5af62bd653b7",
    "batypes.h": "d1909c78e4711397fd21ab1a107199089fa0f6f3ea59fc9fdb546a434a8470ce",
    "baframe.h": "d27babb43789943c4b348f8012958f7820ee52a15fc17982d262bbd96aff6f4e",
    "bacamera.h": "fb035c55a3bf827c12ab4b5556c1985a766119ab499f9104d3bb2bb6613c1234",
}
CAMERA_FRAME_BODIES = {
    0x1aa0b0: (0x1aa11c, "d1df69db48be32aa568ccabf299bfd146172d7ca4694b4925bc527003e5aee2b"),
    0x1a7510: (0x1a7578, "624ddc6f40f31d001aa72a3b386aa2ce2754f08918b32646261f254348ac5906"),
}
# Complete original instruction sequences, including branch-likely and JR/JAL
# delay slots. Their positions retain the source reload/alias and store order.
CAMERA_FRAME_WORDS = {
    0x1aa0b0: (
        0x27bdfff0, 0xffbf0000, 0x8c830004, 0x50600008, 0xac850004,
        0x8c860008, 0x8c83000c, 0xac660000, 0x8c86000c, 0x8c830008,
        0xac660004, 0xac850004, 0x50a0000c, 0xdfbf0000, 0x8ca60090,
        0x24a20090, 0x24830008, 0xac860008, 0xac82000c, 0x8ca20090,
        0x00a0202d, 0xac430004, 0x0c069d44, 0xaca30090, 0xdfbf0000,
        0x03e00008, 0x27bd0010),
    0x1a7510: (
        0x8c8300a0, 0x90660003, 0x30c20003, 0x5440000f, 0x8c8300a0,
        0x3c05006a, 0x24a5f34c, 0x8ca20000, 0xac620008, 0x8c8200a0,
        0xac45000c, 0x8c8300a0, 0x8ca20000, 0x24630008, 0xac430004,
        0x8c8200a0, 0x24420008, 0xaca20000, 0x8c8300a0, 0x34c50003,
        0x0080102d, 0xa0650003, 0x90830003, 0x3463000c, 0x03e00008,
        0xa0830003),
}
CAMERA_FRAME_CALLER_BODIES = (
    (0x17ebb0, 0x17ecd4, "6ddcc305541e227971b6974de36533ee35e96cae52a6936293c5c2d70fdd9c2b"),
    (0x17ee30, 0x17ef44, "c8896a0c2f3c74fd6add7b8f31d4f7b91eb66c15098a47e4a4116f0db6e5c79f"),
    (0x1a64a0, 0x1a650c, "c27ee861601bb38263e7ee430287b8ca18c9d65e6807fc158eb6feb73cadb398"),
)


def validate_camera_frame_semantics(original, sources, headers, start):
    """Bounded E1 candidate: original operations AND SDK body/ABI AND callers.

    No ownership seed list, ledger or native implementation is modified here.
    SDK offsets are not imported into a native layout or a different build.
    """
    if start not in CAMERA_FRAME_BODIES:
        raise ValueError("camera_frame_unknown_role")
    for name in ("batypehf.c", "baframe.c"):
        if hashlib.sha256(sources[name]).hexdigest() != SOURCE_FILES[name]:
            raise ValueError("camera_frame_SDK_source_identity_changed:" + name)
    for name, pin in CAMERA_FRAME_HEADERS.items():
        if hashlib.sha256(headers[name]).hexdigest() != pin:
            raise ValueError("camera_frame_SDK_header_identity_changed:" + name)
    compact = lambda text: b"".join(text.replace(b"\\\n", b"").split())
    declarations = compact(headers["batypehf.h"])
    if (b"externvoid_rwObjectHasFrameSetFrame(void*object,RwFrame*frame);" not in declarations
            or b"RwObjectobject;RwLLLinklFrame;" not in declarations
            or b"RwCameraSetFrameMacro(_camera,_frame)" not in compact(headers["bacamera.h"])):
        raise ValueError("camera_frame_SDK_void_ABI_or_object_relationship_changed")
    file, api = ("batypehf.c", "_rwObjectHasFrameSetFrame") if start == 0x1aa0b0 else ("baframe.c", "RwFrameUpdateObjects")
    marker = b'RWSTRING("' + api.encode("ascii") + b'")'
    if sources[file].count(marker) != 1:
        raise ValueError("camera_frame_SDK_unique_role_marker_changed")
    body = compact(sources[file].split(marker, 1)[1].split(b"\n}", 1)[0])
    operations = (
        b"if(rwObjectGetParent(ohf))", b"rwLinkListRemoveLLLink(&ohf->lFrame);",
        b"rwObjectSetParent(object,frame);", b"if(frame)",
        b"rwLinkListAddLLLink(&frame->objectList,&ohf->lFrame);",
        b"RwFrameUpdateObjects(frame);", b"RWRETURNVOID();",
    ) if start == 0x1aa0b0 else (
        b"oldFlags=rwObjectGetPrivateFlags(frame->root);",
        b"if(!(oldFlags&(rwFRAMEPRIVATEHIERARCHYSYNCLTM|rwFRAMEPRIVATEHIERARCHYSYNCOBJ)))",
        b"rwLinkListAddLLLink(&RWSRCGLOBAL(dirtyFrameList),&frame->root->inDirtyListLink);",
        b"rwObjectSetPrivateFlags(frame->root,oldFlags|",
        b"rwObjectSetPrivateFlags(frame,rwObjectGetPrivateFlags(frame)|", b"RWRETURN(frame);",
    )
    position = 0
    for operation in operations:
        found = body.find(operation, position)
        if found < 0:
            raise ValueError("camera_frame_SDK_operation_or_order_changed:" + operation.decode())
        position = found + len(operation)
    words = []
    for index, expected in enumerate(CAMERA_FRAME_WORDS[start]):
        pc = start + 4 * index
        if original(pc, 4) != struct.pack("<I", expected):
            raise ValueError(f"camera_frame_original_semantics_changed:{pc:08x}")
        words.append({"address": f"{pc:08x}", "word_le": struct.pack("<I", expected).hex()})
    caller_words = (
        (0x17ebd8, 0x0c069968), (0x17ebe0, 0x0040802d),
        (0x17ebec, 0x0c069c74), (0x17ebf4, 0x10400004),
        (0x17ebfc, 0x0040282d), (0x17ec00, 0x0c06a82c), (0x17ec04, 0x0200202d),
        (0x17ee74, 0x8e700004), (0x17ee84, 0x0c06a82c), (0x17ee88, 0x0000282d),
        (0x17ee8c, 0x0c069c90), (0x17ee90, 0x0200202d),
        (0x1a64e4, 0x8c840004), (0x1a64e8, 0x50800004),
        (0x1a64ec, 0x0200102d), (0x1a64f0, 0x0c069d44), (0x1a64f4, 0x00000000),
    )
    for pc, expected in caller_words:
        if original(pc, 4) != struct.pack("<I", expected):
            raise ValueError(f"camera_frame_original_caller_semantics_changed:{pc:08x}")
    for begin, end, pin in CAMERA_FRAME_CALLER_BODIES:
        if hashlib.sha256(original(begin, end-begin)).hexdigest() != pin:
            raise ValueError(f"camera_frame_complete_caller_changed:{begin:08x}")
    end, pin = CAMERA_FRAME_BODIES[start]
    if hashlib.sha256(original(start, end-start)).hexdigest() != pin:
        raise ValueError("camera_frame_complete_body_changed")
    return {"original_semantic_words": words,
            "SDK_header_normalized_pins": CAMERA_FRAME_HEADERS,
            "SDK_source_paths": ["rwsdk/src/" + name for name in ("batypehf.c", "baframe.c")],
            "SDK_source_normalized_pins": {name: SOURCE_FILES[name] for name in ("batypehf.c", "baframe.c")},
            "source_operations_in_order": [operation.decode() for operation in operations],
            "complete_original_callers": [{"entry": f"{a:08x}", "end_exclusive": f"{b:08x}", "sha256": pin}
                                          for a, b, pin in CAMERA_FRAME_CALLER_BODIES],
            "caller_semantic_words": [{"address": f"{pc:08x}", "word_le": struct.pack("<I", w).hex()}
                                      for pc, w in caller_words],
            "return_convention": "VOID; incidental V0 is not an object result" if start == 0x1aa0b0 else "Returns the original frame pointer, not its root",
            "effect_order": "Unlink old attachment; publish nullable parent; insert at new frame object-list head; publish head in JAL delay; mark new frame. Same-frame reassignment still relinks. Null detachment does not mark old frame." if start == 0x1aa0b0 else "Read root flags; if hierarchy clean, insert root dirty link with all four next/previous writes. Then root flags|3 and selected frame flags|12, last store in JR delay. No synchronization callback runs.",
            "original_layout_only": {"object_parent": 4, "object_frame_link": [8, 12], "frame_object_list": [144, 148], "frame_root": 160, "private_flags": 3},
            "limits": "SDK3.1 role/ABI corroboration plus complete retail words and original caller dataflow; no claim of exact SDK version, ownership of callers, production/native binding, generic synchronization or behavioral differential.",
            "ownership_admissions": 0, "implementation_credit_added": 0}


def camera_frame_semantic_controls(original, sources, headers):
    """Falsify fields, branches, link effects, calls and their original order."""
    for start in CAMERA_FRAME_BODIES:
        validate_camera_frame_semantics(original, sources, headers, start)
    mutations = (
        ("old_parent_field", 0x1aa0b0, {0x1aa0b8: 0x8c830008}),
        ("old_parent_branch_likely", 0x1aa0b0, {0x1aa0bc: 0x10600008}),
        ("null_old_parent_delay_store", 0x1aa0b0, {0x1aa0c0: 0}),
        ("unlink_next_effect", 0x1aa0b0, {0x1aa0cc: 0xac660004}),
        ("unlink_previous_effect", 0x1aa0b0, {0x1aa0d8: 0xac660000}),
        ("unlink_store_order", 0x1aa0b0, {0x1aa0cc: 0xac660004, 0x1aa0d8: 0xac660000}),
        ("parent_publication_field", 0x1aa0b0, {0x1aa0dc: 0xac850008}),
        ("null_new_frame_branch", 0x1aa0b0, {0x1aa0e0: 0x54a0000c}),
        ("new_frame_object_list", 0x1aa0b0, {0x1aa0e8: 0x8ca60094}),
        ("insert_link_store_order", 0x1aa0b0, {0x1aa0f4: 0xac82000c, 0x1aa0f8: 0xac860008}),
        ("new_frame_call_argument", 0x1aa0b0, {0x1aa100: 0x0080202d}),
        ("update_call_target", 0x1aa0b0, {0x1aa108: 0x0c069c3c}),
        ("JAL_delay_head_publication", 0x1aa0b0, {0x1aa10c: 0}),
        ("frame_root_field", 0x1a7510, {0x1a7510: 0x8c83009c}),
        ("unsigned_flag_byte", 0x1a7510, {0x1a7514: 0x80660003}),
        ("clean_hierarchy_mask", 0x1a7510, {0x1a7518: 0x30c2000c}),
        ("dirty_root_branch_likely", 0x1a7510, {0x1a751c: 0x1440000f}),
        ("branch_delay_root_reload", 0x1a7510, {0x1a7520: 0}),
        ("dirty_list_global", 0x1a7510, {0x1a7528: 0x24a5f350}),
        ("dirty_next_effect", 0x1a7510, {0x1a7530: 0xac62000c}),
        ("dirty_previous_effect", 0x1a7510, {0x1a7538: 0xac450008}),
        ("dirty_head_backlink", 0x1a7510, {0x1a7548: 0xac430000}),
        ("dirty_list_head_publication", 0x1a7510, {0x1a7554: 0xaca20004}),
        ("root_dirty_bits", 0x1a7510, {0x1a755c: 0x34c5000c}),
        ("selected_frame_return", 0x1a7510, {0x1a7560: 0x0060102d}),
        ("root_then_frame_flag_order", 0x1a7510, {0x1a7564: 0xa0830003, 0x1a7574: 0xa0650003}),
        ("subtree_dirty_bits", 0x1a7510, {0x1a756c: 0x34630003}),
        ("JR_delay_selected_frame_store", 0x1a7510, {0x1a7574: 0}),
        ("producer_camera_argument", 0x1aa0b0, {0x17ec04: 0x0040202d}),
        ("destructor_null_frame_argument", 0x1aa0b0, {0x17ee88: 0x0200282d}),
        ("admitted_view_parent_field", 0x1a7510, {0x1a64e4: 0x8c840008}),
        ("admitted_view_null_branch", 0x1a7510, {0x1a64e8: 0x54800004}),
    )
    controls = []
    for name, start, replacements in mutations:
        def changed(va, size):
            data = bytearray(original(va, size))
            for pc, word in replacements.items():
                low, high = max(va, pc), min(va+size, pc+4)
                if low < high:
                    data[low-va:high-va] = struct.pack("<I", word)[low-pc:high-pc]
            return bytes(data)
        try: validate_camera_frame_semantics(changed, sources, headers, start)
        except ValueError as error: controls.append({"mutation": name, "role": f"{start:08x}", "status": "REJECTED", "reason": str(error)})
        else: raise AssertionError("Camera/frame evidence mutation admitted: " + name)
    for name, file, before, after in (
        ("forged_void_ABI", "batypehf.h", b"extern void _rwObjectHasFrameSetFrame", b"extern int _rwObjectHasFrameSetFrame"),
        ("forged_frame_dirty_bits", "baframe.h", b"rwFRAMEPRIVATEHIERARCHYSYNCLTM  0x01", b"rwFRAMEPRIVATEHIERARCHYSYNCLTM  0x04"),
    ):
        altered = dict(headers); altered[file] = headers[file].replace(before, after)
        if altered[file] == headers[file]: raise AssertionError("No-op source mutation: " + name)
        try: validate_camera_frame_semantics(original, sources, altered, 0x1aa0b0)
        except ValueError as error: controls.append({"mutation": name, "status": "REJECTED", "reason": str(error)})
        else: raise AssertionError("Camera/frame SDK mutation admitted: " + name)
    return {"status": "PASS", "controls": controls, "positive_roles": 2}


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


def validate_heap_close_semantics(original, source, headers):
    """Single E1 evidence addition; no new ownership or implementation rule."""
    if original(0x1a3c40, 8) != struct.pack("<2I", 0x03e00008, 0x24020001):
        raise ValueError("heap_close_complete_original_body_changed")
    if hashlib.sha256(source).hexdigest() != SOURCE_FILES["resmem.c"]:
        raise ValueError("heap_close_SDK_source_identity_changed")
    for name, expected in HEAP_CLOSE_HEADERS.items():
        if hashlib.sha256(headers[name]).hexdigest() != expected:
            raise ValueError("heap_close_SDK_header_identity_changed:" + name)
    signature = b"RwBool\n_rwResHeapClose(void *resHeap __RWUNUSED__)"
    if source.count(signature) != 1:
        raise ValueError("heap_close_unused_pointer_and_integer_return_signature_missing")
    block = source.split(signature, 1)[1].split(b"\n}", 1)[0]
    if block.count(b"RWRETURN(TRUE);") != 1:
        raise ValueError("heap_close_SDK_true_return_missing")
    if (b"extern RwBool   _rwResHeapClose(void *resHeap);" not in headers["rwsdk/src/resmem.h"]
            or b"typedef int RwInt32;" not in headers["rwsdk/os/sky/ostypes.h"]
            or b"typedef RwInt32 RwBool;" not in headers["rwsdk/os/sky/ostypes.h"]):
        raise ValueError("heap_close_RwBool_int32_declaration_missing")
    words = ((0x1a1bb4, 0x0c068f10, "resource-close caller targets this exact leaf"),
             (0x1a1bb8, 0x8c44000c, "caller passes heap field at offset12 in JAL delay"),
             (0x1a2068, 0x0c068f10, "admitted arena caller targets this exact leaf"),
             (0x1a206c, 0x8e24000c, "arena passes old heap field at offset12 in JAL delay"))
    for pc, expected, _ in words:
        if original(pc, 4) != struct.pack("<I", expected):
            raise ValueError("heap_close_original_caller_or_delay_changed")
    caller_bodies = ((0x1a1b90, 0x1a1c10, "16ce0544d22a55583e705449bcf00d05fa8bdca6e4296b3dfa82c61dedaf2382"),
                     (0x1a2020, 0x1a2130, "2ba736d40c4bf1d56d1bef12714117164632ee50c16d3e3783e89e94096413e3"))
    for start, end, expected in caller_bodies:
        if hashlib.sha256(original(start, end-start)).hexdigest() != expected:
            raise ValueError("heap_close_complete_original_caller_body_changed")
    return {"original_semantic_words": [{"address": f"{pc:08x}", "word_le": struct.pack("<I", w).hex(),
                "operation": operation} for pc, w, operation in words],
            "complete_original_callers": [{"entry": f"{start:08x}", "end_exclusive": f"{end:08x}", "sha256": expected}
                for start, end, expected in caller_bodies],
            "SDK_header_pins": HEAP_CLOSE_HEADERS,
            "return_convention": "RwBool is RwInt32/int, not C++ bool. Original ADDIU delay writes signed32 value1 to V0 low64; upper64 and every other register remain unchanged.",
            "version_limit": "SDK3.1 corroborates the role/signature. Retail3.6 behavior is the complete eight-byte JR/ADDIU body, with no memory, logging or external-call effects; SDK MEMORYLOG behavior is not imported.",
            "implementation_credit_added": 0}


def heap_close_semantic_controls(original, source, headers):
    """Six falsifications of this specific body/source/caller evidence join."""
    validate_heap_close_semantics(original, source, headers)
    controls = []
    for name, site, word in (("wrong_return_value", 0x1a3c44, 0x24020000),
            ("wrong_return_target", 0x1a3c40, 0x00400008),
            ("wrong_caller_target", 0x1a2068, 0x0c068f14),
            ("wrong_delay_heap_field", 0x1a206c, 0x8e240010)):
        payload = struct.pack("<I", word)
        def planted(va, size):
            data = bytearray(original(va, size)); begin, end = max(va, site), min(va+size, site+4)
            if begin < end: data[begin-va:end-va] = payload[begin-site:end-site]
            return bytes(data)
        try: validate_heap_close_semantics(planted, source, headers)
        except ValueError as error: controls.append({"mutation": name, "status": "REJECTED", "reason": str(error)})
        else: raise AssertionError("Heap-close evidence mutation admitted: " + name)
    source_plant = source.replace(b"RwBool\n_rwResHeapClose", b"void\n_rwResHeapClose")
    header_plant = dict(headers)
    header_plant["rwsdk/os/sky/ostypes.h"] = headers["rwsdk/os/sky/ostypes.h"].replace(
        b"typedef RwInt32 RwBool;", b"typedef unsigned char RwBool;")
    for name, data, declarations in (("forged_SDK_return_type", source_plant, headers),
                                   ("forged_RwBool_width", source, header_plant)):
        try: validate_heap_close_semantics(original, data, declarations)
        except ValueError as error: controls.append({"mutation": name, "status": "REJECTED", "reason": str(error)})
        else: raise AssertionError("Heap-close source mutation admitted: " + name)
    return {"status": "PASS", "controls": controls}


def matched_sdk_roles(original, sdk_root: Path):
    sources = {}
    for name, expected in SOURCE_FILES.items():
        data = (sdk_root / "rwsdk/src" / name).read_bytes().replace(b"\r\n", b"\n")
        if hashlib.sha256(data).hexdigest() != expected:
            raise ValueError(f"Expected SDK {SDK_REVISION} source identity for {name}")
        sources[name] = data
    heap_headers = {name: (sdk_root / name).read_bytes() for name in HEAP_CLOSE_HEADERS}
    frame_headers = {name: (sdk_root / "rwsdk/src" / name).read_bytes().replace(b"\r\n", b"\n")
                     for name in CAMERA_FRAME_HEADERS}

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
         [0x1a644c, 0x1a64f0, 0x1aa108],
         "Projection and admitted view-window setters pass their nullable parent; the generic object-frame setter passes its new frame. The callee links a clean root into the dirty list, sets root flags3 and selected-frame flags12, and returns that frame without synchronizing it."),
        (0x1aa0b0, 0x1aa11c, "_rwObjectHasFrameSetFrame", "batypehf.c",
         "d1df69db48be32aa568ccabf299bfd146172d7ca4694b4925bc527003e5aee2b",
         [0x17ec00, 0x17ee84],
         "Camera/frame creation passes camera and new frame; teardown passes camera and null before destroying the frame. This VOID routine unlinks any old attachment, publishes the nullable parent, inserts at the new frame object-list head, then marks that frame."),
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
        (0x1a3c40, 0x1a3c48, "_rwResHeapClose", "resmem.c",
         "5ce5ad86d452c4d2422bd63e15223d5d6b3dfb77f224a88c1f476e9fb34e359d",
         [0x1a1bb4, 0x1a2068],
         "Unused source heap pointer; returns RwBool TRUE in the JR delay. The complete retail body has no stores or calls."),
    )
    for start, end, api, file, body_hash, sites, operations in connected:
        semantic_evidence = (validate_resource_arena_semantics(original, sources[file])
                             if start == 0x1a2020 else {})
        if start == 0x1a3c40:
            semantic_evidence = validate_heap_close_semantics(original, sources[file], heap_headers)
            semantic_evidence["semantic_mutation_controls"] = heap_close_semantic_controls(original, sources[file], heap_headers)
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

    # Preserve the established legacy refusal order before checking the new
    # complete caller joins. The dedicated mutation runner also invokes this
    # semantic validator directly, so a body hash cannot mask its checks.
    for start, end, evidence in results:
        if start in CAMERA_FRAME_BODIES:
            evidence.update(validate_camera_frame_semantics(original, sources, frame_headers, start))

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
