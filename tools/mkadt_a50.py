#!/usr/bin/env python3
"""Build the Galaxy A50 (SM-A505FN, Exynos 9610) ADT for the A50 XNU config.

Derived from mkadt_herolte.py. Hardware from the A50 kernel DTS
(exynos9610.dts) and the stage-0 probe on the phone (A50_PROGRESS.md,
2026-11-13). Only what the kernel needs to come up on one CPU: no storage,
touch, GPU or Wi-Fi nodes yet (the A50 kernel leaves the S7 drivers off).

/product (the iPhone X identity for the iOS userland) comes from
research/ipsw-x/d22ap.adt when it exists, as in mkadt.py; without it the
tree still boots the kernel, but SpringBoard would not.
"""
import argparse
import os
from pathlib import Path

import adt
import mkadt

# The window xnu_boot checked against the phone's FDT (loader test,
# A50_PROGRESS.md): free of every /reserved-memory node, below VIPX at
# 0xb8000000. --bank2: the second DRAM bank below Samsung uH's EL2_EARLYMEM.
DRAM_BASE, DRAM_SIZE = 0x90000000, 0x20000000
BANK2_BASE, BANK2_SIZE = 0x880000000, 0x8fe600000 - 0x880000000
SOC_BASE = 0x10000000       # arm-io ranges base; child regs are relative
FB_BASE, FB_WIDTH, FB_HEIGHT = 0xca000000, 1080, 2340
# exynos9610.dts gic: GIC-400, distributor and CPU interface.
GICD, GICC = 0x12301000, 0x12302000
VTIMER_INTID = 27           # arm,armv8-timer: virtual timer PPI 11
TIMEBASE = 26000000         # probe 1: CNTFRQ_EL0 = 0x18cba80
BOOT_MPIDR = 0x0            # probe 1: MPIDR_EL1 = 0x80000000, Cortex-A53 r0p4
UFS_BASE, UFS_MAP = 0x13520000, 0x2000   # HCI + vendor HCI (0x13521100..0x13521300)


def set_prop(node, name, value):
    for i, (key, flags, _) in enumerate(node.props):
        if key == name:
            node.props[i] = (key, flags, value)
            return
    node.props.append((name, 0, value))


def child(node, name, required=True):
    for candidate in node.children:
        if candidate.prop('name') == mkadt.cstr(name):
            return candidate
    if required:
        raise ValueError(f'missing donor ADT node {name}')
    return None


def build(canvas=(FB_WIDTH, FB_HEIGHT), dram=(DRAM_BASE, DRAM_SIZE)):
    root = mkadt.build(seed=os.urandom(64), qemu=True)
    # compatible[0]/model: the iPhone X userland (D22AP), as on herolte.
    # 'samsung,a5xelte' keeps SweetPlatformExpert's IONameMatch.
    set_prop(root, 'compatible', mkadt.cstr('D22AP') + mkadt.cstr('iPhone10,3') +
             mkadt.cstr('samsung,a5xelte') + mkadt.cstr('samsung,a50') +
             mkadt.cstr('samsung,exynos9610') + mkadt.cstr('AppleARM'))
    chosen = child(root, 'chosen')
    set_prop(chosen, 'dram-base', mkadt.u64(dram[0]))
    set_prop(chosen, 'dram-size', mkadt.u64(dram[1]))
    set_prop(chosen, 'firmware-version', mkadt.cstr('a50-entry'))
    set_prop(chosen, 'system-firmware-version', mkadt.cstr('a50-entry'))
    # Synthetic, locally administered addresses (see mkadt_herolte.py).
    for name, last in (('wifi0', 1), ('bluetooth0', 2), ('ethernet0', 3)):
        set_prop(chosen, f'mac-address-{name}', bytes((0x02, 0x41, 0x35, 0x30, 0x00, last)))
    product = child(root, 'product', required=False)
    if product is not None:
        set_prop(product, 'artwork-display-gamut', mkadt.cstr('sRGB'))

    # One CPU: the boot Cortex-A53 (SWEET_SMP_MAX_CPUS 1 in the A50 kernel;
    # SweetCPU panics on any other MPIDR or count).
    cpu0 = child(child(root, 'cpus'), 'cpu0')
    set_prop(cpu0, 'compatible', mkadt.cstr('arm,cortex-a53') + mkadt.cstr('ARM,v8'))
    set_prop(cpu0, 'reg', mkadt.u32(BOOT_MPIDR))
    set_prop(cpu0, 'timebase-frequency', mkadt.u32(TIMEBASE))
    set_prop(cpu0, 'peripheral-frequency', mkadt.u32(TIMEBASE))
    set_prop(cpu0, 'fixed-frequency', mkadt.u32(TIMEBASE))
    set_prop(cpu0, 'clock-frequency', mkadt.u32(1742000000))

    arm_io = child(root, 'arm-io')
    set_prop(arm_io, 'device_type', mkadt.cstr('a50-io'))
    set_prop(arm_io, 'compatible', mkadt.cstr('samsung,exynos9610-io') + mkadt.cstr('arm-io'))
    set_prop(arm_io, 'ranges', mkadt.u64(0, SOC_BASE, 0xf0000000))
    arm_io.children = [n for n in arm_io.children if n.prop('name') == mkadt.cstr('wdt')]
    arm_io.children.append(mkadt.node('gic', [
        ('compatible', mkadt.cstr('arm,cortex-a15-gic')),
        ('interrupt-controller', mkadt.cstr('master')),
        ('virtual-timer-intid', mkadt.u32(VTIMER_INTID)),
        ('reg', mkadt.u64(GICD - SOC_BASE, 0x1000, GICC - SOC_BASE, 0x1000)),
    ]))
    arm_io.children.append(mkadt.node('disp0', [
        ('device_type', mkadt.cstr('display-subsystem')),
        # AppleCLCD (SweetFramebuffer.cpp) matches 'samsung,a5xelte-display'.
        ('compatible', mkadt.cstr('samsung,a5xelte-display') + mkadt.cstr('samsung,a50-display')),
        ('canvas-width', mkadt.u32(canvas[0])),
        ('canvas-height', mkadt.u32(canvas[1])),
    ]))
    # No sweet-fbcon/sweet-ufs (Qualcomm donor) and no herolte-ufs (S7).
    root.children = [n for n in root.children if n.prop('name') not in
                     (mkadt.cstr('sweet-fbcon'), mkadt.cstr('sweet-ufs'))]
    # Exynos 9610 UFSHCI (exynos9610.dts ufs@0x13520000): standard HCI at +0,
    # vendor HCI at +0x1100 (reg 1, 0x13521100). Absolute address, read by
    # bsd/dev/herolte_ufs.c in the A50 kernel (ufsprobe=1, rd=ufsroot0).
    root.children.append(mkadt.node('a50-ufs', [
        ('reg', mkadt.u64(UFS_BASE, UFS_MAP)),
    ]))
    # pe_serial.c looks this node up by name for the kprintf console.
    root.children.append(mkadt.node('a5xelte-fbcon', [
        ('reg', mkadt.u64(FB_BASE, FB_WIDTH * FB_HEIGHT * 4)),
        ('width', mkadt.u32(FB_WIDTH)),
        ('height', mkadt.u32(FB_HEIGHT)),
        ('stride', mkadt.u32(FB_WIDTH * 4)),
    ]))
    return root


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('output', type=Path)
    parser.add_argument('--canvas', default=f'{FB_WIDTH}x{FB_HEIGHT}', help='presented display WxH')
    parser.add_argument('--bank2', action='store_true',
                        help='XNU RAM = the second bank (below uH EL2_EARLYMEM) instead of 512 MiB at 0x90000000')
    parser.add_argument('--dump', action='store_true')
    args = parser.parse_args()
    canvas = tuple(int(v) for v in args.canvas.lower().split('x'))
    dram = (BANK2_BASE, BANK2_SIZE) if args.bank2 else (DRAM_BASE, DRAM_SIZE)
    blob = adt.serialize(build(canvas, dram))
    args.output.write_bytes(blob)
    if args.dump:
        adt.dump(adt.parse(blob)[0])
    print(f'{args.output}: {len(blob)} bytes, XNU RAM {dram[0]:#x}+{dram[1] >> 20} MiB')
