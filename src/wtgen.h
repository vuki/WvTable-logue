#pragma once
#ifndef _WTGEN_H
#define _WTGEN_H

/*
 * wtgen.h
 * Wavetable generator inspired by PPG Wave
 * Author: Grzegorz Szwoch (GregVuki)
 */

#include <stdint.h>
#include "compat.h"
#include "wtdef.h"

#define MAX_PHASE 128.f
#define Q25TOF 2.9802322387695312e-08f
#define MASK_25 0x1ffffff

// Wavetable modes
typedef enum {
    WTMODE_INT2D = 0, // bilinear interpolation: wave and sample
    WTMODE_INT1D = 1, // linear interpolation: only sample
    WTMODE_NOINT = 2 // no interpolation
} WtMode;

typedef struct WtGenState {
    float (*generate)(struct WtGenState*); // pointer to function generating samples
    uint8_t wavetable[61][4]; // wavetable definition
    uint8_t wtnum; // wavetable number
    uint8_t wtmode; // wavetable mode
    uint8_t wave[2]; // numbers of the stored waves (indices into WAVES)
    uint8_t* pwave[2]; // pointer to samples of the waves
    float alpha_w; // linear interpolation coefficient
    uq7_25_t phase; // signal phase, UQ7.25
    uq7_25_t step; // step to increase the phase, UQ7.25
    float recip_step; // 1/step as float
    float phase_scaler; // 1/(ovs*srate)
    float sync_step; // sync step for wavetable 28
    float sync_period; // sync period for wavetable 28
    uq7_25_t skew_bp; // phase skew breakpoint, UQ7.25
    float skew_r1; // phase skew rate below the breakpoint
    float skew_r2; // phase skew rate above the breakpoint
    q7_24_t last_wavenum; // last wave number that was set
    uint8_t last_wtnum; // last wavetable number that was set
} WtGenState;

_INLINE void set_wavetable(WtGenState* state, uint8_t ntable);
_INLINE void set_wave_number(WtGenState* state, q7_24_t wavenum);
_INLINE float generate_wavecycles(WtGenState* state);
_INLINE float generate_wavecycles_noint(WtGenState* state);
_INLINE float generate_wt28(WtGenState* state);
_INLINE float generate_wt28_noint(WtGenState* state);
_INLINE float generate_wt29(WtGenState* state);
_INLINE float generate_wt29_noint(WtGenState* state);

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
    state->phase_scaler = 1.f / srate;
    state->sync_step = 1.f;
    state->sync_period = 128.f;
    state->skew_bp = 0;
    state->skew_r1 = state->skew_r2 = 1.f;
    state->last_wavenum = 0;
    state->last_wtnum = 255;
    set_wavetable(state, 0);
}

/*  wtgen_reset
    Reset the oscillator state
*/
_INLINE void wtgen_reset(WtGenState* state)
{
    state->phase = 0;
}

/*  set_frequency
    Set frequency of the oscscillator
    freq: frequency in Hz
*/
_INLINE void set_frequency(WtGenState* state, float freq)
{
    const float step_f = freq * state->phase_scaler;
    state->step = (uq7_25_t)(step_f * 4294967296.f); // step * 2**32
    state->recip_step = 0.0078125f / step_f; // (1/128)/step_f
}

/*  set_skew
    Sets the phase skew for wave readout.
    bp: phase breakpoint as UQ7.25; 0 disables the skew.
*/
_INLINE void set_skew(WtGenState* state, uq7_25_t bp)
{
    if ((bp > 0) && (bp < 0x80000000)) {
        state->skew_bp = bp;
        const float fbp = (float)bp * Q25TOF;
        state->skew_r1 = 64.f / fbp;
        state->skew_r2 = 64.f / (128.f - fbp);
    } else {
        state->skew_bp = 0;
        // state->skew_bp = 64.f;
        // state->skew_r1 = state->skew_r2 = 1.f;
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

    switch (state->wtnum) {

    case WT_SYNC:
        switch (state->wtmode) {
        case WTMODE_NOINT:
            state->generate = &generate_wt28_noint;
            break;
        default:
            state->generate = &generate_wt28;
        }
        break;

    case WT_STEP:
        switch (state->wtmode) {
        case WTMODE_NOINT:
            state->generate = &generate_wt29_noint;
            break;
        default:
            state->generate = &generate_wt29;
        }
        break;

    default: {
        // Build wavetable indices for wave interpolation
        // entry 1: lower wave number
        // entry 2: upper wave number
        // entry 3: distance from the lower wave position
        // entry 4: span between the lower and the upper wave

        const uint8_t* pwtdef = &WAVETABLES[state->wtnum][0];
        uint8_t n, p1, w1, p2, w2;

        p1 = *pwtdef++;
        w1 = *pwtdef++;
        p2 = *pwtdef++;
        w2 = *pwtdef++;
        for (n = 0; n < 60; n++) {
            if (n == p2) {
                p1 = p2;
                w1 = w2;
                p2 = *pwtdef++;
                w2 = *pwtdef++;
            }
            state->wavetable[n][0] = w1;
            state->wavetable[n][1] = w2;
            state->wavetable[n][2] = p1;
            state->wavetable[n][3] = p2 - p1;
        }
        state->wavetable[60][0] = w1;
        state->wavetable[60][1] = w2;
        state->wavetable[60][2] = p1;
        state->wavetable[60][3] = p2 - p1;

        switch (state->wtmode) {
        case WTMODE_NOINT:
            state->generate = &generate_wavecycles_noint;
            break;
        default:
            state->generate = &generate_wavecycles;
        }
    }
    }

    const q7_24_t last_wn = state->last_wavenum;
    state->last_wavenum = (q7_24_t)0xFFFFFFFF;
    set_wave_number(state, last_wn); // recalculate wave number
}

/*  set_wave_number
    Set the wave number - position within the wavetable.
    wavenum: requested wave number, Q7.24 (signed)
*/
_INLINE void set_wave_number(WtGenState* state, q7_24_t wavenum)
{
    if (wavenum == state->last_wavenum)
        return; // already set
    state->last_wavenum = wavenum;

    // Normalize wave number
    // wavenum is signed Q7.24 (-128..127)
    // convert to unsigned Q6.24 (0..64) with mirroring
    const int32_t tmp = (wavenum << 1) >> 1; // force overflow
    const int32_t sign = tmp >> 31;
    const int32_t norm_wavenum = (tmp ^ sign) + sign; // normalized to (0..64)

    // Convert to floating point wavetable position, 0..61
    const float nwave = (float)norm_wavenum * 5.681067705154419e-08f; // * (2**-24 * 61 / 64)
    const uint8_t nwave_i = (uint8_t)nwave; // integer part of the wave number, 0..60
    // const float nwave_f = (float)nwave - nwave_i; // fractional part of the wave number

    switch (state->wtnum) {
    case WT_SYNC:
        // Wavetable 28 - sync
        // store floating point wave number
        state->alpha_w = (state->wtmode == WTMODE_INT2D) ? nwave : (float)nwave_i;
        // amplitude step for one sample (scaler value found experimentally)
        state->sync_step = state->alpha_w * 0.0859375f + 1.f;
        // sync period - amplitude resets after this number of samples
        state->sync_period = MAX_PHASE / state->sync_step; // ceil
        break;

    case WT_STEP:
        // Wavetable 29 - step
        // store floating point wave number
        state->alpha_w = (state->wtmode == WTMODE_INT2D) ? nwave : (float)nwave_i;
        break;

    default:
        // Memory waves
        // find two waves used for interpolation
        state->wave[0] = state->wavetable[nwave_i][0];
        state->wave[1] = state->wavetable[nwave_i][1];
        state->pwave[0] = (uint8_t*)&WAVES[state->wave[0]][0];
        state->pwave[1] = (uint8_t*)&WAVES[state->wave[1]][0];
        if (state->wtmode != WTMODE_INT2D) {
            state->alpha_w = (nwave - state->wavetable[nwave_i][2]) * WSCALER[state->wavetable[nwave_i][3] - 1];
        } else {
            // only integer wave positions
            state->alpha_w = ((uint8_t)(nwave + 0.5f) - state->wavetable[nwave_i][2]) * WSCALER[state->wavetable[nwave_i][3] - 1];;
        }
    }
}

/*  generate
    Calculate and return one sample value.
    Proxy function that calls the actual function.
    Returns: sample value, floating point, -127.5 to 127.5
*/
_INLINE float generate(WtGenState* state)
{
    return state->generate(state);
}

/*  generate_wavecycles
    Calculate and return one sample value.
    Uses wavetables with memory waves.
    Interpolate sample values.
    Returns: sample value, floating point, -127.5 to 127.5
*/
_INLINE float generate_wavecycles(WtGenState* state)
{
    float out1, out2, y;
    uint8_t w11, w12, w21, w22;
    uint8_t pos, pos2; // integer sample position, 0..128
    float alpha; // fractional part of the sample position

    if (!state->skew_bp) {
        pos = (uint8_t)(state->phase >> 25); // UQ7
        alpha = (float)(state->phase & MASK_25) * Q25TOF;
    } else {
        // apply phase distortion
        const float fpos = (state->phase <= state->skew_bp)
            ? state->skew_r1 * (float)state->phase * Q25TOF
            : state->skew_r2 * (float)(state->phase - state->skew_bp) * Q25TOF + 64.f;
        pos = (uint8_t)fpos;
        alpha = fpos - pos;
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
    y = (1.f - state->alpha_w) * out1 + state->alpha_w * out2 - 127.f;

    state->phase += state->step;
    return y;
}

/*  generate_wavecycles_noint
    Calculate and return one sample value.
    Uses wavetables with memory waves.
    Do not interpolate between samples
    Returns: sample value, floating point, -127.5 to 127.5
*/
_INLINE float generate_wavecycles_noint(WtGenState* state)
{
    float y;
    uint8_t w11, w21;
    uint8_t pos; // integer sample position, 0..128

    if (!state->skew_bp) {
        pos = (uint8_t)(state->phase >> 25); // UQ7
    } else {
        // apply phase distortion
        const float fpos = (state->phase <= state->skew_bp)
            ? state->skew_r1 * (float)state->phase * Q25TOF
            : state->skew_r2 * (float)(state->phase - state->skew_bp) * Q25TOF + 64.f;
        pos = (uint8_t)fpos;
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
    // interpolate between waves
    y = (1.f - state->alpha_w) * w11 + state->alpha_w * w21 - 127.5f;
    state->phase += state->step;
    return y;
}

/*  generate_wt28
    Calculate and return one sample value from wavetable 28 (sync).
    Interpolate between samples.
    Returns: sample value, floating point, -127.5 to 127.5
*/
_INLINE float generate_wt28(WtGenState* state)
{
    // (no aliasing protection)
    float posf = (float)state->phase * Q25TOF; // phase 0..128
    // subtract synched periods
    while (posf >= state->sync_period)
        posf -= state->sync_period;
    const float y = -64.f + posf * state->sync_step;
    // TODO: PolyBlep, at 0 and at each sync point
    // (needs the value at 128 for step length)
    state->phase += state->step;
    return y;
}

/*  generate_wt28_noint
    Calculate and return one sample value from wavetable 28 (sync).
    Do not interpolate between samples.
    Returns: sample value, floating point, -127.5 to 127.5
*/
_INLINE float generate_wt28_noint(WtGenState* state)
{
    // (no aliasing protection)
    float posf = (float)(state->phase >> 25); // phase 0..128
    // subtract synched periods
    while (posf >= state->sync_period)
        posf -= state->sync_period;
    const float y = -64.f + posf * state->sync_step;
    // TODO: PolyBlep, at 0 and at each sync point
    // (needs the value at 128 for step length)
    state->phase += state->step;
    return y;
}

/*  generate_wt29
    Calculate and return one sample value from wavetable 29 (step).
    Interpolate between samples.
    Returns: sample value, floating point, -127.5 to 127.5
*/
_INLINE float generate_wt29(WtGenState* state)
{
    float y;
    // wavetable 29: step wave (with PolyBLEP)
    const float pos = (float)state->phase * Q25TOF;
    const float phase_step = (float)(state->step) * Q25TOF;
    const float edge = 64.f + state->alpha_w; // transition high->low
    y = (pos < edge) ? 32.f : -32.f;
    if (pos < phase_step) {
        const float t = pos * state->recip_step;
        y += (t + t - t * t - 1.f) * 32.f;
    } else if (((edge - phase_step) < pos) && (pos < edge)) {
        const float t = (pos - edge) * state->recip_step;
        y -= (t * t + t + t + 1.f) * 32.f;
    } else if ((edge <= pos) && (pos < (edge + phase_step))) {
        const float t = (pos - edge) * state->recip_step;
        y -= (t + t - t * t - 1.f) * 32.f;
    } else if (pos > 128 - phase_step) {
        const float t = (pos - 128.f) * state->recip_step;
        y += (t * t + t + t + 1.f) * 32.f;
    }
    state->phase += state->step;
    return y;
}

/*  generate_wt29_noint
    Calculate and return one sample value from wavetable 29 (step).
    Do not interpolate between samples.
    Returns: sample value, floating point, -127.5 to 127.5
*/
_INLINE float generate_wt29_noint(WtGenState* state)
{
    // wavetable 29: step wave (no antialiasing)
    const uint8_t pos = (uint8_t)(state->phase >> 25);
    const uint8_t edge = 64 + (uint8_t)state->alpha_w; // transition high->low
    const float y = (pos < edge) ? 32.f : -32.f;
    state->phase += state->step;
    return y;
}

#endif
