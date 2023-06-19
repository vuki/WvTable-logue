#pragma once
#ifndef _WTGEN_H
#define _WTGEN_H

/*
 * wtgen
 * Wavetable generator inspired by PPG Wave
 * Author: Grzegorz Szwoch (GregVuki)
 */

#include <stdint.h>
#include "wtdef.h"

#define MAXPHASE 128.f
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

/*  OvsMode
    Enum defining oversampling mode.
*/
typedef enum {
    OVS_NONE = 1, // no oversampling
    OVS_2x = 2, // oversampling with factor 2
    OVS_4x = 4 // oversampling with factor 4
} OvsMode; // Wavetable generator state

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
    uint8_t retro_mode; // use 'retro mode' for sample generation (no interpolation)
    float alpha_w; // linear interpolation coefficient
    uint32_t phase; // signal phase, Q7.25
    uint32_t step; // step to increase the phase, Q7.25
    float recip_step; // 1/step as float
} WtState;

typedef struct {
    WtState osc[2]; // oscillators: main, sub
    float base_wave[2]; // base wave number without modulation
    float sub_mix; // sub oscillator mix, 0..1 (0..100%)
    float srate;
    float phase_scaler;
    OvsMode ovs_mode; // oversampling rate: 1 (none), 2 (2x), 4 (4x)
    // DecimatorState decimator[2]; // decimator memory
    // ADEnvState wave_env; // wave number envelope
    // RampState wave_ramp; // linear wave number modulation ramp
    // uint8_t phase_reset[2]; // flags that oscillator phase was reset
} WtGenState;

/*  wtgen_init_state
    Initialize the generator
    srate: sampling rate in Hz
    ovs_mode: oversampling mode
*/
void wtgen_init(WtGenState* __restrict state, float srate, OvsMode ovs_mode)
{
    if (ovs_mode == OVS_4x)
        state->srate = srate * 4.f;
    else if (ovs_mode == OVS_2x)
        state->srate = srate * 2.f;
    else
        state->srate = srate;
    state->ovs_mode = ovs_mode;
    int i;
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
        state->osc[i].retro_mode = 0;
        state->base_wave[i] = 0;
        // state->phase_reset[i] = 1;
    }
    state->phase_scaler = 1.f / state->srate;
    state->sub_mix = 0;
    // adenv_init(&state->wave_env, srate); // not oversampled
    // ramp_init(&state->wave_ramp);
}

/*  wtgen_set_frequency
    Set frequency of the main osc
    freq: frequency in Hz
*/
_INLINE void wtgen_set_frequency(WtGenState* __restrict state, float freq)
{
    const float step_f = freq * state->phase_scaler;
    state->osc[0].step = (uint32_t)(step_f * 4294967296.f); // step * 2**32
    state->osc[0].recip_step = 0.0078125f / step_f; // (1/128)/step_f
}

/*  wtgen_set_sub_frequency
    Set frequency of the sub osc
    freq: frequency in Hz
*/
_INLINE void wtgen_set_sub_frequency(WtGenState* __restrict state, float freq)
{
    const float step_f = freq * state->phase_scaler;
    state->osc[1].step = (uint32_t)(step_f * 4294967296.f); // step * 2**32
    state->osc[1].recip_step = 0.0078125f / step_f; // (1/128)/step_f
}

/*  wtgen_set_wavetable
    Set the wavetable to use
    ntable: wavetable number, 0..61
*/
_INLINE void wtgen_set_wavetable(WtGenState* __restrict state, uint8_t ntable)
{
    while (ntable >= 61)
        ntable -= 61;
    uint8_t use_upper = 0;
    if (ntable >= 30) {
        use_upper = 1;
        ntable -= 30;
    }
    if (ntable != state->osc[0].ntable) {
        state->osc[0].ntable = state->osc[1].ntable = ntable;
        state->osc[0].use_upper = state->osc[1].use_upper = use_upper;
        state->osc[0].set_wave = state->osc[1].set_wave = NOWAVE;
        state->osc[0].wt_def_pos = state->osc[1].wt_def_pos = 0;
    }
}

/*  wtgen_set_wave
    Set the wave number for the main oscillator
    nwave: wave number, float 0..127
*/
_INLINE void wtgen_set_wave(WtGenState* __restrict state, float nwave)
{
    state->osc[0].req_wave = nwave;
    // defer actual wave number setting to the generation stage
    state->base_wave[0] = nwave;
}

/*  wtgen_set_sub_wave
    Set the wave number for the sub oscillator
    nwave: wave number, float 0..127
*/
_INLINE void wtgen_set_sub_wave(WtGenState* __restrict state, float nwave)
{
    state->osc[1].req_wave = nwave;
    // defer actual wave number setting to the generation stage
    state->base_wave[1] = nwave;
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
    // while (nwave >= 128.f)
    //     nwave -= 128.f;
    // while (nwave < 0)
    //     nwave += 128.f;
    // if (state->retro_mode)
    //     nwave = (float)((uint8_t)nwave); // only integers
    if (nwave < 64.f) {
        state->wtn = state->ntable;
    } else {
        state->wtn = state->use_upper ? WT_UPPER : state->ntable;
        nwave -= 64.f;
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

_INLINE float wt_generate(WtGenState* __restrict state)
{
    uint8_t k = 0;
    for (k = 0; k < 2; k++) {
        if (state->osc[k].set_wave != state->osc[k].req_wave) {
            set_wave_number(&state->osc[k], state->osc[k].req_wave);
        }
    }

    float y[4];
    const uint8_t nwave[4] = { state->osc[0].wave1, state->osc[0].wave2, state->osc[1].wave1, state->osc[1].wave2 };
    for (k = 0; k < 4; k++) {
        const uint8_t nosc = k >> 1; // 0->0, 1->0, 2->1, 3->1
        const uint32_t phase = state->osc[nosc].phase;
        if (nwave[k] < NWAVES) {
            // memory wave
            const float alpha = (float)(phase & 0x1ffffff) * Q25TOF;
            const uint8_t pos1 = (uint8_t)(phase >> 25); // UQ7
            const uint8_t pos2 = (pos1 + 1) & 0x7f; // UQ7
            const int8_t val1
                = (int8_t)(((pos1 & 0x40) ? (~WAVES[nwave[k]][~pos1 & 0x3F]) : (WAVES[nwave[k]][pos1 & 0x3F])) ^ 0x80);
            const int8_t val2
                = (int8_t)(((pos2 & 0x40) ? (~WAVES[nwave[k]][~pos2 & 0x3F]) : (WAVES[nwave[k]][pos2 & 0x3F])) ^ 0x80);
            y[k] = (1.f - alpha) * val1 + alpha * val2;
        } else if (nwave[k] == STD_TRIANGLE) {
            // standard wave: triangle
            const float pos = (float)phase * Q25TOF;
            y[k] = (pos < 64.f) ? (-96.f + 3.f * pos) : (96.f - 3.f * (pos - 64.f));
        } else if (nwave[k] == STD_PULSE) {
            // standard wave: pulse
            const float pos = (float)phase * Q25TOF;
            y[k] = (pos < 124.f) ? -64.f : 127.f;
        } else if (nwave[k] == STD_SQUARE) {
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
        } else if (nwave[k] == STD_SAW) {
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
        } else if (nwave[k] == WAVE_SYNC) {
            // wavetable 28: sync wave
            const uint8_t nwave = (int8_t)state->osc[nosc].nwave;
            const uint32_t span = (uint32_t)(WT28_SPAN[nwave]) << 24; // EQ8.24
            uint32_t pos = (phase >> 1) % span; // UQ8.24
            // while (pos >= span)
            //     pos -= span;
            y[k] = (1.f + nwave * 0.0859375f) * (float)pos * Q24TOF - 64.f;
        } else if (nwave[k] == WAVE_STEP) {
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
    const float y1 = (1.f - state->osc[0].alpha_w) * y[0] + state->osc[0].alpha_w * y[1];
    const float y2 = (1.f - state->osc[1].alpha_w) * y[2] + state->osc[1].alpha_w * y[3];
    return ((1.f - state->sub_mix) * y1 + state->sub_mix * y2) * 0.0078125f; // * 1/128;
}

#ifdef ARM_MATH_CM4
_INLINE float wt_generate_ovs(WtGenState* __restrict state)
{
    uint8_t k, n;
    for (k = 0; k < 2; k++) {
        if (state->osc[k].set_wave != state->osc[k].req_wave) {
            set_wave_number(&state->osc[k], state->osc[k].req_wave);
        }
    }

    static float y[4][4];
    uint32_t phase[2][4];

    for (n = 0; n < 4; n++) {
        phase[0][n] = state->osc[0].phase;
        phase[1][n] = state->osc[1].phase;
        state->osc[0].phase += state->osc[0].step;
        state->osc[1].phase += state->osc[1].step;
    }

    const uint8_t nwave[4] = { state->osc[0].wave1, state->osc[0].wave2, state->osc[1].wave1, state->osc[1].wave2 };
    for (k = 0; k < 4; k++) {
        const uint8_t nosc = k >> 1; // 0->0, 1->0, 2->1, 3->1
        if (nwave[k] < NWAVES) {
            // memory wave
#pragma GCC unroll 4
            for (n = 0; n < 4; n++) {
                const uint8_t pos1 = (uint8_t)(phase[nosc][n] >> 25); // UQ7
                const uint8_t pos2 = (pos1 + 1) & 0x7f; // UQ7
                const uint8_t val1
                    = (int8_t)(((pos1 & 0x40) ? (~WAVES[nwave[k]][~pos1 & 0x3F]) : (WAVES[nwave[k]][pos1 & 0x3F]))
                        ^ 0x80);
                const uint8_t val2
                    = (int8_t)(((pos2 & 0x40) ? (~WAVES[nwave[k]][~pos2 & 0x3F]) : (WAVES[nwave[k]][pos2 & 0x3F]))
                        ^ 0x80);
                const float alpha = (float)(phase[nosc][n] & 0x1ffffff) * Q25TOF;
                y[k][n] = (1.f - alpha) * val1 + alpha * val2;
            }

            /*
            for (n = 0; n < 4; n++) {
                const uint32_t phase = phasebuf[nosc][n];
                alphabuf[1][n] = (float)(phase & 0x1ffffff) * Q25TOF;
                alphabuf[0][n] = 1.f - alphabuf[1][n];
                const uint8_t pos1 = (uint8_t)(phase >> 25); // UQ7
                const uint8_t pos2 = (pos1 + 1) & 0x7f; // UQ7
                workbuf[0][n] = (float)(int8_t)(((pos1 & 0x40) ? (~WAVES[nwave[k]][~pos1 & 0x3F])
                                                               : (WAVES[nwave[k]][pos1 & 0x3F]))
                    ^ 0x80);
                workbuf[1][n] = (float)(int8_t)(((pos2 & 0x40) ? (~WAVES[nwave[k]][~pos2 & 0x3F])
                                                               : (WAVES[nwave[k]][pos2 & 0x3F]))
                    ^ 0x80);
            }
            // y[k] = (1.f - alpha) * val1 + alpha * val2;
            arm_mult_f32(&workbuf[0][0], &alphabuf[0][0], &workbuf[2][0]);
            arm_mult_f32(&workbuf[1][0], &alphabuf[1][0], &workbuf[3][0]);
            arm_add_f32(&workbuf[2][0], &workbuf[3][0], &y[k][0]);
            */
            /*
            } else if (nwave[k] == STD_TRIANGLE) {
                // standard wave: triangle
                const float pos = (float)phase * Q25TOF;
                y[k] = (pos < 64.f) ? (-96.f + 3.f * pos) : (96.f - 3.f * (pos - 64.f));
            } else if (nwave[k] == STD_PULSE) {
                // standard wave: pulse
                const float pos = (float)phase * Q25TOF;
                y[k] = (pos < 124.f) ? -64.f : 127.f;
            } else if (nwave[k] == STD_SQUARE) {
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
            } else if (nwave[k] == STD_SAW) {
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
            } else if (nwave[k] == WAVE_SYNC) {
                // wavetable 28: sync wave
                const uint8_t nwave = (int8_t)state->osc[nosc].nwave;
                const uint32_t span = (uint32_t)(WT28_SPAN[nwave]) << 24; // EQ8.24
                uint32_t pos = (phase >> 1) % span; // UQ8.24
                // while (pos >= span)
                //     pos -= span;
                y[k] = (1.f + nwave * 0.0859375f) * (float)pos * Q24TOF - 64.f;
            } else if (nwave[k] == WAVE_STEP) {
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
            */
        } else {
            y[k][0] = y[k][1] = y[k][2] = y[k][3] = 0;
        }
    }

#pragma GCC unroll 4
    for (n = 0; n < 4; n++) {
        const float y1 = (1.f - state->osc[0].alpha_w) * y[0][n] + state->osc[0].alpha_w * y[1][n];
        const float y2 = (1.f - state->osc[1].alpha_w) * y[2][n] + state->osc[1].alpha_w * y[3][n];
        y[0][n] = ((1.f - state->sub_mix) * y1 + state->sub_mix * y2) * 0.0078125f; // * 1/128;
    }

    // decimate (simulated)
    return (y[0][0] + y[0][1] + y[0][2] + y[0][3]) * 0.25f;
}

#endif // ifdef ARM_MATH_CM4

#endif
