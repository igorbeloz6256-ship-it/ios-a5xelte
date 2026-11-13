/* SPDX-License-Identifier: GPL-2.0-only
 * Stand-in for XNU in a SWEETXNU payload (tools/a50-test-payload.py).
 * xnu_boot jumps here exactly as it would to XNU's entry: MMU off,
 * x0 = boot_args physical address. Shows what it received, then requests
 * TWRP. The arm64 Image header is unused here but keeps the layout of the
 * other A50 Images.
 */
.section .text.head, "ax"
.global _start
_start:
    b entry
    .long 0
    .quad 0, 0, 0, 0, 0, 0
    .byte 0x41, 0x52, 0x4d, 0x64
    .long 0

entry:
    msr daifset, #15
    mov x19, x0              /* boot_args PA from xnu_boot */
    mrs x20, CurrentEL
    mrs x21, sctlr_el1
    adr x22, _start
    adr x14, a50_paytest_stack_top
    mov sp, x14
    mov x0, x19
    mov x1, x20
    mov x2, x21
    mov x3, x22
    bl a50_paytest_main
    bl a50_reboot_recovery    /* does not return */
halt:
    wfe
    b halt

.section .data, "aw"
.balign 16
a50_paytest_stack:
    .space 8192
a50_paytest_stack_top:
