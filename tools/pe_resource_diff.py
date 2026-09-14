from __future__ import annotations

import argparse
import hashlib
from dataclasses import dataclass
from pathlib import Path
from typing import Iterator

import pefile


RESOURCE_TYPES = {
    value: name.removeprefix("RT_")
    for name, value in pefile.RESOURCE_TYPE.items()
    if isinstance(name, str) and name.startswith("RT_")
}


@dataclass(frozen=True)
class Resource:
    type_name: str
    resource_name: str
    language: int
    code_page: int
    file_offset: int
    size: int
    digest: str

    @property
    def key(self) -> tuple[str, str, int]:
        return self.type_name, self.resource_name, self.language


def entry_name(entry: object) -> str:
    name = getattr(entry, "name", None)
    if name is not None:
        return str(name)
    return str(getattr(entry, "id"))


def type_name(entry: object) -> str:
    identifier = getattr(entry, "id", None)
    if identifier in RESOURCE_TYPES:
        return RESOURCE_TYPES[identifier]
    return entry_name(entry)


def iter_resources(path: Path) -> Iterator[Resource]:
    pe = pefile.PE(str(path), fast_load=False)
    root = getattr(pe, "DIRECTORY_ENTRY_RESOURCE", None)
    if root is None:
        return

    image = path.read_bytes()
    for type_entry in root.entries:
        for name_entry in type_entry.directory.entries:
            for language_entry in name_entry.directory.entries:
                data = language_entry.data.struct
                offset = pe.get_offset_from_rva(data.OffsetToData)
                payload = image[offset : offset + data.Size]
                yield Resource(
                    type_name=type_name(type_entry),
                    resource_name=entry_name(name_entry),
                    language=language_entry.id,
                    code_page=data.CodePage,
                    file_offset=offset,
                    size=data.Size,
                    digest=hashlib.sha256(payload).hexdigest(),
                )


def print_diff(left_path: Path, right_path: Path) -> None:
    left = {resource.key: resource for resource in iter_resources(left_path)}
    right = {resource.key: resource for resource in iter_resources(right_path)}

    print(f"Left:  {left_path} ({len(left)} resources)")
    print(f"Right: {right_path} ({len(right)} resources)")
    print()

    changed = 0
    for key in sorted(left.keys() | right.keys()):
        left_resource = left.get(key)
        right_resource = right.get(key)
        if left_resource == right_resource:
            continue

        changed += 1
        type_label, name_label, language = key
        if left_resource is None:
            state = "only-right"
        elif right_resource is None:
            state = "only-left"
        elif left_resource.digest == right_resource.digest:
            state = "same-data/different-location"
        else:
            state = "changed"

        print(f"{state:28} {type_label}/{name_label}/lang={language}")
        if left_resource is not None:
            print(
                "  left "
                f"offset=0x{left_resource.file_offset:X} size={left_resource.size} "
                f"sha256={left_resource.digest}"
            )
        if right_resource is not None:
            print(
                "  right "
                f"offset=0x{right_resource.file_offset:X} size={right_resource.size} "
                f"sha256={right_resource.digest}"
            )

    print()
    print(f"Changed resource records: {changed}")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Compare the resource trees of two PE files."
    )
    parser.add_argument("left", type=Path)
    parser.add_argument("right", type=Path)
    args = parser.parse_args()
    print_diff(args.left, args.right)


if __name__ == "__main__":
    main()
