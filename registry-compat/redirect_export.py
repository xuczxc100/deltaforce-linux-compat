#!/usr/bin/env python3
"""Add experimental registry wrappers to Wine's ntoskrnl.exe, preserving source.

The two wrappers call existing ntdll import slots. They have genuine executable
entry points, and their compiler-generated unwind records join a copy of the
original exception table. This tool is only for the isolated Wine runner.
"""
import argparse
import hashlib
import json
import struct
import subprocess
from pathlib import Path


def align(value, boundary):
    return (value + boundary - 1) & ~(boundary - 1)


def redirect(source: bytes, build: Path) -> tuple[bytes, dict]:
    data = bytearray(source)
    u16 = lambda p: struct.unpack_from("<H", data, p)[0]
    u32 = lambda p: struct.unpack_from("<I", data, p)[0]
    u64 = lambda p: struct.unpack_from("<Q", data, p)[0]
    if data[:2] != b"MZ":
        raise ValueError("not a PE image")
    pe = u32(0x3C)
    if data[pe:pe + 4] != b"PE\0\0" or u16(pe + 4) != 0x8664:
        raise ValueError("expected an x86-64 PE image")
    optional = pe + 24
    if u16(optional) != 0x20B:
        raise ValueError("expected PE32+")
    directory = optional + 112
    if u32(directory + 32) or u32(directory + 36):
        raise ValueError("refusing to change a signed image")
    export_rva, export_size = u32(directory), u32(directory + 4)
    section_table = optional + u16(pe + 20)
    sections = []
    for i in range(u16(pe + 6)):
        h = section_table + i * 40
        sections.append((h, u32(h + 12), u32(h + 8), u32(h + 20), u32(h + 16)))
        if bytes(data[h:h + 8]).rstrip(b"\0") == b".rgcode":
            raise ValueError("image is already overlaid")

    def offset(rva):
        for _, address, _, raw, raw_size in sections:
            if address <= rva < address + raw_size:
                return raw + rva - address
        raise ValueError(f"RVA {rva:x} is not file backed")

    def string(rva):
        start = offset(rva)
        return bytes(data[start:data.index(0, start)]).decode("ascii")

    imports = {}
    descriptor = offset(u32(directory + 8))
    while u32(descriptor + 12):
        if string(u32(descriptor + 12)).lower() == "ntdll.dll":
            lookup = offset(u32(descriptor) or u32(descriptor + 16))
            iat = u32(descriptor + 16)
            index = 0
            while u64(lookup + index * 8):
                entry = u64(lookup + index * 8)
                if not entry & (1 << 63):
                    imports[string(entry + 2)] = iat + index * 8
                index += 1
        descriptor += 20
    redirects = {"ZwQueryValueKey": "NtQueryValueKey", "ZwSetValueKey": "NtSetValueKey"}
    if not all(name in imports for name in redirects.values()):
        raise ValueError("original ntdll imports are missing")

    export = offset(export_rva)
    functions, names, ordinals = [offset(u32(export + n)) for n in (28, 32, 36)]
    entries = [u32(functions + i * 4) for i in range(u32(export + 20))]
    matches = {}
    leaf_export = "IoGetBaseFileSystemDeviceObject"
    for i in range(u32(export + 24)):
        name = string(u32(names + 4 * i))
        if name in redirects or name == leaf_export:
            if name in matches:
                raise ValueError("duplicate export")
            matches[name] = u16(ordinals + 2 * i)
    if set(matches) != set(redirects) | {leaf_export}:
        raise ValueError("expected registry and file-system exports")
    if any(export_rva <= entries[index] < export_rva + export_size for index in matches.values()):
        raise ValueError("an export already forwards elsewhere")

    template = (build / "registry_thunk.bin").read_bytes()
    unwind = (build / "registry_unwind.bin").read_bytes()
    nm = subprocess.check_output(["x86_64-w64-mingw32-nm", "-n", str(build / "registry_thunk.o")], text=True)
    symbols = {f[2]: int(f[0], 16) for line in nm.splitlines()
               if len(f := line.split()) == 3 and f[2].startswith("registry_")}
    import_operand, code_length = symbols["registry_import_operand"], symbols["registry_thunk_end"]
    if symbols["registry_thunk"] != 0 or not 0 <= import_operand <= code_length - 4:
        raise ValueError("unexpected assembly template")
    if len(template) < code_length or len(unwind) != 8:
        raise ValueError("unexpected code or unwind size")

    section_align, file_align = u32(optional + 32), u32(optional + 36)
    if not section_align or section_align & (section_align - 1) or not file_align or file_align & (file_align - 1):
        raise ValueError("invalid PE alignment")
    new_header = section_table + len(sections) * 40
    if new_header + 40 > min(s[3] for s in sections if s[4]):
        raise ValueError("no room for an additional section header")
    if any(data[new_header:new_header + 40]):
        raise ValueError("section header padding is occupied")
    new_rva = align(u32(optional + 56), section_align)
    raw_offset = align(len(data), file_align)
    payload, records, changed_exports = bytearray(), [], {}
    for name, imported_name in redirects.items():
        payload.extend(b"\x90" * (align(len(payload), 16) - len(payload)))
        code_rva = new_rva + len(payload)
        code = bytearray(template[:code_length])
        struct.pack_into("<i", code, import_operand, imports[imported_name] - code_rva - import_operand - 4)
        payload.extend(code)
        payload.extend(b"\0" * (align(len(payload), 4) - len(payload)))
        unwind_rva = new_rva + len(payload)
        payload.extend(unwind)
        records.append(struct.pack("<III", code_rva, code_rva + code_length, unwind_rva))
        index = matches[name]
        changed_exports[name] = {"original_rva": hex(entries[index]), "wrapper_rva": hex(code_rva),
                                 "original_import_slot": hex(imports[imported_name])}
        struct.pack_into("<I", data, functions + index * 4, code_rva)

    # The file-system selector is a leaf function; x64 needs no unwind record.
    payload.extend(b"\x90" * (align(len(payload), 16) - len(payload)))
    leaf_rva = new_rva + len(payload)
    payload.extend((build / "basefs.bin").read_bytes())
    index = matches[leaf_export]
    struct.pack_into("<I", data, functions + index * 4, leaf_rva)
    changed_exports[leaf_export] = {"original_rva": hex(entries[index]), "wrapper_rva": hex(leaf_rva), "leaf": True}

    old_exception_rva, old_exception_size = u32(directory + 24), u32(directory + 28)
    if not old_exception_rva or old_exception_size % 12:
        raise ValueError("invalid original exception table")
    old_exception_offset = offset(old_exception_rva)
    old_table = source[old_exception_offset:old_exception_offset + old_exception_size]
    if len(old_table) != old_exception_size:
        raise ValueError("truncated exception table")
    if any(struct.unpack_from("<I", old_table, i)[0] >= new_rva for i in range(0, old_exception_size, 12)):
        raise ValueError("exception table would not remain ordered")
    payload.extend(b"\0" * (align(len(payload), 4) - len(payload)))
    new_exception_rva = new_rva + len(payload)
    payload.extend(old_table)
    payload.extend(b"".join(records))
    raw_size = align(len(payload), file_align)
    struct.pack_into("<8sIIIIIIHHI", data, new_header,
                     b".rgcode\0", len(payload), new_rva, raw_size, raw_offset, 0, 0, 0, 0, 0x60000020)
    struct.pack_into("<H", data, pe + 6, len(sections) + 1)
    struct.pack_into("<I", data, optional + 4, u32(optional + 4) + raw_size)
    struct.pack_into("<I", data, optional + 56, align(new_rva + len(payload), section_align))
    struct.pack_into("<II", data, directory + 24, new_exception_rva, old_exception_size + len(records) * 12)
    struct.pack_into("<I", data, optional + 64, 0)
    data.extend(b"\0" * (raw_offset - len(data)))
    data.extend(payload)
    data.extend(b"\0" * (raw_size - len(payload)))
    text = next(s for s in sections if bytes(source[s[0]:s[0] + 8]).rstrip(b"\0") == b".text")
    raw, size = text[3:]
    if data[raw:raw + size] != source[raw:raw + size]:
        raise ValueError("original code was unexpectedly modified")
    manifest = {"exports": changed_exports, "original_text_unchanged": True,
                "added_unwind_entries": len(records), "source_sha256": hashlib.sha256(source).hexdigest(),
                "overlay_sha256": hashlib.sha256(data).hexdigest()}
    return bytes(data), manifest


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    if args.source.resolve() == args.destination.resolve():
        parser.error("source and destination must differ")
    try:
        overlay, manifest = redirect(args.source.read_bytes(), Path(__file__).parent / "build")
    except (ValueError, struct.error, IndexError, KeyError, StopIteration) as error:
        parser.error(str(error))
    args.destination.write_bytes(overlay)
    args.destination.with_suffix(".manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps(manifest, indent=2))
