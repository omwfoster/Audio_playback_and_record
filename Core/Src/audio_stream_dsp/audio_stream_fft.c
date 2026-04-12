/*
 * fft_module.c
 *
 *  Created on: Jun 10, 2025
 *      Author: oliverfoster
 */
#include "stdbool.h"
#include <stdlib.h>
#include <arm_math.h>
#include <audio_stream_dsp/audio_stream.h>
#include <audio_stream_dsp/audio_stream_fft.h>
#include <audio_stream_dsp/audio_stream_window.h>


int16_t  pcm_q15[FFT_SIZE];

extern q15_t fft_output[]; // FFT_SIZE * 2
q15_t abs_output[FFT_SIZE*2];
extern q15_t mag_bins[FFT_SIZE];
q15_t mag_bins_intermediate[FFT_SIZE];
extern q15_t mag_bins_output[FFT_SIZE];
q15_t mag_bins_previous[FFT_SIZE] = { [0 ... FFT_SIZE-1] = (q15_t)6533 };
q15_t mag_bins_new[FFT_SIZE];
q15_t db_output[FFT_SIZE];

q15_t windowed_samples_q15[FFT_SIZE];
q15_t window[FFT_SIZE];

q15_t pcm_samples[FFT_SIZE];

/* Generate Hann window coefficients in Q15 format
 * This function should be called once during initialization
 * @param window: output array to store window coefficients
 * @param size: window size (FFT_SIZE)
 */
void generate_hann_window_q15(q15_t *window, uint16_t size) {
	for (uint16_t i = 0; i < size; i++) {
		// Hann window: w[n] = 0.5 * (1 - cos(2*pi*n/(N-1)))
		// Convert to Q15: multiply by 32767 and round
		float w = 0.5f * (1.0f - cosf(2.0f * 3.14159265f * i / (size - 1)));
		window[i] = (q15_t) (w * 32767.0f + 0.5f);
	}
}

void convert_pcm_for_fft(uint16_t *pcm_data, int16_t *fft_input, uint32_t length) {
    for(uint32_t i = 0; i < length; i++) {
        // Convert unsigned to signed (subtract DC offset)
        fft_input[i] = (int16_t)(pcm_data[i] - 32768);
    }
}

void FFT_Window_Init() {

	//generate_hann_window_q15(hann_window, FFT_SIZE);
	hamming_q15(window, FFT_SIZE, true );

}

/**
 * Apply windowing function to PCM samples
 * @param pcm_samples: input PCM samples in Q15 format
 * @param windowed_samples: output windowed samples in Q15 format
 * @param window: window coefficients in Q15 format
 * @param size: number of samples (FFT_SIZE)
 */
void apply_window_q15(q15_t *pcm_samples,q15_t *windowed_samples,
		q15_t *window, uint16_t size) {
	arm_mult_q15(pcm_samples, window, windowed_samples, size);
}

void convert_magnitude_to_db_q15(q15_t *mag_input, q15_t *db_output, uint16_t length) {
    for(int i = 0; i < length; i++) {
        if(mag_input[i] > 0) {
            // Convert Q15 magnitude to float (Q15 range is -32768 to 32767)
            float32_t mag_float = (float32_t)mag_input[i] / 32768.0f;

            // Calculate dB (20*log10 for magnitude)
            float32_t db_float = 20.0f * log10f(mag_float);

            // Clamp to reasonable dB range
            if(db_float < -120.0f) db_float = -120.0f;
            if(db_float > 0.0f) db_float = 0.0f;  // 0 dB is max for normalized magnitude

            // Map dB range to Q15: -120dB → -32768, 0dB → 32767
            db_output[i] = (q15_t)((db_float + 120.0f) * 32767.0f / 120.0f - 32768.0f);
        } else {
            db_output[i] = -32768;  // Minimum Q15 value (represents -120 dB or silence)
        }
    }
}

// Before FFT, scale up your PCM samples to use full Q15 range
void FFT_Preprocess(int16_t *pcm_samples) {
    // Find peak value
    q15_t max_val = 0;
    for (int i = 0; i < FFT_SIZE; i++) {
        q15_t abs_val = abs(pcm_samples[i]);
        if (abs_val > max_val) max_val = abs_val;
    }

    // Calculate scaling factor (avoid division by zero)
    if (max_val < 100) max_val = 100;  // Minimum threshold

    // Scale factor to use most of Q15 range (leave headroom)
    int32_t scale = (int32_t)(16384 * 32767) / max_val;  // Target 50% of range

    // Apply scaling
    for (int i = 0; i < FFT_SIZE; i++) {
        int32_t scaled = ((int32_t)pcm_samples[i] * scale) >> 15;
        pcm_samples[i] = (q15_t)__SSAT(scaled, 16);
    }
}



void FFT_Test_Raw(int16_t *sample_block) {
	static arm_rfft_instance_q15 fft_instance;
	static bool fft_initialized = false;
	static q15_t temp_previous[FFT_SIZE];
	q15_t dc_value;
	arm_status status;

    if (!fft_initialized) {
        status = arm_rfft_init_q15(&fft_instance, FFT_SIZE, 0, 1);
        if (status != ARM_MATH_SUCCESS) {
            return;
        }
        fft_initialized = true;
    }

	arm_rfft_q15(&fft_instance, (q15_t*) sample_block, fft_output);
	arm_cmplx_mag_q15(fft_output, mag_bins_output, FFT_SIZE /2);

}


void FFT_Postprocess_Exponential(int16_t *sample_block) {
    static arm_rfft_instance_q15 fft_instance;
    static bool fft_initialized = false;
    static q15_t temp_previous[FFT_SIZE];

    arm_status status;
    q15_t dc_value;

    // Initialize FFT instance only once
    if (!fft_initialized) {
        status = arm_rfft_init_q15(&fft_instance, FFT_SIZE, 0, 1);
        if (status != ARM_MATH_SUCCESS) {
            return;
        }
        fft_initialized = true;
        memset(mag_bins_previous, 0, FFT_SIZE * sizeof(q15_t));
    }


    // Apply window and perform FFT
    apply_window_q15(sample_block, windowed_samples_q15, window, FFT_SIZE);
    arm_rfft_q15(&fft_instance, (q15_t*)windowed_samples_q15, fft_output);
    arm_cmplx_mag_q15(fft_output, mag_bins, FFT_SIZE);


    // Exponential averaging
    arm_scale_q15(mag_bins, (q15_t)10922, 0, mag_bins_new, FFT_SIZE);
    arm_scale_q15(mag_bins_previous, (q15_t)21845, 0, temp_previous, FFT_SIZE);
    arm_add_q15(mag_bins_new, temp_previous, mag_bins_output, FFT_SIZE);

    // Update previous
    memcpy(mag_bins_previous, mag_bins_output, FFT_SIZE * sizeof(q15_t));
}


void FFT_Postprocess_Adaptive(volatile int16_t *sample_block) {
    static arm_rfft_instance_q15 fft_instance;
    static bool fft_initialized = false;
    static q15_t temp_previous[FFT_SIZE];

    arm_status status;
    q15_t dc_value;



    // Initialize FFT instance only once
    if (!fft_initialized) {
        status = arm_rfft_init_q15(&fft_instance, FFT_SIZE, 0, 1);
        if (status != ARM_MATH_SUCCESS) {
            return;
        }
        fft_initialized = true;
        memset(mag_bins_previous, 0, FFT_SIZE * sizeof(q15_t));
    }



    arm_rfft_q15(&fft_instance, (q15_t*)sample_block, fft_output);
    arm_cmplx_mag_q15(fft_output, mag_bins_intermediate, FFT_SIZE * 2);


    for (int i = 0; i < FFT_SIZE / 2; i++) {
        int32_t scaled = (int32_t)mag_bins_intermediate[i] << 1;  // Left shift = multiply by 2
        mag_bins_intermediate[i] = (int16_t)__SSAT(scaled, 16);   // Saturate to prevent overflow
    }


    FFT_Adaptive_Averaging(mag_bins_intermediate, mag_bins_previous, mag_bins_output, FFT_SIZE) ;
    // Update previous
    memcpy(mag_bins_previous, mag_bins_output, FFT_SIZE * sizeof(q15_t));



}


void FFT_Postprocess_Adaptive_dB(int16_t *sample_block) {
    static arm_rfft_instance_q15 fft_instance;
    static bool fft_initialized = false;
    static q15_t temp_previous[FFT_SIZE];

    arm_status status;
    q15_t dc_value;

    // Initialize FFT instance only once
    if (!fft_initialized) {
        status = arm_rfft_init_q15(&fft_instance, FFT_SIZE, 0, 1);
        if (status != ARM_MATH_SUCCESS) {
            return;
        }
        fft_initialized = true;
        memset(mag_bins_previous, 0, FFT_SIZE * sizeof(q15_t));
    }


    // Apply window and perform FFT
    apply_window_q15(sample_block, windowed_samples_q15, window, FFT_SIZE);
    arm_rfft_q15(&fft_instance, (q15_t*)windowed_samples_q15, fft_output);
    arm_cmplx_mag_q15(fft_output, mag_bins, FFT_SIZE);
    convert_magnitude_to_db_q15(mag_bins, db_output , FFT_SIZE);
    FFT_Adaptive_Averaging_DB(db_output, mag_bins_previous, mag_bins_output, FFT_SIZE) ;
    // Update previous
    memcpy(mag_bins_previous, mag_bins_output, FFT_SIZE * sizeof(q15_t));

}

void convert_char(const audio_sample_t *s_16, q15_t *pcm, uint16_t num) {
// Convert your hex array to 16-bit samples
	for (int i = 0; i < num; i += 2) {
		int16_t sample = (int16_t) ((uint8_t) s_16[i]
				| ((uint8_t) s_16[i + 1] << 8));
		pcm[i / 2] = (q15_t) sample;
	}

}

void FFT_Test_440Hz_Tone(void) {
	static arm_rfft_instance_q15 fft_instance;

	arm_rfft_init_q15(&fft_instance, FFT_SIZE/*bin count*/,
			0/*forward FFT*/, 1/*output bit order is normal*/);
	arm_rfft_q15(&fft_instance, (q15_t*) test_440, fft_output);
	arm_cmplx_mag_q15(fft_output, mag_bins_output, FFT_SIZE);

}




q15_t calculate_threshold_fast(q15_t *mag_bins, uint32_t size) {
    q15_t max_val, min_val;
    uint32_t max_idx, min_idx;

    // Find min and max
    arm_max_q15(mag_bins, size, &max_val, &max_idx);
    arm_min_q15(mag_bins, size, &min_val, &min_idx);

    // Calculate mean
    q15_t sum;
    arm_mean_q15(mag_bins, size, &sum);
    q15_t mean_val = (q15_t)sum;

    // Threshold is between mean and max
    // threshold = mean + 0.5 * (max - mean)
    q15_t range = max_val - mean_val;
    q15_t threshold = mean_val + (range >> 1);  // Add half the range

    return threshold;
}


// Adjust averaging based on signal variance
void FFT_Adaptive_Averaging(q15_t *current, q15_t *previous, q15_t *output, uint32_t size) {
    // Calculate variance or energy

	static q15_t threshold = 5000;  // Initial guess
	threshold = calculate_threshold_fast(current, size);

	q63_t energy;
    arm_power_q15(current, size, &energy);

    // Adjust alpha based on energy (faster response for high energy)
    q15_t alpha = (energy > threshold) ? 16384 : 10922;  // 0.5 or 0.33
    q15_t one_minus_alpha = 32767 - alpha;

    static q15_t temp[FFT_SIZE];
    arm_scale_q15(current, alpha, 0, temp, size);
    arm_scale_q15(previous, one_minus_alpha, 0, output, size);
    arm_add_q15(temp, output, output, size);
}


q15_t calculate_threshold_fast_db(q15_t *db_bins, uint32_t size) {
    q15_t max_val, min_val;
    uint32_t max_idx, min_idx;

    // Find min and max dB values
    arm_max_q15(db_bins, size, &max_val, &max_idx);
    arm_min_q15(db_bins, size, &min_val, &min_idx);

    // Calculate mean dB
    q15_t mean_val;
    arm_mean_q15(db_bins, size, &mean_val);

    // Threshold is between mean and max
    // threshold = mean + 0.6 * (max - mean)
    // Higher multiplier for dB scale since dynamic range is compressed
    q15_t range = max_val - mean_val;
    q15_t threshold = mean_val + ((range * 19660) >> 15); // 0.6 in Q15

    return threshold;
}


// Asymmetric alpha: fast attack, slow decay
void FFT_Adaptive_Averaging_DB(q15_t *current_db, q15_t *previous_db, q15_t *output_db, uint32_t size) {
    static q15_t threshold = 0;
    threshold = calculate_threshold_fast(current_db, size);

    q15_t max_db;
    uint32_t max_idx;
    arm_max_q15(current_db, size, &max_db, &max_idx);

    static q15_t temp[FFT_SIZE];

    // Per-bin adaptive smoothing with asymmetric response
    for(uint32_t i = 0; i < size; i++) {
        q15_t diff = current_db[i] - previous_db[i];
        q15_t alpha;

        if(diff > 0) {
            // Signal increasing: FAST attack (respond quickly to rises)
            alpha = 26214; // 0.8 in Q15 - very fast
        } else {
            // Signal decreasing: SLOW decay (gradual fall-off)
            alpha = 3277; // 0.1 in Q15 - slow release
        }

        q15_t one_minus_alpha = 32767 - alpha;

        // output = alpha * current + (1-alpha) * previous
        q15_t temp1, temp2;
        temp1 = (q15_t)(((q31_t)current_db[i] * alpha) >> 15);
        temp2 = (q15_t)(((q31_t)previous_db[i] * one_minus_alpha) >> 15);
        output_db[i] = temp1 + temp2;
    }
}

