#!/usr/bin/env python3
"""Synthetic tests for the IOP IRX import inventory tool."""

import importlib.util
import struct
import unittest
from pathlib import Path


SCRIPT = Path(__file__).with_name("iop_irx_imports.py")
SPEC = importlib.util.spec_from_file_location("iop_irx_imports", SCRIPT)
assert SPEC and SPEC.loader
iop_irx_imports = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(iop_irx_imports)


def add_import_table(image: bytearray, offset: int, library: bytes, version: int, ordinals: list[int]) -> None:
    struct.pack_into("<III", image, offset, iop_irx_imports.IOP_IMPORT_MAGIC, 0, version)
    image[offset + 12 : offset + 20] = library[:8].ljust(8, b"\0")
    at = offset + iop_irx_imports.IMPORT_HEADER_SIZE
    for ordinal in ordinals:
        struct.pack_into("<II", image, at, iop_irx_imports.IOP_IMPORT_RETURN, 0x24000000 | ordinal)
        at += iop_irx_imports.IMPORT_STUB_SIZE


def synthetic_irx() -> bytes:
    data = bytearray(0x240)
    data[:16] = b"\x7fELF" + bytes([1, 1, 1]) + bytes(9)
    struct.pack_into(
        "<HHIIIIIHHHHHH",
        data,
        16,
        2,  # ET_EXEC
        8,  # EM_MIPS
        1,
        0,
        52,  # e_phoff
        0,
        0,
        52,
        32,
        2,
        0,
        0,
        0,
    )
    # IOP module-info segment; the runtime reads the name at p_offset + 26.
    struct.pack_into("<IIIIIIII", data, 52, iop_irx_imports.PT_IOPMOD, 0x80, 0, 0, 0x30, 0x30, 4, 4)
    # First PT_LOAD segment, matching the runtime's loader behavior.
    struct.pack_into("<IIIIIIII", data, 84, iop_irx_imports.PT_LOAD, 0x100, 0, 0, 0x100, 0x100, 7, 16)
    data[0x80 + 26 : 0x80 + 26 + 9] = b"MWFILEFR\0"

    image = bytearray(0x100)
    add_import_table(image, 0x20, b"ioman", 0x0104, [4, 5, 6, 7, 8])
    add_import_table(image, 0x70, b"cdvdman", 0x0101, [6, 8, 10, 11, 13, 15])
    data[0x100:0x200] = image
    return bytes(data)


class IopIrxImportTests(unittest.TestCase):
    def test_scan_import_tables_keeps_stub_slot_and_function_ordinal_distinct(self) -> None:
        image = bytearray(0x80)
        add_import_table(image, 0x10, b"ioman", 0x0104, [4, 5, 8])
        tables = iop_irx_imports.scan_import_tables(image)

        self.assertEqual(len(tables), 1)
        self.assertEqual(tables[0]["library"], "ioman")
        self.assertEqual(tables[0]["version"], 0x0104)
        self.assertEqual(
            [(entry["slot"], entry["ordinal"]) for entry in tables[0]["imports"]],
            [(0, 4), (1, 5), (2, 8)],
        )

    def test_parse_irx_reports_load_relative_and_file_offsets(self) -> None:
        import tempfile

        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "MWFILEFR.IRX"
            path.write_bytes(synthetic_irx())
            result = iop_irx_imports.inspect_irx(path)

        self.assertEqual(result["module_name"], "MWFILEFR")
        self.assertEqual(result["load_segment"]["file_offset"], "0x00000100")
        self.assertEqual([table["library"] for table in result["imports"]], ["ioman", "cdvdman"])
        ioman, cdvdman = result["imports"]
        self.assertEqual(ioman["image_offset"], "0x00000020")
        self.assertEqual(ioman["file_offset"], "0x00000120")
        self.assertEqual([entry["ordinal"] for entry in ioman["imports"]], [4, 5, 6, 7, 8])
        self.assertEqual([entry["ordinal"] for entry in cdvdman["imports"]], [6, 8, 10, 11, 13, 15])

    def test_rejects_truncated_program_header_table(self) -> None:
        data = bytearray(synthetic_irx()[:70])
        with self.assertRaisesRegex(ValueError, "program header table is truncated"):
            iop_irx_imports._parse_elf_load_image(bytes(data))


if __name__ == "__main__":
    unittest.main()
