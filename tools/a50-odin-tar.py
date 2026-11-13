#!/usr/bin/env python3
"""Make Odin-flashable files for the Galaxy A50 TWRP setup.

Host-side only: never talks to the phone.

  vbmeta OUT.img
        write an empty AVB vbmeta image with flags=2 (verification disabled),
        the same layout `avbtool make_vbmeta_image --flags 2
        --padding_size 4096` produces (only the release string may differ).
        The unlocked A50 bootloader needs it before it will boot TWRP or a
        custom BOOT.

  tar OUT.tar FILE [FILE ...]
        pack partition images (e.g. recovery.img vbmeta.img) into a plain
        ustar archive for Odin's AP slot. Odin picks the partition from each
        member's file name, so the names must be partition names + ".img".
        Also writes OUT.tar.md5 (tar + md5 trailer), which Odin verifies.
"""
import argparse
import hashlib
import io
from pathlib import Path
import struct
import tarfile
import time

AVB_MAGIC = b"AVB0"
AVB_HEADER_SIZE = 256
AVB_FLAG_HASHTREE_DISABLED = 1
AVB_FLAG_VERIFICATION_DISABLED = 2
ALLOWED = {"recovery.img", "vbmeta.img", "boot.img"}


def make_vbmeta(flags=AVB_FLAG_VERIFICATION_DISABLED, padding=4096):
    h = bytearray(AVB_HEADER_SIZE)
    h[0:4] = AVB_MAGIC
    # required libavb version 1.0; every block size/offset is zero (no
    # signature, no descriptors); algorithm NONE.
    struct.pack_into(">II", h, 4, 1, 0)
    struct.pack_into(">QQ", h, 12, 0, 0)       # auth / aux block sizes
    struct.pack_into(">I", h, 28, 0)            # algorithm_type NONE
    struct.pack_into(">Q", h, 112, 0)           # rollback_index
    struct.pack_into(">II", h, 120, flags, 0)   # flags, rollback_index_location
    release = b"avbtool 1.1.0"
    h[128:128 + len(release)] = release
    return bytes(h) + b"\0" * (padding - AVB_HEADER_SIZE)


def check_vbmeta(data):
    if data[:4] != AVB_MAGIC or len(data) < AVB_HEADER_SIZE:
        raise SystemExit("not an AVB vbmeta image")
    return struct.unpack_from(">I", data, 120)[0]


def cmd_vbmeta(args):
    if args.output.exists():
        raise SystemExit("output exists; choose a new path")
    data = make_vbmeta()
    args.output.write_bytes(data)
    print(f"{args.output}: {len(data)} bytes, flags={check_vbmeta(data)} "
          f"sha256={hashlib.sha256(data).hexdigest()}")


def cmd_tar(args):
    out = args.output
    md5_out = out.with_name(out.name + ".md5")
    if out.exists() or md5_out.exists():
        raise SystemExit("output exists; choose a new path")
    names = [f.name for f in args.files]
    if len(set(names)) != len(names):
        raise SystemExit("duplicate partition image names")
    for f in args.files:
        if f.name not in ALLOWED:
            raise SystemExit(f"{f.name}: only {sorted(ALLOWED)} are allowed here "
                             "(never efs, param, sboot, modem)")
        data = f.read_bytes()
        if f.name == "vbmeta.img":
            print(f"vbmeta.img flags={check_vbmeta(data)}")
        elif data[:8] != b"ANDROID!":
            raise SystemExit(f"{f.name}: not an Android boot/recovery image "
                             "(unpack .img.tar / .lz4 first)")
    buf = io.BytesIO()
    now = int(time.time())
    with tarfile.open(fileobj=buf, mode="w", format=tarfile.USTAR_FORMAT) as tar:
        for f in args.files:
            data = f.read_bytes()
            info = tarfile.TarInfo(f.name)
            info.size = len(data)
            info.mode = 0o644
            info.mtime = now
            info.uname = info.gname = ""
            tar.addfile(info, io.BytesIO(data))
            print(f"  {f.name}: {len(data)} bytes sha256={hashlib.sha256(data).hexdigest()}")
    blob = buf.getvalue()
    out.write_bytes(blob)
    md5 = hashlib.md5(blob).hexdigest()
    md5_out.write_bytes(blob + f"{md5}  {out.name}\n".encode())
    print(f"{out}: {len(blob)} bytes")
    print(f"{md5_out}: md5 {md5}")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    v = sub.add_parser("vbmeta")
    v.add_argument("output", type=Path)
    t = sub.add_parser("tar")
    t.add_argument("output", type=Path)
    t.add_argument("files", type=Path, nargs="+")
    args = ap.parse_args()
    {"vbmeta": cmd_vbmeta, "tar": cmd_tar}[args.cmd](args)


if __name__ == "__main__":
    main()
