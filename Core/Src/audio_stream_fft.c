#include "audio_stream_fft.h"
#include <string.h>
#include <math.h>



/* Define global buffers */


float32_t Clean_Display_Bars[DISPLAY_BINS];



/* Internal DSP Static Variables */

static q15_t mono_block[FFT_LEN];

static q15_t window_block[FFT_LEN];

static q15_t fft_input[FFT_LEN * 2];       /* Complex layout sizing requirement */

static q15_t fft_output_q15[FFT_LEN];

static q15_t hann_window_q15[FFT_LEN];



static float32_t float_magnitude[NUM_RAW_BINS];

static float32_t db_output[NUM_RAW_BINS];



static arm_rfft_instance_q15 fft_handler;

static InterpolationMap_t Lerp_LUT[DISPLAY_BINS];

void alternative_vlog_f32(const float *pSrc, float *pDst, uint32_t blockSize) {
    for (uint32_t i = 0; i < blockSize; i++) {
        pDst[i] = logf(pSrc[i]);
    }
}



void FFT_Pipeline_Init(void) {

    /* 1. Initialize CMSIS-DSP Q15 Real FFT Engine */

    arm_rfft_init_q15(&fft_handler, FFT_LEN, 0, 1);



    /* 2. Pre-calculate Q15 Hann Window to minimize live loop cycles */

    for (int i = 0; i < FFT_LEN; i++) {

        float32_t val = 0.5f * (1.0f - cosf(2.0f * PI * i / (FFT_LEN - 1)));

        arm_float_to_q15(&val, &hann_window_q15[i], 1);

    }



    /* 3. Pre-calculate the Linear Interpolation Map (743 raw bins -> 128 view slots) */

    float32_t index_start = FREQ_MIN / BIN_RESOLUTION;

    float32_t index_end   = FREQ_MAX / BIN_RESOLUTION;

    float32_t index_span  = index_end - index_start;



    for (uint16_t i = 0; i < DISPLAY_BINS; i++) {

        float32_t exact_index = index_start + (((float32_t)i / (DISPLAY_BINS - 1)) * index_span);



        uint32_t idx_floor = (uint32_t)exact_index;

        uint32_t idx_ceil  = idx_floor + 1;



        /* Bound safety checks */

        if (idx_floor >= NUM_RAW_BINS) idx_floor = NUM_RAW_BINS - 1;

        if (idx_ceil  >= NUM_RAW_BINS) idx_ceil  = NUM_RAW_BINS - 1;



        Lerp_LUT[i].index_low  = idx_floor;

        Lerp_LUT[i].index_high = idx_ceil;

        Lerp_LUT[i].weight     = exact_index - (float32_t)idx_floor;

    }

}



void FFT_Process_Audio_Block(int16_t *dma_start_ptr, uint32_t total_samples) {

    uint32_t mono_samples_available = total_samples / CHANNELS;

    q15_t *audio_pool = (q15_t *)dma_start_ptr;



    /* Clear display buffer accumulator before pooling chunks */

    memset(Clean_Display_Bars, 0, sizeof(Clean_Display_Bars));

    uint32_t chunk_count = 0;



    /* Walk through the 500ms half-buffer via shifting HOP_SIZE chunks */

    for (uint32_t offset = 0; (offset + FFT_LEN) <= mono_samples_available; offset += HOP_SIZE) {



        /* Step 1: Extract Mono Channel from DFSDM Interleaved Data Stream */

        for (int i = 0; i < FFT_LEN; i++) {

            int dma_index = (offset + i) * CHANNELS;

            mono_block[i] = audio_pool[dma_index]; /* Extracts left channel */

        }



        /* Step 2: Vector Multiply with Hann Window (Leverages Dual-SIMD instructions) */

        arm_mult_q15(mono_block, hann_window_q15, window_block, FFT_LEN);



        /* Step 3: Compute Hardware Real Q15 FFT */

        arm_rfft_q15(&fft_handler, window_block, fft_input);



        /* Step 4: Extract Magnitude array into raw Q15 categories */

        arm_cmplx_mag_q15(fft_input, fft_output_q15, NUM_RAW_BINS);



        /* Step 5: Fast Translation of current magnitude block to Float [0.0 to 1.0] */

        arm_q15_to_float(fft_output_q15, float_magnitude, NUM_RAW_BINS);



        /* Step 6: Vector Log Scaling Phase (dB Conversion) */

        /* Avoid log(0) crashing to negative infinity by setting an lower clamp limit */

        for(int i = 0; i < NUM_RAW_BINS; i++) {

            if(float_magnitude[i] < 1e-5f) {

                float_magnitude[i] = 1e-5f; /* Hard noise floor floor at approx -100dB */

            }

        }





        alternative_vlog_f32(float_magnitude, db_output, NUM_RAW_BINS);



        /* Multiply natural log by scalar constant (20 / ln(10)) to convert to 20*log10 scale */

        float32_t log10_scaling_factor = 8.6858896f;

        arm_scale_f32(db_output, log10_scaling_factor, db_output, NUM_RAW_BINS);



        /* Step 7: Compress and Interpolate down into 128 Display bins */

        for (uint16_t i = 0; i < DISPLAY_BINS; i++) {

            uint32_t low_idx  = Lerp_LUT[i].index_low;

            uint32_t high_idx = Lerp_LUT[i].index_high;

            float32_t w       = Lerp_LUT[i].weight;



            float32_t db_low  = db_output[low_idx];

            float32_t db_high = db_output[high_idx];



            /* Core linear interpolation equation */

            float32_t interpolated_db = db_low + w * (db_high - db_low);



            /* Normalize dB range [-100.0f to 0.0f] to positive value bounds [0.0 to 1.0] */

            float32_t norm_val = (interpolated_db + 100.0f) / 100.0f;

            if (norm_val < 0.0f) norm_val = 0.0f;

            if (norm_val > 1.0f) norm_val = 1.0f;



            /* Accumulate data over all 500ms block chunks */

            Clean_Display_Bars[i] += norm_val;

        }

        chunk_count++;

    }



    /* Average the aggregated blocks across the entire timeframe buffer */

    if (chunk_count > 0) {

        for(uint16_t i = 0; i < DISPLAY_BINS; i++) {

            Clean_Display_Bars[i] /= (float32_t)chunk_count;

        }

    }

}







