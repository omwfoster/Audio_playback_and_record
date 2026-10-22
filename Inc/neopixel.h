/**
 * @file neopixel.h
 * @brief WS2812B / NeoPixel driver: TIM3 channel 3 PWM fed by circular DMA
 *        through a 2-LED ping-pong buffer, refilled one LED at a time from
 *        the half/full-transfer callbacks. RAM use is 3 bytes per LED plus a
 *        fixed 96-byte DMA buffer, whatever the strip length.
 *
 * Hardware (set up by NeoPixel_Init()): Adafruit 5x8 NeoPixel shield on
 * the Arduino header, data on D5 = PC8 = TIM3_CH3 (the shield's default D6
 * is PF7 / TIM11, which has no DMA, so its data line is re-routed to D5).
 * DMA1 Stream7 channel 5, PWM at 800 kHz from the real timer clock.
 *
 * Usage:
 *   NeoPixel_Init();                            // once, at startup
 *   led_strip[i].r = ...; .g = ...; .b = ...;   // set colours
 *   NeoPixel_Show();                            // send the whole strip
 *   while (NeoPixel_IsBusy()) { }               // optional: wait for it
 */
#ifndef NEOPIXEL_H
#define NEOPIXEL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Number of LEDs on the strip. Override by defining it before this header. */
#ifndef NUM_LEDS
#define NUM_LEDS        40      /* Adafruit 5x8 NeoPixel shield */
#endif

/** One LED's colour, 8 bits per channel. Sent in the strip's GRB order. */
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} NeoPixel_t;

/** Colours to send; write these, then call NeoPixel_Show(). Don't change
 *  them while NeoPixel_IsBusy(): the DMA callbacks read them as they go. */
extern NeoPixel_t led_strip[NUM_LEDS];

/** @brief Set up the pin, TIM3 PWM and DMA. Call once before NeoPixel_Show(). */
void NeoPixel_Init(void);

/**
 * @brief Start sending led_strip to the LEDs. Returns at once; the transfer
 *        runs from the TIM3 DMA callbacks. Ignored if a transfer is still
 *        running.
 */
void NeoPixel_Show(void);

/**
 * @brief Blocking wiring/power check (~1.5 s): one LED chases through the
 *        indices 0..NUM_LEDS-1 (red at index 0 shading to blue at the
 *        end), then all LEDs flash dim white, then all off. Call after
 *        NeoPixel_Init(), with interrupts running.
 * @param brightness  0-255; keep low when powered from the board's header.
 */
void NeoPixel_SelfTest(uint8_t brightness);

/** Driver state for debugging: HAL results (0 = OK), callback counts and
 *  key register bits. Filled by NeoPixel_GetDiag(). */
typedef struct {
    uint8_t  st_pwm_init, st_config, st_dma_init, st_start;  /* HAL_StatusTypeDef */
    uint32_t shows, half_cb, full_cb;                          /* since boot */
    uint8_t  tim_cen, tim_cc3e, tim_cc3de, dma_en, busy;
    uint32_t tim_arr, dma_ndtr, dma_lisr_hisr;
    uint8_t  pc8_mode, pc8_af;                                 /* expect 2 and 2 */
    uint16_t period, t0h, t1h;
} NeoPixel_Diag_t;

void NeoPixel_GetDiag(NeoPixel_Diag_t *d);

/** @brief 1 while a NeoPixel_Show() transfer is still running, else 0. */
uint8_t NeoPixel_IsBusy(void);

#ifdef __cplusplus
}
#endif

#endif /* NEOPIXEL_H */
