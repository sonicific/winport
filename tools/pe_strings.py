from __future__ import annotations

import argparse
import re
from pathlib import Path

import pefile


ASCII_PATTERN = re.compile(rb"[\x20-\x7e]{4,}")
UTF16_PATTERN = re.compile(rb"(?:[\x20-\x7e]\x00){4,}")


def offset_to_rva(pe: pefile.PE, offset: int) -> int | None:
    try:
        return pe.get_rva_from_offset(offset)
    except pefile.PEFormatError:
        return None


def scan(path: Path) -> None:
    image = path.read_bytes()
    pe = pefile.PE(data=image, fast_load=True)
    matches: list[tuple[int, str, str]] = []

    for match in ASCII_PATTERN.finditer(image):
        matches.append((match.start(), "ascii", match.group().decode("ascii")))

    for match in UTF16_PATTERN.finditer(image):
        matches.append(
            (match.start(), "utf16", match.group().decode("utf-16le"))
        )

    for offset, encoding, value in sorted(matches):
        rva = offset_to_rva(pe, offset)
        rva_text = "-" if rva is None else f"0x{rva:08X}"
        print(f"0x{offset:08X} {rva_text} {encoding:5} {value}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Extract printable PE strings with offsets.")
    parser.add_argument("path", type=Path)
    args = parser.parse_args()
    scan(args.path)


if __name__ == "__main__":
    main()
