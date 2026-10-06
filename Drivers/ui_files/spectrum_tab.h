/**
 * @file spectrum_tab.h
 * @brief Scrolling spectrogram (waterfall) on the Spectrogram tab: an LVGL
 *        canvas, one new column per FFT update, shifted left with DMA2D.
 */
#ifndef SPECTRUM_TAB_H
#define SPECTRUM_TAB_H

#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The canvas is sized at runtime to fill the tab (init_spectrogram()), one
 * column of history per pixel of width (~62 s at 100 ms on a ~620 px tab)
 * and FFT bins stretched over the height, low frequency at the bottom.
 * These cap the size, and so the SDRAM buffer: 800 x 480 x 2 = 750 KB. */
#define CANVAS_MAX_WIDTH    800
#define CANVAS_MAX_HEIGHT   480

/* Used if the tab's size can't be read (layout not ready) */
#define CANVAS_DEFAULT_WIDTH   400
#define CANVAS_DEFAULT_HEIGHT  240

/**
 * @brief Create the canvas inside @p parent (e.g. ui_canvasplaceholder),
 *        after making @p parent fill its own parent (the tab) and sizing the
 *        canvas to match. Call once after ui_init(); needs the SDRAM
 *        initialised (tft_init()).
 */
void init_spectrogram(lv_obj_t * parent);

/**
 * @brief Add one column built from FFT bars (0.0-1.0, low frequency first,
 *        e.g. Clean_Display_Bars / DISPLAY_BINS). Call from the main loop.
 */
void spectrogram_push_bars(const float * bars, uint32_t count);

/**
 * @brief Add one column of intensities (0-255), one per canvas row, row 0
 *        at the top. Shifts the existing image left by one pixel first.
 */
void update_spectrogram_hardware(const uint8_t * fft_data);

/** @brief Blank the spectrogram to black. */
void spectrogram_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* SPECTRUM_TAB_H */
