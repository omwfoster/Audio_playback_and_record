#include "spectrum_tab.h"
#include "lvgl.h"
#include "stm32f7xx_hal.h" // Requires STM32 CubeHAL for DMA2D functions
#include "stm32f769i_discovery_lcd.h" // LCD_FB_START_ADDRESS
#include "audio_stream_fft.h"         // SAMPLE_RATE (Nyquist for the bin frequencies)
#include <math.h>
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

/* History as levels (0-255), so a colour-map change can recolour all of it:
 * a ring of columns, one per SPECTROGRAM_COL_PX of canvas width, each
 * canvas_h bytes (row 0 = top). ~310 x 370 = ~115 KB on a tab-sized canvas,
 * in SDRAM 3 MB in, after the largest possible canvas buffer (750 KB at
 * 2 MB). Only the CPU touches it. */
#define LEVELS_BUF_ADDR   (LCD_FB_START_ADDRESS + 0x300000u)
static uint8_t * const levels_buffer = (uint8_t *)LEVELS_BUF_ADDR;
static uint32_t level_cols;    /* columns in the ring (covers the canvas width) */
static uint32_t level_head;    /* ring slot the next column goes in */
static uint32_t level_filled;  /* columns written so far, up to level_cols */

/* Bytes in use, rounded up to whole 32-byte cache lines for the invalidate
 * (this CMSIS doesn't round partial lines itself) */
static uint32_t canvas_buf_bytes(void) {
return (canvas_w * canvas_h * 2u + 31u) & ~31u;
}

/* DMA2D is shared with LVGL's DMA2D draw unit. That unit waits for each of
 * its transfers to finish before returning, and this file only runs from the
 * main loop outside lv_timer_handler(), so the two never overlap. */
static DMA2D_HandleTypeDef hdma2d;

static void redraw_from_levels(void);

/**
* Hardware-accelerated memory shift using the STM32 Chrom-ART (DMA2D) engine
*/
static void dma2d_shift_left(void) {
uint32_t bytes_per_pixel = 2; // RGB565 (lv_color_t is 3-byte RGB888 in LVGL 9)

// Source address: Row 0, Column SPECTROGRAM_COL_PX (skip the oldest columns)
uint32_t src_addr = (uint32_t)canvas_buffer + bytes_per_pixel * SPECTROGRAM_COL_PX;

// Destination address: Row 0, Column 0
uint32_t dst_addr = (uint32_t)canvas_buffer;

// Configure DMA2D for Memory-to-Memory transfer with offsets
hdma2d.Instance = DMA2D;
hdma2d.Init.Mode = DMA2D_M2M;
hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565;
hdma2d.Init.OutputOffset = SPECTROGRAM_COL_PX; // Skip the new columns at the end of every destination line

// Foreground configuration (Source configuration)
hdma2d.LayerCfg[1].InputAlpha = DMA2D_NO_MODIF_ALPHA;
hdma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_RGB565;
hdma2d.LayerCfg[1].InputOffset = SPECTROGRAM_COL_PX; // Skip the oldest columns at the start of every source line

HAL_DMA2D_Init(&hdma2d);
HAL_DMA2D_ConfigLayer(&hdma2d, 1);

// LVGL's DMA2D draw unit never clears the transfer-complete flag (it polls
// CR.START), so TC is usually still set from its last draw. Clear it, or
// HAL_DMA2D_PollForTransfer() returns at once while this copy is still
// running, and LVGL then reprograms DMA2D mid-copy: the rest of the canvas
// lands in LVGL's render buffer as a sheared copy inside the tab.
__HAL_DMA2D_CLEAR_FLAG(&hdma2d, DMA2D_FLAG_TC | DMA2D_FLAG_TE | DMA2D_FLAG_CE);

// Transfer size: (Width - SPECTROGRAM_COL_PX) wide, full height tall.
// Overlapping copy is safe: destination trails source.
HAL_DMA2D_Start(&hdma2d, src_addr, dst_addr, canvas_w - SPECTROGRAM_COL_PX, canvas_h);

// Block thread until hardware completes the block movement
HAL_DMA2D_PollForTransfer(&hdma2d, 100);

// Belt and braces: never leave with DMA2D still running
uint32_t t0 = HAL_GetTick();
while ((DMA2D->CR & DMA2D_CR_START) && (HAL_GetTick() - t0) < 100u) {
}

// DMA2D wrote SDRAM behind the D-cache: drop cached copies so the CPU (and
// LVGL's renderer) read the shifted image, not stale lines
SCB_InvalidateDCache_by_Addr((uint32_t *)canvas_buffer, (int32_t)canvas_buf_bytes());
}

/* Colour maps as control points (position 0-255, R, G, B), interpolated
 * linearly into a 256-entry table whenever the map changes. */
typedef struct { uint8_t pos, r, g, b; } cmap_stop_t;

static const cmap_stop_t cmap_jet[] = {
{0, 0, 0, 128}, {32, 0, 0, 255}, {96, 0, 255, 255},
{160, 255, 255, 0}, {224, 255, 0, 0}, {255, 128, 0, 0},
};
static const cmap_stop_t cmap_viridis[] = {
{0, 68, 1, 84}, {64, 59, 82, 139}, {128, 33, 145, 140},
{192, 94, 201, 98}, {255, 253, 231, 37},
};
static const cmap_stop_t cmap_magma[] = {
{0, 0, 0, 4}, {64, 81, 18, 124}, {128, 183, 55, 121},
{192, 252, 137, 97}, {255, 252, 253, 191},
};
static const cmap_stop_t cmap_plasma[] = {
{0, 13, 8, 135}, {64, 126, 3, 168}, {128, 204, 71, 120},
{192, 248, 149, 64}, {255, 240, 249, 33},
};

spectro_cmap_t spectrogram_colormap_from_name(const char * name) {
static const struct { const char * name; spectro_cmap_t cmap; } names[] = {
{"jet", SPECTRO_CMAP_JET}, {"viridis", SPECTRO_CMAP_VIRIDIS},
{"magma", SPECTRO_CMAP_MAGMA}, {"plasma", SPECTRO_CMAP_PLASMA},
};
if (name != NULL) {
for (uint32_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
const char * a = name;
const char * b = names[i].name;
while (*a && *b && ((*a | 0x20) == *b)) { a++; b++; } // ASCII case-insensitive
if (*a == '\0' && *b == '\0') {
return names[i].cmap;
}
}
}
return SPECTRO_CMAP_JET;
}

static lv_color_t cmap_lut[256];

/* Stops of the current map, for spectrogram_colormap_gradient() */
static const cmap_stop_t * cmap_cur_stops;
static uint32_t cmap_cur_n;
static uint32_t cmap_cur_id = SPECTRO_CMAP_JET;

/* Contrast curve, adjustable at run time (spectrogram_set_contrast) */
static float cmap_black = SPECTROGRAM_CMAP_BLACK;
static float cmap_gamma = SPECTROGRAM_CMAP_GAMMA;

/* Colour of a map at position @p pos (0.0-255.0), interpolating between
 * its stops */
static lv_color_t cmap_sample(const cmap_stop_t * stops, uint32_t n, float pos) {
uint32_t k = 0;
while (k + 2 < n && pos > (float)stops[k + 1].pos) {
k++;
}
const cmap_stop_t * a = &stops[k];
const cmap_stop_t * b = &stops[k + 1];
float span = (float)(b->pos - a->pos);
float t = (span > 0.0f) ? (pos - (float)a->pos) / span : 0.0f;
if (t < 0.0f) t = 0.0f;
if (t > 1.0f) t = 1.0f;
return lv_color_make(
(uint8_t)(a->r + ((float)b->r - a->r) * t),
(uint8_t)(a->g + ((float)b->g - a->g) * t),
(uint8_t)(a->b + ((float)b->b - a->b) * t));
}

void spectrogram_set_colormap(uint32_t cmap) {
const cmap_stop_t * stops;
uint32_t n;
switch (cmap) {
case SPECTRO_CMAP_VIRIDIS: stops = cmap_viridis; n = sizeof(cmap_viridis) / sizeof(cmap_viridis[0]); break;
case SPECTRO_CMAP_MAGMA:   stops = cmap_magma;   n = sizeof(cmap_magma) / sizeof(cmap_magma[0]);     break;
case SPECTRO_CMAP_PLASMA:  stops = cmap_plasma;  n = sizeof(cmap_plasma) / sizeof(cmap_plasma[0]);   break;
default:                   stops = cmap_jet;     n = sizeof(cmap_jet) / sizeof(cmap_jet[0]);         break;
}
cmap_cur_stops = stops;
cmap_cur_n = n;
cmap_cur_id = cmap;

for (uint32_t v = 0; v < 256; v++) {
// Contrast curve: black point, then gamma, then look up the map there
float x = ((float)v / 255.0f - cmap_black) / (1.0f - cmap_black);
if (x < 0.0f) x = 0.0f;
if (x > 1.0f) x = 1.0f;
x = powf(x, cmap_gamma);
cmap_lut[v] = cmap_sample(stops, n, x * 255.0f);
}

// Recolour the existing history (CPU writes; SDRAM is write-through)
if (canvas != NULL) {
redraw_from_levels();
lv_obj_invalidate(canvas);
}
}

/* Repaint the whole canvas from the level history in the current colour
 * map: newest column at the right edge, columns not yet filled black. */
static void redraw_from_levels(void) {
uint16_t lut565[256];
for (uint32_t v = 0; v < 256; v++) {
lut565[v] = lv_color_to_u16(cmap_lut[v]);
}

for (uint32_t x = 0; x < canvas_w; x++) {
// k = 0 for the newest column (right edge), counting leftwards
uint32_t k = (canvas_w - 1 - x) / SPECTROGRAM_COL_PX;
if (k >= level_filled) {
for (uint32_t y = 0; y < canvas_h; y++) {
canvas_buffer[y * canvas_w + x] = 0; // black
}
continue;
}
uint32_t slot = (level_head + level_cols - 1 - k) % level_cols;
const uint8_t * col = &levels_buffer[slot * canvas_h];
for (uint32_t y = 0; y < canvas_h; y++) {
canvas_buffer[y * canvas_w + x] = lut565[col[y]];
}
}
}

void spectrogram_colormap_gradient(lv_grad_dsc_t * grad) {
lv_color_t colors[LV_GRADIENT_MAX_STOPS];
uint8_t fracs[LV_GRADIENT_MAX_STOPS];
const uint32_t n = LV_GRADIENT_MAX_STOPS;

if (grad == NULL || cmap_cur_stops == NULL) {
return;
}
// Sample the finished colour table (contrast curve included) at evenly
// spaced levels, so the bar shows what the display actually uses
for (uint32_t i = 0; i < n; i++) {
uint32_t level = (i * 255u) / (n - 1);
colors[i] = cmap_lut[level];
fracs[i] = (uint8_t)level;
}
memset(grad, 0, sizeof(*grad));
lv_grad_init_stops(grad, colors, NULL, fracs, (int)n);
lv_grad_horizontal_init(grad);
}

void spectrogram_set_contrast(float black, float gamma) {
if (black < 0.0f) black = 0.0f;
if (black > 0.9f) black = 0.9f;
if (gamma < 0.2f) gamma = 0.2f;
if (gamma > 5.0f) gamma = 5.0f;
cmap_black = black;
cmap_gamma = gamma;
spectrogram_set_colormap(cmap_cur_id);  // rebuild the table, recolour history
}

void spectrogram_get_contrast(float * black, float * gamma) {
if (black) *black = cmap_black;
if (gamma) *gamma = cmap_gamma;
}

lv_color_t spectrogram_colormap(uint8_t level) {
return cmap_lut[level];
}

/**
* Maps FFT intensities (0-255) through the selected colour map
*/
static lv_color_t get_spectrogram_color(uint8_t magnitude) {
return cmap_lut[magnitude];
}

void init_spectrogram(lv_obj_t * parent) {
if (parent == NULL) {
parent = lv_screen_active();
}

// Default colour map until the dropdown picks one
spectrogram_set_colormap(SPECTRO_CMAP_JET);

// The SquareLine placeholder is a fixed 100x50: make it fill the tab, then
// lay out now so its real size can be read
lv_obj_set_size(parent, lv_pct(100), lv_pct(100));
lv_obj_update_layout(parent);

canvas_w = (uint32_t)lv_obj_get_content_width(parent);
canvas_h = (uint32_t)lv_obj_get_content_height(parent);
if (canvas_w <= SPECTROGRAM_COL_PX || canvas_h < 1) {
canvas_w = CANVAS_DEFAULT_WIDTH;
canvas_h = CANVAS_DEFAULT_HEIGHT;
}
if (canvas_w > CANVAS_MAX_WIDTH)  canvas_w = CANVAS_MAX_WIDTH;
if (canvas_h > CANVAS_MAX_HEIGHT) canvas_h = CANVAS_MAX_HEIGHT;

// One level column per SPECTROGRAM_COL_PX of width, rounded up so the ring
// covers a partial column at the left edge
level_cols   = (canvas_w + SPECTROGRAM_COL_PX - 1) / SPECTROGRAM_COL_PX;
level_head   = 0;
level_filled = 0;

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
level_head   = 0;
level_filled = 0;
}

/**
* Real-time hardware update loop for STM32F769 Spectrum
*/
void update_spectrogram_hardware(const uint8_t * fft_data) {
if (canvas == NULL) {
return;
}

// 1. Shift the canvas layout left by SPECTROGRAM_COL_PX instantly in hardware
dma2d_shift_left();

// 2. Insert the fresh time snapshot data into the right-most pixel columns
for (uint32_t y = 0; y < canvas_h; y++) {
uint16_t px = lv_color_to_u16(get_spectrogram_color(fft_data[y]));
// First of the new columns on this row
uint16_t * row = &canvas_buffer[(y * canvas_w) + (canvas_w - SPECTROGRAM_COL_PX)];
for (uint32_t c = 0; c < SPECTROGRAM_COL_PX; c++) {
row[c] = px;
}
}

// 3. Keep the levels too, so a colour-map change can recolour the history
memcpy(&levels_buffer[level_head * canvas_h], fft_data, canvas_h);
level_head = (level_head + 1) % level_cols;
if (level_filled < level_cols) level_filled++;

// No cache clean needed: SDRAM is write-through, so these writes are
// already in memory for LVGL's DMA2D image path

// 4. Force LVGL rendering pipe review
lv_obj_invalidate(canvas);
}

void spectrogram_push_bars(const float * bars, uint32_t count) {
static uint8_t column[CANVAS_MAX_HEIGHT];

if (canvas == NULL || bars == NULL || count < 3) {
return;
}

// Log frequency axis: walk up from the bottom row, multiplying the
// frequency by a constant ratio per row, so each octave gets equal height.
// Bin i is at i * hz_per_bin; interpolate between the two nearest bins.
const float hz_per_bin = ((float)SAMPLE_RATE / 2.0f) / (float)count;
const float ratio = (canvas_h > 1)
? powf(SPECTROGRAM_FREQ_MAX / SPECTROGRAM_FREQ_MIN, 1.0f / (float)(canvas_h - 1))
: 1.0f;
float freq = SPECTROGRAM_FREQ_MIN;

for (uint32_t r = 0; r < canvas_h; r++, freq *= ratio) {
float pos = freq / hz_per_bin;          // fractional bin index
uint32_t lo = (uint32_t)pos;
if (lo < 1) lo = 1;                      // never use the DC bin
if (lo > count - 2) lo = count - 2;
float w = pos - (float)lo;
if (w < 0.0f) w = 0.0f;
if (w > 1.0f) w = 1.0f;
float v = bars[lo] + w * (bars[lo + 1] - bars[lo]);

uint32_t y = canvas_h - 1 - r;           // row 0 is the top of the canvas
if (v < 0.0f) v = 0.0f;
if (v > 1.0f) v = 1.0f;
column[y] = (uint8_t)(v * 255.0f);
}
update_spectrogram_hardware(column);
}
