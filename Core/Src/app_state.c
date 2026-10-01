/**
  ******************************************************************************
  * @file    app_state.c
  * @brief   Sole definition site for shared application state, plus the
  *          flattened per-pass processing that replaces the nested switch in
  *          main()'s while(1).
  ******************************************************************************
  */

#include "app_state.h"
#include "main.h"
#include "audio_stream.h"
#include "audio_stream_fft.h"
#include "waverecorder.h"
#include "waveplayer.h"

/* THE definition. Every other TU sees only the extern in app_state.h. */
AppState g_app;

extern AUDIO_IN_BufferTypeDef BufferCtl_In;
extern uint16_t pcm_left[];
extern uint16_t pcm_right[];

/* --------------------------------------------------------------------------
 * Events
 * ----------------------------------------------------------------------- */



/* --------------------------------------------------------------------------
 * Capture control. These are the ONLY places g_app.capture changes, which is
 * what makes the state machine auditable.
 * ----------------------------------------------------------------------- */

static void Capture_Start(void)
{
    if (g_app.capture != CAPTURE_IDLE) {
        return;
    }

    g_app.flags.pending  = BUF_HALF_NONE;
    g_app.flags.overruns = 0u;
    g_app.capture        = CAPTURE_STARTING;

    if (AUDIO_REC_Start() == AUDIO_ERROR_IO) {
        g_app.capture = CAPTURE_ERROR;
        g_app.sinks  &= ~(uint32_t)SINK_SD_RECORD;
    } else {
        g_app.capture = CAPTURE_RUNNING;
    }
}

static void Capture_Stop(void)
{
    if (g_app.capture != CAPTURE_RUNNING && g_app.capture != CAPTURE_STARTING) {
        return;
    }

    g_app.capture = CAPTURE_STOPPING;
//    AUDIO_REC_Stop();                       /* provide this in waverecorder.c */
    g_app.sinks  &= ~(uint32_t)SINK_SD_RECORD;
    g_app.capture = CAPTURE_IDLE;
}

/* --------------------------------------------------------------------------
 * Sample consumption. Note both halves are tested with INDEPENDENT ifs, not
 * else-if: when the loop falls behind, both bits can be set, and the old
 * else-if chain dropped one half without any indication.
 * ----------------------------------------------------------------------- */

static void Capture_DrainHalf(uint16_t *src)
{
    const uint32_t frames = AUDIO_IN_PCM_BUFFER_SIZE / 2u;

    /* A half holds frames/2 stereo pairs, far more than pcm_left/pcm_right
     * (FFT_BLOCK_SIZE * 2 each) can take, so deinterlace in chunks that fit.
     * Each chunk overwrites the last; hook per-chunk processing (e.g. FFT)
     * in here when that pipeline comes back. */
    const uint32_t pairs = frames / 2u;
    const uint32_t chunk = FFT_BLOCK_SIZE * 2u;
    for (uint32_t done = 0; done < pairs; done += chunk) {
        uint32_t n = (pairs - done < chunk) ? (pairs - done) : chunk;
        deinterlace_stereo_pcm(&src[2u * done], pcm_left, pcm_right, n);
    }

    if (App_SinkActive(SINK_UART_RAW)) {
        AudioStream_SendRawSamples(src, frames);
    }

    if (App_SinkActive(SINK_UI_FFT) || App_SinkActive(SINK_UART_FFT)) {
        /* FFT consumes the deinterlaced left channel */
  //      AudioStream_ProcessFFT(pcm_left, frames);
    }
}

static void Capture_Drain(void)
{
    uint32_t pending;

    /* Snapshot and clear atomically -- the ISR ORs into this word. */
    __disable_irq();
    pending = g_app.flags.pending;
    g_app.flags.pending = BUF_HALF_NONE;
    __enable_irq();

    if (pending & (uint32_t)BUF_HALF_FIRST) {
        Capture_DrainHalf(&BufferCtl_In.pcm_buff[0]);
    }

    if (pending & (uint32_t)BUF_HALF_SECOND) {
        Capture_DrainHalf(&BufferCtl_In.pcm_buff[AUDIO_IN_PCM_BUFFER_SIZE / 2u]);
    }
}

/* --------------------------------------------------------------------------
 * The whole per-pass step. Flat: three sequential concerns, no nesting, no
 * duplicated storage checks.
 * ----------------------------------------------------------------------- */

void App_Process(void)
{
    /* 1. Storage transitions -------------------------------------------- */
    if (BSP_SD_IsDetected() == SD_PRESENT) {
        if (g_app.storage == STORAGE_ABSENT) {
            if (f_mount(&SDCard_FatFs, (TCHAR const *)SDCard_Path, 0) == FR_OK) {
                g_app.storage = STORAGE_READY;
            } else {
                g_app.storage = STORAGE_MOUNT_FAILED;
            }
        }
    } else if (g_app.storage != STORAGE_ABSENT) {
        /* Card gone: tear down anything that was writing to it. */
        if (g_app.capture != CAPTURE_IDLE) {
            Capture_Stop();
        }
        if (AUDIO_REC_IsActive()) {
            AUDIO_REC_RequestStop(); /* stops DMA and closes the file on the next AUDIO_REC_Process() */
        }
        BSP_AUDIO_OUT_Stop(CODEC_PDWN_SW);
        g_app.storage = STORAGE_ABSENT;
    }



}

/* --------------------------------------------------------------------------
 * BSP callbacks. These now only set bits -- no processing in interrupt
 * context, and an overrun is counted rather than silently dropped.
 * ----------------------------------------------------------------------- */

void BSP_AUDIO_IN_HalfTransfer_CallBack(void)
{
    if (g_app.flags.pending & (uint32_t)BUF_HALF_FIRST) {
        g_app.flags.overruns++;
    }
    g_app.flags.pending |= (uint32_t)BUF_HALF_FIRST;

    AUDIO_REC_HalfTransfer_Callback();
}

void BSP_AUDIO_IN_TransferComplete_CallBack(void)
{
    if (g_app.flags.pending & (uint32_t)BUF_HALF_SECOND) {
        g_app.flags.overruns++;
    }
    g_app.flags.pending |= (uint32_t)BUF_HALF_SECOND;

    AUDIO_REC_TransferComplete_Callback();
}
