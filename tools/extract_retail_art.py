"""Prepare loose PS2 art files once. Not imported or invoked by the runtime."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


def digest(path):
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def extract(archive, destination, receipt):
    archive = archive.resolve(strict=True)
    destination = destination.absolute()
    receipt = receipt.absolute()
    if destination.exists() or receipt.exists():
        raise ValueError("destination and receipt must be new; existing files are never replaced")
    source_hash = digest(archive)
    rows = []
    with archive.open("rb") as source:
        initial = archive.stat()
        header = source.read(20)
        if len(header) != 20:
            raise ValueError("short original PAK header")
        magic, version, count, table, names_size = struct.unpack("<5I", header)
        if magic != 0x50414B20 or version != 0x100 or not count:
            raise ValueError("unsupported original PAK")
        if table > initial.st_size or count * 12 + names_size > initial.st_size - table:
            raise ValueError("original PAK index outside file")
        source.seek(table)
        entries = [struct.unpack("<3I", source.read(12)) for _ in range(count)]
        names = source.read(names_size)
        unique = set()
        for ordinal, (name_at, offset, size) in enumerate(entries):
            if offset > initial.st_size or size > initial.st_size - offset:
                raise ValueError("original member outside file")
            end = names.find(b"\0", name_at)
            if end < 0 or name_at >= len(names):
                raise ValueError("invalid original member name")
            original = names[name_at:end].decode("ascii")
            prefix = "\\ps2dvd\\art\\"
            if not original.lower().startswith(prefix):
                raise ValueError("unverified original member prefix")
            name = original[len(prefix):]
            if (not name or name in (".", "..") or name[-1] in ". " or
                    any(ord(c) < 32 or ord(c) == 127 or c in '/\\:*?"<>|' for c in name)):
                raise ValueError("unsafe member filename")
            if name.lower() in unique:
                raise ValueError("ambiguous loose filename")
            unique.add(name.lower())
            rows.append(dict(original_index=ordinal, original_name=original,
                             filename=name, original_offset=offset, size=size))
        destination.mkdir(parents=True, exist_ok=False)
        for row in rows:
            source.seek(row["original_offset"])
            target = destination / row["filename"]
            expected = hashlib.sha256()
            remaining = row["size"]
            with target.open("xb") as output:
                while remaining:
                    block = source.read(min(remaining, 1024 * 1024))
                    if not block:
                        raise OSError("short original member read")
                    output.write(block)
                    expected.update(block)
                    remaining -= len(block)
            row["sha256"] = expected.hexdigest()
            row["written_sha256"] = digest(target)
            if row["sha256"] != row["written_sha256"]:
                raise OSError("written loose file differs from original member")
        final = archive.stat()
        if (initial.st_size, initial.st_mtime_ns, initial.st_ino) != (final.st_size, final.st_mtime_ns, final.st_ino):
            raise OSError("original archive changed during extraction")
    if digest(archive) != source_hash:
        raise OSError("original archive identity changed")
    result = dict(status="extracted-and-byte-verified", source=str(archive),
                  source_sha256=source_hash, destination=str(destination.resolve()),
                  members=rows, count=len(rows), bytes=sum(x["size"] for x in rows),
                  runtime_archive_required=False, game_run=False, frames=[], audio=[])
    receipt.parent.mkdir(parents=True, exist_ok=True)
    with receipt.open("x", encoding="utf-8", newline="\n") as output:
        json.dump(result, output, indent=2)
        output.write("\n")
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--destination", type=Path, required=True,
                        help="new art directory inside the native content root")
    parser.add_argument("--receipt", type=Path, required=True)
    args = parser.parse_args()
    result = extract(args.archive, args.destination, args.receipt)
    print(f"EXTRACTED {result['count']} loose files; {result['bytes']} bytes verified")
