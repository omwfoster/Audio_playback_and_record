/**
 * @file ui_tabs.c
 * @brief Implementation of ui_tabs_create() / ui_console_log().
 *
 * Dashboard tab: a row of flat-2.0 style icon buttons (flat fill + 1-2px
 * darker border for depth, per the palette/design system worked out earlier).
 * Console tab: a scrollable, auto-scrolling text log backed by a fixed-size
 * ring buffer (no heap allocation on the log path -- safe for embedded use).
 *
 * Icon assets: by default this uses built-in LV_SYMBOL glyphs so the file
 * compiles standalone with no external image data. Once you've run your
 * flat-icon SVG set through the LVGL image converter, set
 * UI_USE_ICON_IMAGES to 1 and provide the three lv_img_dsc_t descriptors
 * declared below (from your generated C arrays).
 */
#include "ui_tabs.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* Set to 1 once you have converted flat-icon image assets available. */
#define UI_USE_ICON_IMAGES 0

#if UI_USE_ICON_IMAGES
/* Provide these from your LVGL image-converter output (indexed color, e.g. I4). */
extern const lv_img_dsc_t img_icon_spectrum;
extern const lv_img_dsc_t img_icon_spectrogram;
extern const lv_img_dsc_t img_icon_waveform;
#endif

/* ---- Module state ---------------------------------------------------- */

typedef struct {
    char lines[UI_CONSOLE_MAX_LINES][UI_CONSOLE_LINE_LEN + 1];
    uint16_t head;   /* index where the next line will be written */
    uint16_t count;  /* number of valid lines currently stored (<= MAX_LINES) */
} ui_console_ring_t;

static ui_console_ring_t s_ring;
static lv_obj_t * s_console_label = NULL;
static lv_obj_t * s_console_panel = NULL;

static ui_dashboard_icon_cb_t s_icon_cb = NULL;
static void * s_icon_cb_user_data = NULL;

/* ---- Styles (flat-2.0: flat fill + subtle border for depth) ---------- */

static lv_style_t s_style_card;
static lv_style_t s_style_card_pressed;
static lv_style_t s_style_console_panel;
static bool s_styles_initialised = false;

static void ui_tabs_init_styles(void)
{
    if (s_styles_initialised) {
        return;
    }

    /* Base flat card: solid fill + a darker 2px border for contrast.
     * Swap these hex values for your locked palette swatches. */
    lv_style_init(&s_style_card);
    lv_style_set_radius(&s_style_card, 10);
    lv_style_set_bg_opa(&s_style_card, LV_OPA_COVER);
    lv_style_set_bg_color(&s_style_card, lv_color_hex(0x2A5DA8));      /* Accent-Blue */
    lv_style_set_border_width(&s_style_card, 2);
    lv_style_set_border_color(&s_style_card, lv_color_hex(0x1E4480));  /* Accent-Blue-Shadow */
    lv_style_set_border_opa(&s_style_card, LV_OPA_COVER);
    lv_style_set_shadow_width(&s_style_card, 0); /* no blur -- stays flat */
    lv_style_set_pad_all(&s_style_card, 8);
    lv_style_set_text_color(&s_style_card, lv_color_hex(0xE6E6E6));    /* Foreground */

    lv_style_init(&s_style_card_pressed);
    lv_style_set_bg_color(&s_style_card_pressed, lv_color_hex(0x1E4480));
    lv_style_set_translate_y(&s_style_card_pressed, 1); /* subtle "pressed in" feel */

    lv_style_init(&s_style_console_panel);
    lv_style_set_radius(&s_style_console_panel, 6);
    lv_style_set_bg_opa(&s_style_console_panel, LV_OPA_COVER);
    lv_style_set_bg_color(&s_style_console_panel, lv_color_hex(0x1E2226)); /* Background */
    lv_style_set_border_width(&s_style_console_panel, 1);
    lv_style_set_border_color(&s_style_console_panel, lv_color_hex(0x30363C));
    lv_style_set_pad_all(&s_style_console_panel, 6);

    s_styles_initialised = true;
}

/* ---- Dashboard tab ----------------------------------------------------- */

static void icon_btn_event_cb(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    uint32_t index = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    if (s_icon_cb != NULL) {
        s_icon_cb((uint8_t)index, s_icon_cb_user_data);
    }
}

static lv_obj_t * create_icon_button(lv_obj_t * parent, const char * label_text,
                                      uint32_t index)
{
    lv_obj_t * btn = lv_obj_create(parent);
    lv_obj_remove_style_all(btn);
    lv_obj_add_style(btn, &s_style_card, 0);
    lv_obj_add_style(btn, &s_style_card_pressed, LV_STATE_PRESSED);
    lv_obj_set_size(btn, 96, 96);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn, icon_btn_event_cb, LV_EVENT_CLICKED,
                         (void *)(uintptr_t)index);

    lv_obj_t * icon = lv_image_create(btn);
#if UI_USE_ICON_IMAGES
    const lv_img_dsc_t * imgs[UI_DASHBOARD_ICON_COUNT] = {
        &img_icon_spectrum, &img_icon_spectrogram, &img_icon_waveform
    };
    lv_image_set_src(icon, imgs[index]);
#else
    /* Placeholder glyphs until converted flat-icon assets are wired in. */
    static const char * fallback_symbols[UI_DASHBOARD_ICON_COUNT] = {
        LV_SYMBOL_CHARGE, LV_SYMBOL_IMAGE, LV_SYMBOL_AUDIO
    };
    lv_obj_del(icon); /* not an image in fallback mode */
    lv_obj_t * sym = lv_label_create(btn);
    lv_label_set_text(sym, fallback_symbols[index]);
    lv_obj_set_style_text_font(sym, &lv_font_montserrat_28, 0);
    lv_obj_align(sym, LV_ALIGN_CENTER, 0, -10);
#endif

    lv_obj_t * caption = lv_label_create(btn);
    lv_label_set_text(caption, label_text);
    lv_obj_align(caption, LV_ALIGN_BOTTOM_MID, 0, 0);

    return btn;
}

static lv_obj_t * ui_dashboard_tab_create(lv_obj_t * parent,
                                           ui_dashboard_icon_cb_t icon_cb,
                                           void * user_data)
{
    s_icon_cb = icon_cb;
    s_icon_cb_user_data = user_data;

    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_SPACE_EVENLY,
                           LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(parent, 12, 0);
    lv_obj_set_style_pad_row(parent, 12, 0);
    lv_obj_set_style_pad_column(parent, 12, 0);

    static const char * labels[UI_DASHBOARD_ICON_COUNT] = {
        "Spectrum", "Spectrogram", "Waveform"
    };

    for (uint32_t i = 0; i < UI_DASHBOARD_ICON_COUNT; i++) {
        create_icon_button(parent, labels[i], i);
    }

    return parent;
}

/* ---- Console tab -------------------------------------------------------- */

static void console_rebuild_text(void)
{
    if (s_console_label == NULL) {
        return;
    }

    /* Concatenate the ring buffer into one label. For MAX_LINES=100 and
     * LINE_LEN=96 this static buffer is ~9.6KB -- fine on the F769I-DISCO's
     * SRAM; shrink UI_CONSOLE_MAX_LINES/LINE_LEN if your target is tighter. */
    static char text_buf[(UI_CONSOLE_LINE_LEN + 1) * UI_CONSOLE_MAX_LINES];
    text_buf[0] = '\0';
    size_t used = 0;

    uint16_t start = (s_ring.count < UI_CONSOLE_MAX_LINES)
                          ? 0
                          : s_ring.head; /* oldest line index in a full buffer */

    for (uint16_t i = 0; i < s_ring.count; i++) {
        uint16_t idx = (uint16_t)((start + i) % UI_CONSOLE_MAX_LINES);
        size_t line_len = strlen(s_ring.lines[idx]);
        size_t remaining = sizeof(text_buf) - used - 1;
        if (line_len + 1 >= remaining) {
            break; /* out of buffer room; stop appending further lines */
        }
        memcpy(text_buf + used, s_ring.lines[idx], line_len);
        used += line_len;
        text_buf[used++] = '\n';
        text_buf[used] = '\0';
    }

    lv_label_set_text(s_console_label, text_buf);

    /* Auto-scroll to the bottom so the newest line is visible. */
    if (s_console_panel != NULL) {
        lv_obj_scroll_to_y(s_console_panel, LV_COORD_MAX, LV_ANIM_OFF);
    }
}

void ui_console_log(const char * fmt, ...)
{
    if (fmt == NULL) {
        return;
    }

    char line[UI_CONSOLE_LINE_LEN + 1];
    va_list args;
    va_start(args, fmt);
    vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    line[UI_CONSOLE_LINE_LEN] = '\0'; /* guarantee termination on truncation */

    uint16_t write_idx = s_ring.head;
    strncpy(s_ring.lines[write_idx], line, UI_CONSOLE_LINE_LEN);
    s_ring.lines[write_idx][UI_CONSOLE_LINE_LEN] = '\0';

    s_ring.head = (uint16_t)((s_ring.head + 1) % UI_CONSOLE_MAX_LINES);
    if (s_ring.count < UI_CONSOLE_MAX_LINES) {
        s_ring.count++;
    }

    console_rebuild_text();
}

void ui_console_clear(void)
{
    memset(&s_ring, 0, sizeof(s_ring));
    console_rebuild_text();
}

static lv_obj_t * ui_console_tab_create(lv_obj_t * parent)
{
    lv_obj_set_style_pad_all(parent, 8, 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);

    s_console_panel = lv_obj_create(parent);
    lv_obj_remove_style_all(s_console_panel);
    lv_obj_add_style(s_console_panel, &s_style_console_panel, 0);
    lv_obj_set_size(s_console_panel, lv_pct(100), lv_pct(100));
    lv_obj_set_scroll_dir(s_console_panel, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_console_panel, LV_SCROLLBAR_MODE_AUTO);

    s_console_label = lv_label_create(s_console_panel);
    lv_label_set_long_mode(s_console_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_console_label, lv_pct(100));
    lv_obj_set_style_text_font(s_console_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_console_label, lv_color_hex(0xB8E6B8), 0); /* terminal-green-ish */
    lv_label_set_text(s_console_label, "");

    memset(&s_ring, 0, sizeof(s_ring));

    return parent;
}

/* ---- Public entry point -------------------------------------------------- */

lv_obj_t * ui_tabs_create(lv_obj_t * parent, ui_dashboard_icon_cb_t icon_cb,
                           void * user_data)
{
    if (parent == NULL) {
        return NULL;
    }

    ui_tabs_init_styles();

    lv_obj_t * tabview = lv_tabview_create(parent);
    lv_tabview_set_tab_bar_position(tabview, LV_DIR_TOP);
    lv_tabview_set_tab_bar_size(tabview, 44);

    lv_obj_t * tab_dashboard = lv_tabview_add_tab(tabview, "Dashboard");
    lv_obj_t * tab_console   = lv_tabview_add_tab(tabview, "Console");

    if (tab_dashboard == NULL || tab_console == NULL) {
        lv_obj_del(tabview);
        return NULL;
    }

    ui_dashboard_tab_create(tab_dashboard, icon_cb, user_data);
    ui_console_tab_create(tab_console);

    return tabview;
}
