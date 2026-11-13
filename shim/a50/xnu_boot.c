/* SPDX-License-Identifier: GPL-2.0-only
 * Galaxy A50 S-Boot loader for mkpayload.py version 1 (adapted from
 * shim/herolte/xnu_boot.c). Single CPU, 4 KiB XNU.
 *
 * XNU RAM: the window 0x90000000..0xb0000000 or the second DRAM bank below
 * Samsung uH's EL2_EARLYMEM (0x880000000..0x8fe600000). Both were free in
 * the phone's DTB (probe 1, 2026-11-13), and the loader re-checks the chosen
 * range against /memory and /reserved-memory before writing anything there.
 *
 * The payload is appended to this Image (kernel slot, loaded by S-Boot at
 * 0x80240000), so it must end below uH's EL2_CODE at 0x87000000. The initrd
 * (S-Boot puts it at 0x89000000, 16 MiB below the FDT) is only a fallback.
 */
#include <stdint.h>
#include "../fdt.h"
#include "a50.h"
#include "fbtext.h"
#include "fdtinfo.h"
#include "reboot.h"

#define PAGE_SIZE 4096UL
#define BOOT_LINE_LENGTH 608
#define FAIL_DELAY_S 30U
#define MAX_RANGES 32U

struct payload_seg {
    uint64_t vmaddr, vmsize, file_off, file_size;
};
struct payload {
    char magic[8];
    uint32_t version, nsegs;
    uint64_t virt_base, entry_va, adt_off, adt_size, kernel_end;
    uint64_t phys_base, mem_size;
    char cmdline[BOOT_LINE_LENGTH];
    struct payload_seg segs[];
};
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

_Static_assert(__builtin_offsetof(struct payload, segs) == 0x2a8, "payload layout");
_Static_assert(__builtin_offsetof(struct boot_args, CommandLine) == 108, "boot_args layout");
_Static_assert(sizeof(struct boot_args) == 736, "boot_args size");

static void zero_bytes(uintptr_t dst, uint64_t size)
{
    volatile uint8_t *d = (volatile uint8_t *)dst;
    for (uint64_t i = 0; i < size; ++i)
        d[i] = 0;
}

static void copy_bytes(uintptr_t dst, uintptr_t src, uint64_t size)
{
    volatile uint8_t *d = (volatile uint8_t *)dst;
    volatile const uint8_t *s = (volatile const uint8_t *)src;
    for (uint64_t i = 0; i < size; ++i)
        d[i] = s[i];
}

static void clean_bytes(uintptr_t start, uintptr_t end)
{
    uint64_t ctr;
    __asm__ volatile("mrs %0, CTR_EL0" : "=r" (ctr));
    uintptr_t line = 4UL << ((ctr >> 16) & 0xf);
    for (uintptr_t p = start & ~(line - 1); p < end; p += line)
        __asm__ volatile("dc cvac, %0" : : "r" (p) : "memory");
    __asm__ volatile("dsb sy" : : : "memory");
}

static int magic_ok(const char *value)
{
    const char *want = "SWEETXNU";
    for (unsigned i = 0; i < 8; ++i)
        if (value[i] != want[i])
            return 0;
    return 1;
}

static uintptr_t align_page(uintptr_t value)
{
    return (value + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
}

static int overlaps(uint64_t a, uint64_t a_end, uint64_t b, uint64_t b_end)
{
    return a < b_end && b < a_end;
}

/* Show why the loader stopped, give time for a photo, then return 0 so
 * xnu_boot.S requests TWRP. */
static uintptr_t fail(volatile uint64_t *log, int64_t step, const char *why,
                      const char *detail)
{
    log[10] = (uint64_t)step;
    clean_bytes(A50_LOG_BASE, A50_LOG_BASE + PAGE_SIZE);
    a50_fb_text(5, "LOADER FAIL:");
    a50_fb_text(6, why);
    a50_fb_text(7, detail ? detail : "");
    uint64_t freq = a50_counter_hz(), t0 = a50_counter();
    unsigned shown = ~0U;
    for (;;) {
        uint64_t elapsed = (a50_counter() - t0) / freq;
        if (elapsed >= FAIL_DELAY_S)
            return 0;
        unsigned left = FAIL_DELAY_S - (unsigned)elapsed;
        if (left != shown) {
            char line[] = "TWRP IN 00 S";
            line[8] = (char)('0' + left / 10);
            line[9] = (char)('0' + left % 10);
            a50_fb_text(8, line);
            shown = left;
        }
    }
}

/* The XNU RAM [base, end) and the log page must lie inside one /memory
 * range and outside every /reserved-memory child and /memreserve/ entry. */
static void copy_name(char *dst, const char *src)
{
    unsigned i = 0;
    for (; i < 12 && src[i]; ++i)
        dst[i] = src[i];
    dst[i] = 0;
}

static const char *check_ram(uintptr_t fdt, uint64_t base, uint64_t end, char name[13])
{
    struct fdt_range r[MAX_RANGES];
    name[0] = 0;
    unsigned n = fdt_memory(fdt, r, MAX_RANGES);
    if (!n || n > MAX_RANGES)
        return "FDT MEMORY UNREADABLE";
    int inside = 0, log_inside = 0;
    for (unsigned i = 0; i < n; ++i) {
        uint64_t rb = r[i].base, re = r[i].base + r[i].size;
        if (base >= rb && end <= re)
            inside = 1;
        if (A50_LOG_BASE >= rb && A50_LOG_BASE + PAGE_SIZE <= re)
            log_inside = 1;
    }
    if (!inside)
        return "XNU RAM NOT IN FDT MEMORY";
    if (!log_inside)
        return "LOG PAGE NOT IN FDT MEMORY";

    for (int pass = 0; pass < 2; ++pass) {
        n = pass ? fdt_memreserve(fdt, r, MAX_RANGES) : fdt_reserved(fdt, r, MAX_RANGES);
        if (n > MAX_RANGES)
            return "TOO MANY RESERVED RANGES";
        for (unsigned i = 0; i < n; ++i) {
            uint64_t rb = r[i].base, re = r[i].base + r[i].size;
            if (overlaps(base, end, rb, re) ||
                overlaps(A50_LOG_BASE, A50_LOG_BASE + PAGE_SIZE, rb, re)) {
                copy_name(name, pass ? "MEMRESERVE" : r[i].name);
                return "XNU RAM HITS RESERVED";
            }
        }
    }
    return 0;
}

/* Returns entry PA or zero. Status stays in the log page at A50_LOG_BASE. */
uintptr_t a50_prepare_xnu(uintptr_t fdt, volatile uint64_t *log, uint64_t el, uint64_t sctlr)
{
    uint64_t rd_start, rd_end;
    a50_fb_init();
    log[77] = a50_fb.base;
    a50_fb_text(0, "A50 XNU LOADER 1");
    if (el != 4)
        return fail(log, -10, "NOT EL1", 0);
    if (sctlr & 1)
        return fail(log, -11, "MMU ALREADY ON", 0);
    if (!fdt || (fdt & 7))
        return fail(log, -12, "NO FDT FROM S-BOOT", 0);

    /* Appended payload first: its length is bounded by the Image header. */
    extern char _start[], _image_end[];
    uint64_t image_size = *(volatile uint64_t *)(uintptr_t)(_start + 16);
    rd_start = (uintptr_t)_image_end;
    rd_end = (uintptr_t)_start + image_size;
    if (rd_end <= rd_start || !magic_ok((const char *)rd_start)) {
        if (!fdt_chosen_u64(fdt, "linux,initrd-start", &rd_start) ||
            !fdt_chosen_u64(fdt, "linux,initrd-end", &rd_end) ||
            rd_end <= rd_start)
            return fail(log, -1, "NO PAYLOAD", 0);
    }
    /* The payload may not reach uH's EL2_CODE or the XNU window. */
    if (rd_start < A50_DRAM_LOW_BASE || rd_end > A50_XNU_LOW_BASE ||
        overlaps(rd_start, rd_end, A50_EL2_CODE_BASE, A50_EL2_CODE_BASE + 0x200000UL))
        return fail(log, -1, "PAYLOAD IN BAD PLACE", 0);
    log[11] = rd_start;
    log[12] = rd_end;

    const struct payload *p = (const struct payload *)(uintptr_t)rd_start;
    uint64_t length = rd_end - rd_start;
    if (length < sizeof(*p) || !magic_ok(p->magic) || p->version != 1 ||
        !p->nsegs || p->nsegs > 32 ||
        __builtin_offsetof(struct payload, segs) + p->nsegs * sizeof(struct payload_seg) > length ||
        p->adt_off > length || p->adt_size > length - p->adt_off)
        return fail(log, -2, "BAD SWEETXNU PAYLOAD", 0);
    int in_low = p->phys_base >= A50_XNU_LOW_BASE && p->phys_base < A50_XNU_LOW_END;
    int in_bank2 = p->phys_base >= A50_XNU_BANK2_BASE && p->phys_base < A50_XNU_BANK2_END;
    uint64_t window_end = in_bank2 ? A50_XNU_BANK2_END : A50_XNU_LOW_END;
    if (!(in_low || in_bank2) || (p->phys_base & (PAGE_SIZE - 1)) ||
        !p->mem_size || p->mem_size > window_end - p->phys_base ||
        p->entry_va < p->virt_base || p->kernel_end < p->virt_base)
        return fail(log, -3, "PAYLOAD RAM OUTSIDE WINDOW", 0);
    a50_fb_hex2(1, "XNU RAM", p->phys_base, p->mem_size >> 20, "MIB");
    log[13] = p->phys_base;
    log[14] = p->mem_size;
    uintptr_t memory_end = p->phys_base + p->mem_size;
    uintptr_t kernel_end = p->phys_base + (p->kernel_end - p->virt_base);
    if (kernel_end >= memory_end || kernel_end < p->phys_base)
        return fail(log, -3, "KERNEL TOO LARGE", 0);

    char where[13];
    const char *why = check_ram(fdt, p->phys_base, memory_end, where);
    if (why)
        return fail(log, -7, why, where);

    a50_fb_text(2, "COPYING KERNEL");
    for (uint32_t i = 0; i < p->nsegs; ++i) {
        const struct payload_seg *s = &p->segs[i];
        if (s->vmaddr < p->virt_base || s->file_size > s->vmsize ||
            s->file_off > length || s->file_size > length - s->file_off ||
            s->vmsize > p->mem_size || s->vmaddr - p->virt_base > p->mem_size - s->vmsize)
            return fail(log, -4, "BAD SEGMENT", 0);
        uintptr_t dst = p->phys_base + (s->vmaddr - p->virt_base);
        copy_bytes(dst, rd_start + s->file_off, s->file_size);
        zero_bytes(dst + s->file_size, s->vmsize - s->file_size);
        clean_bytes(dst, dst + s->vmsize);
    }
    log[10] = 4;
    uintptr_t adt_pa = kernel_end;
    uintptr_t ba_pa = align_page(adt_pa + p->adt_size);
    if (ba_pa + PAGE_SIZE > memory_end || ba_pa < adt_pa)
        return fail(log, -5, "NO ROOM FOR BOOT ARGS", 0);
    copy_bytes(adt_pa, rd_start + p->adt_off, p->adt_size);
    zero_bytes(ba_pa, PAGE_SIZE);
    volatile struct boot_args *ba = (volatile struct boot_args *)ba_pa;
    ba->Revision = 2;
    ba->Version = 2;
    ba->virtBase = p->virt_base;
    ba->physBase = p->phys_base;
    ba->memSize = p->mem_size;
    ba->topOfKernelData = ba_pa + PAGE_SIZE;
    ba->deviceTreeP = p->virt_base + (adt_pa - p->phys_base);
    ba->deviceTreeLength = (uint32_t)p->adt_size;
    for (unsigned i = 0; i < BOOT_LINE_LENGTH; ++i)
        ba->CommandLine[i] = p->cmdline[i];
    ba->memSizeActual = p->mem_size;
    clean_bytes(adt_pa, ba_pa + PAGE_SIZE);

    uintptr_t entry_pa = p->phys_base + (p->entry_va - p->virt_base);
    if (entry_pa >= kernel_end)
        return fail(log, -6, "ENTRY OUTSIDE KERNEL", 0);
    log[15] = entry_pa;
    log[16] = p->nsegs;
    log[17] = ba_pa;
    log[10] = 5;
    a50_fb_hex(2, "ENTRY", entry_pa);
    a50_fb_hex(3, "BOOTARGS", ba_pa);
    a50_fb_text(4, "JUMP TO XNU");
    clean_bytes(A50_LOG_BASE, A50_LOG_BASE + PAGE_SIZE);
    __asm__ volatile("ic iallu; dsb sy; isb" : : : "memory");
    return entry_pa;
}
