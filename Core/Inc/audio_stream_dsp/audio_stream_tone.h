/*
 * tone..h
 *
 *  Created on: Jul 23, 2025
 *      Author: oliverfoster
 */

#ifndef INC_TONE__H_
#define INC_TONE__H_

#include "arm_math.h"


typedef unsigned char audio_sample_t;
typedef audio_sample_t audio_data_t[256];

extern const int16_t test_440[];


extern const audio_data_t test2k;
extern const audio_data_t test_220;
extern const int16_t test_183hz[];




#endif /* INC_TONE__H_ */
