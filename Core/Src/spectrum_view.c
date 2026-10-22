/**
 * @file spectrum_view.c
 * @brief Drives the SquareLine bar chart (ui_Chart2) with the FFT
 *        pipeline's Clean_Display_Bars.
 */

#include "spectrum_view.h"
#include <string.h>
#include "audio_stream_fft.h"
#include "spectrum_tab.h"   /* shared colour map (ui_heatmap dropdown) */
#include "ui.h"             /* ui_TabView4, ui_FFTtab */
#include "bar_mapping.h"
#include "neopixel.h"

/* --- NeoPixel shield mirror of the FFT bars ------------------------------
 * While the FFT tab is open, each update is reduced to NUM_BARS log-spaced
 * bars (bar_mapping.c) and drawn on the 5x8 shield: one column per bar,
 * lit from the bottom, each LED coloured from the heatmap by its height. */
#define LED_COLS        8           /* shield columns = NUM_BARS */
#define LED_ROWS        5
/* The shield is wired row by row from the top-left LED (index 0), as seen
 * from its LED side. The F769I-DISCO's Arduino header is on the back of the
 * board, so if the bars come out mirrored or upside down, flip these. */
#define LED_FLIP_X      0
#define LED_FLIP_Y      0
/* 0-255. 40 LEDs at full white draw ~2.4 A; 24 keeps the whole shield
 * under ~250 mA, which the board's 5 V header can supply. */
#define LED_BRIGHTNESS  24

static uint8_t s_leds_on;   /* something is lit (so leaving needs a clear) */

/* Chart works in integers: 0.0-1.0 bars are scaled to 0-SPECTRUM_Y_MAX */
#define SPECTRUM_Y_MAX  1000

static lv_obj_t *          s_chart  = NULL;
static lv_chart_series_t * s_series = NULL;

/* The series' values. SquareLine attaches its own sample-data array to the
 * series, sized however many sample points were entered in the designer
 * (an earlier version had 10 for a 128-point chart, so LVGL read past its
 * end). Replacing it means the chart never depends on that array's size. */
static int32_t s_bars[DISPLAY_BINS];

/* Colour each bar by its height through the selected colour map. The chart
 * draws every bar as a fill task with part LV_PART_ITEMS and the bar's index
 * in id2; recolour it as it's queued. */
static void bar_color_cb(lv_event_t * e)
{
    lv_draw_task_t * task = lv_event_get_draw_task(e);
    lv_draw_fill_dsc_t * fill = lv_draw_task_get_fill_dsc(task);
    if (fill == NULL || fill->base.part != LV_PART_ITEMS) {
        return;
    }
    uint32_t i = fill->base.id2;
    if (i >= DISPLAY_BINS) {
        return;
    }
    int32_t v = s_bars[i];
    if (v < 0) v = 0;
    if (v > SPECTRUM_Y_MAX) v = SPECTRUM_Y_MAX;
    fill->color = spectrogram_colormap((uint8_t)((v * 255) / SPECTRUM_Y_MAX));
}

static int fft_tab_open(void)
{
    if (ui_TabView4 == NULL || ui_FFTtab == NULL) {
        return 0;
    }
    return lv_tabview_get_tab_active(ui_TabView4) == (uint32_t)lv_obj_get_index(ui_FFTtab);
}

/* LED index for a shield column (0 = left) and row (0 = bottom) */
static uint32_t led_index(uint32_t col, uint32_t row)
{
    uint32_t x = LED_FLIP_X ? (LED_COLS - 1u - col) : col;
    uint32_t y_top = LED_FLIP_Y ? row : (LED_ROWS - 1u - row);   /* wiring counts from the top */
    return y_top * LED_COLS + x;
}

static void leds_off(void)
{
    if (!s_leds_on || NeoPixel_IsBusy()) {
        return;   /* nothing lit, or try again on the next update */
    }
    memset(led_strip, 0, sizeof(led_strip));
    NeoPixel_Show();
    s_leds_on = 0;
}

static void leds_update(const float * bars, uint32_t count)
{
    static float32_t col_level[NUM_BARS];

    if (!fft_tab_open()) {
        leds_off();
        return;
    }
    if (count < NUM_BARS + 1u || NeoPixel_IsBusy()) {
        return;   /* previous frame still going out: skip this one */
    }

    /* Bin 0 is DC (and any DFSDM offset): leave it out of the low bar */
    BarMapping_Process(&bars[1], col_level);

    for (uint32_t c = 0; c < LED_COLS; c++) {
        float v = col_level[c];          /* can exceed 1 by the EQ gain */
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        uint32_t lit = (uint32_t)(v * LED_ROWS + 0.5f);

        for (uint32_t r = 0; r < LED_ROWS; r++) {
            NeoPixel_t * px = &led_strip[led_index(c, r)];
            if (r < lit) {
                lv_color_t col = spectrogram_colormap((uint8_t)(((r + 1u) * 255u) / LED_ROWS));
                px->r = (uint8_t)((col.red   * LED_BRIGHTNESS) / 255u);
                px->g = (uint8_t)((col.green * LED_BRIGHTNESS) / 255u);
                px->b = (uint8_t)((col.blue  * LED_BRIGHTNESS) / 255u);
            } else {
                px->r = px->g = px->b = 0;
            }
        }
    }
    NeoPixel_Show();
    s_leds_on = 1;
}

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

    /* Per-bar colours from the heatmap (bar_color_cb) */
    lv_obj_add_flag(s_chart, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
    lv_obj_add_event_cb(s_chart, bar_color_cb, LV_EVENT_DRAW_TASK_ADDED, NULL);

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
            /* A label every 1 kHz up to 8 kHz, every 2 kHz above that
             * (0-16 kHz at 32 kHz sampling), with one minor tick between */
            uint32_t step = (khz > 8u) ? 2u : 1u;
            lv_scale_set_range(child, 0, (int32_t)khz);
            lv_scale_set_total_tick_count(child, (khz / step) * 2u + 1u);
            lv_scale_set_major_tick_every(child, 2);
        } else {
            /* dBFS span the 0.0-1.0 bars represent (audio_stream_fft.h) */
            lv_scale_set_range(child, (int32_t)DB_FLOOR, (int32_t)DB_CEIL);
            lv_scale_set_total_tick_count(child, 11);
            lv_scale_set_major_tick_every(child, 2);
        }
    }

    /* NeoPixel bars: bins 1..DISPLAY_BINS-1 (DC left out) into NUM_BARS */
    BarMapping_Init(DISPLAY_BINS - 1u);

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

    leds_update(bars, count);
}

void spectrum_view_refresh(void)
{
    if (s_chart != NULL) {
        lv_chart_refresh(s_chart);
    }
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
    leds_off();
}
