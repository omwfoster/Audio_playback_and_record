/**
 * @file spectrum_view.h
 * @brief Drives the SquareLine bar chart (ui_Chart2) with the FFT
 *        pipeline's Clean_Display_Bars (see audio_stream_fft.h).
 */
#ifndef SPECTRUM_VIEW_H
#define SPECTRUM_VIEW_H

#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Take over an existing bar chart (e.g. ui_Chart2 from SquareLine).
 *        Call once after ui_init(). Gives the chart's first series its own
 *        DISPLAY_BINS-long array, sets the value range, and labels the
 *        chart's scales (dB on the Y axes, kHz on the X axis).
 */
void spectrum_view_init(lv_obj_t * chart);

/**
 * @brief Show a new set of bars. Values are 0.0-1.0, one per bar; extra
 *        values beyond the chart's bar count are ignored. Call from the
 *        LVGL thread (the main loop), not from an interrupt.
 */
void spectrum_view_update(const float * bars, uint32_t count);

/** @brief Drop all bars to zero, e.g. when capture stops. */
void spectrum_view_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* SPECTRUM_VIEW_H */
