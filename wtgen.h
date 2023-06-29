#pragma once
#ifndef _WTGEN_H
#define _WTGEN_H

/*
 * wtgen.h
 * Wavetable generator inspired by PPG Wave
 * Author: Grzegorz Szwoch (GregVuki)
 */

#include <stdint.h>
#include "wtdef.h"

#define MAXPHASE 128.f
#define MAX_TABLE 61
#define MAX_BASE_WAVE 64.f
#define MAX_UPPER_WAVE 128.f
#define EPS 1e-5f
#define NOWAVE -1.f
#define Q25TOF 2.9802322387695312e-08f
#define Q24TOF 5.960464477539063e-08f

#ifndef NO_FORCE_INLINE
#if defined(__GNUC__)
#ifndef __clang__
#define _INLINE static inline __attribute__((always_inline, optimize("Ofast")))
#else
#define _INLINE static inline __attribute__((always_inline))
#endif
#elif defined(_MSC_VER)
#define _INLINE static inline __forceinline
#else
#define _INLINE static inline
#endif
#else
#define _INLINE static inline
#endif // #ifndef NO_FORCE_INLINE

typedef enum { ENV_S, ENV_A, ENV_D } EnvStage;

typedef struct {
    float nwave; // normalized wave number, 0..64
    float set_wave; // last wave number (raw) that was set
    float req_wave; // wave number (raw) that the caller wants to set
    uint8_t ntable; // wavetable number that was set, 0..30
    uint8_t wtn; // currently used wavetable: base or upper
    uint8_t use_upper; // if not 0, use upper wavetable at waves 64..127
    uint8_t wt_def_pos; // position of wave definition for stored waves
    uint8_t wave1; // number of the first stored wave (before the wave)
    uint8_t wave2; // number of the second stored wave (after the wave)
    float alpha_w; // linear interpolation coefficient
    uint32_t phase; // signal phase, Q7.25
    uint32_t step; // step to increase the phase, Q7.25
    float recip_step; // 1/step as float
} WtState;

typedef struct {
    WtState osc[2]; // oscillators: main, sub
    float sub_mix; // sub oscillator mix, 0..1 (0..100%)
    float srate;
    float phase_scaler;
    EnvStage wave_env_stage;
    float wave_env_arate;
    float wave_env_drate;
    float wave_env_amount;
    float wave_env_value;
    float wave_mod;
} WtGenState;

/*  wtgen_init
    Initialize the generator
    srate: sampling rate in Hz
*/
void wtgen_init(WtGenState* __restrict state, float srate)
{
    state->srate = srate;
    uint8_t i;
    for (i = 0; i < 2; i++) {
        state->osc[i].ntable = state->osc[i].wtn = 0;
        state->osc[i].nwave = 0;
        state->osc[i].set_wave = NOWAVE;
        state->osc[i].req_wave = 0;
        state->osc[i].use_upper = 1;
        state->osc[i].wt_def_pos = 0;
        state->osc[i].wave1 = WT_POS[0][1];
        state->osc[i].wave2 = WT_POS[1][1];
        state->osc[i].alpha_w = 0;
        state->osc[i].phase = 0;
        state->osc[i].step = 0x2000000;
    }
    state->phase_scaler = 1.f / state->srate;
    state->sub_mix = 0;
    state->wave_env_stage = ENV_S;
    state->wave_env_arate = 0;
    state->wave_env_drate = 0;
    state->wave_env_amount = 0;
    state->wave_env_value = 0;
    state->wave_mod = 0;
}

/*  set_main_frequency
    Set frequency of the main osc
    freq: frequency in Hz
*/
_INLINE void set_main_frequency(WtGenState* __restrict state, float freq)
{
    const float step_f = freq * state->phase_scaler;
    state->osc[0].step = (uint32_t)(step_f * 4294967296.f); // step * 2**32
    state->osc[0].recip_step = 0.0078125f / step_f; // (1/128)/step_f
}

/*  set_sub_frequency
    Set frequency of the sub osc
    freq: frequency in Hz
*/
_INLINE void set_sub_frequency(WtGenState* __restrict state, float freq)
{
    const float step_f = freq * state->phase_scaler;
    state->osc[1].step = (uint32_t)(step_f * 4294967296.f); // step * 2**32
    state->osc[1].recip_step = 0.0078125f / step_f; // (1/128)/step_f
}

/*  set_wavetable
    Set the wavetable to use
    ntable: wavetable number, 0..61
*/
_INLINE void set_wavetable(WtGenState* __restrict state, uint8_t ntable)
{
    while (ntable >= MAX_TABLE)
        ntable -= MAX_TABLE;
    uint8_t use_upper = 0;
    if (ntable >= WT_UPPER) {
        use_upper = 1;
        ntable -= WT_UPPER;
    }
    if (ntable != state->osc[0].ntable) {
        state->osc[0].ntable = state->osc[1].ntable = ntable;
        state->osc[0].use_upper = state->osc[1].use_upper = use_upper;
        state->osc[0].set_wave = state->osc[1].set_wave = NOWAVE;
        state->osc[0].wt_def_pos = state->osc[1].wt_def_pos = 0;
    }
}

/*  set_wave_number
    Set the wave number
    nwave: requested wave number (float)
*/
_INLINE void set_wave_number(WtState* __restrict state, float nwave)
{
    // if (state->set_wave == nwave)
    //     return; // already set
    state->set_wave = nwave;
    const uint8_t old_wtn = state->wtn;
    while (nwave >= MAX_UPPER_WAVE)
        nwave -= MAX_UPPER_WAVE;
    while (nwave < 0)
        nwave += MAX_UPPER_WAVE;
    if (nwave < MAX_BASE_WAVE) {
        state->wtn = state->ntable;
    } else {
        state->wtn = state->use_upper ? WT_UPPER : state->ntable;
        nwave -= MAX_BASE_WAVE;
    }
    if (nwave == state->nwave && state->wtn == old_wtn)
        return; // same wave and wavetable
    if (state->wtn != old_wtn)
        state->wt_def_pos = 0;
    if (nwave < STANDARD_WAVES && state->wtn != WT_SYNC && state->wtn != WT_STEP) {
        // select from stored waves
        const uint8_t(*wt_def)[2] = &WT_POS[WT_IDX[state->wtn]];
        while (nwave > wt_def[state->wt_def_pos + 1][0])
            state->wt_def_pos++;
        while (nwave < wt_def[state->wt_def_pos][0])
            state->wt_def_pos--;
        state->wave1 = wt_def[state->wt_def_pos][1];
        state->wave2 = wt_def[state->wt_def_pos + 1][1];
        if (nwave < LAST_MEM_WAVE) {
            const uint8_t denom = wt_def[state->wt_def_pos + 1][0] - wt_def[state->wt_def_pos][0] - 1;
            state->alpha_w = (nwave - wt_def[state->wt_def_pos][0]) * WSCALER[denom];
        } else {
            state->wave2 = STD_TRIANGLE;
            state->alpha_w = nwave - LAST_MEM_WAVE;
        }
    } else if (nwave < STANDARD_WAVES && state->wtn == WT_SYNC) {
        // sync wave
        state->wave1 = WAVE_SYNC;
        state->wave2 = (nwave < LAST_MEM_WAVE) ? WAVE_SYNC : STD_TRIANGLE;
        state->alpha_w = nwave - (uint8_t)nwave;
    } else if (nwave < STANDARD_WAVES && state->wtn == WT_STEP) {
        // step wave
        state->wave1 = WAVE_STEP;
        state->wave2 = (nwave < LAST_MEM_WAVE) ? WAVE_STEP : STD_TRIANGLE;
        state->alpha_w = nwave - (uint8_t)nwave;
    } else {
        // standard waves
        const uint8_t nw = (uint8_t)nwave;
        state->wave1 = nw - WV_TRIANGLE + NWAVES;
        const uint8_t next_wt = (state->wtn != WT_UPPER && state->use_upper) ? WT_UPPER : state->wtn;
        state->wave2 = (nw < 63) ? state->wave1 + 1 : WT_POS[WT_IDX[next_wt]][1];
        state->alpha_w = nwave - nw;
    }
    state->nwave = nwave;
}

_INLINE float generate(WtGenState* __restrict state)
{
    // wave number modulation
    uint8_t k;
    for (k = 0; k < 2; k++) {
        const float nwave = state->osc[k].req_wave + state->wave_mod;
        if (nwave != state->osc[k].set_wave) {
            set_wave_number(&state->osc[k], nwave);
        }
    }

    // generate 4 values for interpolation
    // (main_wave1, main_wave2, sub_wave1, sub_wave2)
    static float y[4];
    const uint8_t nwave[4] = { state->osc[0].wave1, state->osc[0].wave2, state->osc[1].wave1, state->osc[1].wave2 };
    for (k = 0; k < 4; k++) {
        const uint8_t nosc = k >> 1; // 0->0, 1->0, 2->1, 3->1
        const uint32_t phase = state->osc[nosc].phase;
        const uint8_t nwavek = nwave[k];
        if (nwavek < NWAVES) {
            // memory wave - interpolate between samples
            const float alpha = (float)(phase & 0x1ffffff) * Q25TOF;
            const uint8_t pos1 = (uint8_t)(phase >> 25); // UQ7
            const uint8_t pos2 = (pos1 + 1) & 0x7f; // UQ7
            // const int8_t val1
            //     = (int8_t)(((pos1 & 0x40) ? (~WAVES[nwavek][~pos1 & 0x3F]) : (WAVES[nwavek][pos1 & 0x3F])) ^ 0x80);
            // const int8_t val2
            //     = (int8_t)(((pos2 & 0x40) ? (~WAVES[nwavek][~pos2 & 0x3F]) : (WAVES[nwavek][pos2 & 0x3F])) ^ 0x80);
            const uint8_t* const pwave = WAVES[nwavek];
            const int8_t val1 = (int8_t)(((pos1 & 0x40) ? (~pwave[~pos1 & 0x3F]) : (pwave[pos1])) ^ 0x80);
            const int8_t val2 = (int8_t)(((pos2 & 0x40) ? (~pwave[~pos2 & 0x3F]) : (pwave[pos2])) ^ 0x80);
            y[k] = (1.f - alpha) * val1 + alpha * val2;
        } else if (nwavek == STD_TRIANGLE) {
            // standard wave: triangle
            const float pos = (float)phase * Q25TOF;
            y[k] = (pos < 64.f) ? (-96.f + 3.f * pos) : (96.f - 3.f * (pos - 64.f));
        } else if (nwavek == STD_PULSE) {
            // standard wave: pulse
            const float pos = (float)phase * Q25TOF;
            y[k] = (pos < 124.f) ? -64.f : 127.f;
        } else if (nwavek == STD_SQUARE) {
            // standard wave: square (with PolyBLEP)
            const float pos = (float)phase * Q25TOF;
            const float phase_step = (float)(state->osc[nosc].step) * Q25TOF;
            const float rstep = (float)(state->osc[nosc].recip_step);
            y[k] = (pos < 64.f) ? -96.f : 96.f;
            if (pos < phase_step) {
                const float t = pos * rstep;
                y[k] -= (t + t - t * t - 1.f) * 96.f;
            } else if (((64.f - phase_step) < pos) && (pos < 64.f)) {
                const float t = (pos - 64.f) * rstep;
                y[k] += (t * t + t + t + 1.f) * 96.f;
            } else if ((64.f <= pos) && (pos < (64.f + phase_step))) {
                const float t = (pos - 64.f) * rstep;
                y[k] += (t + t - t * t - 1.f) * 96.f;
            } else if (pos > 128 - phase_step) {
                const float t = (pos - 128.f) * rstep;
                y[k] -= (t * t + t + t + 1.f) * 96.f;
            }
        } else if (nwavek == STD_SAW) {
            // standard wave: sawtooth (with PolyBLEP)
            const float pos = (float)phase * Q25TOF;
            const float phase_step = (float)(state->osc[nosc].step) * Q25TOF;
            const float rstep = (float)(state->osc[nosc].recip_step);
            y[k] = -64.f + pos;
            if (pos < phase_step) {
                const float t = pos * rstep;
                y[k] -= (t + t - t * t - 1.f) * 64.f;
            } else if (pos > 128.f - phase_step) {
                const float t = (pos - 128.f) * rstep;
                y[k] -= (t * t + t + t + 1.f) * 64.f;
            }
        } else if (nwavek == WAVE_SYNC) {
            // wavetable 28: sync wave
            const uint8_t nwave = (int8_t)state->osc[nosc].nwave;
            const uint32_t span = (uint32_t)(WT28_SPAN[nwave]) << 24; // UQ8.24
            uint32_t pos = (phase >> 1) % span; // UQ8.24
            y[k] = (1.f + nwave * 0.0859375f) * (float)pos * Q24TOF - 64.f;
        } else if (nwavek == WAVE_STEP) {
            // wavetable 29: step wave (with PolyBLEP)
            const float pos = (float)phase * Q25TOF;
            const float phase_step = (float)(state->osc[nosc].step) * Q25TOF;
            const float rstep = (float)(state->osc[nosc].recip_step);
            const float edge = 69.f + state->osc[nosc].nwave;
            y[k] = (pos < edge) ? 32.f : -32.f;
            if (pos < phase_step) {
                const float t = pos * rstep;
                y[k] += (t + t - t * t - 1.f) * 32.f;
            } else if (((edge - phase_step) < pos) && (pos < edge)) {
                const float t = (pos - edge) * rstep;
                y[k] -= (t * t + t + t + 1.f) * 32.f;
            } else if ((edge <= pos) && (pos < (edge + phase_step))) {
                const float t = (pos - edge) * rstep;
                y[k] -= (t + t - t * t - 1.f) * 32.f;
            } else if (pos > 128 - phase_step) {
                const float t = (pos - 128.f) * rstep;
                y[k] += (t * t + t + t + 1.f) * 32.f;
            }
        } else {
            y[k] = 0;
        }
    }
    state->osc[0].phase += state->osc[0].step;
    state->osc[1].phase += state->osc[1].step;
    // interpolation
    const float y1 = (1.f - state->osc[0].alpha_w) * y[0] + state->osc[0].alpha_w * y[1];
    const float y2 = (1.f - state->osc[1].alpha_w) * y[2] + state->osc[1].alpha_w * y[3];
    return ((1.f - state->sub_mix) * y1 + state->sub_mix * y2) * 0.0078125f; // * 1/128;
}

#endif
