#!/usr/bin/env python3
"""Copy missing Tahoma fonts bundled with a private Wine runner.

No fonts are downloaded or distributed by this project.
SPDX-License-Identifier: MIT
"""
import argparse
import hashlib
import shutil
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("runner", type=Path)
    parser.add_argument("prefix", type=Path)
    args = parser.parse_args()
    destination = args.prefix / "drive_c/windows/Fonts"
    if not destination.is_dir():
        parser.error("expected an existing experimental prefix Fonts directory")
    copies = []
    for name in ("tahoma.ttf", "tahomabd.ttf"):
        source = args.runner / "share/wine/fonts" / name
        target = destination / name
        if not source.is_file():
            parser.error(f"missing bundled font: {source}")
        if target.exists():
            if hashlib.sha256(source.read_bytes()).digest() != hashlib.sha256(target.read_bytes()).digest():
                parser.error(f"conflicting existing font: {target}")
        else:
            copies.append((source, target))
    for source, target in copies:
        with source.open("rb") as original, target.open("xb") as installed:
            shutil.copyfileobj(original, installed)
        print(target.relative_to(args.prefix))
    print(f"Installed {len(copies)} missing bundled launcher font(s).")


if __name__ == "__main__":
    main()
