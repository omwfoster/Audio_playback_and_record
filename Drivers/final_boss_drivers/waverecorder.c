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
  *    AUDIO_IN_PCM_BUFFER_SIZE = 9216 half-words (18 432 bytes)
  *    Each half = 4608 half-words = 9216 bytes = ~144 ms of audio
  *    → comfortable margin for SD write latency.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "waverecorder.h"
#include "waveplayer.h"
#include "main.h"

/* Private defines -----------------------------------------------------------*/
/* Touch zones for the record screen */
#define TOUCH_RECORD_XMIN   300
#define TOUCH_RECORD_XMAX   340
#define TOUCH_RECORD_YMIN   212
#define TOUCH_RECORD_YMAX   252

#define TOUCH_STOP_XMIN     205
#define TOUCH_STOP_XMAX     245
#define TOUCH_STOP_YMIN     212
#define TOUCH_STOP_YMAX     252

#define TOUCH_PAUSE_XMIN    125
#define TOUCH_PAUSE_XMAX    149
#define TOUCH_PAUSE_YMIN    212
#define TOUCH_PAUSE_YMAX    252

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
static volatile uint32_t DmaRecHalfBuffCplt;
static volatile uint32_t DmaRecBuffCplt;

/* Running byte count of PCM data written (excludes 44-byte header) */
static volatile uint32_t RecBytesWritten;

/* Recording active / paused flags */
static volatile uint8_t  RecPaused;

/* Private function prototypes -----------------------------------------------*/
static void     WriteWavHeader(FIL *fp, uint32_t sample_rate,
                               uint16_t channels, uint16_t bits,
                               uint32_t data_bytes);
static void     FinaliseWavHeader(FIL *fp, uint32_t data_bytes);
static void     AUDIO_REC_DisplayButtons(void);
static void     AUDIO_REC_DisplayStatus(uint32_t elapsed_ms);
static uint8_t  TouchInRegion(uint16_t x, uint16_t y,
                              uint16_t xmin, uint16_t xmax,
                              uint16_t ymin, uint16_t ymax);

/* Public functions ----------------------------------------------------------*/

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
  BufferCtl_In.pcm_ptr  = 0;
  BufferCtl_In.wr_state = BUFFER_EMPTY;
  BufferCtl_In.offset   = 0;
  BufferCtl_In.fptr     = 0;

  /* Create (or overwrite) the WAV file on the SD card root */
  if (f_open(&WavRecFile, REC_WAVE_NAME,
             FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
  {
    LCD_ErrLog("Cannot create %s on SD card!\n", REC_WAVE_NAME);
    return AUDIO_ERROR_IO;
  }

  /* Write a placeholder WAV header — sizes filled in when recording stops */
  WriteWavHeader(&WavRecFile,
                 DEFAULT_AUDIO_IN_FREQ,
                 DEFAULT_AUDIO_IN_CHANNEL_NBR,
                 DEFAULT_AUDIO_IN_BIT_RESOLUTION,
                 0 /* data size unknown yet */);

  /* Initialise BSP audio input (DFSDM, 16 kHz, 16-bit, stereo) */
  if (BSP_AUDIO_IN_Init(DEFAULT_AUDIO_IN_FREQ,
                        DEFAULT_AUDIO_IN_BIT_RESOLUTION,
                        DEFAULT_AUDIO_IN_CHANNEL_NBR) != AUDIO_OK)
  {
    LCD_ErrLog("BSP_AUDIO_IN_Init failed!\n");
    f_close(&WavRecFile);
    return AUDIO_ERROR_IO;
  }

  BSP_AUDIO_IN_AllocScratch(Scratch, SCRATCH_BUFF_SIZE);

  /* Start DMA capture into the ping-pong buffer */
  BSP_AUDIO_IN_Record((uint16_t *)BufferCtl_In.pcm_buff,
                      AUDIO_IN_PCM_BUFFER_SIZE);

  /* Draw record-screen UI */
  AUDIO_REC_DisplayButtons();
  LCD_UsrLog("\nRecording to %s ...\n", REC_WAVE_NAME);

  /* Switch application state so the main loop calls AUDIO_REC_Process() */
  AudioState = AUDIO_STATE_RECORD;

  return AUDIO_ERROR_NONE;
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
  TS_StateTypeDef     ts;
  uint32_t            bw;
  FRESULT             res;
  AUDIO_ErrorTypeDef  ret = AUDIO_ERROR_NONE;

  /* Capture start timestamp on first call */
  if (AudioState == AUDIO_STATE_RECORD)
  {
    t_start_ms = HAL_GetTick();
    AudioState  = AUDIO_STATE_PLAY; /* reuse PLAY state as "running" sentinel */
  }

  /* ---------- Touch-screen controls ---------- */
  BSP_TS_GetState(&ts);
  if (ts.touchDetected == 1)
  {
    uint16_t x = ts.touchX[0];
    uint16_t y = ts.touchY[0];

    if (TouchInRegion(x, y, TOUCH_STOP_XMIN, TOUCH_STOP_XMAX,
                             TOUCH_STOP_YMIN, TOUCH_STOP_YMAX))
    {
      /* User pressed Stop */
      ret = AUDIO_ERROR_EOF;
      goto done;
    }

    if (TouchInRegion(x, y, TOUCH_PAUSE_XMIN, TOUCH_PAUSE_XMAX,
                             TOUCH_PAUSE_YMIN, TOUCH_PAUSE_YMAX))
    {
      RecPaused ^= 1U;
      if (RecPaused)
        BSP_AUDIO_IN_Pause();
      else
        BSP_AUDIO_IN_Resume();

      /* Debounce: wait for finger lift */
      do { BSP_TS_GetState(&ts); } while (ts.touchDetected > 0);
    }
  }

  /* ---------- Time limit ---------- */
  uint32_t elapsed_ms = HAL_GetTick() - t_start_ms;
  AUDIO_REC_DisplayStatus(elapsed_ms);

  if (elapsed_ms >= (uint32_t)DEFAULT_TIME_REC * 1000U)
  {
    ret = AUDIO_ERROR_EOF;
    goto done;
  }

  if (RecPaused)
    return AUDIO_ERROR_NONE;

  /* ---------- Write first half when DMA half-transfer fires ---------- */
  if (DmaRecHalfBuffCplt == 1)
  {
    DmaRecHalfBuffCplt = 0;
    /* Invalidate D-Cache for the first half before reading */
    SCB_InvalidateDCache_by_Addr(
        (uint32_t *)BufferCtl_In.pcm_buff,
        AUDIO_IN_PCM_BUFFER_SIZE / 2 * sizeof(uint16_t));

    res = f_write(&WavRecFile,
                  (uint8_t *)BufferCtl_In.pcm_buff,
                  AUDIO_IN_PCM_BUFFER_SIZE / 2 * sizeof(uint16_t),
                  (UINT *)&bw);
    if (res != FR_OK)
    {
      LCD_ErrLog("SD write error %d\n", res);
      ret = AUDIO_ERROR_IO;
      goto done;
    }
    RecBytesWritten += bw;
  }

  /* ---------- Write second half when DMA full-transfer fires ---------- */
  if (DmaRecBuffCplt == 1)
  {
    DmaRecBuffCplt = 0;
    SCB_InvalidateDCache_by_Addr(
        (uint32_t *)(BufferCtl_In.pcm_buff + AUDIO_IN_PCM_BUFFER_SIZE / 2),
        AUDIO_IN_PCM_BUFFER_SIZE / 2 * sizeof(uint16_t));

    res = f_write(&WavRecFile,
                  (uint8_t *)(BufferCtl_In.pcm_buff + AUDIO_IN_PCM_BUFFER_SIZE / 2),
                  AUDIO_IN_PCM_BUFFER_SIZE / 2 * sizeof(uint16_t),
                  (UINT *)&bw);
    if (res != FR_OK)
    {
      LCD_ErrLog("SD write error %d\n", res);
      ret = AUDIO_ERROR_IO;
      goto done;
    }
    RecBytesWritten += bw;
  }

  return AUDIO_ERROR_NONE;

done:
  /* Stop DFSDM DMA */
  BSP_AUDIO_IN_Stop();

  /* Update WAV header with the actual data size, then close */
  FinaliseWavHeader(&WavRecFile, RecBytesWritten);
  f_close(&WavRecFile);

  if (ret == AUDIO_ERROR_EOF)
    LCD_UsrLog("Recording stopped. %lu bytes written.\n", RecBytesWritten);

  AudioState = AUDIO_STATE_IDLE;
  return ret;
}

/* Private functions ---------------------------------------------------------*/

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
  * @brief  Draw the record-screen button outlines on the LCD.
  */
static void AUDIO_REC_DisplayButtons(void)
{
  BSP_LCD_SetFont(&LCD_LOG_HEADER_FONT);
  BSP_LCD_ClearStringLine(13);
  BSP_LCD_ClearStringLine(14);
  BSP_LCD_ClearStringLine(15);
  BSP_LCD_SetTextColor(LCD_COLOR_RED);

  /* Stop button — solid square */
  BSP_LCD_FillRect(TOUCH_STOP_XMIN, TOUCH_STOP_YMIN,
                   TOUCH_STOP_XMAX  - TOUCH_STOP_XMIN,
                   TOUCH_STOP_YMAX  - TOUCH_STOP_YMIN);

  /* Pause button — two vertical bars */
  BSP_LCD_SetTextColor(LCD_COLOR_CYAN);
  BSP_LCD_FillRect(TOUCH_PAUSE_XMIN,      TOUCH_PAUSE_YMIN,
                   8, TOUCH_PAUSE_YMAX - TOUCH_PAUSE_YMIN);
  BSP_LCD_FillRect(TOUCH_PAUSE_XMIN + 12, TOUCH_PAUSE_YMIN,
                   8, TOUCH_PAUSE_YMAX - TOUCH_PAUSE_YMIN);

  BSP_LCD_SetTextColor(LCD_COLOR_GREEN);
  BSP_LCD_SetFont(&LCD_LOG_TEXT_FONT);
  BSP_LCD_DisplayStringAtLine(15, (uint8_t *)"[PAUSE]  [STOP]  — touch to control");
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
    sprintf((char *)str, "REC  %02lu:%02lu  |  %lu KB written",
            sec / 60U, sec % 60U, kb);
    BSP_LCD_SetFont(&LCD_LOG_TEXT_FONT);
    BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
    BSP_LCD_ClearStringLine(14);
    BSP_LCD_DisplayStringAtLine(14, str);
  }
}

/**
  * @brief  Return 1 if (x,y) falls inside the given rectangular region.
  */
static uint8_t TouchInRegion(uint16_t x, uint16_t y,
                              uint16_t xmin, uint16_t xmax,
                              uint16_t ymin, uint16_t ymax)
{
  return (x >= xmin && x <= xmax && y >= ymin && y <= ymax) ? 1U : 0U;
}

/* DMA callbacks -------------------------------------------------------------*/



/**
  * @brief  DFSDM error callback.
  */
void BSP_AUDIO_IN_Error_CallBack(void)
{
  LCD_ErrLog("DFSDM DMA error!\n");
}
