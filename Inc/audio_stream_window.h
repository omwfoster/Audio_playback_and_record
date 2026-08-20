////////////////////////
// window_functions_q15.h
////////////////////////

#ifndef WINDOW_FUNCTIONS_Q15_H
#define WINDOW_FUNCTIONS_Q15_H

#include "arm_math.h"
#include <stdbool.h>

// Window generation functions
void hann_q15(q15_t *w, uint32_t n, bool sflag);
void hamming_q15(q15_t *w, uint32_t n, bool sflag);
void blackman_q15(q15_t *w, uint32_t n, bool sflag);
void blackmanharris_q15(q15_t *w, uint32_t n, bool sflag);
void rectwin_q15(q15_t *w, uint32_t n);
void triang_q15(q15_t *w, uint32_t n);
void bartlett_q15(q15_t *w, uint32_t n);



#endif // WINDOW_FUNCTIONS_Q15_H
