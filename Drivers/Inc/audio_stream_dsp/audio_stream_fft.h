/*
 * fft_module.h
 *
 *  Created on: Jun 10, 2025
 *      Author: oliverfoster
 */

#ifndef SRC_FFT_MODULE_H_
#define SRC_FFT_MODULE_H_



#include "stdint.h"
#include <audio_stream_dsp/audio_stream_tone.h>
#include "arm_math.h"


// fft_processing.h
#ifndef FFT_PROCESSING_H
#define FFT_PROCESSING_H

#include "stdint.h"
#include "arm_math.h"



void FFT_Test_Raw(int16_t *sample_block);

/**
 * @brief Initialize FFT processing module
 * Must be called before using FFT functions
 */
void FFT_Init(void);

/**
 * @brief Initialize windowing function (Hamming, Hann, etc.)
 * Prepares window coefficients for FFT processing
 */
void FFT_Window_Init(void);

/**
 * @brief Convert audio samples to Q15 PCM format
 *
 * @param audio_in Pointer to input audio samples
 * @param pcm_out Pointer to output Q15 PCM buffer
 * @param length Number of samples to convert
 */
void FFT_Convert_Audio_To_Q15(const audio_sample_t *audio_in, q15_t *pcm_out, uint16_t length);

/**
 * @brief Perform FFT on raw audio samples (test function)
 *
 * @param sample_block Pointer to input sample block
 */
void FFT_Process_Raw_Samples(int16_t *sample_block);

/**
 * @brief Post-process FFT output (magnitude calculation)
 *
 * @param sample_block Pointer to FFT output block
 */
void FFT_Postprocess(int16_t *sample_block);

/**
 * @brief Post-process FFT with adaptive averaging
 *
 * @param sample_block Pointer to FFT output block
 */
void FFT_Postprocess_Adaptive(volatile int16_t *sample_block);

/**
 * @brief Post-process FFT with adaptive averaging in dB scale
 *
 * @param sample_block Pointer to FFT output block
 */
void FFT_Postprocess_Adaptive_dB(int16_t *sample_block);


/**
 * @brief Generate and process 440Hz test tone
 * Used for testing and calibration
 */
void FFT_Test_440Hz_Tone(void);

/**
 * @brief Convert FFT magnitude to dB scale (Q15 format)
 *
 * @param mag_input Pointer to input magnitude array (Q15)
 * @param db_output Pointer to output dB array (Q15)
 * @param length Number of samples to convert
 */
void FFT_Convert_Magnitude_To_DB(q15_t *mag_input, q15_t *db_output, uint16_t length);

/**
 * @brief Calculate adaptive threshold for magnitude data
 *
 * @param mag_bins Pointer to magnitude bin array
 * @param size Number of bins
 * @return Calculated threshold value in Q15 format
 */
q15_t FFT_Calculate_Threshold(q15_t *mag_bins, uint32_t size);

/**
 * @brief Apply adaptive averaging to magnitude spectrum
 * Uses asymmetric attack/decay for smooth visualization
 *
 * @param current Pointer to current frame magnitude data
 * @param previous Pointer to previous frame magnitude data
 * @param output Pointer to output averaged data
 * @param size Number of bins to process
 */
void FFT_Adaptive_Averaging(q15_t *current, q15_t *previous, q15_t *output, uint32_t size);

/**
 * @brief Apply adaptive averaging in dB domain
 * Optimized for logarithmic scale processing
 *
 * @param current Pointer to current frame dB data
 * @param previous Pointer to previous frame dB data
 * @param output Pointer to output averaged data
 * @param size Number of bins to process
 */


void FFT_Postprocess_Exponential(int16_t *sample_block);


void FFT_Adaptive_Averaging_DB(q15_t *current, q15_t *previous, q15_t *output, uint32_t size);

/**
 * @brief Remove DC component and normalize magnitude data
 *
 * @param mag_block Pointer to magnitude data block
 * @param length Number of samples in block
 */
void FFT_DC_Remove_And_Normalize(int16_t *mag_block, uint32_t length);


void window_init();


void apply_window_q15(q15_t *pcm_samples, q15_t *windowed_samples,
		q15_t *window, uint16_t size);


void dc_norm(int16_t * mag_block,uint32_t length);


#endif // FFT_PROCESSING_H


#endif /* SRC_FFT_MODULE_H_ */
