/**
 * @file console_buffer.h
 * @brief Ring-buffered console log: ui_console_log() appends formatted
 *        lines; ui_console_bind() attaches the buffer to a label/scroll
 *        container created elsewhere (e.g. a SquareLine Studio screen).
 */
#ifndef CONSOLE_BUFFER_H
#define CONSOLE_BUFFER_H

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

/**
 * @brief Append a formatted line to the console tab.
 *
 * Cheap: only writes into the ring buffer and sets a dirty flag. The actual
 * lv_textarea_set_text() call happens later, from the console's own
 * lv_timer (see ui_console_bind()) at a fixed 2fps, independent of
 * whatever rate other channels (Spectrum/Spectrogram/Waveform) redraw at.
 *
 * Thread-safety: the ring-buffer write itself is not locked. If you call
 * this from a context other than the LVGL task/thread (e.g. an ISR-fed
 * queue consumer, or another RTOS task), wrap the call with your LVGL
 * port's lock/unlock (lv_lock()/lv_unlock() on LVGL 9, or your own mutex)
 * at the call site to avoid racing the timer's read of the same buffer.
 * The widget update itself always runs from the timer, i.e. the LVGL
 * task/thread, so no locking is needed for that part.
 *
 * Lines longer than UI_CONSOLE_LINE_LEN-1 are truncated. Once
 * UI_CONSOLE_MAX_LINES is reached, the oldest line is dropped (ring buffer).
 *
 * @param fmt printf-style format string.
 * @param ... format arguments.
 */
void ui_console_log(const char * fmt, ...);

/**
 * @brief Bind the console log to an existing lv_textarea created elsewhere
 *        -- e.g. ui_TextArea1, created by SquareLine Studio on the Console
 *        tab. Must be a textarea (lv_textarea_create()), not a label --
 *        the ring buffer's text is written via lv_textarea_set_text() and
 *        auto-scrolled via the textarea's own cursor position, which only
 *        works on the correct widget type.
 *
 * Also creates the console's own 2fps lv_timer on first call (see
 * UI_CONSOLE_REFRESH_PERIOD_MS in console_buffer.c to change the rate).
 *
 * Call this once, after the screen containing that textarea has been
 * created (ui_init() has run) and before you expect ui_console_log() calls
 * to be visible. Safe to call again later to rebind to a different
 * textarea (e.g. if SquareLine screens get torn down/recreated) -- this
 * retargets the existing timer rather than creating a second one.
 *
 * @param textarea  the lv_textarea that will display the log text.
 */
void ui_console_bind(lv_obj_t * textarea);

/**
 * @brief Clear all lines currently shown in the console tab. Takes effect
 *        on the next 2fps timer tick, same as a logged line.
 */
void ui_console_clear(void);

/**
 * @brief Update a plain label (e.g. ui_dropped) with a formatted dropped-
 *        packet count. Independent of the ring buffer/textarea above --
 *        for a dedicated status label, not the scrolling log.
 *
 * @param dropped_label  the label to update (e.g. ui_dropped).
 * @param count          current dropped-buffer count, e.g. from
 *                        AUDIO_REC_GetDroppedCount().
 */
void ui_console_set_dropped_count(lv_obj_t * dropped_label, uint32_t count);

#ifdef __cplusplus
}
#endif

#endif /* CONSOLE_BUFFER_H */
