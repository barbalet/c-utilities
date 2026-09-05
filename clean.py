#!/usr/bin/env python3
"""Recursively remove compiled code, leaving source, scripts, and media intact."""

import argparse
import os
from pathlib import Path
import struct
import sys

VCS_DIRECTORIES = {".git", ".hg", ".svn"}
MACH_O = {b"\xfe\xed\xfa\xce", b"\xce\xfa\xed\xfe",
          b"\xfe\xed\xfa\xcf", b"\xcf\xfa\xed\xfe"}
FAT_MACH_O = {b"\xca\xfe\xba\xbe": (">", 20), b"\xbe\xba\xfe\xca": ("<", 20),
              b"\xca\xfe\xba\xbf": (">", 32), b"\xbf\xba\xfe\xca": ("<", 32)}


def artifact_kind(path):
    """Recognize objects by suffix and other compiled artifacts by file format."""
    if path.suffix.lower() in {".o", ".obj"}:
        return "object file"
    with path.open("rb") as source:
        header = source.read(64)
        magic = header[:4]
        if magic == b"\x7fELF":
            return "ELF binary"
        if magic in MACH_O:
            return "Mach-O binary"
        if magic in FAT_MACH_O and len(header) >= 8:
            endian, entry_size = FAT_MACH_O[magic]
            count = struct.unpack(endian + "I", header[4:8])[0]
            # Java class files share CAFEBABE; their version field is not an
            # architecture count. Also require a complete architecture table.
            if 0 < count <= 64 and path.stat().st_size >= 8 + count * entry_size:
                return "universal Mach-O binary"
        if header.startswith((b"!<arch>\n", b"!<thin>\n")):
            return "compiled archive"
        if header.startswith(b"\x00asm\x01\x00\x00\x00"):
            return "WebAssembly binary"
        if header.startswith(b"MZ") and len(header) >= 64:
            offset = struct.unpack("<I", header[60:64])[0]
            source.seek(offset)
            if source.read(4) == b"PE\0\0":
                return "PE binary"
    return None


def clean(root, dry_run=False):
    removed = 0
    failures = []

    def walk_error(error):
        failures.append(str(error))

    for directory, children, files in os.walk(root, followlinks=False, onerror=walk_error):
        children[:] = sorted(name for name in children
                             if name not in VCS_DIRECTORIES
                             and not (Path(directory) / name).is_symlink())
        for name in sorted(files):
            path = Path(directory) / name
            if path.is_symlink() or not path.is_file():
                continue
            try:
                kind = artifact_kind(path)
                if kind:
                    if not dry_run:
                        path.unlink()
                    removed += 1
                    print(f"{'Would remove' if dry_run else 'Removed'} {path.relative_to(root)} ({kind})")
            except OSError as error:
                failures.append(f"{path}: {error}")
    print(f"{'Found' if dry_run else 'Removed'} {removed} compiled artifact(s).")
    for error in failures:
        print(error, file=sys.stderr)
    return 1 if failures else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parent,
                        help="directory to clean (default: this repository)")
    parser.add_argument("--dry-run", action="store_true", help="list artifacts without deleting them")
    args = parser.parse_args()
    root = args.root.resolve()
    if not root.is_dir():
        parser.error(f"not a directory: {root}")
    return clean(root, args.dry_run)


if __name__ == "__main__":
    sys.exit(main())
