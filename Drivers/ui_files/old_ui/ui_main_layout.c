/**
 * @file ui_main_layout.c
 * @brief Implementation of ui_main_layout_create() and helpers.
 */
#include "ui_main_layout.h"
#include "ui_theme.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

bool ui_main_layout_create(lv_obj_t * parent, ui_main_layout_t * out)
{
    if (out == NULL) {
        return false;
    }
    memset(out, 0, sizeof(*out));

    lv_obj_t * screen = (parent != NULL) ? parent : lv_obj_create(NULL);
    if (screen == NULL) {
        return false;
    }
    lv_obj_remove_style_all(screen);
    lv_obj_add_style(screen, &ui_style_screen_bg, 0);
    lv_obj_set_size(screen, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    /* Outer column: [ main row (panels + canvas) ] / [ metadata strip ] */
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_style_pad_row(screen, 0, 0);

    lv_obj_t * main_row = lv_obj_create(screen);
    lv_obj_remove_style_all(main_row);
    lv_obj_set_size(main_row, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_grow(main_row, 1);
    lv_obj_set_flex_flow(main_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(main_row, 6, 0);
    lv_obj_set_style_pad_column(main_row, 6, 0);
    lv_obj_clear_flag(main_row, LV_OBJ_FLAG_SCROLLABLE);

    /* Left tool panel -- fixed width, vertical list of chips. */
    lv_obj_t * left_panel = lv_obj_create(main_row);
    lv_obj_remove_style_all(left_panel);
    lv_obj_add_style(left_panel, &ui_style_panel, 0);
    lv_obj_set_size(left_panel, UI_TOOL_PANEL_W, lv_pct(100));
    lv_obj_set_flex_flow(left_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(left_panel, 0, 0);
    lv_obj_set_scroll_dir(left_panel, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(left_panel, LV_SCROLLBAR_MODE_AUTO);

    /* Central canvas -- takes all remaining width/height. Caller attaches
     * a chart/canvas/image widget as a child of this object. */
    lv_obj_t * canvas = lv_obj_create(main_row);
    lv_obj_remove_style_all(canvas);
    lv_obj_add_style(canvas, &ui_style_canvas, 0);
    lv_obj_set_flex_grow(canvas, 1);
    lv_obj_set_height(canvas, lv_pct(100));
    lv_obj_clear_flag(canvas, LV_OBJ_FLAG_SCROLLABLE);

    /* Right tool panel -- mirrors the left. */
    lv_obj_t * right_panel = lv_obj_create(main_row);
    lv_obj_remove_style_all(right_panel);
    lv_obj_add_style(right_panel, &ui_style_panel, 0);
    lv_obj_set_size(right_panel, UI_TOOL_PANEL_W, lv_pct(100));
    lv_obj_set_flex_flow(right_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(right_panel, 0, 0);
    lv_obj_set_scroll_dir(right_panel, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(right_panel, LV_SCROLLBAR_MODE_AUTO);

    /* Bottom metadata strip -- full width, fixed height, monospace text. */
    lv_obj_t * metadata_bar = lv_obj_create(screen);
    lv_obj_remove_style_all(metadata_bar);
    lv_obj_add_style(metadata_bar, &ui_style_metadata_bar, 0);
    lv_obj_set_size(metadata_bar, lv_pct(100), UI_METADATA_H);
    lv_obj_clear_flag(metadata_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * metadata_label = lv_label_create(metadata_bar);
    lv_label_set_long_mode(metadata_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(metadata_label, lv_pct(100));
    lv_obj_align(metadata_label, LV_ALIGN_LEFT_MID, 0, 0);
    lv_label_set_text(metadata_label, "");

    out->screen = screen;
    out->left_panel = left_panel;
    out->canvas = canvas;
    out->right_panel = right_panel;
    out->metadata_label = metadata_label;
    return true;
}

/**
 * @brief Shared chip construction used by both ui_main_layout_add_chip and
 *        ui_main_layout_add_button. Adds a hairline separator above the
 *        chip if it is not the first child of the panel.
 */
static lv_obj_t * create_chip_base(lv_obj_t * panel, const char * text, bool selected)
{
    uint32_t existing = lv_obj_get_child_count(panel);
    if (existing > 0) {
        lv_obj_t * sep = lv_obj_create(panel);
        lv_obj_remove_style_all(sep);
        lv_obj_add_style(sep, &ui_style_hairline_h, 0);
        lv_obj_set_size(sep, lv_pct(100), UI_HAIRLINE_W);
    }

    lv_obj_t * chip = lv_obj_create(panel);
    lv_obj_remove_style_all(chip);
    lv_obj_add_style(chip, &ui_style_chip, 0);
    if (selected) {
        lv_obj_add_style(chip, &ui_style_chip_selected, 0);
    }
    lv_obj_set_size(chip, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_add_flag(chip, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t * label = lv_label_create(chip);
    lv_label_set_text(label, text);
    if (selected) {
        lv_obj_set_style_text_color(label, UI_COLOR_BG, 0);
    }

    return chip;
}

lv_obj_t * ui_main_layout_add_chip(lv_obj_t * panel, const char * text, bool selected)
{
    if (panel == NULL || text == NULL) {
        return NULL;
    }
    return create_chip_base(panel, text, selected);
}

lv_obj_t * ui_main_layout_add_button(lv_obj_t * panel, const char * text,
                                      lv_event_cb_t event_cb, void * user_data)
{
    if (panel == NULL || text == NULL) {
        return NULL;
    }

    lv_obj_t * chip = create_chip_base(panel, text, false);
    if (event_cb != NULL) {
        lv_obj_add_event_cb(chip, event_cb, LV_EVENT_CLICKED, user_data);
    }
    return chip;
}

void ui_main_layout_set_metadata(ui_main_layout_t * layout, const char * fmt, ...)
{
    if (layout == NULL || layout->metadata_label == NULL || fmt == NULL) {
        return;
    }

    char buf[96];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    buf[sizeof(buf) - 1] = '\0';

    lv_label_set_text(layout->metadata_label, buf);
}
