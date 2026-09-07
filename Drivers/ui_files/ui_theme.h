/**
 * @file ui_theme.h
 * @brief Shared LVGL 9 theme: charcoal/amber flat design system.
 *
 * Palette:
 *   Background (charcoal) : #1C1E22
 *   Panel (flat opaque)   : #25282E
 *   Text (off-white)      : #E8E8E8
 *   Accent (amber)        : #E0A020
 *
 * Rules enforced by every style in this file:
 *   - bg_opa is always LV_OPA_COVER -- no transparency, no blur.
 *   - No gradients -- single flat bg_color only.
 *   - No shadows/glow -- shadow_width is always 0.
 *   - Radius is capped at UI_RADIUS (4px).
 *   - Separators are 1px hairlines, never borders styled as dividers.
 *
 * Fonts:
 *   - Chrome (labels, buttons, tab bar): compact sans, UI_FONT_CHROME.
 *   - Metadata / coordinates / numeric values: monospace, UI_FONT_MONO.
 *     LVGL ships a bitmap monospace font (unscii) if LV_FONT_UNSCII_16 is
 *     enabled in lv_conf.h. For production, swap in a proper monospace
 *     (e.g. JetBrains Mono / Space Mono) converted via the LVGL font
 *     converter at your target size -- see the comment at UI_FONT_MONO.
 */
#ifndef UI_THEME_H
#define UI_THEME_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Palette ---------------------------------------------------------- */

#define UI_COLOR_BG       lv_color_hex(0x1C1E22)  /* charcoal, screen background */
#define UI_COLOR_PANEL    lv_color_hex(0x25282E)  /* flat opaque panel fill */
#define UI_COLOR_TEXT     lv_color_hex(0xE8E8E8)  /* off-white text */
#define UI_COLOR_ACCENT   lv_color_hex(0xE0A020)  /* amber -- use sparingly: state/selection only */
#define UI_COLOR_HAIRLINE lv_color_hex(0x35383F)  /* separator lines, slightly lighter than panel */
#define UI_COLOR_TEXT_DIM lv_color_hex(0x9A9DA2)  /* dimmed off-white, for secondary/inactive text */

/* ---- Structural constants ---------------------------------------------- */

#define UI_RADIUS         4    /* hard cap -- never exceed this anywhere */
#define UI_HAIRLINE_W     1    /* separator/border width in px */
#define UI_PANEL_PAD      8
#define UI_TOOL_PANEL_W   150  /* width of left/right flanking panels */
#define UI_METADATA_H     28   /* height of the bottom monospace strip */

/* ---- Fonts -------------------------------------------------------------- */

/** Compact sans for chrome: labels, button captions, tab bar. */
#define UI_FONT_CHROME (&lv_font_montserrat_14)

/**
 * Monospace for metadata/coordinates/values. lv_font_unscii_16 is LVGL's
 * built-in bitmap monospace font -- enable LV_FONT_UNSCII_16 in lv_conf.h.
 * Replace this define with your own converted font object for production
 * (e.g. &jetbrains_mono_14) once you've run it through the LVGL font
 * converter at your target size/bpp.
 */
#define UI_FONT_MONO (&lv_font_unscii_16)

/* ---- Style objects (initialise once via ui_theme_init) ------------------ */

extern lv_style_t ui_style_screen_bg;   /* full-screen charcoal background */
extern lv_style_t ui_style_panel;       /* flat opaque tool-panel fill, 4px radius */
extern lv_style_t ui_style_canvas;      /* central work-area fill (slightly distinct from panel) */
extern lv_style_t ui_style_hairline_v;  /* 1px vertical separator */
extern lv_style_t ui_style_hairline_h;  /* 1px horizontal separator */
extern lv_style_t ui_style_chip;        /* small flat control (button/list item) inside a panel */
extern lv_style_t ui_style_chip_selected; /* chip state: amber accent, still flat/opaque */
extern lv_style_t ui_style_metadata_bar;  /* bottom monospace strip container */

/**
 * @brief Initialise all shared style objects. Call once at startup before
 *        creating any screen that uses ui_style_* objects.
 */
void ui_theme_init(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_THEME_H */
