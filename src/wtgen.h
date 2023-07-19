#pragma once
#ifndef _WTGEN_H
#define _WTGEN_H

#if defined(OVS_4x)
#define OVS 4
#elif defined(OVS_2x)
#define OVS 2
#else
#define OVS 1
#endif

/*
 * wtgen.h
 * Wavetable generator inspired by PPG Wave
 * Author: Grzegorz Szwoch (GregVuki)
 */

#include <stdint.h>
#include "wtdef.h"
#include "decimator.h"
#include "compat.h"

#define MAXPHASE 128.f
#define MAX_TABLE 61
#define Q25TOF 2.9802322387695312e-08f
#define Q24TOF 5.960464477539063e-08f
#define MASK_31 0x7fffffff
#define MASK_25 0x1ffffff
#define MASK_6 0x3f
#define MASK_BIT31 0x80000000

typedef struct {
    uint8_t ntable; // wavetable number that was set, 0..30
    uint8_t wtn; // currently used wavetable: base or upper
    uint8_t use_upper; // if not 0, use upper wavetable at waves 64..127
    // uint8_t wt_def_pos; // position of wave definition for stored waves
    uint8_t wave_pos; // wave position in wavetable, integer 0..63
    uint8_t wave[2]; // numbers of the stored waves (indices into WAVES)
    uint8_t* pwave[2]; // pointer to samples of the waves
    uint16_t* wt_pos; // pointer to position in the wavetable definition
    float alpha_w; // linear interpolation coefficient
    uint32_t phase; // signal phase, Q7.25
    uint32_t step; // step to increase the phase, Q7.25
    float recip_step; // 1/step as float
} WtState;

typedef struct {
    WtState osc; // oscillator state
    float srate;
    float phase_scaler;
    float wave_mod;
#if OVS != 1
    DecimatorState decimator;
#endif
#if OVS == 4
    DecimatorState decimator2;
#endif
} WtGenState;

/*  wtgen_init
    Initialize the generator
    srate: sampling rate in Hz
*/
void wtgen_init(WtGenState* __restrict state, float srate)
{
    state->srate = srate;
    state->osc.ntable = 0;
    state->osc.wtn = 0;
    state->osc.use_upper = 1;
    // state->osc.wt_def_pos = 0;
    state->osc.wave_pos = 0;
    // state->osc.wave[0] = WT_POS[0][1];
    // state->osc.wave[1] = WT_POS[1][1];
    state->osc.wave[0] = WAVETABLES[0][0];
    state->osc.wave[1] = WAVETABLES[0][1];
    state->osc.pwave[0] = (uint8_t*)&WAVES[state->osc.wave[0]][0];
    state->osc.pwave[1] = (uint8_t*)&WAVES[state->osc.wave[1]][0];
    state->osc.wt_pos = (uint16_t*)&WAVETABLES[0][0];
    state->osc.alpha_w = 0;
    state->osc.phase = 0;
    state->osc.step = 0x2000000;
    state->phase_scaler = 1.f / (OVS * state->srate);
    state->wave_mod = 0;
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
    state->osc.phase = 0;
    state->wave_mod = 0;
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
    state->osc.step = (uint32_t)(step_f * 4294967296.f); // step * 2**32
    state->osc.recip_step = 0.0078125f / step_f; // (1/128)/step_f
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
    if (ntable != state->osc.ntable) {
        state->osc.ntable = ntable;
        state->osc.use_upper = use_upper;
        if (state->osc.wtn != WT_UPPER) {
            state->osc.wtn = ntable;
            state->osc.wt_pos = (uint16_t*)&WAVETABLES[ntable][0];
        }
        // state->osc.wt_def_pos = 0;
    }
}

/*  set_wave_number
    Set the wave number
    wavenum: requested wave number, Q7.25
*/
_INLINE void set_wave_number(WtState* __restrict state, uint32_t wavenum)
{

    const uint8_t old_wtn = state->wtn;
    state->wtn = (state->use_upper && (wavenum & MASK_BIT31)) ? WT_UPPER : state->ntable;
    const uint8_t nwave_i = (wavenum >> 25) & MASK_6; // integer part of the wave number, 0..63 (UQ6)
    const float nwave_f = Q25TOF * (wavenum & MASK_31); // full wave number (0..64) as float
    state->wave_pos = nwave_i;
    if (state->wtn != old_wtn) {
        // state->wt_def_pos = 0;
        state->wt_pos = (uint16_t*)&WAVETABLES[state->wtn][0];
    }
    if (nwave_i < STANDARD_WAVES && state->wtn != WT_SYNC && state->wtn != WT_STEP) {
        // select from stored waves
#if 0
        // old version
        const uint8_t(*wt_def)[2] = &WT_POS[WT_IDX[state->wtn]];
        while (nwave_i > wt_def[state->wt_def_pos + 1][0])
            state->wt_def_pos++;
        while (nwave_i < wt_def[state->wt_def_pos][0])
            state->wt_def_pos--;
        state->wave[0] = wt_def[state->wt_def_pos][1];
        state->wave[1] = wt_def[state->wt_def_pos + 1][1];
        state->pwave[0] = (uint8_t*)&WAVES[state->wave[0]][0];
        state->pwave[1] = (uint8_t*)&WAVES[state->wave[1]][0];
        if (nwave_i < LAST_MEM_WAVE) {
            const uint8_t denom = wt_def[state->wt_def_pos + 1][0] - wt_def[state->wt_def_pos][0] - 1;
            state->alpha_w = (nwave_f - wt_def[state->wt_def_pos][0]) * WSCALER[denom];
        } else {
            state->wave[1] = STD_TRIANGLE;
            state->alpha_w = nwave_f - LAST_MEM_WAVE;
        }
#else
        // rewrite
        uint16_t wt_first = *state->wt_pos;
        uint16_t wt_second;
        const uint8_t* wtp1 = (const uint8_t*)&wt_first;
        const uint8_t* wtp2 = (const uint8_t*)&wt_second;
        if (nwave_i < wtp1[0]) {
            while (1) {
                wt_second = wt_first;
                wt_first = *(--state->wt_pos);
                if (nwave_i >= wtp1[0])
                    break;
            }
        } else {
            wt_second = *(state->wt_pos + 1);
            if (nwave_i > wtp2[0]) {
                while (1) {
                    ++state->wt_pos;
                    wt_first = wt_second;
                    wt_second = *(state->wt_pos + 1);
                    if (nwave_i < wtp2[0])
                        break;
                }
            }
        }
        state->wave[0] = wtp1[1];
        state->pwave[0] = (uint8_t*)&WAVES[wtp1[1]][0];
        if (nwave_i < LAST_MEM_WAVE) {
            state->wave[1] = wtp2[1];
            state->pwave[1] = (uint8_t*)&WAVES[wtp2[1]][0];
            const uint8_t denom = wtp2[0] - wtp1[0] - 1;
            state->alpha_w = (nwave_f - wtp1[0]) * WSCALER[denom];
        } else {
            state->wave[1] = STD_TRIANGLE;
            state->alpha_w = nwave_f - LAST_MEM_WAVE;
        }
#endif

    } else if (nwave_i < STANDARD_WAVES && state->wtn == WT_SYNC) {
        // sync wave
        state->wave[0] = WAVE_SYNC;
        state->wave[1] = (nwave_i < LAST_MEM_WAVE) ? WAVE_SYNC : STD_TRIANGLE;
        state->alpha_w = nwave_f - nwave_i;
    } else if (nwave_i < STANDARD_WAVES && state->wtn == WT_STEP) {
        // step wave
        state->wave[0] = WAVE_STEP;
        state->wave[1] = (nwave_i < LAST_MEM_WAVE) ? WAVE_STEP : STD_TRIANGLE;
        state->alpha_w = nwave_f - nwave_i;
    } else {
        // standard waves
        // const uint8_t nw = (uint8_t)nwave;
        state->wave[0] = nwave_i - WV_TRIANGLE + NWAVES;
        const uint8_t next_wt = (state->wtn != WT_UPPER && state->use_upper) ? WT_UPPER : state->wtn;
        // state->wave[1] = (nwave_i < 63) ? state->wave[0] + 1 : WT_POS[WT_IDX[next_wt]][1];
        state->wave[1] = (nwave_i < 63) ? state->wave[0] + 1 : WAVETABLES[next_wt][1];
        state->alpha_w = nwave_f - nwave_i;
    }
}

/*  generate
    Calculate and return one sample value.
    Returns: sample value, floating point, -127.5 to 127.5
*/
_INLINE float generate(WtGenState* __restrict state)
{
    float y[OVS][4];
    uint8_t k, n;
    // Loop over oversampled values
    for (n = 0; n < OVS; n++) {
        const uint32_t phase = state->osc.phase;
        // Loop over two waves
        for (k = 0; k < 2; k++) {
            const uint8_t nwavek = state->osc.wave[k];
            if (nwavek < NWAVES) {
                // memory wave - interpolate between samples
                const float alpha = (float)(phase & 0x1ffffff) * Q25TOF;
#if 0
                const uint8_t pos1 = (uint8_t)(phase >> 25); // UQ7
                const uint8_t pos2 = (pos1 + 1) & 0x7f; // UQ7
                const uint8_t* const pwave = pwaves[k];
                const int8_t val1 = (int8_t)(((pos1 & 0x40) ? (~pwave[~pos1 & 0x3F]) : (pwave[pos1])) ^ 0x80);
                const int8_t val2 = (int8_t)(((pos2 & 0x40) ? (~pwave[~pos2 & 0x3F]) : (pwave[pos2])) ^ 0x80);
                y[n][k] = (1.f - alpha) * val1 + alpha * val2;
#else
                // Version with more operations and branching,
                // but one 16-bit read is used instead of two 8-bit reads.
                // Warning: some reads will be unaligned and they will be slower.
                const uint8_t pos = (uint8_t)(phase >> 25); // UQ7
                const uint8_t* const pwave = state->osc.pwave[k];
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
#endif
            } else if (nwavek == STD_TRIANGLE) {
                // standard wave: triangle
                const float pos = (float)phase * Q25TOF;
                y[n][k] = (pos < 64.f) ? (-96.f + 3.f * pos) : (96.f - 3.f * (pos - 64.f));
            } else if (nwavek == STD_PULSE) {
                // standard wave: pulse
                const float pos = (float)phase * Q25TOF;
                y[n][k] = (pos < 124.f) ? -64.f : 127.f;
            } else if (nwavek == STD_SQUARE) {
                // standard wave: square (with PolyBLEP)
                const float pos = (float)phase * Q25TOF;
                const float phase_step = (float)(state->osc.step) * Q25TOF;
                const float rstep = (float)(state->osc.recip_step);
                y[n][k] = (pos < 64.f) ? -96.f : 96.f;
                if (pos < phase_step) {
                    const float t = pos * rstep;
                    y[n][k] -= (t + t - t * t - 1.f) * 96.f;
                } else if (((64.f - phase_step) < pos) && (pos < 64.f)) {
                    const float t = (pos - 64.f) * rstep;
                    y[n][k] += (t * t + t + t + 1.f) * 96.f;
                } else if ((64.f <= pos) && (pos < (64.f + phase_step))) {
                    const float t = (pos - 64.f) * rstep;
                    y[n][k] += (t + t - t * t - 1.f) * 96.f;
                } else if (pos > 128 - phase_step) {
                    const float t = (pos - 128.f) * rstep;
                    y[n][k] -= (t * t + t + t + 1.f) * 96.f;
                }
            } else if (nwavek == STD_SAW) {
                // standard wave: sawtooth (with PolyBLEP)
                const float pos = (float)phase * Q25TOF;
                const float phase_step = (float)(state->osc.step) * Q25TOF;
                const float rstep = (float)(state->osc.recip_step);
                y[n][k] = -64.f + pos;
                if (pos < phase_step) {
                    const float t = pos * rstep;
                    y[n][k] -= (t + t - t * t - 1.f) * 64.f;
                } else if (pos > 128.f - phase_step) {
                    const float t = (pos - 128.f) * rstep;
                    y[n][k] -= (t * t + t + t + 1.f) * 64.f;
                }
            } else if (nwavek == WAVE_SYNC) {
                // wavetable 28: sync wave
                const uint8_t nwave = (int8_t)state->osc.wave_pos;
                const uint32_t span = (uint32_t)(WT28_SPAN[nwave]) << 24; // UQ8.24
                uint32_t pos = (phase >> 1) % span; // UQ8.24
                y[n][k] = (1.f + nwave * 0.0859375f) * (float)pos * Q24TOF - 64.f;
            } else if (nwavek == WAVE_STEP) {
                // wavetable 29: step wave (with PolyBLEP)
                const float pos = (float)phase * Q25TOF;
                const float phase_step = (float)(state->osc.step) * Q25TOF;
                const float rstep = (float)(state->osc.recip_step);
                const float edge = 69.f + state->osc.wave_pos;
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
        state->osc.phase += state->osc.step;
        // interpolation
        y[n][0] = (1.f - state->osc.alpha_w) * y[n][0] + state->osc.alpha_w * y[n][1];
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

#endif
