/**
 * @file prof.h
 * @brief Minimal cycle-counter profiling (DWT->CYCCNT) for finding where the
 *        main loop's time goes. Header-only so no build files need changing.
 *
 *   uint32_t t = prof_start();
 *   ... code ...
 *   prof_stop(&g_prof_touch, t);
 *
 * The prof_t instances are defined in main.c, which logs and resets them at
 * the end of each recording.
 */
#ifndef PROF_H
#define PROF_H

#include <stdint.h>
#include "stm32f7xx.h"

typedef struct {
    uint32_t max_cyc;    /* longest single call, in CPU cycles */
    uint64_t total_cyc;  /* sum of all calls since the last reset */
    uint32_t calls;
} prof_t;

extern prof_t g_prof_touch;    /* touchpad_read_cb(): BSP_TS_GetState() over I2C */
extern prof_t g_prof_console;  /* console_rebuild_text(): textarea set_text + scroll */
extern prof_t g_prof_lv;       /* lv_task_handler(), the whole thing */

/* Enable the cycle counter. Cortex-M7 needs the DWT unlocked first. */
static inline void prof_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->LAR = 0xC5ACCE55;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static inline uint32_t prof_start(void)
{
    return DWT->CYCCNT;
}

static inline void prof_stop(prof_t * p, uint32_t t0)
{
    uint32_t d = DWT->CYCCNT - t0;
    if (d > p->max_cyc) p->max_cyc = d;
    p->total_cyc += d;
    p->calls++;
}

static inline uint32_t prof_cyc_to_us(uint64_t cyc)
{
    return (uint32_t)(cyc / (SystemCoreClock / 1000000u));
}

#endif /* PROF_H */
