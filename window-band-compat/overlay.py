#!/usr/bin/env python3
"""Overlay one x64 Wine Unix syscall, retaining the PE syscall entry points.

Linux ELF64 only. Source and destination must differ. This is an experimental
overlay of an isolated runner, not a full Wine source build.
SPDX-License-Identifier: MIT
"""
import argparse
import hashlib
import json
import re
import struct
import subprocess
from pathlib import Path


def align(n, a=4096):
    return (n + a - 1) & -a


class Elf:
    def __init__(self, data):
        self.data = data
        if data[:6] != b"\x7fELF\x02\x01" or struct.unpack_from("<H", data, 18)[0] != 62:
            raise ValueError("expected little-endian x86-64 ELF")
        self.phoff, self.shoff = struct.unpack_from("<QQ", data, 32)
        self.phsize, self.phnum, self.shsize, self.shnum, self.shstr = struct.unpack_from("<HHHHH", data, 54)
        if self.phsize != 56 or self.shsize != 64:
            raise ValueError("unexpected ELF header sizes")
        self.ph = [struct.unpack_from("<IIQQQQQQ", data, self.phoff + i * 56) for i in range(self.phnum)]
        sh = [struct.unpack_from("<IIQQQQIIQQ", data, self.shoff + i * 64) for i in range(self.shnum)]
        strings = sh[self.shstr]
        names = data[strings[4]:strings[4] + strings[5]]
        self.sections = {}
        for i, section in enumerate(sh):
            name = names[section[0]:].split(b"\0", 1)[0].decode()
            self.sections[name] = (i, section)

    def offset(self, address):
        for typ, flags, off, va, pa, size, mem, alignment in self.ph:
            if typ == 1 and va <= address < va + size:
                return off + address - va
        raise ValueError(f"address {address:x} is not file backed")


def run(*args):
    return subprocess.check_output(args, text=True)


def overlay(source, build):
    original = source.read_bytes()
    if hashlib.sha256(original).hexdigest() != "2585a00d63361196b02eeff2ab153dde983fab51b7916d9cb2e9fc7025a9fe6e":
        raise ValueError("source is not the recorded, unmodified GE-Proton11-12 x64 win32u.so")
    elf = Elf(original)
    symbols = {}
    for line in run("nm", "-n", str(source)).splitlines():
        f = line.split()
        if len(f) == 3:
            symbols[f[2]] = int(f[0], 16)
    plt = {name: int(address, 16) for address, name in re.findall(
        r"^([0-9a-f]+) <([^>]+)@plt>:", run("objdump", "-d", "-j", ".plt", str(source)), re.M)}
    entry = symbols["NtUserGetWindowBand"]
    stub = elf.offset(entry)
    expected_stub = bytes.fromhex("488d3d9e960700e9")
    if original[stub:stub + 8] != expected_stub:
        raise ValueError("not the recorded GE-Proton11-12 NtUserGetWindowBand stub")
    args_address = symbols["arguments"] + 0x45a
    args_offset = elf.offset(args_address)
    if original[args_offset] != 0:
        raise ValueError("syscall argument size is already nonzero")
    notes = [i for i, p in enumerate(elf.ph) if p[0] == 4]
    if len(notes) != 1:
        raise ValueError("expected one build-id PT_NOTE header")
    note = elf.ph[notes[0]]
    if original[note[2] + 12:note[2] + 16] != b"GNU\0" or struct.unpack_from("<I", original, note[2] + 8)[0] != 3:
        raise ValueError("refusing to replace a non-build-id note header")
    header_index = next(i for i, p in enumerate(elf.ph) if p[0] == 0x6474e550)
    eh = elf.ph[header_index]
    old_hdr = original[eh[2]:eh[2] + eh[5]]
    if old_hdr[:4] != b"\x01\x1b\x03\x3b":
        raise ValueError("unsupported original unwind-header encoding")
    count = struct.unpack_from("<I", old_hdr, 8)[0]
    if len(old_hdr) != 12 + count * 8:
        raise ValueError("unexpected unwind-header length")
    old_frame_address = eh[3] + 4 + struct.unpack_from("<i", old_hdr, 4)[0]
    records = []
    for i in range(count):
        pc, fde = struct.unpack_from("<ii", old_hdr, 12 + i * 8)
        records.append((eh[3] + pc, eh[3] + fde))
    new_va = align(max(p[3] + p[6] for p in elf.ph if p[0] == 1))
    new_offset = align(len(original))
    build.mkdir(parents=True, exist_ok=True)
    script = build / "payload.ld"
    script.write_text(f"""SECTIONS {{
        . = {new_va};
        .payload : {{ *(.text*) *(.rodata*) }}
        . = ALIGN(8);
        .eh_frame : {{ KEEP(*(.eh_frame)) }}
        /DISCARD/ : {{ *(.comment) *(.note*) }}
    }}\n""")
    payload_elf = build / "payload.elf"
    subprocess.run(["ld", "--no-undefined", "-T", str(script),
                    "--defsym", f"band_query_window={symbols['NtUserQueryWindow']}",
                    "--defsym", f"band_set_error={plt['RtlSetLastWin32Error']}",
                    "--defsym", f"band_lookup={plt['dlsym']}",
                    "-o", str(payload_elf), str(build / "window_band.o")], check=True)
    linked = Elf(payload_elf.read_bytes())
    _, code_sec = linked.sections[".payload"]
    _, frame_sec = linked.sections[".eh_frame"]
    if code_sec[3] != new_va:
        raise ValueError("unexpected payload address")
    raw = linked.data[frame_sec[4]:frame_sec[4] + frame_sec[5]]
    pos = 0
    added = []
    while pos + 8 <= len(raw):
        length, cie = struct.unpack_from("<II", raw, pos)
        if not length:
            break
        if length == 0xffffffff or pos + length + 4 > len(raw):
            raise ValueError("unsupported payload frame record")
        if cie:
            pc = frame_sec[3] + pos + 8 + struct.unpack_from("<i", raw, pos + 8)[0]
            added.append((pc, frame_sec[3] + pos))
        pos += length + 4
    if len(added) != 1 or added[0][0] != new_va:
        raise ValueError("expected one frame record for the payload function")
    records.extend(added)
    records.sort()
    payload = bytearray(frame_sec[3] + frame_sec[5] - new_va)
    payload[:code_sec[5]] = linked.data[code_sec[4]:code_sec[4] + code_sec[5]]
    off = frame_sec[3] - new_va
    payload[off:off + frame_sec[5]] = raw
    payload.extend(b"\0" * (align(len(payload), 4) - len(payload)))
    hdr_pos = len(payload)
    hdr_va = new_va + hdr_pos
    new_hdr = bytearray(b"\x01\x1b\x03\x3b")
    new_hdr.extend(struct.pack("<iI", old_frame_address - hdr_va - 4, len(records)))
    for pc, fde in records:
        new_hdr.extend(struct.pack("<ii", pc - hdr_va, fde - hdr_va))
    payload.extend(new_hdr)
    data = bytearray(original)
    data[stub:stub + 5] = b"\xe9" + struct.pack("<i", new_va - entry - 5)
    data[args_offset] = 16  # two native 64-bit syscall arguments
    struct.pack_into("<IIQQQQQQ", data, elf.phoff + notes[0] * 56,
                     1, 5, new_offset, new_va, new_va, len(payload), len(payload), 4096)
    struct.pack_into("<IIQQQQQQ", data, elf.phoff + header_index * 56,
                     0x6474e550, 4, new_offset + hdr_pos, hdr_va, hdr_va, len(new_hdr), len(new_hdr), 4)
    sh_index, section = elf.sections[".eh_frame_hdr"]
    updated = list(section)
    updated[3:6] = [hdr_va, new_offset + hdr_pos, len(new_hdr)]
    struct.pack_into("<IIQQQQIIQQ", data, elf.shoff + sh_index * 64, *updated)
    data.extend(b"\0" * (new_offset - len(data)))
    data.extend(payload)
    manifest = {"source_sha256": hashlib.sha256(original).hexdigest(),
                "output_sha256": hashlib.sha256(data).hexdigest(),
                "syscall": "NtUserGetWindowBand", "syscall_number": "0x145a",
                "original_entry": hex(entry), "payload_entry": hex(new_va),
                "argument_bytes": 16, "added_unwind_entries": len(added),
                "all_existing_unwind_entries_retained": True,
                "build_id_note_bytes_retained": True,
                "replaced_build_id_program_header": True,
                "new_segment_permissions": "read+execute"}
    return bytes(data), manifest


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    if args.source.resolve() == args.destination.resolve():
        parser.error("source and destination must differ")
    try:
        data, manifest = overlay(args.source, Path(__file__).parent / "build")
    except (ValueError, KeyError, StopIteration, struct.error) as error:
        parser.error(str(error))
    args.destination.write_bytes(data)
    args.destination.with_suffix(".manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps(manifest, indent=2))
