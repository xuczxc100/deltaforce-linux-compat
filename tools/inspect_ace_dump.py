#!/usr/bin/env python3
"""Read only exception metadata from a local LZMA-compressed ACE .df.z dump.

The dump stays local. This prints module basenames, not memory, tokens or paths.
SPDX-License-Identifier: MIT
"""
import argparse
import json
import lzma
import struct
from pathlib import Path

LIMIT = 64 * 1024 * 1024


def inspect(path):
    if path.stat().st_size > LIMIT:
        raise ValueError("compressed input exceeds the 64 MiB safety limit")
    source = path.read_bytes()
    if len(source) < 10:
        raise ValueError("truncated compressed header")
    expected, = struct.unpack_from("<I", source)
    prop = source[4]
    dictionary, = struct.unpack_from("<I", source, 5)
    if not 0 < expected <= LIMIT or not 4096 <= dictionary <= LIMIT or prop >= 225:
        raise ValueError("unsupported size or LZMA properties")
    filters = [{"id": lzma.FILTER_LZMA1, "dict_size": dictionary,
                "lc": prop % 9, "lp": prop // 9 % 5, "pb": prop // 45}]
    decoder = lzma.LZMADecompressor(format=lzma.FORMAT_RAW, filters=filters)
    data = decoder.decompress(source[9:], max_length=expected + 1)
    if len(data) != expected or data[:4] != b"MDMP":
        raise ValueError("unrecognized or incomplete minidump")

    def unpack(fmt, offset):
        if offset < 0 or offset + struct.calcsize(fmt) > len(data):
            raise ValueError("minidump metadata points outside the file")
        return struct.unpack_from(fmt, data, offset)

    count, directory = unpack("<II", 8)
    if count > 1024:
        raise ValueError("unexpected number of streams")
    streams = {}
    for i in range(count):
        kind, size, address = unpack("<III", directory + i * 12)
        if address + size > len(data):
            raise ValueError("stream extends beyond the file")
        streams[kind] = (size, address)
    if 4 not in streams or 6 not in streams:
        raise ValueError("module or exception stream is missing")
    _, address = streams[4]
    modules_count, = unpack("<I", address)
    if modules_count > 4096:
        raise ValueError("unexpected module count")
    modules = []
    for i in range(modules_count):
        base, size, _, _, name = unpack("<QIIII", address + 4 + i * 108)
        length, = unpack("<I", name)
        if length > 8192 or name + 4 + length > len(data):
            raise ValueError("invalid module name")
        basename = data[name + 4:name + 4 + length].decode("utf-16-le", errors="replace")
        basename = basename.replace("\\", "/").split("/")[-1]
        modules.append((base, size, basename))
    _, address = streams[6]
    thread, = unpack("<I", address)
    code, flags = unpack("<II", address + 8)
    pc, = unpack("<Q", address + 24)
    module = next(((name, pc - base) for base, size, name in modules if base <= pc < base + size), None)
    return {"exception_code": hex(code), "exception_flags": hex(flags),
            "thread_id": thread, "module": module[0] if module else None,
            "module_offset": hex(module[1]) if module else None}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dump", type=Path)
    args = parser.parse_args()
    try:
        result = inspect(args.dump)
    except (ValueError, OSError, lzma.LZMAError, struct.error) as error:
        parser.error(str(error))
    print(json.dumps(result, indent=2))
