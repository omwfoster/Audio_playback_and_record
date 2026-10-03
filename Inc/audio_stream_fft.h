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

#define FFT_LEN              512

#define NUM_RAW_BINS         (FFT_LEN / 2)  /* 256 unique frequency bins */

#define HOP_SIZE             256           /* 50% Overlap window */

#define BIN_RESOLUTION       ((float32_t)SAMPLE_RATE / FFT_LEN)  /* 31.25 Hz */



/* Display Visual Target Parameters */

#define DISPLAY_BINS         128

#define FREQ_MIN             20.0f

#define FREQ_MAX             (SAMPLE_RATE / 2.0f)  /* Nyquist: 8 kHz */



/* --- Struct Definitions --- */

typedef struct {

    uint32_t index_low;   /* Floor raw bin index */

    uint32_t index_high;  /* Ceiling raw bin index */

    float32_t weight;     /* Fractional blend distance between low and high */

} InterpolationMap_t;



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


#endif /* INC_FFT_PROCESSING_H_ */
