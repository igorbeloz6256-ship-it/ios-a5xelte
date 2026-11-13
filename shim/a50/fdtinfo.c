/* SPDX-License-Identifier: GPL-2.0-only
 * Read-only FDT walk for the stage-0 probe: memory, /memreserve/ and
 * /reserved-memory as S-Boot hands them over. Big-endian, 4-byte tokens,
 * MMU off: only aligned 32-bit loads and byte loads.
 */
#include "fdtinfo.h"

#define FDT_MAGIC      0xd00dfeedU
#define FDT_BEGIN_NODE 1U
#define FDT_END_NODE   2U
#define FDT_PROP       3U
#define FDT_NOP        4U
#define FDT_END        9U
#define MAX_DEPTH      16

static uint32_t be32(uintptr_t a)
{
    return __builtin_bswap32(*(volatile uint32_t *)a);
}

static uint64_t be64(uintptr_t a)
{
    return (uint64_t)be32(a) << 32 | be32(a + 4);
}

static int starts(const char *s, const char *prefix)
{
    while (*prefix)
        if (*s++ != *prefix++)
            return 0;
    return 1;
}

static int streq(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}

static uintptr_t align4(uintptr_t a)
{
    return (a + 3) & ~(uintptr_t)3;
}

static uintptr_t skip_str(uintptr_t p)
{
    while (*(volatile char *)p)
        p++;
    return p + 1;
}

static uint64_t cells(uintptr_t p, uint32_t n)
{
    return n == 2 ? be64(p) : n == 1 ? be32(p) : 0;
}

static void copy_name(char *dst, const char *src)
{
    unsigned i = 0;
    for (; i < 12 && src[i] && src[i] != '@'; ++i)
        dst[i] = src[i];
    dst[i] = 0;
}

static int valid(uintptr_t fdt)
{
    return fdt && !(fdt & 7) && be32(fdt) == FDT_MAGIC;
}

unsigned fdt_memreserve(uintptr_t fdt, struct fdt_range *out, unsigned max)
{
    if (!valid(fdt))
        return 0;
    uintptr_t p = fdt + be32(fdt + 16);   /* off_mem_rsvmap */
    unsigned n = 0;
    for (unsigned guard = 0; guard < 256; ++guard, p += 16) {
        uint64_t base = be64(p), size = be64(p + 8);
        if (!base && !size)
            break;
        if (n < max) {
            out[n].base = base;
            out[n].size = size;
            out[n].name[0] = 0;
        }
        ++n;
    }
    return n;
}

/* mode 0: top-level memory@ nodes (all tuples); 1: /reserved-memory kids. */
static unsigned walk(uintptr_t fdt, int mode, struct fdt_range *out, unsigned max)
{
    if (!valid(fdt))
        return 0;
    uintptr_t p = fdt + be32(fdt + 8);
    uintptr_t strings = fdt + be32(fdt + 12);
    uint32_t acells[MAX_DEPTH], scells[MAX_DEPTH];
    const char *top = "", *cur = "";   /* depth-2 node, current node */
    int depth = 0;
    unsigned n = 0;
    acells[0] = 2;
    scells[0] = 1;

    for (unsigned guard = 0; guard < 400000; ++guard) {
        uint32_t tok = be32(p);
        p += 4;
        if (tok == FDT_BEGIN_NODE) {
            cur = (const char *)p;
            if (++depth >= MAX_DEPTH)
                return n;
            if (depth == 2)
                top = cur;
            /* A node's own #cells (index depth) apply to its children;
             * the defaults are the devicetree spec's 2 and 1. */
            acells[depth] = 2;
            scells[depth] = 1;
            p = align4(skip_str(p));
        } else if (tok == FDT_END_NODE) {
            if (--depth < 0)
                return n;
        } else if (tok == FDT_PROP) {
            uint32_t len = be32(p), nameoff = be32(p + 4);
            uintptr_t val = p + 8;
            const char *prop = (const char *)(strings + nameoff);
            if (streq(prop, "#address-cells") && len == 4)
                acells[depth] = be32(val);
            else if (streq(prop, "#size-cells") && len == 4)
                scells[depth] = be32(val);
            else if (streq(prop, "reg") && depth >= 2) {
                uint32_t ac = acells[depth - 1], sc = scells[depth - 1];
                uint32_t tuple = (ac + sc) * 4;
                int want = mode == 0
                    ? depth == 2 && starts(top, "memory")
                    : depth == 3 && streq(top, "reserved-memory");
                if (want && tuple && ac <= 2 && sc <= 2) {
                    unsigned count = mode == 0 ? len / tuple : (len >= tuple);
                    for (unsigned i = 0; i < count; ++i, ++n)
                        if (n < max) {
                            out[n].base = cells(val + i * tuple, ac);
                            out[n].size = cells(val + i * tuple + ac * 4, sc);
                            copy_name(out[n].name, cur);
                        }
                }
            }
            p = align4(val + len);
        } else if (tok == FDT_NOP) {
            continue;
        } else {
            return n;
        }
    }
    return n;
}

unsigned fdt_memory(uintptr_t fdt, struct fdt_range *out, unsigned max)
{
    return walk(fdt, 0, out, max);
}

unsigned fdt_reserved(uintptr_t fdt, struct fdt_range *out, unsigned max)
{
    return walk(fdt, 1, out, max);
}
