#!/usr/bin/env python3
"""Build a SWEETXNU v1 payload whose "kernel" is shim/a50/out/paytest.Image.

  python tools/a50-test-payload.py shim/a50/out/paytest.Image paytest-payload.bin
  python tools/a50-pack-boot.py pack boot-stock.img shim/a50/out/xnu_boot.Image \
      loadertest-boot.img --append paytest-payload.bin --partition-size N

Same layout as tools/mkpayload.py (see its docstring). xnu_boot copies the
segment to phys_base + 0x4000, puts the 256-byte test ADT and boot_args after
it and jumps there with the MMU off; paytest.c checks all of that on screen.
No Apple files are involved.
"""
import argparse
import struct
from pathlib import Path

BOOT_LINE_LENGTH = 608
CMDLINE_OFF = 0x48
SEG_TABLE = CMDLINE_OFF + BOOT_LINE_LENGTH          # 0x2a8
VIRT_BASE = 0xFFFFFFF006000000                     # 32 MiB aligned, like mkpayload
CODE_OFFSET = 0x4000                               # paytest.c CODE_OFFSET
PAGE = 0x4000
CMDLINE = b"a50 loader test 1"                     # paytest.c cmdline_ok
ADT = b"A50TADT!" + bytes(i & 0xFF for i in range(8, 256))   # paytest.c adt_ok


def build(code, phys_base, mem_size):
    if code[56:60] != b"ARMd":
        raise SystemExit("not an A50 shim Image (no ARMd magic)")
    vmaddr = VIRT_BASE + CODE_OFFSET
    vmsize = (len(code) + PAGE - 1) & ~(PAGE - 1)
    kernel_end = vmaddr + vmsize
    out = bytearray(SEG_TABLE + 32)
    seg_off = len(out)
    out += code
    out += b"\0" * (-len(out) & 0xF)
    adt_off = len(out)
    out += ADT
    out += b"\0" * (-len(out) & 0xF)
    struct.pack_into("<8sII", out, 0, b"SWEETXNU", 1, 1)
    struct.pack_into("<7Q", out, 0x10, VIRT_BASE, vmaddr, adt_off, len(ADT), kernel_end,
                     phys_base, mem_size)
    out[CMDLINE_OFF:CMDLINE_OFF + len(CMDLINE)] = CMDLINE
    struct.pack_into("<4Q", out, SEG_TABLE, vmaddr, vmsize, seg_off, len(code))
    return bytes(out)


def check(blob, code, phys_base, mem_size):
    """Re-read the header the way xnu_boot.c does."""
    magic, version, nsegs = struct.unpack_from("<8sII", blob, 0)
    vb, entry, adt_off, adt_size, kend, pb, ms = struct.unpack_from("<7Q", blob, 0x10)
    vmaddr, vmsize, off, size = struct.unpack_from("<4Q", blob, SEG_TABLE)
    assert (magic, version, nsegs) == (b"SWEETXNU", 1, 1)
    assert (vb, pb, ms) == (VIRT_BASE, phys_base, mem_size)
    assert blob[off:off + size] == code and size <= vmsize
    assert blob[adt_off:adt_off + adt_size] == ADT
    assert entry == vmaddr and pb + (entry - vb) == phys_base + CODE_OFFSET
    assert kend >= vmaddr + vmsize and pb + (kend - vb) < pb + ms
    assert blob[CMDLINE_OFF:CMDLINE_OFF + len(CMDLINE) + 1] == CMDLINE + b"\0"


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("image", type=Path, help="shim/a50/out/paytest.Image")
    ap.add_argument("out", type=Path)
    ap.add_argument("--phys-base", type=lambda s: int(s, 0), default=0x90000000)
    ap.add_argument("--mem-mib", type=int, default=512)
    a = ap.parse_args()
    if a.out.exists():
        raise SystemExit("output exists; choose a new path")
    code = a.image.read_bytes()
    mem_size = a.mem_mib << 20
    blob = build(code, a.phys_base, mem_size)
    check(blob, code, a.phys_base, mem_size)
    a.out.write_bytes(blob)
    print(f"{a.out}: {len(blob)} bytes, entry PA {a.phys_base + CODE_OFFSET:#x}, "
          f"RAM {a.phys_base:#x}+{mem_size >> 20} MiB")


if __name__ == "__main__":
    main()
