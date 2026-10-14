#include "audio_stream_fft.h"
#include <string.h>
#include <math.h>

/* Define global buffers */
float32_t Clean_Display_Bars[DISPLAY_BINS];

/* Internal DSP Static Variables.
 * Float throughout: the Q15 RFFT halves the signal at every stage (a 512-point
 * transform outputs in 9.7 format, i.e. 1/256 scale) and the Q15 magnitude
 * halves it again, so quiet mic audio rounded to 0 and every bar sat on the
 * noise floor. The M7's FPU runs the float FFT at about the same cost. */
static float32_t hann_window[FFT_LEN];
static float32_t fft_input[FFT_LEN];      /* windowed mono block; rfft_fast overwrites it */
static float32_t fft_output[FFT_LEN];     /* packed: [DC, Nyquist, re1, im1, re2, im2, ...] */
static float32_t power[NUM_RAW_BINS];     /* |X[k]|^2 */
static float32_t db_output[NUM_RAW_BINS];

static arm_rfft_fast_instance_f32 fft_handler;

/* Scales |X[k]|^2 so a full-scale sine reads 0 dBFS: amplitude = 4|X|/N
 * (Hann coherent gain 0.5, x2 for the one-sided spectrum), so power scale
 * = (4/N)^2. Input samples are scaled to +/-1.0 first. */
#define POWER_SCALE   (16.0f / ((float32_t)FFT_LEN * (float32_t)FFT_LEN))

/* Floor for the log: well below DB_FLOOR so it never shapes the display */
#define POWER_FLOOR   1e-14f   /* -140 dBFS */

void alternative_vlog_f32(const float *pSrc, float *pDst, uint32_t blockSize) {
    for (uint32_t i = 0; i < blockSize; i++) {
        pDst[i] = logf(pSrc[i]);
    }
}

void FFT_Pipeline_Init(void) {
    /* 1. Initialize CMSIS-DSP float Real FFT Engine */
    arm_rfft_fast_init_f32(&fft_handler, FFT_LEN);

    /* 2. Pre-calculate Hann Window to minimize live loop cycles */
    for (int i = 0; i < FFT_LEN; i++) {
        hann_window[i] = 0.5f * (1.0f - cosf(2.0f * PI * i / (FFT_LEN - 1)));
    }
}

/* One hop: FFT the FFT_LEN frames starting at stereo frame @p start of the
 * circular interleaved buffer @p buf (@p nframes frames long), and add each
 * bin's 0.0-1.0 bar value to @p acc. Indices wrap, so a window may straddle
 * the end of the buffer. */
static void fft_hop(const int16_t *buf, uint32_t nframes, uint32_t start, float32_t *acc) {

    /* Step 1+2: Extract left channel, scale to +/-1.0, apply Hann window */
    uint32_t f = start;
    for (int i = 0; i < FFT_LEN; i++) {
        int16_t s = buf[f * CHANNELS];
        fft_input[i] = ((float32_t)s * (1.0f / 32768.0f)) * hann_window[i];
        if (++f == nframes) f = 0;
    }

    /* Step 3: Real FFT (forward) */
    arm_rfft_fast_f32(&fft_handler, fft_input, fft_output, 0);

    /* Step 4: Power per bin. Bin 0 is packed with Nyquist in
     * fft_output[0..1], so take DC on its own. */
    arm_cmplx_mag_squared_f32(fft_output, power, NUM_RAW_BINS);
    power[0] = fft_output[0] * fft_output[0];

    /* Step 5: Scale to full-scale-sine = 1.0 and clamp away log(0) */
    for (int i = 0; i < NUM_RAW_BINS; i++) {
        float32_t p = power[i] * POWER_SCALE;
        power[i] = (p < POWER_FLOOR) ? POWER_FLOOR : p;
    }

    /* Step 6: dBFS = 10*log10(power) = ln(power) * (10 / ln 10) */
    alternative_vlog_f32(power, db_output, NUM_RAW_BINS);
    arm_scale_f32(db_output, 4.3429448f, db_output, NUM_RAW_BINS);

    /* Step 7: One bar per bin. Normalize [DB_FLOOR .. DB_CEIL] dBFS to
     * [0.0 .. 1.0] and accumulate. */
    for (uint16_t i = 0; i < DISPLAY_BINS; i++) {
        float32_t norm_val = (db_output[i] - DB_FLOOR) / (DB_CEIL - DB_FLOOR);
        if (norm_val < 0.0f) norm_val = 0.0f;
        if (norm_val > 1.0f) norm_val = 1.0f;
        acc[i] += norm_val;
    }
}

void FFT_Process_Audio_Block(int16_t *dma_start_ptr, uint32_t total_samples) {
    uint32_t mono_samples_available = total_samples / CHANNELS;

    /* Clear display buffer accumulator before pooling chunks */
    memset(Clean_Display_Bars, 0, sizeof(Clean_Display_Bars));
    uint32_t chunk_count = 0;

    /* Walk through the block via shifting HOP_SIZE chunks */
    for (uint32_t offset = 0; (offset + FFT_LEN) <= mono_samples_available; offset += HOP_SIZE) {
        fft_hop(dma_start_ptr, mono_samples_available, offset, Clean_Display_Bars);
        chunk_count++;
    }

    /* Average the aggregated blocks across the entire timeframe buffer */
    if (chunk_count > 0) {
        for (uint16_t i = 0; i < DISPLAY_BINS; i++) {
            Clean_Display_Bars[i] /= (float32_t)chunk_count;
        }
    }
}

/* --- Adaptive per-bin scaling ------------------------------------------- */

static float32_t s_adapt_fast[DISPLAY_BINS];  /* S: short-term average, dB */
static float32_t s_adapt_slow[DISPLAY_BINS];  /* L: long-term average, dB */
static uint8_t   s_adapt_seeded;

void FFT_Adaptive_Reset(void) {
    s_adapt_seeded = 0;
}

void FFT_Adaptive_Scale(float32_t *bars, uint32_t count) {
    /* Per-update smoothing factors for the two time constants:
     * alpha = 1 - exp(-dt / tau), dt = one update */
    static float32_t a_fast = -1.0f, a_slow;
    if (a_fast < 0.0f) {
        a_fast = 1.0f - expf(-(float32_t)FFT_REFRESH_MS / FFT_ADAPT_FAST_MS);
        a_slow = 1.0f - expf(-(float32_t)FFT_REFRESH_MS / FFT_ADAPT_SLOW_MS);
    }
    if (count > DISPLAY_BINS) count = DISPLAY_BINS;

    for (uint32_t i = 0; i < count; i++) {
        /* Back from the 0..1 bar scale to dBFS */
        float32_t db = DB_FLOOR + bars[i] * (DB_CEIL - DB_FLOOR);

        if (!s_adapt_seeded) {
            s_adapt_fast[i] = db;
            s_adapt_slow[i] = db;
        } else {
            s_adapt_fast[i] += a_fast * (db - s_adapt_fast[i]);
            s_adapt_slow[i] += a_slow * (db - s_adapt_slow[i]);
        }

        float32_t s = s_adapt_fast[i];
        float32_t rise = s - s_adapt_slow[i];   /* dB above this bin's norm */
        float32_t v;

        if (s < FFT_ADAPT_GATE_DB) {
            v = 0.0f;                             /* noise floor: keep dark */
        } else if (rise >= 0.0f) {
            v = FFT_ADAPT_BASE + (1.0f - FFT_ADAPT_BASE) * (rise / FFT_ADAPT_RANGE_DB);
        } else {
            v = FFT_ADAPT_BASE * (1.0f + rise / FFT_ADAPT_RANGE_DB);
        }
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        bars[i] = v;
    }
    s_adapt_seeded = 1;
}

/* --- Streaming: process hops as the DFSDM interrupt writes them --------- */

static uint32_t  s_read_frame;            /* first frame of the next FFT window */
static float32_t s_acc[DISPLAY_BINS];     /* bars summed since the last update */
static uint32_t  s_acc_hops;

void FFT_Stream_Reset(uint32_t write_pos) {
    s_read_frame = write_pos / CHANNELS;
    memset(s_acc, 0, sizeof(s_acc));
    s_acc_hops = 0;
    FFT_Adaptive_Reset();
}

uint8_t FFT_Stream_Process(const int16_t *buf, uint32_t buf_len, uint32_t write_pos) {
    uint32_t nframes     = buf_len / CHANNELS;
    uint32_t write_frame = (write_pos / CHANNELS) % nframes;
    uint8_t  updated     = 0;

    /* Frames written since s_read_frame, allowing for wrap-around */
    uint32_t avail = (write_frame + nframes - s_read_frame) % nframes;

    /* More than half a buffer behind (main loop stalled): jump to the newest
     * complete window instead of crunching stale audio, and restart the
     * update so it isn't a mix of old and new. */
    if (avail > nframes / 2) {
        s_read_frame = (write_frame + nframes - FFT_LEN) % nframes;
        avail = FFT_LEN;
        memset(s_acc, 0, sizeof(s_acc));
        s_acc_hops = 0;
    }

    while (avail >= FFT_LEN) {
        fft_hop(buf, nframes, s_read_frame, s_acc);
        s_read_frame = (s_read_frame + HOP_SIZE) % nframes;
        avail -= HOP_SIZE;

        if (++s_acc_hops >= FFT_HOPS_PER_REFRESH) {
            for (uint16_t i = 0; i < DISPLAY_BINS; i++) {
                Clean_Display_Bars[i] = s_acc[i] / (float32_t)s_acc_hops;
            }
#if FFT_ADAPT_ENABLE
            FFT_Adaptive_Scale(Clean_Display_Bars, DISPLAY_BINS);
#endif
            memset(s_acc, 0, sizeof(s_acc));
            s_acc_hops = 0;
            updated = 1;
        }
    }
    return updated;
}
