/*
 * bar_mapping.c
 *
 *  Created on: 22 Oct 2026
 *      Author: oliverfoster
 */



/* Bar	64 Bins (Index 0–63)	128 Bins (Index 0–127)	256 Bins (Index 0–255)	512 Bins (Index 0–511)
Bar 1	Bin 0	Bin 0	Bins 0 – 1	Bins 0 – 2
Bar 2	Bin 1	Bins 1 – 2	Bins 2 – 4	Bins 3 – 8
Bar 3	Bins 2 – 3	Bins 3 – 5	Bins 5 – 10	Bins 9 – 20
Bar 4	Bins 4 – 6	Bins 6 – 11	Bins 11 – 22	Bins 21 – 44
Bar 5	Bins 7 – 11	Bins 12 – 21	Bins 23 – 43	Bins 45 – 88
Bar 6	Bins 12 – 20	Bins 22 – 39	Bins 44 – 81	Bins 89 – 166
Bar 7	Bins 21 – 36	Bins 40 – 71	Bins 82 – 147	Bins 167 – 301
Bar 8	Bins 37 – 63	Bins 72 – 127	Bins 148 – 255	Bins 302 – 511

*/

#include "bar_mapping.h"
#include <math.h>

static BarMapping_Config_t bar_config[NUM_BARS];

/**
 * Run this ONCE at startup (e.g., inside your main initialization code).
 * Pre-calculates bin indexes, lengths, and hardware visual tuning weights.
 */
void BarMapping_Init(uint32_t total_bins) {
    uint32_t start_bin = 0;

    for (int b = 1; b <= NUM_BARS; b++) {
        double exponent = (double)b / NUM_BARS;
        uint32_t end_bin = (uint32_t)round(pow((double)total_bins / 2.0, exponent) * 2.0) - 2;

        if (b == NUM_BARS || end_bin >= total_bins) {
            end_bin = total_bins - 1;
        }
        if (end_bin < start_bin) {
            end_bin = start_bin;
        }

        bar_config[b - 1].start_idx = start_bin;
        bar_config[b - 1].length = (end_bin - start_bin) + 1;

        /*
         * VISUAL EQ CORRECTION (Hardware Optimization)
         * Higher bands contain more bins and experience human hearing attenuation.
         * This applies a gentle progression scaling from 1.0x (Bass) up to ~2.5x (Treble).
         * Adjust the 1.5f factor to make higher bars more or less sensitive.
         */
        bar_config[b - 1].eq_gain = 1.0f + (1.5f * ((float32_t)(b - 1) / (NUM_BARS - 1)));

        start_bin = end_bin + 1;
    }
}

/**
 * Process your magnitude spectrum at blazing-fast FPU speed.
 * Call this immediately after your arm_cmplx_mag_f32 function.
 */
void BarMapping_Process(const float32_t *fft_mag_array, float32_t *out_bars) {
    uint32_t unused_idx;
    float32_t peak_val;

    for (int b = 0; b < NUM_BARS; b++) {
        // Safe access configurations via zero-overhead pointer indexing
        uint32_t offset    = bar_config[b].start_idx;
        uint32_t block_sz  = bar_config[b].length;
        float32_t gain     = bar_config[b].eq_gain;

        // Vector FPU execution: extracts peak transient element across the bin subset block
        arm_max_f32(&fft_mag_array[offset], block_sz, &peak_val, &unused_idx);

        // Apply visual weight balancing to the single vector output
        out_bars[b] = peak_val * gain;
    }
}
