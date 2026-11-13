/* SPDX-License-Identifier: GPL-2.0-only
 * Galaxy A50 stage-0 probe: print the S-Boot handoff on the firmware
 * framebuffer, then return so probe.S can request recovery (TWRP).
 * Diagnostic only: no storage, PMIC, clock or MMU changes. The only writes
 * outside our own Image are the scanout buffer, DECON_F HW_SW_TRIG_CONTROL
 * and, at the end, the PMU reboot registers.
 */
#include <stdint.h>
#include "../fdt.h"
#include "a50.h"
#include "fbtext.h"
#include "fdtinfo.h"

#define RECOVERY_DELAY_S 90U

static uint64_t counter(void)
{
    uint64_t value;
    __asm__ volatile("isb; mrs %0, cntpct_el0" : "=r" (value));
    return value;
}

static void show_ranges(unsigned *row, const char *tag, struct fdt_range *r,
                        unsigned found, unsigned max)
{
    unsigned shown = found < max ? found : max;
    for (unsigned i = 0; i < shown && *row + 2 < a50_fb_rows(); ++i) {
        char label[14];
        unsigned n = 0;
        while (tag[n] && n < 4) {
            label[n] = tag[n];
            ++n;
        }
        label[n++] = (char)('0' + i / 10);
        label[n++] = (char)('0' + i % 10);
        label[n] = 0;
        a50_fb_hex2((*row)++, label, r[i].base, r[i].size, r[i].name);
    }
    if (found > shown && *row + 2 < a50_fb_rows())
        a50_fb_hex((*row)++, "MORE", found - shown);
}

void a50_probe_main(uintptr_t fdt, uint64_t el, uint64_t sctlr, uint64_t image)
{
    uint64_t midr, mpidr, mmfr0, pfr0, isar0, freq, start = 0, end = 0;
    struct fdt_range ranges[12];

    a50_fb_init();

    __asm__ volatile("mrs %0, midr_el1" : "=r" (midr));
    __asm__ volatile("mrs %0, mpidr_el1" : "=r" (mpidr));
    __asm__ volatile("mrs %0, id_aa64mmfr0_el1" : "=r" (mmfr0));
    __asm__ volatile("mrs %0, id_aa64pfr0_el1" : "=r" (pfr0));
    __asm__ volatile("mrs %0, id_aa64isar0_el1" : "=r" (isar0));
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r" (freq));

    unsigned row = 0;
    a50_fb_text(row++, "A50 XNU PROBE 1");
    a50_fb_hex(row++, "EL", el >> 2);
    a50_fb_hex(row++, "SCTLR", sctlr);
    a50_fb_hex(row++, "FDT", fdt);
    a50_fb_hex(row++, "IMAGE", image);
    a50_fb_hex(row++, "MIDR", midr);
    a50_fb_hex(row++, "MPIDR", mpidr);
    a50_fb_hex(row++, "MMFR0", mmfr0);
    a50_fb_hex(row++, "PFR0", pfr0);
    a50_fb_hex(row++, "ISAR0", isar0);
    a50_fb_hex(row++, "CNTFRQ", freq);
    a50_fb_hex(row++, "TRIG OLD", a50_fb.trig_before);
    /* FB line: base, width<<16|height; then pitch and the source channel. */
    a50_fb_hex2(row++, "FB", a50_fb.base, (uint64_t)a50_fb.width << 16 | a50_fb.height, "");
    a50_fb_hex2(row++, "FB PITCH", a50_fb.pitch, (uint64_t)(uint32_t)a50_fb.idma,
                a50_fb.idma < 0 ? "DEFAULT" : "FROM DMA");
    for (unsigned i = 0; i < 4; ++i) {
        char label[] = "DMA0 B/S";
        label[3] = (char)('0' + i);
        a50_fb_hex2(row++, label, a50_fb.idma_base[i], a50_fb.idma_src[i], "");
        label[5] = 'E';
        label[7] = 'I';
        a50_fb_hex2(row++, label, a50_fb.idma_en[i], a50_fb.idma_img[i], "");
    }
    /* MMU off: an unaligned FDT pointer would fault on the first load. */
    if (fdt && !(fdt & 7) && fdt_chosen_u64(fdt, "linux,initrd-start", &start) &&
        fdt_chosen_u64(fdt, "linux,initrd-end", &end))
        a50_fb_hex2(row++, "INITRD", start, end, "");
    else
        a50_fb_text(row++, "INITRD ?");
    /* MMFR0[23:20] TGran16: 0 means no 16 KiB pages (needs the 4 KiB XNU). */
    a50_fb_text(row++, ((mmfr0 >> 20) & 15U) ? "16K PAGES: YES" : "16K PAGES: NO");

    unsigned count = fdt_memory(fdt, ranges, 4);
    show_ranges(&row, "MEM", ranges, count, 4);
    count = fdt_memreserve(fdt, ranges, 8);
    show_ranges(&row, "RSV", ranges, count, 8);
    count = fdt_reserved(fdt, ranges, 12);
    show_ranges(&row, "RM", ranges, count, 12);

    if (!freq)
        freq = A50_TIMER_HZ;
    unsigned last = a50_fb_rows() - 1;
    uint64_t t0 = counter();
    unsigned shown = ~0U;
    for (;;) {
        uint64_t elapsed = (counter() - t0) / freq;
        if (elapsed >= RECOVERY_DELAY_S)
            return;
        unsigned left = RECOVERY_DELAY_S - (unsigned)elapsed;
        if (left != shown) {
            char line[] = "TWRP IN 00 S";
            line[8] = (char)('0' + left / 10);
            line[9] = (char)('0' + left % 10);
            a50_fb_text(last, line);
            shown = left;
        }
    }
}
