#!/usr/bin/env python3
"""Install matching ICU files bundled with a private GE-Proton Wine runner.

Only missing files are copied. Existing identical files are retained and
conflicting files cause the operation to abort before any copies are made.
SPDX-License-Identifier: MIT
"""
import argparse
import hashlib
import shutil
from pathlib import Path


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def plan(runner, prefix):
    items = []
    for arch, windows_dir in (("x86_64-windows", "system32"), ("i386-windows", "syswow64")):
        source = runner / "lib/wine/icu" / arch
        destination = prefix / "drive_c/windows" / windows_dir
        if not source.is_dir() or not destination.is_dir():
            raise ValueError(f"missing expected ICU source or Wine prefix directory for {arch}")
        files = sorted(source.glob("*.dll"))
        if not files:
            raise ValueError(f"no bundled ICU DLLs found for {arch}")
        for original in files:
            target = destination / original.name
            if target.exists():
                if digest(original) != digest(target):
                    raise ValueError(f"conflicting existing file: {target}")
                continue
            items.append((original, target))
    return items


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("runner", type=Path, help="private runner directory containing lib/wine/icu")
    parser.add_argument("prefix", type=Path, help="experimental prefix directory containing drive_c")
    args = parser.parse_args()
    try:
        copies = plan(args.runner, args.prefix)
    except ValueError as error:
        parser.error(str(error))
    for original, target in copies:
        # Exclusive creation also prevents a concurrent change being overwritten.
        with original.open("rb") as source, target.open("xb") as destination:
            shutil.copyfileobj(source, destination)
        print(target.relative_to(args.prefix))
    print(f"Installed {len(copies)} missing bundled ICU DLL(s).")
