/* SPDX-License-Identifier: GPL-2.0-only
 * Text on the Galaxy A50 S-Boot framebuffer, adapted from shim/herolte/fbtext.c.
 *
 * Runs with the MMU off: every access is Device memory, so all loads and
 * stores are naturally aligned 32-bit accesses and the code stays position
 * independent (no pointer tables).
 */
#include "fbtext.h"
#include "a50.h"

#define SCALE 4U
#define CELL_W (6U * SCALE)
#define CELL_H (10U * SCALE)
#define LEFT 24U
#define TOP 160U

struct a50_fb a50_fb;

static uint32_t rd32(uintptr_t a)
{
    return *(volatile uint32_t *)a;
}

static void wr32(uintptr_t a, uint32_t v)
{
    *(volatile uint32_t *)a = v;
}

static uintptr_t idma_channel(unsigned i)
{
    /* No pointer table: keeps the raw Image free of absolute relocations. */
    switch (i) {
    case 0: return A50_IDMA_GF;
    case 1: return A50_IDMA_VG0;
    case 2: return A50_IDMA_G0;
    default: return A50_IDMA_G1;
    }
}

static int in_dram(uint64_t base, uint64_t bytes)
{
    return base >= A50_DRAM_LOW_BASE && base + bytes <= A50_DRAM_LOW_END;
}

void a50_fb_init(void)
{
    a50_fb.base = A50_FB_DEFAULT_BASE;
    a50_fb.width = A50_FB_DEFAULT_WIDTH;
    a50_fb.height = A50_FB_DEFAULT_HEIGHT;
    a50_fb.pitch = A50_FB_DEFAULT_WIDTH;
    a50_fb.idma = -1;

    /* Read-only scan of the DPU DMA channels S-Boot may have used. */
    for (unsigned i = 0; i < 4; ++i) {
        uintptr_t ch = idma_channel(i);
        a50_fb.idma_en[i] = rd32(ch + A50_IDMA_ENABLE);
        a50_fb.idma_src[i] = rd32(ch + A50_IDMA_SRC_SIZE);
        a50_fb.idma_img[i] = rd32(ch + A50_IDMA_IMG_SIZE);
        a50_fb.idma_base[i] = rd32(ch + A50_IDMA_IN_BASE_ADDR_Y);
    }
    for (unsigned i = 0; i < 4 && a50_fb.idma < 0; ++i) {
        uint32_t pitch = a50_fb.idma_src[i] & 0xffffU;
        uint32_t src_h = (a50_fb.idma_src[i] >> 16) & 0x3fffU;
        uint32_t w = a50_fb.idma_img[i] & 0x1fffU;
        uint32_t h = (a50_fb.idma_img[i] >> 16) & 0x1fffU;
        uint64_t base = a50_fb.idma_base[i];
        /* Plausible full-screen ARGB buffer in low DRAM only. */
        if (w < 720 || w > 1440 || h < 1280 || h > 3200 || pitch < w || src_h < h)
            continue;
        if (!in_dram(base, (uint64_t)pitch * src_h * 4U) || (base & 3))
            continue;
        a50_fb.base = (uintptr_t)base;
        a50_fb.width = w;
        a50_fb.height = h;
        a50_fb.pitch = pitch;
        a50_fb.idma = (int32_t)i;
    }

    uintptr_t trig = A50_DECON_F_BASE + A50_DECON_HW_SW_TRIG;
    a50_fb.trig_before = rd32(trig);
    wr32(trig, A50_DECON_TRIG_VALUE);
    __asm__ volatile("dsb sy" : : : "memory");
}

unsigned a50_fb_rows(void)
{
    return (a50_fb.height - TOP) / CELL_H;
}

/* Compact 5x7 font, the same glyphs as the herolte/a5xelte consoles. */
struct glyph { char ch; uint8_t rows[7]; };
static const struct glyph font[] = {
    {' ',{0,0,0,0,0,0,0}}, {'-',{0,0,0,31,0,0,0}},
    {':',{0,4,4,0,4,4,0}}, {'?',{14,17,1,2,4,0,4}},
    {'.',{0,0,0,0,0,12,12}}, {'=',{0,0,31,0,31,0,0}},
    {'/',{1,1,2,4,8,16,16}}, {'X',{17,17,10,4,10,17,17}},
    {'0',{14,17,19,21,25,17,14}}, {'1',{4,12,4,4,4,4,14}},
    {'2',{14,17,1,2,4,8,31}}, {'3',{30,1,1,14,1,1,30}},
    {'4',{2,6,10,18,31,2,2}}, {'5',{31,16,30,1,1,17,14}},
    {'6',{6,8,16,30,17,17,14}}, {'7',{31,1,2,4,8,8,8}},
    {'8',{14,17,17,14,17,17,14}}, {'9',{14,17,17,15,1,2,12}},
    {'A',{14,17,17,31,17,17,17}}, {'B',{30,17,17,30,17,17,30}},
    {'C',{14,17,16,16,16,17,14}}, {'D',{30,17,17,17,17,17,30}},
    {'E',{31,16,16,30,16,16,31}}, {'F',{31,16,16,30,16,16,16}},
    {'G',{14,17,16,23,17,17,14}}, {'H',{17,17,17,31,17,17,17}},
    {'I',{14,4,4,4,4,4,14}}, {'J',{7,2,2,2,18,18,12}},
    {'K',{17,18,20,24,20,18,17}}, {'L',{16,16,16,16,16,16,31}},
    {'M',{17,27,21,21,17,17,17}}, {'N',{17,25,21,19,17,17,17}},
    {'O',{14,17,17,17,17,17,14}}, {'P',{30,17,17,30,16,16,16}},
    {'Q',{14,17,17,17,21,18,13}}, {'R',{30,17,17,30,20,18,17}},
    {'S',{15,16,16,14,1,1,30}}, {'T',{31,4,4,4,4,4,4}},
    {'U',{17,17,17,17,17,17,14}}, {'V',{17,17,17,17,17,10,4}},
    {'W',{17,17,17,21,21,21,10}}, {'Y',{17,17,10,4,4,4,4}},
    {'Z',{31,1,2,4,8,16,31}}, {'_',{0,0,0,0,0,0,31}},
};

static const uint8_t *glyph_for(char ch)
{
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - ('a' - 'A'));
    for (unsigned i = 0; i < sizeof(font) / sizeof(font[0]); ++i)
        if (font[i].ch == ch) return font[i].rows;
    return font[3].rows;
}

static void fill_row(unsigned row, uint32_t pixel)
{
    volatile uint32_t *fb = (volatile uint32_t *)a50_fb.base;
    unsigned top = TOP + row * CELL_H;
    for (unsigned y = top; y < top + CELL_H && y < a50_fb.height; ++y)
        for (unsigned x = 0; x < a50_fb.width; ++x)
            fb[y * a50_fb.pitch + x] = pixel;
}

void a50_fb_text(unsigned row, const char *s)
{
    volatile uint32_t *fb = (volatile uint32_t *)a50_fb.base;
    unsigned top = TOP + row * CELL_H + 2U * SCALE;
    if (top + 7U * SCALE >= a50_fb.height) return;
    fill_row(row, 0xff000000U);
    for (unsigned col = 0; s[col] && LEFT + (col + 1U) * CELL_W <= a50_fb.width; ++col) {
        const uint8_t *g = glyph_for(s[col]);
        for (unsigned y = 0; y < 7; ++y)
            for (unsigned x = 0; x < 5; ++x) {
                if (!(g[y] & (1U << (4 - x)))) continue;
                for (unsigned sy = 0; sy < SCALE; ++sy)
                    for (unsigned sx = 0; sx < SCALE; ++sx)
                        fb[(top + y * SCALE + sy) * a50_fb.pitch +
                           LEFT + col * CELL_W + x * SCALE + sx] = 0xffffffffU;
            }
    }
}

static unsigned put_hex(char *line, unsigned n, uint64_t value, int digits)
{
    for (int shift = (digits - 1) * 4; shift >= 0; shift -= 4) {
        unsigned d = (unsigned)(value >> shift) & 15U;
        line[n++] = (char)(d < 10 ? '0' + d : 'A' + d - 10);
    }
    return n;
}

static unsigned put_label(char *line, const char *label)
{
    unsigned n = 0;
    while (label[n] && n < 12) {
        line[n] = label[n];
        ++n;
    }
    line[n++] = ' ';
    return n;
}

/* "LABEL 0123456789ABCDEF" without library helpers. */
void a50_fb_hex(unsigned row, const char *label, uint64_t value)
{
    char line[48];
    unsigned n = put_label(line, label);
    n = put_hex(line, n, value, 16);
    line[n] = 0;
    a50_fb_text(row, line);
}

/* "LABEL AAAAAAAAAA BBBBBBBB SUFFIX": two compact values on one row. */
void a50_fb_hex2(unsigned row, const char *label, uint64_t a, uint64_t b,
                 const char *suffix)
{
    char line[64];
    unsigned n = put_label(line, label);
    n = put_hex(line, n, a, 10);
    line[n++] = ' ';
    n = put_hex(line, n, b, 8);
    if (suffix && suffix[0]) {
        line[n++] = ' ';
        for (unsigned i = 0; suffix[i] && i < 12; ++i)
            line[n++] = suffix[i];
    }
    line[n] = 0;
    a50_fb_text(row, line);
}
