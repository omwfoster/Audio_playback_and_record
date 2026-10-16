/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
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
#include <audio_stream.h>
#include <audio_stream_fft.h>
#include <audio_stream_tone.h>
#include "main.h"
#include <string.h>
#include "ai_logging.h"
#include "waveplayer.h"
#include "waverecorder.h"
#include "app_state.h"
#include "ui.h"
#include "console_buffer.h"
#include "prof.h"
#include "spectrum_view.h"
#include "spectrum_tab.h"
#include "context_panel.h"

prof_t g_prof_touch;
prof_t g_prof_console;
prof_t g_prof_lv;

UART_HandleTypeDef huart1;

FATFS SDCard_FatFs;
char SDCard_Path[4] = "0:/";

extern __attribute__((section(".dma_buffers")))q15_t fft_output[]; // FFT_SIZE * 2

extern __attribute__((section(".dma_buffers")))q15_t mag_bins[FFT_BLOCK_SIZE];

extern __attribute__((section(".dma_buffers")))q15_t mag_bins_output[FFT_BLOCK_SIZE];

uint16_t pcm_left[FFT_BLOCK_SIZE * 2];
uint16_t pcm_right[FFT_BLOCK_SIZE * 2];

extern __attribute__((section(".dma_buffers")))
      AUDIO_IN_BufferTypeDef BufferCtl_In;

AUDIO_PLAYBACK_StateTypeDef AudioState;
AUDIO_ErrorTypeDef status;
AUDIO_ApplicationTypeDef appli_state;
AUDIO_ApplicationTypeDef *h_appli_state;

q15_t fft_output[FFT_BLOCK_SIZE * 2]; // FFT_SIZE * 2

q15_t mag_bins[FFT_BLOCK_SIZE];

q15_t mag_bins_output[FFT_BLOCK_SIZE];

q15_t input_left[FFT_BLOCK_SIZE];
q15_t input_right[FFT_BLOCK_SIZE];

DMA_HandleTypeDef hdma_usart1_tx;
TIM_HandleTypeDef htim3;

extern volatile uint32_t DmaRecHalfBuffCplt;
extern volatile uint32_t DmaRecBuffCplt;

uint32_t PlaybackStarted = 0;
/* USER CODE BEGIN PV */

/* USER CODE END PV */
/* Private function prototypes -----------------------------------------------*/

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */

static void CPU_CACHE_Enable(void);
static void control_panel_init(void);
static void colour_bar_refresh(void);
void MX_DMA_Init();
static void MX_USART1_UART_Init(void);
static void HandleCommand(uint8_t cmd);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */

/* USER CODE BEGIN 0 */

int main(void) {

	/* USER CODE BEGIN 1 */
	uint32_t i;
	appli_state = APPLICATION_IDLE;
	h_appli_state = &appli_state;
	static uint32_t s_last_dropped = 0;
	uint32_t dropped;
	static uint8_t s_was_active = 0;
	static uint32_t s_max_lv_ms = 0;
	static uint32_t s_rec_t0 = 0;

	/* USER CODE END 1 */

	/* MPU Configuration--------------------------------------------------------*/
	MPU_Config();
	prof_init();
	/* Enable the CPU Cache */
	CPU_CACHE_Enable();

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	BSP_SDRAM_Init();
	/* USER CODE BEGIN 2 */

	MX_DMA_Init();

	char sd_path[4] = "0:/"; /* FatFs logical drive path, e.g. "0:/" */
	if (FATFS_LinkDriver(&SD_Driver, sd_path) != 0) {
		/* link failed -- handle however you want unrecoverable storage failures handled */
	}

	lv_init();
	tft_init();
	touchpad_init();
	lv_log_register_print_cb(ui_console_lv_log_cb);

	ui_init();
	ui_console_bind(ui_TextArea1);

	FFT_Pipeline_Init();
	spectrum_view_init(ui_Chart2);
	init_spectrogram(ui_canvasplaceholder);

	/* Right-hand control panel: Start/Stop, Save|FFT mode, colour map,
	 * status card. Events are attached here, not in SquareLine. */
	control_panel_init();

	/* Context area (ui_ContextContainer): FFT status card, or spectrogram
	 * settings while the Spectrogram tab is open */
	context_panel_init(colour_bar_refresh);

	while (1) {
		App_Process();

		if (AUDIO_REC_IsActive()) {
			AUDIO_REC_Process();

		}

		/* Drop diagnosis: a drop means AUDIO_REC_Process() wasn't reached for
		 * ~0.5 s. Track the longest lv_task_handler() pass per recording and
		 * log it (with the recorder's longest f_write) when the recording ends. */

		uint32_t t_lv = HAL_GetTick();
		uint32_t t_prof = prof_start();
		lv_task_handler();
		prof_stop(&g_prof_lv, t_prof);
		t_lv = HAL_GetTick() - t_lv;

		context_panel_update();

		if (AUDIO_REC_IsActive()
				&& (AUDIO_REC_GetMode() == AUDIO_REC_MODE_LOG)) {
			if (!s_was_active) {
				s_was_active = 1;
				s_max_lv_ms = 0;
				s_last_dropped = 0; /* the recorder resets its count on each Start */
				s_rec_t0 = HAL_GetTick();
				memset(&g_prof_touch, 0, sizeof(prof_t));
				memset(&g_prof_console, 0, sizeof(prof_t));
				memset(&g_prof_lv, 0, sizeof(prof_t));
			}
			dropped = AUDIO_REC_GetDroppedCount();

			if (dropped != s_last_dropped) {
				ui_console_log("Dropped %lu buffer(s)",
						(unsigned long) dropped);
				s_last_dropped = dropped;
			}

			extern volatile uint32_t DmaRecCallbackCount;

			static uint32_t s_rate_t0 = 0;
			static uint32_t s_rate_last = 0;
			if (HAL_GetTick() - s_rate_t0 >= 1000U) {
				uint32_t now = DmaRecCallbackCount;
				ui_console_log("rec cb/s: %lu",
						(unsigned long) (now - s_rate_last));
				s_rate_last = now;
				s_rate_t0 = HAL_GetTick();
			}
			if (t_lv > s_max_lv_ms)
				s_max_lv_ms = t_lv;
		} else if (s_was_active) {
			s_was_active = 0;
			ui_console_log("max lv_task_handler: %lu ms",
					(unsigned long) s_max_lv_ms);

			/* Where lv_task_handler's time went over the recording: total ms
			 * spent, longest single call, and number of calls. */
			uint32_t rec_ms = HAL_GetTick() - s_rec_t0;
			const struct {
				const char *name;
				prof_t *p;
			} rows[] = { { "lv_task", &g_prof_lv }, { "touch", &g_prof_touch },
					{ "console", &g_prof_console }, };
			ui_console_log("prof over %lu ms: total ms / max us / calls",
					(unsigned long) rec_ms);
			for (unsigned r = 0; r < sizeof(rows) / sizeof(rows[0]); r++) {
				ui_console_log("  %-7s %lu / %lu / %lu", rows[r].name,
						(unsigned long) (prof_cyc_to_us(rows[r].p->total_cyc)
								/ 1000u),
						(unsigned long) prof_cyc_to_us(rows[r].p->max_cyc),
						(unsigned long) rows[r].p->calls);
			}
		}

		/* FFT mode: AUDIO_REC_Process() above runs FFT_Process_Audio_Block()
		 * on each 512 ms half of BufferCtl_In.pcm_buff and updates the bar
		 * chart (waverecorder.c, ProcessFftHalf()). */
	}
	/* USER CODE END 3 */
}
/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

	/* Enable PWR clock and select Voltage Scale 1 (required above 180 MHz). */
	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

	/* Enable HSE (25 MHz crystal) and run the main PLL from it.
	 *
	 * HSE IS MANDATORY HERE: the DSI D-PHY PLL is fed only from HSE, so the
	 * LCD cannot come up on HSI (DSI reads/writes never complete -> the
	 * OTM8009A/NT35510 init "spins"). In addition, both BSP clock configs
	 * assume HSE/PLLM = 1 MHz:
	 *   - BSP_AUDIO_IN/OUT_ClockConfig -> PLLI2S (SAI2/SAI1, DFSDM)
	 *   - BSP_LCD_InitEx               -> PLLSAI (LTDC) and the DSI PLL
	 *
	 * PLLM = 25 -> 1 MHz PLL input.
	 * PLLN = 384 -> VCO = 384 MHz; PLLP = 2 -> SYSCLK = 192 MHz.
	 * PLLQ = 8   -> PLL48CLK = 384/8 = 48 MHz EXACTLY. This feeds the SDMMC2
	 *   kernel clock (reset default = PLL48CLK), so the BSP dividers produce an
	 *   in-spec card clock: init 48/120 = 400 kHz, transfer 48/2 = 24 MHz.
	 *   (At 50 MHz the transfer clock hit 25 MHz / init 417 kHz -> FR_DISK_ERR.) */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM = 25;
	RCC_OscInitStruct.PLL.PLLN = 384;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 8;
	RCC_OscInitStruct.PLL.PLLR = 7;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/* Activate OverDrive (required above 180 MHz). */
	if (HAL_PWREx_EnableOverDrive() != HAL_OK) {
		Error_Handler();
	}

	/* SYSCLK=192MHz, HCLK=192MHz, APB1=48MHz, APB2=96MHz. */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_6) != HAL_OK) {
		Error_Handler();
	}

	/* IMPORTANT: PLLSAI (LTDC/DSI) and PLLI2S (audio SAI/DFSDM) are configured
	 * by the BSP at init time (BSP_LCD_InitEx, BSP_AUDIO_IN/OUT_ClockConfig).
	 * Do NOT program them here -- a manual PLLSAI setup collides with the LCD
	 * and was the reason the previous build fought over the PLLSAI. */
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	/* USER CODE BEGIN MX_GPIO_Init_1 */

	/* USER CODE END MX_GPIO_Init_1 */

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOE_CLK_ENABLE();
	__HAL_RCC_GPIOG_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOJ_CLK_ENABLE();
	__HAL_RCC_GPIOI_CLK_ENABLE();
	__HAL_RCC_GPIOK_CLK_ENABLE();
	__HAL_RCC_GPIOF_CLK_ENABLE();
	__HAL_RCC_GPIOH_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOJ, LD_USER1_Pin | LD_USER2_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pins : SAI1_FSA_Pin SAI1_SDB_Pin SAI1_SCKA_Pin SAI1_SDA_Pin */
	GPIO_InitStruct.Pin = SAI1_FSA_Pin | SAI1_SDB_Pin | SAI1_SCKA_Pin
			| SAI1_SDA_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF6_SAI1;
	HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

	/*Configure GPIO pin : QSPI_D2_Pin */
	GPIO_InitStruct.Pin = QSPI_D2_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
	HAL_GPIO_Init(QSPI_D2_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : RMII_TXD1_Pin RMII_TXD0_Pin RMII_TX_EN_Pin */
	GPIO_InitStruct.Pin = RMII_TXD1_Pin | RMII_TXD0_Pin | RMII_TX_EN_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
	HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

	/*Configure GPIO pins : FMC_NBL1_Pin FMC_NBL0_Pin FMC_D5_Pin FMC_D6_Pin
	 FMC_D8_Pin FMC_D11_Pin FMC_D4_Pin FMC_D7_Pin
	 FMC_D9_Pin FMC_D12_Pin FMC_D10_Pin */
	GPIO_InitStruct.Pin = FMC_NBL1_Pin | FMC_NBL0_Pin | FMC_D5_Pin | FMC_D6_Pin
			| FMC_D8_Pin | FMC_D11_Pin | FMC_D4_Pin | FMC_D7_Pin | FMC_D9_Pin
			| FMC_D12_Pin | FMC_D10_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF12_FMC;
	HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

	/*Configure GPIO pins : ARDUINO_SCL_D15_Pin ARDUINO_SDA_D14_Pin */
	GPIO_InitStruct.Pin = ARDUINO_SCL_D15_Pin | ARDUINO_SDA_D14_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/*Configure GPIO pins : ULPI_D7_Pin ULPI_D6_Pin ULPI_D5_Pin ULPI_D3_Pin
	 ULPI_D2_Pin ULPI_D1_Pin ULPI_D4_Pin */
	GPIO_InitStruct.Pin = ULPI_D7_Pin | ULPI_D6_Pin | ULPI_D5_Pin | ULPI_D3_Pin
			| ULPI_D2_Pin | ULPI_D1_Pin | ULPI_D4_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/*Configure GPIO pin : CEC_Pin */
	GPIO_InitStruct.Pin = CEC_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF4_CEC;
	HAL_GPIO_Init(CEC_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : AUDIO_SDA_Pin */
	GPIO_InitStruct.Pin = AUDIO_SDA_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF11_I2C4;
	HAL_GPIO_Init(AUDIO_SDA_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : QSPI_NCS_Pin */
	GPIO_InitStruct.Pin = QSPI_NCS_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF10_QUADSPI;
	HAL_GPIO_Init(QSPI_NCS_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : FMC_SDNCAS_Pin FMC_SDCLK_Pin FMC_A11_Pin FMC_A12_Pin
	 FMC_A10_Pin FMC_BA1_Pin FMC_BA0_Pin */
	GPIO_InitStruct.Pin = FMC_SDNCAS_Pin | FMC_SDCLK_Pin | FMC_A11_Pin
			| FMC_A12_Pin | FMC_A10_Pin | FMC_BA1_Pin | FMC_BA0_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF12_FMC;
	HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

	/*Configure GPIO pins : LD_USER1_Pin LD_USER2_Pin */
	GPIO_InitStruct.Pin = LD_USER1_Pin | LD_USER2_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOJ, &GPIO_InitStruct);

	/*Configure GPIO pins : Audio_INT_Pin WIFI_RST_Pin DSI_RESET_Pin ARD_D8_Pin
	 ARD_D7_Pin ARD_D4_Pin ARD_D2_Pin */
	GPIO_InitStruct.Pin = Audio_INT_Pin | WIFI_RST_Pin | DSI_RESET_Pin
			| ARD_D8_Pin | ARD_D7_Pin | ARD_D4_Pin | ARD_D2_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOJ, &GPIO_InitStruct);

	/*Configure GPIO pins : FMC_D2_Pin FMC_D3_Pin FMC_D1_Pin FMC_D15_Pin
	 FMC_D0_Pin FMC_D14_Pin FMC_D13_Pin */
	GPIO_InitStruct.Pin = FMC_D2_Pin | FMC_D3_Pin | FMC_D1_Pin | FMC_D15_Pin
			| FMC_D0_Pin | FMC_D14_Pin | FMC_D13_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF12_FMC;
	HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

	/*Configure GPIO pins : QSPI_D1_Pin QSPI_D0_Pin */
	GPIO_InitStruct.Pin = QSPI_D1_Pin | QSPI_D0_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/*Configure GPIO pin : ARD_D13_SCK_Pin */
	GPIO_InitStruct.Pin = ARD_D13_SCK_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
	HAL_GPIO_Init(ARD_D13_SCK_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : NC4_Pin NC5_Pin uSD_Detect_Pin LCD_BL_CTRL_Pin */
	GPIO_InitStruct.Pin = NC4_Pin | NC5_Pin | uSD_Detect_Pin | LCD_BL_CTRL_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);

	/*Configure GPIO pins : FMC_NBL2_Pin D27_Pin D26_Pin FMC_NBL3_Pin
	 D29_Pin D31_Pin D28_Pin D25_Pin
	 D30_Pin D24_Pin */
	GPIO_InitStruct.Pin = FMC_NBL2_Pin | D27_Pin | D26_Pin | FMC_NBL3_Pin
			| D29_Pin | D31_Pin | D28_Pin | D25_Pin | D30_Pin | D24_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF12_FMC;
	HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);

	/*Configure GPIO pins : NC3_Pin NC2_Pin NC1_Pin NC8_Pin
	 NC7_Pin */
	GPIO_InitStruct.Pin = NC3_Pin | NC2_Pin | NC1_Pin | NC8_Pin | NC7_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOK, &GPIO_InitStruct);

	/*Configure GPIO pin : SPDIF_RX_Pin */
	GPIO_InitStruct.Pin = SPDIF_RX_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF7_SPDIFRX;
	HAL_GPIO_Init(SPDIF_RX_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : uSD_D1_Pin uSD_D0_Pin */
	GPIO_InitStruct.Pin = uSD_D1_Pin | uSD_D0_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF11_SDMMC2;
	HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

	/*Configure GPIO pins : RMII_RXER_Pin OTG_FS_OverCurrent_Pin */
	GPIO_InitStruct.Pin = RMII_RXER_Pin | OTG_FS_OverCurrent_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

	/*Configure GPIO pin : DFSDM_CKOUT_Pin */
	GPIO_InitStruct.Pin = DFSDM_CKOUT_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF3_DFSDM1;
	HAL_GPIO_Init(DFSDM_CKOUT_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : SPI2_NSS_Pin */
	GPIO_InitStruct.Pin = SPI2_NSS_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
	HAL_GPIO_Init(SPI2_NSS_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : PC13 */
	GPIO_InitStruct.Pin = GPIO_PIN_13;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/*Configure GPIO pins : FMC_A0_Pin FMC_A1_Pin FMC_A2_Pin FMC_A3_Pin
	 FMC_A4_Pin FMC_A5_Pin FMC_A6_Pin FMC_A9_Pin
	 FMC_A7_Pin FMC_A8_Pin FMC_SDNRAS_Pin */
	GPIO_InitStruct.Pin = FMC_A0_Pin | FMC_A1_Pin | FMC_A2_Pin | FMC_A3_Pin
			| FMC_A4_Pin | FMC_A5_Pin | FMC_A6_Pin | FMC_A9_Pin | FMC_A7_Pin
			| FMC_A8_Pin | FMC_SDNRAS_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF12_FMC;
	HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

	/*Configure GPIO pins : D23_Pin D21_Pin D22_Pin FMC_SDNME_Pin
	 FMC_SDNE0_Pin FMC_SDCKE0_Pin D20_Pin FMC_D_7_Pin
	 FMC_D19_Pin FMC_D16_Pin FMC_D18_Pin */
	GPIO_InitStruct.Pin = D23_Pin | D21_Pin | D22_Pin | FMC_SDNME_Pin
			| FMC_SDNE0_Pin | FMC_SDCKE0_Pin | D20_Pin | FMC_D_7_Pin
			| FMC_D19_Pin | FMC_D16_Pin | FMC_D18_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF12_FMC;
	HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

	/*Configure GPIO pin : VCP_RX_Pin */
	GPIO_InitStruct.Pin = VCP_RX_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
	HAL_GPIO_Init(VCP_RX_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : VCP_TX_Pin */
	GPIO_InitStruct.Pin = VCP_TX_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
	HAL_GPIO_Init(VCP_TX_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : ULPI_DIR_Pin */
	GPIO_InitStruct.Pin = ULPI_DIR_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
	HAL_GPIO_Init(ULPI_DIR_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : CEC_CLK_Pin */
	GPIO_InitStruct.Pin = CEC_CLK_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF0_MCO;
	HAL_GPIO_Init(CEC_CLK_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : LCD_INT_Pin */
	GPIO_InitStruct.Pin = LCD_INT_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(LCD_INT_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : ARD_D5_PWM_Pin */
	GPIO_InitStruct.Pin = ARD_D5_PWM_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF2_TIM3;
	HAL_GPIO_Init(ARD_D5_PWM_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : ARD_D0_RX_Pin ARDUINO_TX_D1_Pin */
	GPIO_InitStruct.Pin = ARD_D0_RX_Pin | ARDUINO_TX_D1_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF8_USART6;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/*Configure GPIO pin : ULPI_NXT_Pin */
	GPIO_InitStruct.Pin = ULPI_NXT_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
	HAL_GPIO_Init(ULPI_NXT_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : SAI1_MCLKA_Pin */
	GPIO_InitStruct.Pin = SAI1_MCLKA_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF6_SAI1;
	HAL_GPIO_Init(SAI1_MCLKA_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : EXT_SDA_Pin EXT_SCL_Pin */
	GPIO_InitStruct.Pin = EXT_SDA_Pin | EXT_SCL_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

	/*Configure GPIO pins : ARDUINO_A1_Pin ARDUINO_A2_Pin ARDUINO_A3_Pin */
	GPIO_InitStruct.Pin = ARDUINO_A1_Pin | ARDUINO_A2_Pin | ARDUINO_A3_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

	/*Configure GPIO pin : ULPI_STP_Pin */
	GPIO_InitStruct.Pin = ULPI_STP_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
	HAL_GPIO_Init(ULPI_STP_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : RMII_MDC_Pin RMII_RXD0_Pin RMII_RXD1_Pin */
	GPIO_InitStruct.Pin = RMII_MDC_Pin | RMII_RXD0_Pin | RMII_RXD1_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/*Configure GPIO pin : ARD_A2_Pin */
	GPIO_InitStruct.Pin = ARD_A2_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(ARD_A2_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : PB2 */
	GPIO_InitStruct.Pin = GPIO_PIN_2;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/*Configure GPIO pin : AUDIO_SCL_Pin */
	GPIO_InitStruct.Pin = AUDIO_SCL_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF4_I2C4;
	HAL_GPIO_Init(AUDIO_SCL_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : QSPI_D3_Pin */
	GPIO_InitStruct.Pin = QSPI_D3_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
	HAL_GPIO_Init(QSPI_D3_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : RMII_REF_CLK_Pin RMII_MDIO_Pin RMII_CRS_DV_Pin */
	GPIO_InitStruct.Pin =
	RMII_REF_CLK_Pin | RMII_MDIO_Pin | RMII_CRS_DV_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pin : B_USER_Pin */
	GPIO_InitStruct.Pin = B_USER_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(B_USER_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : ARD_A1_Pin ARD_A0_Pin */
	GPIO_InitStruct.Pin = ARD_A1_Pin | ARD_A0_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pin : SPDIF_TX_Pin */
	GPIO_InitStruct.Pin = SPDIF_TX_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF10_SAI2;
	HAL_GPIO_Init(SPDIF_TX_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : EXT_RST_Pin */
	GPIO_InitStruct.Pin = EXT_RST_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(EXT_RST_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : ULPI_CLK_Pin ULPI_D0_Pin */
	GPIO_InitStruct.Pin = ULPI_CLK_Pin | ULPI_D0_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pin : DSIHOST_TE_Pin */
	GPIO_InitStruct.Pin = DSIHOST_TE_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF13_DSI;
	HAL_GPIO_Init(DSIHOST_TE_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : ARDUINO_PWM_D6_Pin */
	GPIO_InitStruct.Pin = ARDUINO_PWM_D6_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF9_TIM12;
	HAL_GPIO_Init(ARDUINO_PWM_D6_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : ARDUINO_MISO_D12_Pin ARDUINO_MOSI_PWM_D11_Pin */
	GPIO_InitStruct.Pin = ARDUINO_MISO_D12_Pin | ARDUINO_MOSI_PWM_D11_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/* USER CODE BEGIN MX_GPIO_Init_2 */

	/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* MPU Configuration */

/**
 * @brief USART1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART1_UART_Init(void) {

	/* USER CODE END USART1_Init 1 */
	huart1.Instance = USART1;
	huart1.Init.BaudRate = 921600;
	huart1.Init.WordLength = UART_WORDLENGTH_8B;
	huart1.Init.StopBits = UART_STOPBITS_1;
	huart1.Init.Parity = UART_PARITY_NONE;
	huart1.Init.Mode = UART_MODE_TX_RX;
	huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart1.Init.OverSampling = UART_OVERSAMPLING_16;
	huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_UART_Init(&huart1) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USART1_Init 2 */

	HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(USART1_IRQn);

	/* USER CODE END USART1_Init 2 */

}

void MX_DMA_Init() {

	__HAL_RCC_DMA2_CLK_ENABLE();

	hdma_usart1_tx.Instance = DMA2_Stream7;
	hdma_usart1_tx.Init.Channel = DMA_CHANNEL_4;
	hdma_usart1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
	hdma_usart1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
	hdma_usart1_tx.Init.MemInc = DMA_MINC_ENABLE;
	hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
	hdma_usart1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
	hdma_usart1_tx.Init.Mode = DMA_NORMAL;
	hdma_usart1_tx.Init.Priority = DMA_PRIORITY_LOW;
	hdma_usart1_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;

	HAL_DMA_Init(&hdma_usart1_tx);
	__HAL_LINKDMA(&huart1, hdmatx, hdma_usart1_tx);

	HAL_NVIC_SetPriority(DMA2_Stream7_IRQn, 6, 0);
	HAL_NVIC_EnableIRQ(DMA2_Stream7_IRQn);

}

static void CPU_CACHE_Enable(void) {
	/* Enable I-Cache */
	SCB_EnableICache();

	/* Enable D-Cache */
	SCB_EnableDCache();
}

void MPU_Config(void) {
	MPU_Region_InitTypeDef MPU_InitStruct = { 0 };

	/* Disables the MPU */
	HAL_MPU_Disable();

	/** Initializes and configures the Region and the memory to be protected
	 */
	MPU_InitStruct.Enable = MPU_REGION_ENABLE;
	MPU_InitStruct.Number = MPU_REGION_NUMBER0;
	MPU_InitStruct.BaseAddress = 0x0;
	MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
	MPU_InitStruct.SubRegionDisable = 0x87;
	MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
	MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
	MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
	MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
	MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
	MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

	HAL_MPU_ConfigRegion(&MPU_InitStruct);
	/* Enables the MPU */

	/* Region 1: all internal SRAM (DTCM + SRAM1 + SRAM2 = 512KB) as non-cacheable.
	 * Base MUST be 512KB-aligned, so it starts at 0x20000000 (NOT 0x20020000 --
	 * that base isn't 512KB-aligned and the MPU would silently mask it to
	 * 0x20000000 anyway). The DTCM portion is overridden by the higher-numbered
	 * Region 2 below (regions overlap -> highest number wins), so only SRAM1/2
	 * (0x20020000-0x20080000, the .dma_buffers) end up non-cacheable. */
	MPU_InitStruct.Number = MPU_REGION_NUMBER1;
	MPU_InitStruct.BaseAddress = 0x20000000;
	MPU_InitStruct.Size = MPU_REGION_SIZE_512KB;
	MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL1;
	MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
	MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
	MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
	MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
	MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
	MPU_InitStruct.SubRegionDisable = 0x00;
	MPU_InitStruct.Enable = MPU_REGION_ENABLE;
	HAL_MPU_ConfigRegion(&MPU_InitStruct);

	/* Region 2: DTCM — stack, .bss, .data */
	MPU_InitStruct.Number = MPU_REGION_NUMBER2;
	MPU_InitStruct.BaseAddress = 0x20000000;
	MPU_InitStruct.Size = MPU_REGION_SIZE_128KB;
	MPU_InitStruct.SubRegionDisable = 0x00;
	MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL1;
	MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
	MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
	MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
	MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
	MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
	HAL_MPU_ConfigRegion(&MPU_InitStruct);

	/* Region 3: Flash — code */
	MPU_InitStruct.Number = MPU_REGION_NUMBER3;
	MPU_InitStruct.BaseAddress = 0x08000000;
	MPU_InitStruct.Size = MPU_REGION_SIZE_2MB;
	MPU_InitStruct.SubRegionDisable = 0x00;
	MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL1;
	MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
	MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
	MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
	MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
	MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
	HAL_MPU_ConfigRegion(&MPU_InitStruct);

	/* Region 4: FMC control registers — device memory, no cache */
	MPU_InitStruct.Number = MPU_REGION_NUMBER4;
	MPU_InitStruct.BaseAddress = 0xA0000000;
	MPU_InitStruct.Size = MPU_REGION_SIZE_8KB;
	MPU_InitStruct.SubRegionDisable = 0x00;
	MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
	MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
	MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
	MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
	MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
	MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE; /* device memory */
	HAL_MPU_ConfigRegion(&MPU_InitStruct);

	/* Region 5: SDRAM — WRITE-THROUGH (TEX=0,C=1,B=0), not write-back.
	 * The LTDC framebuffer lives here and the LTDC scans SDRAM directly,
	 * bypassing the D-cache. With write-back, CPU text draws linger in the
	 * cache and the panel shows stale pixels until eviction -> intermittent
	 * screen corruption. Write-through makes every CPU write land in SDRAM
	 * immediately, keeping the panel coherent (matches ST's reference BSP). */
	MPU_InitStruct.Number = MPU_REGION_NUMBER5;
	MPU_InitStruct.BaseAddress = 0xC0000000;
	MPU_InitStruct.Size = MPU_REGION_SIZE_16MB;
	MPU_InitStruct.SubRegionDisable = 0x00;
	MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
	MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
	MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
	MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
	MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
	MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE; /* write-through */
	HAL_MPU_ConfigRegion(&MPU_InitStruct);

	HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

	/* Region 6: LVGL render buffers -- write-back cacheable, overriding Region 1
	 * (non-cacheable SRAM1/2). Higher region number wins on the overlap.
	 * Base must be 256KB-aligned: 0x20040000 covers 0x20040000-0x2007FFFF. */
	MPU_InitStruct.Number = MPU_REGION_NUMBER6;
	MPU_InitStruct.BaseAddress = 0x20040000;
	MPU_InitStruct.Size = MPU_REGION_SIZE_256KB;
	MPU_InitStruct.SubRegionDisable = 0x00;
	MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL1;
	MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
	MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
	MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
	MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
	MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE; /* was NOT_BUFFERABLE */
	HAL_MPU_ConfigRegion(&MPU_InitStruct);
}

static void HandleCommand(uint8_t cmd) {
	switch (cmd) {
	case CMD_START_RAW_STREAM:
		if (stream_status.mode != STREAM_MODE_RAW) {
			stream_status.mode = STREAM_MODE_RAW;
			// Enable raw audio streaming
		}
		break;

	case CMD_SEND_SINGLE_RAW:
		// Send single raw audio buffer
		// Call function to send one buffer
		//       	AudioStream_SendFFTDataDB(db_bins_output, FFT_SIZE / 2);
		break;

	case CMD_START_FFT_STREAM:
		stream_status.mode = STREAM_MODE_FFT;
		break;

	case CMD_SEND_SINGLE_FFT:
		// Send single FFT result
		// Call function to compute and send one FFT
		//       	AudioStream_SendFFTDataDB(db_bins_output, FFT_SIZE / 2);
		break;

	default:
		// Unknown command — nothing to do (function is void)
		break;
	}
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	volatile uint32_t pllsaicfgr = RCC->PLLSAICFGR;
	volatile uint32_t rcc_cr = RCC->CR;
	while (1) {
	}
	/* USER CODE END Error_Handler_Debug */
}

void Default_Handler_C(void) {
	volatile uint32_t ipsr = __get_IPSR();        // which IRQ
	volatile uint32_t cfsr = SCB->CFSR;           // fault status
	volatile uint32_t hfsr = SCB->HFSR;
	(void) ipsr;
	(void) cfsr;
	(void) hfsr;
	__BKPT(0);
}

/* --- Right-hand control panel (ui_Panel7) ---------------------------------
 *   ui_Button7 / ui_Label11   Start
 *   ui_Button6 / ui_Label18   Stop
 *   ui_uiModeSave, ui_uiModeFFT   Save | FFT mode pair
 *   ui_Dropdown2              colour map (options matched by name)
 *   ui_ColourMap              colour bar for the selected map (below the dropdown)
 *   ui_ContextContainer       status card / spectrogram settings (context_panel.c)
 * All events are attached in control_panel_init(); SquareLine function
 * export is off. */

#define PANEL_ACCENT     0x4FC3F7   /* FFT / selected mode */
#define PANEL_REC        0xEF4444   /* saving to SD */
#define PANEL_IDLE       0x8A8A99
#define PANEL_SEG_BG     0x262631   /* unselected mode button */
#define PANEL_SEG_TEXT   0xC9C9D3
#define PANEL_DARK_TEXT  0x0B0B0F

static void start_btn_cb(lv_event_t * e)
{
	(void)e;
	if (!AUDIO_REC_IsActive()) {
		AUDIO_REC_Start(); /* save or FFT, per AUDIO_REC_SetMode() */
	}
}

static void stop_btn_cb(lv_event_t * e)
{
	(void)e;
	if (AUDIO_REC_IsActive()) {
		AUDIO_REC_RequestStop();
	}
}

/* Style one half of the Save | FFT pair as selected or not */
static void mode_btn_style(lv_obj_t * btn, bool selected)
{
	lv_obj_set_style_bg_color(btn, lv_color_hex(selected ? PANEL_ACCENT : PANEL_SEG_BG), LV_PART_MAIN);
	lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
	lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
	lv_obj_t * label = lv_obj_get_child(btn, 0);
	if (label != NULL) {
		lv_obj_set_style_text_color(label, lv_color_hex(selected ? PANEL_DARK_TEXT : PANEL_SEG_TEXT), LV_PART_MAIN);
	}
}

/* Show the recorder's current mode on the pair */
static void mode_btns_sync(void)
{
	bool fft = (AUDIO_REC_GetMode() == AUDIO_REC_MODE_FFT);
	mode_btn_style(ui_uiModeSave, !fft);
	mode_btn_style(ui_uiModeFFT, fft);
}

static void mode_btn_cb(lv_event_t * e)
{
	if (AUDIO_REC_IsActive()) {
		ui_console_log("Stop the capture before changing mode");
		return;
	}
	lv_obj_t * btn = lv_event_get_target_obj(e);
	AUDIO_REC_SetMode(btn == ui_uiModeFFT ? AUDIO_REC_MODE_FFT : AUDIO_REC_MODE_SAVE);
	mode_btns_sync();
}

/* Colour map dropdown: matched by option text, so its order doesn't matter.
 * Also repaints the colour bar (ui_ColourMap) with the new map. */
static void colormap_apply(lv_obj_t * dd)
{
	char name[16];

	lv_dropdown_get_selected_str(dd, name, sizeof(name));
	spectrogram_set_colormap(spectrogram_colormap_from_name(name));
	colour_bar_refresh();
}

/* Repaint what's drawn from the colour table outside the spectrogram: the
 * colour bar and the bar chart. Also used by the contrast sliders. */
static void colour_bar_refresh(void)
{
	static lv_grad_dsc_t s_bar_grad; /* the style keeps a pointer to it */

	spectrum_view_refresh();
	spectrogram_colormap_gradient(&s_bar_grad);
	lv_obj_set_style_bg_grad(ui_ColourMap, &s_bar_grad, LV_PART_MAIN);
	lv_obj_invalidate(ui_ColourMap);
}

/* ui_ColourMap is a label in SquareLine; turn it into a 10 px rounded strip
 * under the dropdown, painted by colormap_apply() */
static void colour_bar_init(void)
{
	lv_obj_swap(ui_ColourMap, ui_Dropdown2);  /* SquareLine has it above the dropdown */
	lv_label_set_text(ui_ColourMap, "");
	lv_obj_set_size(ui_ColourMap, lv_pct(100), 10);
	lv_obj_set_style_pad_all(ui_ColourMap, 0, LV_PART_MAIN);
	lv_obj_set_style_radius(ui_ColourMap, 5, LV_PART_MAIN);
	lv_obj_set_style_bg_opa(ui_ColourMap, LV_OPA_COVER, LV_PART_MAIN);
}

static void colormap_changed_cb(lv_event_t * e)
{
	colormap_apply(lv_event_get_target_obj(e));
}

static void control_panel_init(void)
{
	/* Icons: LV_SYMBOL_* glyphs are in the built-in Montserrat fonts */
	lv_label_set_text(ui_Label11, LV_SYMBOL_PLAY "  Start");
	lv_label_set_text(ui_Label18, LV_SYMBOL_STOP "  Stop");
	lv_obj_set_style_text_color(ui_Label18, lv_color_hex(PANEL_REC), LV_PART_MAIN);

	lv_obj_add_event_cb(ui_Button7, start_btn_cb, LV_EVENT_CLICKED, NULL);
	lv_obj_add_event_cb(ui_Button6, stop_btn_cb, LV_EVENT_CLICKED, NULL);

	lv_obj_add_event_cb(ui_uiModeSave, mode_btn_cb, LV_EVENT_CLICKED, NULL);
	lv_obj_add_event_cb(ui_uiModeFFT, mode_btn_cb, LV_EVENT_CLICKED, NULL);
	mode_btns_sync();

	colour_bar_init();
	lv_obj_add_event_cb(ui_Dropdown2, colormap_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
	colormap_apply(ui_Dropdown2);

}

#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
