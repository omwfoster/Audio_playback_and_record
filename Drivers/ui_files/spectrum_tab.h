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

/* Width of each new column of history, in pixels. Doubling it halves the
 * time span shown (~31 s at 100 ms per column on a ~620 px tab). */
#define SPECTROGRAM_COL_PX     2

/* Frequency axis: logarithmic, so each octave gets the same height.
 * Bottom row = SPECTROGRAM_FREQ_MIN, top row = SPECTROGRAM_FREQ_MAX (Hz).
 * The minimum also keeps the DC bin (and any DFSDM offset) off screen;
 * keep it above one FFT bin (SAMPLE_RATE / FFT_LEN = 31.25 Hz at 512). */
#define SPECTROGRAM_FREQ_MIN   60.0f
#define SPECTROGRAM_FREQ_MAX   8000.0f

/* Colour-map contrast, applied when the colour table is built (so the
 * spectrogram, bar colours and colour bar all follow it):
 *   levels below SPECTROGRAM_CMAP_BLACK (0.0-1.0) map to the darkest colour;
 *   the rest are stretched over the map through a gamma curve: > 1 pushes
 *   low and mid levels darker so peaks stand out, 1.0 = linear. */
#define SPECTROGRAM_CMAP_BLACK   0.12f
#define SPECTROGRAM_CMAP_GAMMA   1.8f

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
 * @brief Add one column built from FFT bars: @p count values 0.0-1.0, bin i
 *        at i * Nyquist / count Hz (e.g. Clean_Display_Bars / DISPLAY_BINS).
 *        Rows are mapped to frequency on a log scale between
 *        SPECTROGRAM_FREQ_MIN and _MAX, interpolating between bins. Call
 *        from the main loop.
 */
void spectrogram_push_bars(const float * bars, uint32_t count);

/**
 * @brief Add one column of intensities (0-255), one per canvas row, row 0
 *        at the top, SPECTROGRAM_COL_PX pixels wide. Shifts the existing
 *        image left by SPECTROGRAM_COL_PX first.
 */
void update_spectrogram_hardware(const uint8_t * fft_data);

/* Colour maps, shared by the spectrogram and the FFT bar chart. The colour
 * map dropdown is matched by option name (spectrogram_colormap_from_name()),
 * so its options can be in any order. */
typedef enum {
    SPECTRO_CMAP_JET = 0,
    SPECTRO_CMAP_VIRIDIS,
    SPECTRO_CMAP_MAGMA,
    SPECTRO_CMAP_PLASMA,
    SPECTRO_CMAP_COUNT
} spectro_cmap_t;

/**
 * @brief Colour map for a name such as a dropdown option ("Plasma",
 *        "viridis", ...; case-insensitive). Unknown names give jet.
 */
spectro_cmap_t spectrogram_colormap_from_name(const char * name);

/**
 * @brief Select the colour map for the spectrogram and the bar chart.
 *        Out-of-range values select jet. The whole spectrogram history is
 *        recoloured from its stored levels (a full repaint, a few ms).
 */
void spectrogram_set_colormap(uint32_t cmap);

/** @brief Colour for an intensity 0-255 in the current colour map. */
lv_color_t spectrogram_colormap(uint8_t level);

/**
 * @brief Fill @p grad with a horizontal gradient of the current colour map
 *        (low level on the left), for a colour-bar object's bg_grad style.
 *        Needs LV_GRADIENT_MAX_STOPS >= 6. Keep @p grad alive (static): the
 *        style stores the pointer.
 */
void spectrogram_colormap_gradient(lv_grad_dsc_t * grad);

/** @brief Blank the spectrogram to black. */
void spectrogram_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* SPECTRUM_TAB_H */
