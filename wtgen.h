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
void wtgen_init(WtGenState* state, float srate, OvsMode ovs_mode)
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

_INLINE float wt_generate(WtGenState* __restrict state)
{
    float alpha[2];
    float y = 0;
    uint8_t k;
    uint8_t ipos = (uint8_t)state->osc[0].phase;
    alpha[1] = state->osc[0].phase - ipos;
    alpha[0] = 1.f - alpha[1];
    const uint8_t* p_wave = &WAVES[state->osc[0].wave1][0];
    for (k = 0; k < 2; k++, ipos++) {
        if (alpha[k])
            y += alpha[k] * (int8_t)(((ipos & 0x40) ? (~p_wave[~ipos & 0x3F]) : (p_wave[ipos & 0x3F])) ^ 0x80);
    }
    state->osc[0].phase += state->osc[0].step;
    if (state->osc[0].phase > MAXPHASE)
        state->osc[0].phase -= MAXPHASE;
    return y * 0.0078125; // y/128
}

#endif
