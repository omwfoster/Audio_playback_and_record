/**
  ******************************************************************************
  * @file    stm32f769i_eval_audio.h
  * @author  MCD Application Team
  * @brief   This file contains the common defines and functions prototypes for
  *          the stm32f769i_eval_audio.c driver.
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
#ifndef __STM32F769I_EVAL_AUDIO_H
#define __STM32F769I_EVAL_AUDIO_H

#ifdef __cplusplus
 extern "C" {
#endif 

/* Includes ------------------------------------------------------------------*/
/* Include audio component Driver */


#if defined(USE_LCD_HDMI)
/* Include ADV7533 HDMI Driver IC driver code */
#include "../Components/adv7533/adv7533.h"
#endif /* USE_LCD_HDMI */
#include "stm32f769i_eval.h"
#include <stdlib.h>

/** @addtogroup BSP
  * @{
  */ 

/** @addtogroup STM32F769I_EVAL
  * @{
  */
    
/** @addtogroup STM32F769I_EVAL_AUDIO 
  * @{
  */

/** @defgroup STM32F769I_EVAL_AUDIO_Exported_Types STM32F769I_EVAL_AUDIO Exported Types
  * @{
  */
/**
  * @}
  */ 

/** @defgroup STM32F769I_EVAL_AUDIO_Exported_Constants STM32F769I_EVAL_AUDIO Exported Constants
  * @{
  */



/** @defgroup BSP_Audio_Sample_Rate BSP Audio Sample Rate
  * @{
  */
#define AUDIO_FREQUENCY_96K         ((uint32_t)96000U)
#define AUDIO_FREQUENCY_48K         ((uint32_t)48000U)
#define AUDIO_FREQUENCY_44K         ((uint32_t)44100U)
#define AUDIO_FREQUENCY_32K         ((uint32_t)44100U)
#define AUDIO_FREQUENCY_22K         ((uint32_t)22050U)
#define AUDIO_FREQUENCY_16K         ((uint32_t)16000U)
#define AUDIO_FREQUENCY_11K         ((uint32_t)11025U)
#define AUDIO_FREQUENCY_8K          ((uint32_t)8000U)
/**
  * @}
  */
    


/*------------------------------------------------------------------------------
                        AUDIO IN CONFIGURATION
------------------------------------------------------------------------------*/
/* DFSDM Configuration defines */
#define AUDIO_DFSDMx_LEFT_CHANNEL                       DFSDM_CHANNEL_1
#define AUDIO_DFSDMx_RIGHT_CHANNEL                      DFSDM_CHANNEL_0
#define AUDIO_DFSDMx_LEFT_FILTER                        DFSDM1_Filter0
#define AUDIO_DFSDMx_RIGHT_FILTER                       DFSDM1_Filter1

#define AUDIO_DFSDMx_CLK_ENABLE()                       __HAL_RCC_DFSDM1_CLK_ENABLE()

#define AUDIO_DFSDMx_CKOUT_PIN          GPIO_PIN_3
#define AUDIO_DFSDMx_DMIC_DATIN_PIN     GPIO_PIN_6
#define AUDIO_DFSDMx_CKOUT_DMIC_DATIN_GPIO_PORT GPIOD
#define AUDIO_DFSDMx_DMIC_DATIN_AF      GPIO_AF3_DFSDM1
#define AUDIO_DFSDMx_CKOUT_AF           GPIO_AF3_DFSDM1
#define AUDIO_DFSDMx_CKOUT_DMIC_DATIN_GPIO_CLK_ENABLE() __HAL_RCC_GPIOD_CLK_ENABLE()
#define AUDIO_DFSDMx_DMIC_DATIN_AF      GPIO_AF3_DFSDM1
#define AUDIO_DFSDMx_CKOUT_AF           GPIO_AF3_DFSDM1

    
/* DFSDM DMA Right and Left channels definitions */
#define AUDIO_DFSDMx_DMAx_CLK_ENABLE()                  __HAL_RCC_DMA2_CLK_ENABLE()
#define AUDIO_DFSDMx_DMAx_LEFT_CHANNEL                  DMA_CHANNEL_8
#define AUDIO_DFSDMx_DMAx_RIGHT_CHANNEL                 DMA_CHANNEL_8
#define AUDIO_DFSDMx_DMAx_LEFT_IRQ                      DMA2_Stream0_IRQn
#define AUDIO_DFSDMx_DMAx_RIGHT_IRQ                     DMA2_Stream5_IRQn
#define AUDIO_DFSDMx_DMAx_PERIPH_DATA_SIZE              DMA_PDATAALIGN_WORD
#define AUDIO_DFSDMx_DMAx_MEM_DATA_SIZE                 DMA_MDATAALIGN_WORD
   
#define AUDIO_DFSDM_DMAx_LEFT_IRQHandler                DMA2_Stream0_IRQHandler
#define AUDIO_DFSDM_DMAx_RIGHT_IRQHandler               DMA2_Stream5_IRQHandler
  
/* Select the interrupt preemption priority and subpriority for the IT/DMA interrupt */
#define AUDIO_IN_IRQ_PREPRIO                ((uint32_t)0x0F)


/*------------------------------------------------------------------------------
             CONFIGURATION: Audio Driver Configuration parameters
------------------------------------------------------------------------------*/

#define AUDIODATA_SIZE                      2   /* 16-bits audio data size */

/* Audio status definition */     
#define AUDIO_OK                            ((uint8_t)0)
#define AUDIO_ERROR                         ((uint8_t)1)
#define AUDIO_TIMEOUT                       ((uint8_t)2)

/* AudioFreq * DataSize (2 bytes) * NumChannels (Stereo: 2) */
#define DEFAULT_AUDIO_IN_FREQ               I2S_AUDIOFREQ_16K
#define DEFAULT_AUDIO_IN_BIT_RESOLUTION     ((uint8_t)16)
#define DEFAULT_AUDIO_IN_CHANNEL_NBR        ((uint8_t)2) /* Mono = 1, Stereo = 2 */
#define DEFAULT_AUDIO_IN_VOLUME             ((uint16_t)64)



   
/*------------------------------------------------------------------------------
                    OPTIONAL Configuration defines parameters
------------------------------------------------------------------------------*/

/* Delay for the Codec to be correctly reset */
#define CODEC_RESET_DELAY                   ((uint8_t)5)
   

/*------------------------------------------------------------------------------
                            OUTPUT DEVICES definition
------------------------------------------------------------------------------*/
/* Alias on existing output devices to adapt for 2 headphones output */

/**
  * @}
  */
 
/** @defgroup STM32F769I_EVAL_AUDIO_Exported_Variables STM32F769I_EVAL_AUDIO Exported Variables
  * @{
  */
 /**
  * @}
  */
   
/** @defgroup STM32F769I_EVAL_AUDIO_Exported_Macros STM32F769I_EVAL_AUDIO Exported Macros
  * @{
  */
#define DMA_MAX(x)           (((x) <= DMA_MAX_SZE)? (x):DMA_MAX_SZE)
/**
  * @}
  */ 

/** @defgroup STM32F769I_EVAL_AUDIO_OUT_Exported_Functions STM32F769I_EVAL_AUDIO_OUT Exported Functions
  * @{
  */


/**
  * @}
  */ 

/** @defgroup STM32F769I_EVAL_AUDIO_IN_Exported_Functions STM32F769I_EVAL_AUDIO_IN Exported Functions
  * @{
  */
uint8_t BSP_AUDIO_IN_Init(uint32_t AudioFreq, uint32_t BitRes, uint32_t ChnlNbr);
uint8_t BSP_AUDIO_IN_AllocScratch (int32_t *pScratch, uint32_t size);
void    BSP_AUDIO_IN_DeInit(void);
uint8_t BSP_AUDIO_IN_Record(uint16_t *pData, uint32_t Size);
uint8_t BSP_AUDIO_IN_Stop(void);
uint8_t BSP_AUDIO_IN_Pause(void);
uint8_t BSP_AUDIO_IN_Resume(void);

/* User Callbacks: user has to implement these functions in his code if they are needed. */
/* This function should be implemented by the user application.
   It is called into this driver when the current buffer is filled to prepare the next
   buffer pointer and its size. */
void    BSP_AUDIO_IN_TransferComplete_CallBack(void);
void    BSP_AUDIO_IN_HalfTransfer_CallBack(void);

/* This function is called when an Interrupt due to transfer error on or peripheral
   error occurs. */
void    BSP_AUDIO_IN_Error_Callback(void);
     
/* These function can be modified in case the current settings (e.g. DMA stream)
   need to be changed for specific application needs */
void BSP_AUDIO_IN_ClockConfig(DFSDM_Filter_HandleTypeDef *hdfsdm_filter, uint32_t AudioFreq, void *Params);
void BSP_AUDIO_IN_MspInit(void);
void BSP_AUDIO_IN_MspDeInit(void);
/**
  * @}
  */ 

/**
  * @}
  */ 

/**
  * @}
  */

/**
  * @}
  */

#ifdef __cplusplus
}
#endif

#endif /* __STM32F769I_EVAL_AUDIO_H */

