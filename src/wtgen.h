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

#define MAXPHASE 128.f
#define MAX_TABLE 61
#define Q25TOF 2.9802322387695312e-08f
#define Q24TOF 5.960464477539063e-08f
#define MASK_BIT31 0x80000000
#define MASK_31 0x7fffffff
#define MASK_25 0x1ffffff
#define MASK_7 0x7f
#define MASK_6 0x3f

typedef struct {
    uint8_t wavetable[128][4]; // wavetable definition
    uint8_t wave[4]; // numbers of the stored waves (indices into WAVES)
    uint8_t* pwave[2]; // pointer to samples of the waves
    float alpha_w; // linear interpolation coefficient
    uint32_t phase; // signal phase, UQ7.25
    uint32_t step; // step to increase the phase, UQ7.25
    float recip_step; // 1/step as float
    float phase_scaler; // 1/(ovs*srate)
    uint32_t pd_bp; // phase distortion breakpoint, UQ7.25
    float pd_r1; // phase distortion rate below the breakpoint
    float pd_r2; // phase distortion rate above the breakpoint
#if OVS != 1
    DecimatorState decimator;
#endif
#if OVS == 4
    DecimatorState decimator2;
#endif
} WtGenState;

_INLINE void load_wavetable(WtGenState* state, uint8_t wt_number, uint8_t use_upper);

/*  wtgen_init
    Initialize the generator
    srate: sampling rate in Hz
*/
_INLINE void wtgen_init(WtGenState* __restrict state, float srate)
{
    // load_wavetable(&state->osc, 0, 1); // run it from the caller
    state->wave[0] = WAVETABLES[0][0];
    state->wave[1] = WAVETABLES[0][1];
    state->pwave[0] = (uint8_t*)&WAVES[state->wave[0]][0];
    state->pwave[1] = (uint8_t*)&WAVES[state->wave[1]][0];
    state->alpha_w = 0;
    state->phase = 0;
    state->step = 0x2000000;
    state->phase_scaler = 1.f / (OVS * srate);
    state->pd_bp = 0;
    state->pd_r1 = state->pd_r2 = 1.f;
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
_INLINE void wtgen_reset(WtGenState* __restrict state)
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
_INLINE void set_frequency(WtGenState* __restrict state, float freq)
{
    const float step_f = freq * state->phase_scaler;
    state->step = (uint32_t)(step_f * 4294967296.f); // step * 2**32
    state->recip_step = 0.0078125f / step_f; // (1/128)/step_f
}

/*  set_phase_distortion
    Sets phase distortion for wave readout.
    bp: phase breakpoint as UQ7.25; 0 disables the pd.
*/
_INLINE void set_phase_distortion(WtGenState* __restrict state, uint32_t bp)
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
    Set the wavetable to use
    ntable: wavetable number, 0..61
*/
_INLINE void set_wavetable(WtGenState* __restrict state, uint8_t ntable)
{
    while (ntable >= MAX_TABLE)
        ntable -= MAX_TABLE;
    uint8_t use_upper = 1;
    if (ntable >= WT_UPPER) {
        use_upper = 0;
        ntable -= WT_UPPER;
    }
    load_wavetable(state, ntable, use_upper);
}

/*  set_wave_number
    Set the wave number
    wavenum: requested wave number, Q7.25
*/
_INLINE void set_wave_number(WtGenState* __restrict state, uint32_t wavenum)
{

    const uint8_t nwave_i = (wavenum >> 25); // integer part of the wave number, 0..127 (UQ7)
    const float nwave_f = Q25TOF * wavenum; // full wave number (0..127) as float
    uint32_t wavedef = ((uint32_t*)(state->wavetable))[nwave_i];
    uint8_t* pwavedef = (uint8_t*)&wavedef;
    state->alpha_w = (nwave_f - pwavedef[2]) * WSCALER[pwavedef[3] - 1];

    uint8_t n;
    for (n = 0; n < 2; n++) {
        if (pwavedef[n] < NWAVES) {
            state->pwave[n] = (uint8_t*)&WAVES[pwavedef[n]][0];
        } else if (pwavedef[n] == WAVE_SYNC || pwavedef[n] == WAVE_STEP) {
            const uint8_t pos = (nwave_i + n) & MASK_7;
            if (pos < 60)
                pwavedef[n + 2] = pos;
            else if (pos < 64)
                pwavedef[n + 2] = 60;
            else if (pos < 124)
                pwavedef[n + 2] = 124 - pos;
            else
                pwavedef[n + 2] = 0;
        }
    }

    *((uint32_t*)(&state->wave)) = wavedef;
}

/*  generate
    Calculate and return one sample value.
    Returns: sample value, floating point, -127.5 to 127.5
*/
_INLINE float generate(WtGenState* __restrict state)
{
    float y[OVS][4];
    uint8_t k, n;
    const uint32_t nwave_buf = *((uint32_t*)(&state->wave));
    const uint8_t* const nwave = (const uint8_t*)&nwave_buf;

    // Loop over oversampled values
    for (n = 0; n < OVS; n++) {
        const uint32_t phase = state->phase;
        // Loop over two waves
        for (k = 0; k < 2; k++) {
            if (nwave[k] < NWAVES) {
                // memory wave - interpolate between samples
                float alpha;
                uint8_t pos;
                if (!state->pd_bp) {
                    alpha = (float)(phase & 0x1ffffff) * Q25TOF;
                    pos = (uint8_t)(phase >> 25); // UQ7
                } else {
                    // apply phase distortion
                    float fpos;
                    if (phase <= state->pd_bp) {
                        fpos = state->pd_r1 * phase * Q25TOF;
                    } else {
                        fpos = state->pd_r2 * (phase - state->pd_bp) * Q25TOF + 64.f;
                    }
                    pos = (uint8_t)fpos;
                    alpha = fpos - pos;
                }
                const uint8_t* const pwave = state->pwave[k];
                if (!(pos & 0x40)) {
                    // pos 0..63
                    if (pos < 63) {
                        const uint16_t val16 = *((uint16_t*)(pwave + pos));
                        const uint8_t* pair = (const uint8_t*)(&val16);
                        y[n][k] = (1.f - alpha) * pair[0] + alpha * pair[1];
                    } else {
                        // pos == 63, edge case
                        const uint8_t val = pwave[63];
                        y[n][k] = (1.f - alpha) * val + alpha * (uint8_t)(~val);
                    }
                } else {
                    // pos 64..127
                    if (pos > 0) {
                        const uint8_t pos2 = ~pos & 0x3F;
                        const uint16_t val16 = *((uint16_t*)(pwave + pos2 - 1));
                        const uint8_t* pair = (const uint8_t*)(&val16);
                        y[n][k] = (1.f - alpha) * (uint8_t)(~pair[1]) + alpha * (uint8_t)(~pair[0]);
                    } else {
                        // pos == 0, edge case
                        const uint8_t val = *pwave;
                        y[n][k] = (1.f - alpha) * (uint8_t)(~val) + alpha * val;
                    }
                }
                y[n][k] -= 128.f;

            } else if (nwave[k] == WAVE_SYNC) {
                // wavetable 28: sync wave (no aliasing protection)
                const uint8_t nwave_i = nwave[k + 2];
                const uint32_t span = (uint32_t)(WT28_SPAN[nwave_i]) << 24; // UQ8.24
                uint32_t pos = (phase >> 1) % span; // UQ8.24
                y[n][k] = (1.f + nwave_i * 0.0859375f) * (float)pos * Q24TOF - 64.f;
            } else if (nwave[k] == WAVE_STEP) {
                // wavetable 29: step wave (with PolyBLEP)
                const float pos = (float)phase * Q25TOF;
                const float phase_step = (float)(state->step) * Q25TOF;
                const float rstep = (float)(state->recip_step);
                const float edge = 69.f + nwave[k + 2];
                y[n][k] = (pos < edge) ? 32.f : -32.f;
                if (pos < phase_step) {
                    const float t = pos * rstep;
                    y[n][k] += (t + t - t * t - 1.f) * 32.f;
                } else if (((edge - phase_step) < pos) && (pos < edge)) {
                    const float t = (pos - edge) * rstep;
                    y[n][k] -= (t * t + t + t + 1.f) * 32.f;
                } else if ((edge <= pos) && (pos < (edge + phase_step))) {
                    const float t = (pos - edge) * rstep;
                    y[n][k] -= (t + t - t * t - 1.f) * 32.f;
                } else if (pos > 128 - phase_step) {
                    const float t = (pos - 128.f) * rstep;
                    y[n][k] += (t * t + t + t + 1.f) * 32.f;
                }
            } else {
                y[n][k] = 0;
            }
        }
        state->phase += state->step;
        // interpolation
        y[n][0] = (1.f - state->alpha_w) * y[n][0] + state->alpha_w * y[n][1];
    }
#if OVS == 4
    y[0][0] = decimator_do(&state->decimator2, y[0][0], y[1][0]);
    y[1][0] = decimator_do(&state->decimator2, y[2][0], y[3][0]);
#endif
#if OVS != 1
    y[0][0] = decimator_do(&state->decimator, y[0][0], y[1][0]);
#endif
    return y[0][0] + 0.5f; // compensate for DC because of -128..127 range
}

/*  load_wavetable
    Prepares the wavetable definition
    wt_number: wavetable number, 0-30.
    use_upper: 1 - use upper wavetable, 0 - do not use.
*/
_INLINE void load_wavetable(WtGenState* state, uint8_t wt_number, uint8_t use_upper)
{
    if (wt_number > 30)
        return;

    uint32_t wt_entry;
    uint8_t* pwt_entry = (uint8_t*)&wt_entry;
    uint32_t edge; // [w0, w60, w64, w124]
    uint8_t* pedge = (uint8_t*)&edge;
    uint32_t* ptable = (uint32_t*)&state->wavetable[0][0];
    uint16_t wt1, wt2;
    uint8_t* pwt1 = (uint8_t*)&wt1;
    uint8_t* pwt2 = (uint8_t*)&wt2;
    uint8_t p;

    // 0-63: base wt
    if (wt_number != WT_SYNC && wt_number != WT_STEP) {
        uint16_t* pwtdef = (uint16_t*)&WAVETABLES[wt_number][0];
        wt1 = *pwtdef++;
        wt2 = *pwtdef++;
        uint8_t span = pwt2[0] - pwt1[0];
        pedge[0] = pwt1[1];
        for (p = 0; p < 60; p++, ptable++) {
            if (p >= pwt2[0]) {
                wt1 = wt2;
                wt2 = *pwtdef++;
                span = pwt2[0] - pwt1[0];
            }
            pwt_entry[0] = pwt1[1];
            pwt_entry[1] = pwt2[1];
            pwt_entry[2] = pwt1[0];
            pwt_entry[3] = span;
            *ptable = wt_entry;
        }
        pedge[1] = pwt2[1];
    } else {
        const uint8_t wn = (wt_number == WT_SYNC) ? WAVE_SYNC : WAVE_STEP;
        pwt_entry[0] = wn;
        pwt_entry[1] = wn;
        pwt_entry[3] = 1;
        for (p = 0; p < 60; p++, ptable++) {
            pwt_entry[2] = p;
            *ptable = wt_entry;
        }
        pedge[0] = wn;
        pedge[1] = wn;
    }

    // 64-123: upper/base wt reversed
    if (use_upper || (wt_number != WT_SYNC && wt_number != WT_STEP)) {
        ptable = (uint32_t*)&state->wavetable[124][0];
        uint16_t* pwtdef = (uint16_t*)&WAVETABLES[use_upper ? 30 : wt_number][0];
        wt1 = *pwtdef++;
        wt2 = *pwtdef++;
        uint8_t span = pwt2[0] - pwt1[0];
        pedge[3] = pwt1[1];
        uint8_t n = 0;
        for (p = 0; p <= 60; p++, ptable--) {
            if (p > pwt2[0]) {
                wt1 = wt2;
                wt2 = *pwtdef++;
                span = pwt2[0] - pwt1[0];
            }
            pwt_entry[0] = pwt2[1];
            pwt_entry[1] = pwt1[1];
            pwt_entry[2] = 124 - pwt2[0];
            pwt_entry[3] = span;
            *ptable = wt_entry;
        }
        pedge[2] = pwt2[1];
    } else {
        const uint8_t wn = (wt_number == WT_SYNC) ? WAVE_SYNC : WAVE_STEP;
        pwt_entry[0] = wn;
        pwt_entry[1] = wn;
        pwt_entry[3] = 1;
        ptable = (uint32_t*)&state->wavetable[64][0];
        for (p = 64; p < 124; p++, ptable++) {
            pwt_entry[2] = p;
            *ptable = wt_entry;
        }
        pedge[2] = wn;
        pedge[3] = wn;
    }

    // transition 60-64
    pwt_entry[0] = pedge[1];
    pwt_entry[1] = pedge[2];
    pwt_entry[2] = 60;
    pwt_entry[3] = 4;
    ptable = (uint32_t*)&state->wavetable[60][0];
    for (p = 0; p < 4; p++, ptable++) {
        *ptable = wt_entry;
    }

    // transition 124-127
    pwt_entry[0] = pedge[3];
    pwt_entry[1] = pedge[0];
    pwt_entry[2] = 124;
    ptable = (uint32_t*)&state->wavetable[124][0];
    for (p = 0; p < 4; p++, ptable++) {
        *ptable = wt_entry;
    }
}

#endif
