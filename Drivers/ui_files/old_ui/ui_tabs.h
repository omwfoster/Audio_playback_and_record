/**
 * @file ui_tabs.h
 * @brief Two-tab LVGL 9 layout: a flat-style dashboard overview tab and a
 *        scrolling console/log tab. Designed to sit alongside the existing
 *        SquareLine-generated Spectrum/Spectrogram/Waveform tabs.
 */
#ifndef UI_TABS_H
#define UI_TABS_H

#include "lvgl.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Config -------------------------------------------------------- */

/** Max characters retained per console log line (excluding null terminator). */
#define UI_CONSOLE_LINE_LEN   96u

/** Max number of lines kept in the console ring buffer before oldest is dropped. */
#define UI_CONSOLE_MAX_LINES  100u

/** Number of quick-access icon buttons on the dashboard tab. */
#define UI_DASHBOARD_ICON_COUNT 3u

/**
 * @brief Callback fired when a dashboard icon button is pressed.
 * @param index index into the icon set (0..UI_DASHBOARD_ICON_COUNT-1)
 * @param user_data opaque pointer passed through from ui_tabs_create()
 */
typedef void (*ui_dashboard_icon_cb_t)(uint8_t index, void * user_data);

/**
 * @brief Create the tabview containing the Dashboard and Console tabs.
 *
 * Must be called after LVGL and the display driver are initialised.
 * Safe to call once; the returned object owns all child widgets.
 *
 * @param parent      Screen or container to attach the tabview to (e.g. lv_scr_act()).
 * @param icon_cb     Optional callback invoked when a dashboard icon is tapped. May be NULL.
 * @param user_data   Opaque pointer forwarded to icon_cb. May be NULL.
 * @return Pointer to the created tabview object, or NULL on allocation failure.
 */
lv_obj_t * ui_tabs_create(lv_obj_t * parent, ui_dashboard_icon_cb_t icon_cb, void * user_data);

/**
 * @brief Append a formatted line to the console tab.
 *
 * Thread-safety: this function itself does not take the LVGL lock. If you
 * call it from a context other than the LVGL task/thread (e.g. an ISR-fed
 * queue consumer, or another RTOS task), wrap the call with your LVGL
 * port's lock/unlock (lv_lock()/lv_unlock() on LVGL 9, or your own mutex if
 * you're on an older port) at the call site.
 *
 * Lines longer than UI_CONSOLE_LINE_LEN-1 are truncated. Once
 * UI_CONSOLE_MAX_LINES is reached, the oldest line is dropped (ring buffer).
 *
 * @param fmt printf-style format string.
 * @param ... format arguments.
 */
void ui_console_log(const char * fmt, ...);

/**
 * @brief Clear all lines currently shown in the console tab.
 */
void ui_console_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_TABS_H */
