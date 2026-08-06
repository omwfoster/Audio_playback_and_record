/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32f7xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f7xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */


extern UART_HandleTypeDef huart1;
extern DSI_HandleTypeDef hdsi_discovery;

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/

/* USER CODE BEGIN EV */

__attribute__((section(".non_cached_ram")))
extern DFSDM_Channel_HandleTypeDef     hAudioInTopLeftChannel;
__attribute__((section(".non_cached_ram")))
extern DFSDM_Channel_HandleTypeDef     hAudioInTopRightChannel;
__attribute__((section(".non_cached_ram")))
extern DFSDM_Filter_HandleTypeDef      hAudioInTopLeftFilter;
__attribute__((section(".non_cached_ram")))
extern DFSDM_Filter_HandleTypeDef      hAudioInTopRightFilter;
__attribute__((section(".non_cached_ram")))
extern DMA_HandleTypeDef               hDmaTopLeft;
__attribute__((section(".non_cached_ram")))
extern DMA_HandleTypeDef               hDmaTopRight;

__attribute__((section(".non_cached_ram")))
extern DFSDM_Channel_HandleTypeDef     hAudioInButtomLeftChannel;
__attribute__((section(".non_cached_ram")))
extern DFSDM_Channel_HandleTypeDef     hAudioInButtomRightChannel;
__attribute__((section(".non_cached_ram")))
extern DFSDM_Filter_HandleTypeDef      hAudioInButtomLeftFilter;
__attribute__((section(".non_cached_ram")))
extern DFSDM_Filter_HandleTypeDef      hAudioInButtomRightFilter;
__attribute__((section(".non_cached_ram")))
extern DMA_HandleTypeDef               hDmaButtomLeft;
__attribute__((section(".non_cached_ram")))
extern DMA_HandleTypeDef               hDmaButtomRight;

extern UART_HandleTypeDef huart1;

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M7 Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
   while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
/* USER CODE BEGIN HardFault_Reporter */
/* Captured fault context (also inspectable in the debugger) */
volatile uint32_t hf_r0, hf_r1, hf_r2, hf_r3, hf_r12, hf_lr, hf_pc, hf_psr;
volatile uint32_t hf_cfsr, hf_hfsr, hf_bfar, hf_mmfar;

static void hf_putc(char c)
{
  while (!(USART1->ISR & USART_ISR_TXE)) { }
  USART1->TDR = (uint8_t)c;
}
static void hf_str(const char *s) { while (*s) hf_putc(*s++); }
static void hf_hex(uint32_t v)
{
  static const char d[] = "0123456789ABCDEF";
  hf_putc('0'); hf_putc('x');
  for (int i = 28; i >= 0; i -= 4) hf_putc(d[(v >> i) & 0xF]);
}

void hard_fault_handler_c(uint32_t *sp)
{
  hf_r0 = sp[0]; hf_r1 = sp[1]; hf_r2 = sp[2]; hf_r3 = sp[3];
  hf_r12 = sp[4]; hf_lr = sp[5]; hf_pc = sp[6]; hf_psr = sp[7];
  hf_cfsr = SCB->CFSR; hf_hfsr = SCB->HFSR; hf_bfar = SCB->BFAR; hf_mmfar = SCB->MMFAR;

  hf_str("\r\n*** HARDFAULT ***\r\nPC=");  hf_hex(hf_pc);
  hf_str(" LR=");  hf_hex(hf_lr);
  hf_str(" PSR="); hf_hex(hf_psr);
  hf_str("\r\nCFSR="); hf_hex(hf_cfsr);
  hf_str(" HFSR=");    hf_hex(hf_hfsr);
  hf_str(" BFAR=");    hf_hex(hf_bfar);   /* faulting data address (if BFARVALID) */
  hf_str(" MMFAR=");   hf_hex(hf_mmfar);
  hf_str("\r\nR0="); hf_hex(hf_r0);
  hf_str(" R1=");    hf_hex(hf_r1);
  hf_str(" R2=");    hf_hex(hf_r2);
  hf_str(" R3=");    hf_hex(hf_r3);
  hf_str(" R12=");   hf_hex(hf_r12);
  hf_str("\r\n");

  while (1) { }
}
/* USER CODE END HardFault_Reporter */

__attribute__((naked)) void HardFault_Handler(void)
{
  __asm volatile (
    "tst   lr, #4               \n"  /* which stack was in use? */
    "ite   eq                   \n"
    "mrseq r0, msp              \n"
    "mrsne r0, psp              \n"
    "b     hard_fault_handler_c \n"  /* r0 = stacked frame pointer */
  );
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */

    volatile uint32_t mmfar = SCB->MMFAR;  // faulting address
    volatile uint32_t mmfsr = SCB->CFSR & 0xFF; // MemManage status bits
    __disable_irq();
    while(1);


}

/**
  * @brief This function handles Pre-fetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */

  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */

  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles System service call via SWI instruction.
  */
void SVC_Handler(void)
{
  /* USER CODE BEGIN SVCall_IRQn 0 */

  /* USER CODE END SVCall_IRQn 0 */
  /* USER CODE BEGIN SVCall_IRQn 1 */

  /* USER CODE END SVCall_IRQn 1 */
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/**
  * @brief This function handles Pendable request for system service.
  */
void PendSV_Handler(void)
{
  /* USER CODE BEGIN PendSV_IRQn 0 */

  /* USER CODE END PendSV_IRQn 0 */
  /* USER CODE BEGIN PendSV_IRQn 1 */

  /* USER CODE END PendSV_IRQn 1 */
}

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */

  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  /* USER CODE BEGIN SysTick_IRQn 1 */

  lv_tick_inc(1);

  /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32F7xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32f7xx.s).                    */
/******************************************************************************/

/* USER CODE BEGIN 1 */



void USART1_IRQHandler(void) {
    HAL_UART_IRQHandler(&huart1);
}


void USARTx_DMA_TX_IRQHandler(void)
{
  HAL_DMA_IRQHandler(huart1.hdmatx);
}


/**
  * @brief  This function handles UART interrupt request.
  * @param  None
  * @retval None
  * @Note   This function is redefined in "main.h" and related to DMA
  *         used for USART data transmission
  */
void USARTx_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart1);
}

extern DMA_HandleTypeDef hdma_usart1_tx;

void DMA2_Stream7_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart1_tx);
}


void DMA2_Stream0_IRQHandler(void)
{

    HAL_DMA_IRQHandler(hAudioInTopLeftFilter.hdmaReg);

}

/**
  * @brief  This function handles DMA2 Stream 5 interrupt request.
  * @param  None
  * @retval None
  */
void DMA2_Stream5_IRQHandler(void)
{

   HAL_DMA_IRQHandler(hAudioInTopRightFilter.hdmaReg);

}

void DSI_IRQHandler(void){
  HAL_DSI_IRQHandler(&hdsi_discovery);
}





/* USER CODE END 1 */
