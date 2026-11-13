/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <stdint.h>

struct fdt_range {
    uint64_t base, size;
    char name[13];   /* node name, truncated; empty for memreserve */
};

/* Fill up to max entries; return the number found (may exceed max). */
unsigned fdt_memreserve(uintptr_t fdt, struct fdt_range *out, unsigned max);
/* reg tuples of every top-level memory@ node. */
unsigned fdt_memory(uintptr_t fdt, struct fdt_range *out, unsigned max);
/* First reg tuple of each /reserved-memory child that has one. */
unsigned fdt_reserved(uintptr_t fdt, struct fdt_range *out, unsigned max);
