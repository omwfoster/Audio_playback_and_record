#ifndef BAR_MAPPING_H
#define BAR_MAPPING_H

#include "arm_math.h"

#define NUM_BARS 8

// Struct to store zero-overhead boundary configurations
typedef struct {
    uint32_t start_idx;
    uint32_t length;
    float32_t eq_gain;  // Pre-calculated gain multiplier to balance visual spectrum
} BarMapping_Config_t;

// Public API
void BarMapping_Init(uint32_t total_bins);
void BarMapping_Process(const float32_t *fft_mag_array, float32_t *out_bars);

#endif // BAR_MAPPING_H
