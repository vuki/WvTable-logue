#pragma once
#ifndef _WTGEN_H
#define _WTGEN_H

/*
 * wtgen.h
 * Wavetable generator inspired by PPG Wave
 * Author: Grzegorz Szwoch (GregVuki)
 */

#if defined(OVS_4x)
#define OVS 4
#elif defined(OVS_2x)
#define OVS 2
#else
#define OVS 1
#endif

#include <stdint.h>
#include "compat.h"
#include "wtdef.h"
#include "decimator.h"

#define MAX_PHASE 128.f
// #define MAX_TABLE 61
#define MAX_WAVE 61 // maximum wave index in table
// Wavetable modes
#define WTMODE_INT2D 0 // bilinear interpolation: wave and sample
#define WTMODE_INT1D 1 // linear interpolation: only sample
#define WTMODE_NOINT 2 // no interpolation

#define Q25TOF 2.9802322387695312e-08f
#define Q24TOF 5.960464477539063e-08f
#define MASK_BIT31 0x80000000
#define MASK_31 0x7fffffff
#define MASK_25 0x1ffffff
#define MASK_7 0x7f
#define MASK_6 0x3f

typedef struct {
    uint8_t wavetable[64][4]; // wavetable definition
    uint8_t wtnum; // wavetable number
    uint8_t wtmode; // wavetable mode
    uint8_t wave[2]; // numbers of the stored waves (indices into WAVES)
    uint8_t* pwave[2]; // pointer to samples of the waves
    float alpha_w; // linear interpolation coefficient
    uint32_t phase; // signal phase, UQ7.25
    uint32_t step; // step to increase the phase, UQ7.25
    float recip_step; // 1/step as float
    float phase_scaler; // 1/(ovs*srate)
    float sync_step; // sync step for wavetable 28
    float sync_period; // sync period for wavetable 28
    uint32_t pd_bp; // phase distortion breakpoint, UQ7.25
    float pd_r1; // phase distortion rate below the breakpoint
    float pd_r2; // phase distortion rate above the breakpoint
    int32_t last_wavenum; // last wave number that was set
    uint8_t last_wtnum; // last wavetable number that was set
#if OVS != 1
    DecimatorState decimator;
#endif
#if OVS == 4
    DecimatorState decimator2;
#endif
} WtGenState;

_INLINE void set_wavetable(WtGenState* state, uint8_t ntable);
_INLINE void set_wave_number(WtGenState* state, int32_t wavenum);

/*  wtgen_init
    Initialize the generator
    srate: sampling rate in Hz
*/
_INLINE void wtgen_init(WtGenState* state, float srate)
{
    state->wave[0] = WAVETABLES[0][0];
    state->wave[1] = WAVETABLES[0][1];
    state->pwave[0] = (uint8_t*)&WAVES[state->wave[0]][0];
    state->pwave[1] = (uint8_t*)&WAVES[state->wave[1]][0];
    state->alpha_w = 0;
    state->phase = 0;
    state->step = 0x2000000;
    state->phase_scaler = 1.f / (OVS * srate);
    state->sync_step = 1.f;
    state->sync_period = 128.f;
    state->pd_bp = 0;
    state->pd_r1 = state->pd_r2 = 1.f;
    state->last_wavenum = 0;
    state->last_wtnum = 255;

    set_wavetable(state, 0);

#if OVS != 1
    decimator_reset(&state->decimator);
#endif
#if OVS == 4
    decimator_reset(&state->decimator2);
#endif
}

/*  wtgen_reset
    Reset the oscillator state
*/
_INLINE void wtgen_reset(WtGenState* state)
{
    state->phase = 0;
#if OVS != 1
    decimator_reset(&state->decimator);
#endif
#if OVS == 4
    decimator_reset(&state->decimator2);
#endif
}

/*  set_frequency
    Set frequency of the oscscillator
    freq: frequency in Hz
*/
_INLINE void set_frequency(WtGenState* state, float freq)
{
    const float step_f = freq * state->phase_scaler;
    state->step = (uint32_t)(step_f * 4294967296.f); // step * 2**32
    state->recip_step = 0.0078125f / step_f; // (1/128)/step_f
}

/*  set_phase_distortion
    Sets phase distortion for wave readout.
    bp: phase breakpoint as UQ7.25; 0 disables the pd.
*/
_INLINE void set_phase_distortion(WtGenState* state, uint32_t bp)
{
    if ((bp > 0) && (bp < 0x80000000)) {
        state->pd_bp = bp;
        const float fbp = bp * Q25TOF;
        state->pd_r1 = 64.f / fbp;
        state->pd_r2 = 64.f / (128.f - fbp);
    } else {
        state->pd_bp = 0;
    }
}

/*  set_wavetable
    Set the wavetable number.
    ntable: wavetable number, 0..95
*/
_INLINE void set_wavetable(WtGenState* state, uint8_t ntable)
{
    if (ntable == state->last_wtnum)
        return; // already set
    state->last_wtnum = ntable;

    // normalize wavetable number
    state->wtnum = ntable & 0x1F; // lower 5 bits: wavetable number, 0..31
    state->wtmode = (ntable >> 5) & 0x03; // upper bits: wavetable mode

    if (state->wtnum != WT_SYNC && state->wtnum != WT_STEP) {
        // Build wavetable indices for wave interpolation
        // entry 1: lower wave number
        // entry 2: upper wave number
        // entry 3: distance from the lower wave position
        // entry 4: span between the lower and the upper wave

        const uint8_t* pwtdef = &WAVETABLES[state->wtnum][0];
        uint8_t n, p1, w1, p2, w2;

        p1 = *pwtdef++;
        w1 = *pwtdef++;
        for (n = 0; n < 60; n++) {
            if (n == w2) {
                p1 = p2;
                w1 = w2;
                p2 = *pwtdef++;
                w2 = *pwtdef++;
            }
            state->wavetable[n][0] = w1;
            state->wavetable[n][1] = w2;
            state->wavetable[n][2] = n - p1;
            state->wavetable[n][3] = p2 - p1;
        }
        state->wavetable[60][0] = w2;
        state->wavetable[60][1] = w2;
        state->wavetable[60][2] = 0;
        state->wavetable[60][3] = 1;
    }

    const int32_t last_wn = state->last_wavenum;
    state->last_wavenum = 0xFFFFFFFF;
    set_wave_number(state, last_wn); // recalculate wave number
}

/*  set_wave_number
    Set the wave number - position within the wavetable.
    wavenum: requested wave number, Q7.24 (signed)
*/
_INLINE void set_wave_number(WtGenState* state, int32_t wavenum)
{
    if (wavenum == state->last_wavenum)
        return; // already set
    state->last_wavenum = wavenum;

    // Normalize wave number
    // wavenum is signed Q7.24 (-128..127)
    // convert to unsigned Q6.24 (0..64) with mirroring
    const uint8_t tmp = (wavenum << 1) >> 1; // force overflow
    const uint8_t sign = tmp >> 31;
    const uint8_t norm_wavenum = (tmp ^ sign) + sign;

    // Convert to floating point wavetable position, 0..61
    const float nwave = (float)wavenum * 5.681067705154419e-08f; // * (2**-24 * 61 / 64)
    const uint8_t nwave_i = (int)nwave; // integer part of the wave number, 0..60
    const float nwave_f = (float)nwave - nwave_i; // fractional part of the wave number

    if (state->wtnum != WT_SYNC && state->wtnum != WT_STEP) {
        // Memory waves
        // find two waves used for interpolation
        state->wave[0] = state->wavetable[nwave_i][0];
        state->wave[1] = state->wavetable[nwave_i][1];
        state->pwave[0] = (uint8_t*)&WAVES[state->wave[0]][0];
        state->pwave[1] = (uint8_t*)&WAVES[state->wave[1]][0];
        state->alpha_w = (nwave - state->wavetable[nwave_i][2]) * WSCALER[state->wavetable[nwave_i][3] - 1];
        if (state->wtmode != WTMODE_INT2D) {
            // only integer wave positions
            state->alpha_w = (float)((int)state->alpha_w);
        }

    } else if (state->wtnum == WT_SYNC) {
        // Wavetable 28 - sync
        // store floating point wave number
        state->alpha_w = (state->wtmode == WTMODE_INT2D) ? nwave : (float)nwave_i;
        // amplitude step for one sample (scaler value found experimentally)
        state->sync_step = nwave * 0.0859375f + 1.f;
        // sync period - amplitude resets after this number of samples
        state->sync_period = (float)((int)(0.99999999f + MAX_PHASE / state->sync_step)); // ceil
    } else if (state->wtnum == WT_STEP) {
        // Wavetable 29 - step
        // store floating point wave number
        state->alpha_w = (state->wtmode == WTMODE_INT2D) ? nwave : (float)nwave_i;
    }
}

/*  generate
    Calculate and return one sample value.
    Returns: sample value, floating point, -127.5 to 127.5
*/
_INLINE float generate(WtGenState* state)
{
    float y[OVS];
    float out1, out2;
    uint8_t k, n, w11, w12, w21, w22;
    // const uint32_t nwave_buf = *((uint32_t*)(&state->wave));
    // const uint8_t* const nwave = (const uint8_t*)&nwave_buf;

    // Loop over oversampled values
    // TODO: remove this and do the oversampling outside
    for (n = 0; n < OVS; n++) {

        if (state->wtnum != WT_SYNC && state->wtnum != WT_STEP) {
            // standard waves
            uint8_t pos, pos2; // integer sample position, 0..128
            float alpha; // fractional part of the sample position
            if (!state->pd_bp) {
                pos = (uint8_t)(state->phase >> 25); // UQ7
                if (state->wtmode != WTMODE_NOINT)
                    alpha = (float)(state->phase & MASK_25) * Q25TOF;
                else
                    alpha = 0.f; // no sample interpolation
            } else {
                // apply phase distortion
                float fpos;
                if (state->phase <= state->pd_bp) {
                    fpos = state->pd_r1 * state->phase * Q25TOF;
                } else {
                    fpos = state->pd_r2 * (state->phase - state->pd_bp) * Q25TOF + 64.f;
                }
                pos = (uint8_t)fpos;
                alpha = (state->wtmode != WTMODE_NOINT) ? (fpos - pos) : 0.f;
            }
            // get sample values from the stored waves
            if (!(pos & 0x40)) {
                // pos 0..63 - first half of the period
                w11 = state->pwave[0][pos];
                w21 = state->pwave[1][pos];
            } else {
                // pos 64..127 - second half of the period
                // the first falf is mirrored in time and amplitude
                const uint8_t posr = ~pos & 0x3F;
                w11 = ~state->pwave[0][posr];
                w21 = ~state->pwave[1][posr];
            }
            pos2 = (pos + 1) & 0x7F;
            if (!(pos2 & 0x40)) {
                // pos 0..63 - first half of the period
                w12 = state->pwave[0][pos2];
                w22 = state->pwave[1][pos2];
            } else {
                // pos 64..127 - second half of the period
                // the first falf is mirrored in time and amplitude
                const uint8_t pos2r = ~pos2 & 0x3F;
                w12 = ~state->pwave[0][pos2r];
                w22 = ~state->pwave[1][pos2r];
            }
            // interpolate between samples
            out1 = (1.f - alpha) * w11 + alpha * w12;
            out2 = (1.f - alpha) * w21 + alpha * w22;
            // interpolate between waves
            y[n] = (1.f - state->alpha_w) * out1 + state->alpha_w * out2;
            y[n] -= 127.5f; // make bipolar and compensate for DC offset

        } else if (state->wtnum == WAVE_SYNC) {
            // wavetable 28: sync wave (no aliasing protection)
            // state->alpha_w is a floating point wave number, 0..61
            float posf = (float)state->phase * Q25TOF; // phase 0..128
            // subtract synched periods
            while (posf >= state->sync_period)
                posf -= state->sync_period;
            y[n] = -64.f + posf * state->sync_step;
            // TODO: PolyBlep, at 0 and at each sync point
            // (needs the value at 128 for step length)

        } else if (state->wtnum == WAVE_STEP) {
            // wavetable 29: step wave (with PolyBLEP)
            const float pos = (float)state->phase * Q25TOF;
            const float phase_step = (float)(state->step) * Q25TOF;
            const float edge = 69.f + state->alpha_w; // transition high->low
            y[n] = (pos < edge) ? 32.f : -32.f;
            if (pos < phase_step) {
                const float t = pos * state->recip_step;
                y[n] += (t + t - t * t - 1.f) * 32.f;
            } else if (((edge - phase_step) < pos) && (pos < edge)) {
                const float t = (pos - edge) * state->recip_step;
                y[n] -= (t * t + t + t + 1.f) * 32.f;
            } else if ((edge <= pos) && (pos < (edge + phase_step))) {
                const float t = (pos - edge) * state->recip_step;
                y[n] -= (t + t - t * t - 1.f) * 32.f;
            } else if (pos > 128 - phase_step) {
                const float t = (pos - 128.f) * state->recip_step;
                y[n] += (t * t + t + t + 1.f) * 32.f;
            }
        } else {
            y[n] = 0;
        }
    }
    state->phase += state->step;

#if OVS == 4
    y[0] = decimator_do(&state->decimator2, y[0], y[1]);
    y[1] = decimator_do(&state->decimator2, y[2], y[3]);
#endif
#if OVS != 1
    y[0] = decimator_do(&state->decimator, y[0], y[1]);
#endif
    return y[0];
}

#endif
