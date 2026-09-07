/**
  ******************************************************************************
  * @file    app_state.h
  * @brief   Single owner for shared application state.
  *
  * Replaces the overlapping enums previously spread across main.h:
  *   AUDIO_ApplicationTypeDef      -> StorageState
  *   AUDIO_Demo_State              -> (deleted; LVGL owns navigation)
  *   AUDIO_PLAYBACK_StateTypeDef   -> CaptureState + AppEvent + AudioSinkMask
  *   BUFFER_StateTypeDef           -> BufferHalf
  *   WR_BUFFER_StateTypeDef        -> BufferHalf
  *
  * Every symbol here is declared `extern` and defined exactly once, in
  * app_state.c. Nothing in this header is a tentative definition, so it is safe
  * to include from any number of translation units under -fno-common.
  ******************************************************************************
  */

#ifndef __APP_STATE_H
#define __APP_STATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/* ---------------------------------------------------------------------------
 * Axis 1: storage availability. Driven purely by card detect + f_mount.
 * Knows nothing about audio.
 * ------------------------------------------------------------------------ */
typedef enum {
    STORAGE_ABSENT = 0,   /* no card in the slot                             */
    STORAGE_READY,        /* card mounted, writable                          */
    STORAGE_REMOVED,      /* card pulled; needs teardown, then -> ABSENT     */
    STORAGE_MOUNT_FAILED, /* card present but f_mount failed; do not retry
                             every loop -- retry is gated on re-insertion    */
} StorageState;

/* ---------------------------------------------------------------------------
 * Axis 2: capture engine. Tracks the DFSDM/SAI DMA only. Knows nothing about
 * where the samples go.
 * ------------------------------------------------------------------------ */
typedef enum {
    CAPTURE_IDLE = 0,     /* DMA stopped                                     */
    CAPTURE_STARTING,     /* AUDIO_REC_Start issued, awaiting first callback */
    CAPTURE_RUNNING,      /* DMA circular, callbacks arriving                */
    CAPTURE_STOPPING,     /* stop requested, awaiting quiesce                */
    CAPTURE_ERROR,        /* start or process returned an error              */
} CaptureState;

/* ---------------------------------------------------------------------------
 * Axis 3: sinks. A BITMASK, not an enum-as-state: recording to SD while also
 * feeding the on-screen FFT is a normal combination, and the old
 * single-valued stream_status.mode could not express it.
 * ------------------------------------------------------------------------ */
typedef enum {
    SINK_NONE       = 0u,
    SINK_SD_RECORD  = 1u << 0,  /* write PCM to the open WAV file            */
    SINK_UART_RAW   = 1u << 1,  /* stream raw samples over USART1            */
    SINK_UART_FFT   = 1u << 2,  /* stream magnitude bins over USART1         */
    SINK_UI_FFT     = 1u << 3,  /* feed the LVGL spectrum/spectrogram tabs   */
} AudioSink;

typedef uint32_t AudioSinkMask;

/* ---------------------------------------------------------------------------
 * Events. These are things that HAPPENED, not states the system rests in.
 * Keeping them in their own type is what stops VOLUME_UP from being stored in
 * a state variable. Produced by the LVGL callbacks and the UART command
 * parser; consumed once per App_Process() pass and then cleared.
 * ------------------------------------------------------------------------ */
typedef enum {
    EVT_NONE = 0,
    EVT_RECORD_START,
    EVT_RECORD_STOP,
    EVT_PLAY_START,
    EVT_PLAY_STOP,
    EVT_PAUSE,
    EVT_RESUME,
    EVT_VOLUME_UP,
    EVT_VOLUME_DOWN,
    EVT_SINK_ENABLE,      /* payload = AudioSink bit to set                  */
    EVT_SINK_DISABLE,     /* payload = AudioSink bit to clear                */
} AppEventType;

typedef struct {
    AppEventType type;
    uint32_t     payload;
} AppEvent;

/* ---------------------------------------------------------------------------
 * DMA buffer half identification. Replaces BOTH old buffer enums.
 *
 * Note this is a BITMASK too, deliberately. The old code used a pair of
 * volatile flags tested with `else if`, which silently discarded one half when
 * both were pending -- an overrun looked identical to normal operation. Here a
 * missed half is visible: the ISR ORs in a bit, and if the bit was already set
 * it bumps the overrun counter instead of losing the event.
 * ------------------------------------------------------------------------ */
typedef enum {
    BUF_HALF_NONE   = 0u,
    BUF_HALF_FIRST  = 1u << 0,
    BUF_HALF_SECOND = 1u << 1,
} BufferHalf;

typedef struct {
    volatile uint32_t pending;   /* BufferHalf bits awaiting processing      */
    volatile uint32_t overruns;  /* halves dropped because we fell behind    */
} CaptureFlags;

/* ---------------------------------------------------------------------------
 * The single shared state object. One struct beats six loose globals: there is
 * exactly one symbol to own, and the ownership bug that produced the
 * "multiple definition" and "undefined reference" errors cannot recur.
 * ------------------------------------------------------------------------ */
typedef struct {
    StorageState  storage;
    CaptureState  capture;
    AudioSinkMask sinks;
    CaptureFlags  flags;
    AppEvent      pending_event;
} AppState;

extern AppState g_app;

/* Event helpers. Call Post from LVGL callbacks / UART parser (and from ISRs --
 * the write is a single word, so it is atomic on Cortex-M7). */
void       App_PostEvent(AppEventType type, uint32_t payload);
AppEvent   App_TakeEvent(void);
void App_Process(void);

/* Convenience predicates -- these read better at the call site than raw
 * comparisons and keep the enum names out of the loop body. */
static inline bool App_StorageReady(void)  { return g_app.storage == STORAGE_READY; }
static inline bool App_Capturing(void)     { return g_app.capture == CAPTURE_RUNNING; }
static inline bool App_SinkActive(AudioSink s) { return (g_app.sinks & (uint32_t)s) != 0u; }

#ifdef __cplusplus
}
#endif

#endif /* __APP_STATE_H */
