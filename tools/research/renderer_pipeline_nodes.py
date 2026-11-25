"""Nine bounded PS2 pipeline identities, proved from original dataflow + SDK.

This is offline evidence extraction, not a pipeline/device implementation.
The caller supplies original(VMA, byte_count) and performs catalogue/AOT joins.
SDK input is the read-only rwsdk directory inside rwsdk-v3.1-ps2.
"""
from __future__ import annotations

import hashlib
from pathlib import Path
import struct
from typing import Callable


SDK_FILES = {
    "include/sky2/rwcore.h": "42a305ef09659d17a3f76e43ac1f192aed8f2235e7bdc169e8cf735806e38820",
    "world/pipe/p2/sky2/fastim3d.c": "fc606500aa9caf56797950da2cb3b5adaa2f083004731257fc1ac915bf55f574",
    "world/pipe/p2/sky2/nodeps2all.c": "66355f3af95df12afc375b3434ea566eb3eeb13b81f314be8c9330e0e951f5d8",
    "world/pipe/p2/sky2/ps2allmat.c": "8f563c0bdd2734a203b7ab51e3dc43c6d65943ac5f594616c57836aed1e25b45",
}


def recover_pipeline_nodes(original: Callable[[int, int], bytes], sdk_root: Path) -> list[dict]:
    """Return nine source-supported starts/reasons; fail on changed witnesses.

    No range expansion, unknown-null admission, guessed function names, or
    fallback after an SDK/original mismatch. Callbacks 1D1CF0 and 1D4F60 are
    absent from the raw catalogue and MUST NOT receive denominator credit.
    """
    sdk = {}
    for relative, expected in SDK_FILES.items():
        data = (sdk_root / relative).read_bytes()
        if hashlib.sha256(data).hexdigest() != expected:
            raise AssertionError(f"pipeline SDK source identity changed: {relative}")
        sdk[relative] = data.decode("utf-8", errors="strict").replace("\r\n", "\n")

    # Seven 32-bit function-pointer slots follow name in this PS2 data shape.
    # Confirm their semantic order against the pinned SDK declaration, without
    # importing its host compiler layout or assigning meaning to arbitrary data.
    core = sdk["include/sky2/rwcore.h"]
    marker = "struct RxNodeMethods\n{"
    if marker not in core:
        raise AssertionError("RxNodeMethods declaration missing")
    methods = core.split(marker, 1)[1].split("};", 1)[0]
    slots = ["nodeBody", "nodeInit", "nodeTerm", "pipelineNodeInit",
             "pipelineNodeTerm", "pipelineNodeConfig", "configMsgHandler"]
    positions = [methods.find(name + ";") for name in slots]
    if min(positions) < 0 or positions != sorted(positions):
        raise AssertionError("SDK pipeline method order changed")

    def words(address: int, expected: list[int]) -> str:
        raw = original(address, len(expected) * 4)
        if raw != struct.pack("<" + "I" * len(expected), *expected):
            raise AssertionError(f"pipeline original word witness changed: {address:08x}")
        return raw.hex()

    def text(address: int, expected: str) -> None:
        raw = expected.encode("ascii") + b"\0"
        if original(address, len(raw)) != raw:
            raise AssertionError(f"pipeline original string changed: {address:08x}")

    text(0x5A4370, "PS2Im3DFastTransform.csl")
    text(0x5A43D0, "FastIm3DTransform")
    text(0x5A43E8, "PS2All.csl")
    text(0x5A4708, "PS2AllMat.csl")
    fast_record = words(0x5A4390, [0x5A4370, 0x1CFA10, 0, 0, 0, 0, 0, 0])
    all_record = words(0x5A4400, [0x5A43E8, 0x1D1DC0, 0, 0, 0x1D1CF0, 0, 0, 0])
    allmat_record = words(0x5A4720, [0x5A4708, 0x1D4F60, 0, 0, 0x1D4EF0, 0, 0, 0])
    # Original producer: load a2=record, call fragment-adder with a3=null in
    # its delay slot. The private static getter is inlined here; do not invent
    # a separate compiled getter start from the SDK's source definition.
    producer = {
        "001cfaa4": words(0x1CFAA4, [0x3C06005A]),
        "001cfab0": words(0x1CFAB0, [0x24C64390, 0x0C071A88, 0x0000382D]),
    }
    getter = words(0x1D2660, [0x3C02005A, 0x03E00008, 0x24424400])
    allmat_getter = words(0x1D5810, [0x3C02005A, 0x03E00008, 0x24424720])
    # The named descriptor's init slot supplies the consumer proof. Its first
    # JAL calls the process-init-data child, loads self->privateData in that
    # delay slot, and exits with the child's FALSE result before other setup.
    # The SDK init function names this unique first child explicitly. This is
    # stronger than attributing an arbitrary function from diagnostic text.
    allmat_init_child = words(0x1D4EFC, [0x0C0752AC, 0x8C900014,
                                      0x14400003, 0x00000000,
                                      0x10000010, 0xDFBF0010])
    init_data_reads = words(0x1D4ACC, [0x8C910014, 0x0C071828,
                                    0x0000802D, 0x504000EC,
                                    0x24040004, 0x8C430038, 0xA22300B6])
    # Storage-only material node: deliberately returns FALSE. The SDK says it
    # must not be used as an executing node; this is not claimed draw success.
    allmat_false = words(0x1D4F60, [0x03E00008, 0x0000102D])

    # These complete catalogue body spans (and separately labelled uncatalogued
    # witness span) are hashed against the pinned original ELF, not generated C++.
    bodies = {
        0x1CFA10: (88, "a6bad1e39a7e55b74a965a63cea5a38fe7c301b73b2c00d7966640966cc242bb"),
        0x1CFA70: (180, "f998b85c18832b614abc34d44a902c41def72812b95d8b7231f7aa2536b97b9c"),
        0x1D1CF0: (208, "cb3f635f9cd90db3ed370ab2ba65fb2e91c01c0a2aaf93fa688e23b526726dcc"),
        0x1D1DC0: (1044, "cd8817e578fbf0a806d12a027f856dfd6dc58f0ebc67413c06dbdf2de9ae9302"),
        0x1D2660: (12, "85faf6e52643fadb134642aa66131392909279f83920c660d4a26a13b499400a"),
        0x1D4AB0: (1084, "721c514c33ab0d686e6bf954e79bcd322dcb78811b93266a20960f5ef5bba844"),
        0x1D4EF0: (108, "d396573660b74ecb2d97b354a83bb16eb9c511948715f6b23432786c37180280"),
        0x1D4F60: (8, "008d26890102af179c703d77fae42cb7b3f424a11b8db552383dd8e5313f4061"),
        0x1D5810: (12, "241d4e995f747e374d322ac1caaf2469affb21acdb7698ea507d11f0eb244bc2"),
    }
    roles = {
        0x1CFA10: ("PS2Im3DFastTransformNodeBody", "nodeBody at original record+4"),
        0x1CFA70: ("fastIm3DTransformPipeCreate", "actual original producer supplies named record to fragment addition"),
        0x1D1CF0: ("PS2AllPipelineNodeInit", "pipelineNodeInit at original record+16"),
        0x1D1DC0: ("PS2AllNodeBody", "nodeBody at original record+4"),
        0x1D2660: ("RxNodeDefinitionGetPS2All", "three original words return named record, including executed JR delay slot"),
        0x1D4AB0: ("PS2AllMatProcessInitData", "unique source-named first child of descriptor init; original private/init-data reads and FALSE exit path corroborate source"),
        0x1D4EF0: ("PS2AllMatPipelineNodeInit", "pipelineNodeInit at original material descriptor+16; calls process-init child before default publication"),
        0x1D4F60: ("PS2AllMatNodeBody", "nodeBody at original material descriptor+4; intentional two-word returnFALSE storage-only node"),
        0x1D5810: ("RxNodeDefinitionGetPS2AllMat", "three original words return material descriptor, including executed JR delay slot"),
    }
    limits = [
        "SDK3.1 source corroborates role/dataflow, not an exact MKA compiler/library version or binary match.",
        "SDK PS2All getter has lighting setup beyond the retail three-word getter; no full semantic equivalence claimed.",
        "FastIm3D source explicitly defers transformation; this identity proves no completed vertex/GPU effect.",
        "Null callback slots admit no extra functions; no recursive helper/callee or address-range ownership expansion.",
        "PS2AllMatProcessInitData admission uses named descriptor->init->unique child plus original reads; diagnostic text alone is not a reference proof.",
        "PS2AllMatNodeBody returns FALSE and performs no material processing/draw; its stock storage-node role is intentional, not successful rendering.",
        "Other call sites using a node/getter are not themselves getters and are not admitted here.",
        "No return1-only or generic adjacent getter is admitted without the named record and original source connection.",
    ]
    rows = []
    for address, (extent, expected) in bodies.items():
        if hashlib.sha256(original(address, extent)).hexdigest() != expected:
            raise AssertionError(f"pipeline original body identity changed: {address:08x}")
        identity, role = roles[address]
        rows.append({
            "module": "SLUS_214.10", "address": f"{address:08x}",
            "sdk_identity": identity, "kind": "source_corroborated_renderer_pipeline_function",
            "role": role, "original_witness_span_bytes": extent,
            "original_witness_span_sha256": expected,
            "span_limit": ("uncatalogued callback start through next catalogue start; not independently asserted complete body"
                           if address == 0x1D1CF0 else "complete two-word uncatalogued returnFALSE leaf"
                           if address == 0x1D4F60 else "file-backed envelope of the catalogue body ranges"),
            "sdk_source_pins": SDK_FILES.copy(),
            "records": {"005a4390": fast_record, "005a4400": all_record, "005a4720": allmat_record},
            "producer_original_words_le": producer, "getter_original_words_le": getter,
            "allmat_getter_original_words_le": allmat_getter,
            "allmat_init_child_original_words_le": allmat_init_child,
            "allmat_process_init_data_reads_original_words_le": init_data_reads,
            "allmat_storage_body_original_words_le": allmat_false,
            "limits": limits.copy(),
        })
    return rows
