/* SPDX-License-Identifier: GPL-2.0-only
 * Payload test: check what xnu_boot handed over, as XNU would see it.
 * tools/a50-test-payload.py fixes the expected values: the code segment at
 * virt_base + 0x4000, a 256-byte test ADT ("A50TADT!" then bytes i & 0xff)
 * and the command line "a50 loader test 1".
 */
#include <stdint.h>
#include "fbtext.h"
#include "reboot.h"

#define BOOT_LINE_LENGTH 608
#define CODE_OFFSET 0x4000UL
#define ADT_SIZE 256U
#define DELAY_S 60U
#define ROW 10U          /* below the loader's lines */

struct boot_video {
    uint64_t v_baseAddr, v_display, v_rowBytes, v_width, v_height, v_depth;
};
struct boot_args {
    uint16_t Revision, Version;
    uint64_t virtBase, physBase, memSize, topOfKernelData;
    struct boot_video Video;
    uint32_t machineType;
    uint64_t deviceTreeP;
    uint32_t deviceTreeLength;
    char CommandLine[BOOT_LINE_LENGTH];
    uint64_t bootFlags, memSizeActual;
};

static int adt_ok(uintptr_t adt, uint32_t len)
{
    const char *magic = "A50TADT!";
    volatile const uint8_t *a = (volatile const uint8_t *)adt;
    if (len != ADT_SIZE)
        return 0;
    for (unsigned i = 0; i < 8; ++i)
        if (a[i] != (uint8_t)magic[i])
            return 0;
    for (unsigned i = 8; i < ADT_SIZE; ++i)
        if (a[i] != (uint8_t)i)
            return 0;
    return 1;
}

static int cmdline_ok(volatile const char *c)
{
    const char *want = "a50 loader test 1";
    unsigned i = 0;
    for (; want[i]; ++i)
        if (c[i] != want[i])
            return 0;
    return c[i] == 0;
}

void a50_paytest_main(uintptr_t ba_pa, uint64_t el, uint64_t sctlr, uintptr_t pc)
{
    a50_fb_init();
    unsigned row = ROW;
    a50_fb_text(row++, "PAYLOAD TEST 1: RUNNING");
    a50_fb_hex(row++, "EL", el >> 2);
    a50_fb_hex(row++, "SCTLR", sctlr);
    a50_fb_hex(row++, "PC", pc);
    a50_fb_hex(row++, "BOOTARGS", ba_pa);
    if (!ba_pa || (ba_pa & 7)) {
        a50_fb_text(row++, "BOOTARGS POINTER BAD");
    } else {
        volatile const struct boot_args *ba = (volatile const struct boot_args *)ba_pa;
        uint64_t vb = ba->virtBase, pb = ba->physBase;
        a50_fb_hex2(row++, "REV/VER", ba->Revision, ba->Version, "");
        a50_fb_hex(row++, "VIRTBASE", vb);
        a50_fb_hex2(row++, "PHYS/MEM", pb, (uint32_t)(ba->memSize >> 20), "MIB");
        a50_fb_hex(row++, "TOPKDATA", ba->topOfKernelData);
        a50_fb_hex2(row++, "ADT", ba->deviceTreeP - vb + pb, ba->deviceTreeLength, "");

        int ok_rev = ba->Revision == 2 && ba->Version == 2;
        int ok_pc = pc == pb + CODE_OFFSET;
        int ok_mmu = !(sctlr & 1);
        int ok_adt = adt_ok(ba->deviceTreeP - vb + pb, ba->deviceTreeLength);
        int ok_cmd = cmdline_ok(ba->CommandLine);
        int ok_top = ba->topOfKernelData == ba_pa + 4096;
        a50_fb_text(row++, ok_rev ? "REVISION OK" : "REVISION BAD");
        a50_fb_text(row++, ok_pc ? "PC OK" : "PC BAD");
        a50_fb_text(row++, ok_mmu ? "MMU OFF OK" : "MMU ON BAD");
        a50_fb_text(row++, ok_adt ? "ADT OK" : "ADT BAD");
        a50_fb_text(row++, ok_cmd ? "CMDLINE OK" : "CMDLINE BAD");
        a50_fb_text(row++, ok_top ? "TOPKDATA OK" : "TOPKDATA BAD");
        a50_fb_text(row++, ok_rev && ok_pc && ok_mmu && ok_adt && ok_cmd && ok_top
                    ? "ALL CHECKS PASSED" : "SOME CHECKS FAILED");
    }

    uint64_t freq = a50_counter_hz(), t0 = a50_counter();
    unsigned shown = ~0U, last = a50_fb_rows() - 1;
    for (;;) {
        uint64_t elapsed = (a50_counter() - t0) / freq;
        if (elapsed >= DELAY_S)
            return;
        unsigned left = DELAY_S - (unsigned)elapsed;
        if (left != shown) {
            char line[] = "TWRP IN 00 S";
            line[8] = (char)('0' + left / 10);
            line[9] = (char)('0' + left % 10);
            a50_fb_text(last, line);
            shown = left;
        }
    }
}
