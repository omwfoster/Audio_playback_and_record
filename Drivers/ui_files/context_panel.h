/**
 * @file context_panel.h
 * @brief Context area of the right-hand panel (ui_ContextContainer): one
 *        panel shown at a time, switched in code (no tabs, no swiping).
 *
 *   ui_Panel12  FFT status card: state, elapsed time, dropped buffers, FFT size
 *   ui_Panel13  Spectrogram settings: adaptive scaling and contrast sliders
 *
 * The settings panel is shown while the Spectrogram tab is open, the status
 * card otherwise. Panels are built in code inside SquareLine placeholders.
 */
#ifndef CONTEXT_PANEL_H
#define CONTEXT_PANEL_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Show @p panel and hide every other child of its parent. Hidden
 *        children are skipped by flex layout, so the shown panel fills the
 *        container. Works for any panel added to ui_ContextContainer.
 */
void context_show(lv_obj_t * panel);

/**
 * @brief Build the status card and settings panel, and hook the tab view so
 *        the context follows the open tab. Call once after ui_init().
 * @param colour_refresh  called after the contrast sliders rebuild the colour
 *                        table, to repaint things drawn from it elsewhere
 *                        (colour bar, bar chart); may be NULL.
 */
void context_panel_init(void (*colour_refresh)(void));

/**
 * @brief Refresh the status card from the recorder. Call every main-loop
 *        pass; labels only change when something visible changes.
 */
void context_panel_update(void);

#ifdef __cplusplus
}
#endif

#endif /* CONTEXT_PANEL_H */
