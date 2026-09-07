/**
 * @file ui_main_layout.h
 * @brief Main screen layout: dense tool panels flanking a large central
 *        canvas, with a monospace metadata strip along the bottom.
 *        Built entirely on the ui_theme.h style set.
 */
#ifndef UI_MAIN_LAYOUT_H
#define UI_MAIN_LAYOUT_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque handle to the created layout's key sub-objects, so callers can
 *  populate the canvas (e.g. attach a chart) and update metadata/panels. */
typedef struct {
    lv_obj_t * screen;
    lv_obj_t * left_panel;
    lv_obj_t * canvas;          /* central work area -- attach your chart/canvas widget here */
    lv_obj_t * right_panel;
    lv_obj_t * metadata_label;  /* monospace label inside the bottom strip */
} ui_main_layout_t;

/**
 * @brief Build the main screen layout on the given parent (or a fresh
 *        screen if parent is NULL).
 *
 * Call ui_theme_init() once before this.
 *
 * @param parent Screen/container to build into, or NULL to create a new screen.
 * @param out    Filled with pointers to the created sub-objects.
 * @return true on success, false on allocation failure (out is left zeroed).
 */
bool ui_main_layout_create(lv_obj_t * parent, ui_main_layout_t * out);

/**
 * @brief Add a labelled chip (tool-panel row item) to a panel.
 *
 * Adds a hairline separator above the chip if it is not the first child,
 * so panel rows read as a dense list without boxing each item individually.
 *
 * @param panel   left_panel or right_panel from a ui_main_layout_t.
 * @param text    chip label text.
 * @param selected if true, uses the amber "active" chip style instead of flat default.
 * @return the created chip object.
 */
lv_obj_t * ui_main_layout_add_chip(lv_obj_t * panel, const char * text, bool selected);

/**
 * @brief Update the monospace metadata strip text (coordinates/values/etc.).
 * @param layout   pointer previously filled by ui_main_layout_create.
 * @param fmt      printf-style format string.
 */
void ui_main_layout_set_metadata(ui_main_layout_t * layout, const char * fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* UI_MAIN_LAYOUT_H */
