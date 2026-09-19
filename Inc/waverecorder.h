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

#define REC_WAVE_NAME "Wave.wav"

#define REC_SAMPLE_LENGTH   (DEFAULT_TIME_REC * DEFAULT_AUDIO_IN_FREQ * DEFAULT_AUDIO_IN_CHANNEL_NBR * 2)

/* Exported macro ------------------------------------------------------------*/
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

/* ADDED: register a callback fired on every RecActive transition (push,
   not poll). Pass NULL to unregister. Only one callback slot -- if you need
   more than one listener, have your single callback fan out to others. */
void AUDIO_REC_SetStateCallback(AUDIO_REC_StateCallback_t cb);

#endif /* __WAVERECORDER_H */
