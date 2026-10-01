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

/** Number of most recent lines actually put in the textarea. Every console
 *  refresh makes LVGL re-wrap and measure the whole text, so the cost grows
 *  with this; at 100 lines it blocked the main loop for ~0.5 s and caused
 *  audio drops. Roughly one screenful is enough, since the textarea is
 *  pinned to the bottom anyway. */
#ifndef UI_CONSOLE_SHOW_LINES
#define UI_CONSOLE_SHOW_LINES  25u
#endif

/** Lowest LVGL log level ui_console_lv_log_cb() shows. INFO and TRACE
 *  messages come from layout, refresh and timers, and showing them on the
 *  console makes it redraw, which logs more of them: a feedback loop that
 *  pins the CPU. Keep this at WARN or above. */
#ifndef UI_CONSOLE_LV_LOG_MIN_LEVEL
#define UI_CONSOLE_LV_LOG_MIN_LEVEL  LV_LOG_LEVEL_WARN
#endif

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

#if LV_USE_LOG
/**
 * @brief LVGL log print callback that writes LVGL's own log messages into
 *        the console. Register it after lv_init() (lv_init() resets it):
 *
 *            lv_log_register_print_cb(ui_console_lv_log_cb);
 *
 * Only messages at UI_CONSOLE_LV_LOG_MIN_LEVEL (WARN) or above are shown.
 * Tabs become spaces, the trailing newline is dropped, and messages longer
 * than UI_CONSOLE_LINE_LEN are wrapped onto extra lines. Messages LVGL logs
 * while the console itself is updating its textarea are ignored, so a
 * warning from that update can't make the console redraw forever.
 *
 * Needs LV_USE_LOG 1 and LV_LOG_PRINTF 0 in lv_conf.h; LV_LOG_LEVEL picks
 * which messages arrive.
 */
void ui_console_lv_log_cb(lv_log_level_t level, const char * buf);
#endif

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
