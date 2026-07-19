/**
  ******************************************************************************
  * @file    stm32f769i_discovery_sd.c
  * @author  MCD Application Team
  * @brief   This file includes the uSD card driver mounted on STM32F769I-Discovery
  *          board.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2016 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

/* File Info : -----------------------------------------------------------------
                                   User NOTES
1. How To use this driver:
--------------------------
   - This driver is used to drive the micro SD external card mounted on STM32F769I-Discovery
     board.
   - This driver does not need a specific component driver for the micro SD device
     to be included with.

2. Driver description:
---------------------
  + Initialization steps:
     o Initialize the micro SD card using the BSP_SD_Init() function. This
       function includes the MSP layer hardware resources initialization and the
       SDIO interface configuration to interface with the external micro SD. It
       also includes the micro SD initialization sequence.
     o To check the SD card presence you can use the function BSP_SD_IsDetected() which
       returns the detection status
     o If SD presence detection interrupt mode is desired, you must configure the
       SD detection interrupt mode by calling the function BSP_SD_ITConfig(). The interrupt
       is generated as an external interrupt whenever the micro SD card is
       plugged/unplugged in/from the discovery board.
     o The function BSP_SD_GetCardInfo() is used to get the micro SD card information
       which is stored in the structure "HAL_SD_CardInfoTypedef".

  + Micro SD card operations
     o The micro SD card can be accessed with read/write block(s) operations once
       it is reay for access. The access cand be performed whether using the polling
       mode by calling the functions BSP_SD_ReadBlocks()/BSP_SD_WriteBlocks(), or by DMA
       transfer using the functions BSP_SD_ReadBlocks_DMA()/BSP_SD_WriteBlocks_DMA()
     o The DMA transfer complete is used with interrupt mode. Once the SD transfer
       is complete, the SD interrupt is handeled using the function BSP_SD_IRQHandler(),
       the DMA Tx/Rx transfer complete are handeled using the functions
       BSP_SD_DMA_Tx_IRQHandler()/BSP_SD_DMA_Rx_IRQHandler(). The corresponding user callbacks
       are implemented by the user at application level.
     o The SD erase block(s) is performed using the function BSP_SD_Erase() with specifying
       the number of blocks to erase.
     o The SD runtime status is returned when calling the function BSP_SD_GetCardState().

------------------------------------------------------------------------------*/

/* Dependencies
- stm32f7xx_hal_sd.c
- stm32f7xx_ll_sdmmc.c
- stm32f7xx_hal_dma.c  
- stm32f7xx_hal_gpio.c
- stm32f7xx_hal_cortex.c
- stm32f7xx_hal_rcc_ex.h
EndDependencies */ 

/* Includes ------------------------------------------------------------------*/
#include "stm32f769i_discovery_sd.h"

/** @addtogroup BSP
  * @{
  */

/** @addtogroup STM32F769I_DISCOVERY
  * @{
  */

/** @defgroup STM32F769I_DISCOVERY_SD STM32F769I_DISCOVERY SD
  * @{
  */


/** @defgroup STM32F769I_DISCOVERY_SD_Private_TypesDefinitions STM32F769I Discovery Sd Private TypesDef
  * @{
  */
/**
  * @}
  */

/** @defgroup STM32F769I_DISCOVERY_SD_Private_Defines STM32F769I Discovery Sd Private Defines
  * @{
  */
/**
  * @}
  */

/** @defgroup STM32F769I_DISCOVERY_SD_Private_Macros STM32F769I Discovery Sd Private Macro
  * @{
  */
/**
  * @}
  */

/** @defgroup STM32F769I_DISCOVERY_SD_Private_Variables STM32F769I Discovery Sd Private Variables
  * @{
  */
SD_HandleTypeDef uSdHandle;

/**
  * @}
  */

/** @defgroup STM32F769I_DISCOVERY_SD_Private_FunctionPrototypes STM32F769I Discovery Sd Private Prototypes
  * @{
  */
/**
  * @}
  */

/** @defgroup STM32F769I_DISCOVERY_SD_Private_Functions STM32F769I Discovery Sd Private Functions
  * @{
  */

/**
  * @brief  Initializes the SD card device.
  * @retval SD status
  */
uint8_t BSP_SD_Init(void)
{
  uint8_t sd_state = MSD_OK;

  /* PLLSAI is dedicated to LCD periph. Do not use it to get 48MHz*/

  /* uSD device interface configuration */
    uSdHandle.Instance = SDMMC2;
    uSdHandle.Init.ClockEdge           = SDMMC_CLOCK_EDGE_RISING;
    uSdHandle.Init.ClockBypass         = SDMMC_CLOCK_BYPASS_DISABLE;
    uSdHandle.Init.ClockPowerSave      = SDMMC_CLOCK_POWER_SAVE_DISABLE;
    uSdHandle.Init.BusWide             = SDMMC_BUS_WIDE_1B;
    /* Hardware flow control ENABLED: SD transfers run in CPU-fed polling mode
     * while the DFSDM mic ISRs fire every few ms. If an ISR preempts the feed
     * loop long enough, the 32-word SDMMC FIFO underruns mid-sector and the
     * write fails (TX_UNDERRUN -> FR_DISK_ERR). With flow control the SDMMC
     * pauses SDCLK instead of erroring when the FIFO can't keep up. */
    uSdHandle.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_ENABLE;
    uSdHandle.Init.ClockDiv            = SDMMC_TRANSFER_CLK_DIV;

  /* Msp SD Detect pin initialization */
  BSP_SD_Detect_MspInit(&uSdHandle, NULL);
  if(BSP_SD_IsDetected() != SD_PRESENT)   /* Check if SD card is present */
  {
    return MSD_ERROR_SD_NOT_PRESENT;
  }

  /* Msp SD initialization */
  BSP_SD_MspInit(&uSdHandle, NULL);

  /* HAL SD initialization */
  if(HAL_SD_Init(&uSdHandle) != HAL_OK)
  {
    sd_state = MSD_ERROR;
  }

  /* Configure SD Bus width.
   * Use 1-bit mode: only D0 (PG9) is used for data. The 4-bit data lines
   * D2/D3 are on PB3/PB4 (the JTAG JTDO/NJTRST pins) and 4-bit data reads
   * CRC-fail on this setup (command phase over CMD/D0 works, so init/SCR read
   * succeed, but the first 4-bit data block read returns FR_DISK_ERR).
   * 1-bit @ ~24 MHz is ~3 MB/s, far more than this 16 kHz recorder needs. */
  if(sd_state == MSD_OK)
  {
    /* Enable 1-bit operation */
    if(HAL_SD_ConfigWideBusOperation(&uSdHandle, SDMMC_BUS_WIDE_1B) != HAL_OK)
    {
      sd_state = MSD_ERROR;
    }
    else
    {
      sd_state = MSD_OK;
    }
  }
  return  sd_state;
}

/**
  * @brief  DeInitializes the SD card device.
  * @retval SD status
  */
uint8_t BSP_SD_DeInit(void)
{
  uint8_t sd_state = MSD_OK;

  uSdHandle.Instance = SDMMC2;

  /* HAL SD deinitialization */
  if(HAL_SD_DeInit(&uSdHandle) != HAL_OK)
  {
    sd_state = MSD_ERROR;
  }

  /* Msp SD deinitialization */
  uSdHandle.Instance = SDMMC2;
  BSP_SD_MspDeInit(&uSdHandle, NULL);

  return  sd_state;
}

/**
  * @brief  Configures Interrupt mode for SD detection pin.
  * @retval Returns 0
  */
uint8_t BSP_SD_ITConfig(void)
{
  GPIO_InitTypeDef gpio_init_structure;

  /* Configure Interrupt mode for SD detection pin */
  gpio_init_structure.Pin = SD_DETECT_PIN;
  gpio_init_structure.Pull = GPIO_PULLUP;
  gpio_init_structure.Speed = GPIO_SPEED_FAST;
  gpio_init_structure.Mode = GPIO_MODE_IT_RISING_FALLING;
  HAL_GPIO_Init(SD_DETECT_GPIO_PORT, &gpio_init_structure);

  /* Enable and set SD detect EXTI Interrupt to the lowest priority */
  HAL_NVIC_SetPriority((IRQn_Type)(SD_DETECT_EXTI_IRQn), 0x0F, 0x00);
  HAL_NVIC_EnableIRQ((IRQn_Type)(SD_DETECT_EXTI_IRQn));

  return MSD_OK;
}

/**
 * @brief  Detects if SD card is correctly plugged in the memory slot or not.
 * @retval Returns if SD is detected or not
 */
uint8_t BSP_SD_IsDetected(void)
{
  __IO uint8_t  status = SD_PRESENT;

  /* Check SD card detect pin */
  if (HAL_GPIO_ReadPin(SD_DETECT_GPIO_PORT, SD_DETECT_PIN) == GPIO_PIN_SET)
  {
    status = SD_NOT_PRESENT;
  }

    return status;
}

/**
  * @brief  Reads block(s) from a specified address in an SD card, in polling mode.
  * @param  pData: Pointer to the buffer that will contain the data to transmit
  * @param  ReadAddr: Address from where data is to be read
  * @param  NumOfBlocks: Number of SD blocks to read
  * @param  Timeout: Timeout for read operation
  * @retval SD status
  */
uint8_t BSP_SD_ReadBlocks(uint32_t *pData, uint32_t ReadAddr, uint32_t NumOfBlocks, uint32_t Timeout)
{
  if(HAL_SD_ReadBlocks(&uSdHandle, (uint8_t *)pData, ReadAddr, NumOfBlocks, Timeout) != HAL_OK)
  {
    return MSD_ERROR;
  }
  else
  {
    return MSD_OK;
  }
}

/**
  * @brief  Writes block(s) to a specified address in an SD card, in polling mode. 
  * @param  pData: Pointer to the buffer that will contain the data to transmit
  * @param  WriteAddr: Address from where data is to be written
  * @param  NumOfBlocks: Number of SD blocks to write
  * @param  Timeout: Timeout for write operation
  * @retval SD status
  */
uint8_t BSP_SD_WriteBlocks(uint32_t *pData, uint32_t WriteAddr, uint32_t NumOfBlocks, uint32_t Timeout)
{
  if(HAL_SD_WriteBlocks(&uSdHandle, (uint8_t *)pData, WriteAddr, NumOfBlocks, Timeout) != HAL_OK)
  {
    return MSD_ERROR;
  }
  else
  {
    return MSD_OK;
  }
}

/**
  * @brief  Reads block(s) from a specified address in an SD card, in DMA mode.
  * @param  pData: Pointer to the buffer that will contain the data to transmit
  * @param  ReadAddr: Address from where data is to be read
  * @param  NumOfBlocks: Number of SD blocks to read 
  * @retval SD status
  */
uint8_t BSP_SD_ReadBlocks_DMA(uint32_t *pData, uint32_t ReadAddr, uint32_t NumOfBlocks)
{  
  /* Read block(s) in DMA transfer mode */
  if(HAL_SD_ReadBlocks_DMA(&uSdHandle, (uint8_t *)pData, ReadAddr, NumOfBlocks) != HAL_OK)
  {
    return MSD_ERROR;
  }
  else
  {
    return MSD_OK;
  }
}

/**
  * @brief  Writes block(s) to a specified address in an SD card, in DMA mode.
  * @param  pData: Pointer to the buffer that will contain the data to transmit
  * @param  WriteAddr: Address from where data is to be written
  * @param  NumOfBlocks: Number of SD blocks to write 
  * @retval SD status
  */
uint8_t BSP_SD_WriteBlocks_DMA(uint32_t *pData, uint32_t WriteAddr, uint32_t NumOfBlocks)
{ 
  /* Write block(s) in DMA transfer mode */
  if(HAL_SD_WriteBlocks_DMA(&uSdHandle, (uint8_t *)pData, WriteAddr, NumOfBlocks) != HAL_OK)
  {
    return MSD_ERROR;
  }
  else
  {
    return MSD_OK;
  }
}

/**
  * @brief  Erases the specified memory area of the given SD card. 
  * @param  StartAddr: Start byte address
  * @param  EndAddr: End byte address
  * @retval SD status
  */
uint8_t BSP_SD_Erase(uint32_t StartAddr, uint32_t EndAddr)
{
  if(HAL_SD_Erase(&uSdHandle, StartAddr, EndAddr) != HAL_OK)
  {
    return MSD_ERROR;
  }
  else
  {
    return MSD_OK;
  }
}

/**
  * @brief  Initializes the SD MSP.
  * @param  hsd: SD handle
  * @param  Params : pointer on additional configuration parameters, can be NULL.
  */
__weak void BSP_SD_MspInit(SD_HandleTypeDef *hsd, void *Params)
{
  GPIO_InitTypeDef gpio_init_structure;

  /* Enable SDMMC2 clock */
  __HAL_RCC_SDMMC2_CLK_ENABLE();

  /* Enable GPIOs clock */
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  
  /* Common GPIO configuration */
  gpio_init_structure.Mode      = GPIO_MODE_AF_PP;
  gpio_init_structure.Pull      = GPIO_PULLUP;
  gpio_init_structure.Speed     = GPIO_SPEED_HIGH;

  /* GPIOB configuration */
  gpio_init_structure.Alternate = GPIO_AF10_SDMMC2;  
  gpio_init_structure.Pin = GPIO_PIN_3 | GPIO_PIN_4;
  HAL_GPIO_Init(GPIOB, &gpio_init_structure);

  /* GPIOD configuration */
  gpio_init_structure.Alternate = GPIO_AF11_SDMMC2;  
  gpio_init_structure.Pin = GPIO_PIN_6 | GPIO_PIN_7;
  HAL_GPIO_Init(GPIOD, &gpio_init_structure);
  
  /* GPIOG configuration */ 
  gpio_init_structure.Pin = GPIO_PIN_9 | GPIO_PIN_10;
  HAL_GPIO_Init(GPIOG, &gpio_init_structure);
  
  /* NVIC configuration for SDMMC2 interrupts */
  HAL_NVIC_SetPriority(SDMMC2_IRQn, 0x0E, 0);
  HAL_NVIC_EnableIRQ(SDMMC2_IRQn);

  /* NOTE: SD transfers run in POLLING mode (BSP_SD_ReadBlocks / WriteBlocks ->
     HAL_SD_ReadBlocks / WriteBlocks), so the SD card does NOT use DMA.
     The previous code here configured DMA2 Stream0 (Rx) and Stream5 (Tx) for the
     SD with DMA_PFCTRL. Those two streams are ALSO the DFSDM mic streams
     (TOP_LEFT=DMA2_Stream0, TOP_RIGHT=DMA2_Stream5). Configuring/owning them here
     forced the mic DMA into PFCTRL/non-circular mode, turning the bounded
     circular mic capture into an unbounded incrementing write that ran off the
     end of Scratch and clobbered WavRecFile -> HardFault. The SD DMA setup is
     removed so DMA2 Stream0/Stream5 belong solely to the DFSDM capture. */
}

/**
  * @brief  Initializes the SD Detect pin MSP.
  * @param  hsd: SD handle
  * @param  Params : pointer on additional configuration parameters, can be NULL.  
  * @retval None
  */
__weak void BSP_SD_Detect_MspInit(SD_HandleTypeDef *hsd, void *Params)
{
  GPIO_InitTypeDef  gpio_init_structure;

  SD_DETECT_GPIO_CLK_ENABLE();

  /* GPIO configuration in input for uSD_Detect signal */
  gpio_init_structure.Pin       = SD_DETECT_PIN;
  gpio_init_structure.Mode      = GPIO_MODE_INPUT;
  gpio_init_structure.Pull      = GPIO_PULLUP;
  gpio_init_structure.Speed     = GPIO_SPEED_HIGH;
  HAL_GPIO_Init(SD_DETECT_GPIO_PORT, &gpio_init_structure);
}

/**
  * @brief  DeInitializes the SD MSP.
  * @param  hsd: SD handle
  * @param  Params : pointer on additional configuration parameters, can be NULL.
  */
__weak void BSP_SD_MspDeInit(SD_HandleTypeDef *hsd, void *Params)
{
    /* SD uses polling mode (no DMA). DMA2 Stream0/Stream5 belong to the DFSDM
       microphones, so they must NOT be de-initialised here. */

    /* Disable NVIC for SDIO interrupts */
    HAL_NVIC_DisableIRQ(SDIO_IRQn);

    /* DeInit GPIO pins can be done in the application
       (by surcharging this __weak function) */

    /* Disable SDIO clock */
    __HAL_RCC_SDIO_CLK_DISABLE();

    /* GPOI pins clock and DMA cloks can be shut down in the applic
       by surcgarging this __weak function */
}

/**
  * @brief  Gets the current SD card data status.
  * @retval Data transfer state.
  *          This value can be one of the following values:
  *            @arg  SD_TRANSFER_OK: No data transfer is acting
  *            @arg  SD_TRANSFER_BUSY: Data transfer is acting
  */
uint8_t BSP_SD_GetCardState(void)
{
  return((HAL_SD_GetCardState(&uSdHandle) == HAL_SD_CARD_TRANSFER ) ? SD_TRANSFER_OK : SD_TRANSFER_BUSY);
}
  

/**
  * @brief  Get SD information about specific SD card.
  * @param  CardInfo: Pointer to HAL_SD_CardInfoTypedef structure
  * @retval None 
  */
void BSP_SD_GetCardInfo(HAL_SD_CardInfoTypeDef *CardInfo)
{
  /* Get SD card Information */
  HAL_SD_GetCardInfo(&uSdHandle, CardInfo);
}

/**
  * @brief SD Abort callbacks
  * @param hsd: SD handle
  * @retval None
  */
void HAL_SD_AbortCallback(SD_HandleTypeDef *hsd)
{
  BSP_SD_AbortCallback();
}

/**
  * @brief Tx Transfer completed callbacks
  * @param hsd: SD handle
  * @retval None
  */
void HAL_SD_TxCpltCallback(SD_HandleTypeDef *hsd)
{
  BSP_SD_WriteCpltCallback();
}

/**
  * @brief Rx Transfer completed callbacks
  * @param hsd: SD handle
  * @retval None
  */
void HAL_SD_RxCpltCallback(SD_HandleTypeDef *hsd)
{
  BSP_SD_ReadCpltCallback();
}

/**
  * @brief BSP SD Abort callbacks
  * @retval None
  */
__weak void BSP_SD_AbortCallback(void)
{

}

/**
  * @brief BSP Tx Transfer completed callbacks
  * @retval None
  */
__weak void BSP_SD_WriteCpltCallback(void)
{

}

/**
  * @brief BSP Rx Transfer completed callbacks
  * @retval None
  */
__weak void BSP_SD_ReadCpltCallback(void)
{

}

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

