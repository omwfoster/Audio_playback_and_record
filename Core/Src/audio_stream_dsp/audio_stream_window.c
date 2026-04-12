////////////////////////
// window_functions_q15.c
////////////////////////

#include <audio_stream_dsp/audio_stream_window.h>
#include "arm_math.h"

// Q15 representation of pi: π ≈ 3.14159 → 32767 * (π/π) won't work
// Instead, we'll use angle representation where 32767 = π radians

#define Q15_ONE 32767
#define Q15_HALF 16384
#define Q15_PI 25736  // π in Q15 scaled to 0-2π range

// Helper: Fast Q15 cosine using ARM CMSIS-DSP
static inline q15_t cos_q15(q15_t angle) {
    // angle is in range [0, 32767] representing [0, 2π]
    // CMSIS arm_cos_q15 expects input in range [-1, 1] as [-32768, 32767]
    return arm_cos_q15(angle);
}

void hann_q15(q15_t *w, uint32_t n, bool sflag) {
    /**
     * Hann window in Q15 format
     * Original: w[i] = 0.5 - 0.5 * cos(2π * i / (n-1))
     * Q15: w[i] = 16384 - 16384 * cos_q15(angle[i])
     */

    if (n == 1) {
        w[0] = Q15_ONE;
        return;
    }

    const uint32_t wlength = sflag ? (n - 1) : n;

    for (uint32_t i = 0; i < n; i++) {
        // Calculate angle: 2π * i / wlength
        // In Q15 angle representation: 65536 * i / wlength
        uint32_t angle = (65536UL * i) / wlength;

        // cos_q15 expects input scaled to [-32768, 32767]
        q15_t angle_q15 = (q15_t)(angle >> 1);  // Scale to Q15 range

        q15_t cos_val = arm_cos_q15(angle_q15);

        // w[i] = 0.5 - 0.5 * cos_val
        // In Q15: w[i] = 16384 - ((16384 * cos_val) >> 15)
        q31_t temp = ((q31_t)16384 * cos_val) >> 15;
        w[i] = (q15_t)(16384 - temp);
    }
}

void hamming_q15(q15_t *w, uint32_t n, bool sflag) {
    /**
     * Hamming window in Q15 format
     * Original: w[i] = 0.54 - 0.46 * cos(2π * i / (n-1))
     * Q15 coefficients:
     *   0.54 → 17695
     *   0.46 → 15073
     */

    if (n == 1) {
        w[0] = Q15_ONE;
        return;
    }

    const uint32_t wlength = sflag ? (n - 1) : n;
    const q15_t coeff_0_54 = 17695;  // 0.54 in Q15
    const q15_t coeff_0_46 = 15073;  // 0.46 in Q15

    for (uint32_t i = 0; i < n; i++) {
        uint32_t angle = (65536UL * i) / wlength;
        q15_t angle_q15 = (q15_t)(angle >> 1);
        q15_t cos_val = arm_cos_q15(angle_q15);

        // w[i] = 0.54 - 0.46 * cos_val
        q31_t temp = ((q31_t)coeff_0_46 * cos_val) >> 15;
        w[i] = (q15_t)(coeff_0_54 - temp);
    }
}

void blackman_q15(q15_t *w, uint32_t n, bool sflag) {
    /**
     * Blackman window in Q15 format
     * Original: w[i] = 0.42 - 0.5*cos(2πi/N) + 0.08*cos(4πi/N)
     * Q15 coefficients:
     *   0.42 → 13763
     *   0.50 → 16384
     *   0.08 → 2621
     */

    if (n == 1) {
        w[0] = Q15_ONE;
        return;
    }

    const uint32_t wlength = sflag ? (n - 1) : n;
    const q15_t c0 = 13763;  // 0.42
    const q15_t c1 = 16384;  // 0.50
    const q15_t c2 = 2621;   // 0.08

    for (uint32_t i = 0; i < n; i++) {
        // First cosine term: cos(2πi/N)
        uint32_t angle1 = (65536UL * i) / wlength;
        q15_t cos1 = arm_cos_q15((q15_t)(angle1 >> 1));

        // Second cosine term: cos(4πi/N) = cos(2 * 2πi/N)
        uint32_t angle2 = (2 * angle1) & 0xFFFF;  // Keep in range
        q15_t cos2 = arm_cos_q15((q15_t)(angle2 >> 1));

        // w[i] = 0.42 - 0.5*cos1 + 0.08*cos2
        q31_t term1 = ((q31_t)c1 * cos1) >> 15;
        q31_t term2 = ((q31_t)c2 * cos2) >> 15;

        w[i] = (q15_t)(c0 - term1 + term2);
    }
}

void blackmanharris_q15(q15_t *w, uint32_t n, bool sflag) {
    /**
     * Blackman-Harris window in Q15 format
     * Coefficients: {0.35875, -0.48829, 0.14128, -0.01168}
     * Q15: {11755, -16002, 4630, -383}
     */

    if (n == 1) {
        w[0] = Q15_ONE;
        return;
    }

    const uint32_t wlength = sflag ? (n - 1) : n;
    const q15_t coeff[4] = {11755, 16002, 4630, 383};  // Note: using abs for c1 and c3

    for (uint32_t i = 0; i < n; i++) {
        q31_t wi = coeff[0];  // Start with c0

        for (uint32_t j = 1; j < 4; j++) {
            uint32_t angle = (j * 65536UL * i) / wlength;
            q15_t cos_val = arm_cos_q15((q15_t)(angle >> 1));

            q31_t term = ((q31_t)coeff[j] * cos_val) >> 15;

            // Apply sign: c1 and c3 are negative
            if (j == 1 || j == 3) {
                wi -= term;
            } else {
                wi += term;
            }
        }

        w[i] = (q15_t)wi;
    }
}

void rectwin_q15(q15_t *w, uint32_t n) {
    /**
     * Rectangular window - all ones
     */
    for (uint32_t i = 0; i < n; i++) {
        w[i] = Q15_ONE;
    }
}

void triang_q15(q15_t *w, uint32_t n) {
    /**
     * Triangular window
     * Original: w[i] = 1.0 - |2*i - (n-1)| / denominator
     */

    const uint32_t denominator = (n % 2 != 0) ? (n + 1) : n;

    for (uint32_t i = 0; i < n; i++) {
        // Calculate |2*i - (n-1)|
        int32_t diff = (int32_t)(2 * i) - (int32_t)(n - 1);
        if (diff < 0) diff = -diff;

        // w[i] = 1.0 - diff / denominator
        // In Q15: w[i] = 32767 - (32767 * diff) / denominator
        q31_t temp = ((q31_t)Q15_ONE * diff) / denominator;
        w[i] = (q15_t)(Q15_ONE - temp);
    }
}

void bartlett_q15(q15_t *w, uint32_t n) {
    /**
     * Bartlett window
     */

    if (n == 1) {
        w[0] = Q15_ONE;
        return;
    }

    const uint32_t denominator = n - 1;

    for (uint32_t i = 0; i < n; i++) {
        int32_t diff = (int32_t)(2 * i) - (int32_t)(n - 1);
        if (diff < 0) diff = -diff;

        q31_t temp = ((q31_t)Q15_ONE * diff) / denominator;
        w[i] = (q15_t)(Q15_ONE - temp);
    }
}


