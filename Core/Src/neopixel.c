#include "main.h"



extern TIM_HandleTypeDef htim3; // Change to your active timer



#define NUM_LEDS        300    // Supports 10 or 10,000+ LEDs with the same RAM footprint!

#define BITS_PER_LED    24



#define NEOPIXEL_ZERO   40     // Match your ARR calculation (~33% duty cycle)

#define NEOPIXEL_ONE    90     // Match your ARR calculation (~66% duty cycle)



// Circular buffer for exactly 2 LEDs (2 * 24 = 48 half-words)

uint16_t ping_pong_buf[48];



// Tracking variables for the state machine

volatile uint16_t current_led_index = 0;

volatile uint8_t  is_transfer_active = 0;



// Your actual RGB color array (compact pixel memory: only 3 bytes per LED)

typedef struct {

    uint8_t r;

    uint8_t g;

    uint8_t b;

} NeoPixel_t;



NeoPixel_t led_strip[NUM_LEDS];



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

    HAL_TIM_PWM_Start_DMA(&htim3, TIM_CHANNEL_1, (uint32_t *)ping_pong_buf, 48);

}



// -------------------------------------------------------------------------

// DMA INTERRUPT CALLBACKS (Handled automatically by HAL)

// -------------------------------------------------------------------------



// Called when DMA completes transferring the first half (Elements 0-23 / LED 0)

void HAL_TIM_PWM_PulseFinishedHalfCpltCallback(TIM_HandleTypeDef *htim) {

    if (htim->Instance == TIM3) {

        // DMA is currently blasting the second half (LED 1)

        // We can safely rewrite the first half with the upcoming LED

        Fill_Buffer_Slot(current_led_index++, &ping_pong_buf[0]);



        // Push the update out of the Cortex-M7 D-Cache into physical RAM

        SCB_CleanDCache_by_Addr((uint32_t*)&ping_pong_buf[0], 24 * sizeof(uint16_t));



        // Terminate check: If we've processed all LEDs plus enough trailing safety buffers

        // to clear out the reset window (about 2 extra iterations), shut down the engine.

        if (current_led_index >= (NUM_LEDS + 3)) {

            HAL_TIM_PWM_Stop_DMA(&htim3, TIM_CHANNEL_1);

            is_transfer_active = 0;

        }

    }

}



// Called when DMA completes transferring the entire buffer (Elements 24-47 / LED 1)

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)


{}
