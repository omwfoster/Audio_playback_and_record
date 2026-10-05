/**
 * @file spectrum_view.c
 * @brief Drives the SquareLine bar chart (ui_Chart2) with the FFT
 *        pipeline's Clean_Display_Bars.
 */

#include "spectrum_view.h"
#include "audio_stream_fft.h"

/* Chart works in integers: 0.0-1.0 bars are scaled to 0-SPECTRUM_Y_MAX */
#define SPECTRUM_Y_MAX  1000

/* dB span the 0.0-1.0 bars represent (FFT_Process_Audio_Block() maps
 * -100..0 dB onto 0..1); used only for the Y-axis labels */
#define SPECTRUM_DB_MIN  (-100)

static lv_obj_t *          s_chart  = NULL;
static lv_chart_series_t * s_series = NULL;

/* The series' values. SquareLine attaches its own sample-data array to the
 * series, sized however many sample points were entered in the designer
 * (an earlier version had 10 for a 128-point chart, so LVGL read past its
 * end). Replacing it means the chart never depends on that array's size. */
static int32_t s_bars[DISPLAY_BINS];

void spectrum_view_init(lv_obj_t * chart)
{
    if (chart == NULL || s_chart != NULL) {
        return;
    }
    s_chart = chart;

    s_series = lv_chart_get_series_next(s_chart, NULL);
    if (s_series == NULL) {
        s_series = lv_chart_add_series(s_chart, lv_color_hex(0x00C8FF), LV_CHART_AXIS_PRIMARY_Y);
    }

    lv_chart_set_type(s_chart, LV_CHART_TYPE_BAR);
    lv_chart_set_point_count(s_chart, DISPLAY_BINS);
    lv_chart_set_series_ext_y_array(s_chart, s_series, s_bars);
    lv_chart_set_axis_range(s_chart, LV_CHART_AXIS_PRIMARY_Y, 0, SPECTRUM_Y_MAX);

    /* 128 bars in ~700 px leaves ~5 px each: keep the gap at 1 px and the
     * bars square, or the theme's default spacing hides them */
    lv_obj_set_style_pad_column(s_chart, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(s_chart, 0, LV_PART_ITEMS);

    /* Label the scales SquareLine added as chart children: Y in dB,
     * X in kHz from 0 to Nyquist. Scales are found by type, so this keeps
     * working if they're renamed in SquareLine. */
    uint32_t khz = (uint32_t)(FREQ_MAX / 1000.0f);
    for (uint32_t i = 0; i < lv_obj_get_child_count(s_chart); i++) {
        lv_obj_t * child = lv_obj_get_child(s_chart, (int32_t)i);
        if (!lv_obj_check_type(child, &lv_scale_class)) {
            continue;
        }
        lv_scale_mode_t mode = lv_scale_get_mode(child);
        if (mode == LV_SCALE_MODE_HORIZONTAL_BOTTOM || mode == LV_SCALE_MODE_HORIZONTAL_TOP) {
            lv_scale_set_range(child, 0, (int32_t)khz);       /* 0..8 kHz */
            lv_scale_set_total_tick_count(child, khz * 2 + 1);
            lv_scale_set_major_tick_every(child, 2);
        } else {
            lv_scale_set_range(child, SPECTRUM_DB_MIN, 0);     /* -100..0 dB */
            lv_scale_set_total_tick_count(child, 11);
            lv_scale_set_major_tick_every(child, 2);
        }
    }

    spectrum_view_clear();
}

void spectrum_view_update(const float * bars, uint32_t count)
{
    if (s_chart == NULL || bars == NULL) {
        return;
    }
    if (count > DISPLAY_BINS) {
        count = DISPLAY_BINS;
    }

    for (uint32_t i = 0; i < count; i++) {
        float v = bars[i];
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        s_bars[i] = (int32_t)(v * SPECTRUM_Y_MAX);
    }
    lv_chart_refresh(s_chart);
}

void spectrum_view_clear(void)
{
    if (s_chart == NULL) {
        return;
    }
    for (uint32_t i = 0; i < DISPLAY_BINS; i++) {
        s_bars[i] = 0;
    }
    lv_chart_refresh(s_chart);
}
