#pragma once
#ifndef _DECIMATOR_H
#define _DECIMATOR_H

/*
 * decimator
 * Downsamples a signal by a factor of 2.
 * This is a polyphase filter built from two cascades of allpass filters.
 * The code is based on hiir library by Laurent de Soras
 * http://ldesoras.free.fr/prod.html#src_hiir
 * Filter order 8, stopband rejection 69 dB, transition band width 0.01.
 */

#include "compat.h"

// Coefficients of the polyphase filter
#define NC_DSMPL 8
const float DSMPL_COEF[NC_DSMPL] = { 0.0771150798324162f, 0.2659685265210946f, 0.4820706250610472f, 0.6651041532634957f,
    0.7968204713315797f, 0.8841015085506159f, 0.9412514277740471f, 0.9820054141886075f };

// Decimator state
typedef struct {
    float s[NC_DSMPL + 2];
} DecimatorState;

/*  decimator_reset
    Reset the filter state to zero.
*/
_INLINE void decimator_reset(DecimatorState* state)
{
    int i;
    for (i = 0; i < NC_DSMPL + 2; i++)
        state->s[i] = 0;
}

/*  decimator_do
    Decimate two samples into one, using a half-band filter.
    x1: first input sample
    x2: second input sample
    Returns: decimated output sample
*/
_INLINE float decimator_do(DecimatorState* state, float x1, float x2)
{
    float aIn = x2;
    float bIn = x1;
    float aOut, bOut;
    int p;
    for (p = 0; p < NC_DSMPL;) {
        // upper branch
        aOut = (aIn - state->s[p + 2]) * DSMPL_COEF[p] + state->s[p];
        state->s[p] = aIn;
        aIn = aOut;
        p++;
        // lower branch
        bOut = (bIn - state->s[p + 2]) * DSMPL_COEF[p] + state->s[p];
        state->s[p] = bIn;
        bIn = bOut;
        p++;
    }
    state->s[p] = aOut;
    state->s[p + 1] = bOut;
    return 0.5f * (aOut + bOut);
}

/*  decimator_process
    Decimate an array of samples.
    in: pointer to array containing `count` input samples
    out: pointer to array to which `count`/2 output samples will be written
    count: number of the input samples, should be even
    Returns: number of samples written to `out`
*/
_INLINE unsigned int decimator_process(DecimatorState* state, const float* in, float* out, unsigned int count)
{
    unsigned int i, n = 0;
    for (i = 0; i < count - 1; i += 2, n++)
        out[n] = decimator_do(state, in[i], in[i + 1]);
    return n;
}

#endif
