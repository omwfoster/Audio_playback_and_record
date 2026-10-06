/**
  ******************************************************************************
  * @file    Audio/Audio_playback_and_record/Src/waverecorder.c
  * @brief   MEMS microphone (DFSDM) → WAV file on microSD card.
  *
  *  Two MP34DT01 MEMS mics captured at 16 kHz / 16-bit / stereo via DFSDM
  *  with DMA ping-pong buffering.  PCM frames are written to "Wave.wav" on
  *  the microSD card using FATFS blocking writes while DMA fills the other
  *  half of the buffer.
  *
  *  Buffer timing at 16 kHz stereo 16-bit:
  *    AUDIO_IN_PCM_BUFFER_SIZE = 32768 half-words (65 536 bytes)
  *    Each half = 16384 half-words = 32768 bytes = 512 ms of audio
  *    → a half must be written to SD within 512 ms of its callback, or
  *      the DMA starts overwriting it (silent glitch, not counted as a drop).
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "waverecorder.h"
#include "waveplayer.h"
#include "main.h"
#include "console_buffer.h"
#include "audio_stream_fft.h"
#include "spectrum_view.h"
#include "spectrum_tab.h"

/* Private defines -----------------------------------------------------------*/
/* Touch zones for the record screen — bottom 10% (48px) button bar */
#define TOUCH_RECORD_XMIN   300
#define TOUCH_RECORD_XMAX   340
#define TOUCH_RECORD_YMIN   432
#define TOUCH_RECORD_YMAX   480

#define TOUCH_STOP_XMIN     205
#define TOUCH_STOP_XMAX     245
#define TOUCH_STOP_YMIN     432
#define TOUCH_STOP_YMAX     480

#define TOUCH_PAUSE_XMIN    125
#define TOUCH_PAUSE_XMAX    149
#define TOUCH_PAUSE_YMIN    432
#define TOUCH_PAUSE_YMAX    480

/* WAV RIFF constants (little-endian 32-bit tags) */
#define RIFF_TAG    0x46464952U  /* "RIFF" */
#define WAVE_TAG    0x45564157U  /* "WAVE" */
#define FMT_TAG     0x20746D66U  /* "fmt " */
#define DATA_TAG    0x61746164U  /* "data" */

/* Scratch buffer for BSP_AUDIO_IN internal use */
#define SCRATCH_BUFF_SIZE  512

/* Private variables ---------------------------------------------------------*/
static int32_t Scratch[SCRATCH_BUFF_SIZE];

/* DMA ping-pong PCM buffer — must be in a non-cached or cache-coherent region.
   Placed in the DMA-safe SRAM section defined in the linker script.          */
__attribute__((section(".dma_buffers")))
AUDIO_IN_BufferTypeDef BufferCtl_In;

/* File handle for the WAV being recorded */
static FIL WavRecFile;

/* DMA half/full transfer flags set from ISR context */
volatile uint32_t DmaRecHalfBuffCplt;
volatile uint32_t DmaRecBuffCplt;

/* ADDED: incremented whenever a DMA half/full-complete callback fires while
 * the previous one of the same kind hasn't been consumed yet by
 * AUDIO_REC_Process() -- i.e. AUDIO_REC_Process() isn't being called often
 * enough to keep up with the DMA, and a half-buffer of audio was silently
 * overwritten before it got written to the WAV file. */
volatile uint32_t DmaRecPacketDropped;

/* Running byte count of PCM data written (excludes 44-byte header) */
static volatile uint32_t RecBytesWritten;

/* Recording active / paused flags */
static volatile uint8_t  RecPaused;

/* ADDED: external stop request (e.g. from an LVGL button handler running
 * outside this file's own touch-zone logic). Checked each AUDIO_REC_Process()
 * iteration alongside the existing time-limit check. */
static volatile uint8_t  RecStopRequested;
static uint32_t          RecMaxWriteMs; /* longest single f_write() this recording, for drop diagnosis */
static volatile uint8_t  DmaRecError; /* set by BSP_AUDIO_IN_Error_CallBack(), logged by AUDIO_REC_Process() */

/* ADDED: true from a successful AUDIO_REC_Start() until AUDIO_REC_Process()
 * finalises and closes the file (EOF or IO error). Lets external UI code
 * (e.g. a record button) query real state instead of tracking its own
 * separate flag that could drift out of sync if the recording ends on its
 * own (time limit) without the UI having been told to stop it. */
static volatile uint8_t  RecActive;

/* Save to SD or run the FFT; latched at AUDIO_REC_Start() */
static AUDIO_REC_Mode_t  RecMode = AUDIO_REC_MODE_SAVE;

/* ADDED: registered via AUDIO_REC_SetStateCallback(), fired on every
 * RecActive transition so UI code can react without polling. */
static AUDIO_REC_StateCallback_t RecStateCallback = NULL;

/* Private function prototypes -----------------------------------------------*/
static void     WriteWavHeader(FIL *fp, uint32_t sample_rate,
                               uint16_t channels, uint16_t bits,
                               uint32_t data_bytes);
static void     FinaliseWavHeader(FIL *fp, uint32_t data_bytes);
static void     AUDIO_REC_DisplayStatus(uint32_t elapsed_ms);
static void     ProcessFftStream(void);


/* Public functions ----------------------------------------------------------*/

volatile uint32_t DmaRecCallbackCount; /* total half+full callbacks, for rate diagnosis */

void AUDIO_REC_HalfTransfer_Callback(void)
{
  DmaRecCallbackCount++;
  if (DmaRecHalfBuffCplt != 0)
  {
    DmaRecPacketDropped++;
  }
  DmaRecHalfBuffCplt = 1;
}

void AUDIO_REC_TransferComplete_Callback(void)
{
  DmaRecCallbackCount++;
  if (DmaRecBuffCplt != 0)
  {
    DmaRecPacketDropped++;
  }
  DmaRecBuffCplt = 1;
}

/**
  * @brief  Start MEMS microphone capture and open the WAV file on SD.
  * @retval AUDIO_ERROR_NONE on success, AUDIO_ERROR_IO on failure.
  */
AUDIO_ErrorTypeDef AUDIO_REC_Start(void)
{
  /* Reset state */
  DmaRecHalfBuffCplt = 0;
  DmaRecBuffCplt     = 0;
  RecBytesWritten    = 0;
  RecPaused          = 0;
  RecStopRequested   = 0; /* ADDED */
  DmaRecPacketDropped = 0; /* ADDED */
  RecMaxWriteMs      = 0;
  BufferCtl_In.pcm_ptr  = 0;
  BufferCtl_In.wr_state = BUFFER_EMPTY;
  BufferCtl_In.offset   = 0;
  BufferCtl_In.fptr     = 0;

  if (RecMode == AUDIO_REC_MODE_SAVE)
  {
    /* Create (or overwrite) the WAV file on the SD card root */
    FRESULT fr = f_open(&WavRecFile, REC_WAVE_NAME,
                        FA_CREATE_ALWAYS | FA_WRITE);
    if (fr != FR_OK)
    {
      ui_console_log("Cannot create %s (f_open=%d)\n", REC_WAVE_NAME, (int)fr);
      return AUDIO_ERROR_IO;
    }

    /* Write a placeholder WAV header — sizes filled in when recording stops */
    WriteWavHeader(&WavRecFile,
                   DEFAULT_AUDIO_IN_FREQ,
                   DEFAULT_AUDIO_IN_CHANNEL_NBR,
                   DEFAULT_AUDIO_IN_BIT_RESOLUTION,
                   0 /* data size unknown yet */);
  }

  /* Initialise BSP audio input (DFSDM, 16 kHz, 16-bit, stereo) */
  if (BSP_AUDIO_IN_Init(DEFAULT_AUDIO_IN_FREQ,
                        DEFAULT_AUDIO_IN_BIT_RESOLUTION,
                        DEFAULT_AUDIO_IN_CHANNEL_NBR) != AUDIO_OK)
  {
	  ui_console_log("BSP_AUDIO_IN_Init failed!\n");
    if (RecMode == AUDIO_REC_MODE_SAVE)
    {
      f_close(&WavRecFile);
    }
    return AUDIO_ERROR_IO;
  }

  BSP_AUDIO_IN_AllocScratch(Scratch, SCRATCH_BUFF_SIZE);

  /* Start DMA capture into the ping-pong buffer */
  BSP_AUDIO_IN_Record((uint16_t *)BufferCtl_In.pcm_buff,
                      AUDIO_IN_PCM_BUFFER_SIZE);

  if (RecMode == AUDIO_REC_MODE_FFT)
  {
    /* Stream from wherever the DFSDM interrupt starts writing */
    FFT_Stream_Reset(BSP_AUDIO_IN_GetWritePos());
  }

  /* Draw record-screen UI */

  if (RecMode == AUDIO_REC_MODE_SAVE)
  {
    ui_console_log("\nRecording to %s ...\n", REC_WAVE_NAME);
  }
  else
  {
    ui_console_log("\nFFT running ...\n");
  }

  /* Switch application state so the main loop calls AUDIO_REC_Process() */
  AudioState = AUDIO_STATE_RECORD;

  RecActive = 1; /* ADDED */

  if (RecStateCallback != NULL) /* ADDED */
  {
    RecStateCallback(1);
  }

  return AUDIO_ERROR_NONE;
}

/**
  * @brief  ADDED: Register a callback fired on every RecActive transition.
  *         Pass NULL to unregister.
  */
void AUDIO_REC_SetStateCallback(AUDIO_REC_StateCallback_t cb)
{
  RecStateCallback = cb;
}

/**
  * @brief  ADDED: Request that the active recording stop at the next
  *         AUDIO_REC_Process() call. Safe to call from another task/ISR
  *         context (single-byte flag write); the actual file finalisation
  *         and close still happen inside AUDIO_REC_Process() on the main
  *         loop, not synchronously from this call.
  */
void AUDIO_REC_RequestStop(void)
{
  RecStopRequested = 1;
}

/**
  * @brief  Choose what the next capture does: save a WAV to SD, or run the
  *         FFT and update the bar chart. Ignored while a capture is active,
  *         so a running capture can't switch halfway through.
  */
void AUDIO_REC_SetMode(AUDIO_REC_Mode_t mode)
{
  if (!RecActive)
  {
    RecMode = mode;
  }
}

AUDIO_REC_Mode_t AUDIO_REC_GetMode(void)
{
  return RecMode;
}

/**
  * @brief  ADDED: Query whether a recording is currently active (from
  *         successful AUDIO_REC_Start() until AUDIO_REC_Process() finalises
  *         and closes the file). Use this instead of tracking a separate
  *         flag in UI code, so a time-limit-triggered stop is reflected
  *         correctly even if nothing called AUDIO_REC_RequestStop().
  */
uint8_t AUDIO_REC_IsActive(void)
{
  return RecActive;
}

/**
  * @brief  ADDED: Number of DMA half/full-transfer callbacks that fired
  *         before AUDIO_REC_Process() consumed the previous one -- i.e. lost
  *         half-buffers of audio due to processing falling behind the DMA.
  *         Non-zero after a recording means the resulting WAV has gaps.
  */
uint32_t AUDIO_REC_GetDroppedCount(void)
{
  return DmaRecPacketDropped;
}

/**
  * @brief  Process audio recording — called from the main loop.
  *
  *         Writes whichever half of the DMA buffer has been filled to the WAV
  *         file.  Checks the touch screen for Stop / Pause.  Returns
  *         AUDIO_ERROR_EOF when the recording session ends (stop pressed or
  *         time limit reached).
  *
  * @retval AUDIO_ERROR_NONE  keep recording
  *         AUDIO_ERROR_EOF   recording finished (WAV file is valid and closed)
  *         AUDIO_ERROR_IO    file I/O error
  */
AUDIO_ErrorTypeDef AUDIO_REC_Process(void)
{
  static uint32_t     t_start_ms = 0;

  uint32_t            bw;
  FRESULT             res;
  AUDIO_ErrorTypeDef  ret = AUDIO_ERROR_NONE;

  /* Capture start timestamp on first call */
  if (AudioState == AUDIO_STATE_RECORD)
  {
    t_start_ms = HAL_GetTick();
    AudioState  = AUDIO_STATE_PLAY; /* reuse PLAY state as "running" sentinel */
  }



  /* ---------- Time limit ---------- */
  uint32_t elapsed_ms = HAL_GetTick() - t_start_ms;

  /* Logged here, not in the error ISR -- ui_console_log() isn't interrupt-safe. */
  if (DmaRecError)
  {
    DmaRecError = 0;
    ui_console_log("DFSDM DMA error!\n");
  }

  /* End the recording on an external stop request or on hitting the time limit. */
  if (RecStopRequested ||
      (RecMode == AUDIO_REC_MODE_SAVE && elapsed_ms >= (uint32_t)DEFAULT_TIME_REC * 1000U))
  {
    ret = AUDIO_ERROR_EOF;
    goto done;
  }

  /* FFT mode doesn't wait for half/full callbacks: it processes each hop
   * as soon as the DFSDM interrupt has written it (ProcessFftStream()), and
   * skips ahead itself if it falls behind. Clear the flags so they don't
   * count as dropped buffers. */
  if (RecMode == AUDIO_REC_MODE_FFT)
  {
    DmaRecHalfBuffCplt = 0;
    DmaRecBuffCplt     = 0;
    ProcessFftStream();
    return AUDIO_ERROR_NONE;
  }

  /* ---------- Write first half when DMA half-transfer fires ---------- */
  if (DmaRecHalfBuffCplt == 1)
  {
    DmaRecHalfBuffCplt = 0;
    /* Invalidate D-Cache for the first half before reading */
    SCB_CleanDCache_by_Addr(
        (uint32_t *)BufferCtl_In.pcm_buff,
        AUDIO_IN_PCM_BUFFER_SIZE / 2 * sizeof(uint16_t));

    uint32_t t_wr = HAL_GetTick();
    res = f_write(&WavRecFile,
                  (uint8_t *)BufferCtl_In.pcm_buff,
                  AUDIO_IN_PCM_BUFFER_SIZE / 2 * sizeof(uint16_t),
                  (UINT *)&bw);
    t_wr = HAL_GetTick() - t_wr;
    if (t_wr > RecMaxWriteMs) RecMaxWriteMs = t_wr;
    if (res != FR_OK)
    {
      ui_console_log("SD write error %d\n", res);
      ret = AUDIO_ERROR_IO;
      goto done;
    }
    AUDIO_REC_DisplayStatus(elapsed_ms);
    RecBytesWritten += bw;
  }

  /* ---------- Write second half when DMA full-transfer fires ---------- */
  if (DmaRecBuffCplt == 1)
  {
    DmaRecBuffCplt = 0;
    SCB_CleanDCache_by_Addr(
        (uint32_t *)(BufferCtl_In.pcm_buff + AUDIO_IN_PCM_BUFFER_SIZE / 2),
        AUDIO_IN_PCM_BUFFER_SIZE / 2 * sizeof(uint16_t));

    uint32_t t_wr = HAL_GetTick();
    res = f_write(&WavRecFile,
                  (uint8_t *)(BufferCtl_In.pcm_buff + AUDIO_IN_PCM_BUFFER_SIZE / 2),
                  AUDIO_IN_PCM_BUFFER_SIZE / 2 * sizeof(uint16_t),
                  (UINT *)&bw);
    t_wr = HAL_GetTick() - t_wr;
    if (t_wr > RecMaxWriteMs) RecMaxWriteMs = t_wr;
    if (res != FR_OK)
    {
      ui_console_log("SD write error %d\n", res);
      ret = AUDIO_ERROR_IO;
      goto done;
    }
    AUDIO_REC_DisplayStatus(elapsed_ms);
    RecBytesWritten += bw;
  }

  return AUDIO_ERROR_NONE;

done:
  /* Stop DFSDM DMA */
  BSP_AUDIO_IN_Stop();

  if (RecMode == AUDIO_REC_MODE_SAVE)
  {
    /* Update WAV header with the actual data size, then close */
    FinaliseWavHeader(&WavRecFile, RecBytesWritten);
    f_close(&WavRecFile);

    if (ret == AUDIO_ERROR_EOF)
      ui_console_log("Recording stopped. %lu bytes written.\n", RecBytesWritten);
    ui_console_log("max f_write: %lu ms", (unsigned long)RecMaxWriteMs);
  }
  else
  {
    ui_console_log("FFT stopped. max FFT block: %lu ms", (unsigned long)RecMaxWriteMs);
    spectrum_view_clear();
  }

  AudioState = AUDIO_STATE_IDLE;
  RecActive = 0; /* ADDED */

  if (RecStateCallback != NULL) /* ADDED */
  {
    RecStateCallback(0);
  }

  return ret;
}

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  FFT mode: run an FFT for every hop the DFSDM interrupt has
  *         written since the last call, and update the bar chart every
  *         FFT_HOPS_PER_REFRESH hops (about every FFT_REFRESH_MS). Runs on
  *         the main loop, so calling into LVGL here is safe. The longest
  *         call is tracked in RecMaxWriteMs, the slot SAVE mode uses for
  *         f_write().
  */
static void ProcessFftStream(void)
{
  uint32_t t0 = HAL_GetTick();
  if (FFT_Stream_Process((const int16_t *)BufferCtl_In.pcm_buff,
                         AUDIO_IN_PCM_BUFFER_SIZE,
                         BSP_AUDIO_IN_GetWritePos()))
  {
    spectrum_view_update(Clean_Display_Bars, DISPLAY_BINS);
    spectrogram_push_bars(Clean_Display_Bars, DISPLAY_BINS);
  }
  uint32_t dt = HAL_GetTick() - t0;
  if (dt > RecMaxWriteMs) RecMaxWriteMs = dt;
}

/**
  * @brief  Write a standard 44-byte PCM WAV header to the start of fp.
  */
static void WriteWavHeader(FIL *fp, uint32_t sample_rate,
                           uint16_t channels, uint16_t bits,
                           uint32_t data_bytes)
{
  WAVE_FormatTypeDef hdr;
  UINT bw;

  hdr.ChunkID       = RIFF_TAG;
  hdr.FileSize      = 36U + data_bytes;
  hdr.FileFormat    = WAVE_TAG;
  hdr.SubChunk1ID   = FMT_TAG;
  hdr.SubChunk1Size = 16U;
  hdr.AudioFormat   = 1U;  /* PCM */
  hdr.NbrChannels   = channels;
  hdr.SampleRate    = sample_rate;
  hdr.ByteRate      = sample_rate * channels * (bits / 8U);
  hdr.BlockAlign    = (uint16_t)(channels * (bits / 8U));
  hdr.BitPerSample  = bits;
  hdr.SubChunk2ID   = DATA_TAG;
  hdr.SubChunk2Size = data_bytes;

  f_write(fp, &hdr, sizeof(hdr), &bw);
}

/**
  * @brief  Seek to offset 0 and patch the WAV header with the real data size.
  */
static void FinaliseWavHeader(FIL *fp, uint32_t data_bytes)
{
  UINT bw;
  uint32_t val;

  /* SubChunk2Size at byte 40 */
  f_lseek(fp, 40U);
  val = data_bytes;
  f_write(fp, &val, 4U, &bw);

  /* FileSize at byte 4 */
  f_lseek(fp, 4U);
  val = 36U + data_bytes;
  f_write(fp, &val, 4U, &bw);

  f_lseek(fp, f_size(fp));  /* restore pointer to EOF */
}


/**
  * @brief  Update the elapsed-time display (called every process loop).
  */
static void AUDIO_REC_DisplayStatus(uint32_t elapsed_ms)
{
  static uint32_t last_sec = 0xFFFFFFFFU;
  uint32_t sec = elapsed_ms / 1000U;

  if (sec != last_sec)
  {
    last_sec = sec;
    uint8_t str[48];
    uint32_t kb = RecBytesWritten / 1024U;
    ui_console_log("REC  %02lu:%02lu  |  %lu KB written",
            sec / 60U, sec % 60U, kb);

  }
}




/**
  * @brief  DFSDM error callback.
  */
void BSP_AUDIO_IN_Error_CallBack(void)
{
  DmaRecError = 1;
}
