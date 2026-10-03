/**
 * @file console_buffer.c
 * @brief Ring-buffered console log (no heap allocation on the log path --
 *        safe for embedded use). ui_console_log() appends formatted lines;
 *        ui_console_bind() attaches the buffer to a label/scroll container
 *        created elsewhere, e.g. a SquareLine Studio screen/tab.
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include "console_buffer.h"
#include "prof.h"


/* ---- Module state ---------------------------------------------------- */

typedef struct {
    char lines[UI_CONSOLE_MAX_LINES][UI_CONSOLE_LINE_LEN + 1];
    uint16_t head;   /* index where the next line will be written */
    uint16_t count;  /* number of valid lines currently stored (<= MAX_LINES) */
} ui_console_ring_t;

static ui_console_ring_t s_ring;
static lv_obj_t * s_console_ta = NULL; /* the bound lv_textarea, e.g. ui_TextArea1 */
static lv_timer_t * s_console_timer = NULL;
static volatile bool s_console_dirty = false; /* set by ui_console_log(), cleared by the timer */
static bool s_rebuilding = false; /* true while console_rebuild_text() is updating the textarea */

/* Console redraw runs at its own fixed cadence, independent of whatever
 * rate the Spectrum/Spectrogram/Waveform channels redraw at. 2fps = 500ms. */
#define UI_CONSOLE_REFRESH_PERIOD_MS 500u



/* ---- Console tab -------------------------------------------------------- */

static void console_rebuild_text(void)
{
    if (s_console_ta == NULL) {
        return;
    }

    /* Concatenate the ring buffer into one string. For MAX_LINES=100 and
     * LINE_LEN=96 this static buffer is ~9.6KB -- fine on the F769I-DISCO's
     * SRAM; shrink UI_CONSOLE_MAX_LINES/LINE_LEN if your target is tighter. */
    static char text_buf[(UI_CONSOLE_LINE_LEN + 1) * UI_CONSOLE_MAX_LINES + 1]; /* +1 for the final NUL */
    text_buf[0] = '\0';
    size_t used = 0;

    uint16_t start = (s_ring.count < UI_CONSOLE_MAX_LINES)
                          ? 0
                          : s_ring.head; /* oldest line index in a full buffer */

    /* Only the newest UI_CONSOLE_SHOW_LINES lines go to the widget. */
    uint16_t skip = (s_ring.count > UI_CONSOLE_SHOW_LINES)
                        ? (uint16_t)(s_ring.count - UI_CONSOLE_SHOW_LINES)
                        : 0;

    for (uint16_t i = skip; i < s_ring.count; i++) {
        uint16_t idx = (uint16_t)((start + i) % UI_CONSOLE_MAX_LINES);
        size_t line_len = strlen(s_ring.lines[idx]);
        size_t remaining = sizeof(text_buf) - used - 1;
        if (line_len + 1 > remaining) {
            break; /* out of buffer room; stop appending further lines */
        }
        memcpy(text_buf + used, s_ring.lines[idx], line_len);
        used += line_len;
        text_buf[used++] = '\n';
        text_buf[used] = '\0';
    }

    /* ui_TextArea1 is an lv_textarea (lv_textarea_create() in ui_Screen1.c).
     * lv_label_set_text() is only ever called on its inner label below,
     * never on the textarea object itself. */
    s_rebuilding = true;

    /* Set the text on the textarea's inner label, not via
     * lv_textarea_set_text(): that also moves the cursor to the end and
     * scrolls there with an animation, which redraws the whole textarea on
     * every frame of the animation (several full redraws per log line).
     * Here: one jump to the bottom, no animation, one redraw. The cursor
     * isn't shown (the textarea is never focused), so its position doesn't
     * matter. */
    lv_label_set_text(lv_textarea_get_label(s_console_ta), text_buf);
    lv_obj_update_layout(s_console_ta); /* so the new content height is known */
    lv_obj_scroll_to_y(s_console_ta, LV_COORD_MAX, LV_ANIM_OFF);
    s_rebuilding = false;
}

/* Copy one already-formatted line into the ring buffer and flag a redraw. */
static void console_append_line(const char * line)
{
    uint16_t write_idx = s_ring.head;
    strncpy(s_ring.lines[write_idx], line, UI_CONSOLE_LINE_LEN);
    s_ring.lines[write_idx][UI_CONSOLE_LINE_LEN] = '\0';

    s_ring.head = (uint16_t)((s_ring.head + 1) % UI_CONSOLE_MAX_LINES);
    if (s_ring.count < UI_CONSOLE_MAX_LINES) {
        s_ring.count++;
    }

    /* Cheap: just flag that a redraw is due. The actual lv_textarea_set_text()
     * call (which rebuilds the widget's internal layout) happens on the
     * console's own timer, not on every log call. */
    s_console_dirty = true;
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

    console_append_line(line);
}

#if LV_USE_LOG
void ui_console_lv_log_cb(lv_log_level_t level, const char * buf)
{
    /* LVGL already puts "[Warn]" etc. at the start of buf. INFO/TRACE are
     * dropped: the console's own redraw triggers some of them (e.g. the
     * flex layout's "update ... container"), which would redraw it again. */
    if (level < UI_CONSOLE_LV_LOG_MIN_LEVEL) {
        return;
    }

    /* Drop messages caused by our own textarea update; logging them would
     * mark the console dirty again and redraw it on every timer tick. */
    if (buf == NULL || s_rebuilding) {
        return;
    }

    /* Split on newlines, turn tabs into spaces, and wrap anything longer
     * than one ring-buffer line. Never passes buf through a format string,
     * since LVGL messages can contain '%'. */
    char line[UI_CONSOLE_LINE_LEN + 1];
    size_t len = 0;
    for (const char * p = buf; ; p++) {
        char c = *p;
        if (c == '\0' || c == '\n' || c == '\r' || len == UI_CONSOLE_LINE_LEN) {
            if (len > 0) {
                line[len] = '\0';
                console_append_line(line);
                len = 0;
            }
            if (c == '\0') {
                break;
            }
            if (c == '\n' || c == '\r') {
                continue;
            }
        }
        line[len++] = (c == '\t') ? ' ' : c;
    }
}
#endif

/**
 * @brief Fired every UI_CONSOLE_REFRESH_PERIOD_MS. Only touches the widget
 *        if something actually changed since the last tick -- an idle
 *        console costs nothing beyond the flag check.
 */
static void console_timer_cb(lv_timer_t * timer)
{
    (void)timer;
    if (s_console_dirty) {
        s_console_dirty = false;
        uint32_t t_prof = prof_start();
        console_rebuild_text();
        prof_stop(&g_prof_console, t_prof);
    }
}

void ui_console_bind(lv_obj_t * textarea)
{
    s_console_ta = textarea;

    if (s_console_ta != NULL) {
        /* The default light theme's text color reads as very faint against
         * a manually-set black background (SquareLine didn't set an
         * explicit text color on this textarea) -- force full-opacity
         * off-white here so it's readable regardless of theme/background,
         * and this survives ui_Screen1.c being regenerated by Studio. */
        lv_obj_set_style_text_color(s_console_ta, lv_color_hex(0xFAFAFA), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_opa(s_console_ta, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    /* Render whatever's already in the ring buffer immediately, so binding
     * after some ui_console_log() calls have already happened (e.g. boot
     * messages logged before the screen existed) doesn't leave the text
     * area blank until the timer's first tick. */
    console_rebuild_text();
    s_console_dirty = false;

    /* Own timer, independent of any other channel's redraw cadence.
     * Created once -- rebinding to a different textarea later just retargets
     * the existing timer rather than creating a second one. */
    if (s_console_timer == NULL) {
        s_console_timer = lv_timer_create(console_timer_cb, UI_CONSOLE_REFRESH_PERIOD_MS, NULL);
    }
}

void ui_console_clear(void)
{
    memset(&s_ring, 0, sizeof(s_ring));
    s_console_dirty = true; /* next timer tick will blank the textarea */
}

void ui_console_set_dropped_count(lv_obj_t * dropped_label, uint32_t count)
{
    if (dropped_label == NULL) {
        return;
    }
    lv_label_set_text_fmt(dropped_label, "%lu packets dropped", (unsigned long)count);
}
