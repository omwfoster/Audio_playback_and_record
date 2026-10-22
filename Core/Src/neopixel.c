#include "main.h"
#include "neopixel.h"
#include <string.h>



extern TIM_HandleTypeDef htim3; // defined in main.c

/* Hardware: Adafruit 5x8 NeoPixel shield, data re-routed from Arduino D6
 * (PF7 = TIM11, which has no DMA) to Arduino D5 = PC8 = TIM3_CH3 (AF2).
 * TIM3_CH3 DMA request: DMA1 Stream7, channel 5 (free in this project). */
#define NEOPIXEL_TIM_CHANNEL   TIM_CHANNEL_3
#define NEOPIXEL_GPIO_PORT     GPIOC
#define NEOPIXEL_GPIO_PIN      GPIO_PIN_8
#define NEOPIXEL_GPIO_AF       GPIO_AF2_TIM3
#define NEOPIXEL_DMA_STREAM    DMA1_Stream7
#define NEOPIXEL_DMA_CHANNEL   DMA_CHANNEL_5
#define NEOPIXEL_DMA_IRQn      DMA1_Stream7_IRQn

/* Each LED's refill must happen within 30 us (24 bits x 1.25 us), so this
 * DMA interrupt pre-empts the DFSDM audio interrupts (priority 0x0E), whose
 * sample interleaving loops can run for about that long. */
#define NEOPIXEL_DMA_IRQ_PRIO  4

/* Zero slots (30 us each) sent after the last LED: the latch/reset gap.
 * 10 = 300 us, enough for newer WS2812B (V5) parts that need > 280 us. */
#define NEOPIXEL_RESET_SLOTS   10

static DMA_HandleTypeDef hdma_tim3_ch3;

/* Diagnostics (NeoPixel_GetDiag): HAL results and callback counts */
static NeoPixel_Diag_t s_diag;




#define BITS_PER_LED    24



/* PWM period and high times, in timer counts, worked out from the real
 * timer clock in NeoPixel_Init() (96 MHz here: period 120, 0 = 38, 1 = 77).
 * WS2812: 1.25 us bit, '0' high ~0.4 us, '1' high ~0.8 us. */
static uint16_t s_period;
#define NEOPIXEL_ZERO   s_t0h  // ~32% duty
static uint16_t s_t0h;

#define NEOPIXEL_ONE    s_t1h  // ~64% duty
static uint16_t s_t1h;



// Circular buffer for exactly 2 LEDs (2 * 24 = 48 half-words)

// In .dma_buffers (non-cacheable SRAM1): DMA1 reads it, and it can't live
// in DTCM where ordinary statics go.
__attribute__((section(".dma_buffers"), aligned(4))) uint16_t ping_pong_buf[48];



// Tracking variables for the state machine

volatile uint16_t current_led_index = 0;

volatile uint8_t  is_transfer_active = 0;



// Your actual RGB color array (compact pixel memory: only 3 bytes per LED)








// Helper: Converts a single pixel's RGB data directly into 24-bit PWM duty cycles

// target_ptr must point to either &ping_pong_buf[0] or &ping_pong_buf[24]

static inline void Fill_Buffer_Slot(uint16_t led_idx, uint16_t *target_ptr) {

    // Handle the trailer: stream completely dead cycles for the >50us Reset state

    if (led_idx >= NUM_LEDS) {

        for (int i = 0; i < BITS_PER_LED; i++) {

            target_ptr[i] = 0;

        }

        return;

    }



    // NeoPixel uses GRB ordering

    uint32_t color = (led_strip[led_idx].g << 16) | (led_strip[led_idx].r << 8) | led_strip[led_idx].b;



    for (int i = 0; i < BITS_PER_LED; i++) {

        if (color & (1 << (23 - i))) {

            target_ptr[i] = NEOPIXEL_ONE;

        } else {

            target_ptr[i] = NEOPIXEL_ZERO;

        }

    }

}



// Set up PC8 (AF2), TIM3 CH3 PWM at 800 kHz and DMA1 Stream7 (circular,
// half-words into CCR3). Call once at startup, before NeoPixel_Show().
void NeoPixel_Init(void) {
    GPIO_InitTypeDef gpio = {0};
    TIM_OC_InitTypeDef oc = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();
    __HAL_RCC_DMA1_CLK_ENABLE();

    // Pull-down keeps the line low (= reset) whenever the PWM is stopped
    gpio.Pin       = NEOPIXEL_GPIO_PIN;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_PULLDOWN;
    gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = NEOPIXEL_GPIO_AF;
    HAL_GPIO_Init(NEOPIXEL_GPIO_PORT, &gpio);

    // APB1 timers run at 2 x PCLK1 when the APB1 prescaler isn't 1
    uint32_t tim_clk = HAL_RCC_GetPCLK1Freq();
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_HCLK_DIV1) {
        tim_clk *= 2u;
    }
    s_period = (uint16_t)(tim_clk / 800000u);           // 1.25 us
    s_t0h    = (uint16_t)((s_period * 32u) / 100u);     // ~0.40 us
    s_t1h    = (uint16_t)((s_period * 64u) / 100u);     // ~0.80 us

    htim3.Instance               = TIM3;
    htim3.Init.Prescaler         = 0;
    htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim3.Init.Period            = s_period - 1u;
    htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    s_diag.st_pwm_init = (uint8_t)HAL_TIM_PWM_Init(&htim3);

    // PWM1 with CCR preload: each DMA write takes effect on the next bit
    oc.OCMode     = TIM_OCMODE_PWM1;
    oc.Pulse      = 0;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    s_diag.st_config = (uint8_t)HAL_TIM_PWM_ConfigChannel(&htim3, &oc, NEOPIXEL_TIM_CHANNEL);

    hdma_tim3_ch3.Instance                 = NEOPIXEL_DMA_STREAM;
    hdma_tim3_ch3.Init.Channel             = NEOPIXEL_DMA_CHANNEL;
    hdma_tim3_ch3.Init.Direction           = DMA_MEMORY_TO_PERIPH;
    hdma_tim3_ch3.Init.PeriphInc           = DMA_PINC_DISABLE;
    hdma_tim3_ch3.Init.MemInc              = DMA_MINC_ENABLE;
    hdma_tim3_ch3.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_tim3_ch3.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
    hdma_tim3_ch3.Init.Mode                = DMA_CIRCULAR;
    hdma_tim3_ch3.Init.Priority            = DMA_PRIORITY_HIGH;
    hdma_tim3_ch3.Init.FIFOMode            = DMA_FIFOMODE_DISABLE;
    s_diag.st_dma_init = (uint8_t)HAL_DMA_Init(&hdma_tim3_ch3);
    __HAL_LINKDMA(&htim3, hdma[TIM_DMA_ID_CC3], hdma_tim3_ch3);

    HAL_NVIC_SetPriority(NEOPIXEL_DMA_IRQn, NEOPIXEL_DMA_IRQ_PRIO, 0);
    HAL_NVIC_EnableIRQ(NEOPIXEL_DMA_IRQn);
}

void DMA1_Stream7_IRQHandler(void) {
    HAL_DMA_IRQHandler(&hdma_tim3_ch3);
}

// Kick off the data transmission

void NeoPixel_Show(void) {

    if (is_transfer_active) return; // Prevent overlapping triggers



    is_transfer_active = 1;

    current_led_index = 0;



    // Prime the pump: Fill both halves of the buffer with LED 0 and LED 1

    Fill_Buffer_Slot(current_led_index++, &ping_pong_buf[0]);

    Fill_Buffer_Slot(current_led_index++, &ping_pong_buf[24]);



    // Clean D-Cache so DMA reads the freshly written memory layout

    SCB_CleanDCache_by_Addr((uint32_t*)ping_pong_buf, sizeof(ping_pong_buf));



    // Start Timer PWM in Circular DMA mode

    s_diag.shows++;
    s_diag.st_start = (uint8_t)HAL_TIM_PWM_Start_DMA(&htim3, NEOPIXEL_TIM_CHANNEL, (uint32_t *)ping_pong_buf, 48);
    if (s_diag.st_start != HAL_OK) {
        is_transfer_active = 0;   // never started: don't stay busy forever
    }

}



// -------------------------------------------------------------------------

// DMA INTERRUPT CALLBACKS (Handled automatically by HAL)

// -------------------------------------------------------------------------



uint8_t NeoPixel_IsBusy(void) {
    return is_transfer_active;
}

// Send led_strip and wait for it to go out (max ~20 ms, then give up)
static void show_and_wait(void) {
    uint32_t t0 = HAL_GetTick();
    while (NeoPixel_IsBusy() && (HAL_GetTick() - t0) < 20u) { }
    NeoPixel_Show();
    t0 = HAL_GetTick();
    while (NeoPixel_IsBusy() && (HAL_GetTick() - t0) < 20u) { }
}

// Wiring / power check, ~1.5 s, blocking. 1) one LED chases through the
// indices 0..NUM_LEDS-1 (red at index 0, shading to blue at the end), so
// you can see where index 0 is and which way rows run; 2) all LEDs dim
// white for 300 ms; 3) all off.
void NeoPixel_SelfTest(uint8_t brightness) {
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
        memset(led_strip, 0, sizeof(led_strip));
        led_strip[i].r = (uint8_t)((brightness * (NUM_LEDS - 1u - i)) / (NUM_LEDS - 1u));
        led_strip[i].b = (uint8_t)((brightness * i) / (NUM_LEDS - 1u));
        show_and_wait();
        HAL_Delay(25);
    }
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
        led_strip[i].r = led_strip[i].g = led_strip[i].b = brightness / 2u;
    }
    show_and_wait();
    HAL_Delay(300);
    memset(led_strip, 0, sizeof(led_strip));
    show_and_wait();
}

// Called when DMA completes transferring the first half (Elements 0-23 / LED 0)

void HAL_TIM_PWM_PulseFinishedHalfCpltCallback(TIM_HandleTypeDef *htim) {

    if (htim->Instance == TIM3) {
        s_diag.half_cb++;

        // DMA is currently blasting the second half (LED 1)

        // We can safely rewrite the first half with the upcoming LED

        Fill_Buffer_Slot(current_led_index++, &ping_pong_buf[0]);



        // Push the update out of the Cortex-M7 D-Cache into physical RAM

        SCB_CleanDCache_by_Addr((uint32_t*)&ping_pong_buf[0], 24 * sizeof(uint16_t));



        // Terminate check: If we've processed all LEDs plus enough trailing safety buffers

        // to clear out the reset window (about 2 extra iterations), shut down the engine.

        if (current_led_index >= (NUM_LEDS + NEOPIXEL_RESET_SLOTS)) {

            HAL_TIM_PWM_Stop_DMA(&htim3, NEOPIXEL_TIM_CHANNEL);

            is_transfer_active = 0;

        }

    }

}



// Called when DMA completes transferring the entire buffer (Elements 24-47 / LED 1)

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)



{

    if (htim->Instance == TIM3) {
        s_diag.full_cb++;

        // DMA has looped around and is blasting the first half again

        // We can safely rewrite the second half with the upcoming LED

        Fill_Buffer_Slot(current_led_index++, &ping_pong_buf[24]);



        // Push the update out of the Cortex-M7 D-Cache into physical RAM

        SCB_CleanDCache_by_Addr((uint32_t*)&ping_pong_buf[24], 24 * sizeof(uint16_t));

    }

}

void NeoPixel_GetDiag(NeoPixel_Diag_t *d) {
    *d = s_diag;
    d->tim_cen   = (TIM3->CR1 & TIM_CR1_CEN) ? 1 : 0;
    d->tim_cc3e  = (TIM3->CCER & TIM_CCER_CC3E) ? 1 : 0;
    d->tim_cc3de = (TIM3->DIER & TIM_DIER_CC3DE) ? 1 : 0;
    d->tim_arr   = TIM3->ARR;
    d->dma_en    = (NEOPIXEL_DMA_STREAM->CR & DMA_SxCR_EN) ? 1 : 0;
    d->dma_ndtr  = NEOPIXEL_DMA_STREAM->NDTR;
    d->dma_lisr_hisr = DMA1->HISR;
    d->pc8_mode  = (GPIOC->MODER >> (8u * 2u)) & 3u;       // 2 = alternate function
    d->pc8_af    = (GPIOC->AFR[1] >> ((8u - 8u) * 4u)) & 0xFu; // 2 = TIM3
    d->busy      = is_transfer_active;
    d->t0h = s_t0h; d->t1h = s_t1h; d->period = s_period;
}
