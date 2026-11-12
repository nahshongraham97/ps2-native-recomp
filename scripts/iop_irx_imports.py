#!/usr/bin/env python3
"""Inventory IOP IRX imports using the same stub format as runtime link_imports().

This is an input-inspection tool for an IOP AOT compiler. It does not translate
an IRX or prove that any import is called at runtime.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import sys
from pathlib import Path
from typing import Any


PT_LOAD = 1
PT_IOPMOD = 0x70000080
IOP_IMPORT_MAGIC = 0x41E00000
IOP_IMPORT_RETURN = 0x03E00008  # jr ra
IOP_IMPORT_INDEX_MASK = 0xFFFF0000
IOP_IMPORT_INDEX_OP = 0x24000000  # addiu v0, zero, index
ELF_HEADER_SIZE = 52
PROGRAM_HEADER_SIZE = 32
IMPORT_HEADER_SIZE = 20
IMPORT_STUB_SIZE = 8


def _u16(data: bytes | bytearray, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def _u32(data: bytes | bytearray, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def _read_c_string(data: bytes | bytearray, offset: int, limit: int) -> str:
    end = min(len(data), offset + limit)
    raw = bytes(data[offset:end]).split(b"\0", 1)[0]
    return raw.decode("ascii", errors="strict")


def _parse_elf_load_image(data: bytes) -> tuple[bytearray, dict[str, int], str | None]:
    if len(data) < ELF_HEADER_SIZE or data[:4] != b"\x7fELF":
        raise ValueError("input is not a complete ELF file")
    if data[4] != 1 or data[5] != 1:
        raise ValueError("expected a 32-bit little-endian IOP ELF")
    if _u16(data, 18) != 8:  # EM_MIPS
        raise ValueError("ELF machine is not MIPS")

    phoff = _u32(data, 28)
    phentsize = _u16(data, 42)
    phnum = _u16(data, 44)
    if phentsize < PROGRAM_HEADER_SIZE:
        raise ValueError(f"program header size {phentsize} is smaller than 32 bytes")
    if phoff > len(data) or phnum > (len(data) - phoff) // phentsize:
        raise ValueError("program header table is truncated")

    load: tuple[int, int, int, int] | None = None
    iopmod_offset: int | None = None
    for index in range(phnum):
        at = phoff + index * phentsize
        p_type = _u32(data, at)
        p_offset = _u32(data, at + 4)
        p_vaddr = _u32(data, at + 8)
        p_filesz = _u32(data, at + 16)
        p_memsz = _u32(data, at + 20)
        if p_type == PT_IOPMOD and iopmod_offset is None:
            iopmod_offset = p_offset
        # Match the active runtime loader, which copies the first PT_LOAD
        # segment into the module allocation and relocates against that base.
        if p_type == PT_LOAD and load is None:
            load = (p_offset, p_vaddr, p_filesz, p_memsz)

    if load is None:
        raise ValueError("ELF has no PT_LOAD segment")
    file_offset, virtual_address, file_size, memory_size = load
    if file_size > memory_size:
        raise ValueError("PT_LOAD file size exceeds memory size")
    if file_offset > len(data) or file_size > len(data) - file_offset:
        raise ValueError("PT_LOAD file contents are truncated")

    image = bytearray(memory_size)
    image[:file_size] = data[file_offset : file_offset + file_size]

    module_name = None
    if iopmod_offset is not None and iopmod_offset + 26 < len(data):
        try:
            module_name = _read_c_string(data, iopmod_offset + 26, 56) or None
        except UnicodeDecodeError:
            module_name = None

    load_info = {
        "file_offset": file_offset,
        "virtual_address": virtual_address,
        "file_size": file_size,
        "memory_size": memory_size,
    }
    return image, load_info, module_name


def scan_import_tables(image: bytes | bytearray) -> list[dict[str, Any]]:
    """Find valid IOP import headers and their JR/ADDIU stub ordinals."""
    tables: list[dict[str, Any]] = []
    for image_offset in range(0, max(0, len(image) - IMPORT_HEADER_SIZE + 1), 4):
        if _u32(image, image_offset) != IOP_IMPORT_MAGIC:
            continue

        name_bytes = bytes(image[image_offset + 12 : image_offset + 20])
        try:
            library = name_bytes.split(b"\0", 1)[0].decode("ascii", errors="strict")
        except UnicodeDecodeError:
            continue
        if not library or not re.fullmatch(r"[A-Za-z0-9_]{1,8}", library):
            continue

        version = _u32(image, image_offset + 8)
        imports = []
        stub_offset = image_offset + IMPORT_HEADER_SIZE
        while stub_offset + IMPORT_STUB_SIZE <= len(image):
            first = _u32(image, stub_offset)
            second = _u32(image, stub_offset + 4)
            if first != IOP_IMPORT_RETURN or (second & IOP_IMPORT_INDEX_MASK) != IOP_IMPORT_INDEX_OP:
                break
            imports.append(
                {
                    "slot": len(imports),
                    "ordinal": second & 0xFFFF,
                    "image_offset": stub_offset,
                }
            )
            stub_offset += IMPORT_STUB_SIZE

        tables.append(
            {
                "image_offset": image_offset,
                "library": library,
                "version": version,
                "imports": imports,
            }
        )

    return tables


def inspect_irx(path: Path) -> dict[str, Any]:
    data = path.read_bytes()
    image, load, module_name = _parse_elf_load_image(data)
    imports = scan_import_tables(image)
    for table in imports:
        table["file_offset"] = load["file_offset"] + table["image_offset"]
        for entry in table["imports"]:
            entry["file_offset"] = load["file_offset"] + entry["image_offset"]
            entry["image_offset"] = f"0x{entry['image_offset']:08X}"
            entry["file_offset"] = f"0x{entry['file_offset']:08X}"
        table["image_offset"] = f"0x{table['image_offset']:08X}"
        table["file_offset"] = f"0x{table['file_offset']:08X}"
        table["version"] = f"0x{table['version']:04X}"

    return {
        "path": str(path.resolve()),
        "sha256": hashlib.sha256(data).hexdigest().upper(),
        "module_name": module_name,
        "load_segment": {
            key: f"0x{value:08X}" if key in {"file_offset", "virtual_address"} else value
            for key, value in load.items()
        },
        "imports": imports,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("irx", nargs="+", type=Path, help="one or more IOP IRX/ELF files")
    parser.add_argument("--out", type=Path, help="write JSON to this path instead of stdout")
    args = parser.parse_args(argv)

    try:
        result = [inspect_irx(path) for path in args.irx]
        text = json.dumps(result, indent=2) + "\n"
        if args.out:
            args.out.parent.mkdir(parents=True, exist_ok=True)
            args.out.write_text(text, encoding="utf-8")
        else:
            sys.stdout.write(text)
    except (OSError, ValueError, struct.error) as error:
        print(f"iop_irx_imports: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
