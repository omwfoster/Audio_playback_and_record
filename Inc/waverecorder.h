/**
  ******************************************************************************
  * @file    Audio/Audio_playback_and_record/Inc/waverecorder.h
  * @author  MCD Application Team
  * @brief   Header for waverecorder.c module.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2017 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __WAVERECORDER_H
#define __WAVERECORDER_H

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Exported Defines ----------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
/* ADDED: fired synchronously from AUDIO_REC_Start()/AUDIO_REC_Process()
   whenever RecActive changes -- lets UI code react without polling.
   Called from whatever context calls those two functions (typically your
   main loop) -- not from an ISR. */
typedef void (*AUDIO_REC_StateCallback_t)(uint8_t active);

/* Exported constants --------------------------------------------------------*/
/* Defines for the Audio recording process */
#define DEFAULT_TIME_REC                      30  /* Recording time in second (default: 30s) */

/* ADDED: number of DMA half-buffers discarded at the start of every
 * recording before writes to the WAV file begin, to skip the MEMS mic's
 * internal AC-coupling settling transient (and any brief DFSDM filter
 * settling). At AUDIO_IN_PCM_BUFFER_SIZE=9216 half-words stereo 16kHz,
 * each half is ~144ms -- 14 half-buffers is ~2s. Tune by ear/scope: if the
 * recording still starts with an audible/visible ramp, increase this; if
 * recordings feel like they're missing too much of the start, decrease it. */
#define AUDIO_REC_WARMUP_HALFBUFFERS           14U

#define REC_WAVE_NAME "Wave.wav"

#define REC_SAMPLE_LENGTH   (DEFAULT_TIME_REC * DEFAULT_AUDIO_IN_FREQ * DEFAULT_AUDIO_IN_CHANNEL_NBR * 2)

/* Exported macro ------------------------------------------------------------*/
/* What a capture does with each half-buffer of microphone audio */
typedef enum {
  AUDIO_REC_MODE_SAVE = 0,  /* write a WAV file to the SD card (stops at DEFAULT_TIME_REC) */
  AUDIO_REC_MODE_FFT,        /* run the FFT pipeline and update the bar chart; no SD, no time limit */
  AUDIO_REC_MODE_LOG
} AUDIO_REC_Mode_t;

/* Exported functions ------------------------------------------------------- */
AUDIO_ErrorTypeDef AUDIO_REC_Process(void);
AUDIO_ErrorTypeDef AUDIO_REC_Start(void);
AUDIO_ErrorTypeDef AUDIO_PLAYER_Init(void);

/* DFSDM DMA ISR hooks — call these from the BSP audio-in DMA callbacks so the
   SD-write loop is notified when each half of the capture buffer is ready. */
void AUDIO_REC_HalfTransfer_Callback(void);
void AUDIO_REC_TransferComplete_Callback(void);

/* ADDED for LVGL record-button integration */
void    AUDIO_REC_RequestStop(void);
uint8_t AUDIO_REC_IsActive(void);
uint32_t AUDIO_REC_GetDroppedCount(void);

/* Choose save-to-SD or live FFT for the next AUDIO_REC_Start(). Ignored
   while a capture is running. */
void             AUDIO_REC_SetMode(AUDIO_REC_Mode_t mode);
AUDIO_REC_Mode_t AUDIO_REC_GetMode(void);

/* ADDED: register a callback fired on every RecActive transition (push,
   not poll). Pass NULL to unregister. Only one callback slot -- if you need
   more than one listener, have your single callback fan out to others. */
void AUDIO_REC_SetStateCallback(AUDIO_REC_StateCallback_t cb);

#endif /* __WAVERECORDER_H */
