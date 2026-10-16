/**
 * @file context_panel.c
 * @brief Context area of the right-hand panel: status card (ui_Panel12) and
 *        spectrogram settings (ui_Panel13), one shown at a time.
 */

#include "context_panel.h"
#include "ui.h"
#include "main.h"              /* HAL_GetTick */
#include "waverecorder.h"
#include "audio_stream_fft.h"
#include "spectrum_tab.h"

/* Panel palette (matches the control panel in main.c) */
#define CTX_CARD_BG     0x1E1E28
#define CTX_TEXT        0xE5E5EA
#define CTX_TEXT_DIM    0x8A8A99
#define CTX_ACCENT      0x4FC3F7
#define CTX_REC         0xEF4444

static void (*s_colour_refresh)(void);

/* Status card labels: state and time are SquareLine's, the rest are added */
static lv_obj_t * s_lbl_state;
static lv_obj_t * s_lbl_time;
static lv_obj_t * s_lbl_dropped;
static lv_obj_t * s_lbl_info;

/* ------------------------------------------------------------------------ */

void context_show(lv_obj_t * panel)
{
    if (panel == NULL) {
        return;
    }
    lv_obj_t * parent = lv_obj_get_parent(panel);
    uint32_t n = lv_obj_get_child_count(parent);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t * child = lv_obj_get_child(parent, (int32_t)i);
        if (child == panel) {
            lv_obj_remove_flag(child, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(child, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

/* Common card look for both panels; also undoes SquareLine's 103% / x -28 */
static void style_card(lv_obj_t * card)
{
    lv_obj_set_size(card, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(card, 0, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(CTX_CARD_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(card, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_all(card, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_row(card, 4, LV_PART_MAIN);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
}

static lv_obj_t * add_label(lv_obj_t * parent, const lv_font_t * font, uint32_t color)
{
    lv_obj_t * l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(color), LV_PART_MAIN);
    return l;
}

/* --- Status card (ui_Panel12) ------------------------------------------- */

static void build_status(lv_obj_t * card)
{
    style_card(card);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card, 8, LV_PART_MAIN);

    s_lbl_state = ui_Label23;
    lv_obj_set_style_text_font(s_lbl_state, &lv_font_montserrat_18, LV_PART_MAIN);

    s_lbl_time = ui_Label22;
    lv_obj_set_style_text_font(s_lbl_time, &lv_font_montserrat_30, LV_PART_MAIN);
    lv_obj_set_style_text_color(s_lbl_time, lv_color_hex(CTX_TEXT), LV_PART_MAIN);

    s_lbl_dropped = add_label(card, &lv_font_montserrat_14, CTX_TEXT_DIM);

    /* FFT size and resolution: fixed at build time, so set once */
    s_lbl_info = add_label(card, &lv_font_montserrat_14, CTX_TEXT_DIM);
    uint32_t hz10 = (uint32_t)(BIN_RESOLUTION * 10.0f + 0.5f);
    lv_label_set_text_fmt(s_lbl_info, "%d-pt FFT\n%lu.%lu Hz/bin, %d ms",
                          FFT_LEN, (unsigned long)(hz10 / 10u), (unsigned long)(hz10 % 10u),
                          FFT_REFRESH_MS);
    lv_obj_set_style_text_align(s_lbl_info, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
}

void context_panel_update(void)
{
    static int8_t   s_state = -1;       /* 0 idle, 1 saving, 2 FFT */
    static uint32_t s_t0;
    static uint32_t s_sec = UINT32_MAX;
    static uint32_t s_dropped = UINT32_MAX;

    if (s_lbl_state == NULL) {
        return;
    }

    int8_t state = 0;
    if (AUDIO_REC_IsActive()) {
        state = (AUDIO_REC_GetMode() == AUDIO_REC_MODE_FFT) ? 2 : 1;
    }

    if (state != s_state) {
        if (state != 0 && s_state <= 0) {
            s_t0 = HAL_GetTick();  /* a capture started: restart the clock */
            s_sec = UINT32_MAX;
        }
        s_state = state;
        static const char * const text[] = {
            LV_SYMBOL_BULLET " IDLE", LV_SYMBOL_BULLET " RECORDING", LV_SYMBOL_BULLET " LIVE FFT" };
        static const uint32_t color[] = { CTX_TEXT_DIM, CTX_REC, CTX_ACCENT };
        lv_label_set_text(s_lbl_state, text[state]);
        lv_obj_set_style_text_color(s_lbl_state, lv_color_hex(color[state]), LV_PART_MAIN);
    }

    /* Elapsed time runs while capturing and holds its last value when idle */
    if (state != 0) {
        uint32_t sec = (HAL_GetTick() - s_t0) / 1000u;
        if (sec != s_sec) {
            s_sec = sec;
            lv_label_set_text_fmt(s_lbl_time, "%02lu:%02lu",
                                  (unsigned long)(sec / 60u), (unsigned long)(sec % 60u));
        }
        uint32_t dropped = AUDIO_REC_GetDroppedCount();
        if (dropped != s_dropped) {
            s_dropped = dropped;
            lv_label_set_text_fmt(s_lbl_dropped, "Dropped: %lu", (unsigned long)dropped);
        }
    } else if (s_sec == UINT32_MAX) {
        s_sec = 0;
        lv_label_set_text(s_lbl_time, "00:00");
        lv_label_set_text(s_lbl_dropped, "Dropped: 0");
    }
}

/* --- Spectrogram settings (ui_Panel13) ---------------------------------- */

/* One slider row: "Name   value unit" above a slider. Values are integers
 * in the slider; `div` turns them into the real value (e.g. gamma x10). */
typedef struct {
    const char * name;
    const char * unit;
    int32_t      min, max, div;
    float *      target;          /* the setting it drives */
    uint8_t      apply_on_release; /* heavy updates: only when let go */
    void       (*apply)(void);    /* run after the target changes */
    lv_obj_t *   value_label;
} ctx_slider_t;

static float s_black;  /* contrast mirrors: spectrogram_set_contrast() takes both */
static float s_gamma;

static void apply_adapt_times(void) { FFT_Adaptive_ParamsChanged(); }

static void apply_contrast(void)
{
    spectrogram_set_contrast(s_black, s_gamma);
    if (s_colour_refresh != NULL) {
        s_colour_refresh();
    }
}

static ctx_slider_t s_sliders[] = {
    { "Fast",     "ms",  20, 1000,  1, &g_fft_adapt.fast_ms,  0, apply_adapt_times, NULL },
    { "Slow",     "s",    5,  100, 10, &g_fft_adapt.slow_ms,  0, apply_adapt_times, NULL }, /* 0.5-10 s */
    { "Range",    "dB",   3,   40,  1, &g_fft_adapt.range_db, 0, NULL,              NULL },
    { "Gate",     "dB", -100, -40,  1, &g_fft_adapt.gate_db,  0, NULL,              NULL },
    { "Gamma",    "",     5,   40, 10, &s_gamma,              1, apply_contrast,    NULL },
    { "Black",    "%",    0,   40,  1, &s_black,              1, apply_contrast,    NULL },
};

/* Real value of a slider position, in the setting's own units */
static float slider_to_value(const ctx_slider_t * d, int32_t v)
{
    float x = (float)v / (float)d->div;
    if (d->target == &g_fft_adapt.slow_ms) x *= 1000.0f;   /* shown in s, stored in ms */
    if (d->target == &s_black)             x /= 100.0f;    /* shown in %, stored 0-1 */
    return x;
}

static int32_t value_to_slider(const ctx_slider_t * d, float x)
{
    if (d->target == &g_fft_adapt.slow_ms) x /= 1000.0f;
    if (d->target == &s_black)             x *= 100.0f;
    return (int32_t)(x * (float)d->div + (x >= 0 ? 0.5f : -0.5f));
}

static void show_value(const ctx_slider_t * d, int32_t v)
{
    /* LVGL's printf has no %f: print tenths by hand where div is 10 */
    if (d->div == 10) {
        int32_t a = v < 0 ? -v : v;
        lv_label_set_text_fmt(d->value_label, "%s %s%ld.%ld %s", d->name, v < 0 ? "-" : "",
                              (long)(a / 10), (long)(a % 10), d->unit);
    } else {
        lv_label_set_text_fmt(d->value_label, "%s %ld %s", d->name, (long)v, d->unit);
    }
}

static void slider_cb(lv_event_t * e)
{
    ctx_slider_t * d = (ctx_slider_t *)lv_event_get_user_data(e);
    lv_obj_t * slider = lv_event_get_target_obj(e);
    int32_t v = lv_slider_get_value(slider);
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_VALUE_CHANGED) {
        show_value(d, v);
        if (d->apply_on_release) {
            return;   /* label follows the drag; the rebuild waits for release */
        }
    }
    *d->target = slider_to_value(d, v);
    if (d->apply != NULL) {
        d->apply();
    }
}

static void adapt_switch_cb(lv_event_t * e)
{
    lv_obj_t * sw = lv_event_get_target_obj(e);
    g_fft_adapt.enable = lv_obj_has_state(sw, LV_STATE_CHECKED) ? 1 : 0;
    FFT_Adaptive_Reset();   /* re-seed from the next update, not stale history */
}

static void build_settings(lv_obj_t * card)
{
    lv_obj_clean(card);          /* drop the SquareLine placeholder slider */
    ui_Slider2 = NULL;
    style_card(card);

    /* Content can exceed the card: let it scroll vertically only */
    lv_obj_add_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(card, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_ACTIVE);

    /* Adaptive on/off */
    lv_obj_t * row = lv_obj_create(card);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t * l = add_label(row, &lv_font_montserrat_14, CTX_TEXT);
    lv_label_set_text(l, "Adaptive");
    lv_obj_t * sw = lv_switch_create(row);
    lv_obj_set_size(sw, 40, 20);
    lv_obj_set_style_bg_color(sw, lv_color_hex(CTX_ACCENT), LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (g_fft_adapt.enable) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(sw, adapt_switch_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* Sliders */
    spectrogram_get_contrast(&s_black, &s_gamma);
    for (uint32_t i = 0; i < sizeof(s_sliders) / sizeof(s_sliders[0]); i++) {
        ctx_slider_t * d = &s_sliders[i];

        d->value_label = add_label(card, &lv_font_montserrat_14, CTX_TEXT_DIM);

        lv_obj_t * s = lv_slider_create(card);
        lv_obj_set_size(s, lv_pct(100), 8);
        lv_obj_set_style_pad_hor(s, 6, LV_PART_MAIN);   /* room for the knob at the ends */
        lv_slider_set_range(s, d->min, d->max);
        int32_t v = value_to_slider(d, *d->target);
        lv_slider_set_value(s, v, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(s, lv_color_hex(CTX_ACCENT), LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(s, lv_color_hex(CTX_ACCENT), LV_PART_KNOB);
        lv_obj_set_style_margin_bottom(s, 6, LV_PART_MAIN);
        show_value(d, lv_slider_get_value(s));

        lv_obj_add_event_cb(s, slider_cb, LV_EVENT_VALUE_CHANGED, d);
        if (d->apply_on_release) {
            lv_obj_add_event_cb(s, slider_cb, LV_EVENT_RELEASED, d);
        }
    }
}

/* --- Switching ------------------------------------------------------------ */

static void tab_changed_cb(lv_event_t * e)
{
    lv_obj_t * tv = lv_event_get_target_obj(e);
    uint32_t active = lv_tabview_get_tab_active(tv);
    bool spectro = (active == (uint32_t)lv_obj_get_index(ui_Spectrumtab));
    context_show(spectro ? ui_Panel13 : ui_Panel12);
}

void context_panel_init(void (*colour_refresh)(void))
{
    s_colour_refresh = colour_refresh;

    build_status(ui_Panel12);
    build_settings(ui_Panel13);

    lv_obj_add_event_cb(ui_TabView4, tab_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
    uint32_t active = lv_tabview_get_tab_active(ui_TabView4);
    context_show(active == (uint32_t)lv_obj_get_index(ui_Spectrumtab) ? ui_Panel13 : ui_Panel12);

    context_panel_update();
}
