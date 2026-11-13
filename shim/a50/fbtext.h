/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <stdint.h>

/* What the stage-0 code learned about the S-Boot scanout buffer. */
struct a50_fb {
    uintptr_t base;        /* physical address the panel scans out */
    uint32_t width;        /* visible pixels */
    uint32_t height;
    uint32_t pitch;        /* pixels per buffer row (IDMA source width) */
    int32_t idma;          /* DMA channel it came from, -1 = default */
    uint32_t trig_before;  /* DECON_F HW_SW_TRIG_CONTROL before our write */
    uint32_t idma_base[4]; /* raw IN_BASE_ADDR_Y of GF, VG0, G0, G1 */
    uint32_t idma_src[4];  /* raw SRC_SIZE */
    uint32_t idma_img[4];  /* raw IMG_SIZE */
    uint32_t idma_en[4];   /* raw ENABLE */
};

extern struct a50_fb a50_fb;

/* Find the scanout buffer from the DPU DMA registers, then turn the DECON
 * hardware trigger on so CPU writes reach the panel. */
void a50_fb_init(void);
void a50_fb_text(unsigned row, const char *s);
void a50_fb_hex(unsigned row, const char *label, uint64_t value);
void a50_fb_hex2(unsigned row, const char *label, uint64_t a, uint64_t b,
                 const char *suffix);
unsigned a50_fb_rows(void);
