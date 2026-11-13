/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once
#include <stdint.h>

/* PMU INFORM2/INFORM3 + SWRESET: the next boot goes to RECOVERY (TWRP). */
void a50_reboot_recovery(void) __attribute__((noreturn));
uint64_t a50_counter(void);
uint64_t a50_counter_hz(void);   /* CNTFRQ_EL0, or 26 MHz if unset */
