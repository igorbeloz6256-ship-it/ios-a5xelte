/* SPDX-License-Identifier: GPL-2.0-only
 * Request recovery (TWRP) the way the A50 kernel's sec_reboot.c does; the
 * same sequence probe.S proved on the phone (2026-11-13).
 */
#include <stdint.h>
#include "a50.h"
#include "reboot.h"

void a50_reboot_recovery(void)
{
    volatile uint32_t *pmu = (volatile uint32_t *)A50_PMU_BASE;
    uint32_t inform2 = pmu[A50_PMU_INFORM2 / 4];
    inform2 &= ~(0xffU << A50_REBOOT_MAGIC_SHIFT);
    inform2 |= A50_REBOOT_MAGIC_NORMAL << A50_REBOOT_MAGIC_SHIFT;
    pmu[A50_PMU_INFORM2 / 4] = inform2;
    pmu[A50_PMU_INFORM3 / 4] = A50_RESET_REASON_RECOVERY;
    __asm__ volatile("dsb sy" : : : "memory");
    pmu[A50_PMU_SWRESET / 4] = 1;
    __asm__ volatile("dsb sy" : : : "memory");
    for (;;)
        __asm__ volatile("wfe");
}

uint64_t a50_counter(void)
{
    uint64_t value;
    __asm__ volatile("isb; mrs %0, cntpct_el0" : "=r" (value));
    return value;
}

uint64_t a50_counter_hz(void)
{
    uint64_t freq;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r" (freq));
    return freq ? freq : A50_TIMER_HZ;
}
