/**
 * @file ui_theme.c
 * @brief Initialises the shared flat charcoal/amber style objects.
 */
#include "ui_theme.h"

lv_style_t ui_style_screen_bg;
lv_style_t ui_style_panel;
lv_style_t ui_style_canvas;
lv_style_t ui_style_hairline_v;
lv_style_t ui_style_hairline_h;
lv_style_t ui_style_chip;
lv_style_t ui_style_chip_selected;
lv_style_t ui_style_metadata_bar;

static bool s_initialised = false;

void ui_theme_init(void)
{
    if (s_initialised) {
        return;
    }

    /* Screen background: flat charcoal, fully opaque, no wallpaper/image. */
    lv_style_init(&ui_style_screen_bg);
    lv_style_set_bg_opa(&ui_style_screen_bg, LV_OPA_COVER);
    lv_style_set_bg_color(&ui_style_screen_bg, UI_COLOR_BG);
    lv_style_set_border_width(&ui_style_screen_bg, 0);
    lv_style_set_radius(&ui_style_screen_bg, 0);

    /* Tool panel: flat opaque fill, 4px radius, hairline border, no shadow. */
    lv_style_init(&ui_style_panel);
    lv_style_set_bg_opa(&ui_style_panel, LV_OPA_COVER);
    lv_style_set_bg_color(&ui_style_panel, UI_COLOR_PANEL);
    lv_style_set_radius(&ui_style_panel, UI_RADIUS);
    lv_style_set_border_width(&ui_style_panel, UI_HAIRLINE_W);
    lv_style_set_border_color(&ui_style_panel, UI_COLOR_HAIRLINE);
    lv_style_set_border_opa(&ui_style_panel, LV_OPA_COVER);
    lv_style_set_shadow_width(&ui_style_panel, 0);
    lv_style_set_pad_all(&ui_style_panel, UI_PANEL_PAD);
    lv_style_set_text_color(&ui_style_panel, UI_COLOR_TEXT);
    lv_style_set_text_font(&ui_style_panel, UI_FONT_CHROME);

    /* Central canvas: same flatness rules, distinct fill so it reads as the
     * "stage" rather than another panel. Still no gradient/shadow. */
    lv_style_init(&ui_style_canvas);
    lv_style_set_bg_opa(&ui_style_canvas, LV_OPA_COVER);
    lv_style_set_bg_color(&ui_style_canvas, UI_COLOR_BG);
    lv_style_set_radius(&ui_style_canvas, UI_RADIUS);
    lv_style_set_border_width(&ui_style_canvas, UI_HAIRLINE_W);
    lv_style_set_border_color(&ui_style_canvas, UI_COLOR_HAIRLINE);
    lv_style_set_border_opa(&ui_style_canvas, LV_OPA_COVER);
    lv_style_set_shadow_width(&ui_style_canvas, 0);
    lv_style_set_pad_all(&ui_style_canvas, 0);

    /* Hairline separators -- a 1px line, not a styled border-as-divider. */
    lv_style_init(&ui_style_hairline_v);
    lv_style_set_bg_opa(&ui_style_hairline_v, LV_OPA_COVER);
    lv_style_set_bg_color(&ui_style_hairline_v, UI_COLOR_HAIRLINE);
    lv_style_set_radius(&ui_style_hairline_v, 0);
    lv_style_set_border_width(&ui_style_hairline_v, 0);

    lv_style_init(&ui_style_hairline_h);
    lv_style_set_bg_opa(&ui_style_hairline_h, LV_OPA_COVER);
    lv_style_set_bg_color(&ui_style_hairline_h, UI_COLOR_HAIRLINE);
    lv_style_set_radius(&ui_style_hairline_h, 0);
    lv_style_set_border_width(&ui_style_hairline_h, 0);

    /* Chip: a small flat control inside a panel (list item / button row).
     * Default state has no fill distinct from the panel -- separation comes
     * from hairline dividers between chips, not from boxing each one. */
    lv_style_init(&ui_style_chip);
    lv_style_set_bg_opa(&ui_style_chip, LV_OPA_COVER);
    lv_style_set_bg_color(&ui_style_chip, UI_COLOR_PANEL);
    lv_style_set_radius(&ui_style_chip, 0);
    lv_style_set_border_width(&ui_style_chip, 0);
    lv_style_set_shadow_width(&ui_style_chip, 0);
    lv_style_set_pad_all(&ui_style_chip, 6);
    lv_style_set_text_color(&ui_style_chip, UI_COLOR_TEXT);
    lv_style_set_text_font(&ui_style_chip, UI_FONT_CHROME);

    /* Selected/active chip: amber accent used sparingly as a flat fill --
     * never as a glow, gradient, or saturated icon tint. */
    lv_style_init(&ui_style_chip_selected);
    lv_style_set_bg_opa(&ui_style_chip_selected, LV_OPA_COVER);
    lv_style_set_bg_color(&ui_style_chip_selected, UI_COLOR_ACCENT);
    lv_style_set_text_color(&ui_style_chip_selected, UI_COLOR_BG);
    lv_style_set_shadow_width(&ui_style_chip_selected, 0);

    /* Bottom metadata strip: flat panel fill, monospace text, hairline top
     * border acting as the separator from the main work area. */
    lv_style_init(&ui_style_metadata_bar);
    lv_style_set_bg_opa(&ui_style_metadata_bar, LV_OPA_COVER);
    lv_style_set_bg_color(&ui_style_metadata_bar, UI_COLOR_PANEL);
    lv_style_set_radius(&ui_style_metadata_bar, 0);
    lv_style_set_border_width(&ui_style_metadata_bar, UI_HAIRLINE_W);
    lv_style_set_border_side(&ui_style_metadata_bar, LV_BORDER_SIDE_TOP);
    lv_style_set_border_color(&ui_style_metadata_bar, UI_COLOR_HAIRLINE);
    lv_style_set_border_opa(&ui_style_metadata_bar, LV_OPA_COVER);
    lv_style_set_shadow_width(&ui_style_metadata_bar, 0);
    lv_style_set_pad_hor(&ui_style_metadata_bar, 12);
    lv_style_set_pad_ver(&ui_style_metadata_bar, 4);
    lv_style_set_text_color(&ui_style_metadata_bar, UI_COLOR_TEXT);
    lv_style_set_text_font(&ui_style_metadata_bar, UI_FONT_MONO);

    s_initialised = true;
}
