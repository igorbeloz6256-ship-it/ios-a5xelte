/* SPDX-License-Identifier: GPL-2.0-only
 * Samsung Galaxy A50 (SM-A505FN, Exynos 9610) stage-0 handoff probe,
 * adapted from shim/herolte/probe.S.
 * Diagnostic only: no storage, EFS, PMIC or MMU changes. It records the
 * S-Boot handoff on the firmware framebuffer, then requests recovery the
 * way the A50 kernel's sec_reboot.c does: INFORM2[23:16] = 0x4E (normal,
 * not LPM charging), INFORM3 = 0x12345674 (recovery), PMU SWRESET = 1.
 *
 * The arm64 Image header fields text_offset/image_size/flags are rewritten
 * by tools/a50-pack-boot.py from the stock kernel's header.
 */
.section .text.head, "ax"
.global _start
_start:
    b entry
    .long 0
    .quad 0x80000            /* text_offset: replaced by the packer */
    .quad 0                  /* image_size: replaced by the packer */
    .quad 0                  /* flags: replaced by the packer */
    .quad 0, 0, 0
    .byte 0x41, 0x52, 0x4d, 0x64
    .long 0

entry:
    msr daifset, #15
    mov x19, x0              /* FDT from S-Boot */
    mrs x20, CurrentEL
    mrs x21, sctlr_el1
    adr x22, _start
    adr x14, a50_probe_stack_top
    mov sp, x14
    mov x0, x19
    mov x1, x20
    mov x2, x21
    mov x3, x22
    bl a50_probe_main       /* returns only to request recovery */

    /* Exynos9610 PMU 0x11860000 (exynos9610.dts pmu_system_controller). */
    ldr x0, =0x11860000
    ldr w14, [x0, #0x808]    /* INFORM2: keep all but bits 23:16 */
    bic w14, w14, #0x00ff0000
    ldr w15, =0x004e0000     /* not a valid logical immediate */
    orr w14, w14, w15
    str w14, [x0, #0x808]
    ldr w14, =0x12345674     /* INFORM3: SEC_RESET_REASON_RECOVERY */
    str w14, [x0, #0x80c]
    dsb sy
    mov w14, #1
    str w14, [x0, #0x400]    /* SWRESET */
    dsb sy
halt:
    wfe
    b halt
    .ltorg

.section .data, "aw"
.balign 16
a50_probe_stack:
    .space 8192
a50_probe_stack_top:
