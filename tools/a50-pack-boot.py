#!/usr/bin/env python3
"""Inspect or repack a Galaxy A50 (SM-A505FN) BOOT image.

Host-side only: never opens a block device and never flashes the phone.
The stock BOOT may contain private data; keep it and every output off Git.

  info  STOCK                    print the header, the kernel format and
                                 trailing signatures (AVB footer, SEANDROID)
  pack  STOCK IMAGE OUT [--append P] [--ramdisk R] [--partition-size N]
        replace the kernel with IMAGE (probe or XNU loader), keep every other
        section of the stock image, optionally append a SWEETXNU payload to
        IMAGE, and copy the stock arm64 Image header fields.

Handles Android boot header versions 0, 1 and 2, plus the old Samsung layout
where the version field holds a DTBH size. The AVB footer of the stock image
is dropped: the result only boots with an unlocked bootloader and a vbmeta
that has verification disabled (the usual TWRP setup on the A50).
"""
import argparse
import hashlib
from pathlib import Path
import struct
import sys

ARM64_MAGIC = b"ARMd"
SEANDROID = b"SEANDROIDENFORCE"
DEFAULT_TEXT_OFFSET = 0x80000
DEFAULT_FLAGS = 0x0A   # little endian, 4 KiB pages, anywhere in RAM
A50_IMAGE_LOAD = 0x80240000   # where S-Boot put the probe Image (A50_PROGRESS.md)
A50_EL2_CODE = 0x87000000     # Samsung uH reserved-memory el2_code


def parse(boot):
    if boot[:8] != b"ANDROID!":
        raise SystemExit("not an Android BOOT image")
    (kernel_size, kernel_addr, ramdisk_size, ramdisk_addr, second_size,
     second_addr, tags_addr, page, version, os_version) = struct.unpack_from("<10I", boot, 8)
    if page not in (2048, 4096):
        raise SystemExit(f"unexpected page size {page}")
    samsung_dt = 0
    if version > 2:
        # Old Samsung layout (e.g. herolte): the field is the DTBH size.
        samsung_dt, version = version, 0
    dtbo_size = dtb_size = 0
    dtbo_off = dtb_addr = 0
    header_size = 0
    if version >= 1:
        dtbo_size, dtbo_off, header_size = struct.unpack_from("<IQI", boot, 1632)
    if version >= 2:
        dtb_size, dtb_addr = struct.unpack_from("<IQ", boot, 1648)
    align = lambda size: (size + page - 1) // page * page
    sizes = [("kernel", kernel_size), ("ramdisk", ramdisk_size), ("second", second_size),
             ("recovery_dtbo", dtbo_size), ("dtb", dtb_size), ("samsung_dt", samsung_dt)]
    off = page
    parts = {}
    for name, size in sizes:
        if off + size > len(boot):
            raise SystemExit(f"image is truncated in {name}")
        parts[name] = boot[off:off + size]
        off += align(size)
    return {
        "version": version, "page": page, "align": align, "end": off,
        "kernel_addr": kernel_addr, "ramdisk_addr": ramdisk_addr,
        "second_addr": second_addr, "tags_addr": tags_addr,
        "os_version": os_version, "header_size": header_size,
        "dtbo_off": dtbo_off, "dtb_addr": dtb_addr, "samsung_dt": bool(samsung_dt),
        "name": boot[48:64].split(b"\0")[0].decode(errors="replace"),
        "cmdline": boot[64:576].split(b"\0")[0].decode(errors="replace"),
        "parts": parts, "tail": boot[off:],
    }


def kernel_format(kernel):
    if len(kernel) >= 64 and kernel[56:60] == ARM64_MAGIC:
        text_offset, image_size, flags = struct.unpack_from("<3Q", kernel, 8)
        return "raw arm64 Image", (text_offset, image_size, flags)
    for magic, name in ((b"\x1f\x8b", "gzip"), (b"\x02\x21\x4c\x18", "lz4 legacy"),
                        (b"\x04\x22\x4d\x18", "lz4 frame"), (b"\x5d\x00\x00", "lzma")):
        if kernel.startswith(magic):
            return f"{name}-compressed kernel", None
    return f"unknown kernel (starts {kernel[:4].hex()})", None


def boot_id(b):
    """mkbootimg's SHA-1 over each section and its size."""
    digest = hashlib.sha1()
    names = ["kernel", "ramdisk", "second"]
    if b["samsung_dt"]:
        names.append("samsung_dt")
    if b["version"] >= 1:
        names.append("recovery_dtbo")
    if b["version"] >= 2:
        names.append("dtb")
    for name in names:
        part = b["parts"][name]
        digest.update(part)
        digest.update(struct.pack("<I", len(part)))
    return digest.digest()


def cmd_info(args):
    boot = args.stock.read_bytes()
    b = parse(boot)
    print(f"bytes={len(boot)} sha256={hashlib.sha256(boot).hexdigest()}")
    print(f"header_version={b['version']} page={b['page']} name={b['name']!r}"
          f" os_version={b['os_version']:#x} header_size={b['header_size']}")
    print(f"kernel_addr={b['kernel_addr']:#x} ramdisk_addr={b['ramdisk_addr']:#x}"
          f" second_addr={b['second_addr']:#x} tags_addr={b['tags_addr']:#x}"
          f" dtb_addr={b['dtb_addr']:#x}")
    for name, part in b["parts"].items():
        if part:
            print(f"  {name}: {len(part)} bytes")
    print(f"cmdline={b['cmdline']!r}")
    fmt, hdr = kernel_format(b["parts"]["kernel"])
    print(f"kernel: {fmt}")
    if hdr:
        print("  text_offset={:#x} image_size={:#x} flags={:#x}".format(*hdr))
    tail = b["tail"].rstrip(b"\0")
    print(f"data after sections: {len(b['tail'])} bytes ({len(tail)} non-zero-padded)")
    if SEANDROID in boot[b["end"]:b["end"] + 4096]:
        print("  SEANDROIDENFORCE marker present")
    if boot[-64:-60] == b"AVBf" or b"AVBf" in boot[-4096:]:
        print("  AVB footer present (will be dropped by pack)")


def cmd_pack(args):
    if args.output.exists():
        raise SystemExit("output exists; choose a new private path")
    stock = args.stock.read_bytes()
    b = parse(stock)
    image = bytearray(args.image.read_bytes())
    if image[56:60] != ARM64_MAGIC:
        raise SystemExit("IMAGE lacks the arm64 Image header")
    if args.append:
        # xnu_boot looks for the payload at its 4 KiB-aligned _image_end.
        payload = args.append.read_bytes()
        if payload[:8] != b"SWEETXNU":
            raise SystemExit("--append payload is not a SWEETXNU container")
        image.extend(b"\0" * (-len(image) % 4096))
        image.extend(payload)
    _, stock_hdr = kernel_format(b["parts"]["kernel"])
    if stock_hdr:
        text_offset, _, flags = stock_hdr
    else:
        print("warning: stock kernel header unreadable; using text_offset "
              f"{DEFAULT_TEXT_OFFSET:#x} flags {DEFAULT_FLAGS:#x}", file=sys.stderr)
        text_offset, flags = DEFAULT_TEXT_OFFSET, DEFAULT_FLAGS
    # image_size covers the loaded file plus 64 KiB of zeroed headroom.
    image_size = (len(image) + 0x10000 + 0xfff) & ~0xfff
    # A50 probe 1: S-Boot loads the Image at 0x80240000 and Samsung uH's
    # EL2_CODE starts at 0x87000000; the kernel slot must stay below it.
    if A50_IMAGE_LOAD + image_size > A50_EL2_CODE:
        raise SystemExit(f"Image+payload {image_size:#x} bytes would reach uH EL2_CODE "
                         f"({A50_IMAGE_LOAD:#x}+size > {A50_EL2_CODE:#x})")
    struct.pack_into("<3Q", image, 8, text_offset, image_size, flags)

    parts = dict(b["parts"])
    parts["kernel"] = bytes(image)
    if args.ramdisk:
        parts["ramdisk"] = args.ramdisk.read_bytes()
    # A50 Android 11 BOOT has no ramdisk (it lives in system); keep it empty then.
    if b["parts"]["ramdisk"] and not parts["ramdisk"]:
        raise SystemExit("stock BOOT has a ramdisk but the candidate would not")
    nb = dict(b, parts=parts)

    page, align = b["page"], b["align"]
    header = bytearray(stock[:page])
    struct.pack_into("<I", header, 8, len(parts["kernel"]))
    struct.pack_into("<I", header, 16, len(parts["ramdisk"]))
    if b["version"] >= 1:
        # recovery_dtbo_offset is absolute in the file; sections before it may move.
        off = page
        for name in ("kernel", "ramdisk", "second"):
            off += align(len(parts[name]))
        if parts["recovery_dtbo"]:
            struct.pack_into("<Q", header, 1636, off)
    header[576:608] = boot_id(nb).ljust(32, b"\0")
    out = bytearray(header)
    for name in ("kernel", "ramdisk", "second", "recovery_dtbo", "dtb", "samsung_dt"):
        part = parts[name]
        if part:
            out.extend(part)
            out.extend(b"\0" * (align(len(part)) - len(part)))
    # Unlocked S-Boot still looks for this marker; the stock signature is dropped.
    out.extend(SEANDROID)
    if args.partition_size and len(out) > args.partition_size:
        raise SystemExit(f"candidate {len(out)} exceeds BOOT partition {args.partition_size}")
    args.output.write_bytes(out)
    print("candidate only; no device write")
    print(f"stock_sha256={hashlib.sha256(stock).hexdigest()}")
    print(f"candidate_sha256={hashlib.sha256(out).hexdigest()}")
    print(f"bytes={len(out)} header_version={b['version']} kernel_addr={b['kernel_addr']:#x}")
    print(f"arm64 header: text_offset={text_offset:#x} image_size={image_size:#x} flags={flags:#x}")
    if not args.partition_size:
        print("warning: pass --partition-size (blockdev --getsize64 BOOT) to check the fit")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    info = sub.add_parser("info")
    info.add_argument("stock", type=Path)
    pack = sub.add_parser("pack")
    pack.add_argument("stock", type=Path)
    pack.add_argument("image", type=Path)
    pack.add_argument("output", type=Path)
    pack.add_argument("--ramdisk", type=Path)
    pack.add_argument("--append", type=Path,
                      help="SWEETXNU payload appended to IMAGE (kernel slot)")
    pack.add_argument("--partition-size", type=int)
    args = ap.parse_args()
    {"info": cmd_info, "pack": cmd_pack}[args.cmd](args)


if __name__ == "__main__":
    main()
