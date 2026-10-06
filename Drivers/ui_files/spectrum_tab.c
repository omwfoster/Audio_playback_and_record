#include "spectrum_tab.h"
#include "lvgl.h"
#include "stm32f7xx_hal.h" // Requires STM32 CubeHAL for DMA2D functions
#include "stm32f769i_discovery_lcd.h" // LCD_FB_START_ADDRESS
#include <string.h>

static lv_obj_t * canvas;

/* Canvas size in pixels, set by init_spectrogram() to fill the tab */
static uint32_t canvas_w;
static uint32_t canvas_h;

/* Canvas pixels, RGB565. Tab-sized (~620 x 370 x 2 = ~460 KB, up to 750 KB)
 * is far more than internal RAM has free (DTCM ~6 KB, SRAM ~100 KB after the
 * LVGL buffers), so it lives in the 16 MB external SDRAM, 2 MB in: clear of
 * the 800x480 framebuffer at LCD_FB_START_ADDRESS. SDRAM is write-through
 * cacheable (MPU_Config()), so CPU writes reach memory directly; DMA2D writes
 * need a cache invalidate. */
#define CANVAS_BUF_ADDR   (LCD_FB_START_ADDRESS + 0x200000u)
static uint16_t * const canvas_buffer = (uint16_t *)CANVAS_BUF_ADDR;

/* Bytes in use, rounded up to whole 32-byte cache lines for the invalidate
 * (this CMSIS doesn't round partial lines itself) */
static uint32_t canvas_buf_bytes(void) {
return (canvas_w * canvas_h * 2u + 31u) & ~31u;
}

/* DMA2D is shared with LVGL's DMA2D draw unit. That unit waits for each of
 * its transfers to finish before returning, and this file only runs from the
 * main loop outside lv_timer_handler(), so the two never overlap. */
static DMA2D_HandleTypeDef hdma2d;

/**
* Hardware-accelerated memory shift using the STM32 Chrom-ART (DMA2D) engine
*/
static void dma2d_shift_left(void) {
uint32_t bytes_per_pixel = 2; // RGB565 (lv_color_t is 3-byte RGB888 in LVGL 9)

// Source address: Row 0, Column 1 (skip first column)
uint32_t src_addr = (uint32_t)canvas_buffer + bytes_per_pixel;

// Destination address: Row 0, Column 0
uint32_t dst_addr = (uint32_t)canvas_buffer;

// Configure DMA2D for Memory-to-Memory transfer with offsets
hdma2d.Instance = DMA2D;
hdma2d.Init.Mode = DMA2D_M2M;
hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565;
hdma2d.Init.OutputOffset = 1; // Skip 1 pixel at the end of every destination line

// Foreground configuration (Source configuration)
hdma2d.LayerCfg[1].InputAlpha = DMA2D_NO_MODIF_ALPHA;
hdma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_RGB565;
hdma2d.LayerCfg[1].InputOffset = 1; // Skip 1 pixel at the end of every source line

HAL_DMA2D_Init(&hdma2d);
HAL_DMA2D_ConfigLayer(&hdma2d, 1);

// Transfer size: (Width - 1 pixel) wide, full height tall.
// Overlapping copy is safe: destination trails source by one pixel.
HAL_DMA2D_Start(&hdma2d, src_addr, dst_addr, canvas_w - 1, canvas_h);

// Block thread until hardware completes the block movement
HAL_DMA2D_PollForTransfer(&hdma2d, 100);

// DMA2D wrote SDRAM behind the D-cache: drop cached copies so the CPU (and
// LVGL's renderer) read the shifted image, not stale lines
SCB_InvalidateDCache_by_Addr((uint32_t *)canvas_buffer, (int32_t)canvas_buf_bytes());
}

/**
* Maps standard FFT intensities into a custom RGB565 heat profile
*/
static lv_color_t get_spectrogram_color(uint8_t magnitude) {
uint8_t r = 0, g = 0, b = 0;
if (magnitude < 85) {
b = magnitude * 3;
} else if (magnitude < 170) {
g = (magnitude - 85) * 3;
b = 255 - g;
} else {
r = (magnitude - 170) * 3;
g = 255 - r;
}
return lv_color_make(r, g, b);
}

void init_spectrogram(lv_obj_t * parent) {
if (parent == NULL) {
parent = lv_screen_active();
}

// The SquareLine placeholder is a fixed 100x50: make it fill the tab, then
// lay out now so its real size can be read
lv_obj_set_size(parent, lv_pct(100), lv_pct(100));
lv_obj_update_layout(parent);

canvas_w = (uint32_t)lv_obj_get_content_width(parent);
canvas_h = (uint32_t)lv_obj_get_content_height(parent);
if (canvas_w < 2 || canvas_h < 1) {
canvas_w = CANVAS_DEFAULT_WIDTH;
canvas_h = CANVAS_DEFAULT_HEIGHT;
}
if (canvas_w > CANVAS_MAX_WIDTH)  canvas_w = CANVAS_MAX_WIDTH;
if (canvas_h > CANVAS_MAX_HEIGHT) canvas_h = CANVAS_MAX_HEIGHT;

canvas = lv_canvas_create(parent);
lv_canvas_set_buffer(canvas, canvas_buffer, canvas_w, canvas_h, LV_COLOR_FORMAT_RGB565);
lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_COVER);
lv_obj_center(canvas);
}

void spectrogram_clear(void) {
if (canvas == NULL) {
return;
}
lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_COVER);
}

/**
* Real-time hardware update loop for STM32F769 Spectrum
*/
void update_spectrogram_hardware(const uint8_t * fft_data) {
if (canvas == NULL) {
return;
}

// 1. Shift the canvas layout left by 1 pixel instantly in hardware
dma2d_shift_left();

// 2. Insert the fresh time snapshot data into the right-most pixel column
for (uint32_t y = 0; y < canvas_h; y++) {
// Target the last column index of every vertical coordinate step
uint32_t last_col_index = (y * canvas_w) + (canvas_w - 1);
canvas_buffer[last_col_index] = lv_color_to_u16(get_spectrogram_color(fft_data[y]));
}

// 3. No cache clean needed: SDRAM is write-through, so these writes are
//    already in memory for LVGL's DMA2D image path

// 4. Force LVGL rendering pipe review
lv_obj_invalidate(canvas);
}

void spectrogram_push_bars(const float * bars, uint32_t count) {
static uint8_t column[CANVAS_MAX_HEIGHT];

if (canvas == NULL || bars == NULL || count == 0) {
return;
}

// Row 0 is the top of the canvas, so the highest bin goes there; each bin
// covers canvas_h / count rows
for (uint32_t y = 0; y < canvas_h; y++) {
uint32_t bin = (canvas_h - 1 - y) * count / canvas_h;
float v = bars[bin];
if (v < 0.0f) v = 0.0f;
if (v > 1.0f) v = 1.0f;
column[y] = (uint8_t)(v * 255.0f);
}
update_spectrogram_hardware(column);
}
