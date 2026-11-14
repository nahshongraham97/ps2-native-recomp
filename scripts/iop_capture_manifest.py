#!/usr/bin/env python3
"""Validate relocated IOP captures and resolve their original module names.

This produces input metadata for an IOP AOT generator; it does not compile or
execute an IRX. Captures without an identity record are rejected explicitly.
"""
import argparse
import json
import re
import shlex
from pathlib import Path


def fnv1a64(data):
    value = 0xCBF29CE484222325
    for byte in data:
        value = ((value ^ byte) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return value


def read_captures(directory):
    identities = {}
    for number, line in enumerate((directory / 'module-identities.txt').read_text(encoding='utf-8').splitlines(), 1):
        parts = shlex.split(line, posix=True)
        if len(parts) != 2 or not re.fullmatch(r'[A-Za-z0-9_]+', parts[0]):
            raise ValueError(f'invalid identity record at line {number}')
        if parts[0] in identities and identities[parts[0]] != parts[1]:
            raise ValueError(f'conflicting identity for {parts[0]}')
        identities[parts[0]] = parts[1]
    captures = []
    seen = set()
    for number, line in enumerate((directory / 'modules.txt').read_text(encoding='ascii').splitlines(), 1):
        parts = line.split()
        if len(parts) != 4 or not re.fullmatch(r'[A-Za-z0-9_]+', parts[0]):
            raise ValueError(f'invalid capture record at line {number}')
        identifier = parts[0]
        base, size, expected = (int(value, 16) for value in parts[1:])
        if identifier in seen or identifier not in identities:
            raise ValueError(f'duplicate capture or missing identity: {identifier}')
        seen.add(identifier)
        data = (directory / (identifier + '.bin')).read_bytes()
        if not size or size % 4 or len(data) != size or fnv1a64(data) != expected:
            raise ValueError(f'capture size, alignment, or hash mismatch: {identifier}')
        captures.append({'capture': identifier, 'module': identities[identifier],
                         'base': base, 'size': size, 'fnv1a64': f'{expected:016X}'})
    return captures


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    try:
        print(json.dumps(read_captures(args.directory), indent=2))
    except (OSError, ValueError) as error:
        parser.exit(2, f'iop_capture_manifest: {error}\n')


if __name__ == '__main__':
    main()
