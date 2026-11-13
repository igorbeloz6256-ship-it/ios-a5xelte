/* SPDX-License-Identifier: GPL-2.0-only
 * Galaxy A50 (SM-A505FN, Exynos 9610) board constants for the stage-0 shim.
 *
 * Every value here comes from public sources and is NOT yet confirmed on the
 * phone. Sources:
 *   - Samsung A50 kernel (FreshROMs/android_kernel_samsung_exynos9610_mint,
 *     android-14.0): arch/arm64/boot/dts/exynos/exynos9610.dts,
 *     drivers/samsung/sec_reboot.c, include/linux/sec_debug.h,
 *     include/linux/soc/samsung/exynos-regs-pmu.h,
 *     drivers/video/fbdev/exynos/dpu20/cal_9610/regs-{decon,dpp}.h
 *   - uniLoader (ivoszbg/uniLoader): soc/exynos/exynos9610.c (DECON trigger
 *     0x1281), board-m31.c / board-gta4xl.c (S-Boot framebuffer 0xca000000 on
 *     Exynos 9610/9611 phones).
 */
#pragma once

/* PMU (system controller), exynos9610.dts pmu_system_controller. */
#define A50_PMU_BASE              0x11860000UL
#define A50_PMU_SWRESET           0x0400U
#define A50_PMU_INFORM2           0x0808U   /* SEC_DEBUG_MAGIC_INFORM */
#define A50_PMU_INFORM3           0x080CU   /* SEC_DEBUG_PANIC_INFORM */
/* sec_reboot.c: magic byte at INFORM2[23:16], reason in INFORM3. */
#define A50_REBOOT_MAGIC_SHIFT    16
#define A50_REBOOT_MAGIC_NORMAL   0x4EU     /* SEC_REBOOT_NORMAL (not LPM) */
#define A50_RESET_REASON_RECOVERY 0x12345674U

/* DECON_F and the four DPU DMA channels (exynos9610.dts decon_f, dpp_0..3). */
#define A50_DECON_F_BASE          0x148B0000UL
#define A50_DECON_HW_SW_TRIG      0x0070U
/* uniLoader soc/exynos/exynos9610.c: HW trigger on, DECON mask off, so the
 * command-mode panel keeps refreshing from the scanout buffer. */
#define A50_DECON_TRIG_VALUE      0x1281U

#define A50_IDMA_GF               0x14884000UL   /* dpp_0 DPU_DMA */
#define A50_IDMA_VG0              0x14883000UL   /* dpp_1 */
#define A50_IDMA_G0               0x14881000UL   /* dpp_2 */
#define A50_IDMA_G1               0x14882000UL   /* dpp_3 */
#define A50_IDMA_ENABLE           0x0000U
#define A50_IDMA_IN_CON           0x0008U
#define A50_IDMA_SRC_SIZE         0x0010U   /* [29:16] height, [15:0] width */
#define A50_IDMA_IMG_SIZE         0x0018U   /* [28:16] height, [12:0] width */
#define A50_IDMA_IN_BASE_ADDR_Y   0x0040U

/* Fallback scanout buffer: EA8076 panel 1080x2340, ARGB8888. */
#define A50_FB_DEFAULT_BASE       0xca000000UL
#define A50_FB_DEFAULT_WIDTH      1080U
#define A50_FB_DEFAULT_HEIGHT     2340U

/* DRAM: 0x80000000 (2 GiB) + 0x880000000 (2 GiB, 4 GiB model). */
#define A50_DRAM_LOW_BASE         0x80000000UL
#define A50_DRAM_LOW_END          0x100000000UL
#define A50_DRAM_HIGH_BASE        0x880000000UL
/* Probe 1 on the phone (2026-11-13): memory@880000000 is 0x80000000 long. */
#define A50_DRAM_HIGH_END         0x900000000UL

#define A50_TIMER_HZ              26000000U /* exynos9610.dts timer */

/* Seen by probe 1 on the phone (A50_PROGRESS.md, 2026-11-13). S-Boot loaded
 * the Image at 0x80240000, the FDT at 0x8a000000 and an empty initrd at
 * 0x89000000. Samsung uH (EL2) keeps 0x87000000+2M and 0x8fe600000+26M. */
#define A50_EL2_CODE_BASE         0x87000000UL
#define A50_EL2_EARLYMEM_BASE     0x8fe600000UL

/* xnu_boot: status page and the RAM XNU may own (free in the phone's DTB;
 * the loader re-checks them against /memory and /reserved-memory). The
 * appended payload must end below EL2_CODE. */
#define A50_LOG_BASE              0x8ffff000UL   /* XNU marks +0x100 */
#define A50_XNU_LOW_BASE          0x90000000UL
#define A50_XNU_LOW_END           0xb0000000UL   /* VIPX firmware at 0xb8000000 */
#define A50_XNU_BANK2_BASE        A50_DRAM_HIGH_BASE
#define A50_XNU_BANK2_END         A50_EL2_EARLYMEM_BASE
