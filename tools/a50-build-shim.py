#!/usr/bin/env python3
"""Build the Galaxy A50 stage-0 Images (shim/a50/out/*.Image).

Works on Windows and Linux with any aarch64 GCC + binutils, e.g. the Arm GNU
Toolchain "aarch64-none-elf":

  python tools/a50-build-shim.py --cross C:/arm/bin/aarch64-none-elf-

No packaging or flashing happens here.
"""
import argparse
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
SHIM = ROOT / "shim" / "a50"
OUT = SHIM / "out"

CFLAGS = [
    "-c", "-O2", "-ffreestanding", "-fno-builtin", "-fno-stack-protector",
    "-fno-pic", "-fno-pie", "-nostdlib", "-fno-asynchronous-unwind-tables",
    "-fno-jump-tables", "-fno-tree-loop-distribute-patterns",
    "-mgeneral-regs-only", "-mstrict-align", "-mcpu=cortex-a53",
    "-Wall", "-Wextra", "-Werror",
]

IMAGES = {
    "probe": ["probe.S", "probe.c", "fbtext.c", "fdtinfo.c", "../fdt.c"],
    # S-Boot -> XNU loader; a payload is appended at _image_end.
    "xnu_boot": ["xnu_boot.S", "xnu_boot.c", "fbtext.c", "fdtinfo.c", "reboot.c", "../fdt.c"],
    # Stand-in for XNU inside a test payload (tools/a50-test-payload.py).
    "paytest": ["paytest.S", "paytest.c", "fbtext.c", "reboot.c"],
}


def run(cmd, **kw):
    print(" ".join(str(c) for c in cmd))
    return subprocess.run([str(c) for c in cmd], check=True, **kw)


def build(cross, name, sources):
    objs = []
    for src in sources:
        path = (SHIM / src).resolve()
        obj = OUT / f"{name}_{path.name.replace('.', '_')}.o"
        run([f"{cross}gcc", *CFLAGS, path, "-o", obj])
        objs.append(obj)
    elf = OUT / f"{name}.elf"
    run([f"{cross}ld", "-static", "-T", SHIM / "image.ld", "--build-id=none",
         "--no-warn-rwx-segments", *objs, "-o", elf])
    # A raw Image must be position independent: no absolute relocations.
    relocs = run([f"{cross}readelf", "-r", *objs], capture_output=True, text=True).stdout
    if "ABS64" in relocs:
        raise SystemExit(f"{name}: absolute relocation found:\n{relocs}")
    image = OUT / f"{name}.Image"
    run([f"{cross}objcopy", "-O", "binary", elf, image])
    syms = run([f"{cross}nm", elf], capture_output=True, text=True).stdout
    m = re.search(r"^([0-9a-fA-F]+) \S _image_end$", syms, re.M)
    if not m:
        raise SystemExit("_image_end symbol missing")
    data = image.read_bytes()
    if data[56:60] != b"ARMd":
        raise SystemExit("missing arm64 Image magic")
    # --append puts a payload at the 4 KiB-padded end, which must be _image_end.
    if (len(data) + 4095) // 4096 * 4096 != int(m.group(1), 16):
        raise SystemExit("_image_end mismatch")
    print(f"{image}: {len(data)} bytes; arm64 Image header OK")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--cross", default="aarch64-none-elf-",
                    help="toolchain prefix, may include a directory")
    ap.add_argument("images", nargs="*", default=list(IMAGES))
    args = ap.parse_args()
    OUT.mkdir(exist_ok=True)
    for name in args.images:
        if name not in IMAGES:
            sys.exit(f"unknown image {name}; known: {', '.join(IMAGES)}")
        build(args.cross, name, IMAGES[name])


if __name__ == "__main__":
    main()
