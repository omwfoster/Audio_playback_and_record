#ifndef INC_FFT_PROCESSING_H_

#define INC_FFT_PROCESSING_H_



#include "main.h"

#include "arm_math.h"



/* --- Configuration Framework --- */

#define SAMPLE_RATE          16000  /* must match DEFAULT_AUDIO_IN_FREQ */

#define CHANNELS             2  /* Stereo layout */

/* Input comes straight from the recorder's capture buffer,
 * BufferCtl_In.pcm_buff: AUDIO_IN_PCM_BUFFER_SIZE half-words, interleaved
 * stereo, processed one half (512 ms) at a time by AUDIO_REC_Process(). */



/* FFT Engine Variables */

/* FFT_LEN sets both the FFT length and the number of output bins/bars:
 * one bar per frequency bin, FFT_LEN / 2 of them, each
 * SAMPLE_RATE / FFT_LEN Hz wide, covering 0 .. SAMPLE_RATE / 2.
 * Must be a power of two from 32 to 4096 (arm_rfft_fast_f32).
 *   e.g. 64 -> 32 bars of 250 Hz;  512 -> 256 bars of 31.25 Hz */
#define FFT_LEN              256

#define NUM_RAW_BINS         (FFT_LEN / 2)  /* unique frequency bins */

#define HOP_SIZE             (FFT_LEN / 2)  /* 50% Overlap window */

#define BIN_RESOLUTION       ((float32_t)SAMPLE_RATE / FFT_LEN)  /* Hz per bin */

#if (FFT_LEN < 32) || (FFT_LEN > 4096) || ((FFT_LEN & (FFT_LEN - 1)) != 0)
#error "FFT_LEN must be a power of two from 32 to 4096"
#endif



/* Display Visual Target Parameters */

/* How often the bars update when streaming (FFT_Stream_Process): every
 * FFT_HOPS_PER_REFRESH hops, i.e. roughly every FFT_REFRESH_MS. Each update
 * averages the hops since the last one. 100 ms matches LV_DEF_REFR_PERIOD. */
#define FFT_REFRESH_MS       100
#define FFT_HOPS_PER_REFRESH ((FFT_REFRESH_MS * SAMPLE_RATE / 1000 + HOP_SIZE / 2) / HOP_SIZE > 0 \
                              ? (FFT_REFRESH_MS * SAMPLE_RATE / 1000 + HOP_SIZE / 2) / HOP_SIZE : 1)

#define DISPLAY_BINS         NUM_RAW_BINS  /* one bar per FFT bin */

#define FREQ_MAX             (SAMPLE_RATE / 2.0f)  /* Nyquist: 8 kHz, top of the X axis */

/* Bar range in dBFS (0 dBFS = full-scale sine). Bars are 0 at DB_FLOOR and
 * full height at DB_CEIL. Narrow the range to make quiet sounds taller:
 * e.g. DB_FLOOR -90, DB_CEIL -30 for speech on the MEMS mics. */
#define DB_FLOOR             (-100.0f)
#define DB_CEIL              (0.0f)



/* --- External Buffer Declaration --- */

/* Output of FFT_Process_Audio_Block(): one 0.0-1.0 value per display bar */
extern float32_t Clean_Display_Bars[DISPLAY_BINS];



/* --- Function Prototypes --- */

/**

* @brief  Initializes the FFT engine, computes the Hann window,

 *         and builds the linear interpolation lookup table.

*/

void FFT_Pipeline_Init(void);



/**

* @brief  Processes an entire half-buffer of raw audio data.

*         Iterates over sliding windows, calculates FFTs, applies decibel

 *         scaling, and linear-interpolates down to 128 normalized bins.

* @param  dma_start_ptr: Pointer to the start of the ready half-buffer block.

* @param  total_samples: Total number of stereo samples contained in the block.

*/

void FFT_Process_Audio_Block(int16_t *dma_start_ptr, uint32_t total_samples);


/**
* @brief  Start streaming from a capture buffer: the next hop begins at
*         @p write_pos (half-words, from BSP_AUDIO_IN_GetWritePos()) and
*         any partly accumulated update is discarded.
*/
void FFT_Stream_Reset(uint32_t write_pos);

/**
* @brief  Process every complete hop between the last call and @p write_pos.
*         Call often (every main-loop pass). Reads the circular interleaved
*         stereo buffer @p buf of @p buf_len half-words, wrapping as needed.
*         If it has fallen more than half a buffer behind, it skips ahead to
*         the newest audio rather than working through stale hops.
* @retval 1 when Clean_Display_Bars holds a new update (every
*         FFT_HOPS_PER_REFRESH hops), else 0.
*/
uint8_t FFT_Stream_Process(const int16_t *buf, uint32_t buf_len, uint32_t write_pos);

#endif /* INC_FFT_PROCESSING_H_ */
