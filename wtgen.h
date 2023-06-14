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
    float phase; // signal phase, 0..128
    float step; // step to increase the phase
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
    uint8_t phase_reset[2]; // flags that oscillator phase was reset
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
        state->osc[i].step = 1.f;
        state->osc[i].retro_mode = 0;
        state->base_wave[i] = 0;
        state->phase_reset[i] = 1;
    }
    state->phase_scaler = MAXPHASE / state->srate;
    state->sub_mix = 0;
    // adenv_init(&state->wave_env, srate); // not oversampled
    // ramp_init(&state->wave_ramp);
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
    if (state->set_wave == nwave)
        return; // already set
    state->set_wave = nwave;
    const uint8_t old_wtn = state->wtn;
    while (nwave >= 128.f)
        nwave -= 128.f;
    while (nwave < 0)
        nwave += 128.f;
    if (state->retro_mode)
        nwave = (float)((uint8_t)nwave); // only integers
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
            state->alpha_w = nwave - LAST_MEM_WAVE;
        }
    } else if ((state->wtn == WT_SYNC || state->wtn == WT_STEP) && nwave < LAST_MEM_WAVE) {
        state->alpha_w = 0;
    } else {
        const uint8_t nw = (uint8_t)nwave;
        state->wave1 = nw - STANDARD_WAVES + NWAVES;
        state->wave2 = (nw < 63) ? state->wave1 + 1 : WT_POS[WT_IDX[state->wtn]][1];
        state->alpha_w = nwave - nw;
    }
    state->nwave = nwave;
}

_INLINE float wt_generate(WtGenState* __restrict state)
{
    static uint8_t nwave[8]; // wave numbers
    static uint8_t ipos[8]; // integer sample positions
    static float scaler[8]; // scaler values

    uint8_t k = 0;
    for (k = 0; k < 2; k++) {
        if (state->osc[k].set_wave != state->osc[k].req_wave) {
            set_wave_number(&state->osc[k], state->osc[k].req_wave);
        }
    }

    // wave numbers
    nwave[0] = nwave[1] = state->osc[0].wave1;
    nwave[2] = nwave[3] = state->osc[0].wave2;
    nwave[4] = nwave[5] = state->osc[1].wave1;
    nwave[6] = nwave[7] = state->osc[1].wave2;

    // sample positions and sample scalers
    ipos[0] = ipos[2] = (uint8_t)state->osc[0].phase;
    ipos[1] = ipos[3] = ipos[0] + 1; // auto wrap
    ipos[4] = ipos[6] = (uint8_t)state->osc[1].phase;
    ipos[5] = ipos[7] = ipos[4] + 1;
    const float asA = state->osc[0].phase - ipos[0];
    const float masA = 1.f - asA;
    const float asB = state->osc[1].phase - ipos[4];
    const float masB = 1.f - asB;

    // other scalers (1-alpha)
    const float mawA = 1.f - state->osc[0].alpha_w;
    const float mawB = 1.f - state->osc[1].alpha_w;
    const float mamix = 1.f - state->sub_mix;

    // all scalers
    scaler[0] = mamix * mawA * masA;
    scaler[1] = mamix * mawA * asA;
    scaler[2] = mamix * state->osc[0].alpha_w * masA;
    scaler[3] = mamix * state->osc[0].alpha_w * asA;
    scaler[4] = state->sub_mix * mawB * masB;
    scaler[5] = state->sub_mix * mawB * asB;
    scaler[6] = state->sub_mix * state->osc[1].alpha_w * masB;
    scaler[7] = state->sub_mix * state->osc[1].alpha_w * asB;

    // the generation loop
    float y = 0;
    for (k = 0; k < 8; k++) {
        if (scaler[k]) {
            // here, check what kind of wave to generate
            if (nwave[k] < NWAVES)
                // assuming a memory wave:
                y += scaler[k]
                    * (int8_t)(((ipos[k] & 0x40) ? (~WAVES[nwave[k]][~ipos[k] & 0x3F])
                                                 : (WAVES[nwave[k]][ipos[k] & 0x3F]))
                        ^ 0x80);
        }
    }

    for (k = 0; k < 2; k++) {
        state->osc[k].phase += state->osc[k].step;
        if (state->osc[k].phase > MAXPHASE)
            state->osc[k].phase -= MAXPHASE;
    }
    return y * 0.0078125;
}

#endif
